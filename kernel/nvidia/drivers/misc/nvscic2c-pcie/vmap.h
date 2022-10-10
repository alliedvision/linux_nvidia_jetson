/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (c) 2021-2022, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#ifndef __VMAP_H__
#define __VMAP_H__

#include "common.h"

/*
 * Virtual mapping abstraction offers pinning functionalities for the very
 * the specific use-case(s) to achieve NvStreams streaming over NvSciC2cPcie
 * on Tegra PCIe EP or Tegra PCIe RP. It's scope is specific to/for and limited
 * to NvSciC2cPcie.
 *
 * Virtual mapping interfaces primarily offer mapping(also referred as pinning
 * in tegra context) and unmapping(also referred as unpinning in tegra context)
 * dma_buf backed NvRmMemHandles or Syncpoint Shim backed NvRmHost1xSyncpoints.
 * In this abstraction, Memory/Mem objects/objs are NvRmMemHandles(or it's FD),
 * and Sync objects/objs are NvRmHost1xSyncpointHandles (or it's FD).
 *
 * This SW shall run either on Tegra PCIe RP or Tegra PCIe EP.
 *
 * ASSUMPTION: Once pages are mapped/pinned, on tegra they shall not swap out.
 *
 * On tegra, Mem objects are the NvRmMemHandles with struct dma_buf backing.
 * On tegra, Sync objects are the NvRmHost1xSyncpointHandles with Syncpoint
 * shim (aperture) backing.
 *
 * Each object is considered one of the following:
 *
 * 1. Local - The object visibility is limited to local SoC on which it is
 *    allocated. Also sometimes referred as Unexported. These objects are mapped
 *    to PCIe device address space and it's whereabouts are NOT shared with
 *    remote SoC.
 * 2. Export/Exported - The object visibility is across PCIe to remote SoC
 *    and subsequently remote SoC can initiate writes to it. For tegra,
 *    exported objects are never read over PCIe. These objects are mapped to
 *    PCIe device address space and it's whereabouts shall be shared with remote
 *    SoC.
 * 3. Import/Imported - This is a virtual object which points to the
 *    corresponding object exported by remote SoC. Being virtual, it shall be
 *    similar for both Mem and Sync objects - therefore Imported object is
 *    just an import/imported object and NOT imported Mem/Sync obj.
 *
 * Protection/Permission flags:
 * a. Local Mem objects map to PCIe device with READ access.
 * b. Export Mem objects map to PCIe device with WRITE access (We export for
 *    remote to write to it via CPU or PCIe eDMA(zero-copy).
 * c. Local Sync objects are not mapped to PCIe device as NvSciC2cPcie KMD
 *    shall signal(write) them via NvHost1x interface.
 * d. Export Sync objects map to PCIe device with WRITE access (We export for
 *    remote to signal(write) to it via CPU or PCIe eDMA).
 *
 * Mapping:
 * Tegra PCIe EP exposes three BAR memory windows towards PCIe RP. Of these,
 * Only one (1) (BAR0) is available for NvSciC2cPcie access. Therefore, all
 * Export objects must be mapped to a PCIe address which this PCIe EP BAR0
 * translation is programmed with. With the overall PCIe address space being
 * much bigger than the PCIe BAR0 space, there is an explicit need to stitch
 * all Exported objects to a single region. This requires Export objects be
 * mapped using iommu apis to achieve BAR stitching and this mapping is
 * referred as client managed with NvSciC2cPcie managing the iova region.
 *
 * Tegra PCIe EP has limited set of translation registers for it's CPU to raise
 * PCIe transactions towards a PCIe RP. Therefore, when Sync objects are
 * exported from PCIe RP towards PCIe EP to CPU signal them, they must be mapped
 * to a single iova region which PCIe EP has setup for it's translaction
 * registers. This is strictly not applicable for Exported Mem objects as they
 * are written by eDMA always by the importing SoC. However, to keep parity and
 * symmetry in the behavior of Exported Mem objects from Tegra PCIe EE->PCIe RP,
 * Exported Mem objects from Tegra PCIe RP->Tegra PCIe EP shall also be mapped
 * to a client managed iova region.
 *
 * For Local Mem objects which are accessed by local SoC PCIe eDMA, they
 * can be mapped to any pcie address outside the reserved iova region by
 * NvSciC2cPcie for exports. This doesn't require any iova management by
 * client and is prefectly fine to use pci device (smmu) managed iova.
 * This mapping is referred as device(dev) managed mapping. Only on Tegra PCIe
 * RP, Exported Mem objects can be mapped using dev managed iova as Tegra PCIe
 * EP shall write them using eDMA, but as stated before, to keep parity and
 * symmetry in the behavior of Exported Mem objects from Tegra PCIe EE->PCIe RP,
 * Exported Mem objects from Tegra PCIe RP->Tegra PCIe EP shall also be mapped
 * to a client managed iova region.
 *
 * All Sync objects (local or export) are mapped to NvSciC2cPcie for signalling
 * (Write access), based on QNX security policy, there shall be only one
 * signaler allowed and, therefore, Sync objects are pinned/mapped just once.
 * Export Mem objects are mapped to NvSciC2cPcie for remote SoC to produce data
 * (Write access), also in the lack of N producer -> 1 consumer user-case,
 * remote Mem objects are pinned/mapped just once. However, Local Mem objects
 * which have Read access, can be mapped/pinned again. Essentially, all objects
 * requiring Write access by NvSciC2cPcie (PCIe device) are pinned/mapped once.
 *
 * Summary:
 *  i.   Types:
 *        a. Local Mem objects.
 *        b. Export Mem objects.
 *        c. Local Sync objects.
 *        d. Export Sync objects.
 *        e. Import objects.
 *  ii.  Mapping:
 *        a. Local Mem objects - dev managed (READ only).
 *        b. Export Mem objects - client managed (WRITE only).
 *            On Tegra PCIe EP:- compulsarily client managed.
 *            On Tegra PCIe RP:- Can be either dev managed or client managed.
 *            Choose client manged to be in symmetry with Tegra PCIe EP.
 *        c. Local Sync objects - Not mapped but pinned (tracked).
 *        d. Export Sync objects - client managed (WRITE ONLY).
 *            On Tegra PCIe EP:- compulsarily client managed.
 *            On Tegra PCIe RP:- Can be either dev managed (if Tegra PCIe RP use
 *            only eDMA for signaling, but we have use-cases for CPU signaling
 *            also from Tegra PCIe RP. Therefore choose client managed as it can
 *            satisfy CPU and eDMA signaling needs from remote Tegra PCIe EP.
 *        e. Import Objects - virtual objects pointing to exported objects by
 *            remote. Every exported object from a SoC must have a corresponding
 *            import object on remote SoC. Not mapped but pinned(tracked).
 */

