/*
 * Copyright (c) 2014-2022, NVIDIA CORPORATION.  All rights reserved.
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

#include <nvgpu/bug.h>
#include <nvgpu/debug.h>
#include <nvgpu/kmem.h>
#include <nvgpu/log.h>
#include <nvgpu/os_sched.h>
#include <nvgpu/channel.h>
#include <nvgpu/tsg.h>
#include <nvgpu/rc.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/error_notifier.h>
#include <nvgpu/gr/config.h>
#include <nvgpu/gr/ctx.h>
#include <nvgpu/runlist.h>
#include <nvgpu/nvs.h>
#include <nvgpu/static_analysis.h>
#include <nvgpu/nvgpu_init.h>
#ifdef CONFIG_NVGPU_PROFILER
#include <nvgpu/profiler.h>
#endif

void nvgpu_tsg_disable(struct nvgpu_tsg *tsg)
{
	struct gk20a *g = tsg->g;
	struct nvgpu_channel *ch;

	nvgpu_rwsem_down_read(&tsg->ch_list_lock);
	nvgpu_list_for_each_entry(ch, &tsg->ch_list, nvgpu_channel, ch_entry) {
		g->ops.channel.disable(ch);
	}
	nvgpu_rwsem_up_read(&tsg->ch_list_lock);
}

struct nvgpu_tsg *nvgpu_tsg_check_and_get_from_id(struct gk20a *g, u32 tsgid)
{
	struct nvgpu_fifo *f = &g->fifo;

	if (tsgid >= f->num_channels) {
		return NULL;
	}

	return nvgpu_tsg_get_from_id(g, tsgid);
}


struct nvgpu_tsg *nvgpu_tsg_get_from_id(struct gk20a *g, u32 tsgid)
{
	struct nvgpu_fifo *f = &g->fifo;

	return &f->tsg[tsgid];
}

/*
 * API to mark channel as part of TSG
 *
 * Note that channel is not runnable when we bind it to TSG
 */
int nvgpu_tsg_bind_channel(struct nvgpu_tsg *tsg, struct nvgpu_channel *ch)
{
	struct gk20a *g = ch->g;
	u32 max_ch_per_tsg;
	int err = 0;

	nvgpu_log_fn(g, "bind tsg:%u ch:%u\n", tsg->tsgid, ch->chid);

	/* check if channel is already bound to some TSG */
	if (nvgpu_tsg_from_ch(ch) != NULL) {
		return -EINVAL;
	}

	/*
	 * This runlist domain is set either by default or in an explicit
	 * bind. If the default domain has been deleted, explicit bind is
	 * mandatory.
	 */
	if (tsg->rl_domain == NULL) {
		return -EINVAL;
	}

	/* cannot bind more channels than MAX channels supported per TSG */
	nvgpu_rwsem_down_read(&tsg->ch_list_lock);
	max_ch_per_tsg = g->ops.runlist.get_max_channels_per_tsg();
	if (tsg->ch_count == max_ch_per_tsg) {
		nvgpu_rwsem_up_read(&tsg->ch_list_lock);
		nvgpu_warn(g, "TSG %u trying to bind more than supported channels (%u)",
			tsg->tsgid, max_ch_per_tsg);
		return -EINVAL;
	}
	nvgpu_rwsem_up_read(&tsg->ch_list_lock);

	/* Use runqueue selector 1 for all ASYNC ids */
	if (ch->subctx_id > CHANNEL_INFO_VEID0) {
		ch->runqueue_sel = 1;
	}

	/*
	 * All the channels in a TSG must share the same runlist.
	 */
	if (tsg->runlist == NULL) {
		tsg->runlist = ch->runlist;
		/*
		 * The rl domain identifier is stashed in tsg->rl_domain->name
		 * when the tsg is bound to a domain, but at that point there
		 * are no channels yet to describe which runlist id should be
		 * used. Now we know.
		 */
		tsg->rl_domain = nvgpu_rl_domain_get(g, tsg->runlist->id, tsg->rl_domain->name);
		WARN_ON(tsg->rl_domain == NULL);
	} else {
		if (tsg->runlist != ch->runlist) {
			nvgpu_err(tsg->g,
				  "runlist_id mismatch ch[%d] tsg[%d]",
				  ch->runlist->id,
				  tsg->runlist->id);
			return -EINVAL;
		}
	}

	if (g->ops.tsg.bind_channel != NULL) {
		err = g->ops.tsg.bind_channel(tsg, ch);
		if (err != 0) {
			nvgpu_err(tsg->g, "fail to bind ch %u to tsg %u",
				ch->chid, tsg->tsgid);
			return err;
		}
	}

	nvgpu_rwsem_down_write(&tsg->ch_list_lock);
	nvgpu_list_add_tail(&ch->ch_entry, &tsg->ch_list);
	tsg->ch_count = nvgpu_safe_add_u32(tsg->ch_count, 1U);
	ch->tsgid = tsg->tsgid;
	/* channel is serviceable after it is bound to tsg */
	ch->unserviceable = false;
	nvgpu_rwsem_up_write(&tsg->ch_list_lock);

	if (g->ops.tsg.bind_channel_eng_method_buffers != NULL) {
		g->ops.tsg.bind_channel_eng_method_buffers(tsg, ch);
	}

	nvgpu_ref_get(&tsg->refcount);

	return err;
}

