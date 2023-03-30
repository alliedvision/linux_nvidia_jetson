/*
 * Copyright (c) 2017-2022, NVIDIA Corporation.  All rights reserved.
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

#include <nvgpu/enabled.h>
#include <nvgpu/debug.h>
#include <nvgpu/error_notifier.h>
#include <nvgpu/barrier.h>
#include <nvgpu/os_sched.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/channel.h>
#include <nvgpu/dma.h>
#include <nvgpu/fence.h>
#include <nvgpu/grmgr.h>

/*
 * This is required for nvgpu_vm_find_buf() which is used in the tracing
 * code. Once we can get and access userspace buffers without requiring
 * direct dma_buf usage this can be removed.
 */
#include <nvgpu/linux/vm.h>

#include "channel.h"
#include "ioctl_channel.h"
#include "ioctl.h"
#include "os_linux.h"
#include "dmabuf_priv.h"

#include <nvgpu/hw/gk20a/hw_pbdma_gk20a.h>

#include <linux/uaccess.h>
#include <linux/dma-buf.h>
#include <linux/dma-direction.h>

#include <nvgpu/trace.h>
#include <uapi/linux/nvgpu.h>

#include "sync_sema_android.h"
#include "sync_sema_dma.h"
#include <nvgpu/linux/os_fence_dma.h>

u32 nvgpu_submit_gpfifo_user_flags_to_common_flags(u32 user_flags)
{
	u32 flags = 0;

	if (user_flags & NVGPU_SUBMIT_GPFIFO_FLAGS_FENCE_WAIT)
		flags |= NVGPU_SUBMIT_FLAGS_FENCE_WAIT;

	if (user_flags & NVGPU_SUBMIT_GPFIFO_FLAGS_FENCE_GET)
		flags |= NVGPU_SUBMIT_FLAGS_FENCE_GET;

	if (user_flags & NVGPU_SUBMIT_GPFIFO_FLAGS_HW_FORMAT)
		flags |= NVGPU_SUBMIT_FLAGS_HW_FORMAT;

	if (user_flags & NVGPU_SUBMIT_GPFIFO_FLAGS_SYNC_FENCE)
		flags |= NVGPU_SUBMIT_FLAGS_SYNC_FENCE;

	if (user_flags & NVGPU_SUBMIT_GPFIFO_FLAGS_SUPPRESS_WFI)
		flags |= NVGPU_SUBMIT_FLAGS_SUPPRESS_WFI;

	if (user_flags & NVGPU_SUBMIT_GPFIFO_FLAGS_SKIP_BUFFER_REFCOUNTING)
		flags |= NVGPU_SUBMIT_FLAGS_SKIP_BUFFER_REFCOUNTING;

	return flags;
}

/*
 * API to convert error_notifiers in common code and of the form
 * NVGPU_ERR_NOTIFIER_* into Linux specific error_notifiers exposed to user
 * space and of the form  NVGPU_CHANNEL_*
 */
static u32 nvgpu_error_notifier_to_channel_notifier(u32 error_notifier)
{
	switch (error_notifier) {
	case NVGPU_ERR_NOTIFIER_FIFO_ERROR_IDLE_TIMEOUT:
		return NVGPU_CHANNEL_FIFO_ERROR_IDLE_TIMEOUT;
	case NVGPU_ERR_NOTIFIER_GR_ERROR_SW_METHOD:
		return NVGPU_CHANNEL_GR_ERROR_SW_METHOD;
	case NVGPU_ERR_NOTIFIER_GR_ERROR_SW_NOTIFY:
		return NVGPU_CHANNEL_GR_ERROR_SW_NOTIFY;
	case NVGPU_ERR_NOTIFIER_GR_EXCEPTION:
		return NVGPU_CHANNEL_GR_EXCEPTION;
	case NVGPU_ERR_NOTIFIER_GR_SEMAPHORE_TIMEOUT:
		return NVGPU_CHANNEL_GR_SEMAPHORE_TIMEOUT;
	case NVGPU_ERR_NOTIFIER_GR_ILLEGAL_NOTIFY:
		return NVGPU_CHANNEL_GR_ILLEGAL_NOTIFY;
	case NVGPU_ERR_NOTIFIER_FIFO_ERROR_MMU_ERR_FLT:
		return NVGPU_CHANNEL_FIFO_ERROR_MMU_ERR_FLT;
	case NVGPU_ERR_NOTIFIER_PBDMA_ERROR:
		return NVGPU_CHANNEL_PBDMA_ERROR;
	case NVGPU_ERR_NOTIFIER_FECS_ERR_UNIMP_FIRMWARE_METHOD:
		return NVGPU_CHANNEL_FECS_ERR_UNIMP_FIRMWARE_METHOD;
	case NVGPU_ERR_NOTIFIER_RESETCHANNEL_VERIF_ERROR:
		return NVGPU_CHANNEL_RESETCHANNEL_VERIF_ERROR;
	case NVGPU_ERR_NOTIFIER_PBDMA_PUSHBUFFER_CRC_MISMATCH:
		return NVGPU_CHANNEL_PBDMA_PUSHBUFFER_CRC_MISMATCH;
	}

	pr_warn("%s: invalid error_notifier requested %u\n", __func__, error_notifier);

	return error_notifier;
}

