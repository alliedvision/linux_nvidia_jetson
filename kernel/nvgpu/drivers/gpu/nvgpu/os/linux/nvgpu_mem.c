/*
 * Copyright (c) 2017-2020, NVIDIA CORPORATION.  All rights reserved.
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

#include <nvgpu/dma.h>
#include <nvgpu/gmmu.h>
#include <nvgpu/nvgpu_mem.h>
#include <nvgpu/page_allocator.h>
#include <nvgpu/log.h>
#include <nvgpu/bug.h>
#include <nvgpu/enabled.h>
#include <nvgpu/kmem.h>
#include <nvgpu/vidmem.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/string.h>
#include <nvgpu/nvgpu_sgt.h>
#include <nvgpu/nvgpu_sgt_os.h>

#include <nvgpu/linux/dma.h>

#include <linux/vmalloc.h>
#include <linux/dma-mapping.h>

#include "os_linux.h"
#include "dmabuf_vidmem.h"

#include "platform_gk20a.h"

#ifndef DMA_ERROR_CODE
#define DMA_ERROR_CODE DMA_MAPPING_ERROR
#endif

static u64 __nvgpu_sgl_ipa(struct gk20a *g, void *sgl)
{
	return sg_phys((struct scatterlist *)sgl);
}

static u64 __nvgpu_sgl_phys(struct gk20a *g, void *sgl)
{
	struct device *dev = dev_from_gk20a(g);
	struct gk20a_platform *platform = gk20a_get_platform(dev);
	u64 ipa = sg_phys((struct scatterlist *)sgl);

	if (platform->phys_addr)
		return platform->phys_addr(g, ipa, NULL);

	return ipa;
}

/*
 * Obtain a SYSMEM address from a Linux SGL. This should eventually go away
 * and/or become private to this file once all bad usages of Linux SGLs are
 * cleaned up in the driver.
 */
u64 nvgpu_mem_get_addr_sgl(struct gk20a *g, struct scatterlist *sgl)
{
	if (nvgpu_is_enabled(g, NVGPU_MM_USE_PHYSICAL_SG) ||
	    !nvgpu_iommuable(g))
		return g->ops.mm.gmmu.gpu_phys_addr(g, NULL,
			__nvgpu_sgl_phys(g, (void *)sgl));

	if (sg_dma_address(sgl) == 0)
		return g->ops.mm.gmmu.gpu_phys_addr(g, NULL,
			__nvgpu_sgl_phys(g, (void *)sgl));

	if (sg_dma_address(sgl) == DMA_ERROR_CODE)
		return 0;

	return nvgpu_mem_iommu_translate(g, sg_dma_address(sgl));
}

/*
 * Obtain the address the GPU should use from the %mem assuming this is a SYSMEM
 * allocation.
 */
static u64 nvgpu_mem_get_addr_sysmem(struct gk20a *g, struct nvgpu_mem *mem)
{
	return nvgpu_mem_get_addr_sgl(g, mem->priv.sgt->sgl);
}

/*
 * Return the base address of %mem. Handles whether this is a VIDMEM or SYSMEM
 * allocation.
 *
 * Note: this API does not make sense to use for _VIDMEM_ buffers with greater
 * than one scatterlist chunk. If there's more than one scatterlist chunk then
 * the buffer will not be contiguous. As such the base address probably isn't
 * very useful. This is true for SYSMEM as well, if there's no IOMMU.
 *
 * However! It _is_ OK to use this on discontiguous sysmem buffers _if_ there's
 * an IOMMU present and enabled for the GPU.
 *
 * %attrs can be NULL. If it is not NULL then it may be inspected to determine
 * if the address needs to be modified before writing into a PTE.
 */
