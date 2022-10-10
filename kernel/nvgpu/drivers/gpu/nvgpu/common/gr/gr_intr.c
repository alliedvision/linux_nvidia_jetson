/*
 * Copyright (c) 2019-2022, NVIDIA CORPORATION.  All rights reserved.
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
#include <nvgpu/nvgpu_err.h>
#include <nvgpu/io.h>
#include <nvgpu/channel.h>
#include <nvgpu/rc.h>
#include <nvgpu/static_analysis.h>
#include <nvgpu/error_notifier.h>
#include <nvgpu/power_features/pg.h>
#if defined(CONFIG_NVGPU_CYCLESTATS)
#include <nvgpu/cyclestats.h>
#endif
#include <nvgpu/string.h>

#include <nvgpu/gr/gr.h>
#include <nvgpu/gr/gr_intr.h>
#include <nvgpu/gr/config.h>
#include <nvgpu/gr/gr_falcon.h>
#include <nvgpu/gr/fecs_trace.h>
#include <nvgpu/gr/gr_utils.h>

#include "gr_intr_priv.h"

static int gr_intr_handle_pending_tpc_sm_exception(struct gk20a *g, u32 gpc, u32 tpc,
				bool *post_event, struct nvgpu_channel *fault_ch,
				u32 *hww_global_esr)
{
	int tmp_ret, ret = 0;
	u32 esr_sm_sel, sm;
	u32 sm_per_tpc = nvgpu_get_litter_value(g, GPU_LIT_NUM_SM_PER_TPC);

	nvgpu_log(g, gpu_dbg_intr | gpu_dbg_gpu_dbg,
			"GPC%d TPC%d: SM exception pending", gpc, tpc);

	if (g->ops.gr.intr.handle_tpc_sm_ecc_exception != NULL) {
		g->ops.gr.intr.handle_tpc_sm_ecc_exception(g, gpc, tpc);
	}

	g->ops.gr.intr.get_esr_sm_sel(g, gpc, tpc, &esr_sm_sel);

	for (sm = 0; sm < sm_per_tpc; sm++) {

		if ((esr_sm_sel & BIT32(sm)) == 0U) {
			continue;
		}

		nvgpu_log(g, gpu_dbg_intr | gpu_dbg_gpu_dbg,
			"GPC%d TPC%d: SM%d exception pending",
			 gpc, tpc, sm);

		tmp_ret = g->ops.gr.intr.handle_sm_exception(g,
				gpc, tpc, sm, post_event, fault_ch,
				hww_global_esr);
		ret = (ret != 0) ? ret : tmp_ret;

		/* clear the hwws, also causes tpc and gpc
		 * exceptions to be cleared. Should be cleared
		 * only if SM is locked down or empty.
		 */
		g->ops.gr.intr.clear_sm_hww(g,
			gpc, tpc, sm, *hww_global_esr);
	}

	return ret;
}

static int gr_intr_handle_tpc_exception(struct gk20a *g, u32 gpc, u32 tpc,
		bool *post_event, struct nvgpu_channel *fault_ch,
		u32 *hww_global_esr)
{
	int ret = 0;
	struct nvgpu_gr_tpc_exception pending_tpc;
	u32 gpc_offset = nvgpu_gr_gpc_offset(g, gpc);
	u32 tpc_offset = nvgpu_gr_tpc_offset(g, tpc);
	u32 offset = nvgpu_safe_add_u32(gpc_offset, tpc_offset);
	u32 tpc_exception = g->ops.gr.intr.get_tpc_exception(g, offset,
							&pending_tpc);


	nvgpu_log(g, gpu_dbg_intr | gpu_dbg_gpu_dbg,
			"GPC%d TPC%d: pending exception 0x%x",
			gpc, tpc, tpc_exception);

	/* check if an sm exception is pending */
	if (pending_tpc.sm_exception) {
		ret = gr_intr_handle_pending_tpc_sm_exception(g, gpc, tpc,
			post_event, fault_ch, hww_global_esr);
	}

	/* check if a tex exception is pending */
	if (pending_tpc.tex_exception) {
		nvgpu_log(g, gpu_dbg_intr | gpu_dbg_gpu_dbg,
			  "GPC%d TPC%d: TEX exception pending", gpc, tpc);
#ifdef CONFIG_NVGPU_NON_FUSA
		if (g->ops.gr.intr.handle_tex_exception != NULL) {
			g->ops.gr.intr.handle_tex_exception(g, gpc, tpc);
		}
#endif
	}

	/* check if a mpc exception is pending */
	if (pending_tpc.mpc_exception) {
		nvgpu_log(g, gpu_dbg_intr | gpu_dbg_gpu_dbg,
			  "GPC%d TPC%d: MPC exception pending", gpc, tpc);
		if (g->ops.gr.intr.handle_tpc_mpc_exception != NULL) {
			g->ops.gr.intr.handle_tpc_mpc_exception(g, gpc, tpc);
		}
	}

	/* check if a pe exception is pending */
	if (pending_tpc.pe_exception) {
		nvgpu_log(g, gpu_dbg_intr | gpu_dbg_gpu_dbg,
			  "GPC%d TPC%d: PE exception pending", gpc, tpc);
		if (g->ops.gr.intr.handle_tpc_pe_exception != NULL) {
			g->ops.gr.intr.handle_tpc_pe_exception(g, gpc, tpc);
		}
	}

	return ret;
}