/**
 * nvgpu_set_err_notifier_locked()
 * Should be called with ch->error_notifier_mutex held
 *
 * error should be of the form  NVGPU_ERR_NOTIFIER_*
 */
void nvgpu_set_err_notifier_locked(struct nvgpu_channel *ch, u32 error)
{
	struct nvgpu_channel_linux *priv = ch->os_priv;

	error = nvgpu_error_notifier_to_channel_notifier(error);

	if (priv->error_notifier.dmabuf) {
		struct nvgpu_notification *notification =
			priv->error_notifier.notification;
		struct timespec64 time_data;
		u64 nsec;

		ktime_get_real_ts64(&time_data);
		nsec = time_data.tv_sec * 1000000000u + time_data.tv_nsec;
		notification->time_stamp.nanoseconds[0] =
				(u32)nsec;
		notification->time_stamp.nanoseconds[1] =
				(u32)(nsec >> 32);
		notification->info32 = error;
		nvgpu_wmb();
		notification->status = 0xffff;

		if (error == NVGPU_CHANNEL_RESETCHANNEL_VERIF_ERROR) {
			nvgpu_log_info(ch->g,
			    "error notifier set to %d for ch %d",
			    error, ch->chid);
		} else {
			nvgpu_err(ch->g,
			    "error notifier set to %d for ch %d",
			    error, ch->chid);
		}
	}
}

/* error should be of the form  NVGPU_ERR_NOTIFIER_* */
void nvgpu_set_err_notifier(struct nvgpu_channel *ch, u32 error)
{
	struct nvgpu_channel_linux *priv = ch->os_priv;

	nvgpu_mutex_acquire(&priv->error_notifier.mutex);
	nvgpu_set_err_notifier_locked(ch, error);
	nvgpu_mutex_release(&priv->error_notifier.mutex);
}

void nvgpu_set_err_notifier_if_empty(struct nvgpu_channel *ch, u32 error)
{
	struct nvgpu_channel_linux *priv = ch->os_priv;

	nvgpu_mutex_acquire(&priv->error_notifier.mutex);
	if (priv->error_notifier.dmabuf) {
		struct nvgpu_notification *notification =
			priv->error_notifier.notification;

		/* Don't overwrite error flag if it is already set */
		if (notification->status != 0xffff)
			nvgpu_set_err_notifier_locked(ch, error);
	}
	nvgpu_mutex_release(&priv->error_notifier.mutex);
}

/* error_notifier should be of the form  NVGPU_ERR_NOTIFIER_* */
bool nvgpu_is_err_notifier_set(struct nvgpu_channel *ch, u32 error_notifier)
{
	struct nvgpu_channel_linux *priv = ch->os_priv;
	bool notifier_set = false;

	error_notifier = nvgpu_error_notifier_to_channel_notifier(error_notifier);

	nvgpu_mutex_acquire(&priv->error_notifier.mutex);
	if (priv->error_notifier.dmabuf) {
		struct nvgpu_notification *notification =
			priv->error_notifier.notification;
		u32 err = notification->info32;

		if (err == error_notifier)
			notifier_set = true;
	}
	nvgpu_mutex_release(&priv->error_notifier.mutex);

	return notifier_set;
}

static void gk20a_channel_update_runcb_fn(struct work_struct *work)
{
	struct nvgpu_channel_completion_cb *completion_cb =
		container_of(work, struct nvgpu_channel_completion_cb, work);
	struct nvgpu_channel_linux *priv =
		container_of(completion_cb,
				struct nvgpu_channel_linux, completion_cb);
	struct nvgpu_channel *ch = priv->ch;
	void (*fn)(struct nvgpu_channel *, void *);
	void *user_data;

	nvgpu_spinlock_acquire(&completion_cb->lock);
	fn = completion_cb->fn;
	user_data = completion_cb->user_data;
	nvgpu_spinlock_release(&completion_cb->lock);

	if (fn)
		fn(ch, user_data);
}

