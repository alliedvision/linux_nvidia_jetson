/*
 * Semaphore Sync Framework Integration
 *
 * Copyright (c) 2020, NVIDIA Corporation.  All rights reserved.
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

#include <nvgpu/gk20a.h>
#include <nvgpu/semaphore.h>

#include "channel.h"

#include <linux/file.h>
#include <linux/sync_file.h>
#include <linux/dma-fence.h>
#include <linux/dma-fence-array.h>

struct nvgpu_dma_fence {
	struct dma_fence base;
	spinlock_t lock;

	/*
	 * The origin of this sema (a channel) can get closed before this fence
	 * gets freed. This sema would still hold a reference to the pool where
	 * it was allocated from. Another channel may safely get the same sema
	 * location from the pool; the sema will be and stay expired here.
	 */
	struct nvgpu_semaphore *sema;
	struct gk20a *g;
	char timeline_name[16]; /* "ch%d-user" */
};

static inline struct nvgpu_dma_fence *to_nvgpu_dma_fence(struct dma_fence *fence);

static const char *nvgpu_dma_fence_get_driver_name(struct dma_fence *fence)
{
	struct nvgpu_dma_fence *nvfence = to_nvgpu_dma_fence(fence);

	return nvfence->g->name;
}

static const char *nvgpu_dma_fence_get_timeline_name(struct dma_fence *fence)
{
	struct nvgpu_dma_fence *nvfence = to_nvgpu_dma_fence(fence);

	/*
	 * This shouldn't end with a digit because the caller appends the
	 * context number which would then be confusing.
	 */
	return nvfence->timeline_name;
}

static bool nvgpu_dma_fence_enable_signaling(struct dma_fence *fence)
{
	struct nvgpu_dma_fence *f = to_nvgpu_dma_fence(fence);

	if (nvgpu_semaphore_is_released(f->sema))
		return false;

	/* signaling of all semas is always enabled */
	return true;
}

static bool nvgpu_dma_fence_signaled(struct dma_fence *fence)
{
	struct nvgpu_dma_fence *f = to_nvgpu_dma_fence(fence);

	return nvgpu_semaphore_is_released(f->sema);
}

static void nvgpu_dma_fence_release(struct dma_fence *fence)
{
	struct nvgpu_dma_fence *f = to_nvgpu_dma_fence(fence);
	struct gk20a *g = f->g;

	nvgpu_semaphore_put(f->sema);

	nvgpu_kfree(g, f);
}

static const struct dma_fence_ops nvgpu_dma_fence_ops = {
	.get_driver_name = nvgpu_dma_fence_get_driver_name,
	.get_timeline_name = nvgpu_dma_fence_get_timeline_name,
	.enable_signaling = nvgpu_dma_fence_enable_signaling,
	.signaled = nvgpu_dma_fence_signaled,
	.wait = dma_fence_default_wait,
	.release = nvgpu_dma_fence_release,
};

static inline struct nvgpu_dma_fence *to_nvgpu_dma_fence(struct dma_fence *fence)
{
	if (fence->ops != &nvgpu_dma_fence_ops)
		return NULL;

	return container_of(fence, struct nvgpu_dma_fence, base);
}

/* Public API */

u64 nvgpu_sync_dma_context_create(void)
{
	/* syncs in each context can be compared against each other */
	return dma_fence_context_alloc(1);
}

static bool is_nvgpu_dma_fence_array(struct dma_fence *fence)
{
	struct dma_fence_array *farray = to_dma_fence_array(fence);
	unsigned i;

	if (farray == NULL) {
		return false;
	}

	for (i = 0; i < farray->num_fences; i++) {
		if (to_nvgpu_dma_fence(farray->fences[i]) == NULL) {
			return false;
		}
	}

	return true;
}

u32 nvgpu_dma_fence_length(struct dma_fence *fence)
{
	if (to_nvgpu_dma_fence(fence) != NULL) {
		return 1;
	}

	if (is_nvgpu_dma_fence_array(fence)) {
		struct dma_fence_array *farray = to_dma_fence_array(fence);

		return farray->num_fences;
	}

	/*
	 * this shall be called only after a is_nvgpu_dma_fence_or_array check
	 */
	WARN_ON(1);

	return 0;
}

static bool is_nvgpu_dma_fence_or_array(struct dma_fence *fence)
{
	return to_nvgpu_dma_fence(fence) != NULL ||
		is_nvgpu_dma_fence_array(fence);
}

static struct nvgpu_semaphore *nvgpu_dma_fence_sema(struct dma_fence *fence)
{
	struct nvgpu_dma_fence *f = to_nvgpu_dma_fence(fence);

	if (f != NULL) {
		nvgpu_semaphore_get(f->sema);
		return f->sema;
	}

	return NULL;
}

struct nvgpu_semaphore *nvgpu_dma_fence_nth(struct dma_fence *fence, u32 i)
{
	struct nvgpu_semaphore *s = nvgpu_dma_fence_sema(fence);
	struct dma_fence_array *farray;

	if (s != NULL && i == 0) {
		return s;
	}

	farray = to_dma_fence_array(fence);
	nvgpu_assert(i < farray->num_fences);
	return nvgpu_dma_fence_sema(farray->fences[i]);
}

void nvgpu_sync_dma_signal(struct dma_fence *fence)
{
	if (WARN_ON(to_nvgpu_dma_fence(fence) == NULL)) {
		return;
	}

	dma_fence_signal(fence);
}

struct dma_fence *nvgpu_sync_dma_fence_fdget(int fd)
{
	struct dma_fence *fence = sync_file_get_fence(fd);

	if (fence == NULL)
		return NULL;

	if (is_nvgpu_dma_fence_or_array(fence)) {
		return fence;
	} else {
		dma_fence_put(fence);
		return NULL;
	}
}

struct dma_fence *nvgpu_sync_dma_create(struct nvgpu_channel *c,
		struct nvgpu_semaphore *sema)
{
	struct nvgpu_channel_linux *os_channel_priv = c->os_priv;
	struct nvgpu_os_fence_framework *fence_framework;
	struct nvgpu_dma_fence *f;
	struct gk20a *g = c->g;
	u64 context;

	f = nvgpu_kzalloc(g, sizeof(*f));
	if (f == NULL) {
		return NULL;
	}

	f->g = g;
	f->sema = sema;
	snprintf(f->timeline_name, sizeof(f->timeline_name),
		"ch%d-user", c->chid);
	spin_lock_init(&f->lock);

	fence_framework = &os_channel_priv->fence_framework;
	context = fence_framework->context;

	/* our sema values are u32. The dma fence seqnos are unsigned int. */
	dma_fence_init(&f->base, &nvgpu_dma_fence_ops, &f->lock, context,
			(unsigned)nvgpu_semaphore_get_value(sema));
	nvgpu_semaphore_get(sema);

	return &f->base;
}