#if defined(CONFIG_NVGPU_CHANNEL_TSG_CONTROL) && defined(CONFIG_NVGPU_DEBUGGER)
static void gr_intr_post_bpt_events(struct gk20a *g, struct nvgpu_tsg *tsg,
				    u32 global_esr)
{
	if (g->ops.gr.esr_bpt_pending_events(global_esr,
						NVGPU_EVENT_ID_BPT_INT)) {
		g->ops.tsg.post_event_id(tsg, NVGPU_EVENT_ID_BPT_INT);
	}

	if (g->ops.gr.esr_bpt_pending_events(global_esr,
						NVGPU_EVENT_ID_BPT_PAUSE)) {
		g->ops.tsg.post_event_id(tsg, NVGPU_EVENT_ID_BPT_PAUSE);
	}
}
#endif

static int gr_intr_handle_illegal_method(struct gk20a *g,
					  struct nvgpu_gr_isr_data *isr_data)
{
	int ret = g->ops.gr.intr.handle_sw_method(g, isr_data->addr,
			isr_data->class_num, isr_data->offset,
			isr_data->data_lo);
	if (ret != 0) {
		nvgpu_gr_intr_set_error_notifier(g, isr_data,
			 NVGPU_ERR_NOTIFIER_GR_ILLEGAL_NOTIFY);
		nvgpu_err(g, "invalid method class 0x%08x"
			", offset 0x%08x address 0x%08x",
			isr_data->class_num, isr_data->offset, isr_data->addr);
	}
	return ret;
}

static void gr_intr_handle_class_error(struct gk20a *g,
				       struct nvgpu_gr_isr_data *isr_data)
{
	u32 chid = (isr_data->ch != NULL) ?
		isr_data->ch->chid : NVGPU_INVALID_CHANNEL_ID;

	nvgpu_log_fn(g, " ");

	g->ops.gr.intr.handle_class_error(g, chid, isr_data);

	nvgpu_gr_intr_set_error_notifier(g, isr_data,
			 NVGPU_ERR_NOTIFIER_GR_ERROR_SW_NOTIFY);
}

/* Used by sw interrupt thread to translate current ctx to chid.
 * Also used by regops to translate current ctx to chid and tsgid.
 * For performance, we don't want to go through 128 channels every time.
 * curr_ctx should be the value read from gr falcon get_current_ctx op
 * A small tlb is used here to cache translation.
 *
 * Returned channel must be freed with nvgpu_channel_put() */
struct nvgpu_channel *nvgpu_gr_intr_get_channel_from_ctx(struct gk20a *g,
			u32 curr_ctx, u32 *curr_tsgid)
{
	struct nvgpu_fifo *f = &g->fifo;
	struct nvgpu_gr_intr *intr = nvgpu_gr_get_intr_ptr(g);
	u32 chid;
	u32 tsgid = NVGPU_INVALID_TSG_ID;
	u32 i;
	struct nvgpu_channel *ret_ch = NULL;

	/* when contexts are unloaded from GR, the valid bit is reset
	 * but the instance pointer information remains intact.
	 * This might be called from gr_isr where contexts might be
	 * unloaded. No need to check ctx_valid bit
	 */

	nvgpu_spinlock_acquire(&intr->ch_tlb_lock);

	/* check cache first */
	for (i = 0; i < GR_CHANNEL_MAP_TLB_SIZE; i++) {
		if (intr->chid_tlb[i].curr_ctx == curr_ctx) {
			chid = intr->chid_tlb[i].chid;
			tsgid = intr->chid_tlb[i].tsgid;
			ret_ch = nvgpu_channel_from_id(g, chid);
			goto unlock;
		}
	}

	/* slow path */
	for (chid = 0; chid < f->num_channels; chid++) {
		struct nvgpu_channel *ch = nvgpu_channel_from_id(g, chid);

		if (ch == NULL) {
			continue;
		}

		if (nvgpu_inst_block_ptr(g, &ch->inst_block) ==
				g->ops.gr.falcon.get_ctx_ptr(curr_ctx)) {
			tsgid = ch->tsgid;
			/* found it */
			ret_ch = ch;
			break;
		}
		nvgpu_channel_put(ch);
	}

	if (ret_ch == NULL) {
		goto unlock;
	}

	/* add to free tlb entry */
	for (i = 0; i < GR_CHANNEL_MAP_TLB_SIZE; i++) {
		if (intr->chid_tlb[i].curr_ctx == 0U) {
			intr->chid_tlb[i].curr_ctx = curr_ctx;
			intr->chid_tlb[i].chid = chid;
			intr->chid_tlb[i].tsgid = tsgid;
			goto unlock;
		}
	}

	/* no free entry, flush one */
	intr->chid_tlb[intr->channel_tlb_flush_index].curr_ctx = curr_ctx;
	intr->chid_tlb[intr->channel_tlb_flush_index].chid = chid;
	intr->chid_tlb[intr->channel_tlb_flush_index].tsgid = tsgid;

	intr->channel_tlb_flush_index =
		(nvgpu_safe_add_u32(intr->channel_tlb_flush_index, 1U)) &
		(nvgpu_safe_sub_u32(GR_CHANNEL_MAP_TLB_SIZE, 1U));

unlock:
	nvgpu_spinlock_release(&intr->ch_tlb_lock);
	*curr_tsgid = tsgid;
	return ret_ch;
}

