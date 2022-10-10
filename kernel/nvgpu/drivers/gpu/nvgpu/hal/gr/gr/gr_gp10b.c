/*
 * GP10B GPU GR
 *
 * Copyright (c) 2015-2022, NVIDIA CORPORATION.  All rights reserved.
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

#include <nvgpu/timers.h>
#include <nvgpu/kmem.h>
#include <nvgpu/dma.h>
#include <nvgpu/bug.h>
#include <nvgpu/debugger.h>
#include <nvgpu/fuse.h>
#include <nvgpu/enabled.h>
#include <nvgpu/io.h>
#include <nvgpu/utils.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/fifo.h>
#include <nvgpu/channel.h>
#include <nvgpu/regops.h>
#include <nvgpu/gr/subctx.h>
#include <nvgpu/gr/ctx.h>
#include <nvgpu/gr/gr.h>
#include <nvgpu/gr/gr_instances.h>
#include <nvgpu/gr/config.h>
#include <nvgpu/gr/gr_falcon.h>
#include <nvgpu/gr/obj_ctx.h>
#include <nvgpu/engines.h>
#include <nvgpu/engine_status.h>
#include <nvgpu/preempt.h>
#include <nvgpu/runlist.h>

#include "gr_gk20a.h"
#include "gr_gp10b.h"

#include "common/gr/gr_priv.h"

#include <nvgpu/hw/gp10b/hw_gr_gp10b.h>

void gr_gp10b_set_bes_crop_debug3(struct gk20a *g, u32 data)
{
	u32 val;

	nvgpu_log_fn(g, " ");

	val = gk20a_readl(g, gr_bes_crop_debug3_r());
	if ((data & 1U) != 0U) {
		val = set_field(val,
				gr_bes_crop_debug3_blendopt_read_suppress_m(),
				gr_bes_crop_debug3_blendopt_read_suppress_enabled_f());
		val = set_field(val,
				gr_bes_crop_debug3_blendopt_fill_override_m(),
				gr_bes_crop_debug3_blendopt_fill_override_enabled_f());
	} else {
		val = set_field(val,
				gr_bes_crop_debug3_blendopt_read_suppress_m(),
				gr_bes_crop_debug3_blendopt_read_suppress_disabled_f());
		val = set_field(val,
				gr_bes_crop_debug3_blendopt_fill_override_m(),
				gr_bes_crop_debug3_blendopt_fill_override_disabled_f());
	}
	gk20a_writel(g, gr_bes_crop_debug3_r(), val);
}

void gr_gp10b_set_bes_crop_debug4(struct gk20a *g, u32 data)
{
	u32 val;

	nvgpu_log_fn(g, " ");

	val = gk20a_readl(g, gr_bes_crop_debug4_r());
	if ((data & 0x1U) == NVC097_BES_CROP_DEBUG4_CLAMP_FP_BLEND_TO_MAXVAL) {
		val = set_field(val,
			gr_bes_crop_debug4_clamp_fp_blend_m(),
			gr_bes_crop_debug4_clamp_fp_blend_to_maxval_f());
	} else if ((data & 0x1U) == NVC097_BES_CROP_DEBUG4_CLAMP_FP_BLEND_TO_INF) {
		val = set_field(val,
			gr_bes_crop_debug4_clamp_fp_blend_m(),
			gr_bes_crop_debug4_clamp_fp_blend_to_inf_f());
	} else {
		nvgpu_warn(g,
			"gr_gp10b_set_bes_crop_debug4: wrong data sent!");
		return;
	}
	gk20a_writel(g, gr_bes_crop_debug4_r(), val);
}

void gr_gp10b_set_alpha_circular_buffer_size(struct gk20a *g, u32 data)
{
	struct nvgpu_gr *gr = nvgpu_gr_get_cur_instance_ptr(g);
	u32 gpc_index, ppc_index, stride, val;
	u32 pd_ab_max_output;
	u32 alpha_cb_size = data * 4U;
	u32 alpha_cb_size_max = g->ops.gr.init.get_alpha_cb_size(g,
		nvgpu_gr_config_get_tpc_count(gr->config));
	u32 gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_GPC_STRIDE);
	u32 ppc_in_gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_PPC_IN_GPC_STRIDE);

	nvgpu_log_fn(g, " ");

	if (alpha_cb_size > alpha_cb_size_max) {
		alpha_cb_size = alpha_cb_size_max;
	}

	gk20a_writel(g, gr_ds_tga_constraintlogic_alpha_r(),
		(gk20a_readl(g, gr_ds_tga_constraintlogic_alpha_r()) &
		 ~gr_ds_tga_constraintlogic_alpha_cbsize_f(~U32(0U))) |
		 gr_ds_tga_constraintlogic_alpha_cbsize_f(alpha_cb_size));

	pd_ab_max_output = alpha_cb_size *
		gr_gpc0_ppc0_cbm_alpha_cb_size_v_granularity_v() /
		gr_pd_ab_dist_cfg1_max_output_granularity_v();

	nvgpu_writel(g, gr_pd_ab_dist_cfg1_r(),
			gr_pd_ab_dist_cfg1_max_output_f(pd_ab_max_output) |
			gr_pd_ab_dist_cfg1_max_batches_init_f());

	for (gpc_index = 0;
	     gpc_index < nvgpu_gr_config_get_gpc_count(gr->config);
             gpc_index++) {
		stride = gpc_stride * gpc_index;

		for (ppc_index = 0;
		     ppc_index < nvgpu_gr_config_get_gpc_ppc_count(gr->config, gpc_index);
		     ppc_index++) {

			val = gk20a_readl(g, gr_gpc0_ppc0_cbm_alpha_cb_size_r() +
				stride +
				ppc_in_gpc_stride * ppc_index);

			val = set_field(val, gr_gpc0_ppc0_cbm_alpha_cb_size_v_m(),
					gr_gpc0_ppc0_cbm_alpha_cb_size_v_f(alpha_cb_size *
						nvgpu_gr_config_get_pes_tpc_count(gr->config,
							gpc_index, ppc_index)));

			gk20a_writel(g, gr_gpc0_ppc0_cbm_alpha_cb_size_r() +
				stride +
				ppc_in_gpc_stride * ppc_index, val);
		}
	}
}

void gr_gp10b_set_circular_buffer_size(struct gk20a *g, u32 data)
{
	struct nvgpu_gr *gr = nvgpu_gr_get_cur_instance_ptr(g);
	u32 gpc_index, ppc_index, stride, val;
	u32 cb_size_steady = data * 4U, cb_size;
	u32 gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_GPC_STRIDE);
	u32 ppc_in_gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_PPC_IN_GPC_STRIDE);
	u32 attrib_cb_size = g->ops.gr.init.get_attrib_cb_size(g,
		nvgpu_gr_config_get_tpc_count(gr->config));

	nvgpu_log_fn(g, " ");

	if (cb_size_steady > attrib_cb_size) {
		cb_size_steady = attrib_cb_size;
	}
	if (gk20a_readl(g, gr_gpc0_ppc0_cbm_beta_cb_size_r()) !=
		gk20a_readl(g,
			gr_gpc0_ppc0_cbm_beta_steady_state_cb_size_r())) {
		cb_size = cb_size_steady +
			(gr_gpc0_ppc0_cbm_beta_cb_size_v_gfxp_v() -
			 gr_gpc0_ppc0_cbm_beta_cb_size_v_default_v());
	} else {
		cb_size = cb_size_steady;
	}

	gk20a_writel(g, gr_ds_tga_constraintlogic_beta_r(),
		(gk20a_readl(g, gr_ds_tga_constraintlogic_beta_r()) &
		 ~gr_ds_tga_constraintlogic_beta_cbsize_f(~U32(0U))) |
		 gr_ds_tga_constraintlogic_beta_cbsize_f(cb_size_steady));

	for (gpc_index = 0;
	     gpc_index < nvgpu_gr_config_get_gpc_count(gr->config);
	     gpc_index++) {
		stride = gpc_stride * gpc_index;

		for (ppc_index = 0;
		     ppc_index < nvgpu_gr_config_get_gpc_ppc_count(gr->config, gpc_index);
		     ppc_index++) {

			val = gk20a_readl(g, gr_gpc0_ppc0_cbm_beta_cb_size_r() +
				stride +
				ppc_in_gpc_stride * ppc_index);

			val = set_field(val,
				gr_gpc0_ppc0_cbm_beta_cb_size_v_m(),
				gr_gpc0_ppc0_cbm_beta_cb_size_v_f(cb_size *
					nvgpu_gr_config_get_pes_tpc_count(gr->config,
						gpc_index, ppc_index)));

			gk20a_writel(g, gr_gpc0_ppc0_cbm_beta_cb_size_r() +
				stride +
				ppc_in_gpc_stride * ppc_index, val);

			gk20a_writel(g, ppc_in_gpc_stride * ppc_index +
				gr_gpc0_ppc0_cbm_beta_steady_state_cb_size_r() +
				stride,
				gr_gpc0_ppc0_cbm_beta_steady_state_cb_size_v_f(
					cb_size_steady));

			val = gk20a_readl(g, gr_gpcs_swdx_tc_beta_cb_size_r(
						ppc_index + gpc_index));

			val = set_field(val,
				gr_gpcs_swdx_tc_beta_cb_size_v_m(),
				gr_gpcs_swdx_tc_beta_cb_size_v_f(
					cb_size_steady *
					nvgpu_gr_config_get_gpc_ppc_count(gr->config, gpc_index)));

			gk20a_writel(g, gr_gpcs_swdx_tc_beta_cb_size_r(
						ppc_index + gpc_index), val);
		}
	}
}


int gr_gp10b_dump_gr_status_regs(struct gk20a *g,
			   struct nvgpu_debug_context *o)
{
	struct nvgpu_gr *gr = nvgpu_gr_get_cur_instance_ptr(g);
	u32 gr_engine_id;
	struct nvgpu_engine_status_info engine_status;

	gr_engine_id = nvgpu_engine_get_gr_id(g);

	gk20a_debug_output(o, "NV_PGRAPH_STATUS: 0x%x",
		gk20a_readl(g, gr_status_r()));
	gk20a_debug_output(o, "NV_PGRAPH_STATUS1: 0x%x",
		gk20a_readl(g, gr_status_1_r()));
	gk20a_debug_output(o, "NV_PGRAPH_STATUS2: 0x%x",
		gk20a_readl(g, gr_status_2_r()));
	gk20a_debug_output(o, "NV_PGRAPH_ENGINE_STATUS: 0x%x",
		gk20a_readl(g, gr_engine_status_r()));
	gk20a_debug_output(o, "NV_PGRAPH_GRFIFO_STATUS : 0x%x",
		gk20a_readl(g, gr_gpfifo_status_r()));
	gk20a_debug_output(o, "NV_PGRAPH_GRFIFO_CONTROL : 0x%x",
		gk20a_readl(g, gr_gpfifo_ctl_r()));
	gk20a_debug_output(o, "NV_PGRAPH_PRI_FECS_HOST_INT_STATUS : 0x%x",
		gk20a_readl(g, gr_fecs_host_int_status_r()));
	gk20a_debug_output(o, "NV_PGRAPH_EXCEPTION  : 0x%x",
		gk20a_readl(g, gr_exception_r()));
	gk20a_debug_output(o, "NV_PGRAPH_FECS_INTR  : 0x%x",
		gk20a_readl(g, gr_fecs_intr_r()));
	g->ops.engine_status.read_engine_status_info(g, gr_engine_id,
		&engine_status);
	gk20a_debug_output(o, "NV_PFIFO_ENGINE_STATUS(GR) : 0x%x",
		engine_status.reg_data);
	gk20a_debug_output(o, "NV_PGRAPH_ACTIVITY0: 0x%x",
		gk20a_readl(g, gr_activity_0_r()));
	gk20a_debug_output(o, "NV_PGRAPH_ACTIVITY1: 0x%x",
		gk20a_readl(g, gr_activity_1_r()));
	gk20a_debug_output(o, "NV_PGRAPH_ACTIVITY2: 0x%x",
		gk20a_readl(g, gr_activity_2_r()));
	gk20a_debug_output(o, "NV_PGRAPH_ACTIVITY4: 0x%x",
		gk20a_readl(g, gr_activity_4_r()));
	gk20a_debug_output(o, "NV_PGRAPH_PRI_SKED_ACTIVITY: 0x%x",
		gk20a_readl(g, gr_pri_sked_activity_r()));
	gk20a_debug_output(o, "NV_PGRAPH_PRI_GPC0_GPCCS_GPC_ACTIVITY0: 0x%x",
		gk20a_readl(g, gr_pri_gpc0_gpccs_gpc_activity0_r()));
	gk20a_debug_output(o, "NV_PGRAPH_PRI_GPC0_GPCCS_GPC_ACTIVITY1: 0x%x",
		gk20a_readl(g, gr_pri_gpc0_gpccs_gpc_activity1_r()));
	gk20a_debug_output(o, "NV_PGRAPH_PRI_GPC0_GPCCS_GPC_ACTIVITY2: 0x%x",
		gk20a_readl(g, gr_pri_gpc0_gpccs_gpc_activity2_r()));
	gk20a_debug_output(o, "NV_PGRAPH_PRI_GPC0_GPCCS_GPC_ACTIVITY3: 0x%x",
		gk20a_readl(g, gr_pri_gpc0_gpccs_gpc_activity3_r()));
	gk20a_debug_output(o, "NV_PGRAPH_PRI_GPC0_TPC0_TPCCS_TPC_ACTIVITY0: 0x%x",
		gk20a_readl(g, gr_pri_gpc0_tpc0_tpccs_tpc_activity_0_r()));
	if ((nvgpu_gr_config_get_base_count_gpc_tpc(gr->config) != NULL) &&
		(nvgpu_gr_config_get_gpc_tpc_count(gr->config, 0) == 2U)) {
		gk20a_debug_output(o,
			"NV_PGRAPH_PRI_GPC0_TPC1_TPCCS_TPC_ACTIVITY0: 0x%x",
			gk20a_readl(g,
			(gr_pri_gpc0_tpc0_tpccs_tpc_activity_0_r() +
			nvgpu_get_litter_value(g, GPU_LIT_TPC_IN_GPC_STRIDE))));
	}
	gk20a_debug_output(o, "NV_PGRAPH_PRI_GPCS_GPCCS_GPC_ACTIVITY0: 0x%x",
		gk20a_readl(g, gr_pri_gpcs_gpccs_gpc_activity_0_r()));
	gk20a_debug_output(o, "NV_PGRAPH_PRI_GPCS_GPCCS_GPC_ACTIVITY1: 0x%x",
		gk20a_readl(g, gr_pri_gpcs_gpccs_gpc_activity_1_r()));
	gk20a_debug_output(o, "NV_PGRAPH_PRI_GPCS_GPCCS_GPC_ACTIVITY2: 0x%x",
		gk20a_readl(g, gr_pri_gpcs_gpccs_gpc_activity_2_r()));
	gk20a_debug_output(o, "NV_PGRAPH_PRI_GPCS_GPCCS_GPC_ACTIVITY3: 0x%x",
		gk20a_readl(g, gr_pri_gpcs_gpccs_gpc_activity_3_r()));
	gk20a_debug_output(o, "NV_PGRAPH_PRI_GPCS_TPCS_TPCCS_TPC_ACTIVITY0: 0x%x",
		gk20a_readl(g, gr_pri_gpcs_tpcs_tpccs_tpc_activity_0_r()));
	gk20a_debug_output(o, "NV_PGRAPH_PRI_BE0_BECS_BE_ACTIVITY0: 0x%x",
		gk20a_readl(g, gr_pri_be0_becs_be_activity0_r()));
	gk20a_debug_output(o, "NV_PGRAPH_PRI_BE1_BECS_BE_ACTIVITY0: 0x%x",
		gk20a_readl(g, (gr_pri_be0_becs_be_activity0_r() +
		nvgpu_get_litter_value(g, GPU_LIT_ROP_STRIDE))));
	gk20a_debug_output(o, "NV_PGRAPH_PRI_BES_BECS_BE_ACTIVITY0: 0x%x",
		gk20a_readl(g, gr_pri_bes_becs_be_activity0_r()));
	gk20a_debug_output(o, "NV_PGRAPH_PRI_DS_MPIPE_STATUS: 0x%x",
		gk20a_readl(g, gr_pri_ds_mpipe_status_r()));
	gk20a_debug_output(o, "NV_PGRAPH_PRI_FE_GO_IDLE_TIMEOUT : 0x%x",
		gk20a_readl(g, gr_fe_go_idle_timeout_r()));
	gk20a_debug_output(o, "NV_PGRAPH_PRI_FE_GO_IDLE_INFO : 0x%x",
		gk20a_readl(g, gr_pri_fe_go_idle_info_r()));
	gk20a_debug_output(o, "NV_PGRAPH_PRI_GPC0_TPC0_TEX_M_TEX_SUBUNITS_STATUS: 0x%x",
		gk20a_readl(g, gr_pri_gpc0_tpc0_tex_m_tex_subunits_status_r()));
	gk20a_debug_output(o, "NV_PGRAPH_PRI_CWD_FS: 0x%x",
		gk20a_readl(g, gr_cwd_fs_r()));
	gk20a_debug_output(o, "NV_PGRAPH_PRI_FE_TPC_FS: 0x%x",
		gk20a_readl(g, gr_fe_tpc_fs_r()));
	gk20a_debug_output(o, "NV_PGRAPH_PRI_CWD_GPC_TPC_ID(0): 0x%x",
		gk20a_readl(g, gr_cwd_gpc_tpc_id_r(0)));
	gk20a_debug_output(o, "NV_PGRAPH_PRI_CWD_SM_ID(0): 0x%x",
		gk20a_readl(g, gr_cwd_sm_id_r(0)));
	gk20a_debug_output(o, "NV_PGRAPH_PRI_FECS_CTXSW_STATUS_FE_0: 0x%x",
		g->ops.gr.falcon.read_fecs_ctxsw_status0(g));
	gk20a_debug_output(o, "NV_PGRAPH_PRI_FECS_CTXSW_STATUS_1: 0x%x",
		g->ops.gr.falcon.read_fecs_ctxsw_status1(g));
	gk20a_debug_output(o, "NV_PGRAPH_PRI_GPC0_GPCCS_CTXSW_STATUS_GPC_0: 0x%x",
		gk20a_readl(g, gr_gpc0_gpccs_ctxsw_status_gpc_0_r()));
	gk20a_debug_output(o, "NV_PGRAPH_PRI_GPC0_GPCCS_CTXSW_STATUS_1: 0x%x",
		gk20a_readl(g, gr_gpc0_gpccs_ctxsw_status_1_r()));
	gk20a_debug_output(o, "NV_PGRAPH_PRI_FECS_CTXSW_IDLESTATE : 0x%x",
		gk20a_readl(g, gr_fecs_ctxsw_idlestate_r()));
	gk20a_debug_output(o, "NV_PGRAPH_PRI_GPC0_GPCCS_CTXSW_IDLESTATE : 0x%x",
		gk20a_readl(g, gr_gpc0_gpccs_ctxsw_idlestate_r()));
	gk20a_debug_output(o, "NV_PGRAPH_PRI_FECS_CURRENT_CTX : 0x%x",
		g->ops.gr.falcon.get_current_ctx(g));
	gk20a_debug_output(o, "NV_PGRAPH_PRI_FECS_NEW_CTX : 0x%x",
		gk20a_readl(g, gr_fecs_new_ctx_r()));
	gk20a_debug_output(o, "NV_PGRAPH_PRI_BE0_CROP_STATUS1 : 0x%x",
		gk20a_readl(g, gr_pri_be0_crop_status1_r()));
	gk20a_debug_output(o, "NV_PGRAPH_PRI_BES_CROP_STATUS1 : 0x%x",
		gk20a_readl(g, gr_pri_bes_crop_status1_r()));
	gk20a_debug_output(o, "NV_PGRAPH_PRI_BE0_ZROP_STATUS : 0x%x",
		gk20a_readl(g, gr_pri_be0_zrop_status_r()));
	gk20a_debug_output(o, "NV_PGRAPH_PRI_BE0_ZROP_STATUS2 : 0x%x",
		gk20a_readl(g, gr_pri_be0_zrop_status2_r()));
	gk20a_debug_output(o, "NV_PGRAPH_PRI_BES_ZROP_STATUS : 0x%x",
		gk20a_readl(g, gr_pri_bes_zrop_status_r()));
	gk20a_debug_output(o, "NV_PGRAPH_PRI_BES_ZROP_STATUS2 : 0x%x",
		gk20a_readl(g, gr_pri_bes_zrop_status2_r()));
	gk20a_debug_output(o, "NV_PGRAPH_PRI_BE0_BECS_BE_EXCEPTION: 0x%x",
		gk20a_readl(g, gr_pri_be0_becs_be_exception_r()));
	gk20a_debug_output(o, "NV_PGRAPH_PRI_BE0_BECS_BE_EXCEPTION_EN: 0x%x",
		gk20a_readl(g, gr_pri_be0_becs_be_exception_en_r()));
	gk20a_debug_output(o, "NV_PGRAPH_PRI_GPC0_GPCCS_GPC_EXCEPTION: 0x%x",
		gk20a_readl(g, gr_pri_gpc0_gpccs_gpc_exception_r()));
	gk20a_debug_output(o, "NV_PGRAPH_PRI_GPC0_GPCCS_GPC_EXCEPTION_EN: 0x%x",
		gk20a_readl(g, gr_pri_gpc0_gpccs_gpc_exception_en_r()));
	gk20a_debug_output(o, "NV_PGRAPH_PRI_GPC0_TPC0_TPCCS_TPC_EXCEPTION: 0x%x",
		gk20a_readl(g, gr_pri_gpc0_tpc0_tpccs_tpc_exception_r()));
	gk20a_debug_output(o, "NV_PGRAPH_PRI_GPC0_TPC0_TPCCS_TPC_EXCEPTION_EN: 0x%x",
		gk20a_readl(g, gr_pri_gpc0_tpc0_tpccs_tpc_exception_en_r()));
	return 0;
}

#ifdef CONFIG_NVGPU_TEGRA_FUSE
void gr_gp10b_set_gpc_tpc_mask(struct gk20a *g, u32 gpc_index)
{
	struct nvgpu_gr *gr = nvgpu_gr_get_cur_instance_ptr(g);
	nvgpu_tegra_fuse_write_bypass(g, 0x1);
	nvgpu_tegra_fuse_write_access_sw(g, 0x0);

	if (nvgpu_gr_config_get_gpc_tpc_mask(gr->config, gpc_index) == 0x1U) {
		nvgpu_tegra_fuse_write_opt_gpu_tpc0_disable(g, 0x2);
	} else if (nvgpu_gr_config_get_gpc_tpc_mask(gr->config, gpc_index) ==
			0x2U) {
		nvgpu_tegra_fuse_write_opt_gpu_tpc0_disable(g, 0x1);
	} else {
		nvgpu_tegra_fuse_write_opt_gpu_tpc0_disable(g, 0x0);
	}
}
#endif

static int gr_gp10b_disable_channel_or_tsg(struct gk20a *g, struct nvgpu_channel *fault_ch)
{
	int ret = 0;
	struct nvgpu_tsg *tsg;

	tsg = nvgpu_tsg_from_ch(fault_ch);
	if (tsg == NULL) {
		nvgpu_err(g, "CILP: chid: %d is not bound to tsg",
				fault_ch->chid);
		return -EINVAL;
	}

	nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gpu_dbg | gpu_dbg_intr, " ");

	ret = nvgpu_channel_disable_tsg(g, fault_ch);
	if (ret != 0) {
		nvgpu_err(g, "CILP: failed to disable channel/TSG!");
		return ret;
	}

	ret = g->ops.runlist.reload(g, fault_ch->runlist, tsg->rl_domain, true, false);
	if (ret != 0) {
		nvgpu_err(g, "CILP: failed to restart runlist 0!");
		return ret;
	}

	nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gpu_dbg | gpu_dbg_intr,
			"CILP: restarted runlist");

	nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gpu_dbg | gpu_dbg_intr,
			"CILP: tsgid: 0x%x", tsg->tsgid);

	g->ops.fifo.preempt_trigger(g, tsg->tsgid, ID_TYPE_TSG);
	nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gpu_dbg | gpu_dbg_intr,
			"CILP: preempted tsg");
	return ret;
}

int gr_gp10b_set_cilp_preempt_pending(struct gk20a *g,
			struct nvgpu_channel *fault_ch)
{
	int ret;
	struct nvgpu_tsg *tsg;
	struct nvgpu_gr_ctx *gr_ctx;
	struct nvgpu_gr *gr = nvgpu_gr_get_cur_instance_ptr(g);

	nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gpu_dbg | gpu_dbg_intr, " ");

	tsg = nvgpu_tsg_from_ch(fault_ch);
	if (tsg == NULL) {
		return -EINVAL;
	}

	gr_ctx = tsg->gr_ctx;

	if (nvgpu_gr_ctx_get_cilp_preempt_pending(gr_ctx)) {
		nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gpu_dbg | gpu_dbg_intr,
				"CILP is already pending for chid %d",
				fault_ch->chid);
		return 0;
	}

	nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gpu_dbg | gpu_dbg_intr,
		"CILP: ctx id is 0x%x",
		nvgpu_gr_ctx_read_ctx_id(gr_ctx));

	/* send ucode method to set ctxsw interrupt */
	ret = g->ops.gr.falcon.ctrl_ctxsw(g,
		NVGPU_GR_FALCON_METHOD_CONFIGURE_CTXSW_INTR,
		nvgpu_gr_ctx_get_ctx_id(g, gr_ctx), NULL);
	if (ret != 0) {
		nvgpu_err(g, "CILP: failed to enable ctxsw interrupt!");
		return ret;
	}

	nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gpu_dbg | gpu_dbg_intr,
				"CILP: enabled ctxsw completion interrupt");

	nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gpu_dbg | gpu_dbg_intr,
			"CILP: disabling channel %d",
			fault_ch->chid);

	ret = gr_gp10b_disable_channel_or_tsg(g, fault_ch);
	if (ret != 0) {
		nvgpu_err(g, "CILP: failed to disable channel!!");
		return ret;
	}

	/* set cilp_preempt_pending = true and record the channel */
	nvgpu_gr_ctx_set_cilp_preempt_pending(gr_ctx, true);
	gr->cilp_preempt_pending_chid = fault_ch->chid;

