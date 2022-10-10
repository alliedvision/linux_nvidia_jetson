/*
 * GK20A Graphics channel
 *
 * Copyright (c) 2011-2022, NVIDIA CORPORATION.  All rights reserved.
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

#include <nvgpu/trace.h>
#include <linux/file.h>
#include <linux/anon_inodes.h>
#include <linux/dma-buf.h>
#include <linux/poll.h>
#include <uapi/linux/nvgpu.h>

#include <nvgpu/semaphore.h>
#include <nvgpu/timers.h>
#include <nvgpu/kmem.h>
#include <nvgpu/log.h>
#include <nvgpu/list.h>
#include <nvgpu/debug.h>
#include <nvgpu/enabled.h>
#include <nvgpu/error_notifier.h>
#include <nvgpu/barrier.h>
#include <nvgpu/nvhost.h>
#include <nvgpu/os_sched.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/engines.h>
#include <nvgpu/channel.h>
#include <nvgpu/channel_sync.h>
#include <nvgpu/channel_sync_syncpt.h>
#include <nvgpu/channel_user_syncpt.h>
#include <nvgpu/watchdog.h>
#include <nvgpu/runlist.h>
#include <nvgpu/gr/ctx.h>
#include <nvgpu/gr/obj_ctx.h>
#include <nvgpu/gr/gr_instances.h>
#include <nvgpu/fence.h>
#include <nvgpu/preempt.h>
#include <nvgpu/swprofile.h>
#include <nvgpu/nvgpu_init.h>
#include <nvgpu/user_fence.h>
#include <nvgpu/grmgr.h>

#include <nvgpu/fifo/swprofile.h>

#include "platform_gk20a.h"
#include "ioctl_channel.h"
#include "ioctl.h"
#include "channel.h"
#include "os_linux.h"
#include "dmabuf_priv.h"

/* the minimal size of client buffer */
#define CSS_MIN_CLIENT_SNAPSHOT_SIZE				\
		(sizeof(struct gk20a_cs_snapshot_fifo) +	\
		sizeof(struct gk20a_cs_snapshot_fifo_entry) * 256)

#ifdef CONFIG_NVGPU_TRACE
static const char *gr_gk20a_graphics_preempt_mode_name(u32 graphics_preempt_mode)
{
	switch (graphics_preempt_mode) {
	case NVGPU_PREEMPTION_MODE_GRAPHICS_WFI:
		return "WFI";
	default:
		return "?";
	}
}

static const char *gr_gk20a_compute_preempt_mode_name(u32 compute_preempt_mode)
{
	switch (compute_preempt_mode) {
	case NVGPU_PREEMPTION_MODE_COMPUTE_WFI:
		return "WFI";
	case NVGPU_PREEMPTION_MODE_COMPUTE_CTA:
		return "CTA";
	default:
		return "?";
	}
}
#endif

#ifdef CONFIG_NVGPU_TRACE
static void gk20a_channel_trace_sched_param(
	void (*trace)(int chid, int tsgid, pid_t pid, u32 timeslice,
		u32 timeout, const char *interleave,
		const char *graphics_preempt_mode,
		const char *compute_preempt_mode),
	struct nvgpu_channel *ch)
{
	struct nvgpu_tsg *tsg = nvgpu_tsg_from_ch(ch);

	if (!tsg)
		return;

	(trace)(ch->chid, ch->tsgid, ch->pid,
		nvgpu_tsg_from_ch(ch)->timeslice_us,
		ch->ctxsw_timeout_max_ms,
		nvgpu_runlist_interleave_level_name(tsg->interleave_level),
		gr_gk20a_graphics_preempt_mode_name(
			nvgpu_gr_ctx_get_graphics_preemption_mode(tsg->gr_ctx)),
		gr_gk20a_compute_preempt_mode_name(
			nvgpu_gr_ctx_get_compute_preemption_mode(tsg->gr_ctx)));
}
#endif

/*
 * Although channels do have pointers back to the gk20a struct that they were
 * created under in cases where the driver is killed that pointer can be bad.
 * The channel memory can be freed before the release() function for a given
 * channel is called. This happens when the driver dies and userspace doesn't
 * get a chance to call release() until after the entire gk20a driver data is
 * unloaded and freed.
 */
struct channel_priv {
	struct gk20a *g;
	struct nvgpu_channel *c;
	struct nvgpu_cdev *cdev;
};

#if defined(CONFIG_NVGPU_CYCLESTATS)

void gk20a_channel_free_cycle_stats_buffer(struct nvgpu_channel *ch)
{
	struct nvgpu_channel_linux *priv = ch->os_priv;

	/* disable existing cyclestats buffer */
	nvgpu_mutex_acquire(&ch->cyclestate.cyclestate_buffer_mutex);
	if (priv->cyclestate_buffer_handler) {
		gk20a_dmabuf_vunmap(priv->cyclestate_buffer_handler,
				ch->cyclestate.cyclestate_buffer);
		dma_buf_put(priv->cyclestate_buffer_handler);
		priv->cyclestate_buffer_handler = NULL;
		ch->cyclestate.cyclestate_buffer = NULL;
		ch->cyclestate.cyclestate_buffer_size = 0;
	}
	nvgpu_mutex_release(&ch->cyclestate.cyclestate_buffer_mutex);
}

int gk20a_channel_cycle_stats(struct nvgpu_channel *ch, int dmabuf_fd)
{
	struct dma_buf *dmabuf;
	void *virtual_address;
	struct nvgpu_channel_linux *priv = ch->os_priv;

	/* is it allowed to handle calls for current GPU? */
	if (!nvgpu_is_enabled(ch->g, NVGPU_SUPPORT_CYCLE_STATS))
		return -ENOSYS;

	if (dmabuf_fd && !priv->cyclestate_buffer_handler) {

		/* set up new cyclestats buffer */
		dmabuf = dma_buf_get(dmabuf_fd);
		if (IS_ERR(dmabuf))
			return PTR_ERR(dmabuf);

		virtual_address = gk20a_dmabuf_vmap(dmabuf);
		if (!virtual_address) {
			dma_buf_put(dmabuf);
			return -ENOMEM;
		}

		priv->cyclestate_buffer_handler = dmabuf;
		ch->cyclestate.cyclestate_buffer = virtual_address;
		ch->cyclestate.cyclestate_buffer_size = dmabuf->size;
		return 0;

	} else if (!dmabuf_fd && priv->cyclestate_buffer_handler) {
		gk20a_channel_free_cycle_stats_buffer(ch);
		return 0;

	} else if (!dmabuf_fd && !priv->cyclestate_buffer_handler) {
		/* no request from GL */
		return 0;

	} else {
		pr_err("channel already has cyclestats buffer\n");
		return -EINVAL;
	}
}

int gk20a_flush_cycle_stats_snapshot(struct nvgpu_channel *ch)
{
	int ret;

	nvgpu_mutex_acquire(&ch->cs_client_mutex);
	if (ch->cs_client)
		ret = nvgpu_css_flush(ch, ch->cs_client);
	else
		ret = -EBADF;
	nvgpu_mutex_release(&ch->cs_client_mutex);

	return ret;
}