#ifdef CONFIG_NVS_PRESENT
int nvgpu_tsg_bind_domain(struct nvgpu_tsg *tsg, struct nvgpu_nvs_domain *nnvs_domain)
{
	struct nvgpu_runlist_domain *rl_domain;
	struct gk20a *g = tsg->g;
	const char *name;

	/* Hopping channels from one domain to another is not allowed */
	if (tsg->num_active_channels != 0U) {
		return -EINVAL;
	}

	name = nvgpu_nvs_domain_get_name(nnvs_domain);

	/*
	 * The domain ptr will get updated with the right id once the runlist
	 * gets specified based on the first channel.
	 */
	rl_domain = nvgpu_rl_domain_get(g, 0, name);
	if (rl_domain == NULL) {
		nvgpu_err(g, "rl domain not found (%s)", name);
		/*
		 * This shouldn't happen because the nvs domain guarantees RL domains.
		 *
		 * TODO: query this via the nvs domain.
		 */
		return -ENOENT;
	}

	/* Release the default domain ref that was implicitly taken at open */
	if (tsg->nvs_domain != NULL) {
		nvgpu_nvs_domain_put(g, tsg->nvs_domain);
	}

	nvgpu_nvs_domain_get(g, nnvs_domain);
	tsg->rl_domain = rl_domain;
	tsg->nvs_domain = nnvs_domain;

	return 0;
}
#endif

static bool nvgpu_tsg_is_multi_channel(struct nvgpu_tsg *tsg)
{
	bool ret = false;

	nvgpu_rwsem_down_read(&tsg->ch_list_lock);
	if (nvgpu_list_first_entry(&tsg->ch_list, nvgpu_channel,
				   ch_entry) !=
	    nvgpu_list_last_entry(&tsg->ch_list, nvgpu_channel,
				   ch_entry)) {
		ret = true;
	}
	nvgpu_rwsem_up_read(&tsg->ch_list_lock);

	return ret;
}

static int nvgpu_tsg_unbind_channel_common(struct nvgpu_tsg *tsg,
		struct nvgpu_channel *ch)
{
	struct gk20a *g = ch->g;
	int err;
	bool tsg_timedout;

	/* If one channel in TSG times out, we disable all channels */
	nvgpu_rwsem_down_write(&tsg->ch_list_lock);
	tsg_timedout = nvgpu_channel_check_unserviceable(ch);
	nvgpu_rwsem_up_write(&tsg->ch_list_lock);

	/* Disable TSG and examine status before unbinding channel */
	g->ops.tsg.disable(tsg);

	err = g->ops.fifo.preempt_tsg(g, tsg);
	if (err != 0) {
		goto fail_enable_tsg;
	}

	/*
	 * State validation is only necessary if there are multiple channels in
         * the TSG.
	 */
	if (nvgpu_tsg_is_multi_channel(tsg) && !tsg_timedout &&
	    (g->ops.tsg.unbind_channel_check_hw_state != NULL)) {
		err = g->ops.tsg.unbind_channel_check_hw_state(tsg, ch);
		if (err == -EAGAIN) {
			goto fail_enable_tsg;
		}

		if (err != 0) {
			nvgpu_err(g, "invalid hw_state for ch %u", ch->chid);
			goto fail_enable_tsg;
		}
	}

	if (g->ops.channel.clear != NULL) {
		g->ops.channel.clear(ch);
	}

	/* Channel should be seen as TSG channel while updating runlist */
	err = nvgpu_channel_update_runlist(ch, false);
	if (err != 0) {
		nvgpu_err(g, "update runlist failed ch:%u tsg:%u",
				ch->chid, tsg->tsgid);
		goto fail_enable_tsg;
	}

#ifdef CONFIG_NVGPU_DEBUGGER
	while (ch->mmu_debug_mode_refcnt > 0U) {
		err = nvgpu_tsg_set_mmu_debug_mode(ch, false);
		if (err != 0) {
			nvgpu_err(g, "disable mmu debug mode failed ch:%u",
				ch->chid);
			break;
		}
	}
#endif

	/* Remove channel from TSG and re-enable rest of the channels */
	nvgpu_rwsem_down_write(&tsg->ch_list_lock);
	nvgpu_list_del(&ch->ch_entry);
	tsg->ch_count = nvgpu_safe_sub_u32(tsg->ch_count, 1U);
	ch->tsgid = NVGPU_INVALID_TSG_ID;

	/* another thread could have re-enabled the channel because it was
	 * still on the list at that time, so make sure it's truly disabled
	 */
	g->ops.channel.disable(ch);
	nvgpu_rwsem_up_write(&tsg->ch_list_lock);

	/*
	 * Don't re-enable all channels if TSG has timed out already
	 *
	 * Note that we can skip disabling and preempting TSG too in case of
	 * time out, but we keep that to ensure TSG is kicked out
	 */
	if (!tsg_timedout) {
		g->ops.tsg.enable(tsg);
	}

	if (g->ops.channel.abort_clean_up != NULL) {
		g->ops.channel.abort_clean_up(ch);
	}

	return 0;

fail_enable_tsg:
	if (!tsg_timedout) {
		g->ops.tsg.enable(tsg);
	}
	return err;
}

