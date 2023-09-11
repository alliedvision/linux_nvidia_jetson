/*
 * Copyright (c) 2022, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef __NVPVA_SYNCPT_H__
#define __NVPVA_SYNCPT_H__

void nvpva_syncpt_put_ref_ext(struct platform_device *pdev,
			      u32 id);
dma_addr_t nvpva_syncpt_address(struct platform_device *pdev, u32 id,
				bool rw);
void nvpva_syncpt_unit_interface_deinit(struct platform_device *pdev,
					struct platform_device *paux_dev);
int nvpva_syncpt_unit_interface_init(struct platform_device *pdev,
				     struct platform_device *paux_dev);
u32 nvpva_get_syncpt_client_managed(struct platform_device *pdev,
				    const char *syncpt_name);
int nvpva_map_region(struct device *dev,
		     phys_addr_t start,
		     size_t size,
		     dma_addr_t *sp_start,
		     u32 attr);
int nvpva_unmap_region(struct device *dev,
		       dma_addr_t addr,
		       size_t size,
		       u32 attr);
#endif
