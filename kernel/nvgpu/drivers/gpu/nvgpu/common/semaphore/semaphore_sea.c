/*
 * Copyright (c) 2014-2020, NVIDIA CORPORATION.  All rights reserved.
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

#include <nvgpu/log.h>
#include <nvgpu/kmem.h>
#include <nvgpu/dma.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/semaphore.h>

#include "semaphore_priv.h"

void nvgpu_semaphore_sea_lock(struct nvgpu_semaphore_sea *s)
{
	gpu_sema_verbose_dbg(s->gk20a, "Acquiring sema lock...");
	nvgpu_mutex_acquire(&s->sea_lock);
	gpu_sema_verbose_dbg(s->gk20a, "Sema lock aquried!");
}

void nvgpu_semaphore_sea_unlock(struct nvgpu_semaphore_sea *s)
{
	nvgpu_mutex_release(&s->sea_lock);
	gpu_sema_verbose_dbg(s->gk20a, "Released sema lock");
}

static int semaphore_sea_grow(struct nvgpu_semaphore_sea *sea)
{
	int ret = 0;
	struct gk20a *g = sea->gk20a;
	u32 i;

	nvgpu_semaphore_sea_lock(sea);

	ret = nvgpu_dma_alloc_sys(g,
				  NVGPU_CPU_PAGE_SIZE * SEMAPHORE_POOL_COUNT,
				  &sea->sea_mem);
	if (ret != 0) {
		goto out;
	}

	sea->size = SEMAPHORE_POOL_COUNT;
	sea->map_size = SEMAPHORE_POOL_COUNT * NVGPU_CPU_PAGE_SIZE;

	/*
	 * Start the semaphores at values that will soon overflow the 32-bit
	 * integer range. This way any buggy comparisons would start to fail
	 * sooner rather than later.
	 */
	for (i = 0U; i < NVGPU_CPU_PAGE_SIZE * SEMAPHORE_POOL_COUNT; i += 4U) {
		nvgpu_mem_wr(g, &sea->sea_mem, i, 0xfffffff0U);
	}

out:
	nvgpu_semaphore_sea_unlock(sea);
	return ret;
}


/*
 * Return the sema_sea pointer.
 */
struct nvgpu_semaphore_sea *nvgpu_semaphore_get_sea(struct gk20a *g)
{
	return g->sema_sea;
}

void nvgpu_semaphore_sea_allocate_gpu_va(struct nvgpu_semaphore_sea *s,
	struct nvgpu_allocator *a, u64 base, u64 len, u32 page_size)
{
	s->gpu_va = nvgpu_alloc_fixed(a, base, len, page_size);
}

u64 nvgpu_semaphore_sea_get_gpu_va(struct nvgpu_semaphore_sea *s)
{
	return s->gpu_va;
}

/*
 * Create the semaphore sea. Only create it once - subsequent calls to this will
 * return the originally created sea pointer.
 */
struct nvgpu_semaphore_sea *nvgpu_semaphore_sea_create(struct gk20a *g)
{
	if (g->sema_sea != NULL) {
		return g->sema_sea;
	}

	g->sema_sea = nvgpu_kzalloc(g, sizeof(*g->sema_sea));
	if (g->sema_sea == NULL) {
		return NULL;
	}

	g->sema_sea->size = 0;
	g->sema_sea->page_count = 0;
	g->sema_sea->gk20a = g;
	nvgpu_init_list_node(&g->sema_sea->pool_list);
	nvgpu_mutex_init(&g->sema_sea->sea_lock);

	if (semaphore_sea_grow(g->sema_sea) != 0) {
		goto cleanup;
	}

	gpu_sema_dbg(g, "Created semaphore sea!");
	return g->sema_sea;

cleanup:
	nvgpu_mutex_destroy(&g->sema_sea->sea_lock);
	nvgpu_kfree(g, g->sema_sea);
	g->sema_sea = NULL;
	gpu_sema_dbg(g, "Failed to creat semaphore sea!");
	return NULL;
}

void nvgpu_semaphore_sea_destroy(struct gk20a *g)
{
	if (g->sema_sea == NULL) {
		return;
	}

	nvgpu_dma_free(g, &g->sema_sea->sea_mem);
	nvgpu_mutex_destroy(&g->sema_sea->sea_lock);
	nvgpu_kfree(g, g->sema_sea);
	g->sema_sea = NULL;
}