/* The caller must ensure that channel belongs to a tsg */
int nvgpu_tsg_unbind_channel(struct nvgpu_tsg *tsg, struct nvgpu_channel *ch,
			     bool force)
{
	struct gk20a *g = ch->g;
	int err;

	nvgpu_log_fn(g, "unbind tsg:%u ch:%u\n", tsg->tsgid, ch->chid);

	err = nvgpu_tsg_unbind_channel_common(tsg, ch);
	if (!force && err == -EAGAIN) {
		return err;
	}

	if (err != 0) {
		nvgpu_err(g, "unbind common failed, err=%d", err);
		goto fail_common;
	}

	if (g->ops.tsg.unbind_channel != NULL) {
		err = g->ops.tsg.unbind_channel(tsg, ch);
		if (err != 0) {
			/*
			 * ch already removed from TSG's list.
			 * mark error explicitly.
			 */
			(void) nvgpu_channel_mark_error(g, ch);
			nvgpu_err(g, "unbind hal failed, err=%d", err);
			goto fail;
		}
	}

	nvgpu_ref_put(&tsg->refcount, nvgpu_tsg_release);

	return 0;

fail_common:
	if (g->ops.tsg.unbind_channel != NULL) {
		int unbind_err = g->ops.tsg.unbind_channel(tsg, ch);
		if (unbind_err != 0) {
			nvgpu_err(g, "unbind hal failed, err=%d", unbind_err);
		}
	}
fail:
	nvgpu_err(g, "Channel %d unbind failed, tearing down TSG %d",
		ch->chid, tsg->tsgid);

	nvgpu_tsg_abort(g, tsg, true);

	if (g->ops.channel.clear != NULL) {
		g->ops.channel.clear(ch);
	}

	/* If channel unbind fails, channel is still part of runlist */
	if (nvgpu_channel_update_runlist(ch, false) != 0) {
		nvgpu_err(g, "remove ch %u from runlist failed", ch->chid);
	}

#ifdef CONFIG_NVGPU_DEBUGGER
	while (ch->mmu_debug_mode_refcnt > 0U) {
		err = nvgpu_tsg_set_mmu_debug_mode(ch, false);
		if (err != 0) {
			nvgpu_err(g, "disable mmu debug mode failed ch:%u",
				ch->chid);
			break;
		}
	}
#endif

	nvgpu_rwsem_down_write(&tsg->ch_list_lock);
	nvgpu_list_del(&ch->ch_entry);
	ch->tsgid = NVGPU_INVALID_TSG_ID;
	nvgpu_rwsem_up_write(&tsg->ch_list_lock);

	nvgpu_ref_put(&tsg->refcount, nvgpu_tsg_release);

	return err;

}

int nvgpu_tsg_unbind_channel_check_hw_state(struct nvgpu_tsg *tsg,
		struct nvgpu_channel *ch)
{
	struct gk20a *g = ch->g;
	struct nvgpu_channel_hw_state hw_state;
	int err = 0;

	nvgpu_rwsem_down_read(&tsg->ch_list_lock);
	g->ops.channel.read_state(g, ch, &hw_state);
	nvgpu_rwsem_up_read(&tsg->ch_list_lock);

	if (g->ops.tsg.unbind_channel_check_hw_next != NULL) {
		err = g->ops.tsg.unbind_channel_check_hw_next(ch, &hw_state);
	}

	if (g->ops.tsg.unbind_channel_check_ctx_reload != NULL) {
		g->ops.tsg.unbind_channel_check_ctx_reload(tsg, ch, &hw_state);
	}

	if (g->ops.tsg.unbind_channel_check_eng_faulted != NULL) {
		g->ops.tsg.unbind_channel_check_eng_faulted(tsg, ch,
				&hw_state);
	}

	return err;
}

void nvgpu_tsg_unbind_channel_check_ctx_reload(struct nvgpu_tsg *tsg,
	struct nvgpu_channel *ch,
	struct nvgpu_channel_hw_state *hw_state)
{
	struct gk20a *g = ch->g;
	struct nvgpu_channel *temp_ch;

	/* If CTX_RELOAD is set on a channel, move it to some other channel */
	if (hw_state->ctx_reload) {
		nvgpu_rwsem_down_read(&tsg->ch_list_lock);
		nvgpu_list_for_each_entry(temp_ch, &tsg->ch_list,
				nvgpu_channel, ch_entry) {
			if (temp_ch->chid != ch->chid) {
				g->ops.channel.force_ctx_reload(temp_ch);
				break;
			}
		}
		nvgpu_rwsem_up_read(&tsg->ch_list_lock);
	}
}

static void nvgpu_tsg_destroy(struct nvgpu_tsg *tsg)
{
#ifdef CONFIG_NVGPU_CHANNEL_TSG_CONTROL
	nvgpu_mutex_destroy(&tsg->event_id_list_lock);
#endif
	nvgpu_mutex_destroy(&tsg->ctx_init_lock);
}

#ifdef CONFIG_NVGPU_CHANNEL_TSG_CONTROL
/* force reset tsg that the channel is bound to */
int nvgpu_tsg_force_reset_ch(struct nvgpu_channel *ch,
				u32 err_code, bool verbose)
{
	struct gk20a *g = ch->g;

	struct nvgpu_tsg *tsg = nvgpu_tsg_from_ch(ch);

	if (tsg != NULL) {
		nvgpu_tsg_set_error_notifier(g, tsg, err_code);
		nvgpu_rc_tsg_and_related_engines(g, tsg, verbose,
			RC_TYPE_FORCE_RESET);
	} else {
		nvgpu_err(g, "chid: %d is not bound to tsg", ch->chid);
	}

	return 0;
}
#endif

