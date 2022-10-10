/*
 * Copyright (c) 2018-2022, NVIDIA CORPORATION.  All rights reserved.
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
#include <nvgpu/kmem.h>
#include <nvgpu/nvgpu_mem.h>
#include <nvgpu/nvgpu_sgt.h>
#include <nvgpu/page_allocator.h>
#include <nvgpu/enabled.h>
#include <nvgpu/vidmem.h>
#include <nvgpu/posix/posix-fault-injection.h>
#include <nvgpu/posix/posix_vidmem.h>
#include "os_posix.h"

struct nvgpu_posix_fault_inj *nvgpu_dma_alloc_get_fault_injection(void)
{
	struct nvgpu_posix_fault_inj_container *c =
			nvgpu_posix_fault_injection_get_container();

	return &c->dma_fi;
}

/*
 * In userspace vidmem vs sysmem is just a difference in what is placed in the
 * aperture field.
 */
static int __nvgpu_do_dma_alloc(struct gk20a *g, unsigned long flags,
				size_t size, struct nvgpu_mem *mem,
				enum nvgpu_aperture ap)
{
	void *memory;

	(void)g;
	(void)flags;

#ifdef NVGPU_UNITTEST_FAULT_INJECTION_ENABLEMENT
	if (nvgpu_posix_fault_injection_handle_call(
				nvgpu_dma_alloc_get_fault_injection())) {
		return -ENOMEM;
	}
#endif
	memory = malloc(PAGE_ALIGN(size));

	if (memory == NULL) {
		return -ENOMEM;
	}

	memset(memory, 0, PAGE_ALIGN(size));

	mem->cpu_va       = memory;
	mem->aperture     = ap;
	mem->size         = size;
	mem->aligned_size = PAGE_ALIGN(size);
	mem->gpu_va       = 0ULL;
	mem->skip_wmb     = true;
#ifdef CONFIG_NVGPU_DGPU
	mem->vidmem_alloc = NULL;
	mem->allocator    = NULL;
#endif
	return 0;
}

bool nvgpu_iommuable(struct gk20a *g)
{
	struct nvgpu_os_posix *p = nvgpu_os_posix_from_gk20a(g);

	return p->mm_is_iommuable;
}

int nvgpu_dma_alloc_flags_sys(struct gk20a *g, unsigned long flags,
			      size_t size, struct nvgpu_mem *mem)
{
	/* note: fault injection handled in common function */
	return __nvgpu_do_dma_alloc(g, flags, size, mem, APERTURE_SYSMEM);
}

#ifdef CONFIG_NVGPU_DGPU
static u64 __nvgpu_dma_alloc(struct nvgpu_allocator *allocator, u64 at,
				size_t size)
{
	u64 addr = 0;

	if (at)
		addr = nvgpu_alloc_fixed(allocator, at, size, 0);
	else
		addr = nvgpu_alloc(allocator, size);

	return addr;
}

static size_t mock_fb_get_vidmem_size(struct gk20a *g)
{
	(void)g;
	return SZ_4G;
}

static const struct nvgpu_sgt_ops nvgpu_sgt_posix_ops = {
	.sgl_next	= nvgpu_mem_sgl_next,
	.sgl_phys	= nvgpu_mem_sgl_phys,
	.sgl_ipa	= nvgpu_mem_sgl_phys,
	.sgl_ipa_to_pa	= nvgpu_mem_sgl_ipa_to_pa,
	.sgl_dma	= nvgpu_mem_sgl_dma,
	.sgl_length	= nvgpu_mem_sgl_length,
	.sgl_gpu_addr	= nvgpu_mem_sgl_gpu_addr,
	.sgt_iommuable	= nvgpu_mem_sgt_iommuable,
	.sgt_free	= nvgpu_mem_sgt_free,
};

