/*
 * Nvgpu Channel Synchronization Abstraction
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

#ifndef NVGPU_CHANNEL_SYNC_H
#define NVGPU_CHANNEL_SYNC_H

#include <nvgpu/atomic.h>

struct nvgpu_channel_sync;

#ifdef CONFIG_NVGPU_KERNEL_MODE_SUBMIT

struct priv_cmd_entry;
struct nvgpu_channel;
struct nvgpu_fence_type;
struct gk20a;

/* Public APIS for channel_sync below */

/*
 * Generate a gpu wait cmdbuf from sync fd.
 * Returns a gpu cmdbuf that performs the wait when executed
 */
int nvgpu_channel_sync_wait_fence_fd(struct nvgpu_channel_sync *s, int fd,
	struct priv_cmd_entry **entry, u32 max_wait_cmds);

/*
 * Increment syncpoint/semaphore.
 * Returns
 *  - a gpu cmdbuf that performs the increment when executed,
 *  - a fence that can be passed to wait_cpu() and is_expired().
 */
int nvgpu_channel_sync_incr(struct nvgpu_channel_sync *s,
	struct priv_cmd_entry **entry, struct nvgpu_fence_type *fence,
	bool need_sync_fence);

/*
 * Increment syncpoint/semaphore, so that the returned fence represents
 * work completion (may need wfi) and can be returned to user space.
 * Returns
 *  - a gpu cmdbuf that performs the increment when executed,
 *  - a fence that can be passed to wait_cpu() and is_expired(),
 *  - a nvgpu_fence_type that signals when the incr has happened.
 */
int nvgpu_channel_sync_incr_user(struct nvgpu_channel_sync *s,
	struct priv_cmd_entry **entry, struct nvgpu_fence_type *fence,
	bool wfi, bool need_sync_fence);

/*
 * Tell the sync that some progress will eventually happen on it: increase the
 * tracked max value of the underlying syncpoint/semaphore and maybe register
 * an interrupt notifier to be called if needed so that the channel gets a
 * job completion signal.
 *
 * @param register_irq [in] Register an interrupt for the increment.
 */
void nvgpu_channel_sync_mark_progress(struct nvgpu_channel_sync *s,
	bool register_irq);

/*
 * Reset the channel syncpoint/semaphore. Syncpoint increments generally
 * wrap around the range of integer values. Current max value encompasses
 * all jobs tracked by the channel. In order to reset the syncpoint,
 * the min_value is advanced and set to the global max. Similarly,
 * for semaphores.
 */
void nvgpu_channel_sync_set_min_eq_max(struct nvgpu_channel_sync *s);
/*
 * Increment the usage_counter for this instance.
 */
void nvgpu_channel_sync_get_ref(struct nvgpu_channel_sync *s);

/*
 * Decrement the usage_counter for this instance and return if equals 0.
 */
bool nvgpu_channel_sync_put_ref_and_check(struct nvgpu_channel_sync *s);
#endif /* CONFIG_NVGPU_KERNEL_MODE_SUBMIT */

/**
 * @brief Free channel syncpoint/semaphore
 *
 * @param sync [in]	Pointer to syncpoint/semaphore.
 *
 * Free the resources allocated by nvgpu_channel_sync_create.
 */
void nvgpu_channel_sync_destroy(struct nvgpu_channel_sync *sync);

/**
 * @brief Create channel syncpoint/semaphore
 *
 * @param c [in]		Pointer to Channel.
 *
 * Construct a channel_sync backed by either a syncpoint or a semaphore.
 * A channel_sync is by default constructed as backed by a syncpoint
 * if CONFIG_TEGRA_GK20A_NVHOST is defined, otherwise the channel_sync
 * is constructed as backed by a semaphore.
 *
 * @return Pointer to nvgpu_channel_sync in case of success, or NULL
 * in case of failure.
 */
struct nvgpu_channel_sync *nvgpu_channel_sync_create(struct nvgpu_channel *c);

/**
 * @brief Check if OS fence framwework is needed
 *
 * @param g [in]		Pointer to GPU
 *
 * Sync framework requires deferred job cleanup, wrapping syncs in FDs,
 * and other heavy stuff, which prevents deterministic submits.
 *
 * @return True is OS fence framework is needed.
 */
bool nvgpu_channel_sync_needs_os_fence_framework(struct gk20a *g);

#endif /* NVGPU_CHANNEL_SYNC_H */