void nvgpu_tsg_cleanup_sw(struct gk20a *g)
{
	struct nvgpu_fifo *f = &g->fifo;
	u32 tsgid;

	for (tsgid = 0; tsgid < f->num_channels; tsgid++) {
		struct nvgpu_tsg *tsg = &f->tsg[tsgid];

		nvgpu_tsg_destroy(tsg);
	}

	nvgpu_vfree(g, f->tsg);
	f->tsg = NULL;
	nvgpu_mutex_destroy(&f->tsg_inuse_mutex);
}

static void nvgpu_tsg_init_support(struct gk20a *g, u32 tsgid)
{
	struct nvgpu_tsg *tsg = NULL;

	tsg = &g->fifo.tsg[tsgid];

	tsg->in_use = false;
	tsg->tsgid = tsgid;
	tsg->abortable = true;

	nvgpu_init_list_node(&tsg->ch_list);
	nvgpu_rwsem_init(&tsg->ch_list_lock);
	nvgpu_mutex_init(&tsg->ctx_init_lock);

#ifdef CONFIG_NVGPU_CHANNEL_TSG_CONTROL
	nvgpu_init_list_node(&tsg->event_id_list);
	nvgpu_mutex_init(&tsg->event_id_list_lock);
#endif
}

int nvgpu_tsg_setup_sw(struct gk20a *g)
{
	struct nvgpu_fifo *f = &g->fifo;
	u32 tsgid;
	int err;

	nvgpu_mutex_init(&f->tsg_inuse_mutex);

	f->tsg = nvgpu_vzalloc(g, f->num_channels * sizeof(*f->tsg));
	if (f->tsg == NULL) {
		nvgpu_err(g, "no mem for tsgs");
		err = -ENOMEM;
		goto clean_up_mutex;
	}

	for (tsgid = 0; tsgid < f->num_channels; tsgid++) {
		nvgpu_tsg_init_support(g, tsgid);
	}

	return 0;

clean_up_mutex:
	nvgpu_mutex_destroy(&f->tsg_inuse_mutex);
	return err;
}

void nvgpu_tsg_set_unserviceable(struct gk20a *g,
				struct nvgpu_tsg *tsg)
{
	struct nvgpu_channel *ch = NULL;

	(void)g;

	nvgpu_rwsem_down_read(&tsg->ch_list_lock);
	nvgpu_list_for_each_entry(ch, &tsg->ch_list, nvgpu_channel, ch_entry) {
		if (nvgpu_channel_get(ch) != NULL) {
			nvgpu_channel_set_unserviceable(ch);
			nvgpu_channel_put(ch);
		}
	}
	nvgpu_rwsem_up_read(&tsg->ch_list_lock);
}

void nvgpu_tsg_wakeup_wqs(struct gk20a *g,
				struct nvgpu_tsg *tsg)
{
	struct nvgpu_channel *ch = NULL;

	nvgpu_rwsem_down_read(&tsg->ch_list_lock);
	nvgpu_list_for_each_entry(ch, &tsg->ch_list, nvgpu_channel, ch_entry) {
		if (nvgpu_channel_get(ch) != NULL) {
			nvgpu_channel_wakeup_wqs(g, ch);
			nvgpu_channel_put(ch);
		}
	}
	nvgpu_rwsem_up_read(&tsg->ch_list_lock);
}

bool nvgpu_tsg_mark_error(struct gk20a *g,
		struct nvgpu_tsg *tsg)
{
	struct nvgpu_channel *ch = NULL;
	bool verbose = false;

	nvgpu_rwsem_down_read(&tsg->ch_list_lock);
	nvgpu_list_for_each_entry(ch, &tsg->ch_list, nvgpu_channel, ch_entry) {
		if (nvgpu_channel_get(ch) != NULL) {
			if (nvgpu_channel_mark_error(g, ch)) {
				verbose = true;
			}
			nvgpu_channel_put(ch);
		}
	}
	nvgpu_rwsem_up_read(&tsg->ch_list_lock);

	return verbose;

}

#ifdef CONFIG_NVGPU_KERNEL_MODE_SUBMIT
void nvgpu_tsg_set_ctxsw_timeout_accumulated_ms(struct nvgpu_tsg *tsg, u32 ms)
{
	struct nvgpu_channel *ch = NULL;

	nvgpu_rwsem_down_read(&tsg->ch_list_lock);
	nvgpu_list_for_each_entry(ch, &tsg->ch_list, nvgpu_channel, ch_entry) {
		if (nvgpu_channel_get(ch) != NULL) {
			ch->ctxsw_timeout_accumulated_ms = ms;
			nvgpu_channel_put(ch);
		}
	}
	nvgpu_rwsem_up_read(&tsg->ch_list_lock);
}

bool nvgpu_tsg_ctxsw_timeout_debug_dump_state(struct nvgpu_tsg *tsg)
{
	struct nvgpu_channel *ch = NULL;
	bool verbose = false;

	nvgpu_rwsem_down_read(&tsg->ch_list_lock);
	nvgpu_list_for_each_entry(ch, &tsg->ch_list, nvgpu_channel, ch_entry) {
		if (nvgpu_channel_get(ch) != NULL) {
			if (ch->ctxsw_timeout_debug_dump) {
				verbose = true;
			}
			nvgpu_channel_put(ch);
		}
	}
	nvgpu_rwsem_up_read(&tsg->ch_list_lock);

	return verbose;
}
#endif

