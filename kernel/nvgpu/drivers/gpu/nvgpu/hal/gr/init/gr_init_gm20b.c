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
#include <nvgpu/log.h>
#include <nvgpu/bug.h>
#include <nvgpu/static_analysis.h>
#include <nvgpu/timers.h>
#include <nvgpu/enabled.h>
#include <nvgpu/engines.h>
#include <nvgpu/engine_status.h>
#include <nvgpu/netlist.h>
#include <nvgpu/gr/ctx.h>
#include <nvgpu/gr/config.h>
#include <nvgpu/ltc.h>

#include <nvgpu/gr/gr.h>
#include <nvgpu/gr/config.h>
#include <nvgpu/gr/gr_instances.h>

#include "gr_init_gm20b.h"

#include <nvgpu/hw/gm20b/hw_gr_gm20b.h>

#define FE_PWR_MODE_TIMEOUT_MAX_US 2000U
#define FE_PWR_MODE_TIMEOUT_DEFAULT_US 10U
#define FECS_CTXSW_RESET_DELAY_US 10U

void gm20b_gr_init_gpc_mmu(struct gk20a *g)
{
	u32 temp;

	nvgpu_log_info(g, "initialize gpc mmu");

	temp = g->ops.fb.mmu_ctrl(g);
	temp &= gr_gpcs_pri_mmu_ctrl_vm_pg_size_m() |
		gr_gpcs_pri_mmu_ctrl_use_pdb_big_page_size_m() |
		gr_gpcs_pri_mmu_ctrl_use_full_comp_tag_line_m() |
		gr_gpcs_pri_mmu_ctrl_vol_fault_m() |
		gr_gpcs_pri_mmu_ctrl_comp_fault_m() |
		gr_gpcs_pri_mmu_ctrl_miss_gran_m() |
		gr_gpcs_pri_mmu_ctrl_cache_mode_m() |
		gr_gpcs_pri_mmu_ctrl_mmu_aperture_m() |
		gr_gpcs_pri_mmu_ctrl_mmu_vol_m() |
		gr_gpcs_pri_mmu_ctrl_mmu_disable_m();
	nvgpu_writel(g, gr_gpcs_pri_mmu_ctrl_r(), temp);
	nvgpu_writel(g, gr_gpcs_pri_mmu_pm_unit_mask_r(), 0);
	nvgpu_writel(g, gr_gpcs_pri_mmu_pm_req_mask_r(), 0);

	nvgpu_writel(g, gr_gpcs_pri_mmu_debug_ctrl_r(),
			g->ops.fb.mmu_debug_ctrl(g));
	nvgpu_writel(g, gr_gpcs_pri_mmu_debug_wr_r(),
			g->ops.fb.mmu_debug_wr(g));
	nvgpu_writel(g, gr_gpcs_pri_mmu_debug_rd_r(),
			g->ops.fb.mmu_debug_rd(g));

	nvgpu_writel(g, gr_gpcs_mmu_num_active_ltcs_r(),
			nvgpu_ltc_get_ltc_count(g));
}

#ifdef CONFIG_NVGPU_SET_FALCON_ACCESS_MAP
void gm20b_gr_init_get_access_map(struct gk20a *g,
				   u32 **whitelist, u32 *num_entries)
{
	static u32 wl_addr_gm20b[] = {
		/* this list must be sorted (low to high) */
		0x404468, /* gr_pri_mme_max_instructions       */
		0x418380, /* gr_pri_gpcs_rasterarb_line_class  */
		0x418800, /* gr_pri_gpcs_setup_debug           */
		0x418830, /* gr_pri_gpcs_setup_debug_z_gamut_offset */
		0x4188fc, /* gr_pri_gpcs_zcull_ctx_debug       */
		0x418e00, /* gr_pri_gpcs_swdx_config           */
		0x418e40, /* gr_pri_gpcs_swdx_tc_bundle_ctrl   */
		0x418e44, /* gr_pri_gpcs_swdx_tc_bundle_ctrl   */
		0x418e48, /* gr_pri_gpcs_swdx_tc_bundle_ctrl   */
		0x418e4c, /* gr_pri_gpcs_swdx_tc_bundle_ctrl   */
		0x418e50, /* gr_pri_gpcs_swdx_tc_bundle_ctrl   */
		0x418e58, /* gr_pri_gpcs_swdx_tc_bundle_addr   */
		0x418e5c, /* gr_pri_gpcs_swdx_tc_bundle_addr   */
		0x418e60, /* gr_pri_gpcs_swdx_tc_bundle_addr   */
		0x418e64, /* gr_pri_gpcs_swdx_tc_bundle_addr   */
		0x418e68, /* gr_pri_gpcs_swdx_tc_bundle_addr   */
		0x418e6c, /* gr_pri_gpcs_swdx_tc_bundle_addr   */
		0x418e70, /* gr_pri_gpcs_swdx_tc_bundle_addr   */
		0x418e74, /* gr_pri_gpcs_swdx_tc_bundle_addr   */
		0x418e78, /* gr_pri_gpcs_swdx_tc_bundle_addr   */
		0x418e7c, /* gr_pri_gpcs_swdx_tc_bundle_addr   */
		0x418e80, /* gr_pri_gpcs_swdx_tc_bundle_addr   */
		0x418e84, /* gr_pri_gpcs_swdx_tc_bundle_addr   */
		0x418e88, /* gr_pri_gpcs_swdx_tc_bundle_addr   */
		0x418e8c, /* gr_pri_gpcs_swdx_tc_bundle_addr   */
		0x418e90, /* gr_pri_gpcs_swdx_tc_bundle_addr   */
		0x418e94, /* gr_pri_gpcs_swdx_tc_bundle_addr   */
		0x419864, /* gr_pri_gpcs_tpcs_pe_l2_evict_policy */
		0x419a04, /* gr_pri_gpcs_tpcs_tex_lod_dbg      */
		0x419a08, /* gr_pri_gpcs_tpcs_tex_samp_dbg     */
		0x419e10, /* gr_pri_gpcs_tpcs_sm_dbgr_control0 */
		0x419f78, /* gr_pri_gpcs_tpcs_sm_disp_ctrl     */
	};
	size_t array_size;

	(void)g;
	*whitelist = wl_addr_gm20b;
	array_size = ARRAY_SIZE(wl_addr_gm20b);
	*num_entries = nvgpu_safe_cast_u64_to_u32(array_size);
}
#endif

