/*
 * GK20A Graphics FIFO (gr host)
 *
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
#include <nvgpu/log.h>
#include <nvgpu/soc.h>
#include <nvgpu/io.h>
#include <nvgpu/fifo.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/power_features/cg.h>
#include <nvgpu/cic_mon.h>
#include <nvgpu/mc.h>

#include "hal/fifo/fifo_gk20a.h"

#include <nvgpu/hw/gk20a/hw_fifo_gk20a.h>

static void enable_fifo_interrupts(struct gk20a *g)
{
	nvgpu_cic_mon_intr_stall_unit_config(g, NVGPU_CIC_INTR_UNIT_FIFO, NVGPU_CIC_INTR_ENABLE);
	nvgpu_cic_mon_intr_nonstall_unit_config(g, NVGPU_CIC_INTR_UNIT_FIFO,
					   NVGPU_CIC_INTR_ENABLE);

	g->ops.fifo.intr_0_enable(g, true);
	g->ops.fifo.intr_1_enable(g, true);
}

int gk20a_init_fifo_reset_enable_hw(struct gk20a *g)
{
	u32 timeout;
	int err;

	nvgpu_log_fn(g, " ");

	/* enable pmc pfifo */
	err = nvgpu_mc_reset_units(g, NVGPU_UNIT_FIFO);
	if (err != 0) {
		nvgpu_err(g, "Failed to reset FIFO unit");
	}

	nvgpu_cg_slcg_fifo_load_enable(g);

	nvgpu_cg_blcg_fifo_load_enable(g);

	timeout = nvgpu_readl(g, fifo_fb_timeout_r());
	timeout = set_field(timeout, fifo_fb_timeout_period_m(),
			fifo_fb_timeout_period_max_f());
	nvgpu_log_info(g, "fifo_fb_timeout reg val = 0x%08x", timeout);
	nvgpu_writel(g, fifo_fb_timeout_r(), timeout);

	g->ops.pbdma.setup_hw(g);

	enable_fifo_interrupts(g);

	nvgpu_log_fn(g, "done");

	return 0;
}

int gk20a_init_fifo_setup_hw(struct gk20a *g)
{
#ifdef CONFIG_NVGPU_USERD
	struct nvgpu_fifo *f = &g->fifo;
	u64 shifted_addr;

	nvgpu_log_fn(g, " ");

	/* set the base for the userd region now */
	shifted_addr = f->userd_gpu_va >> 12;
	if ((shifted_addr >> 32) != 0U) {
		nvgpu_err(g, "GPU VA > 32 bits %016llx", f->userd_gpu_va);
		return -EFAULT;
	}
	nvgpu_writel(g, fifo_bar1_base_r(),
			fifo_bar1_base_ptr_f(u64_lo32(shifted_addr)) |
			fifo_bar1_base_valid_true_f());
#endif
	nvgpu_log_fn(g, "done");

	return 0;
}

void gk20a_fifo_bar1_snooping_disable(struct gk20a *g)
{
	nvgpu_writel(g, fifo_bar1_base_r(),
		fifo_bar1_base_valid_false_f());
}
