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
#include <nvgpu/engines.h>
#include <nvgpu/static_analysis.h>
#include <nvgpu/gr/ctx.h>
#include <nvgpu/gr/gr_instances.h>
#include <nvgpu/grmgr.h>

#include "nvgpu/gr/config.h"
#include "nvgpu/gr/gr_utils.h"

#include "hal/gr/init/gr_init_gv11b.h"
#include "gr_init_ga10b.h"

#include <nvgpu/hw/ga10b/hw_gr_ga10b.h>

#define NVGPU_GR_GPCS_RESET_DELAY_US		20U


void ga10b_gr_init_override_context_reset(struct gk20a *g)
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

	nvgpu_udelay(FECS_CTXSW_RESET_DELAY_US);
	(void) nvgpu_readl(g, gr_fecs_ctxsw_reset_ctl_r());
	(void) nvgpu_readl(g, gr_gpccs_ctxsw_reset_ctl_r());

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

	nvgpu_udelay(FECS_CTXSW_RESET_DELAY_US);
	(void) nvgpu_readl(g, gr_fecs_ctxsw_reset_ctl_r());
	(void) nvgpu_readl(g, gr_gpccs_ctxsw_reset_ctl_r());
}

void ga10b_gr_init_fe_go_idle_timeout(struct gk20a *g, bool enable)
{
	if (enable) {
#ifdef CONFIG_NVGPU_GRAPHICS
		nvgpu_writel(g, gr_fe_go_idle_timeout_r(),
			gr_fe_go_idle_timeout_count_prod_f());
#endif
		nvgpu_writel(g, gr_fe_compute_go_idle_timeout_r(),
			gr_fe_compute_go_idle_timeout_count_init_f());
	} else {
#ifdef CONFIG_NVGPU_GRAPHICS
		nvgpu_writel(g, gr_fe_go_idle_timeout_r(),
			gr_fe_go_idle_timeout_count_disabled_f());
#endif
		nvgpu_writel(g, gr_fe_compute_go_idle_timeout_r(),
			gr_fe_compute_go_idle_timeout_count_disabled_f());
	}
}

void ga10b_gr_init_auto_go_idle(struct gk20a *g, bool enable)
{
	u32 data = 0U;

	data =  nvgpu_readl(g, gr_debug_2_r());
	if (enable) {
#ifdef CONFIG_NVGPU_GRAPHICS
		data = set_field(data,
				gr_debug_2_graphics_auto_go_idle_m(),
				gr_debug_2_graphics_auto_go_idle_enabled_f());
#endif
		data = set_field(data,
				gr_debug_2_compute_auto_go_idle_m(),
				gr_debug_2_compute_auto_go_idle_enabled_f());
	} else {
#ifdef CONFIG_NVGPU_GRAPHICS
		data = set_field(data,
				gr_debug_2_graphics_auto_go_idle_m(),
				gr_debug_2_graphics_auto_go_idle_disabled_f());
#endif
		data = set_field(data,
				gr_debug_2_compute_auto_go_idle_m(),
				gr_debug_2_compute_auto_go_idle_disabled_f());
	}

	nvgpu_writel(g, gr_debug_2_r(), data);
}

void ga10b_gr_init_gpc_mmu(struct gk20a *g)
{
	u32 temp;

	nvgpu_log_info(g, "initialize gpc mmu");

	temp = g->ops.fb.mmu_ctrl(g);
	temp &= gr_gpcs_pri_mmu_ctrl_vm_pg_size_m() |
		gr_gpcs_pri_mmu_ctrl_use_pdb_big_page_size_m() |
		gr_gpcs_pri_mmu_ctrl_comp_fault_m() |
		gr_gpcs_pri_mmu_ctrl_miss_gran_m() |
		gr_gpcs_pri_mmu_ctrl_cache_mode_m() |
		gr_gpcs_pri_mmu_ctrl_atomic_capability_mode_m()|
		gr_gpcs_pri_mmu_ctrl_atomic_capability_sys_ncoh_mode_m();
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
				g->ops.fb.get_num_active_ltcs(g));
}

