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

u32 gp10b_gr_init_get_sm_id_size(void)
{
	return gr_cwd_sm_id__size_1_v();
}

static bool gr_activity_empty_or_preempted(u32 val)
{
	while (val != 0U) {
		u32 v = val & 7U;

		if ((v != gr_activity_4_gpc0_empty_v()) &&
			(v != gr_activity_4_gpc0_preempted_v())) {
			return false;
		}
		val >>= 3;
	}

	return true;
}

int gp10b_gr_init_wait_empty(struct gk20a *g)
{
	u32 delay = POLL_DELAY_MIN_US;
	bool ctxsw_active;
	bool gr_busy;
	u32 gr_status;
	u32 activity0, activity1, activity2, activity4;
	struct nvgpu_timeout timeout;

	nvgpu_log_fn(g, " ");

	nvgpu_timeout_init_cpu_timer(g, &timeout, nvgpu_get_poll_timeout(g));

	do {
		/* fmodel: host gets fifo_engine_status(gr) from gr
		   only when gr_status is read */
		gr_status = nvgpu_readl(g, gr_status_r());

		ctxsw_active = (gr_status & BIT32(7)) != 0U;

		activity0 = nvgpu_readl(g, gr_activity_0_r());
		activity1 = nvgpu_readl(g, gr_activity_1_r());
		activity2 = nvgpu_readl(g, gr_activity_2_r());
		activity4 = nvgpu_readl(g, gr_activity_4_r());

		gr_busy = !(gr_activity_empty_or_preempted(activity0) &&
			gr_activity_empty_or_preempted(activity1) &&
			(activity2 == 0U) &&
			gr_activity_empty_or_preempted(activity4));

		if (!gr_busy && !ctxsw_active) {
			nvgpu_log_fn(g, "done");
			return 0;
		}

		nvgpu_usleep_range(delay, nvgpu_safe_mult_u32(delay, 2U));
		delay = min_t(u32, delay << 1, POLL_DELAY_MAX_US);
	} while (nvgpu_timeout_expired(&timeout) == 0);

	nvgpu_err(g,
	    "timeout, ctxsw busy : %d, gr busy : %d, %08x, %08x, %08x, %08x",
	    ctxsw_active, gr_busy, activity0, activity1, activity2, activity4);

	return -EAGAIN;
}

void gp10b_gr_init_commit_global_bundle_cb(struct gk20a *g,
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

u32 gp10b_gr_init_pagepool_default_size(struct gk20a *g)
{
	(void)g;
	return gr_scc_pagepool_total_pages_hwmax_value_v();
}

void gp10b_gr_init_commit_global_pagepool(struct gk20a *g,
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

	nvgpu_log_info(g, "pagepool buffer addr : 0x%016llx, size : %lu",
		addr, size);

	pp_addr = nvgpu_safe_cast_u64_to_u32(addr);
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
}

void gp10b_gr_init_commit_global_cb_manager(struct gk20a *g,
	struct nvgpu_gr_config *config, struct nvgpu_gr_ctx *gr_ctx, bool patch)
{
	u32 attrib_offset_in_chunk = 0;
	u32 alpha_offset_in_chunk = 0;
	u32 pd_ab_max_output;
	u32 gpc_index, ppc_index;
	u32 temp, temp2;
	u32 cbm_cfg_size_beta, cbm_cfg_size_alpha, cbm_cfg_size_steadystate;
	u32 attrib_size_in_chunk, cb_attrib_cache_size_init;
	u32 attrib_cb_default_size = g->ops.gr.init.get_attrib_cb_default_size(g);
	u32 alpha_cb_default_size = g->ops.gr.init.get_alpha_cb_default_size(g);
	u32 attrib_cb_size = g->ops.gr.init.get_attrib_cb_size(g,
		nvgpu_gr_config_get_tpc_count(config));
	u32 alpha_cb_size = g->ops.gr.init.get_alpha_cb_size(g,
		nvgpu_gr_config_get_tpc_count(config));
	u32 gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_GPC_STRIDE);
	u32 ppc_in_gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_PPC_IN_GPC_STRIDE);
	u32 num_pes_per_gpc = nvgpu_get_litter_value(g, GPU_LIT_NUM_PES_PER_GPC);

	nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gr, " ");

#ifdef CONFIG_NVGPU_GFXP
	if (nvgpu_gr_ctx_get_graphics_preemption_mode(gr_ctx)
			== NVGPU_PREEMPTION_MODE_GRAPHICS_GFXP) {
		attrib_size_in_chunk =
			g->ops.gr.init.get_attrib_cb_gfxp_size(g);
		cb_attrib_cache_size_init =
			g->ops.gr.init.get_attrib_cb_gfxp_default_size(g);
	} else {
#endif
		attrib_size_in_chunk = attrib_cb_size;
		cb_attrib_cache_size_init = attrib_cb_default_size;
#ifdef CONFIG_NVGPU_GFXP
	}
