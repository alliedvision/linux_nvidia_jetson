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

#include "gr_init_gm20b.h"

#include <nvgpu/hw/gm20b/hw_gr_gm20b.h>

#define FE_PWR_MODE_TIMEOUT_MAX_US 2000U
#define FE_PWR_MODE_TIMEOUT_DEFAULT_US 10U
#define FECS_CTXSW_RESET_DELAY_US 10U

void gm20b_gr_init_lg_coalesce(struct gk20a *g, u32 data)
{
	u32 val;

	nvgpu_log_fn(g, " ");

	val = nvgpu_readl(g, gr_gpcs_tpcs_tex_m_dbg2_r());
	val = set_field(val,
			gr_gpcs_tpcs_tex_m_dbg2_lg_rd_coalesce_en_m(),
			gr_gpcs_tpcs_tex_m_dbg2_lg_rd_coalesce_en_f(data));
	nvgpu_writel(g, gr_gpcs_tpcs_tex_m_dbg2_r(), val);
}

void gm20b_gr_init_su_coalesce(struct gk20a *g, u32 data)
{
	u32 reg;

	reg = nvgpu_readl(g, gr_gpcs_tpcs_tex_m_dbg2_r());
	reg = set_field(reg,
			gr_gpcs_tpcs_tex_m_dbg2_su_rd_coalesce_en_m(),
			gr_gpcs_tpcs_tex_m_dbg2_su_rd_coalesce_en_f(data));

	nvgpu_writel(g, gr_gpcs_tpcs_tex_m_dbg2_r(), reg);
}

void gm20b_gr_init_pes_vsc_stream(struct gk20a *g)
{
	u32 data = nvgpu_readl(g, gr_gpc0_ppc0_pes_vsc_strem_r());

	data = set_field(data, gr_gpc0_ppc0_pes_vsc_strem_master_pe_m(),
			gr_gpc0_ppc0_pes_vsc_strem_master_pe_true_f());
	nvgpu_writel(g, gr_gpc0_ppc0_pes_vsc_strem_r(), data);
}

void gm20b_gr_init_fifo_access(struct gk20a *g, bool enable)
{
	u32 fifo_val;

	fifo_val = nvgpu_readl(g, gr_gpfifo_ctl_r());
	fifo_val &= ~gr_gpfifo_ctl_semaphore_access_f(1U);
	fifo_val &= ~gr_gpfifo_ctl_access_f(1U);

	if (enable) {
		fifo_val |= (gr_gpfifo_ctl_access_enabled_f() |
			gr_gpfifo_ctl_semaphore_access_enabled_f());
	} else {
		fifo_val |= (gr_gpfifo_ctl_access_f(0U) |
			gr_gpfifo_ctl_semaphore_access_f(0U));
	}

	nvgpu_writel(g, gr_gpfifo_ctl_r(), fifo_val);
}

void gm20b_gr_init_pd_tpc_per_gpc(struct gk20a *g,
				  struct nvgpu_gr_config *gr_config)
{
	u32 reg_index;
	u32 tpc_per_gpc;
	u32 gpc_id = 0U;

	for (reg_index = 0U;
	     reg_index < gr_pd_num_tpc_per_gpc__size_1_v();
	     reg_index++) {

		tpc_per_gpc =
		 gr_pd_num_tpc_per_gpc_count0_f(
		  nvgpu_gr_config_get_gpc_tpc_count(gr_config, gpc_id)) |
		 gr_pd_num_tpc_per_gpc_count1_f(
		  nvgpu_gr_config_get_gpc_tpc_count(gr_config,
					nvgpu_safe_add_u32(gpc_id, 1U))) |
		 gr_pd_num_tpc_per_gpc_count2_f(
		  nvgpu_gr_config_get_gpc_tpc_count(gr_config,
					nvgpu_safe_add_u32(gpc_id, 2U))) |
		 gr_pd_num_tpc_per_gpc_count3_f(
		  nvgpu_gr_config_get_gpc_tpc_count(gr_config,
					nvgpu_safe_add_u32(gpc_id, 3U))) |
		 gr_pd_num_tpc_per_gpc_count4_f(
		  nvgpu_gr_config_get_gpc_tpc_count(gr_config,
					nvgpu_safe_add_u32(gpc_id, 4U))) |
		 gr_pd_num_tpc_per_gpc_count5_f(
		  nvgpu_gr_config_get_gpc_tpc_count(gr_config,
					nvgpu_safe_add_u32(gpc_id, 5U))) |
		 gr_pd_num_tpc_per_gpc_count6_f(
		  nvgpu_gr_config_get_gpc_tpc_count(gr_config,
					nvgpu_safe_add_u32(gpc_id, 6U))) |
		 gr_pd_num_tpc_per_gpc_count7_f(
		  nvgpu_gr_config_get_gpc_tpc_count(gr_config,
					nvgpu_safe_add_u32(gpc_id, 7U)));

		nvgpu_writel(g, gr_pd_num_tpc_per_gpc_r(reg_index), tpc_per_gpc);
		nvgpu_writel(g, gr_ds_num_tpc_per_gpc_r(reg_index), tpc_per_gpc);
		gpc_id = nvgpu_safe_add_u32(gpc_id, 8U);
	}
}

