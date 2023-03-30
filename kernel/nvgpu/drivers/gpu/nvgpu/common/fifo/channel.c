/*
 * GK20A Graphics channel
 *
 * Copyright (c) 2011-2022, NVIDIA CORPORATION.  All rights reserved.
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

#include <nvgpu/trace.h>
#include <nvgpu/mm.h>
#include <nvgpu/semaphore.h>
#include <nvgpu/timers.h>
#include <nvgpu/kmem.h>
#include <nvgpu/dma.h>
#include <nvgpu/log.h>
#include <nvgpu/atomic.h>
#include <nvgpu/bug.h>
#include <nvgpu/list.h>
#include <nvgpu/circ_buf.h>
#include <nvgpu/cond.h>
#include <nvgpu/enabled.h>
#include <nvgpu/debug.h>
#include <nvgpu/debugger.h>
#include <nvgpu/ltc.h>
#include <nvgpu/barrier.h>
#include <nvgpu/error_notifier.h>
#include <nvgpu/os_sched.h>
#include <nvgpu/log2.h>
#include <nvgpu/ptimer.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/mc.h>
#include <nvgpu/cic_rm.h>
#include <nvgpu/nvgpu_init.h>
#include <nvgpu/engines.h>
#include <nvgpu/channel.h>
#include <nvgpu/channel_sync.h>
#include <nvgpu/channel_sync_syncpt.h>
#include <nvgpu/channel_sync_semaphore.h>
#include <nvgpu/channel_user_syncpt.h>
#include <nvgpu/runlist.h>
#include <nvgpu/watchdog.h>
#include <nvgpu/fifo/userd.h>
#include <nvgpu/nvhost.h>
#include <nvgpu/fence.h>
#include <nvgpu/preempt.h>
#include <nvgpu/static_analysis.h>
#ifdef CONFIG_NVGPU_DEBUGGER
#include <nvgpu/gr/gr.h>
#endif
#include <nvgpu/job.h>
#include <nvgpu/priv_cmdbuf.h>
#include <nvgpu/string.h>
#include <nvgpu/nvs.h>

#include "channel_wdt.h"
#include "channel_worker.h"

#define CHANNEL_MAX_GPFIFO_ENTRIES	0x80000000U

static void free_channel(struct nvgpu_fifo *f, struct nvgpu_channel *ch);
static void channel_dump_ref_actions(struct nvgpu_channel *ch);

static int channel_setup_ramfc(struct nvgpu_channel *c,
		struct nvgpu_setup_bind_args *args,
		u64 gpfifo_gpu_va, u32 gpfifo_size);

/* allocate GPU channel */
static struct nvgpu_channel *allocate_channel(struct nvgpu_fifo *f)
{
	struct nvgpu_channel *ch = NULL;
#ifdef CONFIG_NVGPU_KERNEL_MODE_SUBMIT
	struct gk20a *g = f->g;
#endif

	nvgpu_mutex_acquire(&f->free_chs_mutex);
	if (!nvgpu_list_empty(&f->free_chs)) {
		ch = nvgpu_list_first_entry(&f->free_chs, nvgpu_channel,
							  free_chs);
		nvgpu_list_del(&ch->free_chs);
		WARN_ON(nvgpu_atomic_read(&ch->ref_count) != 0);
		WARN_ON(ch->referenceable);
		f->used_channels = nvgpu_safe_add_u32(f->used_channels, 1U);
	}
	nvgpu_mutex_release(&f->free_chs_mutex);

#ifdef CONFIG_NVGPU_KERNEL_MODE_SUBMIT
	if ((g->aggressive_sync_destroy_thresh != 0U) &&
			(f->used_channels >
			 g->aggressive_sync_destroy_thresh)) {
		g->aggressive_sync_destroy = true;
	}
#endif

	return ch;
}

static void free_channel(struct nvgpu_fifo *f,
		struct nvgpu_channel *ch)
{
#ifdef CONFIG_NVGPU_KERNEL_MODE_SUBMIT
	struct gk20a *g = f->g;
#endif

#ifdef CONFIG_NVGPU_TRACE
	trace_gk20a_release_used_channel(ch->chid);
#endif
	/* refcount is zero here and channel is in a freed/dead state */
	nvgpu_mutex_acquire(&f->free_chs_mutex);
	/* add to head to increase visibility of timing-related bugs */
	nvgpu_list_add(&ch->free_chs, &f->free_chs);
	f->used_channels = nvgpu_safe_sub_u32(f->used_channels, 1U);
	nvgpu_mutex_release(&f->free_chs_mutex);

	/*
	 * On teardown it is not possible to dereference platform, but ignoring
	 * this is fine then because no new channels would be created.
	 */
#ifdef CONFIG_NVGPU_KERNEL_MODE_SUBMIT
	if (!nvgpu_is_enabled(g, NVGPU_DRIVER_IS_DYING)) {
		if ((g->aggressive_sync_destroy_thresh != 0U) &&
			(f->used_channels <
			 g->aggressive_sync_destroy_thresh)) {
			g->aggressive_sync_destroy = false;
		}
	}
#endif
}

void nvgpu_channel_commit_va(struct nvgpu_channel *c)
{
	struct gk20a *g = c->g;

	nvgpu_log_fn(g, " ");

	if (g->ops.mm.init_inst_block_for_subctxs != NULL) {
		u32 subctx_count = nvgpu_channel_get_max_subctx_count(c);

		nvgpu_log(g, gpu_dbg_info | gpu_dbg_mig,
			"chid: %d max_subctx_count[%u] ",
			c->chid, subctx_count);
		g->ops.mm.init_inst_block_for_subctxs(&c->inst_block, c->vm,
				c->vm->gmmu_page_sizes[GMMU_PAGE_SIZE_BIG],
				subctx_count);
	} else {
		g->ops.mm.init_inst_block(&c->inst_block, c->vm,
				c->vm->gmmu_page_sizes[GMMU_PAGE_SIZE_BIG]);
	}
}

int nvgpu_channel_update_runlist(struct nvgpu_channel *c, bool add)
{
	return c->g->ops.runlist.update(c->g, c->runlist, c, add, true);
}

int nvgpu_channel_enable_tsg(struct gk20a *g, struct nvgpu_channel *ch)
{
	struct nvgpu_tsg *tsg;

	tsg = nvgpu_tsg_from_ch(ch);
	if (tsg != NULL) {
		g->ops.tsg.enable(tsg);
		return 0;
	} else {
		nvgpu_err(ch->g, "chid: %d is not bound to tsg", ch->chid);
		return -EINVAL;
	}
}

int nvgpu_channel_disable_tsg(struct gk20a *g, struct nvgpu_channel *ch)
{
	struct nvgpu_tsg *tsg;

	tsg = nvgpu_tsg_from_ch(ch);
	if (tsg != NULL) {
		g->ops.tsg.disable(tsg);
		return 0;
	} else {
		nvgpu_err(ch->g, "chid: %d is not bound to tsg", ch->chid);
		return -EINVAL;
	}
}

#ifdef CONFIG_NVGPU_KERNEL_MODE_SUBMIT
void nvgpu_channel_abort_clean_up(struct nvgpu_channel *ch)
{
	/* ensure no fences are pending */
	nvgpu_mutex_acquire(&ch->sync_lock);
	if (ch->sync != NULL) {
		nvgpu_channel_sync_set_min_eq_max(ch->sync);
	}

#ifdef CONFIG_TEGRA_GK20A_NVHOST
	if (ch->user_sync != NULL) {
		nvgpu_channel_user_syncpt_set_safe_state(ch->user_sync);
	}
#endif
	nvgpu_mutex_release(&ch->sync_lock);

	/* The update to flush the job queue is only needed to process
	 * nondeterministic resources and ch wdt timeouts. Any others are
	 * either nonexistent or preallocated from pools that can be killed in
	 * one go on deterministic channels; take a look at what would happen
	 * in nvgpu_channel_clean_up_deterministic_job() and what
	 * nvgpu_submit_deterministic() requires.
	 */
	if (!nvgpu_channel_is_deterministic(ch)) {
		/*
		 * When closing the channel, this scheduled update holds one
		 * channel ref which is waited for before advancing with
		 * freeing.
		 */
		nvgpu_channel_update(ch);
	}
}

static void channel_kernelmode_deinit(struct nvgpu_channel *ch)
{
	struct vm_gk20a *ch_vm = ch->vm;

	nvgpu_dma_unmap_free(ch_vm, &ch->gpfifo.mem);
#ifdef CONFIG_NVGPU_DGPU
	nvgpu_big_free(ch->g, ch->gpfifo.pipe);
#endif
	(void) memset(&ch->gpfifo, 0, sizeof(struct gpfifo_desc));

	if (ch->priv_cmd_q != NULL) {
		nvgpu_priv_cmdbuf_queue_free(ch->priv_cmd_q);
		ch->priv_cmd_q = NULL;
	}

	nvgpu_channel_joblist_deinit(ch);

	/* sync must be destroyed before releasing channel vm */
	nvgpu_mutex_acquire(&ch->sync_lock);
	if (ch->sync != NULL) {
		nvgpu_channel_sync_destroy(ch->sync);
		ch->sync = NULL;
	}
	nvgpu_mutex_release(&ch->sync_lock);
}