static void nvgpu_channel_work_completion_init(struct nvgpu_channel *ch)
{
	struct nvgpu_channel_linux *priv = ch->os_priv;

	priv->completion_cb.fn = NULL;
	priv->completion_cb.user_data = NULL;
	nvgpu_spinlock_init(&priv->completion_cb.lock);
	INIT_WORK(&priv->completion_cb.work, gk20a_channel_update_runcb_fn);
}

static void nvgpu_channel_work_completion_clear(struct nvgpu_channel *ch)
{
	struct nvgpu_channel_linux *priv = ch->os_priv;

	nvgpu_spinlock_acquire(&priv->completion_cb.lock);
	priv->completion_cb.fn = NULL;
	priv->completion_cb.user_data = NULL;
	nvgpu_spinlock_release(&priv->completion_cb.lock);
	cancel_work_sync(&priv->completion_cb.work);
}

static void nvgpu_channel_work_completion_signal(struct nvgpu_channel *ch)
{
	struct nvgpu_channel_linux *priv = ch->os_priv;

	if (priv->completion_cb.fn)
		schedule_work(&priv->completion_cb.work);
}

static void nvgpu_channel_work_completion_cancel_sync(struct nvgpu_channel *ch)
{
	struct nvgpu_channel_linux *priv = ch->os_priv;

	if (priv->completion_cb.fn)
		cancel_work_sync(&priv->completion_cb.work);
}

struct nvgpu_channel *gk20a_open_new_channel_with_cb(struct gk20a *g,
		void (*update_fn)(struct nvgpu_channel *, void *),
		void *update_fn_data,
		u32 runlist_id,
		bool is_privileged_channel)
{
	struct nvgpu_channel *ch;
	struct nvgpu_channel_linux *priv;

	ch = nvgpu_channel_open_new(g, runlist_id, is_privileged_channel,
				nvgpu_current_pid(g), nvgpu_current_tid(g));

	if (ch) {
		priv = ch->os_priv;
		nvgpu_spinlock_acquire(&priv->completion_cb.lock);
		priv->completion_cb.fn = update_fn;
		priv->completion_cb.user_data = update_fn_data;
		nvgpu_spinlock_release(&priv->completion_cb.lock);
	}

	return ch;
}

static void nvgpu_channel_open_linux(struct nvgpu_channel *ch)
{
}

static void nvgpu_channel_close_linux(struct nvgpu_channel *ch, bool force)
{
	nvgpu_channel_work_completion_clear(ch);

#if defined(CONFIG_NVGPU_CYCLESTATS)
	gk20a_channel_free_cycle_stats_buffer(ch);
	gk20a_channel_free_cycle_stats_snapshot(ch);
#endif
}

static int nvgpu_channel_alloc_linux(struct gk20a *g, struct nvgpu_channel *ch)
{
	struct nvgpu_channel_linux *priv;

	priv = nvgpu_kzalloc(g, sizeof(*priv));
	if (!priv)
		return -ENOMEM;

	ch->os_priv = priv;
	priv->ch = ch;

#ifndef CONFIG_NVGPU_SYNCFD_NONE
	ch->has_os_fence_framework_support = true;
#endif

	nvgpu_mutex_init(&priv->error_notifier.mutex);

	nvgpu_channel_work_completion_init(ch);

	return 0;
}

static void nvgpu_channel_free_linux(struct gk20a *g, struct nvgpu_channel *ch)
{
	struct nvgpu_channel_linux *priv = ch->os_priv;

	nvgpu_mutex_destroy(&priv->error_notifier.mutex);
	nvgpu_kfree(g, priv);

	ch->os_priv = NULL;

#ifndef CONFIG_NVGPU_SYNCFD_NONE
	ch->has_os_fence_framework_support = false;
#endif
}

