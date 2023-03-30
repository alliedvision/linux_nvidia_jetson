/*
 * Copyright (c) 2020, NVIDIA CORPORATION.  All rights reserved.
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
#ifndef NVGPU_GOPS_CLK_ARB_H
#define NVGPU_GOPS_CLK_ARB_H

#ifdef CONFIG_NVGPU_CLK_ARB
struct gops_clk_arb {
	int (*clk_arb_init_arbiter)(struct gk20a *g);
	int (*arbiter_clk_init)(struct gk20a *g);
	bool (*check_clk_arb_support)(struct gk20a *g);
	u32 (*get_arbiter_clk_domains)(struct gk20a *g);
	int (*get_arbiter_f_points)(struct gk20a *g, u32 api_domain,
			u32 *num_points, u16 *freqs_in_mhz);
	int (*get_arbiter_clk_range)(struct gk20a *g, u32 api_domain,
			u16 *min_mhz, u16 *max_mhz);
	int (*get_arbiter_clk_default)(struct gk20a *g, u32 api_domain,
			u16 *default_mhz);
	void (*clk_arb_run_arbiter_cb)(struct nvgpu_clk_arb *arb);
	/* This function is inherently unsafe to call while
	 *  arbiter is running arbiter must be blocked
	 *  before calling this function
	 */
	u32 (*get_current_pstate)(struct gk20a *g);
	void (*clk_arb_cleanup)(struct nvgpu_clk_arb *arb);
	void (*stop_clk_arb_threads)(struct gk20a *g);
};
#endif

#endif /* NVGPU_GOPS_CLK_ARB_H */
