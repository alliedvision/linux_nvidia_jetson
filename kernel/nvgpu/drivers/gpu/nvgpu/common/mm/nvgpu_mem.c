/*
 * Copyright (c) 2017-2020, NVIDIA CORPORATION.  All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include <nvgpu/bug.h>
#include <nvgpu/kmem.h>
#include <nvgpu/nvgpu_mem.h>
#include <nvgpu/nvgpu_sgt.h>
#include <nvgpu/dma.h>
#include <nvgpu/vidmem.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/pramin.h>
#include <nvgpu/string.h>

/*
 * Make sure to use the right coherency aperture if you use this function! This
 * will not add any checks. If you want to simply use the default coherency then
 * use nvgpu_aperture_mask().
 */
u32 nvgpu_aperture_mask_raw(struct gk20a *g, enum nvgpu_aperture aperture,
			    u32 sysmem_mask, u32 sysmem_coh_mask,
			    u32 vidmem_mask)
{
	u32 ret_mask = 0;

	if ((aperture == APERTURE_INVALID) || (aperture >= APERTURE_MAX_ENUM)) {
		nvgpu_do_assert_print(g, "Bad aperture");
		return 0;
	}

	/*
	 * Some iGPUs treat sysmem (i.e SoC DRAM) as vidmem. In these cases the
	 * "sysmem" aperture should really be translated to VIDMEM.
	 */
	if (!nvgpu_is_enabled(g, NVGPU_MM_HONORS_APERTURE)) {
		aperture = APERTURE_VIDMEM;
	}

	switch (aperture) {
	case APERTURE_SYSMEM_COH:
		ret_mask = sysmem_coh_mask;
		break;
	case APERTURE_SYSMEM:
		ret_mask = sysmem_mask;
		break;
	case APERTURE_VIDMEM:
		ret_mask = vidmem_mask;
		break;
	default:
		nvgpu_do_assert_print(g, "Bad aperture");
		ret_mask = 0;
		break;
	}
	return ret_mask;
}

u32 nvgpu_aperture_mask(struct gk20a *g, struct nvgpu_mem *mem,
			u32 sysmem_mask, u32 sysmem_coh_mask, u32 vidmem_mask)
{
	enum nvgpu_aperture ap = mem->aperture;

	return nvgpu_aperture_mask_raw(g, ap,
				       sysmem_mask,
				       sysmem_coh_mask,
				       vidmem_mask);
}

bool nvgpu_aperture_is_sysmem(enum nvgpu_aperture ap)
{
	return (ap == APERTURE_SYSMEM_COH) || (ap == APERTURE_SYSMEM);
}

bool nvgpu_mem_is_sysmem(struct nvgpu_mem *mem)
{
	return nvgpu_aperture_is_sysmem(mem->aperture);
}

u64 nvgpu_mem_iommu_translate(struct gk20a *g, u64 phys)
{
	/* ensure it is not vidmem allocation */
#ifdef CONFIG_NVGPU_DGPU
	WARN_ON(nvgpu_addr_is_vidmem_page_alloc(phys));
#endif

	if (nvgpu_iommuable(g) && (g->ops.mm.gmmu.get_iommu_bit != NULL)) {
		return phys | (1ULL << g->ops.mm.gmmu.get_iommu_bit(g));
	}

	return phys;
}

u32 nvgpu_mem_rd32(struct gk20a *g, struct nvgpu_mem *mem, u64 w)
{
	u32 data = 0;

	if (mem->aperture == APERTURE_SYSMEM) {
		u32 *ptr = mem->cpu_va;

NVGPU_COV_WHITELIST_BLOCK_BEGIN(false_positive, 1, NVGPU_MISRA(Rule, 10_3), "Bug 2277532")
NVGPU_COV_WHITELIST_BLOCK_BEGIN(false_positive, 1, NVGPU_MISRA(Rule, 14_4), "Bug 2277532")
NVGPU_COV_WHITELIST_BLOCK_BEGIN(false_positive, 1, NVGPU_MISRA(Rule, 15_6), "Bug 2277532")
		WARN_ON(ptr == NULL);
NVGPU_COV_WHITELIST_BLOCK_END(NVGPU_MISRA(Rule, 10_3))
NVGPU_COV_WHITELIST_BLOCK_END(NVGPU_MISRA(Rule, 14_4))
NVGPU_COV_WHITELIST_BLOCK_END(NVGPU_MISRA(Rule, 15_6))
		data = ptr[w];
	}
#ifdef CONFIG_NVGPU_DGPU
	else if (mem->aperture == APERTURE_VIDMEM) {
		nvgpu_pramin_rd_n(g, mem, w * (u64)sizeof(u32),
				(u64)sizeof(u32), &data);
	}
#endif
	else {
		nvgpu_do_assert_print(g, "Accessing unallocated nvgpu_mem");
	}

	return data;
}