void nvgpu_tsg_set_error_notifier(struct gk20a *g, struct nvgpu_tsg *tsg,
		u32 error_notifier)
{
	struct nvgpu_channel *ch = NULL;
	u32 max_error_notifier_id = NVGPU_ERR_NOTIFIER_PBDMA_PUSHBUFFER_CRC_MISMATCH;

	if (error_notifier > max_error_notifier_id) {
		return;
	}

	nvgpu_rwsem_down_read(&tsg->ch_list_lock);
	nvgpu_list_for_each_entry(ch, &tsg->ch_list, nvgpu_channel, ch_entry) {
		if (nvgpu_channel_get(ch) != NULL) {
			nvgpu_channel_set_error_notifier(g, ch, error_notifier);
			nvgpu_channel_put(ch);
		}
	}
	nvgpu_rwsem_up_read(&tsg->ch_list_lock);
}

void nvgpu_tsg_set_ctx_mmu_error(struct gk20a *g, struct nvgpu_tsg *tsg)
{
	nvgpu_err(g, "TSG %d generated a mmu fault", tsg->tsgid);

	nvgpu_tsg_set_error_notifier(g, tsg,
		NVGPU_ERR_NOTIFIER_FIFO_ERROR_MMU_ERR_FLT);
}

#ifdef CONFIG_NVGPU_KERNEL_MODE_SUBMIT
bool nvgpu_tsg_check_ctxsw_timeout(struct nvgpu_tsg *tsg,
		bool *debug_dump, u32 *ms)
{
	struct nvgpu_channel *ch;
	bool recover = false;
	bool progress = false;
	struct gk20a *g = tsg->g;

	*debug_dump = false;
	*ms = g->ctxsw_timeout_period_ms;

	nvgpu_rwsem_down_read(&tsg->ch_list_lock);

	/* check if there was some progress on any of the TSG channels.
	 * fifo recovery is needed if at least one channel reached the
	 * maximum timeout without progress (update in gpfifo pointers).
	 */
	nvgpu_list_for_each_entry(ch, &tsg->ch_list, nvgpu_channel, ch_entry) {
		if (nvgpu_channel_get(ch) != NULL) {
			recover = nvgpu_channel_update_and_check_ctxsw_timeout(ch,
					*ms, &progress);
			if (progress || recover) {
				break;
			}
			nvgpu_channel_put(ch);
		}
	}

	if (recover) {
		/*
		 * if one channel is presumed dead (no progress for too long),
		 * then fifo recovery is needed. we can't really figure out
		 * which channel caused the problem, so set ctxsw timeout error
		 * notifier for all channels.
		 */
		*ms = ch->ctxsw_timeout_accumulated_ms;
		nvgpu_channel_put(ch);
		*debug_dump = nvgpu_tsg_ctxsw_timeout_debug_dump_state(tsg);

	} else {
		/*
		 * if at least one channel in the TSG made some progress, reset
		 * ctxsw_timeout_accumulated_ms for all channels in the TSG. In
		 * particular, this resets ctxsw_timeout_accumulated_ms timeout
		 * for channels that already completed their work.
		 */
		if (progress) {
			nvgpu_log_info(g, "progress on tsg=%d ch=%d",
					tsg->tsgid, ch->chid);
			nvgpu_channel_put(ch);
			*ms = g->ctxsw_timeout_period_ms;
			nvgpu_tsg_set_ctxsw_timeout_accumulated_ms(tsg, *ms);
		}
	}

	/* if we could not detect progress on any of the channel, but none
	 * of them has reached the timeout, there is nothing more to do:
	 * ctxsw_timeout_accumulated_ms has been updated for all of them.
	 */
	nvgpu_rwsem_up_read(&tsg->ch_list_lock);
	return recover;
}
#endif

#ifdef CONFIG_NVGPU_CHANNEL_TSG_SCHEDULING
int nvgpu_tsg_set_interleave(struct nvgpu_tsg *tsg, u32 level)
{
	struct gk20a *g = tsg->g;
	int ret;

	nvgpu_log(g, gpu_dbg_sched,
			"tsgid=%u interleave=%u", tsg->tsgid, level);

	nvgpu_speculation_barrier();

	if ((level != NVGPU_FIFO_RUNLIST_INTERLEAVE_LEVEL_LOW) &&
	    (level != NVGPU_FIFO_RUNLIST_INTERLEAVE_LEVEL_MEDIUM) &&
	    (level != NVGPU_FIFO_RUNLIST_INTERLEAVE_LEVEL_HIGH)) {
		return -EINVAL;
	}

	if (g->ops.tsg.set_interleave != NULL) {
		ret = g->ops.tsg.set_interleave(tsg, level);
		if (ret != 0) {
			nvgpu_err(g,
				"set interleave failed tsgid=%u", tsg->tsgid);
			return ret;
		}
	}

	tsg->interleave_level = level;

	/* TSG may not be bound yet */
	if (tsg->runlist == NULL) {
		return 0;
	}

	return g->ops.runlist.reload(g, tsg->runlist, tsg->rl_domain, true, true);
}

int nvgpu_tsg_set_timeslice(struct nvgpu_tsg *tsg, u32 timeslice_us)
{
	struct gk20a *g = tsg->g;

	nvgpu_log(g, gpu_dbg_sched, "tsgid=%u timeslice=%u us",
			tsg->tsgid, timeslice_us);

	if (timeslice_us < g->tsg_timeslice_min_us ||
		timeslice_us > g->tsg_timeslice_max_us) {
		return -EINVAL;
	}

	tsg->timeslice_us = timeslice_us;

	/* TSG may not be bound yet */
	if (tsg->runlist == NULL) {
		return 0;
	}

	return g->ops.runlist.reload(g, tsg->runlist, tsg->rl_domain, true, true);
}

