/*
 * Nvgpu Semaphores
 *
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

#include <nvgpu/dma.h>
#include <nvgpu/gmmu.h>
#include <nvgpu/kmem.h>
#include <nvgpu/bug.h>
#include <nvgpu/sizes.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/semaphore.h>

#include "semaphore_priv.h"


/*
 * Allocate a semaphore value object from an underlying hw counter.
 *
 * Since semaphores are ref-counted there's no explicit free for external code
 * to use. When the ref-count hits 0 the internal free will happen.
 */
struct nvgpu_semaphore *nvgpu_semaphore_alloc(
		struct nvgpu_hw_semaphore *hw_sema)
{
	struct nvgpu_semaphore_pool *pool = hw_sema->location.pool;
	struct gk20a *g = pool->sema_sea->gk20a;
	struct nvgpu_semaphore *s;

	s = nvgpu_kzalloc(g, sizeof(*s));
	if (s == NULL) {
		return NULL;
	}

	nvgpu_ref_init(&s->ref);
	s->g = g;
	s->location = hw_sema->location;
	nvgpu_atomic_set(&s->value, 0);

	/*
	 * Take a ref on the pool so that we can keep this pool alive for
	 * as long as this semaphore is alive.
	 */
	nvgpu_semaphore_pool_get(pool);

	gpu_sema_dbg(g, "Allocated semaphore (c=%d)", hw_sema->chid);

	return s;
}

static struct nvgpu_semaphore *nvgpu_semaphore_from_ref(struct nvgpu_ref *ref)
{
	return (struct nvgpu_semaphore *)
		((uintptr_t)ref - offsetof(struct nvgpu_semaphore, ref));
}

static void nvgpu_semaphore_free(struct nvgpu_ref *ref)
{
	struct nvgpu_semaphore *s = nvgpu_semaphore_from_ref(ref);

	nvgpu_semaphore_pool_put(s->location.pool);

	nvgpu_kfree(s->g, s);
}

void nvgpu_semaphore_put(struct nvgpu_semaphore *s)
{
	nvgpu_ref_put(&s->ref, nvgpu_semaphore_free);
}

void nvgpu_semaphore_get(struct nvgpu_semaphore *s)
{
	nvgpu_ref_get(&s->ref);
}

/*
 * Return the address of a specific semaphore.
 *
 * Don't call this on a semaphore you don't own - the VA returned will make no
 * sense in your specific channel's VM.
 */
u64 nvgpu_semaphore_gpu_rw_va(struct nvgpu_semaphore *s)
{
	return nvgpu_semaphore_pool_gpu_va(s->location.pool, false) +
		s->location.offset;
}

/*
 * Get the global RO address for the semaphore. Can be called on any semaphore
 * regardless of whether you own it.
 */
u64 nvgpu_semaphore_gpu_ro_va(struct nvgpu_semaphore *s)
{
	return nvgpu_semaphore_pool_gpu_va(s->location.pool, true) +
		s->location.offset;
}

/*
 * Read the underlying value from a semaphore.
 */
u32 nvgpu_semaphore_read(struct nvgpu_semaphore *s)
{
	return nvgpu_mem_rd(s->g, &s->location.pool->rw_mem,
			s->location.offset);
}

u32 nvgpu_semaphore_get_value(struct nvgpu_semaphore *s)
{
	return (u32)nvgpu_atomic_read(&s->value);
}

bool nvgpu_semaphore_is_released(struct nvgpu_semaphore *s)
{
	u32 sema_val = nvgpu_semaphore_read(s);
	u32 wait_payload = nvgpu_semaphore_get_value(s);

	return nvgpu_semaphore_value_released(wait_payload, sema_val);
}

bool nvgpu_semaphore_is_acquired(struct nvgpu_semaphore *s)
{
	return !nvgpu_semaphore_is_released(s);
}

bool nvgpu_semaphore_can_wait(struct nvgpu_semaphore *s)
{
	return s->ready_to_wait;
}

/*
 * Update nvgpu-tracked shadow of the value in "hw_sema" and mark the threshold
 * value to "s" which represents the increment that the caller must write in a
 * pushbuf. The same nvgpu_semaphore will also represent an output fence; when
 * nvgpu_semaphore_is_released(s) == true, the gpu is done with this increment.
 */
void nvgpu_semaphore_prepare(struct nvgpu_semaphore *s,
		struct nvgpu_hw_semaphore *hw_sema)
{
	/* One submission increments the next value by one. */
	int next = nvgpu_hw_semaphore_read_next(hw_sema) + 1;

	/* "s" should be an uninitialized sema. */
	WARN_ON(s->ready_to_wait);

	nvgpu_atomic_set(&s->value, next);
	s->ready_to_wait = true;

	gpu_sema_verbose_dbg(s->g, "PREP sema for c=%d (%u)",
			     hw_sema->chid, next);
}

u64 nvgpu_semaphore_get_hw_pool_page_idx(struct nvgpu_semaphore *s)
{
	return nvgpu_semaphore_pool_get_page_idx(s->location.pool);
}

