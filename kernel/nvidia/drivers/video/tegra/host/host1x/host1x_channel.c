/*
 * drivers/video/tegra/host/host1x/channel_host1x.c
 *
 * Tegra Graphics Host Channel
 *
 * Copyright (c) 2010-2020, NVIDIA CORPORATION.  All rights reserved.
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

#include <linux/slab.h>
#include <linux/version.h>

#include <trace/events/nvhost.h>

#include "nvhost_channel.h"
#include "nvhost_acm.h"
#include "nvhost_job.h"
#include "nvhost_sync.h"
#include "nvhost_intr.h"
#include "class_ids.h"
#include "debug.h"
#include "dev.h"

#define NVHOST_CHANNEL_LOW_PRIO_MAX_WAIT 50

static void submit_work_done_increment(struct nvhost_job *job)
{
	struct nvhost_channel *ch = job->ch;
	struct nvhost_syncpt *sp = &nvhost_get_host(ch->dev)->syncpt;
	struct nvhost_device_data *pdata = platform_get_drvdata(ch->dev);

	if (!pdata->push_work_done)
		return;

	/* make the last increment at job boundary. this will ensure
	 * that the user command buffer is no longer in use */
	job->sp[0].fence = nvhost_syncpt_incr_max(sp, job->sp[0].id, 1);
	nvhost_cdma_push(&ch->cdma, nvhost_opcode_imm_incr_syncpt(0,
			job->sp[0].id), NVHOST_OPCODE_NOOP);
}

static void lock_device(struct nvhost_job *job, bool lock)
{
	struct nvhost_channel *ch = job->ch;
	struct nvhost_device_data *pdata = platform_get_drvdata(ch->dev);
	u32 opcode = lock ?
		nvhost_opcode_acquire_mlock(pdata->modulemutexes[0]) :
		nvhost_opcode_release_mlock(pdata->modulemutexes[0]);

	/* No need to do anything if we have a channel/engine */
	if (pdata->resource_policy == RESOURCE_PER_DEVICE)
		return;

	/* If we have a hardware mlock, use it. */
	if (pdata->modulemutexes[0]) {
		nvhost_cdma_push(&ch->cdma, opcode, NVHOST_OPCODE_NOOP);
		return;
	}

	if (lock) {
		nvhost_cdma_push(&ch->cdma,
			nvhost_opcode_setclass(NV_HOST1X_CLASS_ID,
				host1x_uclass_wait_syncpt_r(), 1),
			nvhost_class_host_wait_syncpt(
				pdata->last_submit_syncpt_id,
				pdata->last_submit_syncpt_value));
	} else {
		pdata->last_submit_syncpt_id = job->sp[0].id;
		pdata->last_submit_syncpt_value = job->sp[0].fence;
	}
}

static void push_wait(struct nvhost_cdma *cdma, unsigned int id,
		      unsigned int thresh)
{
	/*
	 * Force serialization by inserting a host wait for the
	 * previous job to finish before this one can commence.
	 *
	 * NOTE! This cannot be packed because otherwise we might
	 * overwrite the RESTART opcode at the end of the push
	 * buffer.
	 */

	nvhost_cdma_push(cdma,
		nvhost_opcode_setclass(NV_HOST1X_CLASS_ID,
			host1x_uclass_load_syncpt_payload_32_r(), 1),
			thresh);
	nvhost_cdma_push(cdma,
		nvhost_opcode_setclass(NV_HOST1X_CLASS_ID,
			host1x_uclass_wait_syncpt_32_r(), 1),
			id);
}

static void serialize(struct nvhost_job *job)
{
	struct nvhost_channel *ch = job->ch;
	struct nvhost_syncpt *sp = &nvhost_get_host(ch->dev)->syncpt;
	struct nvhost_device_data *pdata = platform_get_drvdata(ch->dev);
	int i;

	if (!job->serialize && !pdata->serialize)
		return;

	for (i = 0; i < job->num_syncpts; ++i)
		push_wait(&ch->cdma, job->sp[i].id,
			  nvhost_syncpt_read_max(sp, job->sp[i].id));
}

#if defined(CONFIG_TEGRA_GRHOST_SYNC)
static int validate_syncpt_id_cb(struct nvhost_ctrl_sync_fence_info info,
				 void *data)
{
	struct nvhost_syncpt *sp = data;

	if (!nvhost_syncpt_is_valid_hw_pt(sp, info.id))
		return -EINVAL;

	return 0;
}