/* In userspace, vidmem requires only a few fields populated */
int nvgpu_dma_alloc_flags_vid_at(struct gk20a *g, unsigned long flags,
				 size_t size, struct nvgpu_mem *mem, u64 at)
{
	u64 addr;
	int err;
#ifdef NVGPU_UNITTEST_FAULT_INJECTION_ENABLEMENT
	if (nvgpu_posix_fault_injection_handle_call(
				nvgpu_dma_alloc_get_fault_injection())) {
		return -ENOMEM;
	}
#endif
	g->ops.fb.get_vidmem_size = mock_fb_get_vidmem_size;

	nvgpu_set_enabled(g, NVGPU_MM_UNIFIED_MEMORY, false);

	/* Initialize nvgpu_allocators */
	err = nvgpu_vidmem_init(&g->mm);
	if (err != 0) {
		nvgpu_err(g, "vidmem init failed with err=%d", err);
		return err;
	}


	struct nvgpu_allocator *vidmem_alloc = g->mm.vidmem.cleared ?
		&g->mm.vidmem.allocator :
		&g->mm.vidmem.bootstrap_allocator;

	if (nvgpu_mem_is_valid(mem)) {
		nvgpu_warn(g, "memory leak !!");
		WARN_ON(1);
	}

	mem->size = size;
	size = PAGE_ALIGN(size);

	if (!nvgpu_alloc_initialized(&g->mm.vidmem.allocator)) {
		err = -ENOSYS;
		nvgpu_err(g, "nvgpu alloc not initialized\n");
		goto dma_err;
	}

	/*
	 * Our own allocator doesn't have any flags yet, and we can't
	 * kernel-map these, so require explicit flags.
	 */
	WARN_ON(flags != NVGPU_DMA_NO_KERNEL_MAPPING);

	addr = __nvgpu_dma_alloc(vidmem_alloc, at, size);
	if (!addr) {
		/*
		 * If memory is known to be freed soon, let the user know that
		 * it may be available after a while.
		 */
		err = -ENOMEM;
		nvgpu_err(g, " not initialized\n");
		goto dma_err;
	}

	if (at)
		mem->mem_flags |= NVGPU_MEM_FLAG_FIXED;

	/* POSIX doesn't have sg_table struct. Allocate memory for nvgpu_sgt */
	mem->priv.sgt = nvgpu_kzalloc(g, sizeof(struct nvgpu_sgt));
	if (!mem->priv.sgt) {
		err = -ENOMEM;
		goto fail_sgtfree;
	}

	mem->priv.sgt->ops = &nvgpu_sgt_posix_ops;

	/* Allocate memory for sgl */
	mem->priv.sgt->sgl = (struct nvgpu_mem_sgl *)
		nvgpu_kzalloc(g, sizeof(struct nvgpu_mem_sgl));
	if (mem->priv.sgt->sgl == NULL) {
		nvgpu_err(g, "sgl allocation failed\n");
		err = -ENOMEM;
		goto fail_sglfree;
	}

	nvgpu_vidmem_set_page_alloc(
		(struct nvgpu_mem_sgl *)mem->priv.sgt->sgl, addr);

	mem->aligned_size = size;
	mem->aperture = APERTURE_VIDMEM;
	mem->vidmem_alloc = (struct nvgpu_page_alloc *)(uintptr_t)addr;
	mem->allocator = vidmem_alloc;

	return 0;

fail_sglfree:
	nvgpu_kfree(g, mem->priv.sgt);
fail_sgtfree:
	nvgpu_free(&g->mm.vidmem.allocator, addr);
	mem->size = 0;
dma_err:
	nvgpu_vidmem_destroy(g);
	nvgpu_cond_destroy(&g->mm.vidmem.clearing_thread_cond);
	nvgpu_thread_stop_graceful(&g->mm.vidmem.clearing_thread, NULL, NULL);
	return err;
}

void nvgpu_dma_free_vid(struct gk20a *g, struct nvgpu_mem *mem)
{

	nvgpu_memset(g, mem, 0, 0, mem->aligned_size);

	if (mem->priv.sgt != NULL) {
		nvgpu_free(mem->allocator, (u64)nvgpu_vidmem_get_page_alloc(
			(struct nvgpu_mem_sgl *)mem->priv.sgt->sgl));
		nvgpu_kfree(g, mem->priv.sgt);
		mem->priv.sgt = NULL;
	}

	mem->size = 0;
	mem->aligned_size = 0;
	mem->aperture = APERTURE_INVALID;

	nvgpu_vidmem_destroy(g);
	nvgpu_cond_destroy(&g->mm.vidmem.clearing_thread_cond);
	nvgpu_thread_stop_graceful(&g->mm.vidmem.clearing_thread, NULL, NULL);
}
#endif

void nvgpu_dma_free_sys(struct gk20a *g, struct nvgpu_mem *mem)
{
	(void)g;

	if (!(mem->mem_flags & NVGPU_MEM_FLAG_SHADOW_COPY)) {
		free(mem->cpu_va);
	}

	(void) memset(mem, 0, sizeof(*mem));
}