u64 nvgpu_mem_rd32_pair(struct gk20a *g, struct nvgpu_mem *mem, u32 lo, u32 hi)
{
	u64 lo_data = U64(nvgpu_mem_rd32(g, mem, lo));
	u64 hi_data = U64(nvgpu_mem_rd32(g, mem, hi));

	return lo_data | (hi_data << 32ULL);
}

u32 nvgpu_mem_rd(struct gk20a *g, struct nvgpu_mem *mem, u64 offset)
{
NVGPU_COV_WHITELIST_BLOCK_BEGIN(false_positive, 1, NVGPU_MISRA(Rule, 10_3), "Bug 2277532")
NVGPU_COV_WHITELIST_BLOCK_BEGIN(false_positive, 1, NVGPU_MISRA(Rule, 14_4), "Bug 2277532")
NVGPU_COV_WHITELIST_BLOCK_BEGIN(false_positive, 1, NVGPU_MISRA(Rule, 15_6), "Bug 2277532")
	WARN_ON((offset & 3ULL) != 0ULL);
NVGPU_COV_WHITELIST_BLOCK_END(NVGPU_MISRA(Rule, 10_3))
NVGPU_COV_WHITELIST_BLOCK_END(NVGPU_MISRA(Rule, 14_4))
NVGPU_COV_WHITELIST_BLOCK_END(NVGPU_MISRA(Rule, 15_6))
	return nvgpu_mem_rd32(g, mem, offset / (u64)sizeof(u32));
}

void nvgpu_mem_rd_n(struct gk20a *g, struct nvgpu_mem *mem,
		u64 offset, void *dest, u64 size)
{
NVGPU_COV_WHITELIST_BLOCK_BEGIN(false_positive, 2, NVGPU_MISRA(Rule, 10_3), "Bug 2277532")
NVGPU_COV_WHITELIST_BLOCK_BEGIN(false_positive, 2, NVGPU_MISRA(Rule, 14_4), "Bug 2277532")
NVGPU_COV_WHITELIST_BLOCK_BEGIN(false_positive, 2, NVGPU_MISRA(Rule, 15_6), "Bug 2277532")
	WARN_ON((offset & 3ULL) != 0ULL);
	WARN_ON((size & 3ULL) != 0ULL);
NVGPU_COV_WHITELIST_BLOCK_END(NVGPU_MISRA(Rule, 10_3))
NVGPU_COV_WHITELIST_BLOCK_END(NVGPU_MISRA(Rule, 14_4))
NVGPU_COV_WHITELIST_BLOCK_END(NVGPU_MISRA(Rule, 15_6))

	if (mem->aperture == APERTURE_SYSMEM) {
		u8 *src = (u8 *)mem->cpu_va + offset;

NVGPU_COV_WHITELIST_BLOCK_BEGIN(false_positive, 1, NVGPU_MISRA(Rule, 10_3), "Bug 2277532")
NVGPU_COV_WHITELIST_BLOCK_BEGIN(false_positive, 1, NVGPU_MISRA(Rule, 14_4), "Bug 2277532")
NVGPU_COV_WHITELIST_BLOCK_BEGIN(false_positive, 1, NVGPU_MISRA(Rule, 15_6), "Bug 2277532")
		WARN_ON(mem->cpu_va == NULL);
NVGPU_COV_WHITELIST_BLOCK_END(NVGPU_MISRA(Rule, 10_3))
NVGPU_COV_WHITELIST_BLOCK_END(NVGPU_MISRA(Rule, 14_4))
NVGPU_COV_WHITELIST_BLOCK_END(NVGPU_MISRA(Rule, 15_6))
		nvgpu_memcpy((u8 *)dest, src, size);
	}
#ifdef CONFIG_NVGPU_DGPU
	else if (mem->aperture == APERTURE_VIDMEM) {
		nvgpu_pramin_rd_n(g, mem, offset, size, dest);
	}
#endif
	else {
		nvgpu_do_assert_print(g, "Accessing unallocated nvgpu_mem");
	}
}

