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

#include <nvgpu/types.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/io.h>
#include <nvgpu/gr/zbc.h>

#include "ltc_ga10b.h"

#include <nvgpu/hw/ga10b/hw_ltc_ga10b.h>

#ifdef CONFIG_NVGPU_GRAPHICS
void ga10b_ltc_set_zbc_stencil_entry(struct gk20a *g, u32 stencil_depth,
					u32 index)
{
	nvgpu_writel(g, ltc_ltcs_ltss_dstg_zbc_index_r(),
		ltc_ltcs_ltss_dstg_zbc_index_address_f(index));

	nvgpu_writel(g,
 		ltc_ltcs_ltss_dstg_zbc_stencil_clear_value_r(), stencil_depth);
}

void ga10b_ltc_set_zbc_color_entry(struct gk20a *g, u32 *color_l2, u32 index)
{
	u32 i;

	nvgpu_writel(g, ltc_ltcs_ltss_dstg_zbc_index_r(),
		ltc_ltcs_ltss_dstg_zbc_index_address_f(index));

	for (i = 0; i < ltc_ltcs_ltss_dstg_zbc_color_clear_value__size_1_v();
									i++) {
		nvgpu_writel(g,
			ltc_ltcs_ltss_dstg_zbc_color_clear_value_r(i),
			color_l2[i]);
	}
}

/*
 * Sets the ZBC depth for the passed index.
 */
void ga10b_ltc_set_zbc_depth_entry(struct gk20a *g, u32 depth_val, u32 index)
{
	nvgpu_writel(g, ltc_ltcs_ltss_dstg_zbc_index_r(),
		ltc_ltcs_ltss_dstg_zbc_index_address_f(index));

	nvgpu_writel(g,
		ltc_ltcs_ltss_dstg_zbc_depth_clear_value_r(), depth_val);
}
#endif

#ifdef CONFIG_NVGPU_DEBUGGER
u32 ga10b_ltc_pri_shared_addr(struct gk20a *g, u32 addr)
{
	u32 ltc_stride = nvgpu_get_litter_value(g, GPU_LIT_LTC_STRIDE);
	u32 lts_stride = nvgpu_get_litter_value(g, GPU_LIT_LTS_STRIDE);
	u32 ltc_shared_base = ltc_ltcs_ltss_v();
	u32 ltc_addr_mask = nvgpu_safe_sub_u32(ltc_stride, 1);
	u32 lts_addr_mask = nvgpu_safe_sub_u32(lts_stride, 1);
	u32 ltc_addr = addr & ltc_addr_mask;
	u32 lts_addr = ltc_addr & lts_addr_mask;

	return ltc_shared_base + lts_addr;
}
#endif