#ifdef CONFIG_NVGPU_CHANNEL_TSG_CONTROL
	g->ops.tsg.post_event_id(tsg, NVGPU_EVENT_ID_CILP_PREEMPTION_STARTED);
#endif

	return 0;
}

/* @brief pre-process work on the SM exceptions to determine if we clear them or not.
 *
 * On Pascal, if we are in CILP preemtion mode, preempt the channel and handle errors with special processing
 */
int gr_gp10b_pre_process_sm_exception(struct gk20a *g,
		u32 gpc, u32 tpc, u32 sm, u32 global_esr, u32 warp_esr,
		bool sm_debugger_attached, struct nvgpu_channel *fault_ch,
		bool *early_exit, bool *ignore_debugger)
{
#ifdef CONFIG_NVGPU_DEBUGGER
	bool cilp_enabled = false;
	struct nvgpu_tsg *tsg;

	*early_exit = false;
	*ignore_debugger = false;

	if (fault_ch != NULL) {
		tsg = nvgpu_tsg_from_ch(fault_ch);
		if (tsg  == NULL) {
			return -EINVAL;
		}

		cilp_enabled =
			(nvgpu_gr_ctx_get_compute_preemption_mode(tsg->gr_ctx) ==
				NVGPU_PREEMPTION_MODE_COMPUTE_CILP);
	}

	nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gpu_dbg, "SM Exception received on gpc %d tpc %d = %u",
			gpc, tpc, global_esr);

	if (cilp_enabled && sm_debugger_attached) {
		u32 gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_GPC_STRIDE);
		u32 tpc_in_gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_TPC_IN_GPC_STRIDE);
		u32 offset = gpc_stride * gpc + tpc_in_gpc_stride * tpc;
		u32 global_mask = 0, dbgr_control0, global_esr_copy;
		int ret;

		if ((global_esr & gr_gpc0_tpc0_sm_hww_global_esr_bpt_int_pending_f()) != 0U) {
			gk20a_writel(g, gr_gpc0_tpc0_sm_hww_global_esr_r() + offset,
					gr_gpc0_tpc0_sm_hww_global_esr_bpt_int_pending_f());
		}

		if ((global_esr & gr_gpc0_tpc0_sm_hww_global_esr_single_step_complete_pending_f()) != 0U) {
			gk20a_writel(g, gr_gpc0_tpc0_sm_hww_global_esr_r() + offset,
					gr_gpc0_tpc0_sm_hww_global_esr_single_step_complete_pending_f());
		}

		global_mask = gr_gpc0_tpc0_sm_hww_global_esr_sm_to_sm_fault_pending_f() |
			gr_gpcs_tpcs_sm_hww_global_esr_l1_error_pending_f() |
			gr_gpcs_tpcs_sm_hww_global_esr_multiple_warp_errors_pending_f() |
			gr_gpcs_tpcs_sm_hww_global_esr_physical_stack_overflow_error_pending_f() |
			gr_gpcs_tpcs_sm_hww_global_esr_timeout_error_pending_f() |
			gr_gpcs_tpcs_sm_hww_global_esr_bpt_pause_pending_f();

		if (warp_esr != 0U || (global_esr & global_mask) != 0U) {
			*ignore_debugger = true;

			nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gpu_dbg,
					"CILP: starting wait for LOCKED_DOWN on gpc %d tpc %d",
					gpc, tpc);

			if (nvgpu_dbg_gpu_broadcast_stop_trigger(fault_ch)) {
				nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gpu_dbg,
						"CILP: Broadcasting STOP_TRIGGER from gpc %d tpc %d",
						gpc, tpc);
				g->ops.gr.suspend_all_sms(g, global_mask, false);

				nvgpu_dbg_gpu_clear_broadcast_stop_trigger(fault_ch);
			} else {
				nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gpu_dbg,
						"CILP: STOP_TRIGGER from gpc %d tpc %d",
						gpc, tpc);
				g->ops.gr.suspend_single_sm(g, gpc, tpc, sm, global_mask, true);
			}

			/* reset the HWW errors after locking down */
			global_esr_copy = g->ops.gr.intr.get_sm_hww_global_esr(g,
							gpc, tpc, sm);
			g->ops.gr.intr.clear_sm_hww(g,
						gpc, tpc, sm, global_esr_copy);
			nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gpu_dbg,
					"CILP: HWWs cleared for gpc %d tpc %d",
					gpc, tpc);

			nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gpu_dbg, "CILP: Setting CILP preempt pending");
			ret = gr_gp10b_set_cilp_preempt_pending(g, fault_ch);
			if (ret != 0) {
				nvgpu_err(g, "CILP: error while setting CILP preempt pending!");
				return ret;
			}

			dbgr_control0 = gk20a_readl(g, gr_gpc0_tpc0_sm_dbgr_control0_r() + offset);
			if ((dbgr_control0 & gr_gpcs_tpcs_sm_dbgr_control0_single_step_mode_enable_f()) != 0U) {
				nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gpu_dbg,
						"CILP: clearing SINGLE_STEP_MODE before resume for gpc %d tpc %d",
						gpc, tpc);
				dbgr_control0 = set_field(dbgr_control0,
						gr_gpcs_tpcs_sm_dbgr_control0_single_step_mode_m(),
						gr_gpcs_tpcs_sm_dbgr_control0_single_step_mode_disable_f());
				gk20a_writel(g, gr_gpc0_tpc0_sm_dbgr_control0_r() + offset, dbgr_control0);
			}

			nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gpu_dbg,
					"CILP: resume for gpc %d tpc %d",
					gpc, tpc);
			g->ops.gr.resume_single_sm(g, gpc, tpc, sm);

			*ignore_debugger = true;
			nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gpu_dbg, "CILP: All done on gpc %d, tpc %d", gpc, tpc);
		}

		*early_exit = true;
	}