static int nvgpu_channel_init_os_fence_framework(struct nvgpu_channel *ch,
	const char *fmt, ...)
{
	struct nvgpu_channel_linux *priv = ch->os_priv;
	struct nvgpu_os_fence_framework *fence_framework;
	char name[30];
	va_list args;

	fence_framework = &priv->fence_framework;

	va_start(args, fmt);
	(void) vsnprintf(name, sizeof(name), fmt, args);
	va_end(args);

#if defined(CONFIG_NVGPU_SYNCFD_ANDROID)
	fence_framework->timeline = gk20a_sync_timeline_create(name);

	if (!fence_framework->timeline)
		return -EINVAL;
#elif defined(CONFIG_NVGPU_SYNCFD_STABLE)
	fence_framework->context = nvgpu_sync_dma_context_create();
	fence_framework->exists = true;
#endif

	return 0;
}
static void nvgpu_channel_signal_os_fence_framework(struct nvgpu_channel *ch,
				struct nvgpu_fence_type *fence)
{
	struct nvgpu_channel_linux *priv = ch->os_priv;
	struct nvgpu_os_fence_framework *fence_framework;
#if defined(CONFIG_NVGPU_SYNCFD_STABLE)
	struct dma_fence *f;
#endif

	fence_framework = &priv->fence_framework;

#if defined(CONFIG_NVGPU_SYNCFD_ANDROID)
	gk20a_sync_timeline_signal(fence_framework->timeline);
#elif defined(CONFIG_NVGPU_SYNCFD_STABLE)
	/*
	 * This is not a good example on how to use the fence type. Don't touch
	 * the priv data. This is os-specific code for the fence unit.
	 */
	f = nvgpu_get_dma_fence(&fence->priv.os_fence);
	/*
	 * Sometimes the post fence of a job isn't a file. It can be a raw
	 * semaphore for kernel-internal tracking, or a raw syncpoint for
	 * internal tracking or for exposing to user.
	 */
	if (f != NULL) {
		nvgpu_sync_dma_signal(f);
	}
#endif
}

static void nvgpu_channel_destroy_os_fence_framework(struct nvgpu_channel *ch)
{
	struct nvgpu_channel_linux *priv = ch->os_priv;
	struct nvgpu_os_fence_framework *fence_framework;

	fence_framework = &priv->fence_framework;

#if defined(CONFIG_NVGPU_SYNCFD_ANDROID)
	gk20a_sync_timeline_destroy(fence_framework->timeline);
	fence_framework->timeline = NULL;
#elif defined(CONFIG_NVGPU_SYNCFD_STABLE)
	/* fence_framework->context cannot be freed, see linux/dma-fence.h */
	fence_framework->exists = false;
#endif
}

static bool nvgpu_channel_fence_framework_exists(struct nvgpu_channel *ch)
{
	struct nvgpu_channel_linux *priv = ch->os_priv;
	struct nvgpu_os_fence_framework *fence_framework;

	fence_framework = &priv->fence_framework;

#if defined(CONFIG_NVGPU_SYNCFD_ANDROID)
	return (fence_framework->timeline != NULL);
#elif defined(CONFIG_NVGPU_SYNCFD_STABLE)
	return fence_framework->exists;
#else
	return false;
#endif
}

static int nvgpu_channel_copy_user_gpfifo(struct nvgpu_gpfifo_entry *dest,
		struct nvgpu_gpfifo_userdata userdata, u32 start, u32 length)
{
	struct nvgpu_gpfifo_entry __user *user_gpfifo = userdata.entries;
	unsigned long n;

	n = copy_from_user(dest, user_gpfifo + start,
			length * sizeof(struct nvgpu_gpfifo_entry));

	return n == 0 ? 0 : -EFAULT;
}

int nvgpu_usermode_buf_from_dmabuf(struct gk20a *g, int dmabuf_fd,
		struct nvgpu_mem *mem, struct nvgpu_usermode_buf_linux *buf)
{
	struct device *dev = dev_from_gk20a(g);
	struct dma_buf *dmabuf;
	struct sg_table *sgt;
	struct dma_buf_attachment *attachment;
	int err;

	dmabuf = dma_buf_get(dmabuf_fd);
	if (IS_ERR(dmabuf)) {
		return PTR_ERR(dmabuf);
	}

	if (gk20a_dmabuf_aperture(g, dmabuf) == APERTURE_INVALID) {
		err = -EINVAL;
		goto put_dmabuf;
	}

	sgt = nvgpu_mm_pin(dev, dmabuf, &attachment, DMA_TO_DEVICE);
	if (IS_ERR(sgt)) {
		nvgpu_err(g, "Failed to pin dma_buf!");
		err = PTR_ERR(sgt);
		goto put_dmabuf;
	}

	buf->dmabuf = dmabuf;
	buf->attachment = attachment;
	buf->sgt = sgt;