void nvgpu_gr_intr_set_error_notifier(struct gk20a *g,
		  struct nvgpu_gr_isr_data *isr_data, u32 error_notifier)
{
	struct nvgpu_channel *ch;
	struct nvgpu_tsg *tsg;

	ch = isr_data->ch;

	if (ch == NULL) {
		return;
	}

	tsg = nvgpu_tsg_from_ch(ch);
	if (tsg != NULL) {
		nvgpu_tsg_set_error_notifier(g, tsg, error_notifier);
	} else {
		nvgpu_err(g, "chid: %d is not bound to tsg", ch->chid);
	}
}

static bool is_global_esr_error(u32 global_esr, u32 global_mask)
{
	return ((global_esr & ~global_mask) != 0U) ? true: false;
}

#ifdef CONFIG_NVGPU_DEBUGGER
static int gr_intr_sm_exception_warp_sync(struct gk20a *g,
		u32 gpc, u32 tpc, u32 sm,
		u32 global_esr, u32 warp_esr, u32 global_mask,
		bool ignore_debugger, bool *post_event)
{
	int ret = 0;
	bool do_warp_sync = false;

	if (!ignore_debugger && ((warp_esr != 0U) ||
			(is_global_esr_error(global_esr, global_mask)))) {
		nvgpu_log(g, gpu_dbg_intr, "warp sync needed");
		do_warp_sync = true;
	}

	if (do_warp_sync) {
		ret = g->ops.gr.lock_down_sm(g, gpc, tpc, sm,
				 global_mask, true);
		if (ret != 0) {
			nvgpu_err(g, "sm did not lock down!");
			return ret;
		}
	}

	if (ignore_debugger) {
		nvgpu_log(g, gpu_dbg_intr | gpu_dbg_gpu_dbg,
			"ignore_debugger set, skipping event posting");
	} else {
		*post_event = true;
	}

	return ret;
}
#endif

int nvgpu_gr_intr_handle_sm_exception(struct gk20a *g, u32 gpc, u32 tpc, u32 sm,
		bool *post_event, struct nvgpu_channel *fault_ch,
		u32 *hww_global_esr)
{
	int ret = 0;
	u32 gpc_offset = nvgpu_gr_gpc_offset(g, gpc);
	u32 tpc_offset = nvgpu_gr_tpc_offset(g, tpc);
	u32 offset = nvgpu_safe_add_u32(gpc_offset, tpc_offset);
	u32 global_esr, warp_esr, global_mask;
#ifdef CONFIG_NVGPU_DEBUGGER
	bool sm_debugger_attached;
	bool early_exit = false, ignore_debugger = false;
	bool disable_sm_exceptions = true;
#endif

	(void)post_event;

	nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gpu_dbg, " ");

	global_esr = g->ops.gr.intr.get_sm_hww_global_esr(g, gpc, tpc, sm);
	*hww_global_esr = global_esr;

	warp_esr = g->ops.gr.intr.get_sm_hww_warp_esr(g, gpc, tpc, sm);
	global_mask = g->ops.gr.intr.get_sm_no_lock_down_hww_global_esr_mask(g);

	nvgpu_log(g, gpu_dbg_intr | gpu_dbg_gpu_dbg,
		  "sm hww global 0x%08x warp 0x%08x", global_esr, warp_esr);

	/*
	 * Check and report any fatal warp errors.
	 */
	if (is_global_esr_error(global_esr, global_mask)) {
		nvgpu_report_err_to_sdl(g, NVGPU_ERR_MODULE_SM,
				GPU_SM_MACHINE_CHECK_ERROR);
		nvgpu_err(g, "sm machine check err. gpc_id(%d), tpc_id(%d), "
				"offset(%d)", gpc, tpc, offset);
	}

	(void)nvgpu_pg_elpg_protected_call(g,
		nvgpu_safe_cast_u32_to_s32(
		g->ops.gr.intr.record_sm_error_state(g, gpc, tpc,
							sm, fault_ch)));

