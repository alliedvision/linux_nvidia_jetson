/*
 * Copyright (c) 2019-2021, NVIDIA CORPORATION.  All rights reserved.
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
#include <nvgpu/gr/config.h>
#include <nvgpu/gr/subctx.h>
#include <nvgpu/gr/ctx.h>
#include <nvgpu/gr/zcull.h>

#include "common/gr/zcull_priv.h"

#include "zcull_gm20b.h"

#include <nvgpu/hw/gm20b/hw_gr_gm20b.h>

int gm20b_gr_init_zcull_hw(struct gk20a *g,
			struct nvgpu_gr_zcull *gr_zcull,
			struct nvgpu_gr_config *gr_config)
{
	u32 gpc_index, gpc_tpc_count, gpc_zcull_count;
	u32 gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_GPC_STRIDE);
	bool floorsweep = false;
	u32 rcp_conserv;
	u32 offset;

	gr_zcull->total_aliquots =
		gr_gpc0_zcull_total_ram_size_num_aliquots_f(
			nvgpu_readl(g, gr_gpc0_zcull_total_ram_size_r()));

	for (gpc_index = 0;
	     gpc_index < nvgpu_gr_config_get_gpc_count(gr_config);
	     gpc_index++) {
		gpc_tpc_count =
			nvgpu_gr_config_get_gpc_tpc_count(gr_config, gpc_index);
		gpc_zcull_count =
			nvgpu_gr_config_get_gpc_zcb_count(gr_config, gpc_index);

		if (gpc_zcull_count !=
			nvgpu_gr_config_get_max_zcull_per_gpc_count(gr_config) &&
			gpc_zcull_count < gpc_tpc_count) {
			nvgpu_err(g,
				"zcull_banks (%d) less than tpcs (%d) for gpc (%d)",
				gpc_zcull_count, gpc_tpc_count, gpc_index);
			return -EINVAL;
		}
		if (gpc_zcull_count !=
			nvgpu_gr_config_get_max_zcull_per_gpc_count(gr_config) &&
		    gpc_zcull_count != 0U) {
			floorsweep = true;
		}
	}

	/* ceil(1.0f / SM_NUM * gr_gpc0_zcull_sm_num_rcp_conservative__max_v())
	 * sm_num in the register manual actually means TPC count
	 */
	rcp_conserv = DIV_ROUND_UP(gr_gpc0_zcull_sm_num_rcp_conservative__max_v(),
		nvgpu_gr_config_get_tpc_count(gr_config));

	for (gpc_index = 0;
	     gpc_index < nvgpu_gr_config_get_gpc_count(gr_config);
	     gpc_index++) {
		offset = gpc_index * gpc_stride;

		if (floorsweep) {
			nvgpu_writel(g, gr_gpc0_zcull_ram_addr_r() + offset,
				gr_gpc0_zcull_ram_addr_row_offset_f(
					nvgpu_gr_config_get_map_row_offset(gr_config)) |
				gr_gpc0_zcull_ram_addr_tiles_per_hypertile_row_per_gpc_f(
					nvgpu_gr_config_get_max_zcull_per_gpc_count(gr_config)));
		} else {
			nvgpu_writel(g, gr_gpc0_zcull_ram_addr_r() + offset,
				gr_gpc0_zcull_ram_addr_row_offset_f(
					nvgpu_gr_config_get_map_row_offset(gr_config)) |
				gr_gpc0_zcull_ram_addr_tiles_per_hypertile_row_per_gpc_f(
					nvgpu_gr_config_get_gpc_tpc_count(gr_config, gpc_index)));
		}

		nvgpu_writel(g, gr_gpc0_zcull_fs_r() + offset,
			gr_gpc0_zcull_fs_num_active_banks_f(
				nvgpu_gr_config_get_gpc_zcb_count(gr_config, gpc_index)) |
			gr_gpc0_zcull_fs_num_sms_f(
				nvgpu_gr_config_get_tpc_count(gr_config)));

		nvgpu_writel(g, gr_gpc0_zcull_sm_num_rcp_r() + offset,
			gr_gpc0_zcull_sm_num_rcp_conservative_f(rcp_conserv));
	}

	nvgpu_writel(g, gr_gpcs_ppcs_wwdx_sm_num_rcp_r(),
		gr_gpcs_ppcs_wwdx_sm_num_rcp_conservative_f(rcp_conserv));

	return 0;
}

int gm20b_gr_get_zcull_info(struct gk20a *g,
			struct nvgpu_gr_config *gr_config,
			struct nvgpu_gr_zcull *zcull,
			struct nvgpu_gr_zcull_info *zcull_params)
{
	zcull_params->width_align_pixels = zcull->width_align_pixels;
	zcull_params->height_align_pixels = zcull->height_align_pixels;
	zcull_params->pixel_squares_by_aliquots =
		zcull->pixel_squares_by_aliquots;
	zcull_params->aliquot_total = zcull->total_aliquots;

	zcull_params->region_byte_multiplier =
		nvgpu_gr_config_get_gpc_count(gr_config) *
		gr_zcull_bytes_per_aliquot_per_gpu_v();
	zcull_params->region_header_size =
		nvgpu_get_litter_value(g, GPU_LIT_NUM_GPCS) *
		gr_zcull_save_restore_header_bytes_per_gpc_v();

	zcull_params->subregion_header_size =
		nvgpu_get_litter_value(g, GPU_LIT_NUM_GPCS) *
		gr_zcull_save_restore_subregion_header_bytes_per_gpc_v();

