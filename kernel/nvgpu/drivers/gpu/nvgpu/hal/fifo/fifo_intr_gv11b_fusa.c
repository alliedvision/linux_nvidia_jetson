/*
 * Copyright (c) 2015-2022, NVIDIA CORPORATION.  All rights reserved.
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
#include <nvgpu/tsg.h>
#include <nvgpu/nvgpu_err.h>
#include <nvgpu/error_notifier.h>
#include <nvgpu/rc.h>

#include <hal/fifo/fifo_intr_gk20a.h>
#include <hal/fifo/fifo_intr_gv11b.h>

#include <nvgpu/hw/gv11b/hw_fifo_gv11b.h>

static u32 fifo_intr_0_err_mask(void)
{
	u32 mask = (fifo_intr_0_bind_error_pending_f() |
				fifo_intr_0_sched_error_pending_f() |
				fifo_intr_0_chsw_error_pending_f() |
				fifo_intr_0_memop_timeout_pending_f() |
				fifo_intr_0_lb_error_pending_f());
	return mask;
}

static const char *const gv11b_sched_error_str[] = {
	"xxx-0",
	"xxx-1",
	"xxx-2",
	"xxx-3",
	"xxx-4",
	"engine_reset",
	"rl_ack_timeout",
	"rl_ack_extra",
	"rl_rdat_timeout",
	"rl_rdat_extra",
	"eng_ctxsw_timeout",
	"xxx-b",
	"rl_req_timeout",
	"new_runlist",
	"code_config_while_busy",
	"xxx-f",
	"xxx-0x10",
	"xxx-0x11",
	"xxx-0x12",
	"xxx-0x13",
	"xxx-0x14",
	"xxx-0x15",
	"xxx-0x16",
	"xxx-0x17",
	"xxx-0x18",
	"xxx-0x19",
	"xxx-0x1a",
	"xxx-0x1b",
	"xxx-0x1c",
	"xxx-0x1d",
	"xxx-0x1e",
	"xxx-0x1f",
	"bad_tsg",
};

static u32 gv11b_fifo_intr_0_en_mask(struct gk20a *g)
{
	u32 intr_0_en_mask = fifo_intr_0_err_mask();

	(void)g;
	intr_0_en_mask |= fifo_intr_0_pbdma_intr_pending_f() |
				 fifo_intr_0_ctxsw_timeout_pending_f();

	return intr_0_en_mask;
}

void gv11b_fifo_intr_0_enable(struct gk20a *g, bool enable)
{
	u32 mask;

	if (!enable) {
		nvgpu_writel(g, fifo_intr_en_0_r(), 0);
		g->ops.fifo.ctxsw_timeout_enable(g, false);
		g->ops.pbdma.intr_enable(g, false);
		return;
	}

	/* Enable interrupts */

	g->ops.fifo.ctxsw_timeout_enable(g, true);
	g->ops.pbdma.intr_enable(g, true);

	/* clear runlist interrupts */
	nvgpu_writel(g, fifo_intr_runlist_r(), ~U32(0U));

	/* clear and enable pfifo interrupt */
	nvgpu_writel(g, fifo_intr_0_r(), U32_MAX);
	mask = gv11b_fifo_intr_0_en_mask(g);
	nvgpu_log_info(g, "fifo_intr_en_0 0x%08x", mask);
	nvgpu_writel(g, fifo_intr_en_0_r(), mask);
}

bool gv11b_fifo_handle_sched_error(struct gk20a *g)
{
	u32 sched_error;

	sched_error = nvgpu_readl(g, fifo_intr_sched_error_r());

	if (sched_error < ARRAY_SIZE(gv11b_sched_error_str)) {
		nvgpu_err(g, "fifo sched error :%s",
			gv11b_sched_error_str[sched_error]);
	} else {
		nvgpu_err(g, "fifo sched error code not supported");
	}

	if (sched_error == SCHED_ERROR_CODE_BAD_TSG) {
		/* id is unknown, preempt all runlists and do recovery */
		nvgpu_rc_sched_error_bad_tsg(g);
	}

	return false;
}