void gm20b_gr_init_pd_skip_table_gpc(struct gk20a *g,
				     struct nvgpu_gr_config *gr_config)
{
	u32 gpc_index;
	u32 skip_mask = 0;

	for (gpc_index = 0;
	     gpc_index < (gr_pd_dist_skip_table__size_1_v() * 4U);
	     gpc_index += 4U) {
		if ((gr_pd_dist_skip_table_gpc_4n0_mask_f(
			nvgpu_gr_config_get_gpc_skip_mask(gr_config,
				gpc_index)) != 0U) ||
			(gr_pd_dist_skip_table_gpc_4n1_mask_f(
				nvgpu_gr_config_get_gpc_skip_mask(gr_config,
					gpc_index + 1U)) != 0U) ||
			(gr_pd_dist_skip_table_gpc_4n2_mask_f(
				nvgpu_gr_config_get_gpc_skip_mask(gr_config,
					gpc_index + 2U)) != 0U) ||
			(gr_pd_dist_skip_table_gpc_4n3_mask_f(
				nvgpu_gr_config_get_gpc_skip_mask(gr_config,
					gpc_index + 3U)) != 0U)) {
			skip_mask = 1;
		}
		nvgpu_writel(g, gr_pd_dist_skip_table_r(gpc_index/4U),
			skip_mask);
	}
}

void gm20b_gr_init_cwd_gpcs_tpcs_num(struct gk20a *g,
				     u32 gpc_count, u32 tpc_count)
{
	nvgpu_writel(g, gr_cwd_fs_r(),
		     gr_cwd_fs_num_gpcs_f(gpc_count) |
		     gr_cwd_fs_num_tpcs_f(tpc_count));
}

int gm20b_gr_init_wait_idle(struct gk20a *g)
{
	u32 delay = POLL_DELAY_MIN_US;
	u32 gr_engine_id;
	bool ctxsw_active;
	bool gr_busy;
	bool ctx_status_invalid;
	struct nvgpu_engine_status_info engine_status;
	struct nvgpu_timeout timeout;

	nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gr, " ");

	gr_engine_id = nvgpu_engine_get_gr_id(g);

	nvgpu_timeout_init_cpu_timer(g, &timeout, nvgpu_get_poll_timeout(g));

	do {
		/*
		 * fmodel: host gets fifo_engine_status(gr) from gr
		 * only when gr_status is read
		 */
		(void) nvgpu_readl(g, gr_status_r());

		g->ops.engine_status.read_engine_status_info(g, gr_engine_id,
							     &engine_status);

		ctxsw_active = engine_status.ctxsw_in_progress;

		ctx_status_invalid = nvgpu_engine_status_is_ctxsw_invalid(
							&engine_status);

		gr_busy = (nvgpu_readl(g, gr_engine_status_r()) &
				gr_engine_status_value_busy_f()) != 0U;

		if (ctx_status_invalid || (!gr_busy && !ctxsw_active)) {
			nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gr, "done");
			return 0;
		}

		nvgpu_usleep_range(delay, nvgpu_safe_mult_u32(delay, 2U));
		delay = min_t(u32, delay << 1, POLL_DELAY_MAX_US);

	} while (nvgpu_timeout_expired(&timeout) == 0);

	nvgpu_err(g, "timeout, ctxsw busy : %d, gr busy : %d",
		  ctxsw_active, gr_busy);

	return -EAGAIN;
}

