/*
 * Copyright (c) 2020-2022, NVIDIA CORPORATION.  All rights reserved.
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
#include <nvgpu/log.h>
#include <nvgpu/io.h>
#include <nvgpu/soc.h>
#include <nvgpu/ptimer.h>
#include <nvgpu/tsg.h>
#include <nvgpu/rc.h>
#include <nvgpu/nvgpu_err.h>
#include <nvgpu/engines.h>
#include <nvgpu/device.h>
#include <nvgpu/runlist.h>
#include <nvgpu/static_analysis.h>

#include "fifo_utils_ga10b.h"
#include "ctxsw_timeout_ga10b.h"

#include <nvgpu/hw/ga10b/hw_runlist_ga10b.h>

static void ga10b_fifo_ctxsw_timeout_clear_and_enable(struct gk20a *g,
			u32 timeout)
{
	u32 i, rleng;
	struct nvgpu_runlist *runlist;
	const struct nvgpu_device *dev;

	for (i = 0U; i < g->fifo.num_runlists; i++) {
		runlist = &g->fifo.active_runlists[i];
		for (rleng = 0U;
			rleng < runlist_engine_ctxsw_timeout_config__size_1_v();
			rleng++) {
			/* clear ctxsw timeout interrupt */
			nvgpu_runlist_writel(g, runlist,
				runlist_intr_0_r(),
				runlist_intr_0_ctxsw_timeout_eng_reset_f(rleng));

			dev = runlist->rl_dev_list[rleng];
			if (dev == NULL) {
				continue;
			}
			/* enable ctxsw timeout interrupt */
			nvgpu_runlist_writel(g, runlist,
				runlist_engine_ctxsw_timeout_config_r(
				dev->rleng_id),
				timeout);
			nvgpu_log_info(g, "ctxsw timeout enable "
				       "rleng: %u timeout_config_val: 0x%08x",
				       dev->rleng_id, timeout);
		}
	}
}

static void ga10b_fifo_ctxsw_timeout_disable_and_clear(struct gk20a *g,
			u32 timeout)
{
	u32 i, rleng;
	struct nvgpu_runlist *runlist;

	for (i = 0U; i < g->fifo.num_runlists; i++) {
		runlist = &g->fifo.active_runlists[i];
		for (rleng = 0U;
			rleng < runlist_engine_ctxsw_timeout_config__size_1_v();
			rleng++) {
			/* disable ctxsw timeout interrupt */
			nvgpu_runlist_writel(g, runlist,
				runlist_engine_ctxsw_timeout_config_r(rleng),
				timeout);
			/* clear ctxsw timeout interrupt */
			nvgpu_runlist_writel(g, runlist,
				runlist_intr_0_r(),
				runlist_intr_0_ctxsw_timeout_eng_reset_f(rleng));
		}
	}
}

void ga10b_fifo_ctxsw_timeout_enable(struct gk20a *g, bool enable)
{
	u32 timeout;
	u32 scaled_timeout;

	nvgpu_log_fn(g, " ");

	if (enable) {
		if (nvgpu_platform_is_silicon(g)) {
			timeout = nvgpu_safe_mult_u32(
					g->ctxsw_timeout_period_ms, MS_TO_US);
			nvgpu_assert(nvgpu_ptimer_scale(g, timeout, &scaled_timeout) == 0);
			timeout =
			 runlist_engine_ctxsw_timeout_config_period_f(scaled_timeout) |
			 runlist_engine_ctxsw_timeout_config_detection_enabled_f();
		} else {
			timeout =
			 runlist_engine_ctxsw_timeout_config_period_max_f() |
			 runlist_engine_ctxsw_timeout_config_detection_enabled_f();
		}
		ga10b_fifo_ctxsw_timeout_clear_and_enable(g, timeout);

	} else {
		timeout =
			runlist_engine_ctxsw_timeout_config_detection_disabled_f();
		timeout |=
			runlist_engine_ctxsw_timeout_config_period_max_f();

		ga10b_fifo_ctxsw_timeout_disable_and_clear(g, timeout);
	}
}