/* forward declaration. */
struct driver_ctx_t;

/* Object type that shall be virtually mapped for PCIe access. */
enum vmap_obj_type {
	/* NvRmMemHandle (struct dma_buf *), aka, memobj.*/
	VMAP_OBJ_TYPE_MEM = STREAM_OBJ_TYPE_MEM,

	/* NvRmHost1xSyncpointHandle ( syncpt id), aka, memobj.*/
	VMAP_OBJ_TYPE_SYNC = STREAM_OBJ_TYPE_SYNC,

	/*(virtual) objects imported from remote SoC.*/
	VMAP_OBJ_TYPE_IMPORT,
};

/*
 * Permissions for pin/mapping Buff/Sync objs to PCIe device
 *
 * at the moment, WRITE for all EXPORT*
 * at the moment, READ for all LOCAL*
 */
enum vmap_obj_prot {
	/* read-only access by PCIe device. */
	VMAP_OBJ_PROT_READ = 1,

	/* write only access to PCIe device. */
	VMAP_OBJ_PROT_WRITE = 2,

	/*
	 * no use-case known today.
	 * VMAP_OBJ_PERM_READWRITE = 4,
	 */
};

/* Which IOVA to use for mapping Mem/Sync objs.*/
enum vmap_mngd {
	/*
	 * Stitching of all exported objects is done by reserving an IOVA region
	 * and mapping Mem and Sync objs to it. The reserved IOVA region is
	 * managed by client/user (NvSciC2cPcie) and use iommu apis to map Mem
	 * or Sync objects to the specific IOVA.
	 */
	VMAP_MNGD_CLIENT = 0,

	/*
	 * The IOVA is managed by pci dev, therefore dev managed. Used only for
	 * Mem objects (Local and possible for exported too).
	 */
	VMAP_MNGD_DEV,
};

/* Returned object attributes after mapping.*/
struct vmap_obj_attributes {
	enum vmap_obj_type type;
	s32 id;
	u64 iova;
	size_t size;
	size_t offsetof;
	/* only for local sync obj.*/
	u32 syncpt_id;
};

/* Parameters to map Mem object.*/
struct vmap_memobj_map_params {
	s32 fd;
	/* To allow mapping Export Mem objects as dev-managed - Tegra PCIe RP.*/
	enum vmap_mngd mngd;
	/* local/source mem as read-only, remote/export as write-only.*/
	enum vmap_obj_prot prot;
};

/* Parameters to map Sync object.*/
struct vmap_syncobj_map_params {
	s32 fd;
	/* client mngd only.*/
	enum vmap_mngd mngd;
	/* write-only.*/
	enum vmap_obj_prot prot;
	/* local sync objs will not be pinned to pcie address space.*/
	bool pin_reqd;
};

/* Parameters to map Import object.*/
struct vmap_importobj_map_params {
	u64 export_desc;
};

struct vmap_obj_map_params {
	enum vmap_obj_type type;
	union vmap_params {
		struct vmap_memobj_map_params memobj;
		struct vmap_syncobj_map_params syncobj;
		struct vmap_importobj_map_params importobj;
	} u;
};

/* Parameters to register Import object, as received from remote NvSciC2cPcie.*/
struct vmap_importobj_reg_params {
	u64 export_desc;
	u64 iova;
	size_t size;
	size_t offsetof;
};

/* Entry point for the virtual mapping sub-module/abstraction. */
int
vmap_init(struct driver_ctx_t *drv_ctx, void **vmap_h);

/* Exit point for nvscic2c-pcie vmap sub-module/abstraction. */
void
vmap_deinit(void **vmap_h);

/* Map objects to pcie device.*/
int
vmap_obj_map(void *vmap_h, struct vmap_obj_map_params *params,
	     struct vmap_obj_attributes *attrib);

/* Unmap objects from pcie device.*/
int
vmap_obj_unmap(void *vmap_h, enum vmap_obj_type type, u32 obj_id);

/* Increment reference count for objects. */
int
vmap_obj_getref(void *vmap_h, enum vmap_obj_type type, u32 obj_id);

/* Decrement reference count for objects. */
int
vmap_obj_putref(void *vmap_h, enum vmap_obj_type type, u32 obj_id);
#endif // __VMAP_H__