#ifdef CONFIG_NVGPU_DEBUGGER
	sm_debugger_attached = g->ops.gr.sm_debugger_attached(g);
	if (!sm_debugger_attached) {
		nvgpu_err(g, "sm hww global 0x%08x warp 0x%08x",
			  global_esr, warp_esr);
		return -EFAULT;
	}

	if (g->ops.gr.pre_process_sm_exception != NULL) {
		ret = g->ops.gr.pre_process_sm_exception(g, gpc, tpc, sm,
				global_esr, warp_esr,
				sm_debugger_attached,
				fault_ch,
				&early_exit,
				&ignore_debugger);
		if (ret != 0) {
			nvgpu_err(g, "could not pre-process sm error!");
			return ret;
		}
	}

	if (early_exit) {
		nvgpu_log(g, gpu_dbg_intr | gpu_dbg_gpu_dbg,
				"returning early");
		return ret;
	}

	/*
	 * Disable forwarding of tpc exceptions,
	 * the debugger will reenable exceptions after servicing them.
	 *
	 * Do not disable exceptions if the only SM exception is BPT_INT
	 */
	if ((g->ops.gr.esr_bpt_pending_events(global_esr,
			NVGPU_EVENT_ID_BPT_INT)) && (warp_esr == 0U)) {
		disable_sm_exceptions = false;
	}

	if (!ignore_debugger && disable_sm_exceptions) {
		g->ops.gr.intr.tpc_exception_sm_disable(g, offset);
		nvgpu_log(g, gpu_dbg_intr | gpu_dbg_gpu_dbg,
			  "SM Exceptions disabled");
	}

	/* if debugger is present and an error has occurred, do a warp sync */
	ret = gr_intr_sm_exception_warp_sync(g, gpc, tpc, sm,
					global_esr, warp_esr, global_mask,
					ignore_debugger, post_event);
#else
	/* Return error so that recovery is triggered */
	ret = -EFAULT;
#endif

	return ret;
}

int nvgpu_gr_intr_handle_fecs_error(struct gk20a *g, struct nvgpu_channel *ch,
					struct nvgpu_gr_isr_data *isr_data)
{
	u32 gr_fecs_intr, mailbox_value;
	int ret = 0;
	u32 chid = (isr_data->ch != NULL) ?
		isr_data->ch->chid : NVGPU_INVALID_CHANNEL_ID;
	u32 mailbox_id = NVGPU_GR_FALCON_FECS_CTXSW_MAILBOX6;
	struct nvgpu_fecs_host_intr_status *fecs_host_intr;

	(void)ch;

	gr_fecs_intr = isr_data->fecs_intr;
	if (gr_fecs_intr == 0U) {
		return 0;
	}
	fecs_host_intr = &isr_data->fecs_host_intr_status;

	if (fecs_host_intr->unimp_fw_method_active) {
		mailbox_value = g->ops.gr.falcon.read_fecs_ctxsw_mailbox(g,
								mailbox_id);
		nvgpu_gr_intr_set_error_notifier(g, isr_data,
			 NVGPU_ERR_NOTIFIER_FECS_ERR_UNIMP_FIRMWARE_METHOD);
		nvgpu_err(g, "firmware method error: "
			"mailxbox6 0x%08x, trapped_addr_reg 0x%08x "
			"set_falcon_method 0x%08x, class 0x%08x "
			"non-whitelist reg: 0x%08x",
			mailbox_value, isr_data->addr,
			isr_data->offset << 2U, isr_data->class_num,
			isr_data->data_lo);
		ret = -1;
	}

	if (fecs_host_intr->ctxsw_intr0 != 0U) {
		mailbox_value = g->ops.gr.falcon.read_fecs_ctxsw_mailbox(g,
								mailbox_id);
#ifdef CONFIG_NVGPU_FECS_TRACE
		if (mailbox_value ==
			g->ops.gr.fecs_trace.get_buffer_full_mailbox_val()) {
			nvgpu_info(g, "ctxsw intr0 set by ucode, "
					"timestamp buffer full");
			nvgpu_gr_fecs_trace_reset_buffer(g);
		} else
#endif
		/*
		 * The mailbox values may vary across chips hence keeping it
		 * as a HAL.
		 */
		if ((g->ops.gr.intr.get_ctxsw_checksum_mismatch_mailbox_val != NULL)
			&& (mailbox_value ==
			g->ops.gr.intr.get_ctxsw_checksum_mismatch_mailbox_val())) {

			nvgpu_report_err_to_sdl(g, NVGPU_ERR_MODULE_FECS,
					GPU_FECS_CTXSW_CRC_MISMATCH);
			nvgpu_err(g, "ctxsw intr0 set by ucode, "
					"ctxsw checksum mismatch");
			ret = -1;
		} else {
			/*
			 * Other errors are also treated as fatal and channel
			 * recovery is initiated and error is reported to
			 * 3LSS.
			 */
			nvgpu_report_err_to_sdl(g, NVGPU_ERR_MODULE_FECS,
					GPU_FECS_FAULT_DURING_CTXSW);
			nvgpu_err(g,
				 "ctxsw intr0 set by ucode, error_code: 0x%08x,",
				 mailbox_value);
			ret = -1;
		}
	}

	if (fecs_host_intr->fault_during_ctxsw_active) {
		nvgpu_report_err_to_sdl(g, NVGPU_ERR_MODULE_FECS,
				GPU_FECS_FAULT_DURING_CTXSW);
		nvgpu_err(g, "fecs fault during ctxsw for channel %u", chid);
		ret = -1;
	}

	if (fecs_host_intr->watchdog_active) {
		nvgpu_report_err_to_sdl(g, NVGPU_ERR_MODULE_FECS,
				GPU_FECS_CTXSW_WATCHDOG_TIMEOUT);
		/* currently, recovery is not initiated */
		nvgpu_err(g, "fecs watchdog triggered for channel %u, "
				"cannot ctxsw anymore !!", chid);
		g->ops.gr.falcon.dump_stats(g);
	}