static u32 ga10b_fifo_ctxsw_timeout_info(struct gk20a *g,
		struct nvgpu_runlist *runlist,
		u32 rleng_id, u32 *info_status)
{
	u32 tsgid = NVGPU_INVALID_TSG_ID;
	u32 info;
	u32 ctx_status;

	info = nvgpu_runlist_readl(g, runlist,
			 runlist_engine_ctxsw_timeout_info_r(rleng_id));

	/*
	 * ctxsw_state and tsgid are snapped at the point of the timeout and
	 * will not change while the corresponding INTR_CTXSW_TIMEOUT_ENGINE bit
	 * is PENDING.
	 */
	ctx_status = runlist_engine_ctxsw_timeout_info_ctxsw_state_v(info);
	if (ctx_status ==
		runlist_engine_ctxsw_timeout_info_ctxsw_state_load_v()) {

		tsgid = runlist_engine_ctxsw_timeout_info_next_tsgid_v(info);
	} else if ((ctx_status ==
			runlist_engine_ctxsw_timeout_info_ctxsw_state_switch_v()) ||
		(ctx_status ==
			runlist_engine_ctxsw_timeout_info_ctxsw_state_save_v())) {

		tsgid = runlist_engine_ctxsw_timeout_info_prev_tsgid_v(info);
	} else {
		nvgpu_log_info(g, "ctxsw_timeout_info_ctxsw_state: 0x%08x",
			ctx_status);
	}
	nvgpu_log_info(g, "ctxsw timeout info: tsgid = %d", tsgid);

	/*
	 * STATUS indicates whether the context request ack was eventually
	 * received and whether a subsequent request timed out.  This field is
	 * updated live while the corresponding INTR_CTXSW_TIMEOUT_ENGINE bit
	 * is PENDING. STATUS starts in AWAITING_ACK, and progresses to
	 * ACK_RECEIVED and finally ends with DROPPED_TIMEOUT.
	 *
	 * AWAITING_ACK - context request ack still not returned from engine.
	 * ENG_WAS_RESET - The engine was reset via a PRI write to NV_PMC_ENABLE
	 * or NV_PMC_ELPG_ENABLE prior to receiving the ack.  Host will not
	 * expect ctx ack to return, but if it is already in flight, STATUS will
	 * transition shortly to ACK_RECEIVED unless the interrupt is cleared
	 * first.  Once the engine is reset, additional context switches can
	 * occur; if one times out, STATUS will transition to DROPPED_TIMEOUT
	 * if the interrupt isn't cleared first.
	 * ACK_RECEIVED - The ack for the timed-out context request was
	 * received between the point of the timeout and this register being
	 * read.  Note this STATUS can be reported during the load stage of the
	 * same context switch that timed out if the timeout occurred during the
	 * save half of a context switch.  Additional context requests may have
	 * completed or may be outstanding, but no further context timeout has
	 * occurred.  This simplifies checking for spurious context switch
	 * timeouts.
	 * DROPPED_TIMEOUT - The originally timed-out context request acked,
	 * but a subsequent context request then timed out.
	 * Information about the subsequent timeout is not stored; in fact, that
	 * context request may also have already been acked by the time SW
	 * SW reads this register.  If not, there is a chance SW can get the
	 * dropped information by clearing the corresponding
	 * INTR_CTXSW_TIMEOUT_ENGINE bit and waiting for the timeout to occur
	 * again. Note, however, that if the engine does time out again,
	 * it may not be from the  original request that caused the
	 * DROPPED_TIMEOUT state, as that request may
	 * be acked in the interim.
	 */
	*info_status = runlist_engine_ctxsw_timeout_info_status_v(info);
	if (*info_status ==
		 runlist_engine_ctxsw_timeout_info_status_ack_received_v()) {

		nvgpu_log_info(g, "ctxsw timeout info: ack received");
		/* no need to recover */
		tsgid = NVGPU_INVALID_TSG_ID;
	} else if (*info_status ==
		runlist_engine_ctxsw_timeout_info_status_dropped_timeout_v()) {

		nvgpu_log_info(g, "ctxsw timeout info: dropped timeout");
		/* no need to recover */
		tsgid = NVGPU_INVALID_TSG_ID;
	} else {
		nvgpu_log_info(g, "ctxsw timeout info status: 0x%08x",
			*info_status);
	}

	return tsgid;
}