	/*
	 * This mem is unmapped and freed in a common path; for Linux, we'll
	 * also need to unref the dmabuf stuff (above) but the sgt here is only
	 * borrowed, so it cannot be freed by nvgpu_mem_*.
	 */
	mem->mem_flags  = NVGPU_MEM_FLAG_FOREIGN_SGT;
	mem->aperture   = APERTURE_SYSMEM;
	mem->skip_wmb   = 0;
	mem->size       = dmabuf->size;

	mem->priv.flags = 0;
	mem->priv.pages = NULL;
	mem->priv.sgt   = sgt;

	return 0;
put_dmabuf:
	dma_buf_put(dmabuf);
	return err;
}

void nvgpu_os_channel_free_usermode_buffers(struct nvgpu_channel *c)
{
	struct nvgpu_channel_linux *priv = c->os_priv;
	struct gk20a *g = c->g;
	struct device *dev = dev_from_gk20a(g);

	if (priv->usermode.gpfifo.dmabuf != NULL) {
		nvgpu_mm_unpin(dev, priv->usermode.gpfifo.dmabuf,
			       priv->usermode.gpfifo.attachment,
			       priv->usermode.gpfifo.sgt);
		dma_buf_put(priv->usermode.gpfifo.dmabuf);
		priv->usermode.gpfifo.dmabuf = NULL;
	}

	if (priv->usermode.userd.dmabuf != NULL) {
		nvgpu_mm_unpin(dev, priv->usermode.userd.dmabuf,
		       priv->usermode.userd.attachment,
		       priv->usermode.userd.sgt);
		dma_buf_put(priv->usermode.userd.dmabuf);
		priv->usermode.userd.dmabuf = NULL;
	}
}

static int nvgpu_channel_alloc_usermode_buffers(struct nvgpu_channel *c,
		struct nvgpu_setup_bind_args *args)
{
	struct nvgpu_channel_linux *priv = c->os_priv;
	struct gk20a *g = c->g;
	struct device *dev = dev_from_gk20a(g);
	size_t gpfifo_size;
	int err;

	if (args->gpfifo_dmabuf_fd == 0 || args->userd_dmabuf_fd == 0) {
		return -EINVAL;
	}

	if (args->gpfifo_dmabuf_offset != 0 ||
			args->userd_dmabuf_offset != 0) {
		/* TODO - not yet supported */
		return -EINVAL;
	}

	err = nvgpu_usermode_buf_from_dmabuf(g, args->gpfifo_dmabuf_fd,
			&c->usermode_gpfifo, &priv->usermode.gpfifo);
	if (err < 0) {
		return err;
	}

	gpfifo_size = max_t(u32, SZ_4K,
			args->num_gpfifo_entries *
			nvgpu_get_gpfifo_entry_size());

	if (c->usermode_gpfifo.size < gpfifo_size) {
		err = -EINVAL;
		goto free_gpfifo;
	}

	c->usermode_gpfifo.gpu_va = nvgpu_gmmu_map(c->vm, &c->usermode_gpfifo,
			0, gk20a_mem_flag_read_only,
			false, c->usermode_gpfifo.aperture);

	if (c->usermode_gpfifo.gpu_va == 0) {
		err = -ENOMEM;
		goto unmap_free_gpfifo;
	}

	err = nvgpu_usermode_buf_from_dmabuf(g, args->userd_dmabuf_fd,
			&c->usermode_userd, &priv->usermode.userd);
	if (err < 0) {
		goto unmap_free_gpfifo;
	}

	args->work_submit_token = g->ops.usermode.doorbell_token(c);

	return 0;
unmap_free_gpfifo:
	nvgpu_dma_unmap_free(c->vm, &c->usermode_gpfifo);
free_gpfifo:
	nvgpu_mm_unpin(dev, priv->usermode.gpfifo.dmabuf,
		       priv->usermode.gpfifo.attachment,
		       priv->usermode.gpfifo.sgt);
	dma_buf_put(priv->usermode.gpfifo.dmabuf);
	priv->usermode.gpfifo.dmabuf = NULL;
	return err;
}