int gm20b_gr_init_wait_fe_idle(struct gk20a *g)
{
	u32 val;
	u32 delay = POLL_DELAY_MIN_US;
	struct nvgpu_timeout timeout;

#ifdef CONFIG_NVGPU_SIM
	if (nvgpu_is_enabled(g, NVGPU_IS_FMODEL)) {
		return 0;
	}
#endif

	nvgpu_log(g, gpu_dbg_verbose, " ");

	nvgpu_timeout_init_cpu_timer(g, &timeout, nvgpu_get_poll_timeout(g));

	do {
		val = nvgpu_readl(g, gr_status_r());

		if (gr_status_fe_method_lower_v(val) == 0U) {
			nvgpu_log(g, gpu_dbg_verbose, "done");
			return 0;
		}

		nvgpu_usleep_range(delay, nvgpu_safe_mult_u32(delay, 2U));
		delay = min_t(u32, delay << 1, POLL_DELAY_MAX_US);
	} while (nvgpu_timeout_expired(&timeout) == 0);

	nvgpu_err(g, "timeout, fe busy : %x", val);

	return -EAGAIN;
}

int gm20b_gr_init_fe_pwr_mode_force_on(struct gk20a *g, bool force_on)
{
	struct nvgpu_timeout timeout;
	int ret = 0;
	u32 reg_val;

#ifdef CONFIG_NVGPU_SIM
	if (nvgpu_is_enabled(g, NVGPU_IS_FMODEL)) {
		return 0;
	}
#endif

	if (force_on) {
		reg_val = gr_fe_pwr_mode_req_send_f() |
			  gr_fe_pwr_mode_mode_force_on_f();
	} else {
		reg_val = gr_fe_pwr_mode_req_send_f() |
			  gr_fe_pwr_mode_mode_auto_f();
	}

	nvgpu_timeout_init_retry(g, &timeout,
				FE_PWR_MODE_TIMEOUT_MAX_US /
				FE_PWR_MODE_TIMEOUT_DEFAULT_US);

	nvgpu_writel(g, gr_fe_pwr_mode_r(), reg_val);

	ret = -ETIMEDOUT;

	do {
		u32 req = gr_fe_pwr_mode_req_v(
				nvgpu_readl(g, gr_fe_pwr_mode_r()));
		if (req == gr_fe_pwr_mode_req_done_v()) {
			ret = 0;
			break;
		}

		nvgpu_udelay(FE_PWR_MODE_TIMEOUT_DEFAULT_US);
	} while (nvgpu_timeout_expired_msg(&timeout,
				"timeout setting FE mode %u", force_on) == 0);

	return ret;
}

void gm20b_gr_init_override_context_reset(struct gk20a *g)
{
	nvgpu_writel(g, gr_fecs_ctxsw_reset_ctl_r(),
			gr_fecs_ctxsw_reset_ctl_sys_halt_disabled_f() |
			gr_fecs_ctxsw_reset_ctl_gpc_halt_disabled_f() |
			gr_fecs_ctxsw_reset_ctl_be_halt_disabled_f() |
			gr_fecs_ctxsw_reset_ctl_sys_engine_reset_disabled_f() |
			gr_fecs_ctxsw_reset_ctl_gpc_engine_reset_disabled_f() |
			gr_fecs_ctxsw_reset_ctl_be_engine_reset_disabled_f() |
			gr_fecs_ctxsw_reset_ctl_sys_context_reset_enabled_f() |
			gr_fecs_ctxsw_reset_ctl_gpc_context_reset_enabled_f() |
			gr_fecs_ctxsw_reset_ctl_be_context_reset_enabled_f());

	nvgpu_udelay(FECS_CTXSW_RESET_DELAY_US);
	(void) nvgpu_readl(g, gr_fecs_ctxsw_reset_ctl_r());

	/* Deassert reset */
	nvgpu_writel(g, gr_fecs_ctxsw_reset_ctl_r(),
			gr_fecs_ctxsw_reset_ctl_sys_halt_disabled_f() |
			gr_fecs_ctxsw_reset_ctl_gpc_halt_disabled_f() |
			gr_fecs_ctxsw_reset_ctl_be_halt_disabled_f() |
			gr_fecs_ctxsw_reset_ctl_sys_engine_reset_disabled_f() |
			gr_fecs_ctxsw_reset_ctl_gpc_engine_reset_disabled_f() |
			gr_fecs_ctxsw_reset_ctl_be_engine_reset_disabled_f() |
			gr_fecs_ctxsw_reset_ctl_sys_context_reset_disabled_f() |
			gr_fecs_ctxsw_reset_ctl_gpc_context_reset_disabled_f() |
			gr_fecs_ctxsw_reset_ctl_be_context_reset_disabled_f());

	nvgpu_udelay(FECS_CTXSW_RESET_DELAY_US);
	(void) nvgpu_readl(g, gr_fecs_ctxsw_reset_ctl_r());
}

