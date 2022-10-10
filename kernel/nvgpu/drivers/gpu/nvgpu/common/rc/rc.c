/*
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

#include <nvgpu/log.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/fifo.h>
#ifdef CONFIG_NVGPU_RECOVERY
#include <nvgpu/engines.h>
#include <nvgpu/debug.h>
#include <nvgpu/nvgpu_err.h>
#include <nvgpu/gr/gr.h>
#include <nvgpu/gr/gr_instances.h>
#endif
#include <nvgpu/channel.h>
#include <nvgpu/tsg.h>
#include <nvgpu/error_notifier.h>
#include <nvgpu/pbdma_status.h>
#include <nvgpu/rc.h>

void nvgpu_rc_fifo_recover(struct gk20a *g, u32 eng_bitmask,
			u32 hw_id, bool id_is_tsg,
			bool id_is_known, bool debug_dump, u32 rc_type)
{
#ifdef CONFIG_NVGPU_RECOVERY
	unsigned int id_type;

	if (debug_dump) {
		gk20a_debug_dump(g);
	}

	if (g->ops.ltc.flush != NULL) {
		g->ops.ltc.flush(g);
	}

	if (id_is_known) {
		id_type = id_is_tsg ? ID_TYPE_TSG : ID_TYPE_CHANNEL;
	} else {
		id_type = ID_TYPE_UNKNOWN;
	}

	g->ops.fifo.recover(g, eng_bitmask, hw_id, id_type,
				 rc_type, NULL);
#else
	WARN_ON(!g->sw_quiesce_pending);
	(void)eng_bitmask;
	(void)hw_id;
	(void)id_is_tsg;
	(void)id_is_known;
	(void)debug_dump;
	(void)rc_type;
#endif
}

void nvgpu_rc_ctxsw_timeout(struct gk20a *g, u32 eng_bitmask,
				struct nvgpu_tsg *tsg, bool debug_dump)
{
	nvgpu_tsg_set_error_notifier(g, tsg,
		NVGPU_ERR_NOTIFIER_FIFO_ERROR_IDLE_TIMEOUT);

#ifdef CONFIG_NVGPU_RECOVERY
	/*
	 * Cancel all channels' wdt since ctxsw timeout causes the runlist to
	 * stuck and might falsely trigger multiple watchdogs at a time. We
	 * won't detect proper wdt timeouts that would have happened, but if
	 * they're stuck, they will trigger the wdt soon enough again.
	 */
	nvgpu_channel_restart_all_wdts(g);

	nvgpu_rc_fifo_recover(g, eng_bitmask, tsg->tsgid, true, true, debug_dump,
			RC_TYPE_CTXSW_TIMEOUT);
#else
	WARN_ON(!g->sw_quiesce_pending);
	(void)eng_bitmask;
	(void)debug_dump;
#endif
}

void nvgpu_rc_pbdma_fault(struct gk20a *g, u32 pbdma_id, u32 error_notifier,
			struct nvgpu_pbdma_status_info *pbdma_status)
{
	u32 id;
	u32 id_type = PBDMA_STATUS_ID_TYPE_INVALID;

	nvgpu_log(g, gpu_dbg_info, "pbdma id %d error notifier %d",
			pbdma_id, error_notifier);

	if (nvgpu_pbdma_status_is_chsw_valid(pbdma_status) ||
			nvgpu_pbdma_status_is_chsw_save(pbdma_status)) {
		id = pbdma_status->id;
		id_type = pbdma_status->id_type;
	} else if (nvgpu_pbdma_status_is_chsw_load(pbdma_status) ||
			nvgpu_pbdma_status_is_chsw_switch(pbdma_status)) {
		id = pbdma_status->next_id;
		id_type = pbdma_status->next_id_type;
	} else {
		/* Nothing to do here */
		nvgpu_err(g, "Invalid pbdma_status.id");
		return;
	}

	if (id_type == PBDMA_STATUS_ID_TYPE_TSGID) {
		struct nvgpu_tsg *tsg = nvgpu_tsg_get_from_id(g, id);

		nvgpu_tsg_set_error_notifier(g, tsg, error_notifier);
		nvgpu_rc_tsg_and_related_engines(g, tsg, true,
			RC_TYPE_PBDMA_FAULT);
	} else if(id_type == PBDMA_STATUS_ID_TYPE_CHID) {
		struct nvgpu_channel *ch = nvgpu_channel_from_id(g, id);
		struct nvgpu_tsg *tsg;
		if (ch == NULL) {
			nvgpu_err(g, "channel is not referenceable");
			return;
		}

		tsg = nvgpu_tsg_from_ch(ch);
		if (tsg != NULL) {
			nvgpu_tsg_set_error_notifier(g, tsg, error_notifier);
			nvgpu_rc_tsg_and_related_engines(g, tsg, true,
				RC_TYPE_PBDMA_FAULT);
		} else {
			nvgpu_err(g, "chid: %d is not bound to tsg", ch->chid);
		}

		nvgpu_channel_put(ch);
	} else {
		nvgpu_err(g, "Invalid pbdma_status.id_type");
	}
}

