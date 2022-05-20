/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021, NVIDIA CORPORATION. All rights reserved.
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
#ifndef __NVMAP_EXPORTS_H
#define __NVMAP_EXPORTS_H

void *nvmap_dma_alloc_attrs(struct device *dev, size_t size,
			    dma_addr_t *dma_handle, gfp_t flag,
			    unsigned long attrs);
void nvmap_dma_free_attrs(struct device *dev, size_t size, void *cpu_addr,
			  dma_addr_t dma_handle, unsigned long attrs);
extern struct device tegra_vpr_dev;
#endif /* __NVMAP_EXPORTS_H */