int gk20a_attach_cycle_stats_snapshot(struct nvgpu_channel *ch,
				u32 dmabuf_fd,
				u32 perfmon_id_count,
				u32 *perfmon_id_start)
{
	int ret = 0;
	struct gk20a *g = ch->g;
	struct gk20a_cs_snapshot_client_linux *client_linux;
	struct gk20a_cs_snapshot_client *client;

	nvgpu_mutex_acquire(&ch->cs_client_mutex);
	if (ch->cs_client) {
		nvgpu_mutex_release(&ch->cs_client_mutex);
		return -EEXIST;
	}

	client_linux = nvgpu_kzalloc(g, sizeof(*client_linux));
	if (!client_linux) {
		ret = -ENOMEM;
		goto err;
	}

	client_linux->dmabuf_fd   = dmabuf_fd;
	client_linux->dma_handler = dma_buf_get(client_linux->dmabuf_fd);
	if (IS_ERR(client_linux->dma_handler)) {
		ret = PTR_ERR(client_linux->dma_handler);
		client_linux->dma_handler = NULL;
		goto err_free;
	}

	client = &client_linux->cs_client;
	client->snapshot_size = client_linux->dma_handler->size;
	if (client->snapshot_size < CSS_MIN_CLIENT_SNAPSHOT_SIZE) {
		ret = -ENOMEM;
		goto err_put;
	}

	client->snapshot = (struct gk20a_cs_snapshot_fifo *)
				gk20a_dmabuf_vmap(client_linux->dma_handler);
	if (!client->snapshot) {
		ret = -ENOMEM;
		goto err_put;
	}

	ch->cs_client = client;

	ret = nvgpu_css_attach(ch,
				perfmon_id_count,
				perfmon_id_start,
				ch->cs_client);

	nvgpu_mutex_release(&ch->cs_client_mutex);

	return ret;

err_put:
	dma_buf_put(client_linux->dma_handler);
err_free:
	nvgpu_kfree(g, client_linux);
err:
	nvgpu_mutex_release(&ch->cs_client_mutex);
	return ret;
}

int gk20a_channel_free_cycle_stats_snapshot(struct nvgpu_channel *ch)
{
	int ret;
	struct gk20a_cs_snapshot_client_linux *client_linux;

	nvgpu_mutex_acquire(&ch->cs_client_mutex);
	if (!ch->cs_client) {
		nvgpu_mutex_release(&ch->cs_client_mutex);
		return 0;
	}

	client_linux = container_of(ch->cs_client,
				struct gk20a_cs_snapshot_client_linux,
				cs_client);

	ret = nvgpu_css_detach(ch, ch->cs_client);

	if (client_linux->dma_handler) {
		if (ch->cs_client->snapshot) {
			gk20a_dmabuf_vunmap(client_linux->dma_handler,
					ch->cs_client->snapshot);
		}

		dma_buf_put(client_linux->dma_handler);
	}

	ch->cs_client = NULL;
	nvgpu_kfree(ch->g, client_linux);

	nvgpu_mutex_release(&ch->cs_client_mutex);

	return ret;
}
#endif

static int gk20a_channel_set_wdt_status(struct nvgpu_channel *ch,
		struct nvgpu_channel_wdt_args *args)
{
#ifdef CONFIG_NVGPU_CHANNEL_WDT
	u32 status = args->wdt_status & (NVGPU_IOCTL_CHANNEL_DISABLE_WDT |
			NVGPU_IOCTL_CHANNEL_ENABLE_WDT);
	bool set_timeout = (args->wdt_status &
			NVGPU_IOCTL_CHANNEL_WDT_FLAG_SET_TIMEOUT) != 0U;
	bool disable_dump = (args->wdt_status &
			NVGPU_IOCTL_CHANNEL_WDT_FLAG_DISABLE_DUMP) != 0U;

	if (ch->deterministic && status != NVGPU_IOCTL_CHANNEL_DISABLE_WDT) {
		/*
		 * Deterministic channels require disabled wdt before
		 * setup_bind gets called and wdt must not be changed after
		 * that point.
		 */
		return -EINVAL;
	}

	if (status == NVGPU_IOCTL_CHANNEL_DISABLE_WDT)
		nvgpu_channel_wdt_disable(ch->wdt);
	else if (status == NVGPU_IOCTL_CHANNEL_ENABLE_WDT)
		nvgpu_channel_wdt_enable(ch->wdt);
	else
		return -EINVAL;

	if (set_timeout)
		nvgpu_channel_wdt_set_limit(ch->wdt, args->timeout_ms);

	nvgpu_channel_set_wdt_debug_dump(ch, !disable_dump);

	return 0;
#else
	return -EINVAL;
#endif
}

static void gk20a_channel_free_error_notifiers(struct nvgpu_channel *ch)
{
	struct nvgpu_channel_linux *priv = ch->os_priv;

	nvgpu_mutex_acquire(&priv->error_notifier.mutex);
	if (priv->error_notifier.dmabuf) {
		gk20a_dmabuf_vunmap(priv->error_notifier.dmabuf,
				    priv->error_notifier.vaddr);
		dma_buf_put(priv->error_notifier.dmabuf);
		priv->error_notifier.dmabuf = NULL;
		priv->error_notifier.notification = NULL;
		priv->error_notifier.vaddr = NULL;
	}
	nvgpu_mutex_release(&priv->error_notifier.mutex);
}

static int gk20a_init_error_notifier(struct nvgpu_channel *ch,
		struct nvgpu_set_error_notifier *args)
{
	struct dma_buf *dmabuf;
	void *va;
	u64 end = args->offset + sizeof(struct nvgpu_notification);
	struct nvgpu_channel_linux *priv = ch->os_priv;

	if (!args->mem) {
		pr_err("gk20a_init_error_notifier: invalid memory handle\n");
		return -EINVAL;
	}

	dmabuf = dma_buf_get(args->mem);

	gk20a_channel_free_error_notifiers(ch);

	if (IS_ERR(dmabuf)) {
		pr_err("Invalid handle: %d\n", args->mem);
		return -EINVAL;
	}

	if (end > dmabuf->size || end < sizeof(struct nvgpu_notification)) {
		dma_buf_put(dmabuf);
		nvgpu_err(ch->g, "gk20a_init_error_notifier: invalid offset");
		return -EINVAL;
	}

	nvgpu_speculation_barrier();

	/* map handle */
	va = gk20a_dmabuf_vmap(dmabuf);
	if (!va) {
		dma_buf_put(dmabuf);
		pr_err("Cannot map notifier handle\n");
		return -ENOMEM;
	}

	priv->error_notifier.notification = va + args->offset;
	priv->error_notifier.vaddr = va;
	(void) memset(priv->error_notifier.notification, 0,
		sizeof(struct nvgpu_notification));

	/* set channel notifiers pointer */
	nvgpu_mutex_acquire(&priv->error_notifier.mutex);
	priv->error_notifier.dmabuf = dmabuf;
	nvgpu_mutex_release(&priv->error_notifier.mutex);

	return 0;
}

/*
 * This returns the channel with a reference. The caller must
 * nvgpu_channel_put() the ref back after use.
 *
 * NULL is returned if the channel was not found.
 */
struct nvgpu_channel *nvgpu_channel_get_from_file(int fd)
{
	struct nvgpu_channel *ch;
	struct channel_priv *priv;
	struct file *f = fget(fd);

