/*
 * GM20B L2 INTR
 *
 * Copyright (c) 2014-2020 NVIDIA CORPORATION.  All rights reserved.
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

#include <nvgpu/ltc.h>
#include <nvgpu/io.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/static_analysis.h>

#include "ltc_intr_gm20b.h"

#include <nvgpu/hw/gm20b/hw_ltc_gm20b.h>


void gm20b_ltc_intr_configure(struct gk20a *g)
{
	u32 reg;

	/* Disable interrupts to reduce noise and increase perf */
	reg = nvgpu_readl(g, ltc_ltcs_ltss_intr_r());
	reg &= ~ltc_ltcs_ltss_intr_en_evicted_cb_m();
	reg &= ~ltc_ltcs_ltss_intr_en_illegal_compstat_access_m();
	reg &= ~ltc_ltcs_ltss_intr_en_illegal_compstat_m();
	nvgpu_writel(g, ltc_ltcs_ltss_intr_r(), reg);
}

static void gm20b_ltc_intr_handle_lts_interrupts(struct gk20a *g,
							u32 ltc, u32 slice)
{
	u32 ltc_intr;
	u32 ltc_stride = nvgpu_get_litter_value(g, GPU_LIT_LTC_STRIDE);
	u32 lts_stride = nvgpu_get_litter_value(g, GPU_LIT_LTS_STRIDE);

	ltc_intr = nvgpu_readl(g, nvgpu_safe_add_u32(ltc_ltc0_lts0_intr_r(),
			nvgpu_safe_add_u32(nvgpu_safe_mult_u32(ltc_stride, ltc),
				nvgpu_safe_mult_u32(lts_stride, slice))));
	nvgpu_log(g, gpu_dbg_intr, "ltc%d, slice %d: %08x",
		  ltc, slice, ltc_intr);
	nvgpu_writel(g, nvgpu_safe_add_u32(ltc_ltc0_lts0_intr_r(),
			nvgpu_safe_add_u32(nvgpu_safe_mult_u32(ltc_stride, ltc),
			   nvgpu_safe_mult_u32(lts_stride, slice))), ltc_intr);
}

void gm20b_ltc_intr_isr(struct gk20a *g, u32 ltc)
{
	u32 slice;

	for (slice = 0U; slice < g->ltc->slices_per_ltc; slice =
					nvgpu_safe_add_u32(slice, 1U)) {
		gm20b_ltc_intr_handle_lts_interrupts(g, ltc, slice);
	}
}
