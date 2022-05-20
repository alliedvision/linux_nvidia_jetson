/*
 * GM20B THERMAL
 *
 * Copyright (c) 2015-2019, NVIDIA CORPORATION.  All rights reserved.
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

#include <nvgpu/io.h>
#include <nvgpu/utils.h>
#include <nvgpu/enabled.h>
#include <nvgpu/fifo.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/device.h>

#include <nvgpu/power_features/cg.h>

#include "therm_gm20b.h"

#include <nvgpu/hw/gm20b/hw_therm_gm20b.h>

int gm20b_init_therm_setup_hw(struct gk20a *g)
{
	u32 v;

	nvgpu_log_fn(g, " ");

	/* program NV_THERM registers */
	nvgpu_writel(g, therm_use_a_r(), therm_use_a_ext_therm_0_enable_f() |
			therm_use_a_ext_therm_1_enable_f()  |
			therm_use_a_ext_therm_2_enable_f());
	nvgpu_writel(g, therm_evt_ext_therm_0_r(),
			therm_evt_ext_therm_0_slow_factor_f(0x2));
	nvgpu_writel(g, therm_evt_ext_therm_1_r(),
			therm_evt_ext_therm_1_slow_factor_f(0x6));
	nvgpu_writel(g, therm_evt_ext_therm_2_r(),
			therm_evt_ext_therm_2_slow_factor_f(0xe));

	nvgpu_writel(g, therm_grad_stepping_table_r(0),
		therm_grad_stepping_table_slowdown_factor0_f(therm_grad_stepping_table_slowdown_factor0_fpdiv_by1p5_f()) |
		therm_grad_stepping_table_slowdown_factor1_f(therm_grad_stepping_table_slowdown_factor0_fpdiv_by2_f()) |
		therm_grad_stepping_table_slowdown_factor2_f(therm_grad_stepping_table_slowdown_factor0_fpdiv_by4_f()) |
		therm_grad_stepping_table_slowdown_factor3_f(therm_grad_stepping_table_slowdown_factor0_fpdiv_by8_f()) |
		therm_grad_stepping_table_slowdown_factor4_f(therm_grad_stepping_table_slowdown_factor0_fpdiv_by8_f()));
	nvgpu_writel(g, therm_grad_stepping_table_r(1),
		therm_grad_stepping_table_slowdown_factor0_f(therm_grad_stepping_table_slowdown_factor0_fpdiv_by8_f()) |
		therm_grad_stepping_table_slowdown_factor1_f(therm_grad_stepping_table_slowdown_factor0_fpdiv_by8_f()) |
		therm_grad_stepping_table_slowdown_factor2_f(therm_grad_stepping_table_slowdown_factor0_fpdiv_by8_f()) |
		therm_grad_stepping_table_slowdown_factor3_f(therm_grad_stepping_table_slowdown_factor0_fpdiv_by8_f()) |
		therm_grad_stepping_table_slowdown_factor4_f(therm_grad_stepping_table_slowdown_factor0_fpdiv_by8_f()));

	v = nvgpu_readl(g, therm_clk_timing_r(0));
	v |= therm_clk_timing_grad_slowdown_enabled_f();
	nvgpu_writel(g, therm_clk_timing_r(0), v);

	v = nvgpu_readl(g, therm_config2_r());
	v |= therm_config2_grad_enable_f(1);
	v |= therm_config2_slowdown_factor_extended_f(1);
	nvgpu_writel(g, therm_config2_r(), v);

	nvgpu_writel(g, therm_grad_stepping1_r(),
			therm_grad_stepping1_pdiv_duration_f(32));

	v = nvgpu_readl(g, therm_grad_stepping0_r());
	v |= therm_grad_stepping0_feature_enable_f();
	nvgpu_writel(g, therm_grad_stepping0_r(), v);

	return 0;
}

