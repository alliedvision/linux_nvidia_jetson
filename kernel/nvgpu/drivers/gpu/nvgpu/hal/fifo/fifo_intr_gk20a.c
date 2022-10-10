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

#include <nvgpu/gk20a.h>
#include <nvgpu/log.h>
#include <nvgpu/io.h>
#include <nvgpu/soc.h>
#include <nvgpu/ptimer.h>
#include <nvgpu/channel.h>
#include <nvgpu/tsg.h>
#include <nvgpu/rc.h>
#include <nvgpu/nvgpu_err.h>
#include <nvgpu/error_notifier.h>
#include <nvgpu/pbdma_status.h>
#include <nvgpu/engines.h>

#include <hal/fifo/fifo_intr_gk20a.h>
#include <hal/fifo/mmu_fault_gk20a.h>

#include <nvgpu/hw/gk20a/hw_fifo_gk20a.h>

static u32 gk20a_fifo_intr_0_error_mask(struct gk20a *g)
{
	u32 intr_0_error_mask =
		fifo_intr_0_bind_error_pending_f() |
		fifo_intr_0_sched_error_pending_f() |
		fifo_intr_0_chsw_error_pending_f() |
		fifo_intr_0_fb_flush_timeout_pending_f() |
		fifo_intr_0_dropped_mmu_fault_pending_f() |
		fifo_intr_0_mmu_fault_pending_f() |
		fifo_intr_0_lb_error_pending_f() |
		fifo_intr_0_pio_error_pending_f();

	(void)g;
	return intr_0_error_mask;
}

static u32 gk20a_fifo_intr_0_en_mask(struct gk20a *g)
{
	u32 intr_0_en_mask;

	intr_0_en_mask = gk20a_fifo_intr_0_error_mask(g);

	intr_0_en_mask |= fifo_intr_0_runlist_event_pending_f() |
				 fifo_intr_0_pbdma_intr_pending_f();

	return intr_0_en_mask;
}

void gk20a_fifo_intr_0_enable(struct gk20a *g, bool enable)
{
	u32 mask;

	if (!enable) {
		nvgpu_writel(g, fifo_intr_en_0_r(), 0U);
		g->ops.fifo.ctxsw_timeout_enable(g, false);
		g->ops.pbdma.intr_enable(g, false);
		return;
	}

	/* Enable interrupts */

	g->ops.fifo.ctxsw_timeout_enable(g, true);
	g->ops.pbdma.intr_enable(g, true);

	/* reset runlist interrupts */
	nvgpu_writel(g, fifo_intr_runlist_r(), ~U32(0U));

	/* clear and enable pfifo interrupt */
	nvgpu_writel(g, fifo_intr_0_r(), U32_MAX);
	mask = gk20a_fifo_intr_0_en_mask(g);
	nvgpu_log_info(g, "fifo_intr_en_0 0x%08x", mask);
	nvgpu_writel(g, fifo_intr_en_0_r(), mask);
}

bool gk20a_fifo_handle_sched_error(struct gk20a *g)
{
	u32 sched_error;
	u32 engine_id;
	u32 id = U32_MAX;
	bool is_tsg = false;
	bool ret = false;

	/* read the scheduler error register */
	sched_error = nvgpu_readl(g, fifo_intr_sched_error_r());

	engine_id = nvgpu_engine_find_busy_doing_ctxsw(g, &id, &is_tsg);

	if (fifo_intr_sched_error_code_f(sched_error) !=
			fifo_intr_sched_error_code_ctxsw_timeout_v()) {
		nvgpu_err(g,
			"fifo sched error : 0x%08x, engine=%u, %s=%d",
			sched_error, engine_id, is_tsg ? "tsg" : "ch", id);
	} else {
		ret = g->ops.fifo.handle_ctxsw_timeout(g);
	}
	return ret;
}