void nvgpu_mem_wr32(struct gk20a *g, struct nvgpu_mem *mem, u64 w, u32 data)
{
	if (mem->aperture == APERTURE_SYSMEM) {
		u32 *ptr = mem->cpu_va;

NVGPU_COV_WHITELIST_BLOCK_BEGIN(false_positive, 1, NVGPU_MISRA(Rule, 10_3), "Bug 2277532")
NVGPU_COV_WHITELIST_BLOCK_BEGIN(false_positive, 1, NVGPU_MISRA(Rule, 14_4), "Bug 2277532")
NVGPU_COV_WHITELIST_BLOCK_BEGIN(false_positive, 1, NVGPU_MISRA(Rule, 15_6), "Bug 2277532")
		WARN_ON(ptr == NULL);
NVGPU_COV_WHITELIST_BLOCK_END(NVGPU_MISRA(Rule, 10_3))
NVGPU_COV_WHITELIST_BLOCK_END(NVGPU_MISRA(Rule, 14_4))
NVGPU_COV_WHITELIST_BLOCK_END(NVGPU_MISRA(Rule, 15_6))
		ptr[w] = data;
	}
#ifdef CONFIG_NVGPU_DGPU
	else if (mem->aperture == APERTURE_VIDMEM) {
		nvgpu_pramin_wr_n(g, mem, w * (u64)sizeof(u32),
				  (u64)sizeof(u32), &data);

		if (!mem->skip_wmb) {
			nvgpu_wmb();
		}
	}
#endif
	else {
		nvgpu_do_assert_print(g, "Accessing unallocated nvgpu_mem");
	}
}

void nvgpu_mem_wr(struct gk20a *g, struct nvgpu_mem *mem, u64 offset, u32 data)
{
NVGPU_COV_WHITELIST_BLOCK_BEGIN(false_positive, 1, NVGPU_MISRA(Rule, 10_3), "Bug 2277532")
NVGPU_COV_WHITELIST_BLOCK_BEGIN(false_positive, 1, NVGPU_MISRA(Rule, 14_4), "Bug 2277532")
NVGPU_COV_WHITELIST_BLOCK_BEGIN(false_positive, 1, NVGPU_MISRA(Rule, 15_6), "Bug 2277532")
	WARN_ON((offset & 3ULL) != 0ULL);
NVGPU_COV_WHITELIST_BLOCK_END(NVGPU_MISRA(Rule, 10_3))
NVGPU_COV_WHITELIST_BLOCK_END(NVGPU_MISRA(Rule, 14_4))
NVGPU_COV_WHITELIST_BLOCK_END(NVGPU_MISRA(Rule, 15_6))
	nvgpu_mem_wr32(g, mem, offset / (u64)sizeof(u32), data);
}

