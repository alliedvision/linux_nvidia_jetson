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

#include <nvgpu/gk20a.h>
#include <nvgpu/io.h>
#include <nvgpu/gr/zbc.h>

#include "zbc_gv11b.h"

#include <nvgpu/hw/gv11b/hw_gr_gv11b.h>

void gv11b_gr_zbc_init_table_indices(struct gk20a *g,
			struct nvgpu_gr_zbc_table_indices *zbc_indices)
{
	(void)g;

	/* Color indices */
	zbc_indices->min_color_index = NVGPU_GR_ZBC_STARTOF_TABLE;
	zbc_indices->max_color_index = gr_gpcs_swdx_dss_zbc_color_r__size_1_v();

	/* Depth indices */
	zbc_indices->min_depth_index = NVGPU_GR_ZBC_STARTOF_TABLE;
	zbc_indices->max_depth_index = gr_gpcs_swdx_dss_zbc_z__size_1_v();

	/* Stencil indices */
	zbc_indices->min_stencil_index = NVGPU_GR_ZBC_STARTOF_TABLE;
	zbc_indices->max_stencil_index = gr_gpcs_swdx_dss_zbc_s__size_1_v();
}

u32 gv11b_gr_zbc_get_gpcs_swdx_dss_zbc_c_format_reg(struct gk20a *g)
{
	(void)g;
	return gr_gpcs_swdx_dss_zbc_c_01_to_04_format_r();
}

u32 gv11b_gr_zbc_get_gpcs_swdx_dss_zbc_z_format_reg(struct gk20a *g)
{
	(void)g;
	return gr_gpcs_swdx_dss_zbc_z_01_to_04_format_r();
}

void gv11b_gr_zbc_add_stencil(struct gk20a *g,
			     struct nvgpu_gr_zbc_entry *stencil_val, u32 index)
{
	u32 zbc_s;
	u32 hw_index = nvgpu_safe_sub_u32(index, NVGPU_GR_ZBC_STARTOF_TABLE);

	nvgpu_log(g, gpu_dbg_zbc, "adding stencil at index %u", index);
	nvgpu_log(g, gpu_dbg_zbc, "stencil: 0x%08x",
		nvgpu_gr_zbc_get_entry_stencil(stencil_val));

	nvgpu_writel(g, gr_gpcs_swdx_dss_zbc_s_r(hw_index),
		nvgpu_gr_zbc_get_entry_stencil(stencil_val));

	/* update format register */
	zbc_s = nvgpu_readl(g, gr_gpcs_swdx_dss_zbc_s_01_to_04_format_r() +
				 (hw_index & ~3U));
	zbc_s &= ~(U32(0x7f) << (hw_index % 4U) * 7U);
	zbc_s |= nvgpu_gr_zbc_get_entry_format(stencil_val) <<
		(hw_index % 4U) * 7U;
	nvgpu_writel(g, gr_gpcs_swdx_dss_zbc_s_01_to_04_format_r() +
				 (hw_index & ~3U), zbc_s);
}
