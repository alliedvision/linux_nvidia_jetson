/*
 * Copyright (c) 2017-2021, NVIDIA CORPORATION.  All rights reserved.
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

#include <linux/dma-mapping.h>
#include <linux/of_platform.h>
#include <linux/slab.h>

#include <nvgpu/gk20a.h>
#include <nvgpu/nvhost.h>
#include <nvgpu/dma.h>

#include "nvhost_priv.h"

#include "os_linux.h"
#include "module.h"

void nvgpu_free_nvhost_dev(struct gk20a *g)
{
	if (nvgpu_iommuable(g) && !nvgpu_is_enabled(g, NVGPU_SUPPORT_NVLINK)) {
		struct device *dev = dev_from_gk20a(g);
		struct nvgpu_mem *mem = &g->syncpt_mem;

		dma_unmap_sg_attrs(dev, mem->priv.sgt->sgl, 1,
				DMA_BIDIRECTIONAL, DMA_ATTR_SKIP_CPU_SYNC);
		sg_free_table(mem->priv.sgt);
		nvgpu_kfree(g, mem->priv.sgt);
	}
	nvgpu_kfree(g, g->nvhost);
}

int nvgpu_nvhost_create_symlink(struct gk20a *g)
{
	struct device *dev = dev_from_gk20a(g);
	int err = 0;

	if (g->nvhost &&
			(dev->parent != &g->nvhost->host1x_pdev->dev)) {
		err = sysfs_create_link(&g->nvhost->host1x_pdev->dev.kobj,
				&dev->kobj,
				dev_name(dev));
	}

	return err;
}

void nvgpu_nvhost_remove_symlink(struct gk20a *g)
{
	struct device *dev = dev_from_gk20a(g);

	if (g->nvhost &&
			(dev->parent != &g->nvhost->host1x_pdev->dev)) {
		sysfs_remove_link(&g->nvhost->host1x_pdev->dev.kobj,
				  dev_name(dev));
	}
}

int nvgpu_nvhost_syncpt_init(struct gk20a *g)
{
	int err = 0;
	struct nvgpu_mem *mem = &g->syncpt_mem;

	if (!nvgpu_has_syncpoints(g))
		return -ENOSYS;

	err = nvgpu_get_nvhost_dev(g);
	if (err) {
		nvgpu_err(g, "host1x device not available");
		err = -ENOSYS;
		goto fail_sync;
	}

	err = nvgpu_nvhost_get_syncpt_aperture(
			g->nvhost,
			&g->syncpt_unit_base,
			&g->syncpt_unit_size);
	if (err) {
		nvgpu_err(g, "Failed to get syncpt interface");
		err = -ENOSYS;
		goto fail_sync;
	}

	/*
	 * If IOMMU is enabled, create iova for syncpt region. This iova is then
	 * used to create nvgpu_mem for syncpt by nvgpu_mem_create_from_phys.
	 * For entire syncpt shim read-only mapping full iova range is used and
	 * for a given syncpt read-write mapping only a part of iova range is
	 * used. Instead of creating another variable to store the sgt,
	 * g->syncpt_mem's priv field is used which later on is needed for
	 * freeing the mapping in deinit.
	 */
	if (nvgpu_iommuable(g) && !nvgpu_is_enabled(g, NVGPU_SUPPORT_NVLINK)) {
		struct device *dev = dev_from_gk20a(g);
		struct scatterlist *sg;

		mem->priv.sgt = nvgpu_kzalloc(g, sizeof(struct sg_table));
		if (!mem->priv.sgt) {
			err = -ENOMEM;
			goto fail_sync;
		}

		err = sg_alloc_table(mem->priv.sgt, 1, GFP_KERNEL);
		if (err) {
			err = -ENOMEM;
			goto fail_kfree;
		}
		sg = mem->priv.sgt->sgl;
		sg_set_page(sg, phys_to_page(g->syncpt_unit_base),
				g->syncpt_unit_size, 0);
		err = dma_map_sg_attrs(dev, sg, 1,
				DMA_BIDIRECTIONAL, DMA_ATTR_SKIP_CPU_SYNC);
		/* dma_map_sg_attrs returns 0 on errors */
		if (err == 0) {
			nvgpu_err(g, "iova creation for syncpoint failed");
			err = -ENOMEM;
			goto fail_sgt;
		}
		g->syncpt_unit_base = sg_dma_address(sg);
	}

	g->syncpt_size =
		nvgpu_nvhost_syncpt_unit_interface_get_byte_offset(g, 1);
	nvgpu_info(g, "syncpt_unit_base %llx syncpt_unit_size %zx size %x\n",
			g->syncpt_unit_base, g->syncpt_unit_size,
			g->syncpt_size);
	return 0;
fail_sgt:
	sg_free_table(mem->priv.sgt);
fail_kfree:
	nvgpu_kfree(g, mem->priv.sgt);
fail_sync:
	nvgpu_set_enabled(g, NVGPU_HAS_SYNCPOINTS, false);
	return err;
}