u32 nvgpu_tsg_get_timeslice(struct nvgpu_tsg *tsg)
{
	return tsg->timeslice_us;
}

int nvgpu_tsg_set_long_timeslice(struct nvgpu_tsg *tsg, u32 timeslice_us)
{
	struct gk20a *g = tsg->g;

	nvgpu_log(g, gpu_dbg_sched, "tsgid=%u timeslice=%u us",
			tsg->tsgid, timeslice_us);

	if (timeslice_us < g->tsg_timeslice_min_us ||
			timeslice_us > g->tsg_dbg_timeslice_max_us) {
		return -EINVAL;
	}

	tsg->timeslice_us = timeslice_us;

	/* TSG may not be bound yet */
	if (tsg->runlist == NULL) {
		return 0;
	}

	return g->ops.runlist.reload(g, tsg->runlist, tsg->rl_domain, true, true);
}
#endif

u32 nvgpu_tsg_default_timeslice_us(struct gk20a *g)
{
	(void)g;
	return NVGPU_TSG_TIMESLICE_DEFAULT_US;
}

static void nvgpu_tsg_release_used_tsg(struct nvgpu_fifo *f,
		struct nvgpu_tsg *tsg)
{
	nvgpu_mutex_acquire(&f->tsg_inuse_mutex);
	f->tsg[tsg->tsgid].in_use = false;
	nvgpu_mutex_release(&f->tsg_inuse_mutex);
}

static struct nvgpu_tsg *nvgpu_tsg_acquire_unused_tsg(struct nvgpu_fifo *f)
{
	struct nvgpu_tsg *tsg = NULL;
	unsigned int tsgid;

	nvgpu_mutex_acquire(&f->tsg_inuse_mutex);
	for (tsgid = 0; tsgid < f->num_channels; tsgid++) {
		if (!f->tsg[tsgid].in_use) {
			f->tsg[tsgid].in_use = true;
			tsg = &f->tsg[tsgid];
			break;
		}
	}
	nvgpu_mutex_release(&f->tsg_inuse_mutex);

	return tsg;
}

int nvgpu_tsg_open_common(struct gk20a *g, struct nvgpu_tsg *tsg, pid_t pid)
{
	u32 no_of_sm = g->ops.gr.init.get_no_of_sm(g);
	int err;

	/* we need to allocate this after g->ops.gr.init_fs_state() since
	 * we initialize gr.config->no_of_sm in this function
	 */
	if (no_of_sm == 0U) {
		nvgpu_err(g, "no_of_sm %d not set, failed allocation", no_of_sm);
		return -EINVAL;
	}

	err = nvgpu_tsg_alloc_sm_error_states_mem(g, tsg, no_of_sm);
	if (err != 0) {
		return err;
	}

	tsg->tgid = pid;
	tsg->g = g;
	tsg->num_active_channels = 0U;
	tsg->ch_count = 0U;
	nvgpu_ref_init(&tsg->refcount);

	tsg->vm = NULL;
	tsg->interleave_level = NVGPU_FIFO_RUNLIST_INTERLEAVE_LEVEL_LOW;
	tsg->timeslice_us = g->ops.tsg.default_timeslice_us(g);
	tsg->runlist = NULL;
	/*
	 * The domain ptr will get updated with the right id once the runlist
	 * gets specified based on the first channel.
	 */
	tsg->rl_domain = nvgpu_rl_domain_get(g, 0, "(default)");
	tsg->nvs_domain = nvgpu_nvs_domain_by_name(g, "(default)");
#ifdef CONFIG_NVGPU_DEBUGGER
	tsg->sm_exception_mask_type = NVGPU_SM_EXCEPTION_TYPE_MASK_NONE;
#endif
	tsg->gr_ctx = nvgpu_alloc_gr_ctx_struct(g);
	if (tsg->gr_ctx == NULL) {
		err = -ENOMEM;
		goto clean_up;
	}

#ifdef CONFIG_NVGPU_SM_DIVERSITY
	nvgpu_gr_ctx_set_sm_diversity_config(tsg->gr_ctx,
		NVGPU_INVALID_SM_CONFIG_ID);
#endif

	if (g->ops.tsg.init_eng_method_buffers != NULL) {
		err = g->ops.tsg.init_eng_method_buffers(g, tsg);
		if (err != 0) {
			nvgpu_err(g, "tsg %d init eng method bufs failed %d",
				  tsg->tsgid, err);
			goto clean_up;
		}
	}

	if (g->ops.tsg.open != NULL) {
		err = g->ops.tsg.open(tsg);
		if (err != 0) {
			nvgpu_err(g, "tsg %d fifo open failed %d",
				  tsg->tsgid, err);
			goto clean_up;
		}
	}

	return 0;

clean_up:
	nvgpu_tsg_release_common(g, tsg);
	nvgpu_ref_put(&tsg->refcount, NULL);

	return err;
}

struct nvgpu_tsg *nvgpu_tsg_open(struct gk20a *g, pid_t pid)
{
	struct nvgpu_tsg *tsg;
	int err;

	tsg = nvgpu_tsg_acquire_unused_tsg(&g->fifo);
	if (tsg == NULL) {
		return NULL;
	}

	err = nvgpu_tsg_open_common(g, tsg, pid);
	if (err != 0) {
		nvgpu_tsg_release_used_tsg(&g->fifo, tsg);
		nvgpu_err(g, "tsg %d open failed %d", tsg->tsgid, err);
		return NULL;
	}