void gm20b_gr_init_sm_id_numbering(struct gk20a *g, u32 gpc, u32 tpc, u32 smid,
				struct nvgpu_gr_config *gr_config,
				struct nvgpu_gr_ctx *gr_ctx,
				bool patch)
{
	u32 gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_GPC_STRIDE);
	u32 tpc_in_gpc_stride = nvgpu_get_litter_value(g,
						GPU_LIT_TPC_IN_GPC_STRIDE);
	u32 gpc_offset = nvgpu_safe_mult_u32(gpc_stride, gpc);
	u32 tpc_offset = nvgpu_safe_mult_u32(tpc_in_gpc_stride, tpc);
	u32 offset_sum = nvgpu_safe_add_u32(gpc_offset, tpc_offset);

	(void)gr_config;
	(void)gr_ctx;
	(void)patch;

	nvgpu_writel(g,
		     nvgpu_safe_add_u32(gr_gpc0_tpc0_sm_cfg_r(), offset_sum),
		     gr_gpc0_tpc0_sm_cfg_sm_id_f(smid));
	nvgpu_writel(g,
		     nvgpu_safe_add_u32(gr_gpc0_gpm_pd_sm_id_r(tpc),
								gpc_offset),
		     gr_gpc0_gpm_pd_sm_id_id_f(smid));
	nvgpu_writel(g,
		     nvgpu_safe_add_u32(gr_gpc0_tpc0_pe_cfg_smid_r(),
								offset_sum),
		     gr_gpc0_tpc0_pe_cfg_smid_value_f(smid));
}

u32 gm20b_gr_init_get_sm_id_size(void)
{
	return gr_cwd_sm_id__size_1_v();
}

int gm20b_gr_init_sm_id_config(struct gk20a *g, u32 *tpc_sm_id,
				struct nvgpu_gr_config *gr_config,
				struct nvgpu_gr_ctx *gr_ctx,
				bool patch)
{
	u32 i, j;
	u32 tpc_index, gpc_index;
	u32 tpc_cnt = nvgpu_safe_sub_u32(
			nvgpu_gr_config_get_tpc_count(gr_config), 1U);

	(void)gr_ctx;
	(void)patch;

	/* Each NV_PGRAPH_PRI_CWD_GPC_TPC_ID can store 4 TPCs.*/
	for (i = 0U;
	     i <= (tpc_cnt / 4U);
	     i++) {
		u32 reg = 0;
		u32 bit_stride = nvgpu_safe_add_u32(
					gr_cwd_gpc_tpc_id_gpc0_s(),
					gr_cwd_gpc_tpc_id_tpc0_s());

		for (j = 0U; j < 4U; j++) {
			u32 sm_id = nvgpu_safe_add_u32(
					nvgpu_safe_mult_u32(i, 4U), j);
			u32 bits;
			struct nvgpu_sm_info *sm_info;

			if (sm_id >=
				nvgpu_gr_config_get_tpc_count(gr_config)) {
				break;
			}
			sm_info =
				nvgpu_gr_config_get_sm_info(gr_config, sm_id);
			gpc_index =
				nvgpu_gr_config_get_sm_info_gpc_index(sm_info);
			tpc_index =
				nvgpu_gr_config_get_sm_info_tpc_index(sm_info);

			bits = gr_cwd_gpc_tpc_id_gpc0_f(gpc_index) |
			       gr_cwd_gpc_tpc_id_tpc0_f(tpc_index);
			reg |= bits << nvgpu_safe_mult_u32(j, bit_stride);

			tpc_sm_id[gpc_index] |=
				(sm_id <<
				nvgpu_safe_mult_u32(tpc_index, bit_stride));
		}
		nvgpu_writel(g, gr_cwd_gpc_tpc_id_r(i), reg);
	}

	for (i = 0; i < gr_cwd_sm_id__size_1_v(); i++) {
		nvgpu_writel(g, gr_cwd_sm_id_r(i), tpc_sm_id[i]);
	}

	return 0;
}

