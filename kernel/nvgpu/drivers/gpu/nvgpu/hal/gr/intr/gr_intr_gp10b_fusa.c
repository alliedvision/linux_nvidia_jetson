/*
 * Copyright (c) 2019-2020, NVIDIA CORPORATION.  All rights reserved.
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
#include <nvgpu/io.h>
#include <nvgpu/class.h>
#include <nvgpu/channel.h>
#include <nvgpu/static_analysis.h>

#include <nvgpu/gr/config.h>
#include <nvgpu/gr/gr.h>
#include <nvgpu/gr/ctx.h>
#include <nvgpu/gr/gr_falcon.h>
#include <nvgpu/gr/gr_intr.h>
#include <nvgpu/gr/gr_utils.h>

#include "common/gr/gr_intr_priv.h"

#include "gr_intr_gp10b.h"

#include <nvgpu/hw/gp10b/hw_gr_gp10b.h>

void gp10b_gr_intr_handle_class_error(struct gk20a *g, u32 chid,
				      struct nvgpu_gr_isr_data *isr_data)
{
	u32 gr_class_error;
	u32 offset_bit_shift = 2U;
	u32 data_hi_set = 0U;

	gr_class_error =
		gr_class_error_code_v(nvgpu_readl(g, gr_class_error_r()));

	nvgpu_err(g, "class error 0x%08x, offset 0x%08x,"
		"sub channel 0x%08x mme generated %d,"
		" mme pc 0x%08xdata high %d priv status %d"
		" unhandled intr 0x%08x for channel %u",
		isr_data->class_num, (isr_data->offset << offset_bit_shift),
		gr_trapped_addr_subch_v(isr_data->addr),
		gr_trapped_addr_mme_generated_v(isr_data->addr),
		gr_trapped_data_mme_pc_v(
			nvgpu_readl(g, gr_trapped_data_mme_r())),
		gr_trapped_addr_datahigh_v(isr_data->addr),
		gr_trapped_addr_priv_v(isr_data->addr),
		gr_class_error, chid);

	nvgpu_err(g, "trapped data low 0x%08x",
		nvgpu_readl(g, gr_trapped_data_lo_r()));
	data_hi_set = gr_trapped_addr_datahigh_v(isr_data->addr);
	if (data_hi_set != 0U) {
		nvgpu_err(g, "trapped data high 0x%08x",
		nvgpu_readl(g, gr_trapped_data_hi_r()));
	}
}

#ifdef CONFIG_NVGPU_CILP
static int gp10b_gr_intr_clear_cilp_preempt_pending(struct gk20a *g,
					       struct nvgpu_channel *fault_ch)
{
	struct nvgpu_tsg *tsg;
	struct nvgpu_gr_ctx *gr_ctx;

	nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gpu_dbg | gpu_dbg_intr, " ");

	tsg = nvgpu_tsg_from_ch(fault_ch);
	if (tsg == NULL) {
		return -EINVAL;
	}

	gr_ctx = tsg->gr_ctx;

	/*
	 * The ucode is self-clearing, so all we need to do here is
	 * to clear cilp_preempt_pending.
	 */
	if (!nvgpu_gr_ctx_get_cilp_preempt_pending(gr_ctx)) {
		nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gpu_dbg | gpu_dbg_intr,
				"CILP is already cleared for chid %d\n",
				fault_ch->chid);
		return 0;
	}

	nvgpu_gr_ctx_set_cilp_preempt_pending(gr_ctx, false);
	nvgpu_gr_clear_cilp_preempt_pending_chid(g);

	return 0;
}

static int gp10b_gr_intr_get_cilp_preempt_pending_chid(struct gk20a *g,
					u32 *chid_ptr)
{
	struct nvgpu_gr_ctx *gr_ctx;
	struct nvgpu_channel *ch;
	struct nvgpu_tsg *tsg;
	u32 chid;
	int ret = -EINVAL;

	chid = nvgpu_gr_get_cilp_preempt_pending_chid(g);
	if (chid == NVGPU_INVALID_CHANNEL_ID) {
		return ret;
	}

	ch = nvgpu_channel_from_id(g, chid);
	if (ch == NULL) {
		return ret;
	}

	tsg = nvgpu_tsg_from_ch(ch);
	if (tsg == NULL) {
		nvgpu_channel_put(ch);
		return -EINVAL;
	}

	gr_ctx = tsg->gr_ctx;

	if (nvgpu_gr_ctx_get_cilp_preempt_pending(gr_ctx)) {
		*chid_ptr = chid;
		ret = 0;
	}

	nvgpu_channel_put(ch);

	return ret;
}
#endif /* CONFIG_NVGPU_CILP */

