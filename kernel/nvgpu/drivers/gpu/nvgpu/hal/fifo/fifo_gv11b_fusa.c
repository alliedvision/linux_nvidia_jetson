/*
 * GV11B fifo
 *
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

#include <nvgpu/log.h>
#include <nvgpu/soc.h>
#include <nvgpu/io.h>
#include <nvgpu/fifo.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/power_features/cg.h>
#include <nvgpu/static_analysis.h>
#include <nvgpu/cic_mon.h>
#include <nvgpu/mc.h>

#include <nvgpu/hw/gv11b/hw_fifo_gv11b.h>

#include "fifo_gv11b.h"

static void enable_fifo_interrupts(struct gk20a *g)
{
	nvgpu_cic_mon_intr_stall_unit_config(g, NVGPU_CIC_INTR_UNIT_FIFO,
					NVGPU_CIC_INTR_ENABLE);
#ifdef CONFIG_NVGPU_NONSTALL_INTR
	nvgpu_cic_mon_intr_nonstall_unit_config(g, NVGPU_CIC_INTR_UNIT_FIFO,
					   NVGPU_CIC_INTR_ENABLE);
#endif

	g->ops.fifo.intr_0_enable(g, true);
	g->ops.fifo.intr_1_enable(g, true);
}

int gv11b_init_fifo_reset_enable_hw(struct gk20a *g)
{
	u32 timeout;
	int err;

	nvgpu_log_fn(g, " ");

	/* enable pmc pfifo */
	err = nvgpu_mc_reset_units(g, NVGPU_UNIT_FIFO);
	if (err != 0) {
		nvgpu_err(g, "Failed to reset FIFO unit");
	}

	nvgpu_cg_slcg_ce2_load_enable(g);

	nvgpu_cg_slcg_fifo_load_enable(g);

	nvgpu_cg_blcg_fifo_load_enable(g);

	timeout = nvgpu_readl(g, fifo_fb_timeout_r());
	nvgpu_log_info(g, "fifo_fb_timeout reg val = 0x%08x", timeout);
	if (!nvgpu_platform_is_silicon(g)) {
		timeout = set_field(timeout, fifo_fb_timeout_period_m(),
					fifo_fb_timeout_period_max_f());
		timeout = set_field(timeout, fifo_fb_timeout_detection_m(),
					fifo_fb_timeout_detection_disabled_f());
		nvgpu_log_info(g, "new fifo_fb_timeout reg val = 0x%08x",
					timeout);
		nvgpu_writel(g, fifo_fb_timeout_r(), timeout);
	}

	g->ops.pbdma.setup_hw(g);

	enable_fifo_interrupts(g);

	nvgpu_log_fn(g, "done");

	return 0;
}

int gv11b_init_fifo_setup_hw(struct gk20a *g)
{
	struct nvgpu_fifo *f = &g->fifo;

	f->max_subctx_count = g->ops.gr.init.get_max_subctx_count();

	/* configure userd writeback timer */
	nvgpu_writel(g, fifo_userd_writeback_r(),
		fifo_userd_writeback_timer_f(
			fifo_userd_writeback_timer_100us_v()));

	return 0;
}

u32 gv11b_fifo_mmu_fault_id_to_pbdma_id(struct gk20a *g, u32 mmu_fault_id)
{
	u32 num_pbdma, reg_val, fault_id_pbdma0;

	reg_val = nvgpu_readl(g, fifo_cfg0_r());
	num_pbdma = fifo_cfg0_num_pbdma_v(reg_val);
	fault_id_pbdma0 = fifo_cfg0_pbdma_fault_id_v(reg_val);

	if ((mmu_fault_id >= fault_id_pbdma0) &&
		(mmu_fault_id <= nvgpu_safe_sub_u32(
					nvgpu_safe_add_u32(fault_id_pbdma0,
							num_pbdma), 1U))) {
		return mmu_fault_id - fault_id_pbdma0;
	}

	return INVAL_ID;
}