	nvgpu_log(g, gpu_dbg_fn, "tsg opened %d\n", tsg->tsgid);

	return tsg;
}

void nvgpu_tsg_release_common(struct gk20a *g, struct nvgpu_tsg *tsg)
{
	if (g->ops.tsg.release != NULL) {
		g->ops.tsg.release(tsg);
	}

	nvgpu_free_gr_ctx_struct(g, tsg->gr_ctx);
	tsg->gr_ctx = NULL;

	if (g->ops.tsg.deinit_eng_method_buffers != NULL) {
		g->ops.tsg.deinit_eng_method_buffers(g, tsg);
	}

#ifdef CONFIG_NVGPU_PROFILER
	if (tsg->prof != NULL) {
		nvgpu_profiler_unbind_context(tsg->prof);
	}
#endif

	if (tsg->nvs_domain != NULL) {
		nvgpu_nvs_domain_put(g, tsg->nvs_domain);
		tsg->nvs_domain = NULL;
		tsg->rl_domain = NULL;
	}

	if (tsg->vm != NULL) {
		nvgpu_vm_put(tsg->vm);
		tsg->vm = NULL;
	}

	if(tsg->sm_error_states != NULL) {
		nvgpu_kfree(g, tsg->sm_error_states);
		tsg->sm_error_states = NULL;
#ifdef CONFIG_NVGPU_DEBUGGER
		nvgpu_mutex_destroy(&tsg->sm_exception_mask_lock);
#endif
	}
}

static struct nvgpu_tsg *tsg_gk20a_from_ref(struct nvgpu_ref *ref)
{
	return (struct nvgpu_tsg *)
		((uintptr_t)ref - offsetof(struct nvgpu_tsg, refcount));
}

void nvgpu_tsg_release(struct nvgpu_ref *ref)
{
	struct nvgpu_tsg *tsg = tsg_gk20a_from_ref(ref);
	struct gk20a *g = tsg->g;
	int err;

	err = gk20a_busy(g);
	if (err != 0) {
		nvgpu_err(g, "cannot busy() err=%d!", err);
		return;
	}

	if ((tsg->gr_ctx != NULL) && (tsg->vm != NULL)) {
		g->ops.gr.setup.free_gr_ctx(g, tsg->vm, tsg->gr_ctx);
	}

#ifdef CONFIG_NVGPU_CHANNEL_TSG_CONTROL
	/* unhook all events created on this TSG */
	nvgpu_mutex_acquire(&tsg->event_id_list_lock);
	while (nvgpu_list_empty(&tsg->event_id_list) == false) {
		nvgpu_list_del(tsg->event_id_list.next);
	}
	nvgpu_mutex_release(&tsg->event_id_list_lock);
#endif

	nvgpu_tsg_release_common(g, tsg);
	nvgpu_tsg_release_used_tsg(&g->fifo, tsg);

	nvgpu_log(g, gpu_dbg_fn, "tsg released %d", tsg->tsgid);

	gk20a_idle(g);
}

struct nvgpu_tsg *nvgpu_tsg_from_ch(struct nvgpu_channel *ch)
{
	struct nvgpu_tsg *tsg = NULL;
	u32 tsgid = ch->tsgid;

	if (tsgid != NVGPU_INVALID_TSG_ID) {
		struct gk20a *g = ch->g;
		struct nvgpu_fifo *f = &g->fifo;

		tsg = &f->tsg[tsgid];
	} else {
		nvgpu_log(ch->g, gpu_dbg_fn, "tsgid is invalid for chid: %d",
			ch->chid);
	}
	return tsg;
}

int nvgpu_tsg_alloc_sm_error_states_mem(struct gk20a *g,
					struct nvgpu_tsg *tsg,
					u32 num_sm)
{
	if (tsg->sm_error_states != NULL) {
		return -EINVAL;
	}

	tsg->sm_error_states = nvgpu_kzalloc(g, nvgpu_safe_mult_u64(
			sizeof(struct nvgpu_tsg_sm_error_state), num_sm));
	if (tsg->sm_error_states == NULL) {
		nvgpu_err(g, "sm_error_states mem allocation failed");
		return -ENOMEM;
	}

#ifdef CONFIG_NVGPU_DEBUGGER
	nvgpu_mutex_init(&tsg->sm_exception_mask_lock);
#endif

	return 0;
}

int nvgpu_tsg_store_sm_error_state(struct nvgpu_tsg *tsg, u32 sm_id,
		u32 hww_global_esr, u32 hww_warp_esr, u64 hww_warp_esr_pc,
		u32 hww_global_esr_report_mask, u32 hww_warp_esr_report_mask)
{
	struct gk20a *g = tsg->g;
	u32 num_of_sm = g->ops.gr.init.get_no_of_sm(g);
	struct nvgpu_tsg_sm_error_state *sm_error_states = NULL;

	if (sm_id >= num_of_sm) {
		nvgpu_err(g, "Invalid number of SMs");
		return -EINVAL;
	}

	if (tsg->sm_error_states == NULL) {
		nvgpu_err(g, "invalid memory");
		return -ENOMEM;
	}

	sm_error_states = &tsg->sm_error_states[sm_id];

	sm_error_states->hww_global_esr = hww_global_esr;
	sm_error_states->hww_warp_esr = hww_warp_esr;
	sm_error_states->hww_warp_esr_pc = hww_warp_esr_pc;
	sm_error_states->hww_global_esr_report_mask = hww_global_esr_report_mask;
	sm_error_states->hww_warp_esr_report_mask = hww_warp_esr_report_mask;

	return 0;
}