	/*
	 * un-supported interrupts will be flagged in
	 * g->ops.gr.falcon.fecs_host_intr_status.
	 */
	g->ops.gr.falcon.fecs_host_clear_intr(g, gr_fecs_intr);

	return ret;
}

static int gr_intr_check_handle_tpc_exception(struct gk20a *g, u32 gpc,
	u32 tpc_exception, bool *post_event, struct nvgpu_gr_config *gr_config,
	struct nvgpu_channel *fault_ch, u32 *hww_global_esr)
{
	int tmp_ret, ret = 0;
	u32 tpc;

	for (tpc = 0;
	     tpc < nvgpu_gr_config_get_gpc_tpc_count(gr_config, gpc);
	     tpc++) {
		if ((tpc_exception & BIT32(tpc)) == 0U) {
			continue;
		}

		nvgpu_log(g, gpu_dbg_intr | gpu_dbg_gpu_dbg,
			  "GPC%d: TPC%d exception pending", gpc, tpc);

		tmp_ret = gr_intr_handle_tpc_exception(g, gpc, tpc,
				post_event, fault_ch, hww_global_esr);
		ret = (ret != 0) ? ret : tmp_ret;
	}
	return ret;
}

int nvgpu_gr_intr_handle_gpc_exception(struct gk20a *g, bool *post_event,
	struct nvgpu_gr_config *gr_config, struct nvgpu_channel *fault_ch,
	u32 *hww_global_esr)
{
	int tmp_ret, ret = 0;
	u32 gpc;
	u32 exception1 = g->ops.gr.intr.read_exception1(g);
	u32 gpc_exception, tpc_exception;

	nvgpu_log(g, gpu_dbg_intr | gpu_dbg_gpu_dbg, " ");

	for (gpc = 0; gpc < nvgpu_gr_config_get_gpc_count(gr_config); gpc++) {
		if ((exception1 & BIT32(gpc)) == 0U) {
			continue;
		}

		nvgpu_log(g, gpu_dbg_intr | gpu_dbg_gpu_dbg,
				"GPC%d exception pending", gpc);
		gpc_exception = g->ops.gr.intr.read_gpc_exception(g, gpc);
		tpc_exception = g->ops.gr.intr.read_gpc_tpc_exception(
							gpc_exception);

		/* check and handle if any tpc has an exception */
		tmp_ret = gr_intr_check_handle_tpc_exception(g, gpc, tpc_exception,
			post_event, gr_config, fault_ch, hww_global_esr);

		ret = (ret != 0) ? ret : tmp_ret;

		/* Handle GCC exception */
		if (g->ops.gr.intr.handle_gcc_exception != NULL) {
			g->ops.gr.intr.handle_gcc_exception(g, gpc,
				gpc_exception,
				&g->ecc.gr.gcc_l15_ecc_corrected_err_count[gpc].counter,
				&g->ecc.gr.gcc_l15_ecc_uncorrected_err_count[gpc].counter);
		}

		/* Handle GPCCS exceptions */
		if (g->ops.gr.intr.handle_gpc_gpccs_exception != NULL) {
			g->ops.gr.intr.handle_gpc_gpccs_exception(g, gpc,
				gpc_exception,
				&g->ecc.gr.gpccs_ecc_corrected_err_count[gpc].counter,
				&g->ecc.gr.gpccs_ecc_uncorrected_err_count[gpc].counter);
		}

		/* Handle GPCMMU exceptions */
		if (g->ops.gr.intr.handle_gpc_gpcmmu_exception != NULL) {
			 g->ops.gr.intr.handle_gpc_gpcmmu_exception(g, gpc,
				gpc_exception,
				&g->ecc.gr.mmu_l1tlb_ecc_corrected_err_count[gpc].counter,
				&g->ecc.gr.mmu_l1tlb_ecc_uncorrected_err_count[gpc].counter);
		}

		/* Handle PROP exception */
		if (g->ops.gr.intr.handle_gpc_prop_exception != NULL) {
			 g->ops.gr.intr.handle_gpc_prop_exception(g, gpc,
				gpc_exception);
		}

		/* Handle ZCULL exception */
		if (g->ops.gr.intr.handle_gpc_zcull_exception != NULL) {
			 g->ops.gr.intr.handle_gpc_zcull_exception(g, gpc,
				gpc_exception);
		}

		/* Handle SETUP exception */
		if (g->ops.gr.intr.handle_gpc_setup_exception != NULL) {
			 g->ops.gr.intr.handle_gpc_setup_exception(g, gpc,
				gpc_exception);
		}

		/* Handle PES exception */
		if (g->ops.gr.intr.handle_gpc_pes_exception != NULL) {
			 g->ops.gr.intr.handle_gpc_pes_exception(g, gpc,
				gpc_exception);
		}

		/* Handle ZROP exception */
		if (g->ops.gr.intr.handle_gpc_zrop_hww != NULL) {
			 g->ops.gr.intr.handle_gpc_zrop_hww(g, gpc,
				gpc_exception);
		}

		/* Handle CROP exception */
		if (g->ops.gr.intr.handle_gpc_crop_hww != NULL) {
			 g->ops.gr.intr.handle_gpc_crop_hww(g, gpc,
				gpc_exception);
		}

		/* Handle RRH exception */
		if (g->ops.gr.intr.handle_gpc_rrh_hww != NULL) {
			 g->ops.gr.intr.handle_gpc_rrh_hww(g, gpc,
				gpc_exception);
		}

	}

	return ret;
}

