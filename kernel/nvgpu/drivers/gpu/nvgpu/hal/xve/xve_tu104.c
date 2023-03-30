/*
 * Copyright (c) 2019, NVIDIA CORPORATION.  All rights reserved.
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

#include <nvgpu/bug.h>
#include <nvgpu/xve.h>
#include <nvgpu/io.h>
#include <nvgpu/utils.h>
#include <nvgpu/timers.h>
#include <nvgpu/gk20a.h>

#include "xve_tu104.h"
#include "xve_gp106.h"

#include <nvgpu/hw/tu104/hw_xve_tu104.h>
#include <nvgpu/hw/tu104/hw_xp_tu104.h>

#define DL_TIMER_LIMIT 0x58EU

void tu104_devinit_deferred_settings(struct gk20a *g)
{
	u32 data;
	g->ops.xve.xve_writel(g, xve_pcie_capability_r(),
			xve_pcie_capability_gen2_capable_enable_f() |
			xve_pcie_capability_gen3_capable_enable_f());
	nvgpu_writel(g, xp_dl_mgr_timing_r(0), DL_TIMER_LIMIT);
	data = xve_high_latency_snoop_latency_value_init_f() |
			xve_high_latency_snoop_latency_scale_init_f() |
			xve_high_latency_no_snoop_latency_value_init_f() |
			xve_high_latency_no_snoop_latency_scale_init_f();
	g->ops.xve.xve_writel(g, xve_high_latency_r(), data);
	g->ops.xve.xve_writel(g, xve_ltr_msg_ctrl_r(),
			xve_ltr_msg_ctrl_trigger_not_pending_f());
}
