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

#ifndef __VMAP_INTERNAL_H__
#define __VMAP_INTERNAL_H__


#include <linux/dma-iommu.h>
#include <linux/dma-buf.h>
#include <linux/pci.h>

#include "common.h"
#include "vmap.h"

/* forward declaration. */
struct vmap_ctx_t;

struct memobj_pin_t {
	/* Input param fd -> dma_buf to be mapped.*/
	struct dma_buf *dmabuf;

	enum vmap_mngd mngd;
	enum vmap_obj_prot prot;
	enum vmap_obj_type type;

	/* Input dmabuf mapped to pci-dev(dev mngd) or dummy dev(client mngd).*/
	struct dma_buf_attachment *attach;
	struct sg_table *sgt;
	enum dma_data_direction dir;

	/*
	 * [OUT]contiguous iova region obtained from client (iova-mngr).
	 * which input dmabuf is mapped to.
	 */
	void *iova_block_h;
	struct vmap_obj_attributes attrib;

	/*
	 * [OUT]
	 * Per scatter-list nent mapping -  used during free.
	 * Used for client-managed map only.
	 */
	u32 nr_nents;
	struct iova_nent {
		u64 iova;
		size_t len;
		bool mapped_iova;
	} *nents;
};

struct syncobj_pin_t {
	/* Input param fd -> syncpoint shim to be mapped.*/
	u32 syncpt_id;
	phys_addr_t phy_addr;

	enum vmap_mngd mngd;
	enum vmap_obj_prot prot;
	enum vmap_obj_type type;

	/* local sync objs do not require pinning to pcie address space.*/
	bool pin_reqd;

	/*
	 * [OUT]contiguous iova region obtained from client (iova-mngr)
	 * which syncpoint shim aper is mapped to.
	 */
	void *iova_block_h;
	struct vmap_obj_attributes attrib;
	bool mapped_iova;
};

struct importobj_reg_t {
	/*
	 * export descriptor and whereabouts of exported obj
	 * as received from remote end.
	 */
	u64 export_desc;

	/* times exported by remote, imported by local.*/
	u32 nr_export;
	u32 nr_import;

	struct vmap_obj_attributes attrib;
};

/* virtual mapping information for Mem obj.*/
struct memobj_map_ref {
	s32 obj_id;
	struct kref refcount;
	struct memobj_pin_t pin;
	struct vmap_ctx_t *vmap_ctx;
};

/* virtual mapping information for Sync obj. */
struct syncobj_map_ref {
	s32 obj_id;
	struct kref refcount;
	struct syncobj_pin_t pin;
	struct vmap_ctx_t *vmap_ctx;
};

/* virtual mapping information for Imported obj. */
struct importobj_map_ref {
	s32 obj_id;
	struct kref refcount;
	struct importobj_reg_t reg;
	struct vmap_ctx_t *vmap_ctx;
};

/* vmap subunit/abstraction context. */
struct vmap_ctx_t {
	/* pci-client abstraction handle.*/
	void *pci_client_h;

	/* comm-channel abstraction. */
	void *comm_channel_h;

	/* host1x platform device for syncpoint interfaces.*/
	struct platform_device *host1x_pdev;

	/*
	 * dummy platform device. - This has smmu disabled to get the
	 * physical addresses of exported Mem objects when using client
	 * managed mapping.
	 */
	struct platform_device *dummy_pdev;
	bool dummy_pdev_init;

	/*
	 * Management of Mem/Sync object Ids.
	 *
	 * All objects mapped are identified by - pin_id. IDR mechanism
	 * generates these IDs. We maintain separate book-keeping for
	 * Mem, Sync and Import objects. The ID shall overalap between
	 * Mem, Sync and Import objects.
	 *
	 * ID is the pinned handle returned to other units.
	 */
	struct idr mem_idr;
	struct idr sync_idr;
	struct idr import_idr;

	/* exclusive access to mem idr.*/
	struct mutex mem_idr_lock;
	/* exclusive access to sync idr.*/
	struct mutex sync_idr_lock;
	/* exclusive access to import idr.*/
	struct mutex import_idr_lock;
};

void
memobj_devmngd_unpin(struct vmap_ctx_t *vmap_ctx,
		     struct memobj_pin_t *pin);
int
memobj_devmngd_pin(struct vmap_ctx_t *vmap_ctx,
		   struct memobj_pin_t *pin);
void
memobj_clientmngd_unpin(struct vmap_ctx_t *vmap_ctx,
			struct memobj_pin_t *pin);
int
memobj_clientmngd_pin(struct vmap_ctx_t *vmap_ctx,
		      struct memobj_pin_t *pin);
void
memobj_unpin(struct vmap_ctx_t *vmap_ctx,
	     struct memobj_pin_t *pin);
int
memobj_pin(struct vmap_ctx_t *vmap_ctx,
	   struct memobj_pin_t *pin);
void
syncobj_clientmngd_unpin(struct vmap_ctx_t *vmap_ctx,
			 struct syncobj_pin_t *pin);
void
syncobj_unpin(struct vmap_ctx_t *vmap_ctx,
	      struct syncobj_pin_t *pin);
int
syncobj_pin(struct vmap_ctx_t *vmap_ctx,
	    struct syncobj_pin_t *pin);

#endif //__VMAP_INTERNAL_H__