void nvgpu_mem_wr_n(struct gk20a *g, struct nvgpu_mem *mem, u64 offset,
		void *src, u64 size)
{
NVGPU_COV_WHITELIST_BLOCK_BEGIN(false_positive, 2, NVGPU_MISRA(Rule, 10_3), "Bug 2277532")
NVGPU_COV_WHITELIST_BLOCK_BEGIN(false_positive, 2, NVGPU_MISRA(Rule, 14_4), "Bug 2277532")
NVGPU_COV_WHITELIST_BLOCK_BEGIN(false_positive, 2, NVGPU_MISRA(Rule, 15_6), "Bug 2277532")
	WARN_ON((offset & 3ULL) != 0ULL);
	WARN_ON((size & 3ULL) != 0ULL);
NVGPU_COV_WHITELIST_BLOCK_END(NVGPU_MISRA(Rule, 10_3))
NVGPU_COV_WHITELIST_BLOCK_END(NVGPU_MISRA(Rule, 14_4))
NVGPU_COV_WHITELIST_BLOCK_END(NVGPU_MISRA(Rule, 15_6))

	if (mem->aperture == APERTURE_SYSMEM) {
		u8 *dest = (u8 *)mem->cpu_va + offset;

NVGPU_COV_WHITELIST_BLOCK_BEGIN(false_positive, 1, NVGPU_MISRA(Rule, 10_3), "Bug 2277532")
NVGPU_COV_WHITELIST_BLOCK_BEGIN(false_positive, 1, NVGPU_MISRA(Rule, 14_4), "Bug 2277532")
NVGPU_COV_WHITELIST_BLOCK_BEGIN(false_positive, 1, NVGPU_MISRA(Rule, 15_6), "Bug 2277532")
		WARN_ON(mem->cpu_va == NULL);
NVGPU_COV_WHITELIST_BLOCK_END(NVGPU_MISRA(Rule, 10_3))
NVGPU_COV_WHITELIST_BLOCK_END(NVGPU_MISRA(Rule, 14_4))
NVGPU_COV_WHITELIST_BLOCK_END(NVGPU_MISRA(Rule, 15_6))
		nvgpu_memcpy(dest, (u8 *)src, size);
	}
#ifdef CONFIG_NVGPU_DGPU
	else if (mem->aperture == APERTURE_VIDMEM) {
		nvgpu_pramin_wr_n(g, mem, offset, size, src);
		if (!mem->skip_wmb) {
			nvgpu_wmb();
		}
	}
#endif
	else {
		nvgpu_do_assert_print(g, "Accessing unallocated nvgpu_mem");
	}
}

void nvgpu_memset(struct gk20a *g, struct nvgpu_mem *mem, u64 offset,
		u32 c, u64 size)
{
NVGPU_COV_WHITELIST_BLOCK_BEGIN(false_positive, 3, NVGPU_MISRA(Rule, 10_3), "Bug 2277532")
NVGPU_COV_WHITELIST_BLOCK_BEGIN(false_positive, 3, NVGPU_MISRA(Rule, 14_4), "Bug 2277532")
NVGPU_COV_WHITELIST_BLOCK_BEGIN(false_positive, 3, NVGPU_MISRA(Rule, 15_6), "Bug 2277532")
	WARN_ON((offset & 3ULL) != 0ULL);
	WARN_ON((size & 3ULL) != 0ULL);
	WARN_ON((c & ~0xffU) != 0U);
NVGPU_COV_WHITELIST_BLOCK_END(NVGPU_MISRA(Rule, 10_3))
NVGPU_COV_WHITELIST_BLOCK_END(NVGPU_MISRA(Rule, 14_4))
NVGPU_COV_WHITELIST_BLOCK_END(NVGPU_MISRA(Rule, 15_6))

	c &= 0xffU;

	if (mem->aperture == APERTURE_SYSMEM) {
		u8 *dest = (u8 *)mem->cpu_va + offset;

NVGPU_COV_WHITELIST_BLOCK_BEGIN(false_positive, 1, NVGPU_MISRA(Rule, 10_3), "Bug 2277532")
NVGPU_COV_WHITELIST_BLOCK_BEGIN(false_positive, 1, NVGPU_MISRA(Rule, 14_4), "Bug 2277532")
NVGPU_COV_WHITELIST_BLOCK_BEGIN(false_positive, 1, NVGPU_MISRA(Rule, 15_6), "Bug 2277532")
		WARN_ON(mem->cpu_va == NULL);
NVGPU_COV_WHITELIST_BLOCK_END(NVGPU_MISRA(Rule, 10_3))
NVGPU_COV_WHITELIST_BLOCK_END(NVGPU_MISRA(Rule, 14_4))
NVGPU_COV_WHITELIST_BLOCK_END(NVGPU_MISRA(Rule, 15_6))
		(void) memset(dest, (int)c, size);
	}
#ifdef CONFIG_NVGPU_DGPU
	else if (mem->aperture == APERTURE_VIDMEM) {
		u32 repeat_value = c | (c << 8) | (c << 16) | (c << 24);

		nvgpu_pramin_memset(g, mem, offset, size, repeat_value);
		if (!mem->skip_wmb) {
			nvgpu_wmb();
		}
	}
#endif
	else {
		nvgpu_do_assert_print(g, "Accessing unallocated nvgpu_mem");
	}
}