#endif
	return 0;
}

u32 gp10b_gr_get_sm_hww_warp_esr(struct gk20a *g,
			u32 gpc, u32 tpc, u32 sm)
{
	u32 offset = nvgpu_gr_gpc_offset(g, gpc) + nvgpu_gr_tpc_offset(g, tpc);
	u32 hww_warp_esr = gk20a_readl(g,
			 gr_gpc0_tpc0_sm_hww_warp_esr_r() + offset);

	(void)sm;

	if ((hww_warp_esr & gr_gpc0_tpc0_sm_hww_warp_esr_addr_valid_m()) == 0U) {
		hww_warp_esr = set_field(hww_warp_esr,
			gr_gpc0_tpc0_sm_hww_warp_esr_addr_error_type_m(),
			gr_gpc0_tpc0_sm_hww_warp_esr_addr_error_type_none_f());
	}

	return hww_warp_esr;
}

bool gr_gp10b_suspend_context(struct nvgpu_channel *ch,
				bool *cilp_preempt_pending)
{
	struct gk20a *g = ch->g;
	struct nvgpu_tsg *tsg;
	struct nvgpu_gr_ctx *gr_ctx;
	bool ctx_resident = false;
	int err = 0;

	tsg = nvgpu_tsg_from_ch(ch);
	if (tsg == NULL) {
		return -EINVAL;
	}

	gr_ctx = tsg->gr_ctx;

	*cilp_preempt_pending = false;

	if (gk20a_is_channel_ctx_resident(ch)) {
		g->ops.gr.suspend_all_sms(g, 0, false);

		if (nvgpu_gr_ctx_get_compute_preemption_mode(gr_ctx) ==
			NVGPU_PREEMPTION_MODE_COMPUTE_CILP) {
			err = gr_gp10b_set_cilp_preempt_pending(g, ch);
			if (err != 0) {
				nvgpu_err(g, "unable to set CILP preempt pending");
			} else {
				*cilp_preempt_pending = true;
			}

			g->ops.gr.resume_all_sms(g);
		}

		ctx_resident = true;
	} else {
		if (nvgpu_channel_disable_tsg(g, ch) != 0) {
			/* ch might not be bound to tsg anymore */
			nvgpu_err(g, "failed to disable channel/TSG");
		}
	}

	return ctx_resident;
}

