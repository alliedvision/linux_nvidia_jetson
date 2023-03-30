/*
 * Copyright (c) 2018-2020, NVIDIA CORPORATION.  All rights reserved.
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

#include <nvgpu/dma.h>
#include <nvgpu/bitops.h>
#include <nvgpu/nvgpu_mem.h>
#include <nvgpu/nvgpu_sgt.h>
#include <nvgpu/nvgpu_sgt_os.h>
#include <nvgpu/log.h>

void *nvgpu_sgt_get_next(struct nvgpu_sgt *sgt, void *sgl)
{
	return sgt->ops->sgl_next(sgl);
}

u64 nvgpu_sgt_get_phys(struct gk20a *g, struct nvgpu_sgt *sgt, void *sgl)
{
	return sgt->ops->sgl_phys(g, sgl);
}

u64 nvgpu_sgt_get_ipa(struct gk20a *g, struct nvgpu_sgt *sgt, void *sgl)
{
	return sgt->ops->sgl_ipa(g, sgl);
}

u64 nvgpu_sgt_ipa_to_pa(struct gk20a *g, struct nvgpu_sgt *sgt,
				void *sgl, u64 ipa, u64 *pa_len)
{
	return sgt->ops->sgl_ipa_to_pa(g, sgl, ipa, pa_len);
}

u64 nvgpu_sgt_get_dma(struct nvgpu_sgt *sgt, void *sgl)
{
	return sgt->ops->sgl_dma(sgl);
}

u64 nvgpu_sgt_get_length(struct nvgpu_sgt *sgt, void *sgl)
{
	return sgt->ops->sgl_length(sgl);
}

u64 nvgpu_sgt_get_gpu_addr(struct gk20a *g, struct nvgpu_sgt *sgt, void *sgl,
				struct nvgpu_gmmu_attrs *attrs)
{
	return sgt->ops->sgl_gpu_addr(g, sgl, attrs);
}

bool nvgpu_sgt_iommuable(struct gk20a *g, struct nvgpu_sgt *sgt)
{
	if (sgt->ops->sgt_iommuable != NULL) {
		return sgt->ops->sgt_iommuable(g, sgt);
	}
	return false;
}

void nvgpu_sgt_free(struct gk20a *g, struct nvgpu_sgt *sgt)
{
	if ((sgt != NULL) && (sgt->ops->sgt_free != NULL)) {
		sgt->ops->sgt_free(g, sgt);
	}
}

/*
 * Determine alignment for a passed buffer. Necessary since the buffer may
 * appear big enough to map with large pages but the SGL may have chunks that
 * are not aligned on a 64/128kB large page boundary. There's also the
 * possibility chunks are odd sizes which will necessitate small page mappings
 * to correctly glue them together into a contiguous virtual mapping.
 */
u64 nvgpu_sgt_alignment(struct gk20a *g, struct nvgpu_sgt *sgt)
{
	u64 align = 0, chunk_align = 0;
	void *sgl;

	/*
	 * If this SGT is iommuable and we want to use the IOMMU address then
	 * the SGT's first entry has the IOMMU address. We will align on this
	 * and double check length of buffer later. Also, since there's an
	 * IOMMU we know that this DMA address is contiguous.
	 */
	if (nvgpu_iommuable(g) &&
		nvgpu_sgt_iommuable(g, sgt) &&
		(nvgpu_sgt_get_dma(sgt, sgt->sgl) != 0ULL)) {
		return 1ULL << (nvgpu_ffs(nvgpu_sgt_get_dma(sgt, sgt->sgl))
						- 1UL);
	}

	/*
	 * Otherwise the buffer is not iommuable (VIDMEM, for example) or we are
	 * bypassing the IOMMU and need to use the underlying physical entries
	 * of the SGT.
	 */
	nvgpu_sgt_for_each_sgl(sgl, sgt) {
		chunk_align = 1ULL << nvgpu_safe_sub_u64(nvgpu_ffs(
					nvgpu_sgt_get_phys(g, sgt, sgl) |
					nvgpu_sgt_get_length(sgt, sgl)), 1UL);

		if (align != 0ULL) {
			align = min(align, chunk_align);
		} else {
			align = chunk_align;
		}
	}

	return align;
}

struct nvgpu_sgt *nvgpu_sgt_create_from_mem(struct gk20a *g,
					    struct nvgpu_mem *mem)
{
	if ((mem->mem_flags & NVGPU_MEM_FLAG_NO_DMA) != 0U) {
		return mem->phys_sgt;
	}

	return nvgpu_sgt_os_create_from_mem(g, mem);
}
