/*
 * GA10B Fifo
 *
 * Copyright (c) 2020-2021, NVIDIA CORPORATION.  All rights reserved.
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
#include <nvgpu/io.h>
#include <nvgpu/utils.h>
#include <nvgpu/fifo.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/power_features/cg.h>

#include <nvgpu/hw/ga10b/hw_runlist_ga10b.h>
#include <nvgpu/hw/ga10b/hw_ctrl_ga10b.h>

#include "fifo_utils_ga10b.h"
#include "fifo_ga10b.h"
#include "fifo_intr_ga10b.h"

static void enable_fifo_interrupts(struct gk20a *g)
{
	g->ops.fifo.intr_top_enable(g, NVGPU_CIC_INTR_ENABLE);
	g->ops.fifo.intr_0_enable(g, true);
	g->ops.fifo.intr_1_enable(g, true);
}

int ga10b_init_fifo_reset_enable_hw(struct gk20a *g)
{
	int err;

	nvgpu_log_fn(g, " ");

	/* enable pmc pfifo */
	err = nvgpu_mc_reset_units(g, NVGPU_UNIT_FIFO);
	if (err != 0) {
		nvgpu_err(g, "Failed to reset FIFO unit");
	}

#ifdef CONFIG_NVGPU_HAL_NON_FUSA
	if (g->ops.mc.elpg_enable != NULL) {
		g->ops.mc.elpg_enable(g);
	}
#endif

	nvgpu_cg_slcg_ce2_load_enable(g);

	nvgpu_cg_slcg_fifo_load_enable(g);

	nvgpu_cg_blcg_fifo_load_enable(g);

	if (g->ops.pbdma.setup_hw != NULL) {
		g->ops.pbdma.setup_hw(g);
	}

#ifdef CONFIG_NVGPU_HAL_NON_FUSA
	if (g->ops.pbdma.pbdma_force_ce_split != NULL) {
		g->ops.pbdma.pbdma_force_ce_split(g);
	}
#endif

	nvgpu_log_fn(g, "done");

	return 0;
}

static void ga10b_fifo_config_userd_writeback_timer(struct gk20a *g)
{
	struct nvgpu_runlist *runlist = NULL;
	u32 reg_val = 0U;
	u32 i = 0U;
	u32 max_runlists = g->fifo.max_runlists;

	for (i = 0U; i < max_runlists; i++) {
		runlist = g->fifo.runlists[i];
		if (runlist == NULL) {
			continue;
		}

		reg_val = runlist_userd_writeback_timescale_0_f() |
			runlist_userd_writeback_timer_100us_f();

		nvgpu_runlist_writel(g, runlist, runlist_userd_writeback_r(),
				reg_val);
	}
}

int ga10b_init_fifo_setup_hw(struct gk20a *g)
{
	struct nvgpu_fifo *f = &g->fifo;

	nvgpu_log_fn(g, " ");

	/*
	 * Current Flow:
	 * Nvgpu Init sequence:
	 * g->ops.fifo.reset_enable_hw
	 * ....
	 * g->ops.fifo.fifo_init_support
	 *
	 * Fifo Init Sequence called from g->ops.fifo.fifo_init_support:
	 * fifo.reset_enable_hw   -> enables interrupts
	 * fifo.fifo_init_support -> fifo.setup_sw (Sets up runlist info)
	 * fifo.fifo_init_support -> fifo.init_fifo_setup_hw
	 *
	 * Runlist info is required for getting vector id and enabling
	 * interrupts at top level.
	 * Get vector ids before enabling interrupts at top level to make sure
	 * vectorids are initialized in nvgpu_mc struct before intr_top_enable
	 * is called.
	 */
	ga10b_fifo_runlist_intr_vectorid_init(g);

	f->max_subctx_count = g->ops.gr.init.get_max_subctx_count();

	g->ops.usermode.setup_hw(g);

	enable_fifo_interrupts(g);

	ga10b_fifo_config_userd_writeback_timer(g);

	return 0;
}

u32 ga10b_fifo_mmu_fault_id_to_pbdma_id(struct gk20a *g, u32 mmu_fault_id)
{
	u32 pbdma_id;

	for (pbdma_id = 0U; pbdma_id < g->ops.pbdma.get_num_of_pbdmas();
				pbdma_id = nvgpu_safe_add_u32(pbdma_id, 1U)) {
		if (g->ops.pbdma.get_mmu_fault_id(g, pbdma_id) ==
							mmu_fault_id) {
			return pbdma_id;
		}
	}

	return INVAL_ID;
}
