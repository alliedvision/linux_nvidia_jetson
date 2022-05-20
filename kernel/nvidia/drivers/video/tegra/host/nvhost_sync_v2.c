/*
 * Copyright (C) 2016-2020 NVIDIA CORPORATION.  All rights reserved.
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

#include <linux/version.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 10, 0)
#include <linux/dma-fence.h>
#include <linux/dma-fence-array.h>
#else
#include <linux/fence.h>
#include <linux/fence-array.h>
#define dma_fence fence
#define dma_fence_context_alloc fence_context_alloc
#define dma_fence_ops fence_ops
#define dma_fence_get fence_get
#define dma_fence_put fence_put
#define dma_fence_default_wait fence_default_wait
#define dma_fence_init fence_init
#define dma_fence_array fence_array
#define dma_fence_array_create fence_array_create
#define to_dma_fence_array to_fence_array
#endif

#include <linux/file.h>
#include <linux/slab.h>
#include <linux/sync_file.h>

#include "host1x/host1x.h"
#include "nvhost_intr.h"
#include "nvhost_syncpt.h"

struct nvhost_dma_fence {
	struct dma_fence base;
	spinlock_t lock;

	struct nvhost_syncpt *syncpt;
	u32 id;
	u32 threshold;

	struct nvhost_master *host;
	void *waiter;

	char timeline_name[10];
};

static inline struct nvhost_dma_fence *to_nvhost_dma_fence(
	struct dma_fence *fence);

static const char *nvhost_dma_fence_get_driver_name(struct dma_fence *fence)
{
	return "nvhost";
}

static const char *nvhost_dma_fence_get_timeline_name(struct dma_fence *fence)
{
	struct nvhost_dma_fence *f = to_nvhost_dma_fence(fence);

	return f->timeline_name;
}

static bool nvhost_dma_fence_signaled(struct dma_fence *fence)
{
	struct nvhost_dma_fence *f = to_nvhost_dma_fence(fence);

	return nvhost_syncpt_is_expired(f->syncpt, f->id, f->threshold);
}

static void nvhost_dma_fence_release(struct dma_fence *fence)
{
	struct nvhost_dma_fence *f = to_nvhost_dma_fence(fence);

	if (f->waiter)
		nvhost_intr_put_ref(&f->host->intr, f->id, f->waiter);

	kfree(f);
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 19, 0)
static bool nvhost_dma_fence_enable_signaling(struct dma_fence *fence)
{
	struct nvhost_dma_fence *f = to_nvhost_dma_fence(fence);

	if (nvhost_syncpt_is_expired(f->syncpt, f->id, f->threshold))
		return false;

	return true;
}
#endif

static const struct dma_fence_ops nvhost_dma_fence_ops = {
	.get_driver_name = nvhost_dma_fence_get_driver_name,
	.get_timeline_name = nvhost_dma_fence_get_timeline_name,
	.signaled = nvhost_dma_fence_signaled,
	.release = nvhost_dma_fence_release,
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 19, 0)
	.enable_signaling = nvhost_dma_fence_enable_signaling,
	.wait = dma_fence_default_wait,
#endif
};

static inline struct nvhost_dma_fence *to_nvhost_dma_fence(
	struct dma_fence *fence)
{
	if (fence->ops != &nvhost_dma_fence_ops)
		return NULL;

	return (struct nvhost_dma_fence *)fence;
}

/**
 * nvhost_dma_fence_is_waitable() - Check if DMA fence can be waited by hardware
 * @fence: DMA fence
 *
 * Check is @fence is only backed by Host1x syncpoints and can therefore be
 * waited using only hardware.
 */
bool nvhost_dma_fence_is_waitable(struct dma_fence *fence)
{
	struct dma_fence_array *array;
	int i;

	array = to_dma_fence_array(fence);
	if (!array)
		return fence->ops == &nvhost_dma_fence_ops;

	for (i = 0; i < array->num_fences; ++i) {
		if (array->fences[i]->ops != &nvhost_dma_fence_ops)
			return false;
	}

	return true;
}

int nvhost_dma_fence_unpack(struct dma_fence *fence, u32 *id, u32 *threshold)
{
	struct nvhost_dma_fence *f = to_nvhost_dma_fence(fence);

	if (!f)
		return -EINVAL;

	*id = f->id;
	*threshold = f->threshold;

	return 0;
}
EXPORT_SYMBOL(nvhost_dma_fence_unpack);

static struct dma_fence *nvhost_dma_fence_create_single(
			struct nvhost_syncpt *syncpt, u32 id, u32 threshold)
{
	struct nvhost_dma_fence *f;
	void *waiter;
	int err;

	f = kzalloc(sizeof(*f), GFP_KERNEL);
	if (!f)
		return ERR_PTR(-ENOMEM);

	f->host = syncpt_to_dev(syncpt);
	f->syncpt = syncpt;
	f->id = id;
	f->threshold = threshold;
	snprintf(f->timeline_name, ARRAY_SIZE(f->timeline_name), "sp%d", id);

	spin_lock_init(&f->lock);
	dma_fence_init(&f->base, &nvhost_dma_fence_ops, &f->lock,
		       syncpt->syncpt_context_base + id, threshold);

	if (nvhost_syncpt_is_expired(f->syncpt, f->id, f->threshold)) {
		dma_fence_signal(&f->base);
	} else {
		waiter = nvhost_intr_alloc_waiter();
		if (!waiter) {
			kfree(f);
			return ERR_PTR(-ENOMEM);
		}

		err = nvhost_intr_add_action(&f->host->intr, id, threshold,
					     NVHOST_INTR_ACTION_SIGNAL_SYNC_PT,
					     f, waiter, &f->waiter);

		if (err) {
			kfree(waiter);
			dma_fence_put((struct dma_fence *)f);
			return ERR_PTR(err);
		}
	}

	return (struct dma_fence *)f;
}

struct dma_fence *nvhost_dma_fence_create(
	struct nvhost_syncpt *syncpt, struct nvhost_ctrl_sync_fence_info *pts,
	int num_pts)
{
	struct dma_fence **fences, *array;
	int i, err;

	if (num_pts == 1)
		return nvhost_dma_fence_create_single(syncpt, pts->id,
						      pts->thresh);

	fences = kmalloc_array(num_pts, sizeof(*fences), GFP_KERNEL);
	if (!fences)
		return ERR_PTR(-ENOMEM);

	for (i = 0; i < num_pts; ++i) {
		fences[i] = nvhost_dma_fence_create_single(syncpt, pts[i].id,
							   pts[i].thresh);
		if (IS_ERR(fences[i])) {
			err = PTR_ERR(fences[i]);
			goto put_fences;
		}
	}

	array = (struct dma_fence *)dma_fence_array_create(
		num_pts, fences, dma_fence_context_alloc(1), 1, false);
	if  (!array) {
		err = -ENOMEM;
		goto put_fences;
	}

	return array;

put_fences:
	while (--i >= 0)
		dma_fence_put(fences[i]);

	kfree(fences);

	return ERR_PTR(err);
}