#endif
	nvgpu_gr_ctx_patch_write(g, gr_ctx, gr_ds_tga_constraintlogic_beta_r(),
		attrib_cb_default_size, patch);
	nvgpu_gr_ctx_patch_write(g, gr_ctx, gr_ds_tga_constraintlogic_alpha_r(),
		alpha_cb_default_size, patch);

	pd_ab_max_output = nvgpu_safe_mult_u32(alpha_cb_default_size,
		gr_gpc0_ppc0_cbm_beta_cb_size_v_granularity_v()) /
		gr_pd_ab_dist_cfg1_max_output_granularity_v();

	nvgpu_gr_ctx_patch_write(g, gr_ctx, gr_pd_ab_dist_cfg1_r(),
		gr_pd_ab_dist_cfg1_max_output_f(pd_ab_max_output) |
		gr_pd_ab_dist_cfg1_max_batches_init_f(), patch);

	attrib_offset_in_chunk = nvgpu_safe_add_u32(alpha_offset_in_chunk,
				   nvgpu_safe_mult_u32(
					nvgpu_gr_config_get_tpc_count(config),
					alpha_cb_size));

	for (gpc_index = 0;
	     gpc_index < nvgpu_gr_config_get_gpc_count(config);
	     gpc_index++) {
		temp = nvgpu_safe_mult_u32(gpc_stride, gpc_index);
		temp2 = nvgpu_safe_mult_u32(num_pes_per_gpc, gpc_index);
		for (ppc_index = 0;
		     ppc_index < nvgpu_gr_config_get_gpc_ppc_count(config, gpc_index);
		     ppc_index++) {
			u32 pes_tpc_count =
				nvgpu_gr_config_get_pes_tpc_count(config,
							gpc_index, ppc_index);
			u32 ppc_posn = nvgpu_safe_mult_u32(ppc_in_gpc_stride,
							ppc_index);
			u32 sum_temp_pcc = nvgpu_safe_add_u32(temp, ppc_posn);

			cbm_cfg_size_beta =
				nvgpu_safe_mult_u32(
						cb_attrib_cache_size_init,
						pes_tpc_count);
			cbm_cfg_size_alpha =
				nvgpu_safe_mult_u32(alpha_cb_default_size,
						pes_tpc_count);
			cbm_cfg_size_steadystate =
				nvgpu_safe_mult_u32(attrib_cb_default_size,
						pes_tpc_count);

			nvgpu_gr_ctx_patch_write(g, gr_ctx,
				nvgpu_safe_add_u32(
					gr_gpc0_ppc0_cbm_beta_cb_size_r(),
					sum_temp_pcc),
				cbm_cfg_size_beta, patch);

			nvgpu_gr_ctx_patch_write(g, gr_ctx,
				nvgpu_safe_add_u32(
					gr_gpc0_ppc0_cbm_beta_cb_offset_r(),
					sum_temp_pcc),
				attrib_offset_in_chunk, patch);

			nvgpu_gr_ctx_patch_write(g, gr_ctx,
			     nvgpu_safe_add_u32(
			       gr_gpc0_ppc0_cbm_beta_steady_state_cb_size_r(),
			       sum_temp_pcc),
			       cbm_cfg_size_steadystate, patch);

			attrib_offset_in_chunk = nvgpu_safe_add_u32(
				attrib_offset_in_chunk,
				nvgpu_safe_mult_u32(attrib_size_in_chunk,
							pes_tpc_count));

			nvgpu_gr_ctx_patch_write(g, gr_ctx,
				nvgpu_safe_add_u32(
					gr_gpc0_ppc0_cbm_alpha_cb_size_r(),
					sum_temp_pcc),
				cbm_cfg_size_alpha, patch);

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
				gr_gpcs_swdx_tc_beta_cb_size_v_f(cbm_cfg_size_steadystate),
				patch);
		}
	}
}

void gp10b_gr_init_get_supported_preemption_modes(
	u32 *graphics_preemption_mode_flags, u32 *compute_preemption_mode_flags)
{
	u32 gfxp_flags = NVGPU_PREEMPTION_MODE_GRAPHICS_WFI;

#ifdef CONFIG_NVGPU_GFXP
	gfxp_flags |= NVGPU_PREEMPTION_MODE_GRAPHICS_GFXP;
#endif
	*graphics_preemption_mode_flags = gfxp_flags;

	*compute_preemption_mode_flags = (NVGPU_PREEMPTION_MODE_COMPUTE_WFI |
					 NVGPU_PREEMPTION_MODE_COMPUTE_CTA);
#ifdef CONFIG_NVGPU_CILP
	*compute_preemption_mode_flags |= NVGPU_PREEMPTION_MODE_COMPUTE_CILP;
#endif
}

void gp10b_gr_init_get_default_preemption_modes(
	u32 *default_graphics_preempt_mode, u32 *default_compute_preempt_mode)
{
	*default_graphics_preempt_mode = NVGPU_PREEMPTION_MODE_GRAPHICS_WFI;
	*default_compute_preempt_mode = NVGPU_PREEMPTION_MODE_COMPUTE_WFI;
}

#ifdef CONFIG_NVGPU_GFXP
u32 gp10b_gr_init_get_ctx_attrib_cb_size(struct gk20a *g, u32 betacb_size,
	u32 tpc_count, u32 max_tpc)
{
	u32 alpha_cb_size = g->ops.gr.init.get_alpha_cb_size(g, tpc_count);
	u32 size;

	size = nvgpu_safe_mult_u32(
		nvgpu_safe_add_u32(betacb_size, alpha_cb_size),
		nvgpu_safe_mult_u32(
			gr_gpc0_ppc0_cbm_beta_cb_size_v_granularity_v(),
			max_tpc));

	return NVGPU_ALIGN(size, 128U);
}

u32 gp10b_gr_init_get_ctx_pagepool_size(struct gk20a *g)
{
	return nvgpu_safe_mult_u32(
		 g->ops.gr.init.pagepool_default_size(g),
		 gr_scc_pagepool_total_pages_byte_granularity_v());
}
#endif /* CONFIG_NVGPU_GFXP */