#ifdef CONFIG_TEGRA_GK20A_NVHOST
int nvgpu_channel_set_syncpt(struct nvgpu_channel *ch)
{
	struct gk20a *g = ch->g;
	struct nvgpu_channel_sync_syncpt *sync_syncpt;
	u32 new_syncpt = 0U;
	u32 old_syncpt = g->ops.ramfc.get_syncpt(ch);
	int err = 0;

	if (ch->sync != NULL) {
		sync_syncpt = nvgpu_channel_sync_to_syncpt(ch->sync);
		if (sync_syncpt != NULL) {
			new_syncpt =
			    nvgpu_channel_sync_get_syncpt_id(sync_syncpt);
		} else {
			new_syncpt = NVGPU_INVALID_SYNCPT_ID;
			/* ??? */
			return -EINVAL;
		}
	} else {
		return -EINVAL;
	}

	if ((new_syncpt != 0U) && (new_syncpt != old_syncpt)) {
		/* disable channel */
		err = nvgpu_channel_disable_tsg(g, ch);
		if (err != 0) {
			nvgpu_err(g, "failed to disable channel/TSG");
			return err;
		}

		/* preempt the channel */
		err = nvgpu_preempt_channel(g, ch);
		nvgpu_assert(err == 0);
		if (err != 0 ) {
			goto out;
		}
		/* no error at this point */
		g->ops.ramfc.set_syncpt(ch, new_syncpt);

		err =  nvgpu_channel_enable_tsg(g, ch);
		if (err != 0) {
			nvgpu_err(g, "failed to enable channel/TSG");
		}
	}

	nvgpu_log_fn(g, "done");
	return err;
out:
	if (nvgpu_channel_enable_tsg(g, ch) != 0) {
		nvgpu_err(g, "failed to enable channel/TSG");
	}
	return err;
}
#endif

static int channel_setup_kernelmode(struct nvgpu_channel *c,
		struct nvgpu_setup_bind_args *args)
{
	u32 gpfifo_size, gpfifo_entry_size;
	u64 gpfifo_gpu_va;
	u32 job_count;

	int err = 0;
	struct gk20a *g = c->g;

	gpfifo_size = args->num_gpfifo_entries;
	gpfifo_entry_size = nvgpu_get_gpfifo_entry_size();

	err = nvgpu_dma_alloc_map_sys(c->vm,
			(size_t)gpfifo_size * (size_t)gpfifo_entry_size,
			&c->gpfifo.mem);
	if (err != 0) {
		nvgpu_err(g, "memory allocation failed");
		goto clean_up;
	}

#ifdef CONFIG_NVGPU_DGPU
	if (c->gpfifo.mem.aperture == APERTURE_VIDMEM) {
		c->gpfifo.pipe = nvgpu_big_malloc(g,
					(size_t)gpfifo_size *
					(size_t)gpfifo_entry_size);
		if (c->gpfifo.pipe == NULL) {
			err = -ENOMEM;
			goto clean_up_unmap;
		}
	}
#endif
	gpfifo_gpu_va = c->gpfifo.mem.gpu_va;

	c->gpfifo.entry_num = gpfifo_size;
	c->gpfifo.get = 0;
	c->gpfifo.put = 0;

	nvgpu_log_info(g, "channel %d : gpfifo_base 0x%016llx, size %d",
		c->chid, gpfifo_gpu_va, c->gpfifo.entry_num);

	g->ops.userd.init_mem(g, c);

	if (g->aggressive_sync_destroy_thresh == 0U) {
		nvgpu_mutex_acquire(&c->sync_lock);
		c->sync = nvgpu_channel_sync_create(c);
		if (c->sync == NULL) {
			err = -ENOMEM;
			nvgpu_mutex_release(&c->sync_lock);
			goto clean_up_unmap;
		}
		nvgpu_mutex_release(&c->sync_lock);

		if (g->ops.channel.set_syncpt != NULL) {
			err = g->ops.channel.set_syncpt(c);
			if (err != 0) {
				goto clean_up_sync;
			}
		}
	}

	err = channel_setup_ramfc(c, args, gpfifo_gpu_va,
		c->gpfifo.entry_num);

	if (err != 0) {
		goto clean_up_sync;
	}

	/*
	 * Allocate priv cmdbuf space for pre and post fences. If the inflight
	 * job count isn't specified, we base it on the gpfifo count. We
	 * multiply by a factor of 1/3 because at most a third of the GPFIFO
	 * entries can be used for user-submitted jobs; another third goes to
	 * wait entries, and the final third to incr entries. There will be one
	 * pair of acq and incr commands for each job.
	 */
	job_count = args->num_inflight_jobs;
	if (job_count == 0U) {
		/*
		 * Round up so the allocation behaves nicely with a very small
		 * gpfifo, and to be able to use all slots when the entry count
		 * would be one too small for both wait and incr commands. An
		 * increment would then still just fit.
		 *
		 * gpfifo_size is required to be at most 2^31 earlier.
		 */
		job_count = nvgpu_safe_add_u32(gpfifo_size, 2U) / 3U;
	}

	err = nvgpu_channel_joblist_init(c, job_count);
	if (err != 0) {
		goto clean_up_sync;
	}

	err = nvgpu_priv_cmdbuf_queue_alloc(c->vm, job_count, &c->priv_cmd_q);
	if (err != 0) {
		goto clean_up_prealloc;
	}

	err = nvgpu_channel_update_runlist(c, true);
	if (err != 0) {
		goto clean_up_priv_cmd;
	}

	return 0;

clean_up_priv_cmd:
	nvgpu_priv_cmdbuf_queue_free(c->priv_cmd_q);
	c->priv_cmd_q = NULL;
clean_up_prealloc:
	nvgpu_channel_joblist_deinit(c);
clean_up_sync:
	if (c->sync != NULL) {
		nvgpu_channel_sync_destroy(c->sync);
		c->sync = NULL;
	}
clean_up_unmap:
#ifdef CONFIG_NVGPU_DGPU
	nvgpu_big_free(g, c->gpfifo.pipe);
#endif
	nvgpu_dma_unmap_free(c->vm, &c->gpfifo.mem);
clean_up:
	(void) memset(&c->gpfifo, 0, sizeof(struct gpfifo_desc));

	return err;

}

/* Update with this periodically to determine how the gpfifo is draining. */
static inline u32 channel_update_gpfifo_get(struct gk20a *g,
				struct nvgpu_channel *c)
{
	u32 new_get = g->ops.userd.gp_get(g, c);

	c->gpfifo.get = new_get;
	return new_get;
}

u32 nvgpu_channel_get_gpfifo_free_count(struct nvgpu_channel *ch)
{
	return (ch->gpfifo.entry_num - (ch->gpfifo.put - ch->gpfifo.get) - 1U) %
		ch->gpfifo.entry_num;
}

u32 nvgpu_channel_update_gpfifo_get_and_get_free_count(struct nvgpu_channel *ch)
{
	(void)channel_update_gpfifo_get(ch->g, ch);
	return nvgpu_channel_get_gpfifo_free_count(ch);
}

int nvgpu_channel_add_job(struct nvgpu_channel *c,
				 struct nvgpu_channel_job *job,
				 bool skip_buffer_refcounting)
{
	struct vm_gk20a *vm = c->vm;
	struct nvgpu_mapped_buf **mapped_buffers = NULL;
	int err = 0;
	u32 num_mapped_buffers = 0;

	if (!skip_buffer_refcounting) {
		err = nvgpu_vm_get_buffers(vm, &mapped_buffers,
					&num_mapped_buffers);
		if (err != 0) {
			return err;
		}
	}

	job->num_mapped_buffers = num_mapped_buffers;
	job->mapped_buffers = mapped_buffers;

	nvgpu_channel_launch_wdt(c);

	nvgpu_channel_joblist_lock(c);
	nvgpu_channel_joblist_add(c, job);
	nvgpu_channel_joblist_unlock(c);

	return 0;
}

/**
 * Release preallocated job resources from a job that's known to be completed.
 */
static void nvgpu_channel_finalize_job(struct nvgpu_channel *c,
		struct nvgpu_channel_job *job)
{
	/*
	 * On deterministic channels, this fence is just backed by a raw
	 * syncpoint. On nondeterministic channels the fence may be backed by a
	 * semaphore or even a syncfd.
	 */
	nvgpu_fence_put(&job->post_fence);

	/*
	 * Free the private command buffers (in order of allocation)
	 */
	if (job->wait_cmd != NULL) {
		nvgpu_priv_cmdbuf_free(c->priv_cmd_q, job->wait_cmd);
	}
	nvgpu_priv_cmdbuf_free(c->priv_cmd_q, job->incr_cmd);

	nvgpu_channel_free_job(c, job);

	nvgpu_channel_joblist_lock(c);
	nvgpu_channel_joblist_delete(c, job);
	nvgpu_channel_joblist_unlock(c);
}

/**
 * Clean up job resources for further jobs to use.
 *
 * Loop all jobs from the joblist until a pending job is found. Pending jobs
 * are detected from the job's post fence, so this is only done for jobs that
 * have job tracking resources. Free all per-job memory for completed jobs; in
 * case of preallocated resources, this opens up slots for new jobs to be
 * submitted.
 */
