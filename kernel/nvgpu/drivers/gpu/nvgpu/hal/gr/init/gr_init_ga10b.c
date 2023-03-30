/*
 * Copyright (c) 2020-2022, NVIDIA CORPORATION.  All rights reserved.
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
#include <nvgpu/soc.h>
#include <nvgpu/log.h>
#include <nvgpu/bug.h>
#include <nvgpu/gr/ctx.h>
#include <nvgpu/enabled.h>
#include <nvgpu/static_analysis.h>

#include "gr_init_ga10b.h"

#include <nvgpu/hw/ga10b/hw_gr_ga10b.h>

#ifdef CONFIG_NVGPU_GRAPHICS
u32 ga10b_gr_init_get_attrib_cb_gfxp_default_size(struct gk20a *g)
{
	(void)g;
	return gr_gpc0_ppc0_cbm_beta_cb_size_v_gfxp_v();
}

u32 ga10b_gr_init_get_attrib_cb_gfxp_size(struct gk20a *g)
{
	(void)g;
	return gr_gpc0_ppc0_cbm_beta_cb_size_v_gfxp_v();
}

u32 ga10b_gr_init_get_ctx_spill_size(struct gk20a *g)
{
	(void)g;
	return  nvgpu_safe_mult_u32(
		  gr_gpc0_swdx_rm_spill_buffer_size_256b_default_v(),
		  gr_gpc0_swdx_rm_spill_buffer_size_256b_byte_granularity_v());
}

u32 ga10b_gr_init_get_ctx_betacb_size(struct gk20a *g)
{
	return nvgpu_safe_add_u32(
		g->ops.gr.init.get_attrib_cb_default_size(g),
		nvgpu_safe_sub_u32(
			gr_gpc0_ppc0_cbm_beta_cb_size_v_gfxp_v(),
			gr_gpc0_ppc0_cbm_beta_cb_size_v_default_v()));
}

void ga10b_gr_init_commit_rops_crop_override(struct gk20a *g,
				struct nvgpu_gr_ctx *gr_ctx, bool patch)
{
	if (nvgpu_is_enabled(g, NVGPU_SUPPORT_EMULATE_MODE) &&
			(g->emulate_mode > 0U)) {
		u32 data = 0U;
		data = nvgpu_readl(g, gr_pri_gpcs_rops_crop_debug1_r());
		data = set_field(data,
			gr_pri_gpcs_rops_crop_debug1_crd_cond_read_m(),
			gr_pri_gpcs_rops_crop_debug1_crd_cond_read_disable_f());
		nvgpu_gr_ctx_patch_write(g, gr_ctx,
			gr_pri_gpcs_rops_crop_debug1_r(), data, patch);
	}
}
#endif

#ifdef CONFIG_NVGPU_SET_FALCON_ACCESS_MAP
void ga10b_gr_init_get_access_map(struct gk20a *g,
				   u32 **whitelist, u32 *num_entries)
{
	static u32 wl_addr_ga10b[] = {
		/* this list must be sorted (low to high) */
		0x418380, /* gr_pri_gpcs_rasterarb_line_class  */
		0x418800, /* gr_pri_gpcs_setup_debug           */
		0x418830, /* gr_pri_gpcs_setup_debug_z_gamut_offset */
		0x4188fc, /* gr_pri_gpcs_zcull_ctx_debug       */
		0x418e00, /* gr_pri_gpcs_swdx_config           */
		0x418e40, /* gr_pri_gpcs_swdx_tc_bundle_ctrl   */
		0x418e44, /* gr_pri_gpcs_swdx_tc_bundle_ctrl   */
		0x418e48, /* gr_pri_gpcs_swdx_tc_bundle_ctrl   */
		0x418e4c, /* gr_pri_gpcs_swdx_tc_bundle_ctrl   */
		0x418e50, /* gr_pri_gpcs_swdx_tc_bundle_laztval_ctrl	*/
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
		0x419ba4, /* gr_pri_gpcs_tpcs_sm_disp_ctrl     */
		0x419e84, /* gr_pri_gpcs_tpcs_sms_dbgr_control0 */
		0x419ea8, /* gr_pri_gpcs_tpcs_sms_hww_warp_esr_report_mask */
	};
	size_t array_size;

	(void)g;
	*whitelist = wl_addr_ga10b;
	array_size = ARRAY_SIZE(wl_addr_ga10b);
	*num_entries = nvgpu_safe_cast_u64_to_u32(array_size);
}
#endif
