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

#ifndef __UAPI_NVSCIC2C_PCIE_IOCTL_H__
#define __UAPI_NVSCIC2C_PCIE_IOCTL_H__

#include <linux/ioctl.h>
#include <linux/types.h>

#if !defined(__KERNEL__)
#define __user
#include <stdbool.h>
#endif

#define MAX_NAME_SZ		(32)

/* Link status between the two peers - encapsulates PCIe link also.*/
enum nvscic2c_pcie_link {
	NVSCIC2C_PCIE_LINK_DOWN = 0,
	NVSCIC2C_PCIE_LINK_UP,
};

/**
 * stream extensions - object type.
 */
enum nvscic2c_pcie_obj_type {
	NVSCIC2C_PCIE_OBJ_TYPE_INVALID = 0,

	/* local NvRmMemHandle(tegra) /NvRmHandle(x86) obj. */
	NVSCIC2C_PCIE_OBJ_TYPE_SOURCE_MEM,

	/* Exported NvRmMemHandle(tegra) /NvRmHandle(x86) obj. */
	NVSCIC2C_PCIE_OBJ_TYPE_TARGET_MEM,

	/* local NvRmHost1xSyncpoint(tegra) /GPU Semaphore(x86) obj. */
	NVSCIC2C_PCIE_OBJ_TYPE_LOCAL_SYNC,

	/* Exported NvRmHost1xSyncpoint(tegra) /GPU Semaphore(x86) obj. */
	NVSCIC2C_PCIE_OBJ_TYPE_REMOTE_SYNC,

	/* (virtual) objects imported from remote SoC. */
	NVSCIC2C_PCIE_OBJ_TYPE_IMPORT,

	NVSCIC2C_PCIE_OBJ_TYPE_MAXIMUM,
};

/**
 * PCIe aperture and PCIe shared memory
 * are divided in different C2C endpoints.
 * Data structure represents endpoint's
 * physical address and size.
 */
struct nvscic2c_pcie_endpoint_mem_info {
	/* would be one of the enum nvscic2c_mem_type.*/
	__u32 offset;

	/* size of this memory type device would like user-space to map.*/
	__u32 size;
};

/**
 * NvSciIpc endpoint information relayed to UMD. This information
 * is per endpoint which shall allow UMD to mmap the endpoint's
 * send, recv and pcie link area in user-space.
 */
struct nvscic2c_pcie_endpoint_info {
	__u32 nframes;
	__u32 frame_size;
	struct nvscic2c_pcie_endpoint_mem_info peer;
	struct nvscic2c_pcie_endpoint_mem_info self;
	struct nvscic2c_pcie_endpoint_mem_info link;
};

/**
 * stream extensions - Pin/Map.
 */
struct nvscic2c_pcie_map_in_arg {
	/*
	 * Mem obj - NvRmMemHandle FD. Sync obj - NvRmHost1xSyncpointHandle FD.
	 */
	__s32 fd;
	__u32 pad;
};

struct nvscic2c_pcie_map_out_arg {
	__s32 handle;
	__u32 pad;
};

struct nvscic2c_pcie_map_obj_args {
	__s32 obj_type;
	__u32 pad;
	struct nvscic2c_pcie_map_in_arg in;
	struct nvscic2c_pcie_map_out_arg out;
};

/**
 * stream extensions - Export.
 */
struct nvscic2c_pcie_export_in_arg {
	__s32 handle;
	__u32 pad;
};

struct nvscic2c_pcie_export_out_arg {
	__u64 desc;
};

struct nvscic2c_pcie_export_obj_args {
	__s32 obj_type;
	__u32 pad;
	struct nvscic2c_pcie_export_in_arg in;
	struct nvscic2c_pcie_export_out_arg out;
};

/**
 * stream extensions - Import.
 */
struct nvscic2c_pcie_import_in_arg {
	__u64 desc;
};

struct nvscic2c_pcie_import_out_arg {
	__s32 handle;
	__u32 pad;
};

struct nvscic2c_pcie_import_obj_args {
	__s32 obj_type;
	__u32 pad;
	struct nvscic2c_pcie_import_in_arg in;
	struct nvscic2c_pcie_import_out_arg out;
};

/**
 * stream extensions - Free Pinned Or Imported objects.
 */
struct nvscic2c_pcie_free_obj_args {
	__s32 obj_type;
	__s32 handle;
};

/**
 * stream extensions - one transfer/copy unit.
 */
struct nvscic2c_pcie_flush_range {
	__s32 src_handle;
	__s32 dst_handle;
	__u64 offset;
	__u64 size;
};