void gm20b_gr_init_tpc_mask(struct gk20a *g, u32 gpc_index, u32 pes_tpc_mask)
{
	(void)gpc_index;
	nvgpu_writel(g, gr_fe_tpc_fs_r(), pes_tpc_mask);
}

#ifdef CONFIG_NVGPU_GRAPHICS
void gm20b_gr_init_rop_mapping(struct gk20a *g,
			      struct nvgpu_gr_config *gr_config)
{
	u32 norm_entries, norm_shift;
	u32 coeff5_mod, coeff6_mod, coeff7_mod, coeff8_mod;
	u32 coeff9_mod, coeff10_mod, coeff11_mod;
	u32 map0, map1, map2, map3, map4, map5;
	u32 tpc_cnt;

	nvgpu_log_fn(g, " ");

	tpc_cnt = nvgpu_gr_config_get_tpc_count(gr_config);

	nvgpu_writel(g, gr_crstr_map_table_cfg_r(),
		     gr_crstr_map_table_cfg_row_offset_f(
			nvgpu_gr_config_get_map_row_offset(gr_config)) |
		     gr_crstr_map_table_cfg_num_entries_f(tpc_cnt));

	map0 =  gr_crstr_gpc_map0_tile0_f(
			nvgpu_gr_config_get_map_tile_count(gr_config, 0)) |
		gr_crstr_gpc_map0_tile1_f(
			nvgpu_gr_config_get_map_tile_count(gr_config, 1)) |
		gr_crstr_gpc_map0_tile2_f(
			nvgpu_gr_config_get_map_tile_count(gr_config, 2)) |
		gr_crstr_gpc_map0_tile3_f(
			nvgpu_gr_config_get_map_tile_count(gr_config, 3)) |
		gr_crstr_gpc_map0_tile4_f(
			nvgpu_gr_config_get_map_tile_count(gr_config, 4)) |
		gr_crstr_gpc_map0_tile5_f(
			nvgpu_gr_config_get_map_tile_count(gr_config, 5));

	map1 =  gr_crstr_gpc_map1_tile6_f(
			nvgpu_gr_config_get_map_tile_count(gr_config, 6)) |
		gr_crstr_gpc_map1_tile7_f(
			nvgpu_gr_config_get_map_tile_count(gr_config, 7)) |
		gr_crstr_gpc_map1_tile8_f(
			nvgpu_gr_config_get_map_tile_count(gr_config, 8)) |
		gr_crstr_gpc_map1_tile9_f(
			nvgpu_gr_config_get_map_tile_count(gr_config, 9)) |
		gr_crstr_gpc_map1_tile10_f(
			nvgpu_gr_config_get_map_tile_count(gr_config, 10)) |
		gr_crstr_gpc_map1_tile11_f(
			nvgpu_gr_config_get_map_tile_count(gr_config, 11));

	map2 =  gr_crstr_gpc_map2_tile12_f(
			nvgpu_gr_config_get_map_tile_count(gr_config, 12)) |
		gr_crstr_gpc_map2_tile13_f(
			nvgpu_gr_config_get_map_tile_count(gr_config, 13)) |
		gr_crstr_gpc_map2_tile14_f(
			nvgpu_gr_config_get_map_tile_count(gr_config, 14)) |
		gr_crstr_gpc_map2_tile15_f(
			nvgpu_gr_config_get_map_tile_count(gr_config, 15)) |
		gr_crstr_gpc_map2_tile16_f(
			nvgpu_gr_config_get_map_tile_count(gr_config, 16)) |
		gr_crstr_gpc_map2_tile17_f(
			nvgpu_gr_config_get_map_tile_count(gr_config, 17));

	map3 =  gr_crstr_gpc_map3_tile18_f(
			nvgpu_gr_config_get_map_tile_count(gr_config, 18)) |
		gr_crstr_gpc_map3_tile19_f(
			nvgpu_gr_config_get_map_tile_count(gr_config, 19)) |
		gr_crstr_gpc_map3_tile20_f(
			nvgpu_gr_config_get_map_tile_count(gr_config, 20)) |
		gr_crstr_gpc_map3_tile21_f(
			nvgpu_gr_config_get_map_tile_count(gr_config, 21)) |
		gr_crstr_gpc_map3_tile22_f(
			nvgpu_gr_config_get_map_tile_count(gr_config, 22)) |
		gr_crstr_gpc_map3_tile23_f(
			nvgpu_gr_config_get_map_tile_count(gr_config, 23));

	map4 =  gr_crstr_gpc_map4_tile24_f(
			nvgpu_gr_config_get_map_tile_count(gr_config, 24)) |
		gr_crstr_gpc_map4_tile25_f(
			nvgpu_gr_config_get_map_tile_count(gr_config, 25)) |
		gr_crstr_gpc_map4_tile26_f(
			nvgpu_gr_config_get_map_tile_count(gr_config, 26)) |
		gr_crstr_gpc_map4_tile27_f(
			nvgpu_gr_config_get_map_tile_count(gr_config, 27)) |
		gr_crstr_gpc_map4_tile28_f(
			nvgpu_gr_config_get_map_tile_count(gr_config, 28)) |
		gr_crstr_gpc_map4_tile29_f(
			nvgpu_gr_config_get_map_tile_count(gr_config, 29));

