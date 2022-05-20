/*
 * Copyright (c) 2011-2021, NVIDIA CORPORATION.  All rights reserved.
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
#include <nvgpu/channel.h>
#include <nvgpu/rc.h>
#include <nvgpu/engines.h>

#include <hal/fifo/ctxsw_timeout_gk20a.h>

#include <nvgpu/hw/gk20a/hw_fifo_gk20a.h>

void gk20a_fifo_ctxsw_timeout_enable(struct gk20a *g, bool enable)
{
	u32 timeout;
	u32 scaled_timeout;

	if (enable) {
		timeout = g->ctxsw_timeout_period_ms * 1000U; /* in us */
		nvgpu_assert(nvgpu_ptimer_scale(g, timeout, &scaled_timeout) == 0);
		scaled_timeout |= fifo_eng_timeout_detection_enabled_f();
		nvgpu_writel(g, fifo_eng_timeout_r(), scaled_timeout);
	} else {
		timeout = nvgpu_readl(g, fifo_eng_timeout_r());
		timeout &= ~(fifo_eng_timeout_detection_enabled_f());
		nvgpu_writel(g, fifo_eng_timeout_r(), timeout);
	}
}

bool gk20a_fifo_handle_ctxsw_timeout(struct gk20a *g)
{
	u32 sched_error;
	u32 engine_id;
	u32 id = U32_MAX;
	bool is_tsg = false;
	bool recover = false;
	struct nvgpu_channel *ch = NULL;
	struct nvgpu_tsg *tsg = NULL;
	struct nvgpu_fifo *f = &g->fifo;
	u32 ms = 0;
	bool debug_dump = false;

	/* read the scheduler error register */
	sched_error = nvgpu_readl(g, fifo_intr_sched_error_r());

	engine_id = nvgpu_engine_find_busy_doing_ctxsw(g, &id, &is_tsg);
	/*
	 * Could not find the engine
	 * Possible Causes:
	 * a)
	 * On hitting engine reset, h/w drops the ctxsw_status to INVALID in
	 * fifo_engine_status register. Also while the engine is held in reset
	 * h/w passes busy/idle straight through. fifo_engine_status registers
	 * are correct in that there is no context switch outstanding
	 * as the CTXSW is aborted when reset is asserted.
	 * This is just a side effect of how gv100 and earlier versions of
	 * ctxsw_timeout behave.
	 * With gv11b and later, h/w snaps the context at the point of error
	 * so that s/w can see the tsg_id which caused the HW timeout.
	 * b)
	 * If engines are not busy and ctxsw state is valid then intr occurred
	 * in the past and if the ctxsw state has moved on to VALID from LOAD
	 * or SAVE, it means that whatever timed out eventually finished
	 * anyways. The problem with this is that s/w cannot conclude which
	 * context caused the problem as maybe more switches occurred before
	 * intr is handled.
	 */
	if (engine_id == NVGPU_INVALID_ENG_ID) {
		nvgpu_info(g, "fifo ctxsw timeout: 0x%08x, failed to find engine "
				"that is busy doing ctxsw. "
				"May be ctxsw already happened", sched_error);
		return false;
	}

	if (!nvgpu_engine_check_valid_id(g, engine_id)) {
		nvgpu_err(g, "fifo ctxsw timeout: 0x%08x, engine_id %u not valid",
			sched_error, engine_id);
		return false;
	}

	if (id > f->num_channels) {
		nvgpu_err(g, "fifo ctxsw timeout error: id is invalid %u", id);
		return false;
	}

	if (is_tsg) {
		tsg = nvgpu_tsg_check_and_get_from_id(g, id);
	} else {
		ch = nvgpu_channel_from_id(g, id);
		if (ch != NULL) {
			tsg = nvgpu_tsg_from_ch(ch);
			nvgpu_channel_put(ch);
		}
	}
#ifdef CONFIG_NVGPU_KERNEL_MODE_SUBMIT
	if (tsg != NULL) {
		recover = g->ops.tsg.check_ctxsw_timeout(tsg, &debug_dump, &ms);
		if (recover) {
			nvgpu_err(g,
				"fifo ctxsw timeout error: "
				"engine=%u, %s=%d, ms=%u",
				engine_id, is_tsg ? "tsg" : "ch", id, ms);

			nvgpu_rc_ctxsw_timeout(g, BIT32(engine_id), tsg, debug_dump);
			return recover;
		}
	}
#endif

	nvgpu_log_info(g,
		"fifo is waiting for ctxsw switch for %d ms, "
			"%s=%d", ms, is_tsg ? "tsg" : "ch", id);

	return recover;
}
