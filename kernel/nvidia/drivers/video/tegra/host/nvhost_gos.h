/*
 * GoS support
 *
 * Copyright (c) 2016-2020, NVIDIA Corporation.  All rights reserved.
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

#ifndef NVHOST_GOS_H
#define NVHOST_GOS_H

struct platform_device;

#if IS_ENABLED(CONFIG_TEGRA_GRHOST_GOS)

int nvhost_syncpt_get_cv_dev_address_table(struct platform_device *engine_pdev,
				   int *count,
				   dma_addr_t **table);

int nvhost_syncpt_get_gos(struct platform_device *engine_pdev,
			      u32 syncpt_id,
			      u32 *gos_id,
			      u32 *gos_offset);

dma_addr_t nvhost_syncpt_gos_address(struct platform_device *engine_pdev,
				     u32 syncpt_id);

int nvhost_syncpt_alloc_gos_backing(struct platform_device *engine_pdev,
				     u32 syncpt_id);
int nvhost_syncpt_release_gos_backing(struct nvhost_syncpt *sp,
				      u32 syncpt_id);

#else

static inline int nvhost_syncpt_get_cv_dev_address_table(struct platform_device *engine_pdev,
					          int *count, dma_addr_t **table)
{
	return -ENODEV;
}

static inline int nvhost_syncpt_get_gos(struct platform_device *engine_pdev,
			      u32 syncpt_id,
			      u32 *gos_id,
			      u32 *gos_offset)
{
	return -ENODEV;
}

static inline dma_addr_t nvhost_syncpt_gos_address(struct platform_device *engine_pdev,
				     u32 syncpt_id)
{
	return (dma_addr_t) 0;
}

static inline int nvhost_syncpt_alloc_gos_backing(struct platform_device *engine_pdev,
				     u32 syncpt_id)
{
	return -ENODEV;
}

static inline int nvhost_syncpt_release_gos_backing(struct nvhost_syncpt *sp,
				      u32 syncpt_id)
{
	return -ENODEV;
}

#endif

#endif