void gm20b_gr_init_pipe_mode_override(struct gk20a *g, bool enable)
{
	if (enable) {
		nvgpu_writel(g, gr_pipe_bundle_config_r(),
			gr_pipe_bundle_config_override_pipe_mode_enabled_f());
	} else {
		nvgpu_writel(g, gr_pipe_bundle_config_r(),
			gr_pipe_bundle_config_override_pipe_mode_disabled_f());
	}
}

void gm20b_gr_init_load_method_init(struct gk20a *g,
		struct netlist_av_list *sw_method_init)
{
	u32 i;
	u32 last_method_data = 0U;
	u32 class_num = 0U;

	for (i = 0U; i < sw_method_init->count; i++) {
		class_num = gr_pri_mme_shadow_ram_index_nvclass_v(
			sw_method_init->l[i].addr);
#ifdef CONFIG_NVGPU_MIG
		if ((nvgpu_is_enabled(g, NVGPU_SUPPORT_MIG)) &&
				(!g->ops.gpu_class.is_valid_compute(
					class_num))) {
			nvgpu_log(g, gpu_dbg_mig | gpu_dbg_gr,
				"(MIG) Skip graphics sw method index[%u] "
					"addr[%x] value[%x] class_num[%x] ",
				i, sw_method_init->l[i].addr,
				sw_method_init->l[i].value,
				class_num);
			continue;
		}
#endif
		if ((i == 0U) ||
				(sw_method_init->l[i].value !=
					last_method_data)) {
			nvgpu_writel(g, gr_pri_mme_shadow_ram_data_r(),
				sw_method_init->l[i].value);
			last_method_data = sw_method_init->l[i].value;
		}
		nvgpu_writel(g, gr_pri_mme_shadow_ram_index_r(),
			gr_pri_mme_shadow_ram_index_write_trigger_f() |
			sw_method_init->l[i].addr);
		nvgpu_log(g, gpu_dbg_gr,
			"Allowed graphics sw method index[%u] "
				"addr[%x] value[%x] class_num[%x] ",
			i, sw_method_init->l[i].addr,
			sw_method_init->l[i].value,
			class_num);
	}
}

u32 gm20b_gr_init_get_global_ctx_cb_buffer_size(struct gk20a *g)
{
	return nvgpu_safe_mult_u32(
		g->ops.gr.init.get_bundle_cb_default_size(g),
		gr_scc_bundle_cb_size_div_256b_byte_granularity_v());
}

u32 gm20b_gr_init_get_global_ctx_pagepool_buffer_size(struct gk20a *g)
{
	return nvgpu_safe_mult_u32(
		g->ops.gr.init.pagepool_default_size(g),
		gr_scc_pagepool_total_pages_byte_granularity_v());
}