const struct nvgpu_tsg_sm_error_state *nvgpu_tsg_get_sm_error_state(struct nvgpu_tsg *tsg, u32 sm_id)
{
	struct gk20a *g = tsg->g;
	u32 num_of_sm = g->ops.gr.init.get_no_of_sm(g);

	if (sm_id >= num_of_sm) {
		nvgpu_err(g, "Invalid number of SMs");
		return NULL;
	}

	if (tsg->sm_error_states == NULL) {
		nvgpu_err(g, "Invalid memory");
		return NULL;
	}

	return &tsg->sm_error_states[sm_id];
}

#ifdef CONFIG_NVGPU_DEBUGGER
int nvgpu_tsg_set_sm_exception_type_mask(struct nvgpu_channel *ch,
		u32 exception_mask)
{
	struct nvgpu_tsg *tsg;

	tsg = nvgpu_tsg_from_ch(ch);
	if (tsg == NULL) {
		return -EINVAL;
	}

	nvgpu_mutex_acquire(&tsg->sm_exception_mask_lock);
	tsg->sm_exception_mask_type = exception_mask;
	nvgpu_mutex_release(&tsg->sm_exception_mask_lock);

	return 0;
}
#endif

void nvgpu_tsg_abort(struct gk20a *g, struct nvgpu_tsg *tsg, bool preempt)
{
	struct nvgpu_channel *ch = NULL;

	nvgpu_log_fn(g, " ");

	WARN_ON(tsg->abortable == false);

	g->ops.tsg.disable(tsg);

	if (preempt) {
		/*
		 * Ignore the return value below. If preempt fails, preempt_tsg
		 * operation will print the error and ctxsw timeout may trigger
		 * a recovery if needed.
		 */
		(void)g->ops.fifo.preempt_tsg(g, tsg);
	}

	nvgpu_rwsem_down_read(&tsg->ch_list_lock);
	nvgpu_list_for_each_entry(ch, &tsg->ch_list, nvgpu_channel, ch_entry) {
		if (nvgpu_channel_get(ch) != NULL) {
			nvgpu_channel_set_unserviceable(ch);
			if (g->ops.channel.abort_clean_up != NULL) {
				g->ops.channel.abort_clean_up(ch);
			}
			nvgpu_channel_put(ch);
		}
	}
	nvgpu_rwsem_up_read(&tsg->ch_list_lock);
}

void nvgpu_tsg_reset_faulted_eng_pbdma(struct gk20a *g, struct nvgpu_tsg *tsg,
		bool eng, bool pbdma)
{
	struct nvgpu_channel *ch;

	if (g->ops.channel.reset_faulted == NULL) {
		return;
	}

	if (tsg == NULL) {
		return;
	}

	nvgpu_log(g, gpu_dbg_info, "reset faulted eng and pbdma bits in ccsr");

	nvgpu_rwsem_down_read(&tsg->ch_list_lock);
	nvgpu_list_for_each_entry(ch, &tsg->ch_list, nvgpu_channel, ch_entry) {
		g->ops.channel.reset_faulted(g, ch, eng, pbdma);
	}
	nvgpu_rwsem_up_read(&tsg->ch_list_lock);
}

#ifdef CONFIG_NVGPU_DEBUGGER
int nvgpu_tsg_set_mmu_debug_mode(struct nvgpu_channel *ch, bool enable)
{
	struct gk20a *g;
	int err = 0;
	u32 ch_refcnt;
	u32 tsg_refcnt;
	u32 fb_refcnt;
	struct nvgpu_tsg *tsg = nvgpu_tsg_from_ch(ch);

	if (tsg == NULL) {
		return -EINVAL;
	}
	g = ch->g;

	if ((g->ops.fb.set_mmu_debug_mode == NULL) &&
		(g->ops.gr.set_mmu_debug_mode == NULL)) {
		return -ENOSYS;
	}

	if (enable) {
		ch_refcnt = ch->mmu_debug_mode_refcnt + 1U;
		tsg_refcnt = tsg->mmu_debug_mode_refcnt + 1U;
		fb_refcnt = g->mmu_debug_mode_refcnt + 1U;
	} else {
		ch_refcnt = ch->mmu_debug_mode_refcnt - 1U;
		tsg_refcnt = tsg->mmu_debug_mode_refcnt - 1U;
		fb_refcnt = g->mmu_debug_mode_refcnt - 1U;
	}

	if (g->ops.gr.set_mmu_debug_mode != NULL) {
		/*
		 * enable GPC MMU debug mode if it was requested for at
		 * least one channel in the TSG
		 */
		err = g->ops.gr.set_mmu_debug_mode(g, ch, tsg_refcnt > 0U);
		if (err != 0) {
			nvgpu_err(g, "set mmu debug mode failed, err=%d", err);
			return err;
		}
	}

	if (g->ops.fb.set_mmu_debug_mode != NULL) {
		/*
		 * enable FB/HS MMU debug mode if it was requested for
		 * at least one TSG
		 */
		g->ops.fb.set_mmu_debug_mode(g, fb_refcnt > 0U);
	}

	ch->mmu_debug_mode_refcnt = ch_refcnt;
	tsg->mmu_debug_mode_refcnt = tsg_refcnt;
	g->mmu_debug_mode_refcnt = fb_refcnt;

	return err;
}
#endif
