/*
 * Copyright (c) 2018-2022, NVIDIA CORPORATION.  All rights reserved.
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

#include <nvgpu/gk20a.h>
#include <nvgpu/nvgpu_init.h>
#include <nvgpu/channel.h>
#include <nvgpu/ltc.h>
#include <nvgpu/os_sched.h>
#include <nvgpu/utils.h>
#include <nvgpu/channel.h>
#include <nvgpu/channel_sync.h>
#include <nvgpu/channel_sync_syncpt.h>
#include <nvgpu/watchdog.h>
#include <nvgpu/job.h>
#include <nvgpu/priv_cmdbuf.h>
#include <nvgpu/bug.h>
#include <nvgpu/fence.h>
#include <nvgpu/swprofile.h>
#include <nvgpu/vpr.h>
#include <nvgpu/trace.h>
#include <nvgpu/nvhost.h>
#include <nvgpu/user_fence.h>

#include <nvgpu/fifo/swprofile.h>

/*
 * We might need two extra gpfifo entries per submit - one for pre fence and
 * one for post fence.
 */
#define EXTRA_GPFIFO_ENTRIES 2U

static int nvgpu_submit_create_wait_cmd(struct nvgpu_channel *c,
		struct nvgpu_channel_fence *fence,
		struct priv_cmd_entry **wait_cmd, bool flag_sync_fence)
{
	/*
	 * A single input sync fd may contain multiple fences. The preallocated
	 * priv cmdbuf space allows exactly one per submit in the worst case.
	 * Require at most one wait for consistent deterministic submits; if
	 * there are more and no space, we'll -EAGAIN in nondeterministic mode.
	 */
	u32 max_wait_cmds = nvgpu_channel_is_deterministic(c) ?
		1U : 0U;
	int err;

	if (flag_sync_fence) {
		nvgpu_assert(fence->id <= (u32)INT_MAX);
		err = nvgpu_channel_sync_wait_fence_fd(c->sync,
			(int)fence->id, wait_cmd, max_wait_cmds);
	} else {
		struct nvgpu_channel_sync_syncpt *sync_syncpt;

		sync_syncpt = nvgpu_channel_sync_to_syncpt(c->sync);
		if (sync_syncpt != NULL) {
			err = nvgpu_channel_sync_wait_syncpt(sync_syncpt,
					fence->id, fence->value, wait_cmd);
		} else {
			nvgpu_info(c->g, "need syncpoint support");
			err = -EINVAL;
		}
	}

	return err;
}

static int nvgpu_submit_create_incr_cmd(struct nvgpu_channel *c,
		struct priv_cmd_entry **incr_cmd,
		struct nvgpu_fence_type *post_fence, bool flag_fence_get,
		bool need_wfi, bool need_sync_fence)
{
	int err;

	if (flag_fence_get) {
		err = nvgpu_channel_sync_incr_user(c->sync, incr_cmd,
				post_fence, need_wfi, need_sync_fence);
	} else {
		err = nvgpu_channel_sync_incr(c->sync, incr_cmd,
				post_fence, need_sync_fence);
	}

	return err;
}

/*
 * Handle the submit synchronization - pre-fences and post-fences.
 */