void gm20b_gr_init_commit_global_attrib_cb(struct gk20a *g,
	struct nvgpu_gr_ctx *gr_ctx, u32 tpc_count, u32 max_tpc, u64 addr,
	bool patch)
{
	u32 cb_addr;

	(void)tpc_count;
	(void)max_tpc;

	addr = addr >> gr_gpcs_setup_attrib_cb_base_addr_39_12_align_bits_v();

	nvgpu_log_info(g, "attrib cb addr : 0x%016llx", addr);

	cb_addr = nvgpu_safe_cast_u64_to_u32(addr);
	nvgpu_gr_ctx_patch_write(g, gr_ctx, gr_gpcs_setup_attrib_cb_base_r(),
		gr_gpcs_setup_attrib_cb_base_addr_39_12_f(cb_addr) |
		gr_gpcs_setup_attrib_cb_base_valid_true_f(), patch);

	nvgpu_gr_ctx_patch_write(g, gr_ctx, gr_gpcs_tpcs_pe_pin_cb_global_base_addr_r(),
		gr_gpcs_tpcs_pe_pin_cb_global_base_addr_v_f(cb_addr) |
		gr_gpcs_tpcs_pe_pin_cb_global_base_addr_valid_true_f(), patch);

	nvgpu_gr_ctx_patch_write(g, gr_ctx, gr_gpcs_tpcs_mpc_vtg_cb_global_base_addr_r(),
		gr_gpcs_tpcs_mpc_vtg_cb_global_base_addr_v_f(cb_addr) |
		gr_gpcs_tpcs_mpc_vtg_cb_global_base_addr_valid_true_f(), patch);
}

u32 gm20b_gr_init_get_patch_slots(struct gk20a *g,
	struct nvgpu_gr_config *config)
{
	(void)g;
	(void)config;
	return PATCH_CTX_SLOTS_PER_PAGE;
}

bool gm20b_gr_init_is_allowed_sw_bundle(struct gk20a *g,
	u32 bundle_addr, u32 bundle_value, int *context)
{
	if (!nvgpu_is_enabled(g, NVGPU_SUPPORT_MIG)) {
		nvgpu_log(g, gpu_dbg_mig,
			"Allowed bundle addr[%x] value[%x] ",
			bundle_addr, bundle_value);
		return true;
	}
	/*
	 * Capture whether the current bundle is compute or not.
	 * Store in context.
	 */
	if (gr_pipe_bundle_address_value_v(bundle_addr) ==
			GR_PIPE_MODE_BUNDLE) {
		*context = (bundle_value == GR_PIPE_MODE_MAJOR_COMPUTE);
		nvgpu_log(g, gpu_dbg_mig, "(MIG) Bundle start "
			"addr[%x] bundle_value[%x] is_compute_start[%d]",
			bundle_addr, bundle_value, (*context != 0));
		return *context != 0;
	}

	/* And now use context, only compute bundles allowed in MIG. */
	if (*context == 0) {
		nvgpu_log(g, gpu_dbg_mig, "(MIG) Skipped bundle "
			"addr[%x] bundle_value[%x] ",
			bundle_addr, bundle_value);
		return false;
	}

	nvgpu_log(g, gpu_dbg_mig, "(MIG) Compute bundle "
		"addr[%x] bundle_value[%x] ",
		bundle_addr, bundle_value);

	return true;
}

#ifndef CONFIG_NVGPU_GR_GOLDEN_CTX_VERIFICATION
int gm20b_gr_init_load_sw_bundle_init(struct gk20a *g,
		struct netlist_av_list *sw_bundle_init)
{
	u32 i;
	int err = 0;
	u32 last_bundle_data = 0U;
	int context = 0;

	for (i = 0U; i < sw_bundle_init->count; i++) {
		if (!g->ops.gr.init.is_allowed_sw_bundle(g,
				sw_bundle_init->l[i].addr,
				sw_bundle_init->l[i].value,
				&context)) {
			continue;
		}

		if (i == 0U || last_bundle_data != sw_bundle_init->l[i].value) {
			nvgpu_writel(g, gr_pipe_bundle_data_r(),
				sw_bundle_init->l[i].value);
			last_bundle_data = sw_bundle_init->l[i].value;
		}

		nvgpu_writel(g, gr_pipe_bundle_address_r(),
			     sw_bundle_init->l[i].addr);

		if (gr_pipe_bundle_address_value_v(sw_bundle_init->l[i].addr) ==
		    GR_GO_IDLE_BUNDLE) {
			err = g->ops.gr.init.wait_idle(g);
			if (err != 0) {
				return err;
			}
		}

		err = g->ops.gr.init.wait_fe_idle(g);
		if (err != 0) {
			return err;
		}
	}

	return err;
}
#endif