void ga10b_gr_init_sm_id_numbering(struct gk20a *g, u32 gpc, u32 tpc, u32 smid,
				struct nvgpu_gr_config *gr_config,
				struct nvgpu_gr_ctx *gr_ctx,
				bool patch)
{
	u32 gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_GPC_STRIDE);
	u32 tpc_in_gpc_stride = nvgpu_get_litter_value(g,
					GPU_LIT_TPC_IN_GPC_STRIDE);
	u32 gpc_offset = nvgpu_safe_mult_u32(gpc_stride, gpc);
	u32 global_tpc_index;
	u32 tpc_offset;
	u32 offset_sum = 0U;
	struct nvgpu_sm_info *sm_info;

	nvgpu_log(g, gpu_dbg_gr, "SM id %u ", smid);

#ifdef CONFIG_NVGPU_SM_DIVERSITY
	sm_info = (((gr_ctx == NULL) ||
		(nvgpu_gr_ctx_get_sm_diversity_config(gr_ctx) ==
		NVGPU_DEFAULT_SM_DIVERSITY_CONFIG)) ?
			nvgpu_gr_config_get_sm_info(gr_config, smid) :
			nvgpu_gr_config_get_redex_sm_info(gr_config, smid));
#else
		sm_info = nvgpu_gr_config_get_sm_info(gr_config, smid);
#endif
	global_tpc_index =
		nvgpu_gr_config_get_sm_info_global_tpc_index(sm_info);

	tpc = g->ops.gr.init.get_nonpes_aware_tpc(g, gpc, tpc, gr_config);
	tpc_offset = nvgpu_safe_mult_u32(tpc_in_gpc_stride, tpc);

	offset_sum = nvgpu_safe_add_u32(gpc_offset, tpc_offset);

	nvgpu_gr_ctx_patch_write(g, gr_ctx,
			nvgpu_safe_add_u32(gr_gpc0_tpc0_sm_cfg_r(), offset_sum),
			gr_gpc0_tpc0_sm_cfg_tpc_id_f(global_tpc_index),
			patch);
	nvgpu_gr_ctx_patch_write(g, gr_ctx,
			nvgpu_safe_add_u32(
				gr_gpc0_gpm_pd_sm_id_r(tpc), gpc_offset),
			gr_gpc0_gpm_pd_sm_id_id_f(global_tpc_index),
			patch);
}

void ga10b_gr_init_commit_global_bundle_cb(struct gk20a *g,
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

u32 ga10b_gr_init_get_min_gpm_fifo_depth(struct gk20a *g)
{
	(void)g;
	return gr_pd_ab_dist_cfg2_state_limit_min_gpm_fifo_depths_v();
}

u32 ga10b_gr_init_get_bundle_cb_token_limit(struct gk20a *g)
{
	(void)g;
	return gr_pd_ab_dist_cfg2_token_limit_init_v();
}

u32 ga10b_gr_init_get_attrib_cb_default_size(struct gk20a *g)
{
	(void)g;
	return gr_gpc0_ppc0_cbm_beta_cb_size_v_default_v();
}

void ga10b_gr_init_fs_state(struct gk20a *g)
{
	u32 data;
#ifdef CONFIG_NVGPU_NON_FUSA
	u32 ecc_val;
#endif
	nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gr, " ");

	data = nvgpu_readl(g, gr_gpcs_tpcs_sm_texio_control_r());
	data = set_field(data,
		gr_gpcs_tpcs_sm_texio_control_oor_addr_check_mode_m(),
		gr_gpcs_tpcs_sm_texio_control_oor_addr_check_mode_arm_63_48_match_f());
	nvgpu_writel(g, gr_gpcs_tpcs_sm_texio_control_r(), data);

	data = nvgpu_readl(g, gr_gpcs_tpcs_sm_disp_ctrl_r());
	data = set_field(data, gr_gpcs_tpcs_sm_disp_ctrl_re_suppress_m(),
			 gr_gpcs_tpcs_sm_disp_ctrl_re_suppress_disable_f());
	nvgpu_writel(g, gr_gpcs_tpcs_sm_disp_ctrl_r(), data);

#ifdef CONFIG_NVGPU_NON_FUSA
	ecc_val = nvgpu_gr_get_override_ecc_val(g);
	if (ecc_val != 0U) {
		g->ops.fuse.write_feature_override_ecc(g, ecc_val);
	}
#endif

	data = nvgpu_readl(g, gr_debug_0_r());
	data = set_field(data,
		gr_debug_0_scg_force_slow_drain_tpc_m(),
		gr_debug_0_scg_force_slow_drain_tpc_enabled_f());
	nvgpu_writel(g, gr_debug_0_r(), data);

	/*
	 * Disable CTA_SUBPARTITION_SKEW to avoid
	 * load imbalance across subpartitions.
	 * Refer nvbug 200593339
	 */
	data = nvgpu_readl(g, gr_gpcs_tpcs_mpc_pix_debug_r());
	data = set_field(data,
		gr_gpcs_tpcs_mpc_pix_debug_cta_subpartition_skew_m(),
		gr_gpcs_tpcs_mpc_pix_debug_cta_subpartition_skew_disable_f());
	nvgpu_writel(g, gr_gpcs_tpcs_mpc_pix_debug_r(), data);

}