void nvgpu_gr_intr_handle_notify_pending(struct gk20a *g,
					struct nvgpu_gr_isr_data *isr_data)
{
	struct nvgpu_channel *ch = isr_data->ch;
	int err;

	if (ch == NULL) {
		return;
	}

	if (nvgpu_tsg_from_ch(ch) == NULL) {
		return;
	}

	nvgpu_log_fn(g, " ");

#if defined(CONFIG_NVGPU_CYCLESTATS)
	nvgpu_cyclestats_exec(g, ch, isr_data->data_lo);
#endif

	err = nvgpu_cond_broadcast_interruptible(&ch->notifier_wq);
	if (err != 0) {
		nvgpu_log(g, gpu_dbg_intr, "failed to broadcast");
	}
}

void nvgpu_gr_intr_handle_semaphore_pending(struct gk20a *g,
					   struct nvgpu_gr_isr_data *isr_data)
{
	struct nvgpu_channel *ch = isr_data->ch;
	struct nvgpu_tsg *tsg;

	if (ch == NULL) {
		return;
	}

	tsg = nvgpu_tsg_from_ch(ch);
	if (tsg != NULL) {
		int err;

#ifdef CONFIG_NVGPU_CHANNEL_TSG_CONTROL
		g->ops.tsg.post_event_id(tsg,
			NVGPU_EVENT_ID_GR_SEMAPHORE_WRITE_AWAKEN);
#endif

		err = nvgpu_cond_broadcast(&ch->semaphore_wq);
		if (err != 0) {
			nvgpu_log(g, gpu_dbg_intr, "failed to broadcast");
		}
	} else {
		nvgpu_err(g, "chid: %d is not bound to tsg", ch->chid);
	}
}

#ifdef CONFIG_NVGPU_DEBUGGER
static void gr_intr_signal_exception_event(struct gk20a *g,
				bool post_event,
				struct nvgpu_channel *fault_ch)
{
	if (g->ops.gr.sm_debugger_attached(g) &&
		post_event && (fault_ch != NULL)) {
		g->ops.debugger.post_events(fault_ch);
	}
}
#endif

static u32 gr_intr_handle_exception_interrupts(struct gk20a *g,
		u32 *clear_intr,
		struct nvgpu_tsg *tsg, u32 *global_esr,
		struct nvgpu_gr_intr_info *intr_info,
		struct nvgpu_gr_isr_data *isr_data)
{
	struct nvgpu_channel *fault_ch = NULL;
	struct nvgpu_gr_config *gr_config = nvgpu_gr_get_config_ptr(g);
	bool need_reset = false;

	if (intr_info->exception != 0U) {
		bool is_gpc_exception = false;

		need_reset = g->ops.gr.intr.handle_exceptions(g,
							&is_gpc_exception);

		/* check if a gpc exception has occurred */
		if (is_gpc_exception &&	!need_reset) {
			bool post_event = false;

			nvgpu_log(g, gpu_dbg_intr | gpu_dbg_gpu_dbg,
					 "GPC exception pending");

			if (tsg != NULL) {
				fault_ch = isr_data->ch;
			}

			/* fault_ch can be NULL */
			/* check if any gpc has an exception */
			if (nvgpu_gr_intr_handle_gpc_exception(g, &post_event,
				gr_config, fault_ch, global_esr) != 0) {
				need_reset = true;
			}

#ifdef CONFIG_NVGPU_DEBUGGER
			/* signal clients waiting on an event */
			gr_intr_signal_exception_event(g,
						post_event, fault_ch);
#endif
		}
		*clear_intr &= ~intr_info->exception;

		if (need_reset) {
			nvgpu_err(g, "set gr exception notifier");
			nvgpu_gr_intr_set_error_notifier(g, isr_data,
					 NVGPU_ERR_NOTIFIER_GR_EXCEPTION);
		}
	}

	return (need_reset)? 1U : 0U;
}

static u32 gr_intr_handle_illegal_interrupts(struct gk20a *g,
		u32 *clear_intr,
		struct nvgpu_gr_intr_info *intr_info,
		struct nvgpu_gr_isr_data *isr_data)
{
	u32 do_reset = 0U;

	if (intr_info->illegal_notify != 0U) {
		nvgpu_report_err_to_sdl(g, NVGPU_ERR_MODULE_PGRAPH,
				GPU_PGRAPH_ILLEGAL_NOTIFY_ERROR);
		nvgpu_gr_intr_set_error_notifier(g, isr_data,
				NVGPU_ERR_NOTIFIER_GR_ILLEGAL_NOTIFY);
		nvgpu_err(g, "illegal notify pending");
		do_reset = 1U;
		*clear_intr &= ~intr_info->illegal_notify;
	}