void ga10b_fifo_ctxsw_timeout_isr(struct gk20a *g,
			struct nvgpu_runlist *runlist)
{
	u32 rleng, reg_val, timeout;
	u32 ms = 0U;
#ifdef CONFIG_NVGPU_KERNEL_MODE_SUBMIT
	u32 active_eng_id;
	bool recover = false;
#endif
	u32 info_status;
	u32 tsgid = NVGPU_INVALID_TSG_ID;
	const struct nvgpu_device *dev;
	struct nvgpu_tsg *tsg = NULL;

#ifdef CONFIG_NVGPU_KERNEL_MODE_SUBMIT
	bool debug_dump = false;
	const char *const ctxsw_timeout_status_desc[] = {
		"awaiting ack",
		"eng was reset",
		"ack received",
		"dropped timeout"
	};
#endif

	for (rleng = 0U;
		rleng < runlist_engine_ctxsw_timeout_info__size_1_v();
		rleng++) {
		reg_val = nvgpu_runlist_readl(g, runlist, runlist_intr_0_r());
		if ((reg_val &
			runlist_intr_0_ctxsw_timeout_eng_pending_f(rleng))
				== 0U) {
			/* ctxsw timeout not pending for this rleng */
			continue;
		}
		dev = runlist->rl_dev_list[rleng];
		if (dev == NULL) {
			nvgpu_err(g, "ctxsw timeout for rleng: %u but "
				"dev is invalid", rleng);
			/* interupt will still be cleared */
			continue;
		}
		/* dump ctxsw timeout for rleng. useful for debugging */
		reg_val = nvgpu_runlist_readl(g, runlist,
			runlist_engine_ctxsw_timeout_config_r(
			dev->rleng_id));
		timeout = runlist_engine_ctxsw_timeout_config_period_v(reg_val);
		nvgpu_log_info(g, "rleng: %u ctxsw timeout period = 0x%x",
			dev->rleng_id, timeout);

		/* handle ctxsw timeout */
		tsgid = ga10b_fifo_ctxsw_timeout_info(g, runlist, rleng,
						&info_status);
		tsg = nvgpu_tsg_check_and_get_from_id(g, tsgid);
		if (tsg == NULL) {
			continue;
		}

		nvgpu_report_err_to_sdl(g, NVGPU_ERR_MODULE_HOST,
				GPU_HOST_PFIFO_CTXSW_TIMEOUT_ERROR);
		nvgpu_err (g, "Host pfifo ctxsw timeout error");

#ifdef CONFIG_NVGPU_KERNEL_MODE_SUBMIT
		recover = g->ops.tsg.check_ctxsw_timeout(tsg, &debug_dump, &ms);
		if (recover) {
			const char *info_status_str = "invalid";
			if (info_status <
				ARRAY_SIZE(ctxsw_timeout_status_desc)) {
				info_status_str =
				ctxsw_timeout_status_desc[info_status];
			}
			active_eng_id = dev->engine_id;
			nvgpu_err(g, "ctxsw timeout error: "
				"active engine id =%u, %s=%d, info: %s ms=%u",
				active_eng_id, "tsg", tsgid, info_status_str,
				ms);
			if (active_eng_id != NVGPU_INVALID_ENG_ID) {
				nvgpu_rc_ctxsw_timeout(g, BIT32(active_eng_id),
					tsg, debug_dump);
			}
			continue;
		}
#endif
		nvgpu_log_info(g, "fifo is waiting for ctxsw switch: "
			"for %d ms, %s=%d", ms, "tsg", tsgid);
	}
}