void nvgpu_channel_clean_up_jobs(struct nvgpu_channel *c)
{
	struct vm_gk20a *vm;
	struct nvgpu_channel_job *job;
	struct gk20a *g;
	bool job_finished = false;
	bool watchdog_on = false;

	if (nvgpu_is_powered_off(c->g)) { /* shutdown case */
		return;
	}

	vm = c->vm;
	g = c->g;

	nvgpu_assert(!nvgpu_channel_is_deterministic(c));

	watchdog_on = nvgpu_channel_wdt_stop(c->wdt);

	while (true) {
		bool completed;

		nvgpu_channel_joblist_lock(c);
		job = nvgpu_channel_joblist_peek(c);
		nvgpu_channel_joblist_unlock(c);

		if (job == NULL) {
			/*
			 * No jobs in flight, timeout will remain stopped until
			 * new jobs are submitted.
			 */
			break;
		}

		completed = nvgpu_fence_is_expired(&job->post_fence);
		if (!completed) {
			/*
			 * The watchdog eventually sees an updated gp_get if
			 * something happened in this loop. A new job can have
			 * been submitted between the above call to stop and
			 * this - in that case, this is a no-op and the new
			 * later timeout is still used.
			 */
			if (watchdog_on) {
				nvgpu_channel_wdt_continue(c->wdt);
			}
			break;
		}

		WARN_ON(c->sync == NULL);

		if (c->sync != NULL) {
			if (c->has_os_fence_framework_support &&
			    g->os_channel.os_fence_framework_inst_exists(c)) {
				g->os_channel.signal_os_fence_framework(c,
						&job->post_fence);
			}

			if (g->aggressive_sync_destroy_thresh != 0U) {
				nvgpu_mutex_acquire(&c->sync_lock);
				if (nvgpu_channel_sync_put_ref_and_check(c->sync)
					&& g->aggressive_sync_destroy) {
					nvgpu_channel_sync_destroy(c->sync);
					c->sync = NULL;
				}
				nvgpu_mutex_release(&c->sync_lock);
			}
		}

		if (job->num_mapped_buffers != 0U) {
			nvgpu_vm_put_buffers(vm, job->mapped_buffers,
				job->num_mapped_buffers);
		}

		nvgpu_channel_finalize_job(c, job);

		job_finished = true;

		/* taken in nvgpu_submit_nondeterministic() */
		gk20a_idle(g);
	}

	if ((job_finished) &&
			(g->os_channel.work_completion_signal != NULL)) {
		g->os_channel.work_completion_signal(c);
	}
}

/**
 * Clean up one job if any to provide space for a new submit.
 *
 * Deterministic channels do very little in the submit path, so the cleanup
 * code does not do much either. This assumes the preconditions that
 * deterministic channels are missing features such as timeouts and mapped
 * buffers.
 */
void nvgpu_channel_clean_up_deterministic_job(struct nvgpu_channel *c)
{
	struct nvgpu_channel_job *job;

	nvgpu_assert(nvgpu_channel_is_deterministic(c));

	nvgpu_channel_joblist_lock(c);
	job = nvgpu_channel_joblist_peek(c);
	nvgpu_channel_joblist_unlock(c);

	if (job == NULL) {
		/* Nothing queued */
		return;
	}

	nvgpu_assert(job->num_mapped_buffers == 0U);

	if (nvgpu_fence_is_expired(&job->post_fence)) {
		nvgpu_channel_finalize_job(c, job);
	}
}

/**
 * Schedule a job cleanup work on this channel to free resources and to signal
 * about completion.
 *
 * Call this when there has been an interrupt about finished jobs, or when job
 * cleanup needs to be performed, e.g., when closing a channel. This is always
 * safe to call even if there is nothing to clean up. Any visible actions on
 * jobs just before calling this are guaranteed to be processed.
 */
void nvgpu_channel_update(struct nvgpu_channel *c)
{
	if (nvgpu_is_powered_off(c->g)) { /* shutdown case */
		return;
	}
#ifdef CONFIG_NVGPU_TRACE
	trace_nvgpu_channel_update(c->chid);
#endif
	/* A queued channel is always checked for job cleanup. */
	nvgpu_channel_worker_enqueue(c);
}

bool nvgpu_channel_update_and_check_ctxsw_timeout(struct nvgpu_channel *ch,
		u32 timeout_delta_ms, bool *progress)
{
	u32 gpfifo_get;

	if (ch->usermode_submit_enabled) {
		ch->ctxsw_timeout_accumulated_ms += timeout_delta_ms;
		*progress = false;
		goto done;
	}

	gpfifo_get = channel_update_gpfifo_get(ch->g, ch);

	if (gpfifo_get == ch->ctxsw_timeout_gpfifo_get) {
		/* didn't advance since previous ctxsw timeout check */
		ch->ctxsw_timeout_accumulated_ms += timeout_delta_ms;
		*progress = false;
	} else {
		/* first ctxsw timeout isr encountered */
		ch->ctxsw_timeout_accumulated_ms = timeout_delta_ms;
		*progress = true;
	}

	ch->ctxsw_timeout_gpfifo_get = gpfifo_get;

done:
	return nvgpu_is_timeouts_enabled(ch->g) &&
		ch->ctxsw_timeout_accumulated_ms > ch->ctxsw_timeout_max_ms;
}

#else

void nvgpu_channel_abort_clean_up(struct nvgpu_channel *ch)
{
	/* ensure no fences are pending */
	nvgpu_mutex_acquire(&ch->sync_lock);
	if (ch->user_sync != NULL) {
		nvgpu_channel_user_syncpt_set_safe_state(ch->user_sync);
	}
	nvgpu_mutex_release(&ch->sync_lock);
}

#endif /* CONFIG_NVGPU_KERNEL_MODE_SUBMIT */

void nvgpu_channel_set_unserviceable(struct nvgpu_channel *ch)
{
	nvgpu_spinlock_acquire(&ch->unserviceable_lock);
	ch->unserviceable = true;
	nvgpu_spinlock_release(&ch->unserviceable_lock);
}

bool  nvgpu_channel_check_unserviceable(struct nvgpu_channel *ch)
{
	bool unserviceable_status;

	nvgpu_spinlock_acquire(&ch->unserviceable_lock);
	unserviceable_status = ch->unserviceable;
	nvgpu_spinlock_release(&ch->unserviceable_lock);

	return unserviceable_status;
}

void nvgpu_channel_abort(struct nvgpu_channel *ch, bool channel_preempt)
{
	struct nvgpu_tsg *tsg = nvgpu_tsg_from_ch(ch);

	nvgpu_log_fn(ch->g, " ");

	if (tsg != NULL) {
		return nvgpu_tsg_abort(ch->g, tsg, channel_preempt);
	} else {
		nvgpu_err(ch->g, "chid: %d is not bound to tsg", ch->chid);
	}
}

void nvgpu_channel_wait_until_counter_is_N(
	struct nvgpu_channel *ch, nvgpu_atomic_t *counter, int wait_value,
	struct nvgpu_cond *c, const char *caller, const char *counter_name)
{
	while (true) {
		if (NVGPU_COND_WAIT(
			    c,
			    nvgpu_atomic_read(counter) == wait_value,
			    5000U) == 0) {
			break;
		}

		nvgpu_warn(ch->g,
			   "%s: channel %d, still waiting, %s left: %d, waiting for: %d",
			   caller, ch->chid, counter_name,
			   nvgpu_atomic_read(counter), wait_value);

		channel_dump_ref_actions(ch);
	}
}

static void nvgpu_channel_usermode_deinit(struct nvgpu_channel *ch)
{
	nvgpu_channel_free_usermode_buffers(ch);
#ifdef CONFIG_NVGPU_USERD
	(void) nvgpu_userd_init_channel(ch->g, ch);
#endif
	ch->usermode_submit_enabled = false;
}

static void channel_free_invoke_unbind(struct nvgpu_channel *ch)
{
	int err = 0;
	struct nvgpu_tsg *tsg;
	struct gk20a *g = ch->g;

	if (!nvgpu_is_enabled(g, NVGPU_DRIVER_IS_DYING)) {
		/* abort channel and remove from runlist */
		tsg = nvgpu_tsg_from_ch(ch);
		if (tsg != NULL) {
			/* Between tsg is not null and unbind_channel call,
			 * ioctl cannot be called anymore because user doesn't
			 * have an open channel fd anymore to use for the unbind
			 * ioctl.
			 */
			err = nvgpu_tsg_unbind_channel(tsg, ch, true);
			if (err != 0) {
				nvgpu_err(g,
					"failed to unbind channel %d from TSG",
					ch->chid);
			}
		} else {
			/*
			 * Channel is already unbound from TSG by User with
			 * explicit call
			 * Nothing to do here in that case
			 */
		}
	}
}

static void channel_free_invoke_deferred_engine_reset(struct nvgpu_channel *ch)
{
#ifdef CONFIG_NVGPU_DEBUGGER
	struct gk20a *g = ch->g;
	struct nvgpu_fifo *f = &g->fifo;
	bool deferred_reset_pending;

	/* if engine reset was deferred, perform it now */
	nvgpu_mutex_acquire(&f->deferred_reset_mutex);
	deferred_reset_pending = g->fifo.deferred_reset_pending;
	nvgpu_mutex_release(&f->deferred_reset_mutex);

	if (deferred_reset_pending) {
		nvgpu_log(g, gpu_dbg_intr | gpu_dbg_gpu_dbg, "engine reset was"
				" deferred, running now");
		nvgpu_mutex_acquire(&g->fifo.engines_reset_mutex);

		nvgpu_assert(nvgpu_channel_deferred_reset_engines(g, ch) == 0);

		nvgpu_mutex_release(&g->fifo.engines_reset_mutex);
	}
#else
	(void)ch;
#endif
}

static void channel_free_invoke_sync_destroy(struct nvgpu_channel *ch)
{
#ifdef CONFIG_TEGRA_GK20A_NVHOST
	nvgpu_mutex_acquire(&ch->sync_lock);
	if (ch->user_sync != NULL) {
		/*
		 * Set user managed syncpoint to safe state
		 * But it's already done if channel is recovered
		 */
		if (!nvgpu_channel_check_unserviceable(ch)) {
			nvgpu_channel_user_syncpt_set_safe_state(ch->user_sync);
		}
		nvgpu_channel_user_syncpt_destroy(ch->user_sync);
		ch->user_sync = NULL;
	}
	nvgpu_mutex_release(&ch->sync_lock);
#else
	(void)ch;
#endif
}

