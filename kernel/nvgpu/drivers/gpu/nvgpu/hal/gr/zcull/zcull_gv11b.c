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

#include <nvgpu/gk20a.h>
#include <nvgpu/io.h>
#include <nvgpu/gr/zcull.h>

#include "zcull_gv11b.h"

#include <nvgpu/hw/gv11b/hw_gr_gv11b.h>

void gv11b_gr_program_zcull_mapping(struct gk20a *g, u32 zcull_num_entries,
					u32 *zcull_map_tiles)
{
	u32 val, i, j = 0U;

	nvgpu_log_fn(g, " ");

	for (i = 0U; i < (zcull_num_entries / 8U); i++) {
		val =
		gr_gpcs_zcull_sm_in_gpc_number_map_tile_0_f(
						zcull_map_tiles[j+0U]) |
		gr_gpcs_zcull_sm_in_gpc_number_map_tile_1_f(
						zcull_map_tiles[j+1U]) |
		gr_gpcs_zcull_sm_in_gpc_number_map_tile_2_f(
						zcull_map_tiles[j+2U]) |
		gr_gpcs_zcull_sm_in_gpc_number_map_tile_3_f(
						zcull_map_tiles[j+3U]) |
		gr_gpcs_zcull_sm_in_gpc_number_map_tile_4_f(
						zcull_map_tiles[j+4U]) |
		gr_gpcs_zcull_sm_in_gpc_number_map_tile_5_f(
						zcull_map_tiles[j+5U]) |
		gr_gpcs_zcull_sm_in_gpc_number_map_tile_6_f(
						zcull_map_tiles[j+6U]) |
		gr_gpcs_zcull_sm_in_gpc_number_map_tile_7_f(
						zcull_map_tiles[j+7U]);

		gk20a_writel(g, gr_gpcs_zcull_sm_in_gpc_number_map_r(i), val);

		j += 8U;
	}
}

