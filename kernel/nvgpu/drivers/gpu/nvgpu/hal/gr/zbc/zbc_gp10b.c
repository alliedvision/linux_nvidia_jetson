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

#include "zbc_gp10b.h"

#include <nvgpu/hw/gp10b/hw_gr_gp10b.h>

void gp10b_gr_zbc_init_table_indices(struct gk20a *g,
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
	zbc_indices->min_stencil_index = 0U;
	zbc_indices->max_stencil_index = 0U;
}

u32 gp10b_gr_zbc_get_gpcs_swdx_dss_zbc_c_format_reg(struct gk20a *g)
{
	(void)g;
	return gr_gpcs_swdx_dss_zbc_c_01_to_04_format_r();
}

u32 gp10b_gr_zbc_get_gpcs_swdx_dss_zbc_z_format_reg(struct gk20a *g)
{
	(void)g;
	return gr_gpcs_swdx_dss_zbc_z_01_to_04_format_r();
}

void gp10b_gr_zbc_add_color(struct gk20a *g,
			   struct nvgpu_gr_zbc_entry *color_val, u32 index)
{
	u32 zbc_c;
	u32 zbc_c_format_reg;
	u32 hw_index = nvgpu_safe_sub_u32(index, NVGPU_GR_ZBC_STARTOF_TABLE);

	nvgpu_log(g, gpu_dbg_zbc, "adding color at index %u", index);
	nvgpu_log(g, gpu_dbg_zbc, "color[0-3]: 0x%08x 0x%08x 0x%08x 0x%08x",
		nvgpu_gr_zbc_get_entry_color_ds(color_val, 0),
		nvgpu_gr_zbc_get_entry_color_ds(color_val, 1),
		nvgpu_gr_zbc_get_entry_color_ds(color_val, 2),
		nvgpu_gr_zbc_get_entry_color_ds(color_val, 3));

	nvgpu_writel_loop(g, gr_gpcs_swdx_dss_zbc_color_r_r(hw_index),
			nvgpu_gr_zbc_get_entry_color_ds(color_val, 0));
	nvgpu_writel_loop(g, gr_gpcs_swdx_dss_zbc_color_g_r(hw_index),
			nvgpu_gr_zbc_get_entry_color_ds(color_val, 1));
	nvgpu_writel_loop(g, gr_gpcs_swdx_dss_zbc_color_b_r(hw_index),
			nvgpu_gr_zbc_get_entry_color_ds(color_val, 2));
	nvgpu_writel_loop(g, gr_gpcs_swdx_dss_zbc_color_a_r(hw_index),
			nvgpu_gr_zbc_get_entry_color_ds(color_val, 3));

	/* Update format register */
	zbc_c_format_reg = g->ops.gr.zbc.get_gpcs_swdx_dss_zbc_c_format_reg(g);
	zbc_c = nvgpu_readl(g, zbc_c_format_reg + (hw_index & ~3U));
	zbc_c &= ~(U32(0x7f) << ((hw_index % 4U) * 7U));
	zbc_c |= nvgpu_gr_zbc_get_entry_format(color_val) <<
			((hw_index % 4U) * 7U);
	nvgpu_writel_loop(g, zbc_c_format_reg + (hw_index & ~3U), zbc_c);
}

void gp10b_gr_zbc_add_depth(struct gk20a *g,
			   struct nvgpu_gr_zbc_entry *depth_val, u32 index)
{
	u32 zbc_z;
	u32 zbc_z_format_reg;
	u32 hw_index = nvgpu_safe_sub_u32(index, NVGPU_GR_ZBC_STARTOF_TABLE);

	nvgpu_log(g, gpu_dbg_zbc, "adding depth at index %u", index);
	nvgpu_log(g, gpu_dbg_zbc, "depth: 0x%08x",
		nvgpu_gr_zbc_get_entry_depth(depth_val));

	nvgpu_writel(g, gr_gpcs_swdx_dss_zbc_z_r(hw_index),
		nvgpu_gr_zbc_get_entry_depth(depth_val));

	/* Update format register */
	zbc_z_format_reg = g->ops.gr.zbc.get_gpcs_swdx_dss_zbc_z_format_reg(g);
	zbc_z = nvgpu_readl(g, zbc_z_format_reg + (hw_index & ~3U));
	zbc_z &= ~(U32(0x7f) << ((hw_index % 4U) * 7U));
	zbc_z |= nvgpu_gr_zbc_get_entry_format(depth_val) <<
			(hw_index % 4U) * 7U;
	nvgpu_writel(g, zbc_z_format_reg + (hw_index & ~3U), zbc_z);
}
