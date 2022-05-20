/*
 * Copyright (c) 2017-2019, NVIDIA CORPORATION.  All rights reserved.
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
#ifndef NVGPU_LINUX_CHANNEL_H
#define NVGPU_LINUX_CHANNEL_H

#include <linux/workqueue.h>
#include <linux/dma-buf.h>

#include <nvgpu/types.h>

struct nvgpu_channel;
struct nvgpu_gpfifo;
struct nvgpu_submit_gpfifo_args;
struct nvgpu_channel_fence;
struct nvgpu_fence_type;
struct nvgpu_swprofile;
struct nvgpu_os_linux;
struct nvgpu_cdev;

struct sync_fence;
struct sync_timeline;

struct nvgpu_channel_completion_cb {
	/*
	 * Signal channel owner via a callback, if set, in job cleanup with
	 * schedule_work. Means that something finished on the channel (perhaps
	 * more than one job).
	 */
	void (*fn)(struct nvgpu_channel *, void *);
	void *user_data;
	/* Make access to the two above atomic */
	struct nvgpu_spinlock lock;
	/* Per-channel async work task, cannot reschedule itself */
	struct work_struct work;
};

struct nvgpu_error_notifier {
	struct dma_buf *dmabuf;
	void *vaddr;

	struct nvgpu_notification *notification;

	struct nvgpu_mutex mutex;
};

/*
 * channel-global data for sync fences created from the hardware
 * synchronization primitive in each particular channel.
 */
struct nvgpu_os_fence_framework {
#if defined(CONFIG_NVGPU_SYNCFD_ANDROID)
	struct sync_timeline *timeline;
#elif defined(CONFIG_NVGPU_SYNCFD_STABLE)
	u64 context;
	bool exists;
#endif
};

struct nvgpu_usermode_bufs_linux {
	/*
	 * Common low level info of these is stored in nvgpu_mems in
	 * channel_gk20a; these hold lifetimes for the actual dmabuf and its
	 * dma mapping.
	 */
	struct nvgpu_usermode_buf_linux {
		struct dma_buf *dmabuf;
		struct dma_buf_attachment *attachment;
		struct sg_table *sgt;
	} gpfifo, userd;
};

struct nvgpu_channel_linux {
	struct nvgpu_channel *ch;

	struct nvgpu_os_fence_framework fence_framework;

	struct nvgpu_channel_completion_cb completion_cb;
	struct nvgpu_error_notifier error_notifier;

	struct dma_buf *cyclestate_buffer_handler;

	struct nvgpu_usermode_bufs_linux usermode;

	struct nvgpu_cdev *cdev;
};

u32 nvgpu_submit_gpfifo_user_flags_to_common_flags(u32 user_flags);
int nvgpu_channel_init_support_linux(struct nvgpu_os_linux *l);
void nvgpu_channel_remove_support_linux(struct nvgpu_os_linux *l);

/* Deprecated. Use fences in new code. */
struct nvgpu_channel *gk20a_open_new_channel_with_cb(struct gk20a *g,
		void (*update_fn)(struct nvgpu_channel *, void *),
		void *update_fn_data,
		u32 runlist_id,
		bool is_privileged_channel);

#endif