/*
 * @local_post_fences: user memory atleast of size:
 *  num_local_post_fences * sizeof(__s32) - local sync handles
 *
 * @remote_post_fences: user memory atleast of size:
 *  num_remote_post_fences * sizeof(__s32) - import sync handles
 *
 * @copy_requests: user memory atleast of size:
 *  num_flush_ranges * sizeof(struct nvscic2c_pcie_flush_range)
 */
struct nvscic2c_pcie_submit_copy_args {
	__u64 num_local_post_fences;
	__u64 local_post_fences;
	__u64 num_remote_post_fences;
	__u64 remote_post_fences;
	__u64 num_flush_ranges;
	__u64 flush_ranges;
	__u64 remote_post_fence_values;
};

/**
 * stream extensions - Pass upper limit for the total possible outstanding
 * submit copy requests.
 * @max_copy_requests: Maximum outstanding @nvscic2c_pcie_submit_copy_args.
 * @max_flush_ranges: Maximum @nvscic2c_pcie_flush_range possible for each
 *  of the @max_copy_requests (@nvscic2c_pcie_submit_copy_args)
 * @max_post_fences: Maximum post-fences possible for each of the
 *  @max_copy_requests (@nvscic2c_pcie_submit_copy_args)
 */
struct nvscic2c_pcie_max_copy_args {
	__u64 max_copy_requests;
	__u64 max_flush_ranges;
	__u64 max_post_fences;
};

struct nvscic2c_link_change_ack {
	bool done;
};

/* Only to facilitate calculation of maximum size of ioctl arguments.*/
union nvscic2c_pcie_ioctl_arg_max_size {
	struct nvscic2c_pcie_max_copy_args mc;
	struct nvscic2c_pcie_submit_copy_args cr;
	struct nvscic2c_pcie_free_obj_args fo;
	struct nvscic2c_pcie_import_obj_args io;
	struct nvscic2c_pcie_export_obj_args eo;
	struct nvscic2c_pcie_map_obj_args mp;
	struct nvscic2c_pcie_endpoint_info ep;
	struct nvscic2c_link_change_ack ack;
};

/* IOCTL magic number - seen available in ioctl-number.txt*/
#define NVSCIC2C_PCIE_IOCTL_MAGIC    0xC2

#define NVSCIC2C_PCIE_IOCTL_GET_INFO \
	_IOWR(NVSCIC2C_PCIE_IOCTL_MAGIC, 1,\
	      struct nvscic2c_pcie_endpoint_info)

/**
 * notify remote
 */
#define NVSCIC2C_PCIE_IOCTL_NOTIFY_REMOTE \
	_IO(NVSCIC2C_PCIE_IOCTL_MAGIC, 2)

/**
 * Pin/Map Mem or Sync objects.
 */
#define NVSCIC2C_PCIE_IOCTL_MAP \
	_IOWR(NVSCIC2C_PCIE_IOCTL_MAGIC, 3,\
	      struct nvscic2c_pcie_map_obj_args)

/**
 * Get Export descriptor for Target/Remote Mem/Sync objects.
 */
#define NVSCIC2C_PCIE_IOCTL_GET_AUTH_TOKEN \
	_IOWR(NVSCIC2C_PCIE_IOCTL_MAGIC, 4,\
	      struct nvscic2c_pcie_export_obj_args)

/**
 * Get Handle from the imported export descriptor.
 */
#define NVSCIC2C_PCIE_IOCTL_GET_HANDLE \
	_IOWR(NVSCIC2C_PCIE_IOCTL_MAGIC, 5,\
	      struct nvscic2c_pcie_import_obj_args)

/**
 * Free the Mapped/Pinned Source, Target or Imported Mem or Sync object handle.
 */
#define NVSCIC2C_PCIE_IOCTL_FREE \
	_IOW(NVSCIC2C_PCIE_IOCTL_MAGIC, 6,\
	      struct nvscic2c_pcie_free_obj_args)

/**
 * Submit a Copy request for transfer.
 */
#define NVSCIC2C_PCIE_IOCTL_SUBMIT_COPY_REQUEST \
	_IOW(NVSCIC2C_PCIE_IOCTL_MAGIC, 7,\
	      struct nvscic2c_pcie_submit_copy_args)

/**
 * Set the maximum possible outstanding copy requests that can be submitted.
 */
#define NVSCIC2C_PCIE_IOCTL_MAX_COPY_REQUESTS \
	_IOW(NVSCIC2C_PCIE_IOCTL_MAGIC, 8,\
	      struct nvscic2c_pcie_max_copy_args)

#define NVSCIC2C_PCIE_LINK_STATUS_CHANGE_ACK \
	_IOW(NVSCIC2C_PCIE_IOCTL_MAGIC, 9,\
	     struct nvscic2c_link_change_ack)

#define NVSCIC2C_PCIE_IOCTL_NUMBER_MAX 9

#endif /*__UAPI_NVSCIC2C_PCIE_IOCTL_H__*/