	if (!f)
		return NULL;

	if (f->f_op != &gk20a_channel_ops) {
		fput(f);
		return NULL;
	}

	priv = (struct channel_priv *)f->private_data;
	ch = nvgpu_channel_get(priv->c);
	fput(f);
	return ch;
}

int gk20a_channel_release(struct inode *inode, struct file *filp)
{
	struct channel_priv *priv = filp->private_data;
	struct nvgpu_channel_linux *os_priv;
	struct nvgpu_channel *ch;
	struct gk20a *g;

	int err;

	/* We could still end up here even if the channel_open failed, e.g.
	 * if we ran out of hw channel IDs.
	 */
	if (!priv)
		return 0;

	ch = priv->c;
	g = priv->g;

	os_priv = ch->os_priv;
	os_priv->cdev = NULL;

	err = gk20a_busy(g);
	if (err) {
		nvgpu_err(g, "failed to release a channel!");
		goto channel_release;
	}

#ifdef CONFIG_NVGPU_TRACE
	trace_gk20a_channel_release(dev_name(dev_from_gk20a(g)));
#endif

	nvgpu_channel_close(ch);
	gk20a_channel_free_error_notifiers(ch);

	gk20a_idle(g);

channel_release:
	nvgpu_put(g);
	nvgpu_kfree(g, filp->private_data);
	filp->private_data = NULL;
	return 0;
}

/* note: runlist_id -1 is synonym for the NVGPU_ENGINE_GR runlist id */
static int __gk20a_channel_open(struct gk20a *g, struct nvgpu_cdev *cdev,
		struct file *filp, s32 runlist_id)
{
	int err;
	struct nvgpu_channel *ch;
	struct channel_priv *priv;
	struct nvgpu_channel_linux *os_priv;
	u32 tmp_runlist_id;
	u32 gpu_instance_id;

	nvgpu_log_fn(g, " ");

	g = nvgpu_get(g);
	if (!g)
		return -ENODEV;

	gpu_instance_id = nvgpu_get_gpu_instance_id_from_cdev(g, cdev);
	nvgpu_assert(gpu_instance_id < g->mig.num_gpu_instances);

	nvgpu_assert(runlist_id >= -1);
	if (runlist_id == -1) {
		tmp_runlist_id = nvgpu_grmgr_get_gpu_instance_runlist_id(g, gpu_instance_id);
	} else {
		if (nvgpu_grmgr_is_valid_runlist_id(g, gpu_instance_id, runlist_id)) {
			tmp_runlist_id = runlist_id;
		} else {
			return -EINVAL;
		}
	}

#ifdef CONFIG_NVGPU_TRACE
	trace_gk20a_channel_open(dev_name(dev_from_gk20a(g)));
#endif

	priv = nvgpu_kzalloc(g, sizeof(*priv));
	if (!priv) {
		err = -ENOMEM;
		goto free_ref;
	}

	err = gk20a_busy(g);
	if (err) {
		nvgpu_err(g, "failed to power on, %d", err);
		goto fail_busy;
	}
	/* All the user space channel should be non privilege */
	ch = nvgpu_channel_open_new(g, tmp_runlist_id, false,
				nvgpu_current_pid(g), nvgpu_current_tid(g));
	gk20a_idle(g);
	if (!ch) {
		nvgpu_err(g,
			"failed to get f");
		err = -ENOMEM;
		goto fail_busy;
	}

#ifdef CONFIG_NVGPU_TRACE
	gk20a_channel_trace_sched_param(
		trace_gk20a_channel_sched_defaults, ch);
#endif

	priv->g = g;
	priv->c = ch;
	priv->cdev = cdev;

	os_priv = ch->os_priv;
	os_priv->cdev = cdev;

	nvgpu_log(g, gpu_dbg_mig, "Use runlist %u for channel %u on GPU instance %u",
		tmp_runlist_id, ch->chid, gpu_instance_id);

	filp->private_data = priv;
	return 0;

fail_busy:
	nvgpu_kfree(g, priv);
free_ref:
	nvgpu_put(g);
	return err;
}

int gk20a_channel_open(struct inode *inode, struct file *filp)
{
	struct gk20a *g;
	int ret;
	struct nvgpu_cdev *cdev;

	cdev = container_of(inode->i_cdev, struct nvgpu_cdev, cdev);
	g = nvgpu_get_gk20a_from_cdev(cdev);

	nvgpu_log_fn(g, "start");
	ret = __gk20a_channel_open(g, cdev, filp, -1);

	nvgpu_log_fn(g, "end");
	return ret;
}

int gk20a_channel_open_ioctl(struct gk20a *g, struct nvgpu_cdev *cdev,
		struct nvgpu_channel_open_args *args)
{
	int err;
	int fd;
	struct file *file;
	char name[64];
	s32 runlist_id = args->in.runlist_id;

	err = get_unused_fd_flags(O_RDWR | O_CLOEXEC);
	if (err < 0)
		return err;
	fd = err;

	(void) snprintf(name, sizeof(name), "nvhost-%s-fd%d",
		 dev_name(dev_from_gk20a(g)), fd);

	file = anon_inode_getfile(name, &gk20a_channel_ops, NULL, O_RDWR);
	if (IS_ERR(file)) {
		err = PTR_ERR(file);
		goto clean_up;
	}

	err = __gk20a_channel_open(g, cdev, file, runlist_id);
	if (err)
		goto clean_up_file;

	fd_install(fd, file);
	args->out.channel_fd = fd;
	return 0;

clean_up_file:
	fput(file);
clean_up:
	put_unused_fd(fd);
	return err;
}

static u32 nvgpu_setup_bind_user_flags_to_common_flags(u32 user_flags)
{
	u32 flags = 0;

	if (user_flags & NVGPU_CHANNEL_SETUP_BIND_FLAGS_VPR_ENABLED)
		flags |= NVGPU_SETUP_BIND_FLAGS_SUPPORT_VPR;

	if (user_flags & NVGPU_CHANNEL_SETUP_BIND_FLAGS_DETERMINISTIC)
		flags |= NVGPU_SETUP_BIND_FLAGS_SUPPORT_DETERMINISTIC;

	if (user_flags & NVGPU_CHANNEL_SETUP_BIND_FLAGS_REPLAYABLE_FAULTS_ENABLE)
		flags |= NVGPU_SETUP_BIND_FLAGS_REPLAYABLE_FAULTS_ENABLE;

	if (user_flags & NVGPU_CHANNEL_SETUP_BIND_FLAGS_USERMODE_SUPPORT)
		flags |= NVGPU_SETUP_BIND_FLAGS_USERMODE_SUPPORT;

	return flags;
}

static void nvgpu_get_setup_bind_args(
		struct nvgpu_channel_setup_bind_args *channel_setup_bind_args,
		struct nvgpu_setup_bind_args *setup_bind_args)
{
	setup_bind_args->num_gpfifo_entries =
		channel_setup_bind_args->num_gpfifo_entries;
	setup_bind_args->num_inflight_jobs =
		channel_setup_bind_args->num_inflight_jobs;
	setup_bind_args->userd_dmabuf_fd =
		channel_setup_bind_args->userd_dmabuf_fd;
	setup_bind_args->userd_dmabuf_offset =
		channel_setup_bind_args->userd_dmabuf_offset;
	setup_bind_args->gpfifo_dmabuf_fd =
		channel_setup_bind_args->gpfifo_dmabuf_fd;
	setup_bind_args->gpfifo_dmabuf_offset =
		channel_setup_bind_args->gpfifo_dmabuf_offset;
	setup_bind_args->flags = nvgpu_setup_bind_user_flags_to_common_flags(
			channel_setup_bind_args->flags);
}