static int push_wait_cb(struct nvhost_ctrl_sync_fence_info info, void *data)
{
	struct nvhost_channel *ch = data;
	struct nvhost_master *host = nvhost_get_host(ch->dev);
	struct nvhost_syncpt *sp = &host->syncpt;

	if (nvhost_syncpt_is_expired(sp, info.id, info.thresh))
		return 0;

	nvhost_cdma_push(&ch->cdma,
		nvhost_opcode_setclass(NV_HOST1X_CLASS_ID,
			host1x_uclass_load_syncpt_payload_32_r(), 1),
			info.thresh);
	nvhost_cdma_push(&ch->cdma,
		nvhost_opcode_setclass(NV_HOST1X_CLASS_ID,
			host1x_uclass_wait_syncpt_32_r(), 1),
			info.id);

	return 0;
}

static void add_sync_waits(struct nvhost_channel *ch, int fd)
{
	struct nvhost_master *host = nvhost_get_host(ch->dev);
	struct nvhost_syncpt *sp = &host->syncpt;
	struct nvhost_fence *fence;

	if (fd < 0)
		return;

	fence = nvhost_fence_get(fd);
	if (!fence)
		return;

	if (nvhost_fence_foreach_pt(fence, validate_syncpt_id_cb, sp)) {
		nvhost_fence_put(fence);
		return;
	}

	/*
	 * Force serialization by inserting a host wait for the
	 * previous job to finish before this one can commence.
	 *
	 * NOTE! This cannot be packed because otherwise we might
	 * overwrite the RESTART opcode at the end of the push
	 * buffer.
	 */
	nvhost_fence_foreach_pt(fence, push_wait_cb, ch);

	nvhost_fence_put(fence);
}
#else
static void add_sync_waits(struct nvhost_channel *ch, int fd)
{
	(void)ch;
	(void)fd;
}
#endif

static void push_waits(struct nvhost_job *job)
{
	struct nvhost_device_data *pdata = platform_get_drvdata(job->ch->dev);
	struct nvhost_syncpt *sp = &nvhost_get_host(job->ch->dev)->syncpt;
	struct nvhost_channel *ch = job->ch;
	int i;

	for (i = 0; i < job->num_waitchk; i++) {
		struct nvhost_waitchk *wait = &job->waitchk[i];

		/* skip pushing waits if we allow them (map-at-open mode)
		 * and userspace wants to push a wait to some explicit
		 * position */
		if (pdata->resource_policy == RESOURCE_PER_DEVICE && wait->mem)
			continue;

		/* Skip pushing wait if it has already been expired */
		if (nvhost_syncpt_is_expired(sp, wait->syncpt_id,
					     wait->thresh))
			continue;

		push_wait(&ch->cdma, wait->syncpt_id, wait->thresh);
	}

	for (i = 0; i < job->num_gathers; i++)
		add_sync_waits(job->ch, job->gathers[i].pre_fence);
}

static void submit_work(struct nvhost_job *job)
{
	struct nvhost_device_data *pdata = platform_get_drvdata(job->ch->dev);
	void *cpuva = NULL;
	bool use_locking =
		pdata->resource_policy == RESOURCE_PER_CHANNEL_INSTANCE;
	u32 cur_class = 0;
	int i;

	/* make all waits in the beginning */
	push_waits(job);

	/* push user gathers */
	for (i = 0 ; i < job->num_gathers; i++) {
		struct nvhost_job_gather *g = &job->gathers[i];
		u32 op1;
		u32 op2;

		/* handle class changing */
		if (!cur_class || cur_class != g->class_id) {
			/* first, release current class */
			if (use_locking && cur_class &&
			    cur_class != NV_HOST1X_CLASS_ID) {
				lock_device(job, false);
				dev_warn(&job->ch->dev->dev, "%s changes out from engine class",
					 current->comm);
			}

			/* acquire lock of the new class */
			if (use_locking && g->class_id != NV_HOST1X_CLASS_ID)
				lock_device(job, true);

			/* update current class */
			nvhost_cdma_push(&job->ch->cdma,
				nvhost_opcode_setclass(g->class_id, 0, 0),
				NVHOST_OPCODE_NOOP);
			cur_class = g->class_id;

			/* initialize class context */
			if (cur_class != NV_HOST1X_CLASS_ID &&
			    pdata->init_class_context)
				pdata->init_class_context(
					job->ch->dev, &job->ch->cdma);
		}

		op1 = nvhost_opcode_gather(g->words);
		op2 = job->gathers[i].mem_base + g->offset;

		if (nvhost_debug_trace_cmdbuf)
			cpuva = dma_buf_vmap(g->buf);
		nvhost_cdma_push_gather(&job->ch->cdma,
				cpuva,
				job->gathers[i].mem_base,
				g->offset,
				op1, op2);
		if (cpuva)
			dma_buf_vunmap(g->buf, cpuva);
	}

	/* wait all work to complete */
	serialize(job);

	/* make final increment */
	submit_work_done_increment(job);

	/* release the engine */
	if (use_locking && cur_class && cur_class != NV_HOST1X_CLASS_ID)
		lock_device(job, false);
}

