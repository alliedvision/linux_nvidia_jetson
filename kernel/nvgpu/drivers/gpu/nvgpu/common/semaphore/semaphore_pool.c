/*
 * Copyright (c) 2014-2021, NVIDIA CORPORATION.  All rights reserved.
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
#include <nvgpu/gmmu.h>
#include <nvgpu/vm.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/bug.h>
#include <nvgpu/semaphore.h>

#include "semaphore_priv.h"

#define pool_to_gk20a(p) ((p)->sema_sea->gk20a)

/*
 * Allocate a pool from the sea.
 */
int nvgpu_semaphore_pool_alloc(struct nvgpu_semaphore_sea *sea,
			       struct nvgpu_semaphore_pool **pool)
{
	struct nvgpu_semaphore_pool *p;
	unsigned long page_idx;
	int ret;

	p = nvgpu_kzalloc(sea->gk20a, sizeof(*p));
	if (p == NULL) {
		return -ENOMEM;
	}

	nvgpu_semaphore_sea_lock(sea);

	nvgpu_mutex_init(&p->pool_lock);

	ret = semaphore_bitmap_alloc(sea->pools_alloced,
				       SEMAPHORE_POOL_COUNT);
	if (ret < 0) {
		goto fail;
	}

	page_idx = (unsigned long)ret;

	p->page_idx = page_idx;
	p->sema_sea = sea;
	nvgpu_init_list_node(&p->pool_list_entry);
	nvgpu_ref_init(&p->ref);

	sea->page_count++;
	nvgpu_list_add(&p->pool_list_entry, &sea->pool_list);
	nvgpu_semaphore_sea_unlock(sea);

	gpu_sema_dbg(sea->gk20a,
		     "Allocated semaphore pool: page-idx=%llu", p->page_idx);

	*pool = p;
	return 0;

fail:
	nvgpu_mutex_destroy(&p->pool_lock);
	nvgpu_semaphore_sea_unlock(sea);
	nvgpu_kfree(sea->gk20a, p);
	gpu_sema_dbg(sea->gk20a, "Failed to allocate semaphore pool!");
	return ret;
}

/*
 * Map a pool into the passed vm's address space. This handles both the fixed
 * global RO mapping and the non-fixed private RW mapping.
 */
int nvgpu_semaphore_pool_map(struct nvgpu_semaphore_pool *p,
			     struct vm_gk20a *vm)
{
	int err = 0;
	u64 addr;

	if (p->mapped) {
		return -EBUSY;
	}

	gpu_sema_dbg(pool_to_gk20a(p),
		     "Mapping semaphore pool! (idx=%llu)", p->page_idx);

	/*
	 * Take the sea lock so that we don't race with a possible change to the
	 * nvgpu_mem in the sema sea.
	 */
	nvgpu_semaphore_sea_lock(p->sema_sea);

	addr = nvgpu_gmmu_map_fixed(vm, &p->sema_sea->sea_mem,
				    p->sema_sea->gpu_va,
				    p->sema_sea->map_size,
				    0, gk20a_mem_flag_read_only, 0,
				    p->sema_sea->sea_mem.aperture);
	if (addr == 0ULL) {
		err = -ENOMEM;
		goto fail_unlock;
	}

	p->gpu_va_ro = addr;
	p->mapped = true;

	gpu_sema_dbg(pool_to_gk20a(p),
		     "  %llu: GPU read-only  VA = 0x%llx",
		     p->page_idx, p->gpu_va_ro);

	/*
	 * Now the RW mapping. This is a bit more complicated. We make a
	 * nvgpu_mem describing a page of the bigger RO space and then map
	 * that. Unlike above this does not need to be a fixed address.
	 */
	err = nvgpu_mem_create_from_mem(vm->mm->g,
					&p->rw_mem, &p->sema_sea->sea_mem,
					p->page_idx, 1UL);
	if (err != 0) {
		goto fail_unmap;
	}

	addr = nvgpu_gmmu_map_partial(vm, &p->rw_mem, SZ_4K, 0,
			      gk20a_mem_flag_none, 0,
			      p->rw_mem.aperture);

	if (addr == 0ULL) {
		err = -ENOMEM;
		goto fail_free_submem;
	}

	p->gpu_va = addr;