static void channel_free_unlink_debug_session(struct nvgpu_channel *ch)
{
#ifdef CONFIG_NVGPU_DEBUGGER
	struct gk20a *g = ch->g;
	struct dbg_session_gk20a *dbg_s;
	struct dbg_session_data *session_data, *tmp_s;
	struct dbg_session_channel_data *ch_data, *tmp;

	/* unlink all debug sessions */
	nvgpu_mutex_acquire(&g->dbg_sessions_lock);

	nvgpu_list_for_each_entry_safe(session_data, tmp_s,
			&ch->dbg_s_list, dbg_session_data, dbg_s_entry) {
		dbg_s = session_data->dbg_s;
		nvgpu_mutex_acquire(&dbg_s->ch_list_lock);
		nvgpu_list_for_each_entry_safe(ch_data, tmp, &dbg_s->ch_list,
				dbg_session_channel_data, ch_entry) {
			if (ch_data->chid == ch->chid) {
				if (ch_data->unbind_single_channel(dbg_s,
						ch_data) != 0) {
					nvgpu_err(g,
						"unbind failed for chid: %d",
						ch_data->chid);
				}
			}
		}
		nvgpu_mutex_release(&dbg_s->ch_list_lock);
	}

	nvgpu_mutex_release(&g->dbg_sessions_lock);
#else
	(void)ch;
#endif
}

static void channel_free_wait_for_refs(struct nvgpu_channel *ch,
		int wait_value, bool force)
{
	/* wait until no more refs to the channel */
	if (!force) {
		nvgpu_channel_wait_until_counter_is_N(
			ch, &ch->ref_count, wait_value, &ch->ref_count_dec_wq,
			__func__, "references");
	}

}

#ifdef CONFIG_NVGPU_DETERMINISTIC_CHANNELS
static void channel_free_put_deterministic_ref_from_init(
		struct nvgpu_channel *ch)
{
	struct gk20a *g = ch->g;

	/* put back the channel-wide submit ref from init */
	if (ch->deterministic) {
		nvgpu_rwsem_down_read(&g->deterministic_busy);
		ch->deterministic = false;
		if (!ch->deterministic_railgate_allowed) {
			gk20a_idle(g);
		}
		ch->deterministic_railgate_allowed = false;

		nvgpu_rwsem_up_read(&g->deterministic_busy);
	}
}
#endif

/* call ONLY when no references to the channel exist: after the last put */
static void channel_free(struct nvgpu_channel *ch, bool force)
{
	struct gk20a *g = ch->g;
	struct nvgpu_fifo *f = &g->fifo;
	struct vm_gk20a *ch_vm = ch->vm;
	unsigned long timeout;

	if (g == NULL) {
		nvgpu_do_assert_print(g, "ch already freed");
		return;
	}

	nvgpu_log_fn(g, " ");

	timeout = nvgpu_get_poll_timeout(g);

#ifdef CONFIG_NVGPU_TRACE
	trace_gk20a_free_channel(ch->chid);
#endif

	/*
	 * Disable channel/TSG and unbind here. This should not be executed if
	 * HW access is not available during shutdown/removal path as it will
	 * trigger a timeout
	 */
	channel_free_invoke_unbind(ch);

	/*
	 * OS channel close may require that syncpoint should be set to some
	 * safe value before it is called. nvgpu_tsg_unbind_channel(above)
	 * is internally doing that by calling nvgpu_nvhost_syncpt_set_safe_-
	 * state deep down in the stack. Otherwise os_channel close may block if
	 * the app is killed abruptly (which was going to do the syncpoint
	 * signal).
	 */
	if (g->os_channel.close != NULL) {
		g->os_channel.close(ch, force);
	}

	/* wait until there's only our ref to the channel */
	channel_free_wait_for_refs(ch, 1, force);

	/* wait until all pending interrupts for recently completed
	 * jobs are handled */
	nvgpu_cic_rm_wait_for_deferred_interrupts(g);

	/* prevent new refs */
	nvgpu_spinlock_acquire(&ch->ref_obtain_lock);
	if (!ch->referenceable) {
		nvgpu_spinlock_release(&ch->ref_obtain_lock);
		nvgpu_err(ch->g,
			  "Extra %s() called to channel %u",
			  __func__, ch->chid);
		return;
	}
	ch->referenceable = false;
	nvgpu_spinlock_release(&ch->ref_obtain_lock);

	/* matches with the initial reference in nvgpu_channel_open_new() */
	nvgpu_atomic_dec(&ch->ref_count);

	channel_free_wait_for_refs(ch, 0, force);

	channel_free_invoke_deferred_engine_reset(ch);

	if (!nvgpu_channel_as_bound(ch)) {
		goto unbind;
	}

	nvgpu_log_info(g, "freeing bound channel context, timeout=%ld",
			timeout);

#ifdef CONFIG_NVGPU_FECS_TRACE
	if (g->ops.gr.fecs_trace.unbind_channel && !ch->vpr)
		g->ops.gr.fecs_trace.unbind_channel(g, &ch->inst_block);
#endif

	if (g->ops.gr.setup.free_subctx != NULL) {
		g->ops.gr.setup.free_subctx(ch);
		ch->subctx = NULL;
	}

	g->ops.gr.intr.flush_channel_tlb(g);

	if (ch->usermode_submit_enabled) {
		nvgpu_channel_usermode_deinit(ch);
	} else {
#ifdef CONFIG_NVGPU_KERNEL_MODE_SUBMIT
		channel_kernelmode_deinit(ch);
#endif
	}

	channel_free_invoke_sync_destroy(ch);

	/*
	 * When releasing the channel we unbind the VM - so release the ref.
	 */
	nvgpu_vm_put(ch_vm);

	/* make sure we don't have deferred interrupts pending that
	 * could still touch the channel */
	nvgpu_cic_rm_wait_for_deferred_interrupts(g);

unbind:
	g->ops.channel.unbind(ch);
	g->ops.channel.free_inst(g, ch);

	nvgpu_channel_wdt_destroy(ch->wdt);
	ch->wdt = NULL;

#ifdef CONFIG_NVGPU_DETERMINISTIC_CHANNELS
	channel_free_put_deterministic_ref_from_init(ch);
#endif

	ch->vpr = false;
	ch->vm = NULL;

#ifdef CONFIG_NVGPU_KERNEL_MODE_SUBMIT
	WARN_ON(ch->sync != NULL);
#endif

	channel_free_unlink_debug_session(ch);

#if GK20A_CHANNEL_REFCOUNT_TRACKING
	(void) memset(ch->ref_actions, 0, sizeof(ch->ref_actions));
	ch->ref_actions_put = 0;
#endif

	nvgpu_cond_destroy(&ch->notifier_wq);
	nvgpu_cond_destroy(&ch->semaphore_wq);

	/* make sure we catch accesses of unopened channels in case
	 * there's non-refcounted channel pointers hanging around */
	ch->g = NULL;
	nvgpu_smp_wmb();

	/* ALWAYS last */
	free_channel(f, ch);
}

static void channel_dump_ref_actions(struct nvgpu_channel *ch)
{
#if GK20A_CHANNEL_REFCOUNT_TRACKING
	size_t i, get;
	s64 now = nvgpu_current_time_ms();
	s64 prev = 0;
	struct gk20a *g = ch->g;

	nvgpu_spinlock_acquire(&ch->ref_actions_lock);

	nvgpu_info(g, "ch %d: refs %d. Actions, most recent last:",
			ch->chid, nvgpu_atomic_read(&ch->ref_count));

	/* start at the oldest possible entry. put is next insertion point */
	get = ch->ref_actions_put;

	/*
	 * If the buffer is not full, this will first loop to the oldest entry,
	 * skipping not-yet-initialized entries. There is no ref_actions_get.
	 */
	for (i = 0; i < GK20A_CHANNEL_REFCOUNT_TRACKING; i++) {
		struct nvgpu_channel_ref_action *act = &ch->ref_actions[get];

		if (act->trace.nr_entries) {
			nvgpu_info(g,
				"%s ref %zu steps ago (age %lld ms, diff %lld ms)",
				act->type == channel_gk20a_ref_action_get
					? "GET" : "PUT",
				GK20A_CHANNEL_REFCOUNT_TRACKING - 1 - i,
				now - act->timestamp_ms,
				act->timestamp_ms - prev);

			print_stack_trace(&act->trace, 0);
			prev = act->timestamp_ms;
		}

		get = (get + 1) % GK20A_CHANNEL_REFCOUNT_TRACKING;
	}

	nvgpu_spinlock_release(&ch->ref_actions_lock);
#else
	(void)ch;
#endif
}

#if GK20A_CHANNEL_REFCOUNT_TRACKING
static void channel_save_ref_source(struct nvgpu_channel *ch,
		enum nvgpu_channel_ref_action_type type)
{
	struct nvgpu_channel_ref_action *act;

	nvgpu_spinlock_acquire(&ch->ref_actions_lock);

	act = &ch->ref_actions[ch->ref_actions_put];
	act->type = type;
	act->trace.max_entries = GK20A_CHANNEL_REFCOUNT_TRACKING_STACKLEN;
	act->trace.nr_entries = 0;
	act->trace.skip = 3; /* onwards from the caller of this */
	act->trace.entries = act->trace_entries;
	save_stack_trace(&act->trace);
	act->timestamp_ms = nvgpu_current_time_ms();
	ch->ref_actions_put = (ch->ref_actions_put + 1) %
		GK20A_CHANNEL_REFCOUNT_TRACKING;

	nvgpu_spinlock_release(&ch->ref_actions_lock);
}
#endif

/* Try to get a reference to the channel. Return nonzero on success. If fails,
 * the channel is dead or being freed elsewhere and you must not touch it.
 *
 * Always when a nvgpu_channel pointer is seen and about to be used, a
 * reference must be held to it - either by you or the caller, which should be
 * documented well or otherwise clearly seen. This usually boils down to the
 * file from ioctls directly, or an explicit get in exception handlers when the
 * channel is found by a chid.
 *
 * Most global functions in this file require a reference to be held by the
 * caller.
 */
