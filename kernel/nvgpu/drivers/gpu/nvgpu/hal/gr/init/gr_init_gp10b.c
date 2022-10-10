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
#include <nvgpu/gr/ctx.h>
#include <nvgpu/gr/config.h>
#include <nvgpu/gr/gr.h>
#include <nvgpu/gr/gr_utils.h>

#include "gr_init_gm20b.h"
#include "gr_init_gp10b.h"

#include <nvgpu/hw/gp10b/hw_gr_gp10b.h>

#define GFXP_WFI_TIMEOUT_COUNT_DEFAULT 100000U

#ifdef CONFIG_NVGPU_SET_FALCON_ACCESS_MAP
void gp10b_gr_init_get_access_map(struct gk20a *g,
				   u32 **whitelist, u32 *num_entries)
{
	static u32 wl_addr_gp10b[] = {
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
	*whitelist = wl_addr_gp10b;
	array_size = ARRAY_SIZE(wl_addr_gp10b);
	*num_entries = nvgpu_safe_cast_u64_to_u32(array_size);
}
#endif

int gp10b_gr_init_sm_id_config(struct gk20a *g, u32 *tpc_sm_id,
				struct nvgpu_gr_config *gr_config,
				struct nvgpu_gr_ctx *gr_ctx,
				bool patch)
{
	u32 i, j;
	u32 tpc_index, gpc_index;
	u32 max_gpcs = nvgpu_get_litter_value(g, GPU_LIT_NUM_GPCS);
	u32 tpc_cnt = nvgpu_safe_sub_u32(
				nvgpu_gr_config_get_tpc_count(gr_config), 1U);

	(void)gr_ctx;
	(void)patch;

	/* Each NV_PGRAPH_PRI_CWD_GPC_TPC_ID can store 4 TPCs.*/
	for (i = 0U; i <= (tpc_cnt / 4U); i++) {
		u32 reg = 0;
		u32 bit_stride = nvgpu_safe_add_u32(
					gr_cwd_gpc_tpc_id_gpc0_s(),
					 gr_cwd_gpc_tpc_id_tpc0_s());

		for (j = 0U; j < 4U; j++) {
			u32 sm_id = nvgpu_safe_mult_u32(i, 4U) + j;
			u32 bits;
			u32 index = 0U;
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

			index = nvgpu_safe_mult_u32(max_gpcs,
						((tpc_index & 4U) >> 2U));
			index = nvgpu_safe_add_u32(gpc_index, index);
			tpc_sm_id[index]
				|= (sm_id <<
					nvgpu_safe_mult_u32(bit_stride,
							(tpc_index & 3U)));
		}
		nvgpu_writel(g, gr_cwd_gpc_tpc_id_r(i), reg);
	}

	for (i = 0; i < gr_cwd_sm_id__size_1_v(); i++) {
		nvgpu_writel(g, gr_cwd_sm_id_r(i), tpc_sm_id[i]);
	}

	return 0;
}

void gp10b_gr_init_fs_state(struct gk20a *g)
{
	u32 data;
	u32 ecc_val = nvgpu_gr_get_override_ecc_val(g);

	nvgpu_log_fn(g, " ");

	data = nvgpu_readl(g, gr_gpcs_tpcs_sm_texio_control_r());
	data = set_field(data,
		gr_gpcs_tpcs_sm_texio_control_oor_addr_check_mode_m(),
		gr_gpcs_tpcs_sm_texio_control_oor_addr_check_mode_arm_63_48_match_f());
	nvgpu_writel(g, gr_gpcs_tpcs_sm_texio_control_r(), data);

	data = nvgpu_readl(g, gr_gpcs_tpcs_sm_disp_ctrl_r());
	data = set_field(data, gr_gpcs_tpcs_sm_disp_ctrl_re_suppress_m(),
			 gr_gpcs_tpcs_sm_disp_ctrl_re_suppress_disable_f());
	nvgpu_writel(g, gr_gpcs_tpcs_sm_disp_ctrl_r(), data);

	if (ecc_val != 0U) {
		nvgpu_writel(g, gr_fecs_feature_override_ecc_r(), ecc_val);
	}

	gm20b_gr_init_fs_state(g);
}

int gp10b_gr_init_preemption_state(struct gk20a *g)
{
	u32 debug_2;
	u32 gfxp_wfi_timeout_count = GFXP_WFI_TIMEOUT_COUNT_DEFAULT;

	nvgpu_writel(g, gr_fe_gfxp_wfi_timeout_r(),
			gr_fe_gfxp_wfi_timeout_count_f(gfxp_wfi_timeout_count));

	debug_2 = nvgpu_readl(g, gr_debug_2_r());
	debug_2 = set_field(debug_2,
			gr_debug_2_gfxp_wfi_always_injects_wfi_m(),
			gr_debug_2_gfxp_wfi_always_injects_wfi_enabled_f());
	nvgpu_writel(g, gr_debug_2_r(), debug_2);

	return 0;
}

#define GP10B_CBM_BETA_CB_NO_DEEP_TILING_SIZE_DEFAULT	0x800
/*
 * Ideally, gr_gpc0_ppc0_cbm_beta_cb_size_v_default_v() gives default beta cb
 * size.
 * To enable deep tiling this size value was increased to accommodate additional
 * state information to be preserved through the pipeline. But, deep tiling is
 * not used and hw manuals are not updated. Use hw recommended beta cb default
 * size hardcoded value (same as gm20b).
 */
u32 gp10b_gr_init_get_attrib_cb_default_size(struct gk20a *g)
{
	(void)g;
	return GP10B_CBM_BETA_CB_NO_DEEP_TILING_SIZE_DEFAULT;
}

u32 gp10b_gr_init_get_alpha_cb_default_size(struct gk20a *g)
{
	(void)g;
	return gr_gpc0_ppc0_cbm_alpha_cb_size_v_default_v();
}

u32 gp10b_gr_init_get_attrib_cb_size(struct gk20a *g, u32 tpc_count)
{
	return min(g->ops.gr.init.get_attrib_cb_default_size(g),
		 gr_gpc0_ppc0_cbm_beta_cb_size_v_f(~U32(0U)) / tpc_count);
}

u32 gp10b_gr_init_get_alpha_cb_size(struct gk20a *g, u32 tpc_count)
{
	return min(g->ops.gr.init.get_alpha_cb_default_size(g),
		 gr_gpc0_ppc0_cbm_alpha_cb_size_v_f(~U32(0U)) / tpc_count);
}

u32 gp10b_gr_init_get_global_attr_cb_size(struct gk20a *g, u32 tpc_count,
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

	size = NVGPU_ALIGN(size, 128U);

	return size;
}

void gp10b_gr_init_commit_global_attrib_cb(struct gk20a *g,
	struct nvgpu_gr_ctx *gr_ctx, u32 tpc_count, u32 max_tpc, u64 addr,
	bool patch)
{
	u32 attrBufferSize;
	u32 cb_addr;

	gm20b_gr_init_commit_global_attrib_cb(g, gr_ctx, tpc_count, max_tpc,
		addr, patch);

	addr = addr >> gr_gpcs_setup_attrib_cb_base_addr_39_12_align_bits_v();

#ifdef CONFIG_NVGPU_GFXP
	if (nvgpu_gr_ctx_get_preempt_ctxsw_buffer(gr_ctx)->gpu_va != 0ULL) {
		attrBufferSize = nvgpu_safe_cast_u64_to_u32(
			nvgpu_gr_ctx_get_betacb_ctxsw_buffer(gr_ctx)->size);
	} else {
#endif
		attrBufferSize = g->ops.gr.init.get_global_attr_cb_size(g,
			tpc_count, max_tpc);
#ifdef CONFIG_NVGPU_GFXP
	}
#endif

	attrBufferSize /= gr_gpcs_tpcs_tex_rm_cb_1_size_div_128b_granularity_f();

	nvgpu_assert(u64_hi32(addr) == 0U);

	cb_addr = (u32)addr;
	nvgpu_gr_ctx_patch_write(g, gr_ctx, gr_gpcs_tpcs_mpc_vtg_cb_global_base_addr_r(),
		gr_gpcs_tpcs_mpc_vtg_cb_global_base_addr_v_f(cb_addr) |
		gr_gpcs_tpcs_mpc_vtg_cb_global_base_addr_valid_true_f(), patch);

	nvgpu_gr_ctx_patch_write(g, gr_ctx, gr_gpcs_tpcs_tex_rm_cb_0_r(),
		gr_gpcs_tpcs_tex_rm_cb_0_base_addr_43_12_f(cb_addr), patch);

	nvgpu_gr_ctx_patch_write(g, gr_ctx, gr_gpcs_tpcs_tex_rm_cb_1_r(),
		gr_gpcs_tpcs_tex_rm_cb_1_size_div_128b_f(attrBufferSize) |
		gr_gpcs_tpcs_tex_rm_cb_1_valid_true_f(), patch);
}

void gp10b_gr_init_commit_cbes_reserve(struct gk20a *g,
	struct nvgpu_gr_ctx *gr_ctx, bool patch)
{
	u32 cbes_reserve = gr_gpcs_swdx_beta_cb_ctrl_cbes_reserve_gfxp_v();

	nvgpu_gr_ctx_patch_write(g, gr_ctx,
		gr_gpcs_swdx_beta_cb_ctrl_r(),
		gr_gpcs_swdx_beta_cb_ctrl_cbes_reserve_f(cbes_reserve),
		patch);
	nvgpu_gr_ctx_patch_write(g, gr_ctx,
		gr_gpcs_ppcs_cbm_beta_cb_ctrl_r(),
		gr_gpcs_ppcs_cbm_beta_cb_ctrl_cbes_reserve_f(cbes_reserve),
		patch);
}

u32 gp10b_gr_init_get_attrib_cb_gfxp_default_size(struct gk20a *g)
{
	return nvgpu_safe_add_u32(
			g->ops.gr.init.get_attrib_cb_default_size(g),
			nvgpu_safe_sub_u32(
				gr_gpc0_ppc0_cbm_beta_cb_size_v_gfxp_v(),
				gr_gpc0_ppc0_cbm_beta_cb_size_v_default_v()));
}

u32 gp10b_gr_init_get_attrib_cb_gfxp_size(struct gk20a *g)
{
	return nvgpu_safe_add_u32(
			g->ops.gr.init.get_attrib_cb_default_size(g),
			nvgpu_safe_sub_u32(
				gr_gpc0_ppc0_cbm_beta_cb_size_v_gfxp_v(),
				gr_gpc0_ppc0_cbm_beta_cb_size_v_default_v()));
}

u32 gp10b_gr_init_get_ctx_spill_size(struct gk20a *g)
{
	(void)g;
	return  nvgpu_safe_mult_u32(
		 gr_gpc0_swdx_rm_spill_buffer_size_256b_default_v(),
		 gr_gpc0_swdx_rm_spill_buffer_size_256b_byte_granularity_v());
}

u32 gp10b_gr_init_get_ctx_betacb_size(struct gk20a *g)
{
	return nvgpu_safe_add_u32(
		g->ops.gr.init.get_attrib_cb_default_size(g),
		nvgpu_safe_sub_u32(
			gr_gpc0_ppc0_cbm_beta_cb_size_v_gfxp_v(),
			gr_gpc0_ppc0_cbm_beta_cb_size_v_default_v()));
}

void gp10b_gr_init_commit_ctxsw_spill(struct gk20a *g,
	struct nvgpu_gr_ctx *gr_ctx, u64 addr, u32 size, bool patch)
{
	addr = addr >> gr_gpc0_swdx_rm_spill_buffer_addr_39_8_align_bits_v();

	size /= gr_gpc0_swdx_rm_spill_buffer_size_256b_byte_granularity_v();

	nvgpu_assert(u64_hi32(addr) == 0U);
	nvgpu_gr_ctx_patch_write(g, gr_ctx,
			gr_gpc0_swdx_rm_spill_buffer_addr_r(),
			gr_gpc0_swdx_rm_spill_buffer_addr_39_8_f((u32)addr),
			patch);
	nvgpu_gr_ctx_patch_write(g, gr_ctx,
			gr_gpc0_swdx_rm_spill_buffer_size_r(),
			gr_gpc0_swdx_rm_spill_buffer_size_256b_f(size),
			patch);
}