void ga10b_gr_init_commit_global_timeslice(struct gk20a *g)
{
	u32 pd_ab_dist_cfg0 = 0U;
	u32 pe_vaf;
	u32 pe_vsc_vpc;

	nvgpu_log_fn(g, " ");

	pe_vaf = nvgpu_readl(g, gr_gpcs_tpcs_pe_vaf_r());
	pe_vsc_vpc = nvgpu_readl(g, gr_gpcs_tpcs_pes_vsc_vpc_r());

	pe_vaf = gr_gpcs_tpcs_pe_vaf_fast_mode_switch_true_f() | pe_vaf;
	pe_vsc_vpc = gr_gpcs_tpcs_pes_vsc_vpc_fast_mode_switch_true_f() |
		     pe_vsc_vpc;

	nvgpu_gr_ctx_patch_write(g, NULL, gr_gpcs_tpcs_pe_vaf_r(), pe_vaf,
		false);
	nvgpu_gr_ctx_patch_write(g, NULL, gr_gpcs_tpcs_pes_vsc_vpc_r(),
		pe_vsc_vpc, false);

	pd_ab_dist_cfg0 = nvgpu_readl(g, gr_pd_ab_dist_cfg0_r());
	pd_ab_dist_cfg0 = pd_ab_dist_cfg0 |
		gr_pd_ab_dist_cfg0_timeslice_enable_en_f();
	nvgpu_gr_ctx_patch_write(g, NULL, gr_pd_ab_dist_cfg0_r(),
		pd_ab_dist_cfg0, false);
}

int ga10b_gr_init_wait_idle(struct gk20a *g)
{
	u32 delay = POLL_DELAY_MIN_US;
	bool gr_busy;
	struct nvgpu_timeout timeout;

	nvgpu_log(g, gpu_dbg_verbose | gpu_dbg_gr, " ");

	nvgpu_timeout_init_cpu_timer(g, &timeout, nvgpu_get_poll_timeout(g));

	do {
		/*
		 * TODO legacy code has checks for invalid ctx.
		 * It is guaranteed that graphics is not doing any work if
		 * the ctx status is invalid. In that case, the busy/idle
		 * is not valid and can sometimes report busy even when it
		 * is not. We will detect that case and return early without
		 * looking at the idle status of the engine. For more details,
		 * see bugs 1762495, 200364484, 1972403.
		 */

		gr_busy = (nvgpu_readl(g, gr_status_r()) &
				gr_status_state_busy_v()) != 0U;

		if (!gr_busy) {
			nvgpu_log(g, gpu_dbg_verbose | gpu_dbg_gr, "done");
			return 0;
		}

		nvgpu_usleep_range(delay, nvgpu_safe_mult_u32(delay, 2U));
		delay = min_t(u32, delay << 1, POLL_DELAY_MAX_US);
	} while (nvgpu_timeout_expired(&timeout) == 0);

        nvgpu_err(g, "timeout gr busy : %x", gr_busy);

	return -EAGAIN;
}