	map5 =  gr_crstr_gpc_map5_tile30_f(
			nvgpu_gr_config_get_map_tile_count(gr_config, 30)) |
		gr_crstr_gpc_map5_tile31_f(
			nvgpu_gr_config_get_map_tile_count(gr_config, 31)) |
		gr_crstr_gpc_map5_tile32_f(0) |
		gr_crstr_gpc_map5_tile33_f(0) |
		gr_crstr_gpc_map5_tile34_f(0) |
		gr_crstr_gpc_map5_tile35_f(0);

	nvgpu_writel(g, gr_crstr_gpc_map0_r(), map0);
	nvgpu_writel(g, gr_crstr_gpc_map1_r(), map1);
	nvgpu_writel(g, gr_crstr_gpc_map2_r(), map2);
	nvgpu_writel(g, gr_crstr_gpc_map3_r(), map3);
	nvgpu_writel(g, gr_crstr_gpc_map4_r(), map4);
	nvgpu_writel(g, gr_crstr_gpc_map5_r(), map5);

	switch (tpc_cnt) {
	case 1:
		norm_shift = 4;
		break;
	case 2:
	case 3:
		norm_shift = 3;
		break;
	case 4:
	case 5:
	case 6:
	case 7:
		norm_shift = 2;
		break;
	case 8:
	case 9:
	case 10:
	case 11:
	case 12:
	case 13:
	case 14:
	case 15:
		norm_shift = 1;
		break;
	default:
		norm_shift = 0;
		break;
	}

	norm_entries = tpc_cnt << norm_shift;
	coeff5_mod = BIT32(5) % norm_entries;
	coeff6_mod = BIT32(6) % norm_entries;
	coeff7_mod = BIT32(7) % norm_entries;
	coeff8_mod = BIT32(8) % norm_entries;
	coeff9_mod = BIT32(9) % norm_entries;
	coeff10_mod = BIT32(10) % norm_entries;
	coeff11_mod = BIT32(11) % norm_entries;

	nvgpu_writel(g, gr_ppcs_wwdx_map_table_cfg_r(),
	 gr_ppcs_wwdx_map_table_cfg_row_offset_f(
	  nvgpu_gr_config_get_map_row_offset(gr_config)) |
	 gr_ppcs_wwdx_map_table_cfg_normalized_num_entries_f(norm_entries) |
	 gr_ppcs_wwdx_map_table_cfg_normalized_shift_value_f(norm_shift) |
	 gr_ppcs_wwdx_map_table_cfg_coeff5_mod_value_f(coeff5_mod) |
	 gr_ppcs_wwdx_map_table_cfg_num_entries_f(tpc_cnt));

	nvgpu_writel(g, gr_ppcs_wwdx_map_table_cfg2_r(),
	 gr_ppcs_wwdx_map_table_cfg2_coeff6_mod_value_f(coeff6_mod) |
	 gr_ppcs_wwdx_map_table_cfg2_coeff7_mod_value_f(coeff7_mod) |
	 gr_ppcs_wwdx_map_table_cfg2_coeff8_mod_value_f(coeff8_mod) |
	 gr_ppcs_wwdx_map_table_cfg2_coeff9_mod_value_f(coeff9_mod) |
	 gr_ppcs_wwdx_map_table_cfg2_coeff10_mod_value_f(coeff10_mod) |
	 gr_ppcs_wwdx_map_table_cfg2_coeff11_mod_value_f(coeff11_mod));

	nvgpu_writel(g, gr_ppcs_wwdx_map_gpc_map0_r(), map0);
	nvgpu_writel(g, gr_ppcs_wwdx_map_gpc_map1_r(), map1);
	nvgpu_writel(g, gr_ppcs_wwdx_map_gpc_map2_r(), map2);
	nvgpu_writel(g, gr_ppcs_wwdx_map_gpc_map3_r(), map3);
	nvgpu_writel(g, gr_ppcs_wwdx_map_gpc_map4_r(), map4);
	nvgpu_writel(g, gr_ppcs_wwdx_map_gpc_map5_r(), map5);

	nvgpu_writel(g, gr_rstr2d_map_table_cfg_r(),
		gr_rstr2d_map_table_cfg_row_offset_f(
		 nvgpu_gr_config_get_map_row_offset(gr_config)) |
		 gr_rstr2d_map_table_cfg_num_entries_f(tpc_cnt));

	nvgpu_writel(g, gr_rstr2d_gpc_map0_r(), map0);
	nvgpu_writel(g, gr_rstr2d_gpc_map1_r(), map1);
	nvgpu_writel(g, gr_rstr2d_gpc_map2_r(), map2);
	nvgpu_writel(g, gr_rstr2d_gpc_map3_r(), map3);
	nvgpu_writel(g, gr_rstr2d_gpc_map4_r(), map4);
	nvgpu_writel(g, gr_rstr2d_gpc_map5_r(), map5);
}
#endif