int nvgpu_channel_init_support_linux(struct nvgpu_os_linux *l)
{
	struct gk20a *g = &l->g;
	struct nvgpu_fifo *f = &g->fifo;
	int chid;
	int err;

	for (chid = 0; chid < (int)f->num_channels; chid++) {
		struct nvgpu_channel *ch = &f->channel[chid];

		err = nvgpu_channel_alloc_linux(g, ch);
		if (err)
			goto err_clean;
	}

	g->os_channel.open = nvgpu_channel_open_linux;
	g->os_channel.close = nvgpu_channel_close_linux;
	g->os_channel.work_completion_signal =
		nvgpu_channel_work_completion_signal;
	g->os_channel.work_completion_cancel_sync =
		nvgpu_channel_work_completion_cancel_sync;

	g->os_channel.os_fence_framework_inst_exists =
		nvgpu_channel_fence_framework_exists;
	g->os_channel.init_os_fence_framework =
		nvgpu_channel_init_os_fence_framework;
	g->os_channel.signal_os_fence_framework =
		nvgpu_channel_signal_os_fence_framework;
	g->os_channel.destroy_os_fence_framework =
		nvgpu_channel_destroy_os_fence_framework;

	g->os_channel.copy_user_gpfifo =
		nvgpu_channel_copy_user_gpfifo;

	g->os_channel.alloc_usermode_buffers =
		nvgpu_channel_alloc_usermode_buffers;

	g->os_channel.free_usermode_buffers =
		nvgpu_os_channel_free_usermode_buffers;

	return 0;

err_clean:
	for (; chid >= 0; chid--) {
		struct nvgpu_channel *ch = &f->channel[chid];

		nvgpu_channel_free_linux(g, ch);
	}
	return err;
}

void nvgpu_channel_remove_support_linux(struct nvgpu_os_linux *l)
{
	struct gk20a *g = &l->g;
	struct nvgpu_fifo *f = &g->fifo;
	unsigned int chid;

	for (chid = 0; chid < f->num_channels; chid++) {
		struct nvgpu_channel *ch = &f->channel[chid];

		nvgpu_channel_free_linux(g, ch);
	}

	g->os_channel.os_fence_framework_inst_exists = NULL;
	g->os_channel.init_os_fence_framework = NULL;
	g->os_channel.signal_os_fence_framework = NULL;
	g->os_channel.destroy_os_fence_framework = NULL;
}

u32 nvgpu_channel_get_max_subctx_count(struct nvgpu_channel *ch)
{
	struct nvgpu_channel_linux *priv = ch->os_priv;
	struct gk20a *g = ch->g;
	u32 gpu_instance_id;

	if (priv->cdev == NULL) {
		/* CE channels reserved by nvgpu do not have cdev pointer */
		return nvgpu_grmgr_get_gpu_instance_max_veid_count(g, 0U);
	}

	gpu_instance_id = nvgpu_get_gpu_instance_id_from_cdev(g, priv->cdev);
	nvgpu_assert(gpu_instance_id < g->mig.num_gpu_instances);

	return nvgpu_grmgr_get_gpu_instance_max_veid_count(g, gpu_instance_id);
}

#ifdef CONFIG_DEBUG_FS
static void trace_write_pushbuffer(struct nvgpu_channel *c,
				   struct nvgpu_gpfifo_entry *g)
{
	void *mem = NULL;
	unsigned int words;
	u64 offset;
	struct dma_buf *dmabuf = NULL;

	if (gk20a_debug_trace_cmdbuf) {
		u64 gpu_va = (u64)g->entry0 |
			(u64)((u64)pbdma_gp_entry1_get_hi_v(g->entry1) << 32);
		int err;

		words = pbdma_gp_entry1_length_v(g->entry1);
		err = nvgpu_vm_find_buf(c->vm, gpu_va, &dmabuf, &offset);
		if (!err)
			mem = gk20a_dmabuf_vmap(dmabuf);
	}

	if (mem) {
#ifdef CONFIG_NVGPU_TRACE
		u32 i;
		/*
		 * Write in batches of 128 as there seems to be a limit
		 * of how much you can output to ftrace at once.
		 */
		for (i = 0; i < words; i += 128U) {
			trace_gk20a_push_cmdbuf(
				c->g->name,
				0,
				min(words - i, 128U),
				offset + i * sizeof(u32),
				mem);
		}
#endif
		gk20a_dmabuf_vunmap(dmabuf, mem);
	}
}

void trace_write_pushbuffers(struct nvgpu_channel *c, u32 count)
{
	struct nvgpu_gpfifo_entry *gp = c->gpfifo.mem.cpu_va;
	u32 n = c->gpfifo.entry_num;
	u32 start = c->gpfifo.put;
	u32 i;

	if (!gk20a_debug_trace_cmdbuf)
		return;

	if (!gp)
		return;

	for (i = 0; i < count; i++)
		trace_write_pushbuffer(c, &gp[(start + i) % n]);
}
#endif