struct nvgpu_channel *nvgpu_channel_get__func(struct nvgpu_channel *ch,
					 const char *caller)
{
	struct nvgpu_channel *ret;

	nvgpu_spinlock_acquire(&ch->ref_obtain_lock);

	if (likely(ch->referenceable)) {
#if GK20A_CHANNEL_REFCOUNT_TRACKING
		channel_save_ref_source(ch, channel_gk20a_ref_action_get);
#endif
		nvgpu_atomic_inc(&ch->ref_count);
		ret = ch;
	} else {
		ret = NULL;
	}

	nvgpu_spinlock_release(&ch->ref_obtain_lock);

#ifdef CONFIG_NVGPU_TRACE
	if (ret != NULL) {
		trace_nvgpu_channel_get(ch->chid, caller);
	}
#else
	(void)caller;
#endif

	return ret;
}

void nvgpu_channel_put__func(struct nvgpu_channel *ch, const char *caller)
{
#if GK20A_CHANNEL_REFCOUNT_TRACKING
	channel_save_ref_source(ch, channel_gk20a_ref_action_put);
#endif
#ifdef CONFIG_NVGPU_TRACE
	trace_nvgpu_channel_put(ch->chid, caller);
#else
	(void)caller;
#endif
	nvgpu_atomic_dec(&ch->ref_count);
	if (nvgpu_cond_broadcast(&ch->ref_count_dec_wq) != 0) {
		nvgpu_warn(ch->g, "failed to broadcast");
	}

	/* More puts than gets. Channel is probably going to get
	 * stuck. */
	WARN_ON(nvgpu_atomic_read(&ch->ref_count) < 0);

	/* Also, more puts than gets. ref_count can go to 0 only if
	 * the channel is closing. Channel is probably going to get
	 * stuck. */
	WARN_ON((nvgpu_atomic_read(&ch->ref_count) == 0) && ch->referenceable);
}

struct nvgpu_channel *nvgpu_channel_from_id__func(struct gk20a *g,
				u32 chid, const char *caller)
{
	if (chid >= g->fifo.num_channels) {
		return NULL;
	}

	return nvgpu_channel_get__func(&g->fifo.channel[chid], caller);
}

void nvgpu_channel_close(struct nvgpu_channel *ch)
{
	channel_free(ch, false);
}

/*
 * Be careful with this - it is meant for terminating channels when we know the
 * driver is otherwise dying. Ref counts and the like are ignored by this
 * version of the cleanup.
 */
void nvgpu_channel_kill(struct nvgpu_channel *ch)
{
	channel_free(ch, true);
}

struct nvgpu_channel *nvgpu_channel_open_new(struct gk20a *g,
		u32 runlist_id,
		bool is_privileged_channel,
		pid_t pid, pid_t tid)
{
	struct nvgpu_fifo *f = &g->fifo;
	struct nvgpu_channel *ch;

	/* compatibility with existing code */
	if (!nvgpu_engine_is_valid_runlist_id(g, runlist_id)) {
		runlist_id = nvgpu_engine_get_gr_runlist_id(g);
	}

	nvgpu_log_fn(g, " ");

	ch = allocate_channel(f);
	if (ch == NULL) {
		/* TBD: we want to make this virtualizable */
		nvgpu_err(g, "out of hw chids");
		return NULL;
	}

#ifdef CONFIG_NVGPU_TRACE
	trace_nvgpu_channel_open_new(ch->chid);
#endif

	BUG_ON(ch->g != NULL);
	ch->g = g;

	/* Runlist for the channel */
	ch->runlist = f->runlists[runlist_id];

	/* Channel privilege level */
	ch->is_privileged_channel = is_privileged_channel;

	ch->pid = tid;
	ch->tgid = pid;  /* process granularity for FECS traces */

#ifdef CONFIG_NVGPU_USERD
	if (nvgpu_userd_init_channel(g, ch) != 0) {
		nvgpu_err(g, "userd init failed");
		goto clean_up;
	}
#endif

	if (g->ops.channel.alloc_inst(g, ch) != 0) {
		nvgpu_err(g, "inst allocation failed");
		goto clean_up;
	}

	/* now the channel is in a limbo out of the free list but not marked as
	 * alive and used (i.e. get-able) yet */

	/* By default, channel is regular (non-TSG) channel */
	ch->tsgid = NVGPU_INVALID_TSG_ID;

	/* clear ctxsw timeout counter and update timestamp */
	ch->ctxsw_timeout_accumulated_ms = 0;
	ch->ctxsw_timeout_gpfifo_get = 0;
	/* set gr host default timeout */
	ch->ctxsw_timeout_max_ms = nvgpu_get_poll_timeout(g);
	ch->ctxsw_timeout_debug_dump = true;
	/* ch is unserviceable until it is bound to tsg */
	ch->unserviceable = true;

#ifdef CONFIG_NVGPU_CHANNEL_WDT
	ch->wdt = nvgpu_channel_wdt_alloc(g);
	if (ch->wdt == NULL) {
		nvgpu_err(g, "wdt alloc failed");
		goto clean_up;
	}
	ch->wdt_debug_dump = true;
#endif

	ch->obj_class = 0;
	ch->subctx_id = 0;
	ch->runqueue_sel = 0;

	ch->mmu_nack_handled = false;

	/* The channel is *not* runnable at this point. It still needs to have
	 * an address space bound and allocate a gpfifo and grctx. */

	if (nvgpu_cond_init(&ch->notifier_wq) != 0) {
		nvgpu_err(g, "cond init failed");
		goto clean_up;
	}
	if (nvgpu_cond_init(&ch->semaphore_wq) != 0) {
		nvgpu_err(g, "cond init failed");
		goto clean_up;
	}

	/* Mark the channel alive, get-able, with 1 initial use
	 * references. The initial reference will be decreased in
	 * channel_free().
	 *
	 * Use the lock, since an asynchronous thread could
	 * try to access this channel while it's not fully
	 * initialized.
	 */
	nvgpu_spinlock_acquire(&ch->ref_obtain_lock);
	ch->referenceable = true;
	nvgpu_atomic_set(&ch->ref_count, 1);
	nvgpu_spinlock_release(&ch->ref_obtain_lock);

	return ch;

clean_up:
	ch->g = NULL;
	free_channel(f, ch);
	return NULL;
}

static int channel_setup_ramfc(struct nvgpu_channel *c,
		struct nvgpu_setup_bind_args *args,
		u64 gpfifo_gpu_va, u32 gpfifo_size)
{
	int err = 0;
	u64 pbdma_acquire_timeout = 0ULL;
	struct gk20a *g = c->g;

	if (nvgpu_channel_wdt_enabled(c->wdt) &&
			nvgpu_is_timeouts_enabled(c->g)) {
		pbdma_acquire_timeout = nvgpu_channel_wdt_limit(c->wdt);
	}

	err = g->ops.ramfc.setup(c, gpfifo_gpu_va, gpfifo_size,
			pbdma_acquire_timeout, args->flags);

	return err;
}

static int nvgpu_channel_setup_usermode(struct nvgpu_channel *c,
		struct nvgpu_setup_bind_args *args)
{
	u32 gpfifo_size = args->num_gpfifo_entries;
	int err = 0;
	struct gk20a *g = c->g;
	u64 gpfifo_gpu_va;

	if (g->os_channel.alloc_usermode_buffers != NULL) {
		err = g->os_channel.alloc_usermode_buffers(c, args);
		if (err != 0) {
			nvgpu_err(g, "Usermode buffer alloc failed");
			goto clean_up;
		}
		c->userd_mem = &c->usermode_userd;
		c->userd_offset = 0U;
		c->userd_iova = nvgpu_mem_get_addr(g, c->userd_mem);
		c->usermode_submit_enabled = true;
	} else {
		nvgpu_err(g, "Usermode submit not supported");
		err = -EINVAL;
		goto clean_up;
	}
	gpfifo_gpu_va = c->usermode_gpfifo.gpu_va;

	nvgpu_log_info(g, "channel %d : gpfifo_base 0x%016llx, size %d",
		c->chid, gpfifo_gpu_va, gpfifo_size);

	err = channel_setup_ramfc(c, args, gpfifo_gpu_va, gpfifo_size);

	if (err != 0) {
		goto clean_up_unmap;
	}

	err = nvgpu_channel_update_runlist(c, true);
	if (err != 0) {
		goto clean_up_unmap;
	}

	return 0;

clean_up_unmap:
	nvgpu_channel_free_usermode_buffers(c);
#ifdef CONFIG_NVGPU_USERD
	(void) nvgpu_userd_init_channel(g, c);
#endif
	c->usermode_submit_enabled = false;
clean_up:
	return err;
}

static int channel_setup_bind_prechecks(struct nvgpu_channel *c,
		struct nvgpu_setup_bind_args *args)
{
	struct gk20a *g = c->g;
	struct nvgpu_tsg *tsg;
	int err = 0;

	if (args->num_gpfifo_entries > CHANNEL_MAX_GPFIFO_ENTRIES) {
		nvgpu_err(g,
			"num_gpfifo_entries exceeds max limit of 2^31");
		err = -EINVAL;
		goto fail;
	}

	/*
	 * The gpfifo ring buffer is empty when get == put and it's full when
	 * get == put + 1. Just one entry wouldn't make sense.
	 */
	if (args->num_gpfifo_entries < 2U) {
		nvgpu_err(g, "gpfifo has no space for any jobs");
		err = -EINVAL;
		goto fail;
	}

	/* an address space needs to have been bound at this point. */
	if (!nvgpu_channel_as_bound(c)) {
		nvgpu_err(g,
			"not bound to an address space at time of setup_bind");
		err = -EINVAL;
		goto fail;
	}

	/* The channel needs to be bound to a tsg at this point */
	tsg = nvgpu_tsg_from_ch(c);
	if (tsg == NULL) {
		nvgpu_err(g,
			"not bound to tsg at time of setup_bind");
		err = -EINVAL;
		goto fail;
	}