	zcull_params->subregion_width_align_pixels =
		nvgpu_gr_config_get_tpc_count(gr_config) *
		gr_gpc0_zcull_zcsize_width_subregion__multiple_v();
	zcull_params->subregion_height_align_pixels =
		gr_gpc0_zcull_zcsize_height_subregion__multiple_v();
	zcull_params->subregion_count = gr_zcull_subregion_qty_v();

	return 0;
}

void gm20b_gr_program_zcull_mapping(struct gk20a *g, u32 zcull_num_entries,
						u32 *zcull_map_tiles)
{
	u32 val;

	nvgpu_log_fn(g, " ");

	if (zcull_num_entries >= 8U) {
		nvgpu_log_fn(g, "map0");
		val =
		gr_gpcs_zcull_sm_in_gpc_number_map0_tile_0_f(
						zcull_map_tiles[0]) |
		gr_gpcs_zcull_sm_in_gpc_number_map0_tile_1_f(
						zcull_map_tiles[1]) |
		gr_gpcs_zcull_sm_in_gpc_number_map0_tile_2_f(
						zcull_map_tiles[2]) |
		gr_gpcs_zcull_sm_in_gpc_number_map0_tile_3_f(
						zcull_map_tiles[3]) |
		gr_gpcs_zcull_sm_in_gpc_number_map0_tile_4_f(
						zcull_map_tiles[4]) |
		gr_gpcs_zcull_sm_in_gpc_number_map0_tile_5_f(
						zcull_map_tiles[5]) |
		gr_gpcs_zcull_sm_in_gpc_number_map0_tile_6_f(
						zcull_map_tiles[6]) |
		gr_gpcs_zcull_sm_in_gpc_number_map0_tile_7_f(
						zcull_map_tiles[7]);

		gk20a_writel(g, gr_gpcs_zcull_sm_in_gpc_number_map0_r(), val);
	}

	if (zcull_num_entries >= 16U) {
		nvgpu_log_fn(g, "map1");
		val =
		gr_gpcs_zcull_sm_in_gpc_number_map1_tile_8_f(
						zcull_map_tiles[8]) |
		gr_gpcs_zcull_sm_in_gpc_number_map1_tile_9_f(
						zcull_map_tiles[9]) |
		gr_gpcs_zcull_sm_in_gpc_number_map1_tile_10_f(
						zcull_map_tiles[10]) |
		gr_gpcs_zcull_sm_in_gpc_number_map1_tile_11_f(
						zcull_map_tiles[11]) |
		gr_gpcs_zcull_sm_in_gpc_number_map1_tile_12_f(
						zcull_map_tiles[12]) |
		gr_gpcs_zcull_sm_in_gpc_number_map1_tile_13_f(
						zcull_map_tiles[13]) |
		gr_gpcs_zcull_sm_in_gpc_number_map1_tile_14_f(
						zcull_map_tiles[14]) |
		gr_gpcs_zcull_sm_in_gpc_number_map1_tile_15_f(
						zcull_map_tiles[15]);

		gk20a_writel(g, gr_gpcs_zcull_sm_in_gpc_number_map1_r(), val);
	}

	if (zcull_num_entries >= 24U) {
		nvgpu_log_fn(g, "map2");
		val =
		gr_gpcs_zcull_sm_in_gpc_number_map2_tile_16_f(
						zcull_map_tiles[16]) |
		gr_gpcs_zcull_sm_in_gpc_number_map2_tile_17_f(
						zcull_map_tiles[17]) |
		gr_gpcs_zcull_sm_in_gpc_number_map2_tile_18_f(
						zcull_map_tiles[18]) |
		gr_gpcs_zcull_sm_in_gpc_number_map2_tile_19_f(
						zcull_map_tiles[19]) |
		gr_gpcs_zcull_sm_in_gpc_number_map2_tile_20_f(
						zcull_map_tiles[20]) |
		gr_gpcs_zcull_sm_in_gpc_number_map2_tile_21_f(
						zcull_map_tiles[21]) |
		gr_gpcs_zcull_sm_in_gpc_number_map2_tile_22_f(
						zcull_map_tiles[22]) |
		gr_gpcs_zcull_sm_in_gpc_number_map2_tile_23_f(
						zcull_map_tiles[23]);

		gk20a_writel(g, gr_gpcs_zcull_sm_in_gpc_number_map2_r(), val);
	}

	if (zcull_num_entries >= 32U) {
		nvgpu_log_fn(g, "map3");
		val =
		gr_gpcs_zcull_sm_in_gpc_number_map3_tile_24_f(
						zcull_map_tiles[24]) |
		gr_gpcs_zcull_sm_in_gpc_number_map3_tile_25_f(
						zcull_map_tiles[25]) |
		gr_gpcs_zcull_sm_in_gpc_number_map3_tile_26_f(
						zcull_map_tiles[26]) |
		gr_gpcs_zcull_sm_in_gpc_number_map3_tile_27_f(
						zcull_map_tiles[27]) |
		gr_gpcs_zcull_sm_in_gpc_number_map3_tile_28_f(
						zcull_map_tiles[28]) |
		gr_gpcs_zcull_sm_in_gpc_number_map3_tile_29_f(
						zcull_map_tiles[29]) |
		gr_gpcs_zcull_sm_in_gpc_number_map3_tile_30_f(
						zcull_map_tiles[30]) |
		gr_gpcs_zcull_sm_in_gpc_number_map3_tile_31_f(
						zcull_map_tiles[31]);

		gk20a_writel(g, gr_gpcs_zcull_sm_in_gpc_number_map3_r(), val);
	}

}

