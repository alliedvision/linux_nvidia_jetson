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

#include "zbc_gm20b.h"

#include <nvgpu/hw/gm20b/hw_gr_gm20b.h>

void gm20b_gr_zbc_init_table_indices(struct gk20a *g,
			struct nvgpu_gr_zbc_table_indices *zbc_indices)
{
	(void)g;

	/* Color indices */
	zbc_indices->min_color_index = NVGPU_GR_ZBC_STARTOF_TABLE;
	zbc_indices->max_color_index = 15U;

	/* Depth indices */
	zbc_indices->min_depth_index = NVGPU_GR_ZBC_STARTOF_TABLE;
	zbc_indices->max_depth_index = 15U;

	/* Stencil indices */
	zbc_indices->min_stencil_index = 0U;
	zbc_indices->max_stencil_index = 0U;
}

void gm20b_gr_zbc_add_color(struct gk20a *g,
			struct nvgpu_gr_zbc_entry *color_val, u32 index)
{
	/* update ds table */
	nvgpu_writel(g, gr_ds_zbc_color_r_r(),
		gr_ds_zbc_color_r_val_f(
			nvgpu_gr_zbc_get_entry_color_ds(color_val, 0)));
	nvgpu_writel(g, gr_ds_zbc_color_g_r(),
		gr_ds_zbc_color_g_val_f(
			nvgpu_gr_zbc_get_entry_color_ds(color_val, 1)));
	nvgpu_writel(g, gr_ds_zbc_color_b_r(),
		gr_ds_zbc_color_b_val_f(
			nvgpu_gr_zbc_get_entry_color_ds(color_val, 2)));
	nvgpu_writel(g, gr_ds_zbc_color_a_r(),
		gr_ds_zbc_color_a_val_f(
			nvgpu_gr_zbc_get_entry_color_ds(color_val, 3)));

	nvgpu_writel(g, gr_ds_zbc_color_fmt_r(),
		gr_ds_zbc_color_fmt_val_f(
			nvgpu_gr_zbc_get_entry_format(color_val)));

	nvgpu_writel(g, gr_ds_zbc_tbl_index_r(),
		gr_ds_zbc_tbl_index_val_f(index));

	/* trigger the write */
	nvgpu_writel(g, gr_ds_zbc_tbl_ld_r(),
		gr_ds_zbc_tbl_ld_select_c_f() |
		gr_ds_zbc_tbl_ld_action_write_f() |
		gr_ds_zbc_tbl_ld_trigger_active_f());

}

void gm20b_gr_zbc_add_depth(struct gk20a *g,
			   struct nvgpu_gr_zbc_entry *depth_val, u32 index)
{
	/* update ds table */
	nvgpu_writel(g, gr_ds_zbc_z_r(),
		gr_ds_zbc_z_val_f(
			nvgpu_gr_zbc_get_entry_depth(depth_val)));

	nvgpu_writel(g, gr_ds_zbc_z_fmt_r(),
		gr_ds_zbc_z_fmt_val_f(
			nvgpu_gr_zbc_get_entry_format(depth_val)));

	nvgpu_writel(g, gr_ds_zbc_tbl_index_r(),
		gr_ds_zbc_tbl_index_val_f(index));

	/* trigger the write */
	nvgpu_writel(g, gr_ds_zbc_tbl_ld_r(),
		gr_ds_zbc_tbl_ld_select_z_f() |
		gr_ds_zbc_tbl_ld_action_write_f() |
		gr_ds_zbc_tbl_ld_trigger_active_f());

}