void gm20b_gr_init_load_tpc_mask(struct gk20a *g,
			struct nvgpu_gr_config *config)
{
	u32 pes_tpc_mask = 0;
	u32 gpc, pes;
	u32 num_tpc_per_gpc = nvgpu_get_litter_value(g,
				GPU_LIT_NUM_TPC_PER_GPC);
#ifdef CONFIG_NVGPU_NON_FUSA
	u32 max_tpc_count = nvgpu_gr_config_get_max_tpc_count(config);
	u32 fuse_tpc_mask;
	u32 val;
	u32 cur_gr_instance = nvgpu_gr_get_cur_instance_id(g);
	u32 gpc_phys_id;
#endif
	/* gv11b has 1 GPC and 4 TPC/GPC, so mask will not overflow u32 */
	for (gpc = 0; gpc < nvgpu_gr_config_get_gpc_count(config); gpc++) {
		for (pes = 0;
		     pes < nvgpu_gr_config_get_pe_count_per_gpc(config);
		     pes++) {
			pes_tpc_mask |= nvgpu_gr_config_get_pes_tpc_mask(
					config, gpc, pes) <<
					nvgpu_safe_mult_u32(num_tpc_per_gpc, gpc);
		}
	}

	nvgpu_log_info(g, "pes_tpc_mask %u\n", pes_tpc_mask);

#ifdef CONFIG_NVGPU_NON_FUSA
	if (!nvgpu_is_enabled(g, NVGPU_SUPPORT_MIG)) {
		/*
		 * Fuse registers must be queried with physical gpc-id and not
		 * the logical ones. For tu104 and before chips logical gpc-id
		 * is same as physical gpc-id for non-floorswept config but for
		 * chips after tu104 it may not be true.
		 */
		gpc_phys_id = nvgpu_grmgr_get_gr_gpc_phys_id(g,
				cur_gr_instance, 0U);
		fuse_tpc_mask = g->ops.gr.config.get_gpc_tpc_mask(g, config, gpc_phys_id);
		if ((g->tpc_fs_mask_user != 0U) &&
					(g->tpc_fs_mask_user != fuse_tpc_mask)) {
			if (fuse_tpc_mask == nvgpu_safe_sub_u32(BIT32(max_tpc_count),
									U32(1))) {
				val = g->tpc_fs_mask_user;
				val &= nvgpu_safe_sub_u32(BIT32(max_tpc_count), U32(1));
				/*
				 * skip tpc to disable the other tpc cause channel
				 * timeout
				 */
				val = nvgpu_safe_sub_u32(BIT32(hweight32(val)), U32(1));
				pes_tpc_mask = val;
			}
		}
	}
#endif

	g->ops.gr.init.tpc_mask(g, 0, pes_tpc_mask);

}

void gm20b_gr_init_fs_state(struct gk20a *g)
{
	nvgpu_log_fn(g, " ");

	nvgpu_writel(g, gr_bes_zrop_settings_r(),
		     gr_bes_zrop_settings_num_active_ltcs_f(
			nvgpu_ltc_get_ltc_count(g)));
	nvgpu_writel(g, gr_bes_crop_settings_r(),
		     gr_bes_crop_settings_num_active_ltcs_f(
			nvgpu_ltc_get_ltc_count(g)));

	nvgpu_writel(g, gr_bes_crop_debug3_r(),
		     gk20a_readl(g, gr_be0_crop_debug3_r()) |
		     gr_bes_crop_debug3_comp_vdc_4to2_disable_m());
}

void gm20b_gr_init_commit_global_timeslice(struct gk20a *g)
{
	u32 gpm_pd_cfg;
	u32 pd_ab_dist_cfg0;
	u32 ds_debug;
	u32 mpc_vtg_debug;
	u32 pe_vaf;
	u32 pe_vsc_vpc;

	nvgpu_log_fn(g, " ");

	gpm_pd_cfg = nvgpu_readl(g, gr_gpcs_gpm_pd_cfg_r());
	pd_ab_dist_cfg0 = nvgpu_readl(g, gr_pd_ab_dist_cfg0_r());
	ds_debug = nvgpu_readl(g, gr_ds_debug_r());
	mpc_vtg_debug = nvgpu_readl(g, gr_gpcs_tpcs_mpc_vtg_debug_r());

	pe_vaf = nvgpu_readl(g, gr_gpcs_tpcs_pe_vaf_r());
	pe_vsc_vpc = nvgpu_readl(g, gr_gpcs_tpcs_pes_vsc_vpc_r());

	gpm_pd_cfg = gr_gpcs_gpm_pd_cfg_timeslice_mode_enable_f() | gpm_pd_cfg;
	pe_vaf = gr_gpcs_tpcs_pe_vaf_fast_mode_switch_true_f() | pe_vaf;
	pe_vsc_vpc = gr_gpcs_tpcs_pes_vsc_vpc_fast_mode_switch_true_f() |
		     pe_vsc_vpc;
	pd_ab_dist_cfg0 = gr_pd_ab_dist_cfg0_timeslice_enable_en_f() |
			  pd_ab_dist_cfg0;
	ds_debug = gr_ds_debug_timeslice_mode_enable_f() | ds_debug;
	mpc_vtg_debug = gr_gpcs_tpcs_mpc_vtg_debug_timeslice_mode_enabled_f() |
			mpc_vtg_debug;

	nvgpu_gr_ctx_patch_write(g, NULL, gr_gpcs_gpm_pd_cfg_r(), gpm_pd_cfg,
		false);
	nvgpu_gr_ctx_patch_write(g, NULL, gr_gpcs_tpcs_pe_vaf_r(), pe_vaf,
		false);
	nvgpu_gr_ctx_patch_write(g, NULL, gr_gpcs_tpcs_pes_vsc_vpc_r(),
		pe_vsc_vpc, false);
	nvgpu_gr_ctx_patch_write(g, NULL, gr_pd_ab_dist_cfg0_r(),
		pd_ab_dist_cfg0, false);
	nvgpu_gr_ctx_patch_write(g, NULL, gr_gpcs_tpcs_mpc_vtg_debug_r(),
		mpc_vtg_debug, false);
	nvgpu_gr_ctx_patch_write(g, NULL, gr_ds_debug_r(), ds_debug, false);
}