	nvgpu_semaphore_sea_unlock(p->sema_sea);

	gpu_sema_dbg(pool_to_gk20a(p),
		     "  %llu: GPU read-write VA = 0x%llx",
		     p->page_idx, p->gpu_va);
	gpu_sema_dbg(pool_to_gk20a(p),
		     "  %llu: CPU VA            = 0x%p",
		     p->page_idx, p->rw_mem.cpu_va);

	return 0;

fail_free_submem:
	nvgpu_dma_free(pool_to_gk20a(p), &p->rw_mem);
fail_unmap:
	nvgpu_gmmu_unmap_addr(vm, &p->sema_sea->sea_mem, p->gpu_va_ro);
	gpu_sema_dbg(pool_to_gk20a(p),
		     "  %llu: Failed to map semaphore pool!", p->page_idx);
fail_unlock:
	nvgpu_semaphore_sea_unlock(p->sema_sea);
	return err;
}

/*
 * Unmap a semaphore_pool.
 */
void nvgpu_semaphore_pool_unmap(struct nvgpu_semaphore_pool *p,
				struct vm_gk20a *vm)
{
	nvgpu_semaphore_sea_lock(p->sema_sea);

	nvgpu_gmmu_unmap_addr(vm, &p->sema_sea->sea_mem, p->gpu_va_ro);
	nvgpu_gmmu_unmap_addr(vm, &p->rw_mem, p->gpu_va);
	nvgpu_dma_free(pool_to_gk20a(p), &p->rw_mem);

	p->gpu_va = 0;
	p->gpu_va_ro = 0;
	p->mapped = false;

	nvgpu_semaphore_sea_unlock(p->sema_sea);

	gpu_sema_dbg(pool_to_gk20a(p),
		     "Unmapped semaphore pool! (idx=%llu)", p->page_idx);
}

static struct nvgpu_semaphore_pool *
nvgpu_semaphore_pool_from_ref(struct nvgpu_ref *ref)
{
	return (struct nvgpu_semaphore_pool *)
		((uintptr_t)ref - offsetof(struct nvgpu_semaphore_pool, ref));
}

/*
 * Completely free a semaphore_pool. You should make sure this pool is not
 * mapped otherwise there's going to be a memory leak.
 */
static void nvgpu_semaphore_pool_free(struct nvgpu_ref *ref)
{
	struct nvgpu_semaphore_pool *p = nvgpu_semaphore_pool_from_ref(ref);
	struct nvgpu_semaphore_sea *s = p->sema_sea;

	/* Freeing a mapped pool is a bad idea. */
	WARN_ON((p->mapped) ||
		(p->gpu_va != 0ULL) ||
		(p->gpu_va_ro != 0ULL));

	nvgpu_semaphore_sea_lock(s);
	nvgpu_list_del(&p->pool_list_entry);
	nvgpu_clear_bit((u32)p->page_idx, s->pools_alloced);
	s->page_count--;
	nvgpu_semaphore_sea_unlock(s);

	nvgpu_mutex_destroy(&p->pool_lock);

	gpu_sema_dbg(pool_to_gk20a(p),
		     "Freed semaphore pool! (idx=%llu)", p->page_idx);
	nvgpu_kfree(p->sema_sea->gk20a, p);
}

void nvgpu_semaphore_pool_get(struct nvgpu_semaphore_pool *p)
{
	nvgpu_ref_get(&p->ref);
}

void nvgpu_semaphore_pool_put(struct nvgpu_semaphore_pool *p)
{
	nvgpu_ref_put(&p->ref, nvgpu_semaphore_pool_free);
}

/*
 * Get the address for a semaphore_pool - if global is true then return the
 * global RO address instead of the RW address owned by the semaphore's VM.
 */
u64 nvgpu_semaphore_pool_gpu_va(struct nvgpu_semaphore_pool *p, bool global)
{
	if (!global) {
		return p->gpu_va;
	}

	return p->gpu_va_ro + (NVGPU_CPU_PAGE_SIZE * p->page_idx);
}

/*
 * Return the index into the sea bitmap
 */
u64 nvgpu_semaphore_pool_get_page_idx(struct nvgpu_semaphore_pool *p)
{
	return p->page_idx;
}
