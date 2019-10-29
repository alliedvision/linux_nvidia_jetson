/*
 * GM20B THERMAL
 *
 * Copyright (c) 2015-2018, NVIDIA CORPORATION.  All rights reserved.
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

#include "gk20a/gk20a.h"

#include "therm_gm20b.h"

#include <nvgpu/hw/gm20b/hw_therm_gm20b.h>

int gm20b_init_therm_setup_hw(struct gk20a *g)
{
	u32 v;

	nvgpu_log_fn(g, " ");

	/* program NV_THERM registers */
	gk20a_writel(g, therm_use_a_r(), therm_use_a_ext_therm_0_enable_f() |
			therm_use_a_ext_therm_1_enable_f()  |
			therm_use_a_ext_therm_2_enable_f());
	gk20a_writel(g, therm_evt_ext_therm_0_r(),
			therm_evt_ext_therm_0_slow_factor_f(0x2));
	gk20a_writel(g, therm_evt_ext_therm_1_r(),
			therm_evt_ext_therm_1_slow_factor_f(0x6));
	gk20a_writel(g, therm_evt_ext_therm_2_r(),
			therm_evt_ext_therm_2_slow_factor_f(0xe));

	gk20a_writel(g, therm_grad_stepping_table_r(0),
		therm_grad_stepping_table_slowdown_factor0_f(therm_grad_stepping_table_slowdown_factor0_fpdiv_by1p5_f()) |
		therm_grad_stepping_table_slowdown_factor1_f(therm_grad_stepping_table_slowdown_factor0_fpdiv_by2_f()) |
		therm_grad_stepping_table_slowdown_factor2_f(therm_grad_stepping_table_slowdown_factor0_fpdiv_by4_f()) |
		therm_grad_stepping_table_slowdown_factor3_f(therm_grad_stepping_table_slowdown_factor0_fpdiv_by8_f()) |
		therm_grad_stepping_table_slowdown_factor4_f(therm_grad_stepping_table_slowdown_factor0_fpdiv_by8_f()));
	gk20a_writel(g, therm_grad_stepping_table_r(1),
		therm_grad_stepping_table_slowdown_factor0_f(therm_grad_stepping_table_slowdown_factor0_fpdiv_by8_f()) |
		therm_grad_stepping_table_slowdown_factor1_f(therm_grad_stepping_table_slowdown_factor0_fpdiv_by8_f()) |
		therm_grad_stepping_table_slowdown_factor2_f(therm_grad_stepping_table_slowdown_factor0_fpdiv_by8_f()) |
		therm_grad_stepping_table_slowdown_factor3_f(therm_grad_stepping_table_slowdown_factor0_fpdiv_by8_f()) |
		therm_grad_stepping_table_slowdown_factor4_f(therm_grad_stepping_table_slowdown_factor0_fpdiv_by8_f()));

	v = gk20a_readl(g, therm_clk_timing_r(0));
	v |= therm_clk_timing_grad_slowdown_enabled_f();
	gk20a_writel(g, therm_clk_timing_r(0), v);

	v = gk20a_readl(g, therm_config2_r());
	v |= therm_config2_grad_enable_f(1);
	v |= therm_config2_slowdown_factor_extended_f(1);
	gk20a_writel(g, therm_config2_r(), v);

	gk20a_writel(g, therm_grad_stepping1_r(),
			therm_grad_stepping1_pdiv_duration_f(32));

	v = gk20a_readl(g, therm_grad_stepping0_r());
	v |= therm_grad_stepping0_feature_enable_f();
	gk20a_writel(g, therm_grad_stepping0_r(), v);

	return 0;
}