static u32 gk20a_fifo_intr_handle_errors(struct gk20a *g, u32 fifo_intr)
{
	u32 handled = 0U;

	nvgpu_log_fn(g, "fifo_intr=0x%08x", fifo_intr);

	if ((fifo_intr & fifo_intr_0_pio_error_pending_f()) != 0U) {
		/* pio mode is unused.  this shouldn't happen, ever. */
		/* should we clear it or just leave it pending? */
		nvgpu_err(g, "fifo pio error!");
		BUG();
	}

	if ((fifo_intr & fifo_intr_0_bind_error_pending_f()) != 0U) {
		u32 bind_error = nvgpu_readl(g, fifo_intr_bind_error_r());

		nvgpu_err(g, "fifo bind error: 0x%08x", bind_error);
		handled |= fifo_intr_0_bind_error_pending_f();
	}

	if ((fifo_intr & fifo_intr_0_chsw_error_pending_f()) != 0U) {
		gk20a_fifo_intr_handle_chsw_error(g);
		handled |= fifo_intr_0_chsw_error_pending_f();
	}

	if ((fifo_intr & fifo_intr_0_fb_flush_timeout_pending_f()) != 0U) {
		nvgpu_report_err_to_sdl(g, NVGPU_ERR_MODULE_HOST,
				GPU_HOST_PFIFO_FB_FLUSH_TIMEOUT_ERROR);
		nvgpu_err(g, "fifo fb flush timeout error");
		handled |= fifo_intr_0_fb_flush_timeout_pending_f();
	}

	if ((fifo_intr & fifo_intr_0_lb_error_pending_f()) != 0U) {
		nvgpu_err(g, "fifo lb error");
		handled |= fifo_intr_0_lb_error_pending_f();
	}

	return handled;
}

void gk20a_fifo_intr_0_isr(struct gk20a *g)
{
	u32 clear_intr = 0U;
	u32 fifo_intr = nvgpu_readl(g, fifo_intr_0_r());

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

	if (unlikely((fifo_intr & gk20a_fifo_intr_0_error_mask(g)) !=
							0U)) {
		clear_intr |= gk20a_fifo_intr_handle_errors(g,
				fifo_intr);
	}

	if ((fifo_intr & fifo_intr_0_runlist_event_pending_f()) != 0U) {
		gk20a_fifo_intr_handle_runlist_event(g);
		clear_intr |= fifo_intr_0_runlist_event_pending_f();
	}

	if ((fifo_intr & fifo_intr_0_pbdma_intr_pending_f()) != 0U) {
		clear_intr |= gk20a_fifo_pbdma_isr(g);
	}

	if ((fifo_intr & fifo_intr_0_mmu_fault_pending_f()) != 0U) {
		gk20a_fifo_handle_mmu_fault(g, 0, INVAL_ID, false);
		clear_intr |= fifo_intr_0_mmu_fault_pending_f();
	}

	if ((fifo_intr & fifo_intr_0_sched_error_pending_f()) != 0U) {
		(void) g->ops.fifo.handle_sched_error(g);
		clear_intr |= fifo_intr_0_sched_error_pending_f();
	}

	if ((fifo_intr & fifo_intr_0_dropped_mmu_fault_pending_f()) != 0U) {
		gk20a_fifo_handle_dropped_mmu_fault(g);
		clear_intr |= fifo_intr_0_dropped_mmu_fault_pending_f();
	}

	nvgpu_mutex_release(&g->fifo.intr.isr.mutex);

	nvgpu_writel(g, fifo_intr_0_r(), clear_intr);
}

bool gk20a_fifo_is_mmu_fault_pending(struct gk20a *g)
{
	if ((nvgpu_readl(g, fifo_intr_0_r()) &
	     fifo_intr_0_mmu_fault_pending_f()) != 0U) {
		return true;
	} else {
		return false;
	}
}

void gk20a_fifo_intr_set_recover_mask(struct gk20a *g)
{
	u32 val;

	val = nvgpu_readl(g, fifo_intr_en_0_r());
	val &= ~(fifo_intr_en_0_sched_error_m() |
		fifo_intr_en_0_mmu_fault_m());
	nvgpu_writel(g, fifo_intr_en_0_r(), val);
	nvgpu_writel(g, fifo_intr_0_r(), fifo_intr_0_sched_error_reset_f());
}

void gk20a_fifo_intr_unset_recover_mask(struct gk20a *g)
{
	u32 val;

	val = nvgpu_readl(g, fifo_intr_en_0_r());
	val |= fifo_intr_en_0_mmu_fault_f(1) | fifo_intr_en_0_sched_error_f(1);
	nvgpu_writel(g, fifo_intr_en_0_r(), val);

}