static u32 gv11b_fifo_intr_handle_errors(struct gk20a *g, u32 fifo_intr)
{
	u32 handled = 0U;

	nvgpu_log_fn(g, "fifo_intr=0x%08x", fifo_intr);

	if ((fifo_intr & fifo_intr_0_bind_error_pending_f()) != 0U) {
		u32 bind_error = nvgpu_readl(g, fifo_intr_bind_error_r());
		nvgpu_report_err_to_sdl(g, NVGPU_ERR_MODULE_HOST,
				GPU_HOST_PFIFO_BIND_ERROR);
		nvgpu_err(g, "fifo bind error: 0x%08x", bind_error);
		handled |= fifo_intr_0_bind_error_pending_f();
	}

	if ((fifo_intr & fifo_intr_0_chsw_error_pending_f()) != 0U) {
		gk20a_fifo_intr_handle_chsw_error(g);
		handled |= fifo_intr_0_chsw_error_pending_f();
	}

	if ((fifo_intr & fifo_intr_0_memop_timeout_pending_f()) != 0U) {
		nvgpu_report_err_to_sdl(g, NVGPU_ERR_MODULE_HOST,
				GPU_HOST_PFIFO_MEMOP_TIMEOUT_ERROR);
		nvgpu_err(g, "fifo memop timeout error");
		handled |= fifo_intr_0_memop_timeout_pending_f();
	}

	if ((fifo_intr & fifo_intr_0_lb_error_pending_f()) != 0U) {
		nvgpu_report_err_to_sdl(g, NVGPU_ERR_MODULE_HOST,
				GPU_HOST_PFIFO_LB_ERROR);
		nvgpu_err(g, "fifo lb error");
		handled |= fifo_intr_0_lb_error_pending_f();
	}

	return handled;
}

void gv11b_fifo_intr_0_isr(struct gk20a *g)
{
	u32 clear_intr = 0U;
	u32 fifo_intr = nvgpu_readl(g, fifo_intr_0_r());
	u32 intr_0_en_mask = 0U;

	/* TODO: sw_ready is needed only for recovery part */
	if (!g->fifo.sw_ready) {
		nvgpu_err(g, "unhandled fifo intr: 0x%08x", fifo_intr);
		nvgpu_writel(g, fifo_intr_0_r(), fifo_intr);
		return;
	}
	/* note we're not actually in an "isr", but rather
	 * in a threaded interrupt context... */
	nvgpu_mutex_acquire(&g->fifo.intr.isr.mutex);

	nvgpu_log(g, gpu_dbg_intr, "fifo isr %08x", fifo_intr);

	intr_0_en_mask = fifo_intr_0_err_mask();

	if (unlikely((fifo_intr & intr_0_en_mask) != 0U)) {
		clear_intr |= gv11b_fifo_intr_handle_errors(g,
				fifo_intr);
	}

	if ((fifo_intr & fifo_intr_0_runlist_event_pending_f()) != 0U) {
		gk20a_fifo_intr_handle_runlist_event(g);
		clear_intr |= fifo_intr_0_runlist_event_pending_f();
	}

	if ((fifo_intr & fifo_intr_0_pbdma_intr_pending_f()) != 0U) {
		clear_intr |= gk20a_fifo_pbdma_isr(g);
	}

	if ((fifo_intr & fifo_intr_0_sched_error_pending_f()) != 0U) {
		(void)g->ops.fifo.handle_sched_error(g);
		clear_intr |= fifo_intr_0_sched_error_pending_f();
	}

	if ((fifo_intr & fifo_intr_0_ctxsw_timeout_pending_f()) != 0U) {
		(void)g->ops.fifo.handle_ctxsw_timeout(g);
		clear_intr |= fifo_intr_0_ctxsw_timeout_pending_f();
	}

	nvgpu_mutex_release(&g->fifo.intr.isr.mutex);

	nvgpu_writel(g, fifo_intr_0_r(), clear_intr);
}

void gv11b_fifo_intr_set_recover_mask(struct gk20a *g)
{
	u32 val;

	/*
	 * ctxsw timeout error prevents recovery, and ctxsw error will retrigger
	 * every 100ms. Disable ctxsw timeout error to allow recovery.
	 */
	val = nvgpu_readl(g, fifo_intr_en_0_r());
	val &= ~fifo_intr_0_ctxsw_timeout_pending_f();
	nvgpu_writel(g, fifo_intr_en_0_r(), val);
	nvgpu_writel(g, fifo_intr_ctxsw_timeout_r(),
			nvgpu_readl(g, fifo_intr_ctxsw_timeout_r()));

}

void gv11b_fifo_intr_unset_recover_mask(struct gk20a *g)
{
	u32 val;

	/* enable ctxsw timeout interrupt */
	val = nvgpu_readl(g, fifo_intr_en_0_r());
	val |= fifo_intr_0_ctxsw_timeout_pending_f();
	nvgpu_writel(g, fifo_intr_en_0_r(), val);
}