	if (intr_info->illegal_method != 0U) {
		if (gr_intr_handle_illegal_method(g, isr_data) != 0) {
			nvgpu_report_err_to_sdl(g, NVGPU_ERR_MODULE_PGRAPH,
					GPU_PGRAPH_ILLEGAL_METHOD_ERROR);
			nvgpu_err(g, "illegal method");

			do_reset = 1U;
		}
		*clear_intr &= ~intr_info->illegal_method;
	}

	if (intr_info->illegal_class != 0U) {
		nvgpu_report_err_to_sdl(g, NVGPU_ERR_MODULE_PGRAPH,
				GPU_PGRAPH_ILLEGAL_CLASS_ERROR);
		nvgpu_err(g, "invalid class 0x%08x, offset 0x%08x",
			  isr_data->class_num, isr_data->offset);

		nvgpu_gr_intr_set_error_notifier(g, isr_data,
				NVGPU_ERR_NOTIFIER_GR_ERROR_SW_NOTIFY);
		do_reset = 1U;
		*clear_intr &= ~intr_info->illegal_class;
	}
	return do_reset;
}

static u32 gr_intr_handle_error_interrupts(struct gk20a *g,
		u32 *clear_intr,
		struct nvgpu_gr_intr_info *intr_info,
		struct nvgpu_gr_isr_data *isr_data)
{
	u32 do_reset = 0U;

	if (intr_info->fecs_error != 0U) {
		isr_data->fecs_intr = g->ops.gr.falcon.fecs_host_intr_status(g,
					&(isr_data->fecs_host_intr_status));
		if (g->ops.gr.intr.handle_fecs_error(g,
				isr_data->ch, isr_data) != 0) {
			do_reset = 1U;
		}
		*clear_intr &= ~intr_info->fecs_error;
	}

	if (intr_info->class_error != 0U) {
		nvgpu_report_err_to_sdl(g, NVGPU_ERR_MODULE_PGRAPH,
				GPU_PGRAPH_CLASS_ERROR);
		nvgpu_err(g, "class error");
		gr_intr_handle_class_error(g, isr_data);
		do_reset = 1U;
		*clear_intr &= ~intr_info->class_error;
	}

	/* this one happens if someone tries to hit a non-whitelisted
	 * register using set_falcon[4] */
	if (intr_info->fw_method != 0U) {
		u32 ch_id = (isr_data->ch != NULL) ?
			isr_data->ch->chid : NVGPU_INVALID_CHANNEL_ID;
		nvgpu_err(g,
		   "firmware method 0x%08x, offset 0x%08x for channel %u",
		   isr_data->class_num, isr_data->offset,
		   ch_id);

		nvgpu_gr_intr_set_error_notifier(g, isr_data,
			 NVGPU_ERR_NOTIFIER_GR_ERROR_SW_NOTIFY);
		do_reset = 1U;
		*clear_intr &= ~intr_info->fw_method;
	}
	return do_reset;
}

#ifdef CONFIG_NVGPU_NON_FUSA
static void gr_intr_handle_pending_interrupts(struct gk20a *g,
		u32 *clear_intr,
		struct nvgpu_gr_intr_info *intr_info,
		struct nvgpu_gr_isr_data *isr_data)
{
	if (intr_info->notify != 0U) {
		g->ops.gr.intr.handle_notify_pending(g, isr_data);
		*clear_intr &= ~intr_info->notify;
	}

	if (intr_info->semaphore != 0U) {
		g->ops.gr.intr.handle_semaphore_pending(g, isr_data);
		*clear_intr &= ~intr_info->semaphore;
	}

	if (intr_info->buffer_notify != 0U) {
		/*
		 * This notifier event is ignored at present as there is no
		 * real usecase.
		 */
		nvgpu_log(g, gpu_dbg_intr, "buffer notify interrupt");
		*clear_intr &= ~intr_info->buffer_notify;
	}

	if (intr_info->debug_method != 0U) {
		nvgpu_warn(g, "dropping method(0x%x) on subchannel(%d)",
				isr_data->offset, isr_data->sub_chan);

		*clear_intr &= ~intr_info->debug_method;
	}
}
#endif

static struct nvgpu_tsg *gr_intr_get_channel_from_ctx(struct gk20a *g,
			u32 gr_intr, u32 *chid,
			struct nvgpu_gr_isr_data *isr_data)
{
	struct nvgpu_channel *ch = NULL;
	u32 tsgid = NVGPU_INVALID_TSG_ID;
	struct nvgpu_tsg *tsg_info = NULL;
	u32 channel_id;

	ch = nvgpu_gr_intr_get_channel_from_ctx(g, isr_data->curr_ctx, &tsgid);
	isr_data->ch = ch;
	channel_id = (ch != NULL) ? ch->chid : NVGPU_INVALID_CHANNEL_ID;

	if (ch == NULL) {
		nvgpu_err(g,
			"pgraph intr: 0x%08x, channel_id: INVALID", gr_intr);
	} else {
		tsg_info = nvgpu_tsg_from_ch(ch);
		if (tsg_info == NULL) {
			nvgpu_err(g, "pgraph intr: 0x%08x, channel_id: %d "
				"not bound to tsg", gr_intr, channel_id);
		}
	}

