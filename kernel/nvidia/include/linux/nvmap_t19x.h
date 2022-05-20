/*
 * include/linux/nvmap_t19x.h
 *
 * structure declarations for nvmem and nvmap user-space ioctls
 *
 * Copyright (c) 2009-2021, NVIDIA CORPORATION. All rights reserved.
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

#include <linux/types.h>
#include <linux/bitmap.h>
#include <linux/device.h>
#include <linux/spinlock.h>

#ifndef _LINUX_NVMAP_T19x_H
#define _LINUX_NVMAP_T19x_H

#define NVMAP_HEAP_CARVEOUT_CVSRAM  (1ul<<25)
#define NVMAP_HEAP_CARVEOUT_GOS     (1ul<<24)

int nvmap_register_cvsram_carveout(struct device *dma_dev,
		phys_addr_t base, size_t size,
		int (*pmops_busy)(void), int (*pmops_idle)(void));

#define NVMAP_MAX_GOS_COUNT		64
#define NVMAP_MAX_GOS_PAGES		12

struct cv_dev_info {
	struct device_node *np;
	struct sg_table *sgt;
	void *cpu_addr;
	int idx; /* index uses to identify the gos area */
	int count; /* number of sgt */
	spinlock_t goslock;
	DECLARE_BITMAP(gosmap, NVMAP_MAX_GOS_COUNT);
};

struct cv_dev_info *nvmap_fetch_cv_dev_info(struct device *dev);

int nvmap_alloc_gos_slot(struct device *dev,
		u32 *return_index,
		u32 *return_offset,
		u32 **return_address);
void nvmap_free_gos_slot(u32 index, u32 offset);

struct nvmap_handle_t19x {
	atomic_t nc_pin; /* no. of pins from non io coherent devices */
};

#if defined(NVMAP_LOADABLE_MODULE)
int nvmap_t19x_init(void);
void nvmap_t19x_deinit(void);
#endif

extern bool nvmap_version_t19x;

#endif /* _LINUX_NVMAP_T19x_H */