int gp10b_gr_intr_handle_fecs_error(struct gk20a *g,
				struct nvgpu_channel *ch_ptr,
				struct nvgpu_gr_isr_data *isr_data)
{
#ifdef CONFIG_NVGPU_CILP
	struct nvgpu_channel *ch;
	u32 chid = NVGPU_INVALID_CHANNEL_ID;
	int ret = 0;
	struct nvgpu_fecs_host_intr_status *fecs_host_intr;
#ifdef CONFIG_NVGPU_CHANNEL_TSG_CONTROL
	struct nvgpu_tsg *tsg;
#endif
#endif
	u32 gr_fecs_intr = isr_data->fecs_intr;

	nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gpu_dbg | gpu_dbg_intr, " ");

	if (gr_fecs_intr == 0U) {
		return 0;
	}


#ifdef CONFIG_NVGPU_CILP
	fecs_host_intr = &isr_data->fecs_host_intr_status;
	/*
	 * INTR1 (bit 1 of the HOST_INT_STATUS_CTXSW_INTR)
	 * indicates that a CILP ctxsw save has finished
	 */
	if (fecs_host_intr->ctxsw_intr1 != 0U) {
		nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gpu_dbg | gpu_dbg_intr,
				"CILP: ctxsw save completed!\n");

		/* now clear the interrupt */
		g->ops.gr.falcon.fecs_host_clear_intr(g,
					fecs_host_intr->ctxsw_intr1);
		/**
		 * clear the interrupt from isr_data too. This is
		 * for nvgpu_gr_intr_handle_fecs_error to not handle
		 * already handled interrupt.
		 */
		isr_data->fecs_intr &= ~(fecs_host_intr->ctxsw_intr1);
		fecs_host_intr->ctxsw_intr1 = 0U;

		ret = gp10b_gr_intr_get_cilp_preempt_pending_chid(g, &chid);
		if ((ret != 0) || (chid == NVGPU_INVALID_CHANNEL_ID)) {
			goto clean_up;
		}

		ch = nvgpu_channel_from_id(g, chid);
		if (ch == NULL) {
			goto clean_up;
		}

		/* set preempt_pending to false */
		ret = gp10b_gr_intr_clear_cilp_preempt_pending(g, ch);
		if (ret != 0) {
			nvgpu_err(g, "CILP: error while unsetting CILP preempt pending!");
			nvgpu_channel_put(ch);
			goto clean_up;
		}

#ifdef CONFIG_NVGPU_DEBUGGER
		/* Post events to UMD */
		g->ops.debugger.post_events(ch);
#endif

#ifdef CONFIG_NVGPU_CHANNEL_TSG_CONTROL
		tsg = &g->fifo.tsg[ch->tsgid];
		g->ops.tsg.post_event_id(tsg,
				NVGPU_EVENT_ID_CILP_PREEMPTION_COMPLETE);
#endif

		nvgpu_channel_put(ch);
	}

clean_up:
#endif /* CONFIG_NVGPU_CILP */

	/* handle any remaining interrupts */
	return nvgpu_gr_intr_handle_fecs_error(g, ch_ptr, isr_data);
}

#if defined(CONFIG_NVGPU_DEBUGGER) && defined(CONFIG_NVGPU_GRAPHICS)
void gp10b_gr_intr_set_go_idle_timeout(struct gk20a *g, u32 data)
{
	nvgpu_writel(g, gr_fe_go_idle_timeout_r(), data);
}

void gp10b_gr_intr_set_coalesce_buffer_size(struct gk20a *g, u32 data)
{
	u32 val;

	nvgpu_log_fn(g, " ");

	val = nvgpu_readl(g, gr_gpcs_tc_debug0_r());
	val = set_field(val, gr_gpcs_tc_debug0_limit_coalesce_buffer_size_m(),
			gr_gpcs_tc_debug0_limit_coalesce_buffer_size_f(data));
	nvgpu_writel(g, gr_gpcs_tc_debug0_r(), val);

	nvgpu_log_fn(g, "done");
}
#endif