u64 nvgpu_mem_get_addr(struct gk20a *g, struct nvgpu_mem *mem)
{
#ifdef CONFIG_NVGPU_DGPU
	struct nvgpu_page_alloc *alloc;

	if (mem->aperture == APERTURE_SYSMEM)
		return nvgpu_mem_get_addr_sysmem(g, mem);

	/*
	 * Otherwise get the vidmem address.
	 */
	alloc = mem->vidmem_alloc;

	/* This API should not be used with > 1 chunks */
	WARN_ON(alloc->nr_chunks != 1);

	return alloc->base;
#else
	if (mem->aperture == APERTURE_SYSMEM)
		return nvgpu_mem_get_addr_sysmem(g, mem);

	return 0;
#endif
}

/*
 * This should only be used on contiguous buffers regardless of whether
 * there's an IOMMU present/enabled. This applies to both SYSMEM and
 * VIDMEM.
 */
u64 nvgpu_mem_get_phys_addr(struct gk20a *g, struct nvgpu_mem *mem)
{
#ifdef CONFIG_NVGPU_DGPU
	/*
	 * For a VIDMEM buf, this is identical to simply get_addr() so just fall
	 * back to that.
	 */
	if (mem->aperture == APERTURE_VIDMEM)
		return nvgpu_mem_get_addr(g, mem);
#endif

	return __nvgpu_sgl_phys(g, (void *)mem->priv.sgt->sgl);
}

/*
 * Be careful how you use this! You are responsible for correctly freeing this
 * memory.
 */
int nvgpu_mem_create_from_mem(struct gk20a *g,
			      struct nvgpu_mem *dest, struct nvgpu_mem *src,
			      u64 start_page, size_t nr_pages)
{
	int ret;
	u64 start = start_page * NVGPU_CPU_PAGE_SIZE;
	u64 size = nr_pages * NVGPU_CPU_PAGE_SIZE;
	dma_addr_t new_iova;

	if (src->aperture != APERTURE_SYSMEM)
		return -EINVAL;

	/* Some silly things a caller might do... */
	if (size > src->size)
		return -EINVAL;
	if ((start + size) > src->size)
		return -EINVAL;

	dest->mem_flags = src->mem_flags | NVGPU_MEM_FLAG_SHADOW_COPY;
	dest->aperture  = src->aperture;
	dest->skip_wmb  = src->skip_wmb;
	dest->size      = size;

	/* Re-use the CPU mapping only if the mapping was made by the DMA API */
	if (!(src->priv.flags & NVGPU_DMA_NO_KERNEL_MAPPING))
		dest->cpu_va = src->cpu_va + (NVGPU_CPU_PAGE_SIZE * start_page);

	dest->priv.pages = src->priv.pages + start_page;
	dest->priv.flags = src->priv.flags;

	new_iova = sg_dma_address(src->priv.sgt->sgl) ?
		sg_dma_address(src->priv.sgt->sgl) + start : 0;

	/*
	 * Make a new SG table that is based only on the subset of pages that
	 * is passed to us. This table gets freed by the dma free routines.
	 */
	if (src->priv.flags & NVGPU_DMA_NO_KERNEL_MAPPING)
		ret = nvgpu_get_sgtable_from_pages(g, &dest->priv.sgt,
						   src->priv.pages + start_page,
						   new_iova, size);
	else
		ret = nvgpu_get_sgtable(g, &dest->priv.sgt, dest->cpu_va,
					new_iova, size);

	return ret;
}

static void *nvgpu_mem_linux_sgl_next(void *sgl)
{
	return (void *)sg_next((struct scatterlist *)sgl);
}

static u64 nvgpu_mem_linux_sgl_ipa(struct gk20a *g, void *sgl)
{
	return __nvgpu_sgl_ipa(g, sgl);
}

static u64 nvgpu_mem_linux_sgl_ipa_to_pa(struct gk20a *g,
		void *sgl, u64 ipa, u64 *pa_len)
{
	struct device *dev = dev_from_gk20a(g);
	struct gk20a_platform *platform = gk20a_get_platform(dev);

	if (platform->phys_addr)
		return platform->phys_addr(g, ipa, pa_len);

	return ipa;
}

static u64 nvgpu_mem_linux_sgl_phys(struct gk20a *g, void *sgl)
{
	return (u64)__nvgpu_sgl_phys(g, sgl);
}

