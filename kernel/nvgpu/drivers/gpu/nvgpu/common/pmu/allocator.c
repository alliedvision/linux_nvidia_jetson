/*
 * Copyright (c) 2017-2021, NVIDIA CORPORATION.  All rights reserved.
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

#include <nvgpu/pmu/allocator.h>
#include <nvgpu/allocator.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/pmu.h>
#include <nvgpu/pmu/fw.h>
#include <nvgpu/dma.h>

void nvgpu_pmu_allocator_dmem_init(struct gk20a *g,
	struct nvgpu_pmu *pmu, struct nvgpu_allocator *dmem,
	union pmu_init_msg_pmu *init)
{
	struct pmu_fw_ver_ops *fw_ops = &pmu->fw->ops;

	if (!nvgpu_alloc_initialized(dmem)) {
		/* Align start and end addresses */
		u32 start =
			NVGPU_ALIGN(U32(fw_ops->get_init_msg_sw_mngd_area_off(init)),
			PMU_DMEM_ALLOC_ALIGNMENT);
		u32 end = (U32(fw_ops->get_init_msg_sw_mngd_area_off(init)) +
			U32(fw_ops->get_init_msg_sw_mngd_area_size(init))) &
			~(PMU_DMEM_ALLOC_ALIGNMENT - 1U);
		u32 size = end - start;

		if (size != 0U) {
			nvgpu_allocator_init(g, dmem, NULL, "gk20a_pmu_dmem",
				start, size, PMU_DMEM_ALLOC_ALIGNMENT, 0ULL, 0ULL,
				BITMAP_ALLOCATOR);
		} else {
			dmem->priv = NULL;
		}
	}
}

void nvgpu_pmu_allocator_dmem_destroy(struct nvgpu_allocator *dmem)
{
	if (nvgpu_alloc_initialized(dmem)) {
		nvgpu_alloc_destroy(dmem);
	}
}

void nvgpu_pmu_allocator_surface_free(struct gk20a *g, struct nvgpu_mem *mem)
{
	if (nvgpu_mem_is_valid(mem)) {
		nvgpu_dma_free(g, mem);
	}
}

void nvgpu_pmu_allocator_surface_describe(struct gk20a *g, struct nvgpu_mem *mem,
		struct flcn_mem_desc_v0 *fb)
{
	fb->address.lo = u64_lo32(mem->gpu_va);
	fb->address.hi = u64_hi32(mem->gpu_va);
	fb->params = ((u32)mem->size & 0xFFFFFFU);
	fb->params |= (GK20A_PMU_DMAIDX_VIRT << 24U);
}

int nvgpu_pmu_allocator_sysmem_surface_alloc(struct gk20a *g,
		struct nvgpu_mem *mem, u32 size)
{
	struct mm_gk20a *mm = &g->mm;
	struct vm_gk20a *vm = mm->pmu.vm;
	int err;

	err = nvgpu_dma_alloc_map_sys(vm, size, mem);
	if (err != 0) {
		nvgpu_err(g, "failed to allocate memory\n");
		return -ENOMEM;
	}

	return 0;
}