static void nvgpu_get_gpfifo_ex_args(
		struct nvgpu_alloc_gpfifo_ex_args *alloc_gpfifo_ex_args,
		struct nvgpu_setup_bind_args *setup_bind_args)
{
	setup_bind_args->num_gpfifo_entries = alloc_gpfifo_ex_args->num_entries;
	setup_bind_args->num_inflight_jobs =
		alloc_gpfifo_ex_args->num_inflight_jobs;
	setup_bind_args->flags = nvgpu_setup_bind_user_flags_to_common_flags(
			alloc_gpfifo_ex_args->flags);
}

static void nvgpu_get_fence_args(
		struct nvgpu_fence *fence_args_in,
		struct nvgpu_channel_fence *fence_args_out)
{
	fence_args_out->id = fence_args_in->id;
	fence_args_out->value = fence_args_in->value;
}

static bool channel_test_user_semaphore(struct dma_buf *dmabuf, void *data,
		u32 offset, u32 payload)
{
	u32 *semaphore;
	bool ret;
	int err;

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 19, 0)
	err = dma_buf_begin_cpu_access(dmabuf, offset, sizeof(u32), DMA_FROM_DEVICE);
#else
	err = dma_buf_begin_cpu_access(dmabuf, DMA_FROM_DEVICE);
#endif
	if (err != 0) {
		pr_err("nvgpu: sema begin cpu access failed\n");
		return false;
	}

	semaphore = (u32 *)((uintptr_t)data + offset);
	ret = *semaphore == payload;

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 19, 0)
	dma_buf_end_cpu_access(dmabuf, offset, sizeof(u32), DMA_FROM_DEVICE);
#else
	dma_buf_end_cpu_access(dmabuf, DMA_FROM_DEVICE);
#endif

	return ret;
}

static int gk20a_channel_wait_semaphore(struct nvgpu_channel *ch,
					ulong id, u32 offset,
					u32 payload, u32 timeout)
{
	struct dma_buf *dmabuf;
	void *data;
	int ret = 0;

	/* do not wait if channel has timed out */
	if (nvgpu_channel_check_unserviceable(ch)) {
		return -ETIMEDOUT;
	}

	if (!IS_ALIGNED(offset, 4)) {
		nvgpu_err(ch->g, "invalid semaphore offset %u", offset);
		return -EINVAL;
	}

	dmabuf = dma_buf_get(id);
	if (IS_ERR(dmabuf)) {
		nvgpu_err(ch->g, "invalid semaphore dma_buf handle 0x%lx", id);
		return -EINVAL;
	}

	if (offset > (dmabuf->size - sizeof(u32))) {
		nvgpu_err(ch->g, "invalid semaphore offset %u", offset);
		ret = -EINVAL;
		goto cleanup_put;
	}

	nvgpu_speculation_barrier();

	data = gk20a_dmabuf_vmap(dmabuf);
	if (!data) {
		nvgpu_err(ch->g, "failed to map semaphore memory");
		ret = -EINVAL;
		goto cleanup_put;
	}

	ret = NVGPU_COND_WAIT_INTERRUPTIBLE(
			&ch->semaphore_wq,
			channel_test_user_semaphore(dmabuf, data, offset, payload) ||
				nvgpu_channel_check_unserviceable(ch),
			timeout);

	gk20a_dmabuf_vunmap(dmabuf, data);
cleanup_put:
	dma_buf_put(dmabuf);
	return ret;
}

static int gk20a_channel_wait(struct nvgpu_channel *ch,
			      struct nvgpu_wait_args *args)
{
	struct dma_buf *dmabuf;
	struct gk20a *g = ch->g;
	struct notification *notif;
	struct timespec64 tv;
	u64 jiffies;
	ulong id;
	u32 offset;
	int remain, ret = 0;
	u64 end;

	nvgpu_log_fn(g, " ");

	if (nvgpu_channel_check_unserviceable(ch)) {
		return -ETIMEDOUT;
	}

