/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

#ifndef NVFS_H
#define NVFS_H

#include <linux/types.h>
#include <linux/delay.h>
#include <linux/blkdev.h>
#include <linux/cpumask.h>
#include <linux/scatterlist.h>
#include <linux/percpu-defs.h>
#include <linux/dma-direction.h>

#define REGSTR2(x) x##_register_nvfs_dma_ops
#define REGSTR(x)  REGSTR2(x)

#define UNREGSTR2(x) x##_unregister_nvfs_dma_ops
#define UNREGSTR(x)  UNREGSTR2(x)

#define REGISTER_FUNC REGSTR(MODULE_PREFIX)
#define UNREGISTER_FUNC UNREGSTR(MODULE_PREFIX)

#define NVFS_IO_ERR                    -1
#define NVFS_CPU_REQ                   -2

#define NVFS_HOLD_TIME_MS 1000

extern struct nvfs_dma_rw_ops *nvfs_ops;

extern atomic_t nvfs_shutdown;

DECLARE_PER_CPU(long, nvfs_n_ops);

static inline long nvfs_count_ops(void)
{
	int i;
	long sum = 0;

	for_each_possible_cpu(i)
		sum += per_cpu(nvfs_n_ops, i);
	return sum;
}

static inline bool nvfs_get_ops(void)
{
	if (nvfs_ops && !atomic_read(&nvfs_shutdown)) {
		this_cpu_inc(nvfs_n_ops);
		return true;
	}
	return false;
}

static inline void nvfs_put_ops(void)
{
	this_cpu_dec(nvfs_n_ops);
}

struct nvfs_dma_rw_ops {
	unsigned long long ft_bmap; // feature bitmap

	int (*nvfs_blk_rq_map_sg)(struct request_queue *q,
			struct request *req,
			struct scatterlist *sglist);

	int (*nvfs_dma_map_sg_attrs)(struct device *device,
			struct scatterlist *sglist,
			int nents,
			enum dma_data_direction dma_dir,
			unsigned long attrs);

	int (*nvfs_dma_unmap_sg)(struct device *device,
			struct scatterlist *sglist,
			int nents,
			enum dma_data_direction dma_dir);

	bool (*nvfs_is_gpu_page)(struct page *page);

	unsigned int (*nvfs_gpu_index)(struct page *page);

	unsigned int (*nvfs_device_priority)(struct device *dev, unsigned int gpu_index);
};

// feature list for dma_ops, values indicate bit pos
enum ft_bits {
	nvfs_ft_prep_sglist         = 1ULL << 0,
	nvfs_ft_map_sglist          = 1ULL << 1,
	nvfs_ft_is_gpu_page         = 1ULL << 2,
	nvfs_ft_device_priority     = 1ULL << 3,
};

// check features for use in registration with vendor drivers
#define NVIDIA_FS_CHECK_FT_SGLIST_PREP(ops)         ((ops)->ft_bmap & nvfs_ft_prep_sglist)
#define NVIDIA_FS_CHECK_FT_SGLIST_DMA(ops)          ((ops)->ft_bmap & nvfs_ft_map_sglist)
#define NVIDIA_FS_CHECK_FT_GPU_PAGE(ops)            ((ops)->ft_bmap & nvfs_ft_is_gpu_page)
#define NVIDIA_FS_CHECK_FT_DEVICE_PRIORITY(ops)     ((ops)->ft_bmap & nvfs_ft_device_priority)

int REGISTER_FUNC(struct nvfs_dma_rw_ops *ops);

void UNREGISTER_FUNC(void);

#endif /* NVFS_H */