u32 gm20b_gr_init_get_bundle_cb_default_size(struct gk20a *g)
{
	(void)g;
	return gr_scc_bundle_cb_size_div_256b__prod_v();
}

u32 gm20b_gr_init_get_min_gpm_fifo_depth(struct gk20a *g)
{
	(void)g;
	return gr_pd_ab_dist_cfg2_state_limit_min_gpm_fifo_depths_v();
}

u32 gm20b_gr_init_get_bundle_cb_token_limit(struct gk20a *g)
{
	(void)g;
	return gr_pd_ab_dist_cfg2_token_limit_init_v();
}

u32 gm20b_gr_init_get_attrib_cb_default_size(struct gk20a *g)
{
	(void)g;
	return gr_gpc0_ppc0_cbm_beta_cb_size_v_default_v();
}

u32 gm20b_gr_init_get_alpha_cb_default_size(struct gk20a *g)
{
	(void)g;
	return gr_gpc0_ppc0_cbm_alpha_cb_size_v_default_v();
}

u32 gm20b_gr_init_get_attrib_cb_size(struct gk20a *g, u32 tpc_count)
{
	(void)tpc_count;
	return nvgpu_safe_add_u32(
		g->ops.gr.init.get_attrib_cb_default_size(g),
		(g->ops.gr.init.get_attrib_cb_default_size(g) >> 1));
}

u32 gm20b_gr_init_get_alpha_cb_size(struct gk20a *g, u32 tpc_count)
{
	(void)tpc_count;
	return nvgpu_safe_add_u32(
		g->ops.gr.init.get_alpha_cb_default_size(g),
		(g->ops.gr.init.get_alpha_cb_default_size(g) >> 1));
}

u32 gm20b_gr_init_get_global_attr_cb_size(struct gk20a *g, u32 tpc_count,
	u32 max_tpc)
{
	u32 size;

	size = nvgpu_safe_mult_u32(
		g->ops.gr.init.get_attrib_cb_size(g, tpc_count),
		nvgpu_safe_mult_u32(
			gr_gpc0_ppc0_cbm_beta_cb_size_v_granularity_v(),
			max_tpc));

	size = nvgpu_safe_add_u32(size, nvgpu_safe_mult_u32(
		g->ops.gr.init.get_alpha_cb_size(g, tpc_count),
		nvgpu_safe_mult_u32(
			gr_gpc0_ppc0_cbm_alpha_cb_size_v_granularity_v(),
			max_tpc)));

	return size;
}

void gm20b_gr_init_commit_global_bundle_cb(struct gk20a *g,
	struct nvgpu_gr_ctx *gr_ctx, u64 addr, u32 size, bool patch)
{
	u32 data;
	u32 cb_addr;
	u32 bundle_cb_token_limit = g->ops.gr.init.get_bundle_cb_token_limit(g);

	addr = addr >> gr_scc_bundle_cb_base_addr_39_8_align_bits_v();

	nvgpu_log_info(g, "bundle cb addr : 0x%016llx, size : %u",
		addr, size);
	nvgpu_assert(u64_hi32(addr) == 0U);

	cb_addr = (u32)addr;
	nvgpu_gr_ctx_patch_write(g, gr_ctx, gr_scc_bundle_cb_base_r(),
		gr_scc_bundle_cb_base_addr_39_8_f(cb_addr), patch);

	nvgpu_gr_ctx_patch_write(g, gr_ctx, gr_scc_bundle_cb_size_r(),
		gr_scc_bundle_cb_size_div_256b_f(size) |
		gr_scc_bundle_cb_size_valid_true_f(), patch);

	nvgpu_gr_ctx_patch_write(g, gr_ctx, gr_gpcs_swdx_bundle_cb_base_r(),
		gr_gpcs_swdx_bundle_cb_base_addr_39_8_f(cb_addr), patch);

	nvgpu_gr_ctx_patch_write(g, gr_ctx, gr_gpcs_swdx_bundle_cb_size_r(),
		gr_gpcs_swdx_bundle_cb_size_div_256b_f(size) |
		gr_gpcs_swdx_bundle_cb_size_valid_true_f(), patch);

	/* data for state_limit */
	data = nvgpu_safe_mult_u32(
		g->ops.gr.init.get_bundle_cb_default_size(g),
		gr_scc_bundle_cb_size_div_256b_byte_granularity_v()) /
		gr_pd_ab_dist_cfg2_state_limit_scc_bundle_granularity_v();