void nvgpu_rc_runlist_update(struct gk20a *g, u32 runlist_id)
{
#ifdef CONFIG_NVGPU_RECOVERY
	u32 eng_bitmask = nvgpu_engine_get_runlist_busy_engines(g, runlist_id);

	if (eng_bitmask != 0U) {
		nvgpu_rc_fifo_recover(g, eng_bitmask, INVAL_ID, false, false, true,
				RC_TYPE_RUNLIST_UPDATE_TIMEOUT);
	}
#else
	/*
	 * Runlist update occurs in non-mission mode, when
	 * adding/removing channel/TSGs. The pending bit
	 * is a debug only feature. As a result logging a
	 * warning is sufficient.
	 * We expect other HW safety mechanisms such as
	 * PBDMA timeout to detect issues that caused pending
	 * to not clear. It's possible bad base address could
	 * cause some MMU faults too.
	 * Worst case we rely on the application level task
	 * monitor to detect the GPU tasks are not completing
	 * on time.
	 */
	WARN_ON(!g->sw_quiesce_pending);
	(void)runlist_id;
#endif
}

void nvgpu_rc_preempt_timeout(struct gk20a *g, struct nvgpu_tsg *tsg)
{
	nvgpu_tsg_set_error_notifier(g, tsg,
		NVGPU_ERR_NOTIFIER_FIFO_ERROR_IDLE_TIMEOUT);

#ifdef CONFIG_NVGPU_RECOVERY
	nvgpu_rc_tsg_and_related_engines(g, tsg, true, RC_TYPE_PREEMPT_TIMEOUT);
#else
	BUG_ON(!g->sw_quiesce_pending);
#endif
}

void nvgpu_rc_gr_fault(struct gk20a *g, struct nvgpu_tsg *tsg,
		struct nvgpu_channel *ch)
{
#ifdef CONFIG_NVGPU_RECOVERY
	u32 gr_engine_id;
	u32 gr_eng_bitmask = 0U;
	u32 cur_gr_instance_id = nvgpu_gr_get_cur_instance_id(g);
	u32 inst_id = nvgpu_gr_get_syspipe_id(g, cur_gr_instance_id);

	nvgpu_log(g, gpu_dbg_gr, "RC GR%u inst_id%u",
		cur_gr_instance_id, inst_id);

	gr_engine_id = nvgpu_engine_get_gr_id_for_inst(g, inst_id);
	if (gr_engine_id != NVGPU_INVALID_ENG_ID) {
		gr_eng_bitmask = BIT32(gr_engine_id);
	} else {
		nvgpu_warn(g, "gr_engine_id is invalid");
	}

	if (tsg != NULL) {
		nvgpu_rc_fifo_recover(g, gr_eng_bitmask, tsg->tsgid,
				   true, true, true, RC_TYPE_GR_FAULT);
	} else {
		if (ch != NULL) {
			nvgpu_err(g, "chid: %d referenceable but not "
				"bound to tsg", ch->chid);
		}
		nvgpu_rc_fifo_recover(g, gr_eng_bitmask, INVAL_ID,
				   false, false, true, RC_TYPE_GR_FAULT);
	}
#else
	WARN_ON(!g->sw_quiesce_pending);
	(void)tsg;
	(void)ch;
#endif
	nvgpu_log(g, gpu_dbg_gr, "done");
}

void nvgpu_rc_ce_fault(struct gk20a *g, u32 inst_id)
{
	struct nvgpu_channel *ch = NULL;
	struct nvgpu_tsg *tsg = NULL;
	u32 chid = NVGPU_INVALID_CHANNEL_ID;
	u64 inst_ptr = 0U;

	if (g->ops.ce.get_inst_ptr_from_lce != NULL) {
		inst_ptr = g->ops.ce.get_inst_ptr_from_lce(g,
						inst_id);
	}
	/* refch will be put back before recovery */
	ch = nvgpu_channel_refch_from_inst_ptr(g, inst_ptr);
	if (ch == NULL) {
		return;
	} else {
		chid = ch->chid;
		nvgpu_channel_put(ch);
		tsg = nvgpu_tsg_from_ch(ch);
		if (tsg == NULL) {
			nvgpu_err(g, "channel_id: %d not bound to tsg",
						chid);
			/* ToDo: Trigger Quiesce? */
			return;
		}
		nvgpu_tsg_set_error_notifier(g, tsg, NVGPU_ERR_NOTIFIER_CE_ERROR);
	}
#ifdef CONFIG_NVGPU_RECOVERY
	nvgpu_rc_tsg_and_related_engines(g, tsg, true,
			RC_TYPE_CE_FAULT);
#else
	WARN_ON(!g->sw_quiesce_pending);
	(void)tsg;
#endif
}

