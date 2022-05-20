/*
 * Copyright (c) 2020, NVIDIA CORPORATION.  All rights reserved.
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
#include <nvgpu/engines.h>
#include <nvgpu/static_analysis.h>
#include <nvgpu/gr/ctx.h>

#include "nvgpu/gr/config.h"
#include "nvgpu/gr/gr_utils.h"

#include "hal/gr/init/gr_init_gv11b.h"
#include "gr_init_ga100.h"

#include <nvgpu/hw/ga100/hw_gr_ga100.h>

u32 ga100_gr_init_get_min_gpm_fifo_depth(struct gk20a *g)
{
	return gr_pd_ab_dist_cfg2_state_limit_min_gpm_fifo_depths_v();
}

u32 ga100_gr_init_get_bundle_cb_token_limit(struct gk20a *g)
{
	return gr_pd_ab_dist_cfg2_token_limit_init_v();
}

u32 ga100_gr_init_get_attrib_cb_default_size(struct gk20a *g)
{
	return gr_gpc0_ppc0_cbm_beta_cb_size_v_default_v();
}

void ga100_gr_init_commit_global_bundle_cb(struct gk20a *g,
	struct nvgpu_gr_ctx *gr_ctx, u64 addr, u32 size, bool patch)
{
	u32 data;
	u32 cb_addr;
	u32 bundle_cb_token_limit = g->ops.gr.init.get_bundle_cb_token_limit(g);

	addr = addr >> gr_scc_bundle_cb_base_addr_39_8_align_bits_v();

	nvgpu_log_info(g, "bundle cb addr : 0x%016llx, size : %u",
		addr, size);

	cb_addr = nvgpu_safe_cast_u64_to_u32(addr);
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

void ga100_gr_init_override_context_reset(struct gk20a *g)
{
	nvgpu_writel(g, gr_fecs_ctxsw_reset_ctl_r(),
			gr_fecs_ctxsw_reset_ctl_sys_halt_disabled_f() |
			gr_fecs_ctxsw_reset_ctl_sys_engine_reset_disabled_f() |
			gr_fecs_ctxsw_reset_ctl_sys_context_reset_enabled_f());

	nvgpu_writel(g, gr_gpccs_ctxsw_reset_ctl_r(),
			gr_gpccs_ctxsw_reset_ctl_gpc_halt_disabled_f() |
			gr_gpccs_ctxsw_reset_ctl_gpc_reset_disabled_f() |
			gr_gpccs_ctxsw_reset_ctl_gpc_context_reset_enabled_f() |
			gr_gpccs_ctxsw_reset_ctl_zcull_reset_enabled_f());

	if (!nvgpu_is_enabled(g, NVGPU_SUPPORT_MIG)) {
		nvgpu_writel(g, gr_bes_becs_ctxsw_be_reset_ctl_r(),
				gr_bes_becs_ctxsw_be_reset_ctl_be_halt_disabled_f() |
				gr_bes_becs_ctxsw_be_reset_ctl_be_engine_reset_disabled_f() |
				gr_bes_becs_ctxsw_be_reset_ctl_be_context_reset_enabled_f());
	}

	nvgpu_udelay(FECS_CTXSW_RESET_DELAY_US);
	(void) nvgpu_readl(g, gr_fecs_ctxsw_reset_ctl_r());
	(void) nvgpu_readl(g, gr_gpccs_ctxsw_reset_ctl_r());

	if (!nvgpu_is_enabled(g, NVGPU_SUPPORT_MIG)) {
		(void) nvgpu_readl(g, gr_bes_becs_ctxsw_be_reset_ctl_r());
	}

	/* Deassert reset */
	nvgpu_writel(g, gr_fecs_ctxsw_reset_ctl_r(),
			gr_fecs_ctxsw_reset_ctl_sys_halt_disabled_f() |
			gr_fecs_ctxsw_reset_ctl_sys_engine_reset_disabled_f() |
			gr_fecs_ctxsw_reset_ctl_sys_context_reset_disabled_f());

	nvgpu_writel(g, gr_gpccs_ctxsw_reset_ctl_r(),
			gr_gpccs_ctxsw_reset_ctl_gpc_halt_disabled_f() |
			gr_gpccs_ctxsw_reset_ctl_gpc_reset_disabled_f() |
			gr_gpccs_ctxsw_reset_ctl_gpc_context_reset_disabled_f() |
			gr_gpccs_ctxsw_reset_ctl_zcull_reset_disabled_f());

	if (!nvgpu_is_enabled(g, NVGPU_SUPPORT_MIG)) {
		nvgpu_writel(g, gr_bes_becs_ctxsw_be_reset_ctl_r(),
				gr_bes_becs_ctxsw_be_reset_ctl_be_halt_disabled_f() |
				gr_bes_becs_ctxsw_be_reset_ctl_be_engine_reset_disabled_f() |
				gr_bes_becs_ctxsw_be_reset_ctl_be_context_reset_disabled_f());
	}

	nvgpu_udelay(FECS_CTXSW_RESET_DELAY_US);
	(void) nvgpu_readl(g, gr_fecs_ctxsw_reset_ctl_r());
	(void) nvgpu_readl(g, gr_gpccs_ctxsw_reset_ctl_r());

	if (!nvgpu_is_enabled(g, NVGPU_SUPPORT_MIG)) {
		(void) nvgpu_readl(g, gr_bes_becs_ctxsw_be_reset_ctl_r());
	}
}