	if (c->usermode_submit_enabled) {
		nvgpu_err(g, "channel %d : "
			    "usermode buffers allocated", c->chid);
		err = -EEXIST;
		goto fail;
	}

#ifdef CONFIG_NVGPU_KERNEL_MODE_SUBMIT
	if (nvgpu_mem_is_valid(&c->gpfifo.mem)) {
		nvgpu_err(g, "channel %d :"
			   "gpfifo already allocated", c->chid);
		err = -EEXIST;
		goto fail;
	}
#endif
	if ((args->flags & NVGPU_SETUP_BIND_FLAGS_SUPPORT_DETERMINISTIC) != 0U
			&& nvgpu_channel_wdt_enabled(c->wdt)) {
		/*
		 * The watchdog would need async job tracking, but that's not
		 * compatible with deterministic mode. We won't disable it
		 * implicitly; the user has to ask.
		 */
		nvgpu_err(g,
			"deterministic is not compatible with watchdog");
		err = -EINVAL;
		goto fail;
	}

	/* FUSA build for now assumes that the deterministic flag is not useful */
#ifdef CONFIG_NVGPU_IOCTL_NON_FUSA
	if ((args->flags & NVGPU_SETUP_BIND_FLAGS_USERMODE_SUPPORT) != 0U &&
	    (args->flags & NVGPU_SETUP_BIND_FLAGS_SUPPORT_DETERMINISTIC) == 0U) {
		/*
		 * Usermode submit shares various preconditions with
		 * deterministic mode. Require that it's explicitly set to
		 * avoid surprises.
		 */
		nvgpu_err(g, "need deterministic for usermode submit");
		err = -EINVAL;
		goto fail;
	}
#endif

fail:
	return err;
}

int nvgpu_channel_setup_bind(struct nvgpu_channel *c,
		struct nvgpu_setup_bind_args *args)
{
	struct gk20a *g = c->g;
	int err = 0;

	err = channel_setup_bind_prechecks(c, args);
	if (err != 0) {
		goto fail;
	}

#ifdef CONFIG_NVGPU_VPR
	if ((args->flags & NVGPU_SETUP_BIND_FLAGS_SUPPORT_VPR) != 0U) {
		if (!nvgpu_is_enabled(g, NVGPU_SUPPORT_VPR)) {
			err = -EINVAL;
			goto fail;
		}

		c->vpr = true;
	}
#else
	c->vpr = false;
#endif

#ifdef CONFIG_NVGPU_DETERMINISTIC_CHANNELS
	if ((args->flags & NVGPU_SETUP_BIND_FLAGS_SUPPORT_DETERMINISTIC) != 0U) {
		nvgpu_rwsem_down_read(&g->deterministic_busy);
		/*
		 * Railgating isn't deterministic; instead of disallowing
		 * railgating globally, take a power refcount for this
		 * channel's lifetime. The gk20a_idle() pair for this happens
		 * when the channel gets freed.
		 *
		 * Deterministic flag and this busy must be atomic within the
		 * busy lock.
		 */
		err = gk20a_busy(g);
		if (err != 0) {
			nvgpu_rwsem_up_read(&g->deterministic_busy);
			return err;
		}

		c->deterministic = true;
		nvgpu_rwsem_up_read(&g->deterministic_busy);
	}
#endif

	if ((args->flags & NVGPU_SETUP_BIND_FLAGS_USERMODE_SUPPORT) != 0U) {
		err = nvgpu_channel_setup_usermode(c, args);
	} else {
#ifdef CONFIG_NVGPU_KERNEL_MODE_SUBMIT
		if (g->os_channel.open != NULL) {
			g->os_channel.open(c);
		}
		err = channel_setup_kernelmode(c, args);
#else
		err = -EINVAL;
#endif
	}

	if (err != 0) {
		goto clean_up_idle;
	}

	g->ops.channel.bind(c);

	nvgpu_log_fn(g, "done");
	return 0;

clean_up_idle:
#ifdef CONFIG_NVGPU_DETERMINISTIC_CHANNELS
	if (nvgpu_channel_is_deterministic(c)) {
		nvgpu_rwsem_down_read(&g->deterministic_busy);
		gk20a_idle(g);
		c->deterministic = false;
		nvgpu_rwsem_up_read(&g->deterministic_busy);
	}
#endif
fail:
	nvgpu_err(g, "fail");
	return err;
}

void nvgpu_channel_free_usermode_buffers(struct nvgpu_channel *c)
{
	if (nvgpu_mem_is_valid(&c->usermode_userd)) {
		nvgpu_dma_free(c->g, &c->usermode_userd);
	}
	if (nvgpu_mem_is_valid(&c->usermode_gpfifo)) {
		nvgpu_dma_unmap_free(c->vm, &c->usermode_gpfifo);
	}
	if (c->g->os_channel.free_usermode_buffers != NULL) {
		c->g->os_channel.free_usermode_buffers(c);
	}
}

static bool nvgpu_channel_ctxsw_timeout_debug_dump_state(
				struct nvgpu_channel *ch)
{
	bool verbose = false;
	if (nvgpu_is_err_notifier_set(ch,
			NVGPU_ERR_NOTIFIER_FIFO_ERROR_IDLE_TIMEOUT)) {
		verbose = ch->ctxsw_timeout_debug_dump;
	}

	return verbose;
}

void nvgpu_channel_wakeup_wqs(struct gk20a *g,
				struct nvgpu_channel *ch)
{
	/* unblock pending waits */
	if (nvgpu_cond_broadcast_interruptible(&ch->semaphore_wq) != 0) {
		nvgpu_warn(g, "failed to broadcast");
	}
	if (nvgpu_cond_broadcast_interruptible(&ch->notifier_wq) != 0) {
		nvgpu_warn(g, "failed to broadcast");
	}
}

bool nvgpu_channel_mark_error(struct gk20a *g, struct nvgpu_channel *ch)
{
	bool verbose;

	verbose = nvgpu_channel_ctxsw_timeout_debug_dump_state(ch);

	/* mark channel as faulted */
	nvgpu_channel_set_unserviceable(ch);

	nvgpu_channel_wakeup_wqs(g, ch);

	return verbose;
}

void nvgpu_channel_set_error_notifier(struct gk20a *g, struct nvgpu_channel *ch,
				u32 error_notifier)
{
	g->ops.channel.set_error_notifier(ch, error_notifier);
}

void nvgpu_channel_sw_quiesce(struct gk20a *g)
{
	struct nvgpu_fifo *f = &g->fifo;
	struct nvgpu_channel *ch;
	u32 chid;

	for (chid = 0; chid < f->num_channels; chid++) {
		ch = nvgpu_channel_get(&f->channel[chid]);
		if (ch != NULL) {
			nvgpu_channel_set_error_notifier(g, ch,
				NVGPU_ERR_NOTIFIER_FIFO_ERROR_IDLE_TIMEOUT);
			nvgpu_channel_set_unserviceable(ch);
			nvgpu_channel_wakeup_wqs(g, ch);
			nvgpu_channel_put(ch);
		}
	}
}

#ifdef CONFIG_NVGPU_DETERMINISTIC_CHANNELS
/*
 * Stop deterministic channel activity for do_idle() when power needs to go off
 * momentarily but deterministic channels keep power refs for potentially a
 * long time.
 *
 * Takes write access on g->deterministic_busy.
 *
 * Must be paired with nvgpu_channel_deterministic_unidle().
 */
void nvgpu_channel_deterministic_idle(struct gk20a *g)
{
	struct nvgpu_fifo *f = &g->fifo;
	u32 chid;

	/* Grab exclusive access to the hw to block new submits */
	nvgpu_rwsem_down_write(&g->deterministic_busy);

	for (chid = 0; chid < f->num_channels; chid++) {
		struct nvgpu_channel *ch = nvgpu_channel_from_id(g, chid);

		if (ch == NULL) {
			continue;
		}

		if (ch->deterministic && !ch->deterministic_railgate_allowed) {
			/*
			 * Drop the power ref taken when setting deterministic
			 * flag. deterministic_unidle will put this and the
			 * channel ref back. If railgate is allowed separately
			 * for this channel, the power ref has already been put
			 * away.
			 *
			 * Hold the channel ref: it must not get freed in
			 * between. A race could otherwise result in lost
			 * gk20a_busy() via unidle, and in unbalanced
			 * gk20a_idle() via closing the channel.
			 */
			gk20a_idle(g);
		} else {
			/* Not interesting, carry on. */
			nvgpu_channel_put(ch);
		}
	}
}

/*
 * Allow deterministic channel activity again for do_unidle().
 *
 * This releases write access on g->deterministic_busy.
 */
void nvgpu_channel_deterministic_unidle(struct gk20a *g)
{
	struct nvgpu_fifo *f = &g->fifo;
	u32 chid;
	int err;

	for (chid = 0; chid < f->num_channels; chid++) {
		struct nvgpu_channel *ch = nvgpu_channel_from_id(g, chid);

		if (ch == NULL) {
			continue;
		}

		/*
		 * Deterministic state changes inside deterministic_busy lock,
		 * which we took in deterministic_idle.
		 */
		if (ch->deterministic && !ch->deterministic_railgate_allowed) {
			err = gk20a_busy(g);
			if (err != 0) {
				nvgpu_err(g, "cannot busy() again!");
			}
			/* Took this in idle() */
			nvgpu_channel_put(ch);
		}

		nvgpu_channel_put(ch);
	}

	/* Release submits, new deterministic channels and frees */
	nvgpu_rwsem_up_write(&g->deterministic_busy);
}
#endif