	switch (args->type) {
	case NVGPU_WAIT_TYPE_NOTIFIER:
		id = args->condition.notifier.dmabuf_fd;
		offset = args->condition.notifier.offset;
		end = offset + sizeof(struct notification);

		dmabuf = dma_buf_get(id);
		if (IS_ERR(dmabuf)) {
			nvgpu_err(g, "invalid notifier dma_buf handle 0x%lx",
				   id);
			return -EINVAL;
		}

		if (end > dmabuf->size || end < sizeof(struct notification)) {
			dma_buf_put(dmabuf);
			nvgpu_err(g, "invalid notifier offset");
			return -EINVAL;
		}

		nvgpu_speculation_barrier();

		notif = gk20a_dmabuf_vmap(dmabuf);
		if (!notif) {
			nvgpu_err(g, "failed to map notifier memory");
			return -ENOMEM;
		}
		notif = (struct notification *)((uintptr_t)notif + offset);

		/* user should set status pending before
		 * calling this ioctl */
		remain = NVGPU_COND_WAIT_INTERRUPTIBLE(
				&ch->notifier_wq,
				notif->status == 0 ||
				nvgpu_channel_check_unserviceable(ch),
				args->timeout);

		if (remain == 0 && notif->status != 0) {
			ret = -ETIMEDOUT;
			goto notif_clean_up;
		} else if (remain < 0) {
			ret = -EINTR;
			goto notif_clean_up;
		}

		/* TBD: fill in correct information */
		jiffies = get_jiffies_64();
		jiffies_to_timespec64(jiffies, &tv);
		notif->timestamp.nanoseconds[0] = tv.tv_nsec;
		notif->timestamp.nanoseconds[1] = tv.tv_sec;
		notif->info32 = 0xDEADBEEF; /* should be object name */
		notif->info16 = ch->chid; /* should be method offset */

notif_clean_up:
		gk20a_dmabuf_vunmap(dmabuf, notif);
		return ret;

	case NVGPU_WAIT_TYPE_SEMAPHORE:
		ret = gk20a_channel_wait_semaphore(ch,
				args->condition.semaphore.dmabuf_fd,
				args->condition.semaphore.offset,
				args->condition.semaphore.payload,
				args->timeout);

		break;

	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

#ifdef CONFIG_NVGPU_GRAPHICS
static int gk20a_channel_zcull_bind(struct nvgpu_channel *ch,
			    struct nvgpu_zcull_bind_args *args)
{
	struct gk20a *g = ch->g;

	nvgpu_log_fn(g, " ");

	return g->ops.gr.setup.bind_ctxsw_zcull(g, ch,
				args->gpu_va, args->mode);
}
#endif

static int gk20a_ioctl_channel_submit_gpfifo(
	struct nvgpu_channel *ch,
	struct nvgpu_submit_gpfifo_args *args)
{
	struct nvgpu_channel_fence fence;
	struct nvgpu_user_fence fence_out = nvgpu_user_fence_init();
	u32 submit_flags = 0;
	int fd = -1;
	struct gk20a *g = ch->g;
	struct nvgpu_fifo *f = &g->fifo;
	struct nvgpu_swprofiler *kickoff_profiler = &f->kickoff_profiler;
	struct nvgpu_gpfifo_userdata userdata = { NULL, NULL };
	bool flag_fence_wait = (args->flags &
			NVGPU_SUBMIT_GPFIFO_FLAGS_FENCE_WAIT) != 0U;
	bool flag_fence_get = (args->flags &
			NVGPU_SUBMIT_GPFIFO_FLAGS_FENCE_GET) != 0U;
	bool flag_sync_fence = (args->flags &
			NVGPU_SUBMIT_GPFIFO_FLAGS_SYNC_FENCE) != 0U;

	int ret = 0;
	nvgpu_log_fn(g, " ");

	nvgpu_swprofile_begin_sample(kickoff_profiler);
	nvgpu_swprofile_snapshot(kickoff_profiler, PROF_KICKOFF_IOCTL_ENTRY);

	if (nvgpu_channel_check_unserviceable(ch)) {
		return -ETIMEDOUT;
	}

#ifdef CONFIG_NVGPU_SYNCFD_NONE
	if (flag_sync_fence) {
		return -EINVAL;
	}
#endif

	/*
	 * In case we need the sync framework, require that the user requests
	 * it too for any fences. That's advertised in the gpu characteristics.
	 */
	if (nvgpu_channel_sync_needs_os_fence_framework(g) &&
		(flag_fence_wait || flag_fence_get) && !flag_sync_fence) {
		return -EINVAL;
	}

	/* Try and allocate an fd here*/
	if (flag_fence_get && flag_sync_fence) {
		fd = get_unused_fd_flags(O_RDWR | O_CLOEXEC);
		if (fd < 0)
			return fd;
	}

	userdata.entries = (struct nvgpu_gpfifo_entry __user *)
		(uintptr_t)args->gpfifo;
	userdata.context = NULL;

	nvgpu_get_fence_args(&args->fence, &fence);
	submit_flags =
		nvgpu_submit_gpfifo_user_flags_to_common_flags(args->flags);

	ret = nvgpu_submit_channel_gpfifo_user(ch,
			userdata, args->num_entries,
			submit_flags, &fence, &fence_out, kickoff_profiler);

	if (ret) {
		if (fd != -1)
			put_unused_fd(fd);
		goto clean_up;
	}

	/* Convert fence_out to something we can pass back to user space. */
	if (flag_fence_get) {
		if (flag_sync_fence) {
			ret = fence_out.os_fence.ops->install_fence(
					&fence_out.os_fence, fd);
			if (ret)
				put_unused_fd(fd);
			else
				args->fence.id = fd;
		} else {
			args->fence.id = fence_out.syncpt_id;
			args->fence.value = fence_out.syncpt_value;
		}
		nvgpu_user_fence_release(&fence_out);
	}

	nvgpu_swprofile_snapshot(kickoff_profiler, PROF_KICKOFF_IOCTL_EXIT);

clean_up:
	return ret;
}

/*
 * Convert linux specific runlist level of the form NVGPU_RUNLIST_INTERLEAVE_LEVEL_*
 * to common runlist level of the form NVGPU_FIFO_RUNLIST_INTERLEAVE_LEVEL_*
 */
u32 nvgpu_get_common_runlist_level(u32 level)
{
	nvgpu_speculation_barrier();
	switch (level) {
	case NVGPU_RUNLIST_INTERLEAVE_LEVEL_LOW:
		return NVGPU_FIFO_RUNLIST_INTERLEAVE_LEVEL_LOW;
	case NVGPU_RUNLIST_INTERLEAVE_LEVEL_MEDIUM:
		return NVGPU_FIFO_RUNLIST_INTERLEAVE_LEVEL_MEDIUM;
	case NVGPU_RUNLIST_INTERLEAVE_LEVEL_HIGH:
		return NVGPU_FIFO_RUNLIST_INTERLEAVE_LEVEL_HIGH;
	default:
		pr_err("%s: incorrect runlist level\n", __func__);
	}

	return level;
}

static u32 nvgpu_obj_ctx_user_flags_to_common_flags(u32 user_flags)
{
	u32 flags = 0;

	if (user_flags & NVGPU_ALLOC_OBJ_FLAGS_GFXP)
		flags |= NVGPU_OBJ_CTX_FLAGS_SUPPORT_GFXP;

	if (user_flags & NVGPU_ALLOC_OBJ_FLAGS_CILP)
		flags |= NVGPU_OBJ_CTX_FLAGS_SUPPORT_CILP;

	return flags;
}

static int nvgpu_ioctl_channel_alloc_obj_ctx(struct nvgpu_channel *ch,
	u32 class_num, u32 user_flags)
{
	return ch->g->ops.gr.setup.alloc_obj_ctx(ch, class_num,
			nvgpu_obj_ctx_user_flags_to_common_flags(user_flags));
}

/*
 * Convert common preemption mode flags of the form NVGPU_PREEMPTION_MODE_GRAPHICS_*
 * into linux preemption mode flags of the form NVGPU_GRAPHICS_PREEMPTION_MODE_*
 */
u32 nvgpu_get_ioctl_graphics_preempt_mode_flags(u32 graphics_preempt_mode_flags)
{
	u32 flags = 0;

	if (graphics_preempt_mode_flags & NVGPU_PREEMPTION_MODE_GRAPHICS_WFI)
		flags |= NVGPU_GRAPHICS_PREEMPTION_MODE_WFI;
	if (graphics_preempt_mode_flags & NVGPU_PREEMPTION_MODE_GRAPHICS_GFXP)
		flags |= NVGPU_GRAPHICS_PREEMPTION_MODE_GFXP;

	return flags;
}

/*
 * Convert common preemption mode flags of the form NVGPU_PREEMPTION_MODE_COMPUTE_*
 * into linux preemption mode flags of the form NVGPU_COMPUTE_PREEMPTION_MODE_*
 */
u32 nvgpu_get_ioctl_compute_preempt_mode_flags(u32 compute_preempt_mode_flags)
{
	u32 flags = 0;

	if (compute_preempt_mode_flags & NVGPU_PREEMPTION_MODE_COMPUTE_WFI)
		flags |= NVGPU_COMPUTE_PREEMPTION_MODE_WFI;
	if (compute_preempt_mode_flags & NVGPU_PREEMPTION_MODE_COMPUTE_CTA)
		flags |= NVGPU_COMPUTE_PREEMPTION_MODE_CTA;
	if (compute_preempt_mode_flags & NVGPU_PREEMPTION_MODE_COMPUTE_CILP)
		flags |= NVGPU_COMPUTE_PREEMPTION_MODE_CILP;

	return flags;
}

/*
 * Convert common preemption modes of the form NVGPU_PREEMPTION_MODE_GRAPHICS_*
 * into linux preemption modes of the form NVGPU_GRAPHICS_PREEMPTION_MODE_*
 */
u32 nvgpu_get_ioctl_graphics_preempt_mode(u32 graphics_preempt_mode)
{
	switch (graphics_preempt_mode) {
	case NVGPU_PREEMPTION_MODE_GRAPHICS_WFI:
		return NVGPU_GRAPHICS_PREEMPTION_MODE_WFI;
	case NVGPU_PREEMPTION_MODE_GRAPHICS_GFXP:
		return NVGPU_GRAPHICS_PREEMPTION_MODE_GFXP;
	}

	return graphics_preempt_mode;
}

/*
 * Convert common preemption modes of the form NVGPU_PREEMPTION_MODE_COMPUTE_*
 * into linux preemption modes of the form NVGPU_COMPUTE_PREEMPTION_MODE_*
 */
u32 nvgpu_get_ioctl_compute_preempt_mode(u32 compute_preempt_mode)
{
	switch (compute_preempt_mode) {
	case NVGPU_PREEMPTION_MODE_COMPUTE_WFI:
		return NVGPU_COMPUTE_PREEMPTION_MODE_WFI;
	case NVGPU_PREEMPTION_MODE_COMPUTE_CTA:
		return NVGPU_COMPUTE_PREEMPTION_MODE_CTA;
	case NVGPU_PREEMPTION_MODE_COMPUTE_CILP:
		return NVGPU_COMPUTE_PREEMPTION_MODE_CILP;
	}

	return compute_preempt_mode;
}

/*
 * Convert linux preemption modes of the form NVGPU_GRAPHICS_PREEMPTION_MODE_*
 * into common preemption modes of the form NVGPU_PREEMPTION_MODE_GRAPHICS_*
 */
static u32 nvgpu_get_common_graphics_preempt_mode(u32 graphics_preempt_mode)
{
	nvgpu_speculation_barrier();
	switch (graphics_preempt_mode) {
	case NVGPU_GRAPHICS_PREEMPTION_MODE_WFI:
		return NVGPU_PREEMPTION_MODE_GRAPHICS_WFI;
	case NVGPU_GRAPHICS_PREEMPTION_MODE_GFXP:
		return NVGPU_PREEMPTION_MODE_GRAPHICS_GFXP;
	}

	return graphics_preempt_mode;
}

/*
 * Convert linux preemption modes of the form NVGPU_COMPUTE_PREEMPTION_MODE_*
 * into common preemption modes of the form NVGPU_PREEMPTION_MODE_COMPUTE_*
 */
static u32 nvgpu_get_common_compute_preempt_mode(u32 compute_preempt_mode)
{
	nvgpu_speculation_barrier();
	switch (compute_preempt_mode) {
	case NVGPU_COMPUTE_PREEMPTION_MODE_WFI:
		return NVGPU_PREEMPTION_MODE_COMPUTE_WFI;
	case NVGPU_COMPUTE_PREEMPTION_MODE_CTA:
		return NVGPU_PREEMPTION_MODE_COMPUTE_CTA;
	case NVGPU_COMPUTE_PREEMPTION_MODE_CILP:
		return NVGPU_PREEMPTION_MODE_COMPUTE_CILP;
	}

	return compute_preempt_mode;
}

static int nvgpu_ioctl_channel_set_preemption_mode(struct nvgpu_channel *ch,
		u32 graphics_preempt_mode, u32 compute_preempt_mode,
		u32 gr_instance_id)
{
	int err;

	if (ch->g->ops.gr.setup.set_preemption_mode) {
		err = gk20a_busy(ch->g);
		if (err) {
			nvgpu_err(ch->g, "failed to power on, %d", err);
			return err;
		}
		err = ch->g->ops.gr.setup.set_preemption_mode(ch,
			nvgpu_get_common_graphics_preempt_mode(graphics_preempt_mode),
			nvgpu_get_common_compute_preempt_mode(compute_preempt_mode),
			gr_instance_id);
		gk20a_idle(ch->g);
	} else {
		err = -EINVAL;
	}

	return err;
}

static int nvgpu_ioctl_channel_get_user_syncpoint(struct nvgpu_channel *ch,
	struct nvgpu_get_user_syncpoint_args *args)
{
#ifdef CONFIG_TEGRA_GK20A_NVHOST
	struct gk20a *g = ch->g;
	int err;

	if (!nvgpu_is_enabled(g, NVGPU_SUPPORT_USER_SYNCPOINT)) {
		nvgpu_err(g, "user syncpoints not supported");
		return -EINVAL;
	}

	if (!nvgpu_has_syncpoints(g)) {
		nvgpu_err(g, "syncpoints not supported");
		return -EINVAL;
	}

	if (g->aggressive_sync_destroy_thresh) {
		nvgpu_err(g, "sufficient syncpoints not available");
		return -EINVAL;
	}

	nvgpu_mutex_acquire(&ch->sync_lock);
	if (ch->user_sync) {
		nvgpu_mutex_release(&ch->sync_lock);
	} else {
		ch->user_sync = nvgpu_channel_user_syncpt_create(ch);
		if (!ch->user_sync) {
			nvgpu_mutex_release(&ch->sync_lock);
			return -ENOMEM;
		}
		nvgpu_mutex_release(&ch->sync_lock);
	}

	args->syncpoint_id = nvgpu_channel_user_syncpt_get_id(ch->user_sync);

	/* The current value is the max we're expecting at the moment */
	err = nvgpu_nvhost_syncpt_read_ext_check(g->nvhost, args->syncpoint_id,
			&args->syncpoint_max);
	if (err != 0) {
		nvgpu_mutex_acquire(&ch->sync_lock);
		nvgpu_channel_user_syncpt_destroy(ch->user_sync);
		nvgpu_mutex_release(&ch->sync_lock);
		return err;
	}

	if (nvgpu_is_enabled(g, NVGPU_SUPPORT_SYNCPOINT_ADDRESS)) {
		args->gpu_va =
			nvgpu_channel_user_syncpt_get_address(ch->user_sync);
	} else {
		args->gpu_va = 0;
	}

	return 0;
#else
	return -EINVAL;
#endif
}

long gk20a_channel_ioctl(struct file *filp,
	unsigned int cmd, unsigned long arg)
{
	struct channel_priv *priv = filp->private_data;
	struct nvgpu_channel *ch = priv->c;
	struct device *dev = dev_from_gk20a(ch->g);
	u8 buf[NVGPU_IOCTL_CHANNEL_MAX_ARG_SIZE] = {0};
	int err = 0;
	struct gk20a *g = ch->g;
	u32 gpu_instance_id, gr_instance_id;

	nvgpu_log_fn(g, "start %d", _IOC_NR(cmd));

	if ((_IOC_TYPE(cmd) != NVGPU_IOCTL_MAGIC) ||
		(_IOC_NR(cmd) == 0) ||
		(_IOC_NR(cmd) > NVGPU_IOCTL_CHANNEL_LAST) ||
		(_IOC_SIZE(cmd) > NVGPU_IOCTL_CHANNEL_MAX_ARG_SIZE))
		return -EINVAL;

	if (_IOC_DIR(cmd) & _IOC_WRITE) {
		if (copy_from_user(buf, (void __user *)arg, _IOC_SIZE(cmd)))
			return -EFAULT;
	}

	/* take a ref or return timeout if channel refs can't be taken */
	ch = nvgpu_channel_get(ch);
	if (!ch)
		return -ETIMEDOUT;

	gpu_instance_id = nvgpu_get_gpu_instance_id_from_cdev(g, priv->cdev);
	nvgpu_assert(gpu_instance_id < g->mig.num_gpu_instances);

	gr_instance_id = nvgpu_grmgr_get_gr_instance_id(g, gpu_instance_id);
	nvgpu_assert(gr_instance_id < g->num_gr_instances);

	/* protect our sanity for threaded userspace - most of the channel is
	 * not thread safe */
	nvgpu_mutex_acquire(&ch->ioctl_lock);

	/* this ioctl call keeps a ref to the file which keeps a ref to the
	 * channel */

	nvgpu_speculation_barrier();
	switch (cmd) {
	case NVGPU_IOCTL_CHANNEL_OPEN:
		err = gk20a_channel_open_ioctl(ch->g, priv->cdev,
			(struct nvgpu_channel_open_args *)buf);
		break;
	case NVGPU_IOCTL_CHANNEL_SET_NVMAP_FD:
		break;
	case NVGPU_IOCTL_CHANNEL_ALLOC_OBJ_CTX:
	{
		struct nvgpu_alloc_obj_ctx_args *args =
				(struct nvgpu_alloc_obj_ctx_args *)buf;

		err = gk20a_busy(ch->g);
		if (err) {
			dev_err(dev,
				"%s: failed to host gk20a for ioctl cmd: 0x%x",
				__func__, cmd);
			break;
		}

#ifdef CONFIG_NVGPU_SM_DIVERSITY
	{
		struct nvgpu_tsg *tsg = nvgpu_tsg_from_ch(ch);

		if (tsg == NULL) {
			err = -EINVAL;
			break;
		}

		if (nvgpu_gr_ctx_get_sm_diversity_config(tsg->gr_ctx) ==
			NVGPU_INVALID_SM_CONFIG_ID) {
			nvgpu_gr_ctx_set_sm_diversity_config(tsg->gr_ctx,
				NVGPU_DEFAULT_SM_DIVERSITY_CONFIG);
		}
	}
#endif

		err = nvgpu_gr_exec_with_err_for_instance(g, gr_instance_id,
				nvgpu_ioctl_channel_alloc_obj_ctx(ch, args->class_num,
					args->flags));
		gk20a_idle(ch->g);
		break;
	}
	case NVGPU_IOCTL_CHANNEL_SETUP_BIND:
	{
		struct nvgpu_channel_setup_bind_args *channel_setup_bind_args =
			(struct nvgpu_channel_setup_bind_args *)buf;
		struct nvgpu_setup_bind_args setup_bind_args;

		nvgpu_get_setup_bind_args(channel_setup_bind_args,
				&setup_bind_args);

		err = gk20a_busy(ch->g);
		if (err) {
			dev_err(dev,
				"%s: failed to host gk20a for ioctl cmd: 0x%x",
				__func__, cmd);
			break;
		}

		/*
		 * This restriction is because the last entry is kept empty and used to
		 * determine buffer empty or full condition. Additionally, kmd submit
		 * uses pre/post sync which need another entry.
		 */
		if ((setup_bind_args.flags &
			NVGPU_CHANNEL_SETUP_BIND_FLAGS_USERMODE_SUPPORT) != 0U) {
			if (setup_bind_args.num_gpfifo_entries < 2U) {
				err = -EINVAL;
				gk20a_idle(ch->g);
				break;
			}
		} else {
			if (setup_bind_args.num_gpfifo_entries < 4U) {
				err = -EINVAL;
				gk20a_idle(ch->g);
				break;
			}
		}

		if (!is_power_of_2(setup_bind_args.num_gpfifo_entries)) {
			err = -EINVAL;
			gk20a_idle(ch->g);
			break;
		}

		/*
		 * setup_bind_args.num_gpfifo_entries * nvgpu_get_gpfifo_entry_size() has
		 * to fit in u32.
		 */
		if (setup_bind_args.num_gpfifo_entries >
		   (U32_MAX / nvgpu_get_gpfifo_entry_size())) {
			err = -EINVAL;
			gk20a_idle(ch->g);
			break;
		}

		err = nvgpu_channel_setup_bind(ch, &setup_bind_args);
		channel_setup_bind_args->work_submit_token =
			setup_bind_args.work_submit_token;
		gk20a_idle(ch->g);
		break;
	}
	case NVGPU_IOCTL_CHANNEL_ALLOC_GPFIFO_EX:
	{
		struct nvgpu_alloc_gpfifo_ex_args *alloc_gpfifo_ex_args =
			(struct nvgpu_alloc_gpfifo_ex_args *)buf;
		struct nvgpu_setup_bind_args setup_bind_args;

		nvgpu_get_gpfifo_ex_args(alloc_gpfifo_ex_args, &setup_bind_args);

		err = gk20a_busy(ch->g);
		if (err) {
			dev_err(dev,
				"%s: failed to host gk20a for ioctl cmd: 0x%x",
				__func__, cmd);
			break;
		}

		/*
		 * This restriction is because the last entry is kept empty and used to
		 * determine buffer empty or full condition. Additionally, kmd submit
		 * uses pre/post sync which need another entry.
		 */
		if ((alloc_gpfifo_ex_args->flags &
			NVGPU_CHANNEL_SETUP_BIND_FLAGS_USERMODE_SUPPORT) != 0U) {
			if (alloc_gpfifo_ex_args->num_entries < 2U) {
				err = -EINVAL;
				gk20a_idle(ch->g);
				break;
			}
		} else {
			if (alloc_gpfifo_ex_args->num_entries < 4U) {
				err = -EINVAL;
				gk20a_idle(ch->g);
				break;
			}
		}


		if (!is_power_of_2(alloc_gpfifo_ex_args->num_entries)) {
			err = -EINVAL;
			gk20a_idle(ch->g);
			break;
		}

		/*
		 * alloc_gpfifo_ex_args->num_entries * nvgpu_get_gpfifo_entry_size() has
		 * to fit in u32.
		 */
		if (alloc_gpfifo_ex_args->num_entries >
		   (U32_MAX / nvgpu_get_gpfifo_entry_size())) {
			err = -EINVAL;
			gk20a_idle(ch->g);
			break;
		}

		err = nvgpu_channel_setup_bind(ch, &setup_bind_args);
		gk20a_idle(ch->g);
		break;
	}
	case NVGPU_IOCTL_CHANNEL_SUBMIT_GPFIFO:
		err = gk20a_ioctl_channel_submit_gpfifo(ch,
				(struct nvgpu_submit_gpfifo_args *)buf);
		break;
	case NVGPU_IOCTL_CHANNEL_WAIT:
		err = gk20a_busy(ch->g);
		if (err) {
			dev_err(dev,
				"%s: failed to host gk20a for ioctl cmd: 0x%x",
				__func__, cmd);
			break;
		}

		/* waiting is thread-safe, not dropping this mutex could
		 * deadlock in certain conditions */
		nvgpu_mutex_release(&ch->ioctl_lock);

		err = gk20a_channel_wait(ch,
				(struct nvgpu_wait_args *)buf);

		nvgpu_mutex_acquire(&ch->ioctl_lock);

		gk20a_idle(ch->g);
		break;
#ifdef CONFIG_NVGPU_GRAPHICS
	case NVGPU_IOCTL_CHANNEL_ZCULL_BIND:
		err = gk20a_busy(ch->g);
		if (err) {
			dev_err(dev,
				"%s: failed to host gk20a for ioctl cmd: 0x%x",
				__func__, cmd);
			break;
		}
		err = gk20a_channel_zcull_bind(ch,
				(struct nvgpu_zcull_bind_args *)buf);
		gk20a_idle(ch->g);
		break;
#endif
	case NVGPU_IOCTL_CHANNEL_SET_ERROR_NOTIFIER:
		err = gk20a_busy(ch->g);
		if (err) {
			dev_err(dev,
				"%s: failed to host gk20a for ioctl cmd: 0x%x",
				__func__, cmd);
			break;
		}
		err = gk20a_init_error_notifier(ch,
				(struct nvgpu_set_error_notifier *)buf);
		gk20a_idle(ch->g);
		break;
	case NVGPU_IOCTL_CHANNEL_SET_TIMEOUT:
	{
		u32 timeout =
			(u32)((struct nvgpu_set_timeout_args *)buf)->timeout;
		nvgpu_log(g, gpu_dbg_gpu_dbg, "setting timeout (%d ms) for chid %d",
			   timeout, ch->chid);
		ch->ctxsw_timeout_max_ms = timeout;
#ifdef CONFIG_NVGPU_TRACE
		gk20a_channel_trace_sched_param(
			trace_gk20a_channel_set_timeout, ch);
#endif
		break;
	}
	case NVGPU_IOCTL_CHANNEL_SET_TIMEOUT_EX:
	{
		u32 timeout =
			(u32)((struct nvgpu_set_timeout_args *)buf)->timeout;
		bool ctxsw_timeout_debug_dump = !((u32)
			((struct nvgpu_set_timeout_ex_args *)buf)->flags &
			(1 << NVGPU_TIMEOUT_FLAG_DISABLE_DUMP));
		nvgpu_log(g, gpu_dbg_gpu_dbg, "setting timeout (%d ms) for chid %d",
			   timeout, ch->chid);
		ch->ctxsw_timeout_max_ms = timeout;
		ch->ctxsw_timeout_debug_dump = ctxsw_timeout_debug_dump;
#ifdef CONFIG_NVGPU_TRACE
		gk20a_channel_trace_sched_param(
			trace_gk20a_channel_set_timeout, ch);
#endif
		break;
	}
	case NVGPU_IOCTL_CHANNEL_GET_TIMEDOUT:
		((struct nvgpu_get_param_args *)buf)->value =
			nvgpu_channel_check_unserviceable(ch);
		break;
	case NVGPU_IOCTL_CHANNEL_ENABLE:
		err = gk20a_busy(ch->g);
		if (err) {
			dev_err(dev,
				"%s: failed to host gk20a for ioctl cmd: 0x%x",
				__func__, cmd);
			break;
		}
		if (ch->g->ops.channel.enable)
			ch->g->ops.channel.enable(ch);
		else
			err = -ENOSYS;
		gk20a_idle(ch->g);
		break;
	case NVGPU_IOCTL_CHANNEL_DISABLE:
		err = gk20a_busy(ch->g);
		if (err) {
			dev_err(dev,
				"%s: failed to host gk20a for ioctl cmd: 0x%x",
				__func__, cmd);
			break;
		}
		if (ch->g->ops.channel.disable)
			ch->g->ops.channel.disable(ch);
		else
			err = -ENOSYS;
		gk20a_idle(ch->g);
		break;
	case NVGPU_IOCTL_CHANNEL_PREEMPT:
		err = gk20a_busy(ch->g);
		if (err) {
			dev_err(dev,
				"%s: failed to host gk20a for ioctl cmd: 0x%x",
				__func__, cmd);
			break;
		}
		err = nvgpu_preempt_channel(ch->g, ch);
		gk20a_idle(ch->g);
		break;
	case NVGPU_IOCTL_CHANNEL_RESCHEDULE_RUNLIST:
		if (!capable(CAP_SYS_NICE)) {
			err = -EPERM;
			break;
		}
		if (!ch->g->ops.runlist.reschedule) {
			err = -ENOSYS;
			break;
		}
		err = gk20a_busy(ch->g);
		if (err) {
			dev_err(dev,
				"%s: failed to host gk20a for ioctl cmd: 0x%x",
				__func__, cmd);
			break;
		}
		err = ch->g->ops.runlist.reschedule(ch,
			NVGPU_RESCHEDULE_RUNLIST_PREEMPT_NEXT &
			((struct nvgpu_reschedule_runlist_args *)buf)->flags);
		gk20a_idle(ch->g);
		break;
	case NVGPU_IOCTL_CHANNEL_FORCE_RESET:
		err = gk20a_busy(ch->g);
		if (err) {
			dev_err(dev,
				"%s: failed to host gk20a for ioctl cmd: 0x%x",
				__func__, cmd);
			break;
		}
		err = ch->g->ops.tsg.force_reset(ch,
				NVGPU_ERR_NOTIFIER_RESETCHANNEL_VERIF_ERROR, true);
		gk20a_idle(ch->g);
		break;
	case NVGPU_IOCTL_CHANNEL_WDT:
		err = gk20a_channel_set_wdt_status(ch,
				(struct nvgpu_channel_wdt_args *)buf);
		break;
	case NVGPU_IOCTL_CHANNEL_SET_PREEMPTION_MODE:
		err = nvgpu_ioctl_channel_set_preemption_mode(ch,
		     ((struct nvgpu_preemption_mode_args *)buf)->graphics_preempt_mode,
		     ((struct nvgpu_preemption_mode_args *)buf)->compute_preempt_mode,
			gr_instance_id);
		break;
	case NVGPU_IOCTL_CHANNEL_SET_BOOSTED_CTX:
		if (ch->g->ops.gr.set_boosted_ctx) {
			bool boost =
				((struct nvgpu_boosted_ctx_args *)buf)->boost;

			err = gk20a_busy(ch->g);
			if (err) {
				dev_err(dev,
					"%s: failed to host gk20a for ioctl cmd: 0x%x",
					__func__, cmd);
				break;
			}
			err = ch->g->ops.gr.set_boosted_ctx(ch, boost);
			gk20a_idle(ch->g);
		} else {
			err = -EINVAL;
		}
		break;
	case NVGPU_IOCTL_CHANNEL_GET_USER_SYNCPOINT:
		err = gk20a_busy(ch->g);
		if (err) {
			dev_err(dev,
				"%s: failed to host gk20a for ioctl cmd: 0x%x",
				__func__, cmd);
			break;
		}
		err = nvgpu_ioctl_channel_get_user_syncpoint(ch,
		      (struct nvgpu_get_user_syncpoint_args *)buf);
		gk20a_idle(ch->g);
		break;
	default:
		dev_dbg(dev, "unrecognized ioctl cmd: 0x%x", cmd);
		err = -ENOTTY;
		break;
	}

	if ((err == 0) && (_IOC_DIR(cmd) & _IOC_READ))
		err = copy_to_user((void __user *)arg, buf, _IOC_SIZE(cmd));

	nvgpu_mutex_release(&ch->ioctl_lock);

	nvgpu_channel_put(ch);

	nvgpu_log_fn(g, "end");

	return err;
}
