/*
 * Copyright (c) 2016-2022, NVIDIA CORPORATION.  All rights reserved.
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
#include <nvgpu/device.h>

#include <hal/fifo/ctxsw_timeout_gv11b.h>

#include <nvgpu/hw/gv11b/hw_fifo_gv11b.h>

void gv11b_fifo_ctxsw_timeout_enable(struct gk20a *g, bool enable)
{
	u32 timeout;
	u32 scaled_timeout;

	if (enable) {
		/* clear ctxsw timeout interrupts */
		nvgpu_writel(g, fifo_intr_ctxsw_timeout_r(), ~U32(0U));

		if (nvgpu_platform_is_silicon(g)) {
			timeout = g->ctxsw_timeout_period_ms * 1000U;
			nvgpu_assert(nvgpu_ptimer_scale(g, timeout, &scaled_timeout) == 0);
			scaled_timeout |= fifo_eng_ctxsw_timeout_detection_enabled_f();
			nvgpu_writel(g, fifo_eng_ctxsw_timeout_r(), scaled_timeout);
		} else {
			timeout = nvgpu_readl(g, fifo_eng_ctxsw_timeout_r());
			nvgpu_log_info(g,
				"fifo_eng_ctxsw_timeout reg val = 0x%08x",
				 timeout);
			timeout = set_field(timeout,
					fifo_eng_ctxsw_timeout_period_m(),
					fifo_eng_ctxsw_timeout_period_max_f());
			timeout = set_field(timeout,
					fifo_eng_ctxsw_timeout_detection_m(),
					fifo_eng_ctxsw_timeout_detection_disabled_f());
			nvgpu_log_info(g,
				"new fifo_eng_ctxsw_timeout reg val = 0x%08x",
				 timeout);
			nvgpu_writel(g, fifo_eng_ctxsw_timeout_r(), timeout);
		}

	} else {
		timeout = nvgpu_readl(g, fifo_eng_ctxsw_timeout_r());
		timeout = set_field(timeout,
				fifo_eng_ctxsw_timeout_detection_m(),
				fifo_eng_ctxsw_timeout_detection_disabled_f());
		nvgpu_writel(g, fifo_eng_ctxsw_timeout_r(), timeout);
		timeout = nvgpu_readl(g, fifo_eng_ctxsw_timeout_r());
		nvgpu_log_info(g,
			"fifo_eng_ctxsw_timeout disabled val = 0x%08x",
			timeout);
		/* clear ctxsw timeout interrupts */
		nvgpu_writel(g, fifo_intr_ctxsw_timeout_r(), ~U32(0U));
	}
}

static u32 gv11b_fifo_ctxsw_timeout_info(struct gk20a *g, u32 active_eng_id,
						u32 *info_status)
{
	u32 tsgid = NVGPU_INVALID_TSG_ID;
	u32 timeout_info;
	u32 ctx_status;

	timeout_info = nvgpu_readl(g,
			 fifo_intr_ctxsw_timeout_info_r(active_eng_id));

	/*
	 * ctxsw_state and tsgid are snapped at the point of the timeout and
	 * will not change while the corresponding INTR_CTXSW_TIMEOUT_ENGINE bit
	 * is PENDING.
	 */
	ctx_status = fifo_intr_ctxsw_timeout_info_ctxsw_state_v(timeout_info);
	if (ctx_status ==
		fifo_intr_ctxsw_timeout_info_ctxsw_state_load_v()) {

		tsgid = fifo_intr_ctxsw_timeout_info_next_tsgid_v(timeout_info);

	} else if ((ctx_status ==
			fifo_intr_ctxsw_timeout_info_ctxsw_state_switch_v()) ||
		(ctx_status ==
			fifo_intr_ctxsw_timeout_info_ctxsw_state_save_v())) {

		tsgid = fifo_intr_ctxsw_timeout_info_prev_tsgid_v(timeout_info);
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
	*info_status = fifo_intr_ctxsw_timeout_info_status_v(timeout_info);
	if (*info_status ==
		 fifo_intr_ctxsw_timeout_info_status_ack_received_v()) {

		nvgpu_log_info(g, "ctxsw timeout info: ack received");
		/* no need to recover */
		tsgid = NVGPU_INVALID_TSG_ID;

	} else if (*info_status ==
		fifo_intr_ctxsw_timeout_info_status_dropped_timeout_v()) {

		nvgpu_log_info(g, "ctxsw timeout info: dropped timeout");
		/* no need to recover */
		tsgid = NVGPU_INVALID_TSG_ID;

	} else {
		nvgpu_log_info(g, "ctxsw timeout info status: 0x%08x",
			*info_status);
	}

	return tsgid;
}

bool gv11b_fifo_handle_ctxsw_timeout(struct gk20a *g)
{
	bool recover = false;
	u32 tsgid = NVGPU_INVALID_TSG_ID;
	u32 i;
	u32 timeout_val, ctxsw_timeout_engines;
	u32 info_status;
	struct nvgpu_tsg *tsg = NULL;

	/* get ctxsw timedout engines */
	ctxsw_timeout_engines = nvgpu_readl(g, fifo_intr_ctxsw_timeout_r());
	if (ctxsw_timeout_engines == 0U) {
		nvgpu_err(g, "no eng ctxsw timeout pending");
		return false;
	}

	timeout_val = nvgpu_readl(g, fifo_eng_ctxsw_timeout_r());
	timeout_val = fifo_eng_ctxsw_timeout_period_v(timeout_val);

	nvgpu_log_info(g, "eng ctxsw timeout period = 0x%x", timeout_val);

	for (i = 0; i < g->fifo.num_engines; i++) {
		const struct nvgpu_device *dev = g->fifo.active_engines[i];

		if ((ctxsw_timeout_engines &
			fifo_intr_ctxsw_timeout_engine_pending_f(
				dev->engine_id)) != 0U) {
			u32 ms = 0;
#ifdef CONFIG_NVGPU_KERNEL_MODE_SUBMIT
			bool debug_dump = false;
			const char *const ctxsw_timeout_status_desc[] = {
				"awaiting ack",
				"eng was reset",
				"ack received",
				"dropped timeout"
			};
#endif
			tsgid = gv11b_fifo_ctxsw_timeout_info(g, dev->engine_id,
						&info_status);
			tsg = nvgpu_tsg_check_and_get_from_id(g, tsgid);
			if (tsg == NULL) {
				continue;
			}

#ifdef CONFIG_NVGPU_KERNEL_MODE_SUBMIT
			recover = g->ops.tsg.check_ctxsw_timeout(tsg,
					&debug_dump, &ms);
			if (recover) {
				const char *info_status_str = "invalid";
				if (info_status <
					ARRAY_SIZE(ctxsw_timeout_status_desc)) {
					info_status_str =
					ctxsw_timeout_status_desc[info_status];
				}

				nvgpu_err(g,
					  "ctxsw timeout error: engine_id=%u"
					  "%s=%d, info: %s ms=%u",
					  dev->engine_id, "tsg", tsgid,
					  info_status_str, ms);

				nvgpu_rc_ctxsw_timeout(g, BIT32(dev->engine_id),
					tsg, debug_dump);
				continue;
			}
#endif
			nvgpu_log_info(g,
				"fifo is waiting for ctxsw switch: "
				"for %d ms, %s=%d", ms, "tsg", tsgid);
		}
	}
	/* clear interrupt */
	nvgpu_writel(g, fifo_intr_ctxsw_timeout_r(), ctxsw_timeout_engines);

	return recover;
}