int gr_gp10b_suspend_contexts(struct gk20a *g,
				struct dbg_session_gk20a *dbg_s,
				int *ctx_resident_ch_fd)
{
	u32 delay = POLL_DELAY_MIN_US;
	bool cilp_preempt_pending = false;
	struct nvgpu_channel *cilp_preempt_pending_ch = NULL;
	struct nvgpu_channel *ch;
	struct dbg_session_channel_data *ch_data;
	int err = 0;
	int local_ctx_resident_ch_fd = -1;
	bool ctx_resident;

	nvgpu_mutex_acquire(&g->dbg_sessions_lock);

	err = nvgpu_gr_disable_ctxsw(g);
	if (err != 0) {
		nvgpu_err(g, "unable to stop gr ctxsw");
		nvgpu_mutex_release(&g->dbg_sessions_lock);
		goto clean_up;
	}

	nvgpu_mutex_acquire(&dbg_s->ch_list_lock);

	nvgpu_list_for_each_entry(ch_data, &dbg_s->ch_list,
			dbg_session_channel_data, ch_entry) {
		ch = g->fifo.channel + ch_data->chid;

		ctx_resident = gr_gp10b_suspend_context(ch,
					&cilp_preempt_pending);
		if (ctx_resident) {
			local_ctx_resident_ch_fd = ch_data->channel_fd;
		}
		if (cilp_preempt_pending) {
			cilp_preempt_pending_ch = ch;
		}
	}

	nvgpu_mutex_release(&dbg_s->ch_list_lock);

	err = nvgpu_gr_enable_ctxsw(g);
	if (err != 0) {
		nvgpu_mutex_release(&g->dbg_sessions_lock);
		goto clean_up;
	}

	nvgpu_mutex_release(&g->dbg_sessions_lock);

	if (cilp_preempt_pending_ch != NULL) {
		struct nvgpu_tsg *tsg;
		struct nvgpu_gr_ctx *gr_ctx;
		struct nvgpu_timeout timeout;

		nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gpu_dbg | gpu_dbg_intr,
			"CILP preempt pending, waiting %u msecs for preemption",
			nvgpu_get_poll_timeout(g));

		tsg = nvgpu_tsg_from_ch(cilp_preempt_pending_ch);
		if (tsg == NULL) {
			err = -EINVAL;
			goto clean_up;
		}

		gr_ctx = tsg->gr_ctx;

		nvgpu_timeout_init_cpu_timer(g, &timeout, nvgpu_get_poll_timeout(g));
		do {
			if (!nvgpu_gr_ctx_get_cilp_preempt_pending(gr_ctx)) {
				break;
			}

			nvgpu_usleep_range(delay, delay * 2U);
			delay = min_t(u32, delay << 1, POLL_DELAY_MAX_US);
		} while (nvgpu_timeout_expired(&timeout) == 0);

		/* If cilp is still pending at this point, timeout */
		if (nvgpu_gr_ctx_get_cilp_preempt_pending(gr_ctx)) {
			err = -ETIMEDOUT;
		}
	}

	*ctx_resident_ch_fd = local_ctx_resident_ch_fd;