void ga10b_gr_init_eng_config(struct gk20a *g)
{
	u32 data = 0U;

	data |= gr_engine_config_supported_compute_true_f();
	if (!nvgpu_is_enabled(g, NVGPU_SUPPORT_MIG)) {
		data |= gr_engine_config_supported_i2m_true_f();
#ifdef CONFIG_NVGPU_GRAPHICS
		data |= gr_engine_config_supported_2d_true_f();
		data |= gr_engine_config_supported_3d_true_f();
#endif
	}
	nvgpu_writel(g, gr_engine_config_r(), data);
}

static void ga10b_gr_init_gpcs_enable(struct gk20a *g, bool enable)
{
	u32 reg_val;

	if (enable) {
		reg_val = gr_gpcs_gpccs_engine_reset_ctl_gpc_engine_reset_enabled_f();
	} else {
		reg_val = gr_gpcs_gpccs_engine_reset_ctl_gpc_engine_reset_disabled_f();
	}
	nvgpu_writel(g, gr_gpcs_gpccs_engine_reset_ctl_r(), reg_val);
	/* Read same register back to ensure hw propagation of write */
	reg_val = nvgpu_readl(g, gr_gpcs_gpccs_engine_reset_ctl_r());
}

static bool ga10b_gr_init_is_gpcs_enabled(struct gk20a *g)
{
	u32 gpc, gpc_offset;
	u32 enabled_gpcs = 0U;
	u32 reg_offset;
	u32 reg_val;
	u32 gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_GPC_STRIDE);
	u32 cur_gr_instance_id = nvgpu_gr_get_cur_instance_id(g);
	u32 gpc_count = nvgpu_grmgr_get_gr_num_gpcs(g, cur_gr_instance_id);

	for (gpc = 0U; gpc < gpc_count; gpc++) {
		gpc_offset = nvgpu_safe_mult_u32(gpc_stride, gpc);
		reg_offset = nvgpu_safe_add_u32(gpc_offset,
				gr_gpc0_gpccs_engine_reset_ctl_r());
		reg_val = nvgpu_readl(g, reg_offset);
		if (gr_gpc0_gpccs_engine_reset_ctl_gpc_engine_reset_v(reg_val)
			== gr_gpc0_gpccs_engine_reset_ctl_gpc_engine_reset_disabled_v()) {
			enabled_gpcs++;
		}
	}
	if (enabled_gpcs == gpc_count) {
		return true;
	} else {
		nvgpu_log_info(g, "total gpc_count %d enabled gpcs %d",
				gpc_count, enabled_gpcs);
		return false;
	}
}

int ga10b_gr_init_reset_gpcs(struct gk20a *g)
{
	int err = 0;

	nvgpu_log(g, gpu_dbg_gr, " ");

	ga10b_gr_init_gpcs_enable(g, true);
	nvgpu_udelay(NVGPU_GR_GPCS_RESET_DELAY_US);
	ga10b_gr_init_gpcs_enable(g, false);

	/* After issuing gpcs reset, check if gpcs are enabled */
	if (!ga10b_gr_init_is_gpcs_enabled(g)) {
		err = -EIO;
		nvgpu_err(g, "GPCS are not out of reset");
	}

	return err;
}

static bool ga10b_gr_init_activity_empty_or_preempted(u32 val)
{
	while (val != 0U) {
		u32 v = gr_activity_4_gpc0_v(val);

		if ((v != gr_activity_4_gpc0_empty_v()) &&
			(v != gr_activity_4_gpc0_preempted_v())) {
			return false;
		}
		val >>= gr_activity_4_gpc0_s();
	}

	return true;
}