static void nvgpu_channel_destroy(struct nvgpu_channel *c)
{
	nvgpu_mutex_destroy(&c->ioctl_lock);
#ifdef CONFIG_NVGPU_KERNEL_MODE_SUBMIT
	nvgpu_mutex_destroy(&c->joblist.pre_alloc.read_lock);
#endif
	nvgpu_mutex_destroy(&c->sync_lock);
#if defined(CONFIG_NVGPU_CYCLESTATS)
	nvgpu_mutex_destroy(&c->cyclestate.cyclestate_buffer_mutex);
	nvgpu_mutex_destroy(&c->cs_client_mutex);
#endif
#if defined(CONFIG_NVGPU_DEBUGGER)
	nvgpu_mutex_destroy(&c->dbg_s_lock);
#endif
}

void nvgpu_channel_cleanup_sw(struct gk20a *g)
{
	struct nvgpu_fifo *f = &g->fifo;
	u32 chid;

	/*
	 * Make sure all channels are closed before deleting them.
	 */
	for (chid = 0; chid < f->num_channels; chid++) {
		struct nvgpu_channel *ch = &f->channel[chid];

		/*
		 * Could race but worst that happens is we get an error message
		 * from channel_free() complaining about multiple closes.
		 */
		if (ch->referenceable) {
			nvgpu_channel_kill(ch);
		}

		nvgpu_channel_destroy(ch);
	}

	nvgpu_vfree(g, f->channel);
	f->channel = NULL;
	nvgpu_mutex_destroy(&f->free_chs_mutex);
}

int nvgpu_channel_init_support(struct gk20a *g, u32 chid)
{
	struct nvgpu_channel *c = &g->fifo.channel[chid];
	int err;

	c->g = NULL;
	c->chid = chid;
	nvgpu_atomic_set(&c->bound, 0);
	nvgpu_spinlock_init(&c->ref_obtain_lock);
	nvgpu_atomic_set(&c->ref_count, 0);
	c->referenceable = false;
	err = nvgpu_cond_init(&c->ref_count_dec_wq);
	if (err != 0) {
		nvgpu_err(g, "cond_init failed");
		return err;
	}

	nvgpu_spinlock_init(&c->unserviceable_lock);

#if GK20A_CHANNEL_REFCOUNT_TRACKING
	nvgpu_spinlock_init(&c->ref_actions_lock);
#endif
#ifdef CONFIG_NVGPU_KERNEL_MODE_SUBMIT
	nvgpu_init_list_node(&c->worker_item);

	nvgpu_mutex_init(&c->joblist.pre_alloc.read_lock);

#endif /* CONFIG_NVGPU_KERNEL_MODE_SUBMIT */
	nvgpu_mutex_init(&c->ioctl_lock);
	nvgpu_mutex_init(&c->sync_lock);
#if defined(CONFIG_NVGPU_CYCLESTATS)
	nvgpu_mutex_init(&c->cyclestate.cyclestate_buffer_mutex);
	nvgpu_mutex_init(&c->cs_client_mutex);
#endif
#if defined(CONFIG_NVGPU_DEBUGGER)
	nvgpu_init_list_node(&c->dbg_s_list);
	nvgpu_mutex_init(&c->dbg_s_lock);
#endif
	nvgpu_init_list_node(&c->ch_entry);
	nvgpu_list_add(&c->free_chs, &g->fifo.free_chs);

	return 0;
}

int nvgpu_channel_setup_sw(struct gk20a *g)
{
	struct nvgpu_fifo *f = &g->fifo;
	u32 chid, i;
	int err;

	f->num_channels = g->ops.channel.count(g);

	nvgpu_mutex_init(&f->free_chs_mutex);

	f->channel = nvgpu_vzalloc(g, f->num_channels * sizeof(*f->channel));
	if (f->channel == NULL) {
		nvgpu_err(g, "no mem for channels");
		err = -ENOMEM;
		goto clean_up_mutex;
	}

	nvgpu_init_list_node(&f->free_chs);

	for (chid = 0; chid < f->num_channels; chid++) {
		err = nvgpu_channel_init_support(g, chid);
		if (err != 0) {
			nvgpu_err(g, "channel init failed, chid=%u", chid);
			goto clean_up;
		}
	}

	return 0;

clean_up:
	for (i = 0; i < chid; i++) {
		struct nvgpu_channel *ch = &f->channel[i];

		nvgpu_channel_destroy(ch);
	}
	nvgpu_vfree(g, f->channel);
	f->channel = NULL;

clean_up_mutex:
	nvgpu_mutex_destroy(&f->free_chs_mutex);

	return err;
}

int nvgpu_channel_suspend_all_serviceable_ch(struct gk20a *g)
{
	struct nvgpu_fifo *f = &g->fifo;
	u32 chid;
	bool channels_in_use = false;
	u32 active_runlist_ids = 0;
	int err;

	nvgpu_log_fn(g, " ");

	for (chid = 0; chid < f->num_channels; chid++) {
		struct nvgpu_channel *ch = nvgpu_channel_from_id(g, chid);

		if (ch == NULL) {
			continue;
		}
		if (nvgpu_channel_check_unserviceable(ch)) {
			nvgpu_log_info(g, "do not suspend recovered "
						"channel %d", chid);
		} else {
			nvgpu_log_info(g, "suspend channel %d", chid);
			/* disable channel */
			if (nvgpu_channel_disable_tsg(g, ch) != 0) {
				nvgpu_err(g, "failed to disable channel/TSG");
			}
			/* preempt the channel */
			err = nvgpu_preempt_channel(g, ch);
			if (err != 0) {
				nvgpu_err(g, "failed to preempt channel/TSG");
			}
#ifdef CONFIG_NVGPU_KERNEL_MODE_SUBMIT
			/* wait for channel update notifiers */
			if (g->os_channel.work_completion_cancel_sync != NULL) {
				g->os_channel.work_completion_cancel_sync(ch);
			}
#endif

			g->ops.channel.unbind(ch);

			channels_in_use = true;

			active_runlist_ids |=  BIT32(ch->runlist->id);
		}

		nvgpu_channel_put(ch);
	}

	if (channels_in_use) {
		nvgpu_assert(nvgpu_runlist_reload_ids(g,
				active_runlist_ids, false) == 0);
	}

	nvgpu_log_fn(g, "done");
	return 0;
}

int nvgpu_channel_resume_all_serviceable_ch(struct gk20a *g)
{
	struct nvgpu_fifo *f = &g->fifo;
	u32 chid;
	bool channels_in_use = false;
	u32 active_runlist_ids = 0;

	nvgpu_log_fn(g, " ");

	for (chid = 0; chid < f->num_channels; chid++) {
		struct nvgpu_channel *ch = nvgpu_channel_from_id(g, chid);

		if (ch == NULL) {
			continue;
		}
		if (nvgpu_channel_check_unserviceable(ch)) {
			nvgpu_log_info(g, "do not resume recovered "
						"channel %d", chid);
		} else {
			nvgpu_log_info(g, "resume channel %d", chid);
			g->ops.channel.bind(ch);
			channels_in_use = true;
			active_runlist_ids |= BIT32(ch->runlist->id);
		}
		nvgpu_channel_put(ch);
	}

	if (channels_in_use) {
		nvgpu_assert(nvgpu_runlist_reload_ids(g,
				active_runlist_ids, true) == 0);
	}

	nvgpu_log_fn(g, "done");

	return 0;
}

static void nvgpu_channel_semaphore_signal(struct nvgpu_channel *c,
		bool post_events)
{
	struct gk20a *g = c->g;

	(void)post_events;

	if (nvgpu_cond_broadcast_interruptible( &c->semaphore_wq) != 0) {
		nvgpu_warn(g, "failed to broadcast");
	}

#ifdef CONFIG_NVGPU_CHANNEL_TSG_CONTROL
	if (post_events) {
		struct nvgpu_tsg *tsg = nvgpu_tsg_from_ch(c);
		if (tsg != NULL) {
			g->ops.tsg.post_event_id(tsg,
			    NVGPU_EVENT_ID_BLOCKING_SYNC);
		}
	}
#endif

#ifdef CONFIG_NVGPU_KERNEL_MODE_SUBMIT
	/*
	 * Only non-deterministic channels get the channel_update callback. We
	 * don't allow semaphore-backed syncs for these channels anyways, since
	 * they have a dependency on the sync framework. If deterministic
	 * channels are receiving a semaphore wakeup, it must be for a
	 * user-space managed semaphore.
	 */
	if (!nvgpu_channel_is_deterministic(c)) {
		nvgpu_channel_update(c);
	}
#endif
}

void nvgpu_channel_semaphore_wakeup(struct gk20a *g, bool post_events)
{
	struct nvgpu_fifo *f = &g->fifo;
	u32 chid;

	nvgpu_log_fn(g, " ");

	/*
	 * Ensure that all pending writes are actually done  before trying to
	 * read semaphore values from DRAM.
	 */
	nvgpu_assert(g->ops.mm.cache.fb_flush(g) == 0);

	for (chid = 0; chid < f->num_channels; chid++) {
		struct nvgpu_channel *c = &g->fifo.channel[chid];
		if (nvgpu_channel_get(c) != NULL) {
			if (nvgpu_atomic_read(&c->bound) != 0) {
				nvgpu_channel_semaphore_signal(c, post_events);
			}
			nvgpu_channel_put(c);
		}
	}
}

/* return with a reference to the channel, caller must put it back */
struct nvgpu_channel *nvgpu_channel_refch_from_inst_ptr(struct gk20a *g,
			u64 inst_ptr)
{
	struct nvgpu_fifo *f = &g->fifo;
	unsigned int ci;

	if (unlikely(f->channel == NULL)) {
		return NULL;
	}
	for (ci = 0; ci < f->num_channels; ci++) {
		struct nvgpu_channel *ch;
		u64 ch_inst_ptr;

		ch = nvgpu_channel_from_id(g, ci);
		/* only alive channels are searched */
		if (ch == NULL) {
			continue;
		}

		ch_inst_ptr = nvgpu_inst_block_addr(g, &ch->inst_block);
		if (inst_ptr == ch_inst_ptr) {
			return ch;
		}

		nvgpu_channel_put(ch);
	}
	return NULL;
}