static int nvgpu_submit_prepare_syncs(struct nvgpu_channel *c,
				      struct nvgpu_channel_fence *fence,
				      struct nvgpu_channel_job *job,
				      u32 flags)
{
	struct gk20a *g = c->g;
	bool need_sync_fence;
	bool new_sync_created = false;
	int err = 0;
	bool need_wfi = (flags & NVGPU_SUBMIT_FLAGS_SUPPRESS_WFI) == 0U;
	bool flag_fence_get = (flags & NVGPU_SUBMIT_FLAGS_FENCE_GET) != 0U;
	bool flag_sync_fence = (flags & NVGPU_SUBMIT_FLAGS_SYNC_FENCE) != 0U;
	bool flag_fence_wait = (flags & NVGPU_SUBMIT_FLAGS_FENCE_WAIT) != 0U;

	if (g->aggressive_sync_destroy_thresh != 0U) {
		nvgpu_mutex_acquire(&c->sync_lock);
		if (c->sync == NULL) {
			c->sync = nvgpu_channel_sync_create(c);
			if (c->sync == NULL) {
				err = -ENOMEM;
				goto clean_up_unlock;
			}
			new_sync_created = true;
		}
		nvgpu_channel_sync_get_ref(c->sync);
	}

	if ((g->ops.channel.set_syncpt != NULL) && new_sync_created) {
		err = g->ops.channel.set_syncpt(c);
		if (err != 0) {
			goto clean_up_put_sync;
		}
	}

	/*
	 * Optionally insert syncpt/semaphore wait in the beginning of gpfifo
	 * submission when user requested.
	 */
	if (flag_fence_wait) {
		err = nvgpu_submit_create_wait_cmd(c, fence, &job->wait_cmd,
				flag_sync_fence);
		if (err != 0) {
			goto clean_up_put_sync;
		}
	}

	need_sync_fence = flag_fence_get && flag_sync_fence;

	/*
	 * Always generate an increment at the end of a GPFIFO submission. When
	 * we do job tracking, post fences are needed for various reasons even
	 * if not requested by user.
	 */
	err = nvgpu_submit_create_incr_cmd(c, &job->incr_cmd, &job->post_fence,
			flag_fence_get, need_wfi, need_sync_fence);
	if (err != 0) {
		goto clean_up_wait_cmd;
	}

	if (g->aggressive_sync_destroy_thresh != 0U) {
		nvgpu_mutex_release(&c->sync_lock);
	}
	return 0;

clean_up_wait_cmd:
	if (job->wait_cmd != NULL) {
		nvgpu_priv_cmdbuf_rollback(c->priv_cmd_q, job->wait_cmd);
	}
	job->wait_cmd = NULL;
clean_up_put_sync:
	if (g->aggressive_sync_destroy_thresh != 0U) {
		if (nvgpu_channel_sync_put_ref_and_check(c->sync)
		    && g->aggressive_sync_destroy) {
			nvgpu_channel_sync_destroy(c->sync);
		}
	}
clean_up_unlock:
	if (g->aggressive_sync_destroy_thresh != 0U) {
		nvgpu_mutex_release(&c->sync_lock);
	}
	return err;
}

static void nvgpu_submit_append_priv_cmdbuf(struct nvgpu_channel *c,
		struct priv_cmd_entry *cmd)
{
	struct gk20a *g = c->g;
	struct nvgpu_mem *gpfifo_mem = &c->gpfifo.mem;
	struct nvgpu_gpfifo_entry gpfifo_entry;
	u64 gva;
	u32 size;

	nvgpu_priv_cmdbuf_finish(g, cmd, &gva, &size);
	g->ops.pbdma.format_gpfifo_entry(g, &gpfifo_entry, gva, size);

	nvgpu_mem_wr_n(g, gpfifo_mem,
			c->gpfifo.put * (u32)sizeof(gpfifo_entry),
			&gpfifo_entry, (u32)sizeof(gpfifo_entry));

	c->gpfifo.put = (c->gpfifo.put + 1U) & (c->gpfifo.entry_num - 1U);
}

static int nvgpu_submit_append_gpfifo_user_direct(struct nvgpu_channel *c,
		struct nvgpu_gpfifo_userdata userdata,
		u32 num_entries)
{
	struct gk20a *g = c->g;
	struct nvgpu_gpfifo_entry *gpfifo_cpu = c->gpfifo.mem.cpu_va;
	u32 gpfifo_size = c->gpfifo.entry_num;
	u32 len = num_entries;
	u32 start = c->gpfifo.put;
	u32 end = start + len; /* exclusive */
	int err;

	nvgpu_speculation_barrier();
	if (end > gpfifo_size) {
		/* wrap-around */
		u32 length0 = gpfifo_size - start;
		u32 length1 = len - length0;

		err = g->os_channel.copy_user_gpfifo(
				&gpfifo_cpu[start], userdata,
				0, length0);
		if (err != 0) {
			return err;
		}

		err = g->os_channel.copy_user_gpfifo(
				gpfifo_cpu, userdata,
				length0, length1);
		if (err != 0) {
			return err;
		}
	} else {
		err = g->os_channel.copy_user_gpfifo(
				&gpfifo_cpu[start], userdata,
				0, len);
		if (err != 0) {
			return err;
		}
	}

	return 0;
}