clean_up:
	return err;
}

#ifdef CONFIG_NVGPU_CHANNEL_TSG_SCHEDULING
int gr_gp10b_set_boosted_ctx(struct nvgpu_channel *ch,
				    bool boost)
{
	struct nvgpu_tsg *tsg;
	struct nvgpu_gr_ctx *gr_ctx;
	struct gk20a *g = ch->g;
	struct nvgpu_mem *mem;
	int err = 0;

	tsg = nvgpu_tsg_from_ch(ch);
	if (tsg == NULL) {
		return -EINVAL;
	}

	gr_ctx = tsg->gr_ctx;
	nvgpu_gr_ctx_set_boosted_ctx(gr_ctx, boost);
	mem = nvgpu_gr_ctx_get_ctx_mem(gr_ctx);

	err = nvgpu_channel_disable_tsg(g, ch);
	if (err != 0) {
		return err;
	}

	err = nvgpu_preempt_channel(g, ch);
	if (err != 0) {
		goto out;
	}

	if (g->ops.gr.ctxsw_prog.set_pmu_options_boost_clock_frequencies !=
			NULL) {
		g->ops.gr.ctxsw_prog.set_pmu_options_boost_clock_frequencies(g,
			mem, nvgpu_gr_ctx_get_boosted_ctx(gr_ctx));
	} else {
		err = -ENOSYS;
		goto out;
	}
	/* no error at this point */
	err = nvgpu_channel_enable_tsg(g, ch);
	if (err != 0) {
		nvgpu_err(g, "failed to enable channel/TSG");
	}
	return err;

out:
	/*
	 * control reaches here if preempt failed or
	 * set_pmu_options_boost_clock_frequencies fn pointer is NULL.
	 * Propagate preempt failure err or err for
	 * set_pmu_options_boost_clock_frequencies fn pointer being NULL
	 */
	if (nvgpu_channel_enable_tsg(g, ch) != 0) {
		/* ch might not be bound to tsg anymore */
		nvgpu_err(g, "failed to enable channel/TSG");
	}
	return err;
}
#endif