int nvgpu_channel_alloc_inst(struct gk20a *g, struct nvgpu_channel *ch)
{
	int err;

	nvgpu_log_fn(g, " ");

	err = nvgpu_alloc_inst_block(g, &ch->inst_block);
	if (err != 0) {
		return err;
	}

	nvgpu_log_info(g, "channel %d inst block physical addr: 0x%16llx",
		ch->chid, nvgpu_inst_block_addr(g, &ch->inst_block));

	nvgpu_log_fn(g, "done");
	return 0;
}

void nvgpu_channel_free_inst(struct gk20a *g, struct nvgpu_channel *ch)
{
	nvgpu_free_inst_block(g, &ch->inst_block);
}

static void nvgpu_channel_sync_debug_dump(struct gk20a *g,
	struct nvgpu_debug_context *o, struct nvgpu_channel_dump_info *info)
{
#ifdef CONFIG_NVGPU_NON_FUSA
	gk20a_debug_output(o,
			"RAMFC: TOP: %012llx PUT: %012llx GET: %012llx "
			"FETCH: %012llx "
			"HEADER: %08x COUNT: %08x "
			"SYNCPOINT: %08x %08x "
			"SEMAPHORE: %08x %08x %08x %08x",
			info->inst.pb_top_level_get,
			info->inst.pb_put,
			info->inst.pb_get,
			info->inst.pb_fetch,
			info->inst.pb_header,
			info->inst.pb_count,
			info->inst.syncpointa,
			info->inst.syncpointb,
			info->inst.semaphorea,
			info->inst.semaphoreb,
			info->inst.semaphorec,
			info->inst.semaphored);

	g->ops.pbdma.syncpt_debug_dump(g, o, info);
#else
	(void)g;
	(void)o;
	(void)info;
#endif
}

static void nvgpu_channel_info_debug_dump(struct gk20a *g,
			     struct nvgpu_debug_context *o,
			     struct nvgpu_channel_dump_info *info)
{
	/**
	 * Use gpu hw version to control the channel instance fields
	 * dump in nvgpu_channel_dump_info struct.
	 * For hw version before gv11b, dump syncpoint a/b, semaphore a/b/c/d.
	 * For hw version after gv11b, dump sem addr/payload/execute.
	 */
	u32 ver = nvgpu_safe_add_u32(g->params.gpu_arch, g->params.gpu_impl);

	gk20a_debug_output(o, "%d-%s, TSG: %u, pid %d, refs: %d, deterministic: %s, domain name: %s",
			info->chid,
			g->name,
			info->tsgid,
			info->pid,
			info->refs,
			info->deterministic ? "yes" : "no",
			info->nvs_domain_name);
	gk20a_debug_output(o, "channel status: %s in use %s %s",
			info->hw_state.enabled ? "" : "not",
			info->hw_state.status_string,
			info->hw_state.busy ? "busy" : "not busy");

	if (ver < NVGPU_GPUID_GV11B) {
		nvgpu_channel_sync_debug_dump(g, o, info);
	} else {
		gk20a_debug_output(o,
			"RAMFC: TOP: %012llx PUT: %012llx GET: %012llx "
			"FETCH: %012llx "
			"HEADER: %08x COUNT: %08x "
			"SEMAPHORE: addr %012llx "
			"payload %016llx execute %08x",
			info->inst.pb_top_level_get,
			info->inst.pb_put,
			info->inst.pb_get,
			info->inst.pb_fetch,
			info->inst.pb_header,
			info->inst.pb_count,
			info->inst.sem_addr,
			info->inst.sem_payload,
			info->inst.sem_execute);
	}

	if (info->sema.addr != 0ULL) {
		gk20a_debug_output(o, "SEMA STATE: value: 0x%08x "
				   "next_val: 0x%08x addr: 0x%010llx",
				  info->sema.value,
				  info->sema.next,
				  info->sema.addr);
	}

	gk20a_debug_output(o, " ");
}

void nvgpu_channel_debug_dump_all(struct gk20a *g,
		 struct nvgpu_debug_context *o)
{
	struct nvgpu_fifo *f = &g->fifo;
	u32 chid;
	struct nvgpu_channel_dump_info **infos;

	infos = nvgpu_kzalloc(g, sizeof(*infos) * f->num_channels);
	if (infos == NULL) {
		gk20a_debug_output(o, "cannot alloc memory for channels");
		return;
	}

	for (chid = 0U; chid < f->num_channels; chid++) {
		struct nvgpu_channel *ch = nvgpu_channel_from_id(g, chid);

		if (ch != NULL) {
			struct nvgpu_channel_dump_info *info;

			info = nvgpu_kzalloc(g, sizeof(*info));

			/*
			 * ref taken stays to below loop with
			 * successful allocs
			 */
			if (info == NULL) {
				nvgpu_channel_put(ch);
			} else {
				infos[chid] = info;
			}
		}
	}

	for (chid = 0U; chid < f->num_channels; chid++) {
		struct nvgpu_channel *ch = &f->channel[chid];
		struct nvgpu_channel_dump_info *info = infos[chid];
		struct nvgpu_tsg *tsg;
		const char *domain_name;
#ifdef CONFIG_NVGPU_SW_SEMAPHORE
		struct nvgpu_channel_sync_semaphore *sync_sema;
		struct nvgpu_hw_semaphore *hw_sema = NULL;

		if (ch->sync != NULL) {
			sync_sema = nvgpu_channel_sync_to_semaphore(ch->sync);
			if (sync_sema != NULL) {
				hw_sema = nvgpu_channel_sync_semaphore_hw_sema(
						sync_sema);
			}
		}
#endif

		/* if this info exists, the above loop took a channel ref */
		if (info == NULL) {
			continue;
		}

		tsg = nvgpu_tsg_from_ch(ch);
		info->chid = ch->chid;
		info->tsgid = ch->tsgid;
		info->pid = ch->pid;
		info->refs = nvgpu_atomic_read(&ch->ref_count);
		info->deterministic = nvgpu_channel_is_deterministic(ch);
		if (tsg) {
			if (tsg->nvs_domain) {
				domain_name = nvgpu_nvs_domain_get_name(tsg->nvs_domain);
			} else {
				domain_name = "(no domain)";
			}
		} else {
			domain_name = "(no tsg)";
		}
		(void)strncpy(info->nvs_domain_name, domain_name,
				sizeof(info->nvs_domain_name) - 1U);

#ifdef CONFIG_NVGPU_SW_SEMAPHORE
		if (hw_sema != NULL) {
			info->sema.value = nvgpu_hw_semaphore_read(hw_sema);
			info->sema.next =
				(u32)nvgpu_hw_semaphore_read_next(hw_sema);
			info->sema.addr = nvgpu_hw_semaphore_addr(hw_sema);
		}
#endif

		g->ops.channel.read_state(g, ch, &info->hw_state);
		g->ops.ramfc.capture_ram_dump(g, ch, info);

		nvgpu_channel_put(ch);
	}

	gk20a_debug_output(o, "Channel Status - chip %-5s", g->name);
	gk20a_debug_output(o, "---------------------------");
	for (chid = 0U; chid < f->num_channels; chid++) {
		struct nvgpu_channel_dump_info *info = infos[chid];

		if (info != NULL) {
			nvgpu_channel_info_debug_dump(g, o, info);
			nvgpu_kfree(g, info);
		}
	}

	nvgpu_kfree(g, infos);
}

#ifdef CONFIG_NVGPU_DEBUGGER
int nvgpu_channel_deferred_reset_engines(struct gk20a *g,
		struct nvgpu_channel *ch)
{
	unsigned long engine_id, engines = 0U;
	struct nvgpu_tsg *tsg;
	bool deferred_reset_pending;
	struct nvgpu_fifo *f = &g->fifo;
	int err = 0;

	nvgpu_mutex_acquire(&g->dbg_sessions_lock);

	nvgpu_mutex_acquire(&f->deferred_reset_mutex);
	deferred_reset_pending = g->fifo.deferred_reset_pending;
	nvgpu_mutex_release(&f->deferred_reset_mutex);

	if (!deferred_reset_pending) {
		nvgpu_mutex_release(&g->dbg_sessions_lock);
		return 0;
	}

	err = nvgpu_gr_disable_ctxsw(g);
	if (err != 0) {
		nvgpu_err(g, "failed to disable ctxsw");
		goto fail;
	}

	tsg = nvgpu_tsg_from_ch(ch);
	if (tsg != NULL) {
		engines = nvgpu_engine_get_mask_on_id(g, tsg->tsgid, true);
	} else {
		nvgpu_err(g, "chid: %d is not bound to tsg", ch->chid);
		engines = g->fifo.deferred_fault_engines;
	}

	if (engines == 0U) {
		goto clean_up;
	}

	/*
	 * If deferred reset is set for an engine, and channel is running
	 * on that engine, reset it
	 */

	for_each_set_bit(engine_id, &g->fifo.deferred_fault_engines, 32UL) {
		if ((BIT64(engine_id) & engines) != 0ULL) {
			nvgpu_engine_reset(g, (u32)engine_id);
		}
	}

	nvgpu_mutex_acquire(&f->deferred_reset_mutex);
	g->fifo.deferred_fault_engines = 0;
	g->fifo.deferred_reset_pending = false;
	nvgpu_mutex_release(&f->deferred_reset_mutex);

clean_up:
	err = nvgpu_gr_enable_ctxsw(g);
	if (err != 0) {
		nvgpu_err(g, "failed to enable ctxsw");
	}
fail:
	nvgpu_mutex_release(&g->dbg_sessions_lock);

	return err;
}
#endif