	data = min_t(u32, data, g->ops.gr.init.get_min_gpm_fifo_depth(g));

	nvgpu_log_info(g, "bundle cb token limit : %d, state limit : %d",
		bundle_cb_token_limit, data);

	nvgpu_gr_ctx_patch_write(g, gr_ctx, gr_pd_ab_dist_cfg2_r(),
		gr_pd_ab_dist_cfg2_token_limit_f(bundle_cb_token_limit) |
		gr_pd_ab_dist_cfg2_state_limit_f(data), patch);
}

u32 gm20b_gr_init_pagepool_default_size(struct gk20a *g)
{
	(void)g;
	return gr_scc_pagepool_total_pages_hwmax_value_v();
}

void gm20b_gr_init_commit_global_pagepool(struct gk20a *g,
	struct nvgpu_gr_ctx *gr_ctx, u64 addr, size_t size, bool patch,
	bool global_ctx)
{
	u32 pp_addr;
	u32 pp_size;

	addr = addr >> gr_scc_pagepool_base_addr_39_8_align_bits_v();

	if (global_ctx) {
		size = size / gr_scc_pagepool_total_pages_byte_granularity_v();
	}

	if (size == g->ops.gr.init.pagepool_default_size(g)) {
		size = gr_scc_pagepool_total_pages_hwmax_v();
	}

	nvgpu_assert(u64_hi32(addr) == 0U);
	nvgpu_log_info(g, "pagepool buffer addr : 0x%016llx, size : %lu",
		addr, size);

	pp_addr = (u32)addr;
	pp_size = nvgpu_safe_cast_u64_to_u32(size);
	nvgpu_gr_ctx_patch_write(g, gr_ctx, gr_scc_pagepool_base_r(),
		gr_scc_pagepool_base_addr_39_8_f(pp_addr), patch);

	nvgpu_gr_ctx_patch_write(g, gr_ctx, gr_scc_pagepool_r(),
		gr_scc_pagepool_total_pages_f(pp_size) |
		gr_scc_pagepool_valid_true_f(), patch);

	nvgpu_gr_ctx_patch_write(g, gr_ctx, gr_gpcs_gcc_pagepool_base_r(),
		gr_gpcs_gcc_pagepool_base_addr_39_8_f(pp_addr), patch);

	nvgpu_gr_ctx_patch_write(g, gr_ctx, gr_gpcs_gcc_pagepool_r(),
		gr_gpcs_gcc_pagepool_total_pages_f(pp_size), patch);

	nvgpu_gr_ctx_patch_write(g, gr_ctx, gr_pd_pagepool_r(),
		gr_pd_pagepool_total_pages_f(pp_size) |
		gr_pd_pagepool_valid_true_f(), patch);

	nvgpu_gr_ctx_patch_write(g, gr_ctx, gr_gpcs_swdx_rm_pagepool_r(),
		gr_gpcs_swdx_rm_pagepool_total_pages_f(pp_size) |
		gr_gpcs_swdx_rm_pagepool_valid_true_f(), patch);
}