	nvgpu_log(g, gpu_dbg_intr | gpu_dbg_gpu_dbg,
		"channel %d: addr 0x%08x, "
		"data 0x%08x 0x%08x,"
		"ctx 0x%08x, offset 0x%08x, "
		"subchannel 0x%08x, class 0x%08x",
		channel_id, isr_data->addr,
		isr_data->data_hi, isr_data->data_lo,
		isr_data->curr_ctx, isr_data->offset,
		isr_data->sub_chan, isr_data->class_num);

	*chid = channel_id;

	return tsg_info;
}

static void gr_clear_intr_status(struct gk20a *g,
			    struct nvgpu_gr_isr_data *isr_data,
			    u32 clear_intr, u32 gr_intr, u32 chid)
{
	if (clear_intr != 0U) {
		if (isr_data->ch == NULL) {
			/*
			 * This is probably an interrupt during
			 * gk20a_free_channel()
			 */
			nvgpu_err(g, "unhandled gr intr 0x%08x for "
				"unreferenceable channel, clearing",
				gr_intr);
		} else {
			nvgpu_err(g, "unhandled gr intr 0x%08x for chid %u",
				gr_intr, chid);
		}
	}
}

int nvgpu_gr_intr_stall_isr(struct gk20a *g)
{
	struct nvgpu_gr_isr_data isr_data;
	struct nvgpu_gr_intr_info intr_info;
	u32 need_reset = 0U;
	struct nvgpu_tsg *tsg = NULL;
	u32 global_esr = 0;
	u32 chid = NVGPU_INVALID_CHANNEL_ID;
	u32 gr_intr = g->ops.gr.intr.read_pending_interrupts(g, &intr_info);
	u32 clear_intr = gr_intr;

	nvgpu_log_fn(g, " ");
	nvgpu_log(g, gpu_dbg_intr, "pgraph intr 0x%08x", gr_intr);

	if (gr_intr == 0U) {
		return 0;
	}

	(void) memset(&isr_data, 0, sizeof(struct nvgpu_gr_isr_data));

	/* Disable fifo access */
	g->ops.gr.init.fifo_access(g, false);

	g->ops.gr.intr.trapped_method_info(g, &isr_data);

	if (isr_data.curr_ctx != 0U) {
		tsg = gr_intr_get_channel_from_ctx(g, gr_intr, &chid,
							&isr_data);
	}

#ifdef CONFIG_NVGPU_NON_FUSA
	gr_intr_handle_pending_interrupts(g, &clear_intr,
					&intr_info, &isr_data);
#endif
	need_reset |= gr_intr_handle_illegal_interrupts(g,
				&clear_intr, &intr_info, &isr_data);

	need_reset |= gr_intr_handle_error_interrupts(g,
				&clear_intr, &intr_info, &isr_data);

	need_reset |= gr_intr_handle_exception_interrupts(g, &clear_intr,
				tsg, &global_esr, &intr_info, &isr_data);

	if (need_reset != 0U) {
		nvgpu_rc_gr_fault(g, tsg, isr_data.ch);
	}

	gr_clear_intr_status(g, &isr_data, clear_intr, gr_intr, chid);

	/* clear handled and unhandled interrupts */
	g->ops.gr.intr.clear_pending_interrupts(g, gr_intr);

	/* Enable fifo access */
	g->ops.gr.init.fifo_access(g, true);

#if defined(CONFIG_NVGPU_CHANNEL_TSG_CONTROL) && defined(CONFIG_NVGPU_DEBUGGER)
	/* Posting of BPT events should be the last thing in this function */
	if ((global_esr != 0U) && (tsg != NULL) && (need_reset == 0U)) {
		gr_intr_post_bpt_events(g, tsg, global_esr);
	}
#endif

	if (isr_data.ch != NULL) {
		nvgpu_channel_put(isr_data.ch);
	}

	return 0;
}

/* invalidate channel lookup tlb */
void nvgpu_gr_intr_flush_channel_tlb(struct gk20a *g)
{
	struct nvgpu_gr_intr *intr = nvgpu_gr_get_intr_ptr(g);

	nvgpu_spinlock_acquire(&intr->ch_tlb_lock);
	(void) memset(intr->chid_tlb, 0,
		sizeof(struct gr_channel_map_tlb_entry) *
		GR_CHANNEL_MAP_TLB_SIZE);
	nvgpu_spinlock_release(&intr->ch_tlb_lock);
}

struct nvgpu_gr_intr *nvgpu_gr_intr_init_support(struct gk20a *g)
{
	struct nvgpu_gr_intr *intr;

	nvgpu_log_fn(g, " ");

	intr = nvgpu_kzalloc(g, sizeof(*intr));
	if (intr == NULL) {
		return intr;
	}

	nvgpu_spinlock_init(&intr->ch_tlb_lock);

	return intr;
}

void nvgpu_gr_intr_remove_support(struct gk20a *g, struct nvgpu_gr_intr *intr)
{
	nvgpu_log_fn(g, " ");

	if (intr == NULL) {
		return;
	}
	nvgpu_kfree(g, intr);
}
