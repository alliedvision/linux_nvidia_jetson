/*
 * Copyright (c) 2018, NVIDIA CORPORATION.  All rights reserved.
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

#include <stdlib.h>

#include <nvgpu/mm.h>
#include <nvgpu/vm.h>
#include <nvgpu/bug.h>
#include <nvgpu/dma.h>
#include <nvgpu/gmmu.h>
#include <nvgpu/nvgpu_mem.h>
#include <nvgpu/enabled.h>

/*
 * In userspace vidmem vs sysmem is just a difference in what is placed in the
 * aperture field.
 */
static int __nvgpu_do_dma_alloc(struct gk20a *g, unsigned long flags,
				size_t size, struct nvgpu_mem *mem,
				enum nvgpu_aperture ap)
{
	void *memory = malloc(mem->aligned_size);

	if (memory == NULL)
		return -ENOMEM;

	mem->cpu_va       = memory;
	mem->aperture     = ap;
	mem->size         = size;
	mem->aligned_size = PAGE_ALIGN(size);
	mem->gpu_va       = 0ULL;
	mem->skip_wmb     = true;
	mem->vidmem_alloc = NULL;
	mem->allocator    = NULL;

	return 0;
}

bool nvgpu_iommuable(struct gk20a *g)
{
	return false;
}

int nvgpu_dma_alloc(struct gk20a *g, size_t size, struct nvgpu_mem *mem)
{
	return nvgpu_dma_alloc_flags(g, 0, size, mem);
}

int nvgpu_dma_alloc_flags(struct gk20a *g, unsigned long flags, size_t size,
			  struct nvgpu_mem *mem)
{
	if (!nvgpu_is_enabled(g, NVGPU_MM_UNIFIED_MEMORY)) {
		/*
		 * First try vidmem. Obviously in userspace there's no such
		 * thing as vidmem per se but we will mark the aperture as
		 * vidmem.
		 */
		int err = nvgpu_dma_alloc_flags_vid(g, 0, size, mem);

		if (!err)
			return 0;
		/*
		 * Fall back to sysmem (which may then also fail) in case
		 * vidmem is exhausted.
		 */
	}

	return nvgpu_dma_alloc_flags_sys(g, flags, size, mem);

}

int nvgpu_dma_alloc_sys(struct gk20a *g, size_t size, struct nvgpu_mem *mem)
{
	return nvgpu_dma_alloc_flags_sys(g, 0, size, mem);
}

int nvgpu_dma_alloc_flags_sys(struct gk20a *g, unsigned long flags,
			      size_t size, struct nvgpu_mem *mem)
{
	return __nvgpu_do_dma_alloc(g, flags, size, mem, APERTURE_SYSMEM);
}

int nvgpu_dma_alloc_vid(struct gk20a *g, size_t size, struct nvgpu_mem *mem)
{
	return nvgpu_dma_alloc_flags_vid(g, 0, size, mem);
}

int nvgpu_dma_alloc_flags_vid(struct gk20a *g, unsigned long flags,
			      size_t size, struct nvgpu_mem *mem)
{
	return __nvgpu_do_dma_alloc(g, flags, size, mem, APERTURE_VIDMEM);
}

int nvgpu_dma_alloc_vid_at(struct gk20a *g,
				 size_t size, struct nvgpu_mem *mem, u64 at)
{
	BUG();

	return 0;
}

int nvgpu_dma_alloc_flags_vid_at(struct gk20a *g, unsigned long flags,
				 size_t size, struct nvgpu_mem *mem, u64 at)
{
	BUG();

	return 0;
}

void nvgpu_dma_free(struct gk20a *g, struct nvgpu_mem *mem)
{
	if (!(mem->mem_flags & NVGPU_MEM_FLAG_SHADOW_COPY))
		free(mem->cpu_va);

	memset(mem, 0, sizeof(*mem));
}

int nvgpu_dma_alloc_map(struct vm_gk20a *vm, size_t size,
		struct nvgpu_mem *mem)
{
	return nvgpu_dma_alloc_map_flags(vm, 0, size, mem);
}

int nvgpu_dma_alloc_map_flags(struct vm_gk20a *vm, unsigned long flags,
		size_t size, struct nvgpu_mem *mem)
{
	if (!nvgpu_is_enabled(gk20a_from_vm(vm), NVGPU_MM_UNIFIED_MEMORY)) {
		int err = nvgpu_dma_alloc_map_flags_vid(vm,
				flags | NVGPU_DMA_NO_KERNEL_MAPPING,
				size, mem);

		if (!err)
			return 0;
		/*
		 * Fall back to sysmem (which may then also fail) in case
		 * vidmem is exhausted.
		 */
	}

	return nvgpu_dma_alloc_map_flags_sys(vm, flags, size, mem);
}

int nvgpu_dma_alloc_map_sys(struct vm_gk20a *vm, size_t size,
		struct nvgpu_mem *mem)
{
	return nvgpu_dma_alloc_map_flags_sys(vm, 0, size, mem);
}

int nvgpu_dma_alloc_map_flags_sys(struct vm_gk20a *vm, unsigned long flags,
		size_t size, struct nvgpu_mem *mem)
{
	int err = nvgpu_dma_alloc_flags_sys(vm->mm->g, flags, size, mem);

	if (err)
		return err;

	mem->gpu_va = nvgpu_gmmu_map(vm, mem, size, 0,
				     gk20a_mem_flag_none, false,
				     mem->aperture);
	if (!mem->gpu_va) {
		err = -ENOMEM;
		goto fail_free;
	}

	return 0;

fail_free:
	nvgpu_dma_free(vm->mm->g, mem);
	return err;
}

int nvgpu_dma_alloc_map_vid(struct vm_gk20a *vm, size_t size,
		struct nvgpu_mem *mem)
{
	return nvgpu_dma_alloc_map_flags_vid(vm,
			NVGPU_DMA_NO_KERNEL_MAPPING, size, mem);
}

int nvgpu_dma_alloc_map_flags_vid(struct vm_gk20a *vm, unsigned long flags,
		size_t size, struct nvgpu_mem *mem)
{
	int err = nvgpu_dma_alloc_flags_vid(vm->mm->g, flags, size, mem);

	if (err)
		return err;

	mem->gpu_va = nvgpu_gmmu_map(vm, mem, size, 0,
				     gk20a_mem_flag_none, false,
				     mem->aperture);
	if (!mem->gpu_va) {
		err = -ENOMEM;
		goto fail_free;
	}

	return 0;

fail_free:
	nvgpu_dma_free(vm->mm->g, mem);
	return err;
}

void nvgpu_dma_unmap_free(struct vm_gk20a *vm, struct nvgpu_mem *mem)
{
	if (mem->gpu_va)
		nvgpu_gmmu_unmap(vm, mem, mem->gpu_va);
	mem->gpu_va = 0;

	nvgpu_dma_free(vm->mm->g, mem);
}