static u64 nvgpu_mem_linux_sgl_dma(void *sgl)
{
	return (u64)sg_dma_address((struct scatterlist *)sgl);
}

static u64 nvgpu_mem_linux_sgl_length(void *sgl)
{
	return (u64)((struct scatterlist *)sgl)->length;
}

static u64 nvgpu_mem_linux_sgl_gpu_addr(struct gk20a *g,
					void *sgl,
					struct nvgpu_gmmu_attrs *attrs)
{
	if (sg_dma_address((struct scatterlist *)sgl) == 0)
		return g->ops.mm.gmmu.gpu_phys_addr(g, attrs,
				__nvgpu_sgl_phys(g, sgl));

	if (sg_dma_address((struct scatterlist *)sgl) == DMA_ERROR_CODE)
		return 0;

	return nvgpu_mem_iommu_translate(g,
				sg_dma_address((struct scatterlist *)sgl));
}

static bool nvgpu_mem_linux_sgt_iommuable(struct gk20a *g,
					  struct nvgpu_sgt *sgt)
{
	if (nvgpu_is_enabled(g, NVGPU_MM_USE_PHYSICAL_SG))
		return false;
	return true;
}

static void nvgpu_mem_linux_sgl_free(struct gk20a *g, struct nvgpu_sgt *sgt)
{
	/*
	 * Free this SGT. All we do is free the passed SGT. The actual Linux
	 * SGT/SGL needs to be freed separately.
	 */
	nvgpu_kfree(g, sgt);
}

static const struct nvgpu_sgt_ops nvgpu_linux_sgt_ops = {
	.sgl_next      = nvgpu_mem_linux_sgl_next,
	.sgl_phys      = nvgpu_mem_linux_sgl_phys,
	.sgl_ipa       = nvgpu_mem_linux_sgl_ipa,
	.sgl_ipa_to_pa = nvgpu_mem_linux_sgl_ipa_to_pa,
	.sgl_dma       = nvgpu_mem_linux_sgl_dma,
	.sgl_length    = nvgpu_mem_linux_sgl_length,
	.sgl_gpu_addr  = nvgpu_mem_linux_sgl_gpu_addr,
	.sgt_iommuable = nvgpu_mem_linux_sgt_iommuable,
	.sgt_free      = nvgpu_mem_linux_sgl_free,
};

#ifdef CONFIG_NVGPU_DGPU
static struct nvgpu_sgt *__nvgpu_mem_get_sgl_from_vidmem(
	struct gk20a *g,
	struct scatterlist *linux_sgl)
{
	struct nvgpu_page_alloc *vidmem_alloc;

	vidmem_alloc = nvgpu_vidmem_get_page_alloc(linux_sgl);
	if (!vidmem_alloc)
		return NULL;

	return &vidmem_alloc->sgt;
}
#endif

struct nvgpu_sgt *nvgpu_linux_sgt_create(struct gk20a *g, struct sg_table *sgt)
{
	struct nvgpu_sgt *nvgpu_sgt;
	struct scatterlist *linux_sgl = sgt->sgl;

#ifdef CONFIG_NVGPU_DGPU
	if (nvgpu_addr_is_vidmem_page_alloc(sg_dma_address(linux_sgl)))
		return __nvgpu_mem_get_sgl_from_vidmem(g, linux_sgl);
#endif

	nvgpu_sgt = nvgpu_kzalloc(g, sizeof(*nvgpu_sgt));
	if (!nvgpu_sgt)
		return NULL;

	nvgpu_log(g, gpu_dbg_sgl, "Making Linux SGL!");

	nvgpu_sgt->sgl = (void *)linux_sgl;
	nvgpu_sgt->ops = &nvgpu_linux_sgt_ops;

	return nvgpu_sgt;
}

struct nvgpu_sgt *nvgpu_sgt_os_create_from_mem(struct gk20a *g,
					       struct nvgpu_mem *mem)
{
	return nvgpu_linux_sgt_create(g, mem->priv.sgt);
}