static int host1x_channel_submit(struct nvhost_job *job)
{
	struct nvhost_channel *ch = job->ch;
	struct nvhost_syncpt *sp = &nvhost_get_host(job->ch->dev)->syncpt;
	u32 prev_max = 0;
	int err, i;
	void *completed_waiters[job->num_syncpts];

	memset(completed_waiters, 0, sizeof(void *) * job->num_syncpts);

	/* Turn on the client module and host1x */
	for (i = 0; i < job->num_syncpts; ++i) {
		err = nvhost_module_busy(ch->dev);
		if (err) {
			nvhost_module_idle_mult(ch->dev, i);
			nvhost_putchannel(ch, i);
			return err;
		}

		nvhost_getchannel(ch);
	}

	/* before error checks, return current max */
	prev_max = job->sp->fence = nvhost_syncpt_read_max(sp, job->sp->id);

	/* get submit lock */
	err = mutex_lock_interruptible(&ch->submitlock);
	if (err) {
		nvhost_module_idle_mult(ch->dev, job->num_syncpts);
		nvhost_putchannel(ch, job->num_syncpts);
		goto error;
	}

	for (i = 0; i < job->num_syncpts; ++i) {
		completed_waiters[i] = nvhost_intr_alloc_waiter();
		if (!completed_waiters[i]) {
			nvhost_module_idle_mult(ch->dev, job->num_syncpts);
			nvhost_putchannel(ch, job->num_syncpts);
			mutex_unlock(&ch->submitlock);
			err = -ENOMEM;
			goto error;
		}
		if (nvhost_intr_has_pending_jobs(
			&nvhost_get_host(ch->dev)->intr, job->sp[i].id, ch))
			dev_warn(&ch->dev->dev,
				"%s: cross-channel dependencies on syncpt %d\n",
				__func__, job->sp[i].id);
	}

	/* begin a CDMA submit */
	err = nvhost_cdma_begin(&ch->cdma, job);
	if (err) {
		nvhost_module_idle_mult(ch->dev, job->num_syncpts);
		nvhost_putchannel(ch, job->num_syncpts);
		mutex_unlock(&ch->submitlock);
		goto error;
	}

	/* determine fences for all syncpoints */
	for (i = 0; i < job->num_syncpts; ++i) {
		u32 incrs = job->sp[i].incrs;

		/* create a valid max for client managed syncpoints */
		if (nvhost_syncpt_client_managed(sp, job->sp[i].id)) {
			u32 min = nvhost_syncpt_read(sp, job->sp[i].id);
			nvhost_syncpt_set_max(sp, job->sp[i].id, min);
			nvhost_syncpt_set_manager(sp, job->sp[i].id, false);
		}

		job->sp[i].fence =
			nvhost_syncpt_incr_max(sp, job->sp[i].id, incrs);

		/* mark syncpoint used by this channel */
		nvhost_syncpt_get_ref(sp, job->sp[i].id);
		nvhost_syncpt_mark_used(sp, ch->chid, job->sp[i].id);
	}

	/* mark also client managed syncpoint used by this channel */
	if (job->client_managed_syncpt)
		nvhost_syncpt_mark_used(sp, ch->chid,
					job->client_managed_syncpt);

	/* push work to hardware */
	submit_work(job);

	/* end CDMA submit & stash pinned hMems into sync queue */
	nvhost_cdma_end(&ch->cdma, job);

	trace_nvhost_channel_submitted(ch->dev->name, prev_max,
		job->sp->fence);

	for (i = 0; i < job->num_syncpts; ++i) {
		/* schedule a submit complete interrupt */
		err = nvhost_intr_add_action(&nvhost_get_host(ch->dev)->intr,
			job->sp[i].id, job->sp[i].fence,
			NVHOST_INTR_ACTION_SUBMIT_COMPLETE, ch,
			completed_waiters[i],
			NULL);
		WARN(err, "Failed to set submit complete interrupt");
	}

	mutex_unlock(&ch->submitlock);

	return 0;

error:
	for (i = 0; i < job->num_syncpts; ++i)
		kfree(completed_waiters[i]);
	return err;
}

static int host1x_channel_init_gather_filter(struct platform_device *pdev,
	struct nvhost_channel *ch)
{

	struct nvhost_master *master = nvhost_get_host(pdev);

	if (!nvhost_gather_filter_enabled(&master->syncpt))
		return -EINVAL;

	host1x_channel_writel(ch, host1x_channel_channelctrl_r(),
		host1x_channel_channelctrl_kernel_filter_gbuffer_f(1));

	return 0;
}

static int host1x_channel_init(struct nvhost_channel *ch,
	struct nvhost_master *dev)
{
	ch->aperture = host1x_channel_aperture(dev->aperture, ch->chid);

	return 0;
}

static const struct nvhost_channel_ops host1x_channel_ops = {
	.init = host1x_channel_init,
	.submit = host1x_channel_submit,
	.init_gather_filter = host1x_channel_init_gather_filter,
};
