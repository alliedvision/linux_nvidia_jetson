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
#include <nvgpu/mc.h>
#include <nvgpu/log.h>
#include <nvgpu/io.h>
#include <nvgpu/soc.h>
#include <nvgpu/ptimer.h>
#include <nvgpu/channel.h>
#include <nvgpu/tsg.h>
#include <nvgpu/rc.h>
#include <nvgpu/nvgpu_err.h>
#include <nvgpu/cic_mon.h>
#include <nvgpu/engines.h>

#include <hal/fifo/fifo_intr_gk20a.h>
#include <hal/fifo/mmu_fault_gk20a.h>

#include <nvgpu/hw/gk20a/hw_fifo_gk20a.h>

void gk20a_fifo_intr_1_enable(struct gk20a *g, bool enable)
{
	if (enable) {
		nvgpu_writel(g, fifo_intr_en_1_r(),
			fifo_intr_0_channel_intr_pending_f());
		nvgpu_log_info(g, "fifo_intr_en_1 = 0x%08x",
			nvgpu_readl(g, fifo_intr_en_1_r()));
	} else {
		nvgpu_writel(g, fifo_intr_en_1_r(), 0U);
	}
}

u32 gk20a_fifo_intr_1_isr(struct gk20a *g)
{
	u32 fifo_intr = nvgpu_readl(g, fifo_intr_0_r());

	nvgpu_log(g, gpu_dbg_intr, "fifo nonstall isr 0x%08x", fifo_intr);

	if ((fifo_intr & fifo_intr_0_channel_intr_pending_f()) != 0U) {
		nvgpu_writel(g, fifo_intr_0_r(),
			fifo_intr_0_channel_intr_pending_f());
		return NVGPU_CIC_NONSTALL_OPS_WAKEUP_SEMAPHORE;
	}

	return 0U;
}

void gk20a_fifo_intr_handle_chsw_error(struct gk20a *g)
{
	u32 intr;

	intr = nvgpu_readl(g, fifo_intr_chsw_error_r());
	nvgpu_report_err_to_sdl(g, NVGPU_ERR_MODULE_HOST,
			GPU_HOST_PFIFO_CHSW_ERROR);
	nvgpu_err(g, "chsw: %08x", intr);
	g->ops.gr.falcon.dump_stats(g);
	nvgpu_writel(g, fifo_intr_chsw_error_r(), intr);
}

void gk20a_fifo_intr_handle_runlist_event(struct gk20a *g)
{
	u32 runlist_event = nvgpu_readl(g, fifo_intr_runlist_r());

	nvgpu_log(g, gpu_dbg_intr, "runlist event %08x",
		  runlist_event);

	nvgpu_writel(g, fifo_intr_runlist_r(), runlist_event);
}

u32 gk20a_fifo_pbdma_isr(struct gk20a *g)
{
	u32 pbdma_id;
	u32 num_pbdma = nvgpu_get_litter_value(g, GPU_LIT_HOST_NUM_PBDMA);
	u32 pbdma_pending_bitmask = nvgpu_readl(g, fifo_intr_pbdma_id_r());

	for (pbdma_id = 0; pbdma_id < num_pbdma; pbdma_id++) {
		if (fifo_intr_pbdma_id_status_v(pbdma_pending_bitmask, pbdma_id) != 0U) {
			nvgpu_log(g, gpu_dbg_intr, "pbdma id %d intr pending",
				pbdma_id);
			g->ops.pbdma.handle_intr(g, pbdma_id, true);
		}
	}
	return fifo_intr_0_pbdma_intr_pending_f();
}
