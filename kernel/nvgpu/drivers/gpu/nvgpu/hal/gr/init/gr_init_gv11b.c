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
#include <nvgpu/soc.h>
#include <nvgpu/log.h>
#include <nvgpu/bug.h>
#include <nvgpu/static_analysis.h>
#include <nvgpu/gr/ctx.h>
#include <nvgpu/ltc.h>
#include <nvgpu/netlist.h>

#include <nvgpu/gr/config.h>
#include <nvgpu/gr/gr_utils.h>

#include "gr_init_gm20b.h"
#include "gr_init_gv11b.h"

#include <nvgpu/hw/gv11b/hw_gr_gv11b.h>

/* ecc scrubbing will done in 1 pri read cycle,but for safety used 10 retries */
#define GR_ECC_SCRUBBING_TIMEOUT_MAX_US 1000U
#define GR_ECC_SCRUBBING_TIMEOUT_DEFAULT_US 10U

/*
 * Each gpc can have maximum 32 tpcs, so each tpc index need
 * 5 bits. Each map register(32bits) can hold 6 tpcs info.
 */
#define GR_TPCS_INFO_FOR_MAPREGISTER 6U

#define GFXP_WFI_TIMEOUT_COUNT_IN_USEC_DEFAULT 100U

#ifdef CONFIG_NVGPU_SET_FALCON_ACCESS_MAP
void gv11b_gr_init_get_access_map(struct gk20a *g,
				   u32 **whitelist, u32 *num_entries)
{
	static u32 wl_addr_gv11b[] = {
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
		0x419ba4, /* gr_pri_gpcs_tpcs_sm_disp_ctrl     */
		0x419e84, /* gr_pri_gpcs_tpcs_sms_dbgr_control0 */
		0x419ea8, /* gr_pri_gpcs_tpcs_sms_hww_warp_esr_report_mask */
	};
	size_t array_size;

	(void)g;
	*whitelist = wl_addr_gv11b;
	array_size = ARRAY_SIZE(wl_addr_gv11b);
	*num_entries = nvgpu_safe_cast_u64_to_u32(array_size);
}
#endif