void gm20b_gr_init_commit_global_cb_manager(struct gk20a *g,
	struct nvgpu_gr_config *config, struct nvgpu_gr_ctx *gr_ctx, bool patch)
{
	u32 attrib_offset_in_chunk = 0;
	u32 alpha_offset_in_chunk = 0;
	u32 pd_ab_max_output;
	u32 gpc_index, ppc_index;
	u32 cbm_cfg_size1, cbm_cfg_size2;
	u32 attrib_cb_default_size = g->ops.gr.init.get_attrib_cb_default_size(g);
	u32 alpha_cb_default_size = g->ops.gr.init.get_alpha_cb_default_size(g);
	u32 attrib_cb_size = g->ops.gr.init.get_attrib_cb_size(g,
		nvgpu_gr_config_get_tpc_count(config));
	u32 alpha_cb_size = g->ops.gr.init.get_alpha_cb_size(g,
		nvgpu_gr_config_get_tpc_count(config));
	u32 gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_GPC_STRIDE);
	u32 ppc_in_gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_PPC_IN_GPC_STRIDE);
	u32 num_pes_per_gpc = nvgpu_get_litter_value(g,
			GPU_LIT_NUM_PES_PER_GPC);

	nvgpu_log_fn(g, " ");

	nvgpu_gr_ctx_patch_write(g, gr_ctx, gr_ds_tga_constraintlogic_r(),
		gr_ds_tga_constraintlogic_beta_cbsize_f(attrib_cb_default_size) |
		gr_ds_tga_constraintlogic_alpha_cbsize_f(alpha_cb_default_size),
		patch);

	pd_ab_max_output = nvgpu_safe_mult_u32(alpha_cb_default_size,
		gr_gpc0_ppc0_cbm_beta_cb_size_v_granularity_v()) /
		gr_pd_ab_dist_cfg1_max_output_granularity_v();

	nvgpu_gr_ctx_patch_write(g, gr_ctx, gr_pd_ab_dist_cfg1_r(),
		gr_pd_ab_dist_cfg1_max_output_f(pd_ab_max_output) |
		gr_pd_ab_dist_cfg1_max_batches_init_f(), patch);

	alpha_offset_in_chunk =
		nvgpu_safe_add_u32(attrib_offset_in_chunk,
			nvgpu_safe_mult_u32(
				nvgpu_gr_config_get_tpc_count(config),
				attrib_cb_size));

	for (gpc_index = 0;
	     gpc_index < nvgpu_gr_config_get_gpc_count(config);
	     gpc_index++) {
		u32 temp = nvgpu_safe_mult_u32(gpc_stride, gpc_index);
		u32 temp2 = nvgpu_safe_mult_u32(num_pes_per_gpc, gpc_index);
		for (ppc_index = 0;
		     ppc_index < nvgpu_gr_config_get_gpc_ppc_count(config,
		     gpc_index);
		     ppc_index++) {
			u32 pes_tpc_count =
				nvgpu_gr_config_get_pes_tpc_count(config,
					gpc_index, ppc_index);
			u32 ppc_posn = nvgpu_safe_mult_u32(ppc_in_gpc_stride,
							ppc_index);
			u32 sum_temp_pcc = nvgpu_safe_add_u32(temp, ppc_posn);

			cbm_cfg_size1 =
				nvgpu_safe_mult_u32(attrib_cb_default_size,
							pes_tpc_count);
			cbm_cfg_size2 =
				nvgpu_safe_mult_u32(alpha_cb_default_size,
							pes_tpc_count);

			nvgpu_gr_ctx_patch_write(g, gr_ctx,
				nvgpu_safe_add_u32(
					gr_gpc0_ppc0_cbm_beta_cb_size_r(),
					sum_temp_pcc),
				cbm_cfg_size1, patch);

			nvgpu_gr_ctx_patch_write(g, gr_ctx,
				nvgpu_safe_add_u32(
					gr_gpc0_ppc0_cbm_beta_cb_offset_r(),
					sum_temp_pcc),
				attrib_offset_in_chunk, patch);

			attrib_offset_in_chunk = nvgpu_safe_add_u32(
				attrib_offset_in_chunk,
				nvgpu_safe_mult_u32(attrib_cb_size,
						pes_tpc_count));

			nvgpu_gr_ctx_patch_write(g, gr_ctx,
				nvgpu_safe_add_u32(
					gr_gpc0_ppc0_cbm_alpha_cb_size_r(),
					sum_temp_pcc),
				cbm_cfg_size2, patch);

			nvgpu_gr_ctx_patch_write(g, gr_ctx,
				nvgpu_safe_add_u32(
					gr_gpc0_ppc0_cbm_alpha_cb_offset_r(),
					sum_temp_pcc),
				alpha_offset_in_chunk, patch);

			alpha_offset_in_chunk = nvgpu_safe_add_u32(
				alpha_offset_in_chunk,
				nvgpu_safe_mult_u32(alpha_cb_size,
						pes_tpc_count));

			nvgpu_gr_ctx_patch_write(g, gr_ctx,
				gr_gpcs_swdx_tc_beta_cb_size_r(
					nvgpu_safe_add_u32(ppc_index, temp2)),
				gr_gpcs_swdx_tc_beta_cb_size_v_f(cbm_cfg_size1) |
				gr_gpcs_swdx_tc_beta_cb_size_div3_f(cbm_cfg_size1/3U),
				patch);
		}
	}
}

void gm20b_gr_init_detect_sm_arch(struct gk20a *g)
{
	u32 v = gk20a_readl(g, gr_gpc0_tpc0_sm_arch_r());

	g->params.sm_arch_spa_version =
		gr_gpc0_tpc0_sm_arch_spa_version_v(v);
	g->params.sm_arch_sm_version =
		gr_gpc0_tpc0_sm_arch_sm_version_v(v);
	g->params.sm_arch_warp_count =
		gr_gpc0_tpc0_sm_arch_warp_count_v(v);
}

void gm20b_gr_init_get_supported_preemption_modes(
	u32 *graphics_preemption_mode_flags, u32 *compute_preemption_mode_flags)
{
	*graphics_preemption_mode_flags = NVGPU_PREEMPTION_MODE_GRAPHICS_WFI;
	*compute_preemption_mode_flags = (NVGPU_PREEMPTION_MODE_COMPUTE_WFI |
					 NVGPU_PREEMPTION_MODE_COMPUTE_CTA);
}

void gm20b_gr_init_get_default_preemption_modes(
	u32 *default_graphics_preempt_mode, u32 *default_compute_preempt_mode)
{
	*default_graphics_preempt_mode = NVGPU_PREEMPTION_MODE_GRAPHICS_WFI;
	*default_compute_preempt_mode = NVGPU_PREEMPTION_MODE_COMPUTE_CTA;
}

void gm20b_gr_init_fe_go_idle_timeout(struct gk20a *g, bool enable)
{
	if (enable) {
		nvgpu_writel(g, gr_fe_go_idle_timeout_r(),
			gr_fe_go_idle_timeout_count_prod_f());
	} else {
		nvgpu_writel(g, gr_fe_go_idle_timeout_r(),
			gr_fe_go_idle_timeout_count_disabled_f());
	}
}