static void nvgpu_submit_append_gpfifo_common(struct nvgpu_channel *c,
		struct nvgpu_gpfifo_entry *src, u32 num_entries)
{
	struct gk20a *g = c->g;
	struct nvgpu_mem *gpfifo_mem = &c->gpfifo.mem;
	/* in bytes */
	u32 gpfifo_size =
		c->gpfifo.entry_num * (u32)sizeof(struct nvgpu_gpfifo_entry);
	u32 len = num_entries * (u32)sizeof(struct nvgpu_gpfifo_entry);
	u32 start = c->gpfifo.put * (u32)sizeof(struct nvgpu_gpfifo_entry);
	u32 end = start + len; /* exclusive */

	if (end > gpfifo_size) {
		/* wrap-around */
		u32 length0 = gpfifo_size - start;
		u32 length1 = len - length0;
		struct nvgpu_gpfifo_entry *src2 = &src[length0];

		nvgpu_mem_wr_n(g, gpfifo_mem, start, src, length0);
		nvgpu_mem_wr_n(g, gpfifo_mem, 0, src2, length1);
	} else {
		nvgpu_mem_wr_n(g, gpfifo_mem, start, src, len);
	}
}

/*
 * Copy source gpfifo entries into the gpfifo ring buffer, potentially
 * splitting into two memcpys to handle wrap-around.
 */
