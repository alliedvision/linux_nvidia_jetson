/*
 * Copyright (c) 2019-2022, NVIDIA CORPORATION.  All rights reserved.
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
#ifndef NVGPU_GOPS_CG_H
#define NVGPU_GOPS_CG_H

#include <nvgpu/types.h>

/**
 * @file
 *
 * CG HAL interface.
 */
struct gk20a;

/**
 * CG HAL operations.
 *
 * @see gpu_ops.
 */
struct gops_cg {
	/** @cond DOXYGEN_SHOULD_SKIP_THIS */
	void (*slcg_bus_load_gating_prod)(struct gk20a *g, bool prod);
	void (*slcg_ce2_load_gating_prod)(struct gk20a *g, bool prod);
	void (*slcg_chiplet_load_gating_prod)(struct gk20a *g, bool prod);
	void (*slcg_fb_load_gating_prod)(struct gk20a *g, bool prod);
	void (*slcg_fifo_load_gating_prod)(struct gk20a *g, bool prod);
	void (*slcg_gr_load_gating_prod)(struct gk20a *g, bool prod);
	void (*slcg_ltc_load_gating_prod)(struct gk20a *g, bool prod);
	void (*slcg_perf_load_gating_prod)(struct gk20a *g, bool prod);
	void (*slcg_priring_load_gating_prod)(struct gk20a *g, bool prod);
	void (*slcg_pmu_load_gating_prod)(struct gk20a *g, bool prod);
	void (*slcg_therm_load_gating_prod)(struct gk20a *g, bool prod);
	void (*slcg_xbar_load_gating_prod)(struct gk20a *g, bool prod);
	void (*slcg_hshub_load_gating_prod)(struct gk20a *g, bool prod);
	void (*slcg_acb_load_gating_prod)(struct gk20a *g, bool prod);
	void (*blcg_bus_load_gating_prod)(struct gk20a *g, bool prod);
	void (*blcg_ce_load_gating_prod)(struct gk20a *g, bool prod);
	void (*blcg_fb_load_gating_prod)(struct gk20a *g, bool prod);
	void (*blcg_fifo_load_gating_prod)(struct gk20a *g, bool prod);
	void (*blcg_gr_load_gating_prod)(struct gk20a *g, bool prod);
	void (*blcg_ltc_load_gating_prod)(struct gk20a *g, bool prod);
	void (*blcg_pmu_load_gating_prod)(struct gk20a *g, bool prod);
	void (*blcg_xbar_load_gating_prod)(struct gk20a *g, bool prod);
	void (*blcg_hshub_load_gating_prod)(struct gk20a *g, bool prod);
	void (*slcg_runlist_load_gating_prod)(struct gk20a *g, bool prod);
	void (*blcg_runlist_load_gating_prod)(struct gk20a *g, bool prod);

	/* Ring station slcg prod gops */
	void (*slcg_rs_ctrl_fbp_load_gating_prod)(struct gk20a *g, bool prod);
	void (*slcg_rs_ctrl_gpc_load_gating_prod)(struct gk20a *g, bool prod);
	void (*slcg_rs_ctrl_sys_load_gating_prod)(struct gk20a *g, bool prod);
	void (*slcg_rs_fbp_load_gating_prod)(struct gk20a *g, bool prod);
	void (*slcg_rs_gpc_load_gating_prod)(struct gk20a *g, bool prod);
	void (*slcg_rs_sys_load_gating_prod)(struct gk20a *g, bool prod);

	void (*slcg_timer_load_gating_prod)(struct gk20a *g, bool prod);

	void (*slcg_ctrl_load_gating_prod)(struct gk20a *g, bool prod);

	void (*slcg_gsp_load_gating_prod)(struct gk20a *g, bool prod);

	void (*elcg_ce_load_gating_prod)(struct gk20a *g, bool prod);
	/** @endcond DOXYGEN_SHOULD_SKIP_THIS */
};

#endif