static void *nvgpu_mem_phys_sgl_next(void *sgl)
{
	struct nvgpu_mem_sgl *sgl_impl = (struct nvgpu_mem_sgl *)sgl;

	return (void *)(void *)sgl_impl->next;
}

/*
 * Provided for compatibility - the DMA address is the same as the phys address
 * for these nvgpu_mem's.
 */
static u64 nvgpu_mem_phys_sgl_dma(void *sgl)
{
	struct nvgpu_mem_sgl *sgl_impl = (struct nvgpu_mem_sgl *)sgl;

	return sgl_impl->phys;
}

static u64 nvgpu_mem_phys_sgl_phys(struct gk20a *g, void *sgl)
{
	struct nvgpu_mem_sgl *sgl_impl = (struct nvgpu_mem_sgl *)sgl;

	return sgl_impl->phys;
}

static u64 nvgpu_mem_phys_sgl_ipa_to_pa(struct gk20a *g,
		void *sgl, u64 ipa, u64 *pa_len)
{
	return ipa;
}

static u64 nvgpu_mem_phys_sgl_length(void *sgl)
{
	struct nvgpu_mem_sgl *sgl_impl = (struct nvgpu_mem_sgl *)sgl;

	return sgl_impl->length;
}

static u64 nvgpu_mem_phys_sgl_gpu_addr(struct gk20a *g, void *sgl,
					 struct nvgpu_gmmu_attrs *attrs)
{
	struct nvgpu_mem_sgl *sgl_impl = (struct nvgpu_mem_sgl *)sgl;

	return sgl_impl->phys;
}

static void nvgpu_mem_phys_sgt_free(struct gk20a *g, struct nvgpu_sgt *sgt)
{
	/*
	 * No-op here. The free is handled by freeing the nvgpu_mem itself.
	 */
}

NVGPU_COV_WHITELIST_BLOCK_BEGIN(false_positive, 1, NVGPU_MISRA(Rule, 8_7), "Bug 2823817")
static const struct nvgpu_sgt_ops nvgpu_mem_phys_ops = {
NVGPU_COV_WHITELIST_BLOCK_END(NVGPU_MISRA(Rule, 8_7))
	.sgl_next      = nvgpu_mem_phys_sgl_next,
	.sgl_dma       = nvgpu_mem_phys_sgl_dma,
	.sgl_phys      = nvgpu_mem_phys_sgl_phys,
	.sgl_ipa       = nvgpu_mem_phys_sgl_phys,
	.sgl_ipa_to_pa = nvgpu_mem_phys_sgl_ipa_to_pa,
	.sgl_length    = nvgpu_mem_phys_sgl_length,
	.sgl_gpu_addr  = nvgpu_mem_phys_sgl_gpu_addr,
	.sgt_free      = nvgpu_mem_phys_sgt_free,

	/*
	 * The physical nvgpu_mems are never IOMMU'able by definition.
	 */
	.sgt_iommuable = NULL
};

int nvgpu_mem_create_from_phys(struct gk20a *g, struct nvgpu_mem *dest,
			       u64 src_phys, u64 nr_pages)
{
	int ret = 0;
	struct nvgpu_sgt *sgt;
	struct nvgpu_mem_sgl *sgl;

	/*
	 * Do the two operations that can fail before touching *dest.
	 */
	sgt = nvgpu_kzalloc(g, sizeof(*sgt));
	sgl = nvgpu_kzalloc(g, sizeof(*sgl));
	if ((sgt == NULL) || (sgl == NULL)) {
		nvgpu_kfree(g, sgt);
		nvgpu_kfree(g, sgl);
		return -ENOMEM;
	}

	(void) memset(dest, 0, sizeof(*dest));

	dest->aperture     = APERTURE_SYSMEM;
	dest->size         = nvgpu_safe_mult_u64(nr_pages,
			(u64)NVGPU_CPU_PAGE_SIZE);
	dest->aligned_size = dest->size;
	dest->mem_flags    = NVGPU_MEM_FLAG_NO_DMA;
	dest->phys_sgt     = sgt;

	sgl->next   = NULL;
	sgl->phys   = src_phys;
	sgl->length = dest->size;
	sgt->sgl    = (void *)sgl;
	sgt->ops    = &nvgpu_mem_phys_ops;

	return ret;
}