int gm20b_elcg_init_idle_filters(struct gk20a *g)
{
	u32 gate_ctrl, idle_filter, i;
	const struct nvgpu_device *dev;
	struct nvgpu_fifo *f = &g->fifo;

	nvgpu_log_fn(g, " ");

	for (i = 0; i < f->num_engines; i++) {
		dev = f->active_engines[i];
		gate_ctrl = nvgpu_readl(g, therm_gate_ctrl_r(dev->engine_id));

#ifdef CONFIG_NVGPU_SIM
		if (nvgpu_is_enabled(g, NVGPU_IS_FMODEL)) {
			gate_ctrl = set_field(gate_ctrl,
				therm_gate_ctrl_eng_delay_after_m(),
				therm_gate_ctrl_eng_delay_after_f(4));
		}
#endif

		/* 2 * (1 << 9) = 1024 clks */
		gate_ctrl = set_field(gate_ctrl,
			therm_gate_ctrl_eng_idle_filt_exp_m(),
			therm_gate_ctrl_eng_idle_filt_exp_f(9));
		gate_ctrl = set_field(gate_ctrl,
			therm_gate_ctrl_eng_idle_filt_mant_m(),
			therm_gate_ctrl_eng_idle_filt_mant_f(2));
		nvgpu_writel(g, therm_gate_ctrl_r(dev->engine_id), gate_ctrl);
	}

	/* default fecs_idle_filter to 0 */
	idle_filter = nvgpu_readl(g, therm_fecs_idle_filter_r());
	idle_filter &= ~therm_fecs_idle_filter_value_m();
	nvgpu_writel(g, therm_fecs_idle_filter_r(), idle_filter);
	/* default hubmmu_idle_filter to 0 */
	idle_filter = nvgpu_readl(g, therm_hubmmu_idle_filter_r());
	idle_filter &= ~therm_hubmmu_idle_filter_value_m();
	nvgpu_writel(g, therm_hubmmu_idle_filter_r(), idle_filter);

	nvgpu_log_fn(g, "done");
	return 0;
}

void gm20b_therm_init_elcg_mode(struct gk20a *g, u32 mode, u32 engine)
{
	u32 gate_ctrl;

	gate_ctrl = nvgpu_readl(g, therm_gate_ctrl_r(engine));

	if (!nvgpu_is_enabled(g, NVGPU_GPU_CAN_ELCG)) {
		return;
	}

	switch (mode) {
	case ELCG_RUN:
		gate_ctrl = set_field(gate_ctrl,
				therm_gate_ctrl_eng_clk_m(),
				therm_gate_ctrl_eng_clk_run_f());
		gate_ctrl = set_field(gate_ctrl,
				therm_gate_ctrl_eng_pwr_m(),
				/* set elpg to auto to meet hw expectation */
				therm_gate_ctrl_eng_pwr_auto_f());
		break;
	case ELCG_STOP:
		gate_ctrl = set_field(gate_ctrl,
				therm_gate_ctrl_eng_clk_m(),
				therm_gate_ctrl_eng_clk_stop_f());
		break;
	case ELCG_AUTO:
		gate_ctrl = set_field(gate_ctrl,
				therm_gate_ctrl_eng_clk_m(),
				therm_gate_ctrl_eng_clk_auto_f());
		break;
	default:
		nvgpu_err(g,
			"invalid elcg mode %d", mode);
		break;
	}

	nvgpu_writel(g, therm_gate_ctrl_r(engine), gate_ctrl);
}

void gm20b_therm_throttle_enable(struct gk20a *g, u32 val)
{
	nvgpu_writel(g, therm_use_a_r(), val);
}

u32 gm20b_therm_throttle_disable(struct gk20a *g)
{
	u32 val = nvgpu_readl(g, therm_use_a_r());

	nvgpu_writel(g, therm_use_a_r(), 0);
	return val;
}

void gm20b_therm_idle_slowdown_enable(struct gk20a *g, u32 val)
{
	nvgpu_writel(g, therm_clk_slowdown_r(0), val);
}

u32 gm20b_therm_idle_slowdown_disable(struct gk20a *g)
{
	u32 saved_val = nvgpu_readl(g, therm_clk_slowdown_r(0));
	u32 val = set_field(saved_val, therm_clk_slowdown_idle_factor_m(),
			therm_clk_slowdown_idle_factor_disabled_f());
	nvgpu_writel_check(g, therm_clk_slowdown_r(0), val);

	return saved_val;
}

void gm20b_therm_init_blcg_mode(struct gk20a *g, u32 mode, u32 engine)
{
	u32 gate_ctrl;
	bool error_status = false;

	if (!nvgpu_is_enabled(g, NVGPU_GPU_CAN_BLCG)) {
		return;
	}

	gate_ctrl = nvgpu_readl(g, therm_gate_ctrl_r(engine));

	switch (mode) {
	case BLCG_RUN:
		gate_ctrl = set_field(gate_ctrl,
				therm_gate_ctrl_blk_clk_m(),
				therm_gate_ctrl_blk_clk_run_f());
		break;
	case BLCG_AUTO:
		gate_ctrl = set_field(gate_ctrl,
				therm_gate_ctrl_blk_clk_m(),
				therm_gate_ctrl_blk_clk_auto_f());
		break;
	default:
		nvgpu_err(g,
			"invalid blcg mode %d", mode);
		error_status = true;
		break;
	}

	if (error_status == true) {
		return;
	}

	nvgpu_writel(g, therm_gate_ctrl_r(engine), gate_ctrl);
}
