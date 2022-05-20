/*
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
#include <nvgpu/soc.h>
#include <nvgpu/io.h>
#include <nvgpu/utils.h>
#include <nvgpu/enabled.h>
#include <nvgpu/fifo.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/device.h>

#include <nvgpu/power_features/cg.h>

#include "therm_ga10b.h"

#include <nvgpu/hw/ga10b/hw_therm_ga10b.h>

u32 ga10b_therm_grad_stepping_pdiv_duration(void)
{
	/* minimum duration between steps 15usec * UTILSCLK@102 MHz */
	return 0x5FA;
}

u32 ga10b_therm_max_fpdiv_factor(void)
{
	return therm_grad_stepping_table_slowdown_factor0_fpdiv_by31_f();
}

int ga10b_elcg_init_idle_filters(struct gk20a *g)
{
	u32 gate_ctrl, idle_filter;
	u32 i;
	const struct nvgpu_device *dev;
	struct nvgpu_fifo *f = &g->fifo;

	if (nvgpu_platform_is_simulation(g)) {
		return 0;
	}

	nvgpu_log_info(g, "init clock/power gate reg");

	for (i = 0U; i < f->num_engines; i++) {
		dev = f->active_engines[i];

		gate_ctrl = nvgpu_readl(g, therm_gate_ctrl_r(dev->engine_id));
		gate_ctrl = set_field(gate_ctrl,
			therm_gate_ctrl_eng_idle_filt_exp_m(),
			therm_gate_ctrl_eng_idle_filt_exp__prod_f());
		gate_ctrl = set_field(gate_ctrl,
			therm_gate_ctrl_eng_idle_filt_mant_m(),
			therm_gate_ctrl_eng_idle_filt_mant__prod_f());
		gate_ctrl = set_field(gate_ctrl,
			therm_gate_ctrl_eng_delay_before_m(),
			therm_gate_ctrl_eng_delay_before__prod_f());
		gate_ctrl = set_field(gate_ctrl,
				therm_gate_ctrl_eng_delay_after_m(),
				therm_gate_ctrl_eng_delay_after__prod_f());
		nvgpu_writel(g, therm_gate_ctrl_r(dev->engine_id), gate_ctrl);
	}

	idle_filter = nvgpu_readl(g, therm_fecs_idle_filter_r());
	idle_filter = set_field(idle_filter,
			therm_fecs_idle_filter_value_m(),
			therm_fecs_idle_filter_value__prod_f());
	nvgpu_writel(g, therm_fecs_idle_filter_r(), idle_filter);

	idle_filter = nvgpu_readl(g, therm_hubmmu_idle_filter_r());
	idle_filter = set_field(idle_filter,
			therm_hubmmu_idle_filter_value_m(),
			therm_hubmmu_idle_filter_value__prod_f());
	nvgpu_writel(g, therm_hubmmu_idle_filter_r(), idle_filter);

	return 0;
}
