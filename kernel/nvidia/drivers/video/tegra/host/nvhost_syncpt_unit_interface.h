/*
 * Engine side synchronization support
 *
 * Copyright (c) 2016-2017, NVIDIA Corporation.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef NVHOST_SYNCPT_UNIT_INTERFACE_H
#define NVHOST_SYNCPT_UNIT_INTERFACE_H

#if IS_ENABLED(CONFIG_TEGRA_GRHOST_GOS)
#include <linux/nvmap_t19x.h>
#endif

struct platform_device;
struct nvhost_syncpt;

struct nvhost_syncpt_unit_interface {
	dma_addr_t start;
	uint32_t syncpt_page_size;

#if IS_ENABLED(CONFIG_TEGRA_GRHOST_GOS)
	int cv_dev_count;
	dma_addr_t cv_dev_address_table[NVMAP_MAX_GOS_PAGES];
#endif
};

dma_addr_t nvhost_syncpt_address(struct platform_device *engine_pdev, u32 id);

int nvhost_syncpt_unit_interface_init(struct platform_device *engine_pdev);

#endif