void nvgpu_rc_sched_error_bad_tsg(struct gk20a *g)
{
#ifdef CONFIG_NVGPU_RECOVERY
	/* id is unknown, preempt all runlists and do recovery */
	nvgpu_rc_fifo_recover(g, 0, INVAL_ID, false, false, false,
		RC_TYPE_SCHED_ERR);
#else
	WARN_ON(!g->sw_quiesce_pending);
#endif
}

void nvgpu_rc_tsg_and_related_engines(struct gk20a *g, struct nvgpu_tsg *tsg,
			 bool debug_dump, u32 rc_type)
{
#ifdef CONFIG_NVGPU_RECOVERY
	u32 eng_bitmask = 0U;
	int err = 0;

#ifdef CONFIG_NVGPU_DEBUGGER
	nvgpu_mutex_acquire(&g->dbg_sessions_lock);
#endif

	/* disable tsg so that it does not get scheduled again */
	g->ops.tsg.disable(tsg);

	/*
	 * On hitting engine reset, h/w drops the ctxsw_status to INVALID in
	 * fifo_engine_status register. Also while the engine is held in reset
	 * h/w passes busy/idle straight through. fifo_engine_status registers
	 * are correct in that there is no context switch outstanding
	 * as the CTXSW is aborted when reset is asserted.
	 */
	nvgpu_log_info(g, "acquire engines_reset_mutex");
	nvgpu_mutex_acquire(&g->fifo.engines_reset_mutex);

	/*
	 * stop context switching to prevent engine assignments from
	 * changing until engine status is checked to make sure tsg
	 * being recovered is not loaded on the engines
	 */
	err = nvgpu_gr_disable_ctxsw(g);
	if (err != 0) {
		/* if failed to disable ctxsw, just abort tsg */
		nvgpu_err(g, "failed to disable ctxsw");
	} else {
		/* recover engines if tsg is loaded on the engines */
		eng_bitmask = nvgpu_engine_get_mask_on_id(g, tsg->tsgid, true);

		/*
		 * it is ok to enable ctxsw before tsg is recovered. If engines
		 * is 0, no engine recovery is needed and if it is  non zero,
		 * gk20a_fifo_recover will call get_mask_on_id again.
		 * By that time if tsg is not on the engine, engine need not
		 * be reset.
		 */
		err = nvgpu_gr_enable_ctxsw(g);
		if (err != 0) {
			nvgpu_err(g, "failed to enable ctxsw");
		}
	}
	nvgpu_log_info(g, "release engines_reset_mutex");
	nvgpu_mutex_release(&g->fifo.engines_reset_mutex);

	if (eng_bitmask != 0U) {
		nvgpu_rc_fifo_recover(g, eng_bitmask, tsg->tsgid, true, true,
			debug_dump, rc_type);
	} else {
		if (nvgpu_tsg_mark_error(g, tsg) && debug_dump) {
			gk20a_debug_dump(g);
		}

		nvgpu_tsg_abort(g, tsg, false);
	}

#ifdef CONFIG_NVGPU_DEBUGGER
	nvgpu_mutex_release(&g->dbg_sessions_lock);
#endif
#else
	WARN_ON(!g->sw_quiesce_pending);
	(void)tsg;
	(void)debug_dump;
	(void)rc_type;
#endif
}

void nvgpu_rc_mmu_fault(struct gk20a *g, u32 act_eng_bitmask,
			u32 id, unsigned int id_type, unsigned int rc_type,
			 struct mmu_fault_info *mmufault)
{
	nvgpu_err(g, "mmu fault id=%u id_type=%u act_eng_bitmask=%08x",
		id, id_type, act_eng_bitmask);

#ifdef CONFIG_NVGPU_RECOVERY
	g->ops.fifo.recover(g, act_eng_bitmask,
		id, id_type, rc_type, mmufault);
#else
	if ((id != INVAL_ID) && (id_type == ID_TYPE_TSG)) {
		struct nvgpu_tsg *tsg = &g->fifo.tsg[id];
		nvgpu_tsg_set_ctx_mmu_error(g, tsg);
		(void)nvgpu_tsg_mark_error(g, tsg);
	}

	WARN_ON(!g->sw_quiesce_pending);
	(void)rc_type;
	(void)mmufault;
#endif
}