int ga10b_gr_init_wait_empty(struct gk20a *g)
{
	u32 delay = POLL_DELAY_MIN_US;
	bool ctxsw_active;
	bool gr_busy;
	u32 gr_status;
	u32 activity0, activity1, activity4;
	struct nvgpu_timeout timeout;

	nvgpu_log_fn(g, " ");

	nvgpu_timeout_init_cpu_timer(g, &timeout, nvgpu_get_poll_timeout(g));

	do {
		gr_status = nvgpu_readl(g, gr_status_r());

		ctxsw_active = (gr_status_state_v(gr_status) ==
					gr_status_state_busy_v() ||
				gr_status_fe_method_upper_v(gr_status) ==
					gr_status_fe_method_upper_busy_v() ||
				gr_status_fe_method_lower_v(gr_status) ==
					gr_status_fe_method_lower_busy_v());

		activity0 = nvgpu_readl(g, gr_activity_0_r());
		activity1 = nvgpu_readl(g, gr_activity_1_r());
		activity4 = nvgpu_readl(g, gr_activity_4_r());

		/* activity_1 status start from gr_activity_1_memfmt_b() */
		activity1 >>= gr_activity_1_memfmt_b();

		gr_busy = !(ga10b_gr_init_activity_empty_or_preempted(activity0)
			&& ga10b_gr_init_activity_empty_or_preempted(activity1)
			&&
			ga10b_gr_init_activity_empty_or_preempted(activity4));

		if (!gr_busy && !ctxsw_active) {
			nvgpu_log_fn(g, "done");
			return 0;
		}

		nvgpu_usleep_range(delay, nvgpu_safe_mult_u32(delay, 2U));
		delay = min_t(u32, delay << 1, POLL_DELAY_MAX_US);
	} while (nvgpu_timeout_expired(&timeout) == 0);

	nvgpu_err(g,
		"timeout, ctxsw busy: %d, gr busy: %d, 0x%08x, 0x%08x, 0x%08x",
			ctxsw_active, gr_busy, activity0, activity1, activity4);

	return -EAGAIN;
}

#ifndef CONFIG_NVGPU_NON_FUSA
void ga10b_gr_init_set_default_compute_regs(struct gk20a *g,
		struct nvgpu_gr_ctx *gr_ctx)
{
	u32 reg_val;

	nvgpu_gr_ctx_patch_write_begin(g, gr_ctx, true);

	reg_val = nvgpu_readl(g, gr_sked_hww_esr_en_r());
	reg_val = set_field(reg_val,
		gr_sked_hww_esr_en_skedcheck18_l1_config_too_small_m(),
		gr_sked_hww_esr_en_skedcheck18_l1_config_too_small_disabled_f());
	nvgpu_gr_ctx_patch_write(g, gr_ctx, gr_sked_hww_esr_en_r(),
		reg_val, true);

	nvgpu_gr_ctx_patch_write_end(g, gr_ctx, true);
}
#endif