static int nvgpu_submit_append_gpfifo(struct nvgpu_channel *c,
		struct nvgpu_gpfifo_entry *kern_gpfifo,
		struct nvgpu_gpfifo_userdata userdata,
		u32 num_entries)
{
	int err;

#ifdef CONFIG_NVGPU_DGPU
	if ((kern_gpfifo == NULL) && (c->gpfifo.pipe == NULL)) {
#else
	if (kern_gpfifo == NULL) {
#endif
		/*
		 * This path (from userspace to sysmem) is special in order to
		 * avoid two copies unnecessarily (from user to pipe, then from
		 * pipe to gpu sysmem buffer).
		 */
		err = nvgpu_submit_append_gpfifo_user_direct(c, userdata,
				num_entries);
		if (err != 0) {
			return err;
		}
	}
#ifdef CONFIG_NVGPU_DGPU
	else if (kern_gpfifo == NULL) {
		/* from userspace to vidmem, use the common path */
		err = c->g->os_channel.copy_user_gpfifo(c->gpfifo.pipe,
				userdata, 0, num_entries);
		if (err != 0) {
			return err;
		}

		nvgpu_submit_append_gpfifo_common(c, c->gpfifo.pipe,
				num_entries);
	}
#endif
	else {
		/* from kernel to either sysmem or vidmem, don't need
		 * copy_user_gpfifo so use the common path */
		nvgpu_submit_append_gpfifo_common(c, kern_gpfifo, num_entries);
	}

	trace_write_pushbuffers(c, num_entries);

	c->gpfifo.put = (c->gpfifo.put + num_entries) &
		(c->gpfifo.entry_num - 1U);

	return 0;
}

static int nvgpu_submit_prepare_gpfifo_track(struct nvgpu_channel *c,
		struct nvgpu_gpfifo_entry *gpfifo,
		struct nvgpu_gpfifo_userdata userdata,
		u32 num_entries,
		u32 flags,
		struct nvgpu_channel_fence *fence,
		struct nvgpu_fence_type **fence_out,
		struct nvgpu_swprofiler *profiler,
		bool need_deferred_cleanup)
{
	bool skip_buffer_refcounting = (flags &
			NVGPU_SUBMIT_FLAGS_SKIP_BUFFER_REFCOUNTING) != 0U;
	struct nvgpu_channel_job *job = NULL;
	int err;

	nvgpu_channel_joblist_lock(c);
	err = nvgpu_channel_alloc_job(c, &job);
	nvgpu_channel_joblist_unlock(c);
	if (err != 0) {
		return err;
	}

	err = nvgpu_submit_prepare_syncs(c, fence, job, flags);
	if (err != 0) {
		goto clean_up_job;
	}

	nvgpu_swprofile_snapshot(profiler, PROF_KICKOFF_JOB_TRACKING);

	/*
	 * wait_cmd can be unset even if flag_fence_wait exists; the
	 * android sync framework for example can provide entirely
	 * empty fences that act like trivially expired waits.
	 */
	if (job->wait_cmd != NULL) {
		nvgpu_submit_append_priv_cmdbuf(c, job->wait_cmd);
	}

	err = nvgpu_submit_append_gpfifo(c, gpfifo, userdata, num_entries);
	if (err != 0) {
		goto clean_up_gpfifo_wait;
	}

	nvgpu_submit_append_priv_cmdbuf(c, job->incr_cmd);

	err = nvgpu_channel_add_job(c, job, skip_buffer_refcounting);
	if (err != 0) {
		goto clean_up_gpfifo_incr;
	}

	nvgpu_channel_sync_mark_progress(c->sync, need_deferred_cleanup);

	if (fence_out != NULL) {
		/* This fence ref is going somewhere else but it's owned by the
		 * job; the caller is expected to release it promptly, so that
		 * a subsequent job cannot reclaim its memory.
		 */
		*fence_out = nvgpu_fence_get(&job->post_fence);
	}

	return 0;

clean_up_gpfifo_incr:
	/*
	 * undo the incr priv cmdbuf and the user entries:
	 * new gp.put =
	 * (gp.put - (1 + num_entries)) & (gp.entry_num - 1) =
	 * (gp.put + (gp.entry_num - (1 + num_entries))) & (gp.entry_num - 1)
	 * the + entry_num does not affect the result but avoids wrapping below
	 * zero for MISRA, although it would be well defined.
	 */
	c->gpfifo.put =
		(nvgpu_safe_add_u32(c->gpfifo.put,
		  nvgpu_safe_sub_u32(c->gpfifo.entry_num,
		    nvgpu_safe_add_u32(1U, num_entries)))) &
		nvgpu_safe_sub_u32(c->gpfifo.entry_num, 1U);
clean_up_gpfifo_wait:
	if (job->wait_cmd != NULL) {
		/*
		 * undo the wait priv cmdbuf entry:
		 * gp.put =
		 * (gp.put - 1) & (gp.entry_num - 1) =
		 * (gp.put + (gp.entry_num - 1)) & (gp.entry_num - 1)
		 * same as above with the gp.entry_num on the left side.
		 */
		c->gpfifo.put =
			nvgpu_safe_add_u32(c->gpfifo.put,
			  nvgpu_safe_sub_u32(c->gpfifo.entry_num, 1U)) &
			nvgpu_safe_sub_u32(c->gpfifo.entry_num, 1U);
	}
	nvgpu_fence_put(&job->post_fence);
	nvgpu_priv_cmdbuf_rollback(c->priv_cmd_q, job->incr_cmd);
	if (job->wait_cmd != NULL) {
		nvgpu_priv_cmdbuf_rollback(c->priv_cmd_q, job->wait_cmd);
	}
clean_up_job:
	nvgpu_channel_free_job(c, job);
	return err;
}

static int nvgpu_submit_prepare_gpfifo_notrack(struct nvgpu_channel *c,
		struct nvgpu_gpfifo_entry *gpfifo,
		struct nvgpu_gpfifo_userdata userdata,
		u32 num_entries,
		struct nvgpu_fence_type **fence_out,
		struct nvgpu_swprofiler *profiler)
{
	int err;

	nvgpu_swprofile_snapshot(profiler, PROF_KICKOFF_JOB_TRACKING);

	err = nvgpu_submit_append_gpfifo(c, gpfifo, userdata,
			num_entries);
	if (err != 0) {
		return err;
	}

	if (fence_out != NULL) {
		*fence_out = NULL;
	}

	return 0;
}

static int check_gpfifo_capacity(struct nvgpu_channel *c, u32 required)
{
	/*
	 * Make sure we have enough space for gpfifo entries. Check cached
	 * values first and then read from HW. If no space, return -EAGAIN
	 * and let userpace decide to re-try request or not.
	 */
	if (nvgpu_channel_get_gpfifo_free_count(c) < required) {
		if (nvgpu_channel_update_gpfifo_get_and_get_free_count(c) <
				required) {
			return -EAGAIN;
		}
	}

	return 0;
}

static int nvgpu_do_submit(struct nvgpu_channel *c,
		struct nvgpu_gpfifo_entry *gpfifo,
		struct nvgpu_gpfifo_userdata userdata,
		u32 num_entries,
		u32 flags,
		struct nvgpu_channel_fence *fence,
		struct nvgpu_fence_type **fence_out,
		struct nvgpu_swprofiler *profiler,
		bool need_job_tracking,
		bool need_deferred_cleanup)
{
	struct gk20a *g = c->g;
	int err;

#ifdef CONFIG_NVGPU_TRACE
	trace_gk20a_channel_submit_gpfifo(g->name,
					  c->chid,
					  num_entries,
					  flags,
					  fence ? fence->id : 0,
					  fence ? fence->value : 0);
#endif

	nvgpu_log_info(g, "pre-submit put %d, get %d, size %d",
		c->gpfifo.put, c->gpfifo.get, c->gpfifo.entry_num);

	err = check_gpfifo_capacity(c, num_entries + EXTRA_GPFIFO_ENTRIES);
	if (err != 0) {
		return err;
	}

	if (need_job_tracking) {
		err = nvgpu_submit_prepare_gpfifo_track(c, gpfifo,
				userdata, num_entries, flags, fence,
				fence_out, profiler, need_deferred_cleanup);
	} else {
		err = nvgpu_submit_prepare_gpfifo_notrack(c, gpfifo,
				userdata, num_entries, fence_out, profiler);
	}

	if (err != 0) {
		return err;
	}

	nvgpu_swprofile_snapshot(profiler, PROF_KICKOFF_APPEND);

	g->ops.userd.gp_put(g, c);

	return 0;
}

#ifdef CONFIG_NVGPU_DETERMINISTIC_CHANNELS
static int nvgpu_submit_deterministic(struct nvgpu_channel *c,
				struct nvgpu_gpfifo_entry *gpfifo,
				struct nvgpu_gpfifo_userdata userdata,
				u32 num_entries,
				u32 flags,
				struct nvgpu_channel_fence *fence,
				struct nvgpu_fence_type **fence_out,
				struct nvgpu_swprofiler *profiler)
{
	bool skip_buffer_refcounting = (flags &
			NVGPU_SUBMIT_FLAGS_SKIP_BUFFER_REFCOUNTING) != 0U;
	bool flag_fence_wait = (flags & NVGPU_SUBMIT_FLAGS_FENCE_WAIT) != 0U;
	bool flag_fence_get = (flags & NVGPU_SUBMIT_FLAGS_FENCE_GET) != 0U;
	bool flag_sync_fence = (flags & NVGPU_SUBMIT_FLAGS_SYNC_FENCE) != 0U;
	struct gk20a *g = c->g;
	bool need_job_tracking;
	int err = 0;

	nvgpu_assert(nvgpu_channel_is_deterministic(c));

	/* sync framework on post fences would not be deterministic */
	if (flag_fence_get && flag_sync_fence) {
		nvgpu_info(g, "can't use sync fence in deterministic mode");
		return -EINVAL;
	}

	/* this would be O(n) */
	if (!skip_buffer_refcounting) {
		nvgpu_info(g, "can't use buf refcounting in deterministic mode");
		return -EINVAL;
	}

	/* the watchdog needs periodic job cleanup */
	if (nvgpu_channel_wdt_enabled(c->wdt)) {
		nvgpu_info(g, "can't use watchdog in deterministic mode");
		return -EINVAL;
	}

	/*
	 * Job tracking is necessary on deterministic channels if and only if
	 * pre- or post-fence functionality is needed. If not, a fast submit
	 * can be done (ie. only need to write out userspace GPFIFO entries and
	 * update GP_PUT).
	 */
	need_job_tracking = flag_fence_wait || flag_fence_get;

	if (need_job_tracking) {
		/* nvgpu_semaphore is dynamically allocated, not pooled */
		if (!nvgpu_has_syncpoints(g)) {
			nvgpu_info(g, "can't use sema tracking in deterministic mode");
			return -EINVAL;
		}

		/* dynamic sync allocation wouldn't be deterministic */
		if (g->aggressive_sync_destroy_thresh != 0U) {
			nvgpu_info(g, "can't use dynamic syncs in deterministic mode");
			return -EINVAL;
		}

		/*
		 * (Try to) clean up a single job, if available. Each job
		 * requires the same amount of metadata, so this is enough for
		 * the job list, fence pool, and private command buffers that
		 * this submit will need.
		 *
		 * This submit might still need more gpfifo space than what the
		 * previous has used. The job metadata doesn't look at it
		 * though - the hw GP_GET pointer can be much further away than
		 * our metadata pointers; gpfifo space is "freed" by the HW.
		 */
		nvgpu_channel_clean_up_deterministic_job(c);
	}

	/* Grab access to HW to deal with do_idle */
	nvgpu_rwsem_down_read(&g->deterministic_busy);

	if (c->deterministic_railgate_allowed) {
		/*
		 * Nope - this channel has dropped its own power ref. As
		 * deterministic submits don't hold power on per each submitted
		 * job like normal ones do, the GPU might railgate any time now
		 * and thus submit is disallowed.
		 */
		nvgpu_info(g, "can't submit on dormant deterministic channel");
		err = -EINVAL;
		goto clean_up;
	}

	err = nvgpu_do_submit(c, gpfifo, userdata, num_entries, flags, fence,
			fence_out, profiler, need_job_tracking, false);
	if (err != 0) {
		goto clean_up;
	}

	/* No hw access beyond this point */
	nvgpu_rwsem_up_read(&g->deterministic_busy);

	return 0;

clean_up:
	nvgpu_log_fn(g, "fail %d", err);
	nvgpu_rwsem_up_read(&g->deterministic_busy);

	return err;
}
#endif

static int nvgpu_submit_nondeterministic(struct nvgpu_channel *c,
				struct nvgpu_gpfifo_entry *gpfifo,
				struct nvgpu_gpfifo_userdata userdata,
				u32 num_entries,
				u32 flags,
				struct nvgpu_channel_fence *fence,
				struct nvgpu_fence_type **fence_out,
				struct nvgpu_swprofiler *profiler)
{
	bool skip_buffer_refcounting = (flags &
			NVGPU_SUBMIT_FLAGS_SKIP_BUFFER_REFCOUNTING) != 0U;
	bool flag_fence_wait = (flags & NVGPU_SUBMIT_FLAGS_FENCE_WAIT) != 0U;
	bool flag_fence_get = (flags & NVGPU_SUBMIT_FLAGS_FENCE_GET) != 0U;
	struct gk20a *g = c->g;
	bool need_job_tracking;
	int err = 0;

	nvgpu_assert(!nvgpu_channel_is_deterministic(c));

	/*
	 * Job tracking is necessary for any of the following conditions on
	 * non-deterministic channels:
	 *  - pre- or post-fence functionality
	 *  - GPU rail-gating
	 *  - VPR resize enabled
	 *  - buffer refcounting
	 *  - channel watchdog
	 *
	 * If none of the conditions are met, then job tracking is not
	 * required and a fast submit can be done (ie. only need to write
	 * out userspace GPFIFO entries and update GP_PUT).
	 */
	need_job_tracking = flag_fence_wait ||
			flag_fence_get ||
			nvgpu_is_enabled(g, NVGPU_CAN_RAILGATE) ||
			nvgpu_is_vpr_resize_enabled() ||
			!skip_buffer_refcounting ||
			nvgpu_channel_wdt_enabled(c->wdt);

	if (need_job_tracking) {
		/*
		 * Get a power ref because this isn't a deterministic
		 * channel that holds them during the channel lifetime.
		 * This one is released by nvgpu_channel_clean_up_jobs,
		 * via syncpt or sema interrupt, whichever is used.
		 */
		err = gk20a_busy(g);
		if (err != 0) {
			nvgpu_err(g,
				"failed to host gk20a to submit gpfifo");
			nvgpu_print_current(g, NULL, NVGPU_ERROR);
			return err;
		}
	}

	err = nvgpu_do_submit(c, gpfifo, userdata, num_entries, flags, fence,
			fence_out, profiler, need_job_tracking, true);
	if (err != 0) {
		goto clean_up;
	}

	return 0;

clean_up:
	nvgpu_log_fn(g, "fail %d", err);
	gk20a_idle(g);

	return err;
}

static int check_submit_allowed(struct nvgpu_channel *c)
{
	struct gk20a *g = c->g;

	if (nvgpu_is_enabled(g, NVGPU_DRIVER_IS_DYING)) {
		nvgpu_info(g, "can't submit, driver dying");
		return -ENODEV;
	}

	if (nvgpu_channel_check_unserviceable(c)) {
		nvgpu_info(g, "can't submit, channel is unserviceable");
		return -ETIMEDOUT;
	}

	if (c->usermode_submit_enabled) {
		nvgpu_info(g, "can't submit, user mode only");
		return -EINVAL;
	}

	if (!nvgpu_mem_is_valid(&c->gpfifo.mem)) {
		nvgpu_info(g, "can't submit without gpfifo");
		return -ENOMEM;
	}

	/* an address space needs to have been bound at this point. */
	if (!nvgpu_channel_as_bound(c)) {
		nvgpu_err(g,
			    "not bound to an address space at time of gpfifo"
			    " submission.");
		return -EINVAL;
	}

	return 0;
}

static int nvgpu_submit_channel_gpfifo(struct nvgpu_channel *c,
				struct nvgpu_gpfifo_entry *gpfifo,
				struct nvgpu_gpfifo_userdata userdata,
				u32 num_entries,
				u32 flags,
				struct nvgpu_channel_fence *fence,
				struct nvgpu_fence_type **fence_out,
				struct nvgpu_swprofiler *profiler)
{
	struct gk20a *g = c->g;
	int err;

	err = check_submit_allowed(c);
	if (err != 0) {
		return err;
	}

	/*
	 * Fifo not large enough for request. Return error immediately.
	 * Kernel can insert gpfifo entries before and after user gpfifos.
	 * So, add extra entries in user request. Also, HW with fifo size N
	 * can accept only N-1 entries.
	 */
	if (c->gpfifo.entry_num - 1U < num_entries + EXTRA_GPFIFO_ENTRIES) {
		nvgpu_err(g, "not enough gpfifo space allocated");
		return -ENOMEM;
	}

	nvgpu_swprofile_snapshot(profiler, PROF_KICKOFF_ENTRY);

	/* update debug settings */
	nvgpu_ltc_sync_enabled(g);

	nvgpu_log_info(g, "channel %d", c->chid);

#ifdef CONFIG_NVGPU_DETERMINISTIC_CHANNELS
	if (c->deterministic) {
		err = nvgpu_submit_deterministic(c, gpfifo, userdata,
				num_entries, flags, fence, fence_out, profiler);
	} else
#endif
	{
		err = nvgpu_submit_nondeterministic(c, gpfifo, userdata,
				num_entries, flags, fence, fence_out, profiler);
	}

	if (err != 0) {
		return err;
	}

#ifdef CONFIG_NVGPU_TRACE
	if (fence_out != NULL && *fence_out != NULL) {
		/*
		 * This is not a good example on how to use the fence type.
		 * Don't touch the priv data. The debug trace is special.
		 */
#ifdef CONFIG_TEGRA_GK20A_NVHOST
		trace_gk20a_channel_submitted_gpfifo(g->name,
					c->chid, num_entries, flags,
					(*fence_out)->priv.syncpt_id,
					(*fence_out)->priv.syncpt_value);
#else
		trace_gk20a_channel_submitted_gpfifo(g->name,
					c->chid, num_entries, flags,
					0, 0);
#endif
	} else {
		trace_gk20a_channel_submitted_gpfifo(g->name,
					c->chid, num_entries, flags,
					0, 0);
	}
#endif

	nvgpu_log_info(g, "post-submit put %d, get %d, size %d",
		c->gpfifo.put, c->gpfifo.get, c->gpfifo.entry_num);

	nvgpu_swprofile_snapshot(profiler, PROF_KICKOFF_END);

	nvgpu_log_fn(g, "done");
	return err;
}

int nvgpu_submit_channel_gpfifo_user(struct nvgpu_channel *c,
				struct nvgpu_gpfifo_userdata userdata,
				u32 num_entries,
				u32 flags,
				struct nvgpu_channel_fence *fence,
				struct nvgpu_user_fence *fence_out,
				struct nvgpu_swprofiler *profiler)
{
	struct nvgpu_fence_type *fence_internal = NULL;
	int err;

	err = nvgpu_submit_channel_gpfifo(c, NULL, userdata, num_entries,
			flags, fence, &fence_internal, profiler);
	if (err == 0 && fence_internal != NULL) {
		*fence_out = nvgpu_fence_extract_user(fence_internal);
		nvgpu_fence_put(fence_internal);
	}
	return err;
}

int nvgpu_submit_channel_gpfifo_kernel(struct nvgpu_channel *c,
				struct nvgpu_gpfifo_entry *gpfifo,
				u32 num_entries,
				u32 flags,
				struct nvgpu_channel_fence *fence,
				struct nvgpu_fence_type **fence_out)
{
	struct nvgpu_gpfifo_userdata userdata = { NULL, NULL };

	return nvgpu_submit_channel_gpfifo(c, gpfifo, userdata, num_entries,
			flags, fence, fence_out, NULL);
}