#ifdef CONFIG_NVGPU_MIG
bool ga10b_gr_init_is_allowed_reg(struct gk20a *g, u32 addr)
{
	struct nvgpu_gr_gfx_reg_range gfx_range[] = {
		/* start_addr, end_addr */
		{ 0x00405800 /* gr_pri_ds_debug */,
			0x00405864 /* gr_pri_ds_cg1 */ },
		{ 0x00405900 /* gr_pri_pdb - start */,
			0x004059ff /* gr_pri_pdb - end */ },
		{ 0x00405a00 /* gr_pri_ssync - start */,
			0x00405aff /* gr_pri_ssync - end */ },
		{ 0x00406000 /* gr_pri_pd_cg */,
			0x00406518
			/* gr_pri_pd_output_batch_stall__priv_level_mask */ },
		{ 0x00407800 /* gr_pri_pd_rstr2d - start */,
			0x00407fff /* gr_pri_pd_rstr2d - end */ },
		{ 0x00408000 /* gr_pri_pd_scc - start */,
			0x004087ff /* gr_pri_pd_scc - end */ },
		/*
		 * ga10b doesn't have bes, but for some ampere GPU,
		 * the following pes reg_range is valid.
		 * For ga10b, the following bes range is unused.
		 */
		{ 0x00408800 /* gr_pri_bes - start */,
			0x004089ff /* gr_pri_bes_rdm - end */ },
		{ 0x00408a24 /* gr_pri_bes_becs_cg1 - start */,
			0x00408a24 /* gr_pri_bes_becs_cg1 - end */ },
		{ 0x00408a80 /* gr_pri_bes_crop_cg - start */,
			0x00408a84 /* gr_pri_bes_crop_cg1 - end */ },
		/*
		 * For ga10b, end_addr is 0x00418ea7.
		 * but for some ampere GPU, end_address is 0x00418eff.
		 * So maximum possible end_addr is 0x00418eff.
		 * For ga10b, range 0x00418ea7 - 0x00418eff is unused.
		 */
		{ 0x00418000 /* gr_pri_gpcs_swdx_dss_debug */,
			0x00418eff
				/* gr_pri_gpcs_swdx_tc_beta_cb_size */ },

		{ 0x00418380 /* gr_pri_gpcs_rasterarb - start */,
			0x004183ff /* gr_pri_gpcs_rasterarb - end */ },
		{ 0x00418400 /* gr_pri_gpcs_prop - start */,
			0x004185ff /* gr_pri_gpcs_prop - end */ },
		{ 0x00418600 /* gr_pri_gpcs_frstr - start */,
			0x0041867f /* gr_pri_gpcs_frstr - end */ },
		{ 0x00418680 /* gr_pri_gpcs_widcilp - start */,
			0x004186ff /* gr_pri_gpcs_widcilp - end */ },
		{ 0x00418700 /* gr_pri_gpcs_tc - start */,
			0x004187ff /* gr_pri_gpcs_tc - end */ },
		{ 0x00418800 /* gr_pri_gpcs_setup - start */,
			0x0041887f /* gr_pri_gpcs_setup - end */ },
		{ 0x004188c0 /* gr_pri_gpcs_zcull_zcram_index */,
			0x00418af8 /* gr_pri_gpcs_zcull_zcsstatus_7 */ },
		{ 0x00418b00 /* gr_pri_gpcs_crstr - start */,
			0x00418bff /* gr_pri_gpcs_crstr - end */ },
		{ 0x00418d00 /* gr_pri_gpcs_gpm_rpt - start */,
			0x00418d7f /* gr_pri_gpcs_gpm_rpt - end */ },
		{ 0x00418f00 /* gr_pri_gpcs_wdxps - start */,
			0x00418fff /* gr_pri_gpcs_wdxps - end */ },
		{ 0x00419804 /* gr_pri_gpcs_tpcs_pe_blkcg_cg */,
			0x00419900
			/* gr_pri_gpcs_tpcs_pe_blk_activity_weigts_c */ },
		{ 0x0041be00 /* gr_pri_gpcs_ppcs */,
			0x0041bfff /* gr_pri_gpcs_ppcs_wwdx - end */ },
	};

	u32 gfx_range_size = (sizeof(gfx_range) /
		sizeof(struct nvgpu_gr_gfx_reg_range));
	u32 index;

	if (!nvgpu_is_enabled(g, NVGPU_SUPPORT_MIG)) {
		nvgpu_log(g, gpu_dbg_gr,
			"Allowed reg addr[%x] ", addr);
		return true;
	}
	/*
	 * Capture whether the ctx_load address is compute subunit or not.
	 */
	for (index = 0U; index < gfx_range_size; index++) {
		if ((addr >= gfx_range[index].start_addr) &&
				(addr <= gfx_range[index].end_addr)) {
			nvgpu_log(g, gpu_dbg_mig | gpu_dbg_gr,
				"(MIG) Skip graphics reg index[%u] "
					"addr[%x] start_addr[%x] end_addr[%x] ",
				index, addr,
				gfx_range[index].start_addr,
				gfx_range[index].end_addr);
			return false;
		}
	}

	nvgpu_log(g, gpu_dbg_gr, "Allowed compute reg addr[%x] ",
		addr);

	return true;
}
#endif
