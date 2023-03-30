/*
 * GV11b GPU GR
 *
 * Copyright (c) 2016-2022, NVIDIA CORPORATION.  All rights reserved.
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

#include <nvgpu/dma.h>
#include <nvgpu/log.h>
#include <nvgpu/debug.h>
#include <nvgpu/enabled.h>
#include <nvgpu/fuse.h>
#include <nvgpu/debugger.h>
#include <nvgpu/error_notifier.h>
#include <nvgpu/io.h>
#include <nvgpu/utils.h>
#include <nvgpu/bitops.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/regops.h>
#include <nvgpu/gr/ctx.h>
#include <nvgpu/gr/config.h>
#include <nvgpu/gr/gr.h>
#include <nvgpu/gr/gr_instances.h>
#include <nvgpu/gr/warpstate.h>
#include <nvgpu/channel.h>
#include <nvgpu/engines.h>
#include <nvgpu/engine_status.h>
#include <nvgpu/fbp.h>
#include <nvgpu/nvgpu_err.h>
#include <nvgpu/string.h>

#include "gr_pri_gk20a.h"
#include "gr_pri_gv11b.h"
#include "gr_gk20a.h"
#include "gr_gp10b.h"
#include "gr_gv11b.h"

#include "common/gr/gr_priv.h"

#include <nvgpu/hw/gv11b/hw_gr_gv11b.h>
#include <nvgpu/hw/gv11b/hw_proj_gv11b.h>
#include <nvgpu/hw/gv11b/hw_perf_gv11b.h>

#define EGPC_PRI_BASE        0x580000U
#define EGPC_PRI_SHARED_BASE 0x480000U

#define PRI_BROADCAST_FLAGS_SMPC  BIT32(17)

void gr_gv11b_set_alpha_circular_buffer_size(struct gk20a *g, u32 data)
{
	struct nvgpu_gr *gr = nvgpu_gr_get_cur_instance_ptr(g);
	u32 gpc_index, ppc_index, stride, val;
	u32 pd_ab_max_output;
	u32 alpha_cb_size = data * 4U;
	u32 alpha_cb_size_max = g->ops.gr.init.get_alpha_cb_size(g,
		nvgpu_gr_config_get_tpc_count(gr->config));

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

	gk20a_writel(g, gr_pd_ab_dist_cfg1_r(),
		gr_pd_ab_dist_cfg1_max_output_f(pd_ab_max_output) |
		gr_pd_ab_dist_cfg1_max_batches_init_f());

	for (gpc_index = 0;
	     gpc_index < nvgpu_gr_config_get_gpc_count(gr->config);
	     gpc_index++) {
		stride = proj_gpc_stride_v() * gpc_index;

		for (ppc_index = 0;
		     ppc_index < nvgpu_gr_config_get_gpc_ppc_count(gr->config, gpc_index);
		     ppc_index++) {

			val = gk20a_readl(g, gr_gpc0_ppc0_cbm_alpha_cb_size_r() +
				stride +
				proj_ppc_in_gpc_stride_v() * ppc_index);

			val = set_field(val, gr_gpc0_ppc0_cbm_alpha_cb_size_v_m(),
					gr_gpc0_ppc0_cbm_alpha_cb_size_v_f(alpha_cb_size *
						nvgpu_gr_config_get_pes_tpc_count(gr->config, gpc_index, ppc_index)));

			gk20a_writel(g, gr_gpc0_ppc0_cbm_alpha_cb_size_r() +
				stride +
				proj_ppc_in_gpc_stride_v() * ppc_index, val);
		}
	}
}

void gr_gv11b_set_circular_buffer_size(struct gk20a *g, u32 data)
{
	struct nvgpu_gr *gr = nvgpu_gr_get_cur_instance_ptr(g);
	u32 gpc_index, ppc_index, stride, val;
	u32 cb_size_steady = data * 4U, cb_size;
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
		stride = proj_gpc_stride_v() * gpc_index;

		for (ppc_index = 0;
		     ppc_index < nvgpu_gr_config_get_gpc_ppc_count(gr->config, gpc_index);
		     ppc_index++) {

			val = gk20a_readl(g, gr_gpc0_ppc0_cbm_beta_cb_size_r() +
				stride +
				proj_ppc_in_gpc_stride_v() * ppc_index);

			val = set_field(val,
				gr_gpc0_ppc0_cbm_beta_cb_size_v_m(),
				gr_gpc0_ppc0_cbm_beta_cb_size_v_f(cb_size *
					nvgpu_gr_config_get_pes_tpc_count(gr->config,
						gpc_index, ppc_index)));

			gk20a_writel(g, gr_gpc0_ppc0_cbm_beta_cb_size_r() +
				stride +
				proj_ppc_in_gpc_stride_v() * ppc_index, val);

			gk20a_writel(g, proj_ppc_in_gpc_stride_v() * ppc_index +
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

static void gr_gv11b_dump_gr_per_sm_regs(struct gk20a *g,
			struct nvgpu_debug_context *o,
			u32 gpc, u32 tpc, u32 sm, u32 offset)
{

	gk20a_debug_output(o,
		"NV_PGRAPH_PRI_GPC%d_TPC%d_SM%d_HWW_WARP_ESR: 0x%x",
		gpc, tpc, sm, gk20a_readl(g,
		gr_gpc0_tpc0_sm0_hww_warp_esr_r() + offset));

	gk20a_debug_output(o,
		"NV_PGRAPH_PRI_GPC%d_TPC%d_SM%d_HWW_WARP_ESR_REPORT_MASK: 0x%x",
		gpc, tpc, sm, gk20a_readl(g,
		gr_gpc0_tpc0_sm0_hww_warp_esr_report_mask_r() + offset));

	gk20a_debug_output(o,
		"NV_PGRAPH_PRI_GPC%d_TPC%d_SM%d_HWW_GLOBAL_ESR: 0x%x",
		gpc, tpc, sm, gk20a_readl(g,
		gr_gpc0_tpc0_sm0_hww_global_esr_r() + offset));

	gk20a_debug_output(o,
		"NV_PGRAPH_PRI_GPC%d_TPC%d_SM%d_HWW_GLOBAL_ESR_REPORT_MASK: 0x%x",
		gpc, tpc, sm, gk20a_readl(g,
		gr_gpc0_tpc0_sm0_hww_global_esr_report_mask_r() + offset));

	gk20a_debug_output(o,
		"NV_PGRAPH_PRI_GPC%d_TPC%d_SM%d_DBGR_CONTROL0: 0x%x",
		gpc, tpc, sm, gk20a_readl(g,
		gr_gpc0_tpc0_sm0_dbgr_control0_r() + offset));

	gk20a_debug_output(o,
		"NV_PGRAPH_PRI_GPC%d_TPC%d_SM%d_DBGR_STATUS0: 0x%x",
		gpc, tpc, sm, gk20a_readl(g,
		gr_gpc0_tpc0_sm0_dbgr_status0_r() + offset));
}

static void gr_gv11b_dump_gr_sm_regs(struct gk20a *g,
			   struct nvgpu_debug_context *o)
{
	u32 gpc, tpc, sm, sm_per_tpc;
	u32 gpc_offset, tpc_offset, offset;
	struct nvgpu_gr *gr = nvgpu_gr_get_cur_instance_ptr(g);

	gk20a_debug_output(o,
		"NV_PGRAPH_PRI_GPCS_TPCS_SMS_HWW_GLOBAL_ESR_REPORT_MASK: 0x%x",
		gk20a_readl(g,
		gr_gpcs_tpcs_sms_hww_global_esr_report_mask_r()));
	gk20a_debug_output(o,
		"NV_PGRAPH_PRI_GPCS_TPCS_SMS_HWW_WARP_ESR_REPORT_MASK: 0x%x",
		gk20a_readl(g, gr_gpcs_tpcs_sms_hww_warp_esr_report_mask_r()));
	gk20a_debug_output(o,
		"NV_PGRAPH_PRI_GPCS_TPCS_SMS_HWW_GLOBAL_ESR: 0x%x",
		gk20a_readl(g, gr_gpcs_tpcs_sms_hww_global_esr_r()));
	gk20a_debug_output(o,
		"NV_PGRAPH_PRI_GPCS_TPCS_SMS_DBGR_CONTROL0: 0x%x",
		gk20a_readl(g, gr_gpcs_tpcs_sms_dbgr_control0_r()));
	gk20a_debug_output(o,
		"NV_PGRAPH_PRI_GPCS_TPCS_SMS_DBGR_STATUS0: 0x%x",
		gk20a_readl(g, gr_gpcs_tpcs_sms_dbgr_status0_r()));
	gk20a_debug_output(o,
		"NV_PGRAPH_PRI_GPCS_TPCS_SMS_DBGR_BPT_PAUSE_MASK_0: 0x%x",
		gk20a_readl(g, gr_gpcs_tpcs_sms_dbgr_bpt_pause_mask_0_r()));
	gk20a_debug_output(o,
		"NV_PGRAPH_PRI_GPCS_TPCS_SMS_DBGR_BPT_PAUSE_MASK_1: 0x%x",
		gk20a_readl(g, gr_gpcs_tpcs_sms_dbgr_bpt_pause_mask_1_r()));

	sm_per_tpc = nvgpu_get_litter_value(g, GPU_LIT_NUM_SM_PER_TPC);
	for (gpc = 0; gpc < nvgpu_gr_config_get_gpc_count(gr->config); gpc++) {
		gpc_offset = nvgpu_gr_gpc_offset(g, gpc);

		for (tpc = 0;
		     tpc < nvgpu_gr_config_get_gpc_tpc_count(gr->config, gpc);
		     tpc++) {
			tpc_offset = nvgpu_gr_tpc_offset(g, tpc);

			for (sm = 0; sm < sm_per_tpc; sm++) {
				offset = gpc_offset + tpc_offset +
					nvgpu_gr_sm_offset(g, sm);

				gr_gv11b_dump_gr_per_sm_regs(g, o,
					gpc, tpc, sm, offset);
			}
		}
	}
}

int gr_gv11b_dump_gr_status_regs(struct gk20a *g,
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
	gk20a_debug_output(o, "NV_PGRAPH_PRI_FE_TPC_FS(0): 0x%x",
		gk20a_readl(g, gr_fe_tpc_fs_r(0)));
	gk20a_debug_output(o, "NV_PGRAPH_PRI_CWD_GPC_TPC_ID: 0x%x",
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
	gk20a_debug_output(o, "NV_PGRAPH_PRI_FECS_HOST_INT_ENABLE : 0x%x",
		gk20a_readl(g, gr_fecs_host_int_enable_r()));
	gk20a_debug_output(o, "NV_PGRAPH_PRI_FECS_HOST_INT_STATUS : 0x%x",
		gk20a_readl(g, gr_fecs_host_int_status_r()));
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

	gr_gv11b_dump_gr_sm_regs(g, o);

	return 0;
}

#ifdef CONFIG_NVGPU_TEGRA_FUSE
void gr_gv11b_set_gpc_tpc_mask(struct gk20a *g, u32 gpc_index)
{
	u32 fuse_val;
	struct nvgpu_gr *gr = nvgpu_gr_get_cur_instance_ptr(g);

	if (nvgpu_gr_config_get_gpc_tpc_mask(gr->config, gpc_index) == 0U) {
		return;
	}

	/*
	 * For s/w value nvgpu_gr_config_get_gpc_tpc_mask(gr->config, gpc_index), bit value 1 indicates
	 * corresponding TPC is enabled. But for h/w fuse register, bit value 1
	 * indicates corresponding TPC is disabled.
	 * So we need to flip the bits and ensure we don't write to bits greater
	 * than TPC count
	 */
	fuse_val = nvgpu_gr_config_get_gpc_tpc_mask(gr->config, gpc_index);
	fuse_val = ~fuse_val;
	fuse_val = fuse_val & 0xfU; /* tpc0_disable fuse is only 4-bit wide */

	nvgpu_tegra_fuse_write_bypass(g, 0x1);
	nvgpu_tegra_fuse_write_access_sw(g, 0x0);

	nvgpu_tegra_fuse_write_opt_gpu_tpc0_disable(g, fuse_val);
}
#endif

#ifdef CONFIG_NVGPU_DEBUGGER
static int gr_gv11b_handle_warp_esr_error_mmu_nack(struct gk20a *g,
	u32 gpc, u32 tpc, u32 sm,
	u32 warp_esr_error,
	struct nvgpu_channel *fault_ch)
{
	u32 offset;
	int err = 0;

	fault_ch = nvgpu_channel_get(fault_ch);
	if (fault_ch != NULL) {
		if (!fault_ch->mmu_nack_handled) {
			/* recovery is not done for the channel implying mmu
			 * nack interrupt is serviced before mmu fault. Force
			 * recovery by returning an error. Also indicate we
			 * should skip a second recovery.
			 */
			fault_ch->mmu_nack_handled = true;
			err = -EFAULT;
		}
	}
	/* else mmu fault is serviced first and channel is closed */

	/* do not release reference to ch as we do not want userspace to close
	 * this channel on recovery. Otherwise mmu fault handler will enter
	 * recovery path even if channel is invalid. We want to explicitly check
	 * for teardown value in mmu fault handler.
	 */
	if (err == 0 && fault_ch != NULL) {
		nvgpu_channel_put(fault_ch);
	}

	/* clear interrupt */
	offset = nvgpu_gr_gpc_offset(g, gpc) +
			nvgpu_gr_tpc_offset(g, tpc) +
			nvgpu_gr_sm_offset(g, sm);
	nvgpu_writel(g,
		gr_gpc0_tpc0_sm0_hww_warp_esr_r() + offset, 0);

	nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gpu_dbg,
			"ESR %s(0x%x)",
			"MMU NACK ERROR",
			warp_esr_error);
	return err;
}

bool gv11b_gr_check_warp_esr_error(struct gk20a *g, u32 warp_esr_error)
{
	u32 index = 0U;
	bool esr_err = false;

	struct warp_esr_error_table_s {
		u32 error_value;
		const char *error_name;
	};

	struct warp_esr_error_table_s warp_esr_error_table[] = {
		{ gr_gpc0_tpc0_sm0_hww_warp_esr_error_stack_error_f(),
				"STACK ERROR"},
		{ gr_gpc0_tpc0_sm0_hww_warp_esr_error_api_stack_error_f(),
				"API STACK ERROR"},
		{ gr_gpc0_tpc0_sm0_hww_warp_esr_error_pc_wrap_f(),
				"PC WRAP ERROR"},
		{ gr_gpc0_tpc0_sm0_hww_warp_esr_error_misaligned_pc_f(),
				"MISALIGNED PC ERROR"},
		{ gr_gpc0_tpc0_sm0_hww_warp_esr_error_pc_overflow_f(),
				"PC OVERFLOW ERROR"},
		{ gr_gpc0_tpc0_sm0_hww_warp_esr_error_misaligned_reg_f(),
				"MISALIGNED REG ERROR"},
		{ gr_gpc0_tpc0_sm0_hww_warp_esr_error_illegal_instr_encoding_f(),
				"ILLEGAL INSTRUCTION ENCODING ERROR"},
		{ gr_gpc0_tpc0_sm0_hww_warp_esr_error_illegal_instr_param_f(),
				"ILLEGAL INSTRUCTION PARAM ERROR"},
		{ gr_gpc0_tpc0_sm0_hww_warp_esr_error_oor_reg_f(),
				"OOR REG ERROR"},
		{ gr_gpc0_tpc0_sm0_hww_warp_esr_error_oor_addr_f(),
				"OOR ADDR ERROR"},
		{ gr_gpc0_tpc0_sm0_hww_warp_esr_error_misaligned_addr_f(),
				"MISALIGNED ADDR ERROR"},
		{ gr_gpc0_tpc0_sm0_hww_warp_esr_error_invalid_addr_space_f(),
				"INVALID ADDR SPACE ERROR"},
		{ gr_gpc0_tpc0_sm0_hww_warp_esr_error_invalid_const_addr_ldc_f(),
				"INVALID ADDR LDC ERROR"},
		{ gr_gpc0_tpc0_sm0_hww_warp_esr_error_stack_overflow_f(),
				"STACK OVERFLOW ERROR"},
		{ gr_gpc0_tpc0_sm0_hww_warp_esr_error_mmu_fault_f(),
				"MMU FAULT ERROR"},
		{ gr_gpc0_tpc0_sm0_hww_warp_esr_error_tex_format_f(),
				"TEX FORMAT ERROR"},
		{ gr_gpc0_tpc0_sm0_hww_warp_esr_error_tex_layout_f(),
				"TEX LAYOUT ERROR"},
	};

	for (index = 0; index < ARRAY_SIZE(warp_esr_error_table); index++) {
		if (warp_esr_error_table[index].error_value == warp_esr_error) {
			esr_err = true;
			nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gpu_dbg,
				"WARP_ESR %s(0x%x)",
				warp_esr_error_table[index].error_name,
				esr_err);
			break;
		}
	}

	return esr_err;
}

static int gr_gv11b_handle_all_warp_esr_errors(struct gk20a *g,
						u32 gpc, u32 tpc, u32 sm,
						u32 warp_esr_error,
						struct nvgpu_channel *fault_ch)
{
	struct nvgpu_tsg *tsg;
	u32 offset = 0U;
	bool is_esr_error = false;

	/*
	 * Check for an esr error
	 */
	is_esr_error = g->ops.gr.check_warp_esr_error(g, warp_esr_error);
	if (!is_esr_error) {
		nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gpu_dbg,
			"No ESR error, Skip RC recovery and Trigger CILP");
		return 0;
	}

	if (fault_ch != NULL) {
		tsg = nvgpu_tsg_check_and_get_from_id(g, fault_ch->tsgid);
		if (tsg == NULL) {
			nvgpu_err(g, "fault ch %u not found", fault_ch->chid);
			goto clear_intr;
		}

		/*
		 * Check SET_EXCEPTION_TYPE_MASK is being set.
		 * If set, skip the recovery and trigger CILP
		 * If not set, trigger the recovery.
		 */
		if ((tsg->sm_exception_mask_type &
			NVGPU_SM_EXCEPTION_TYPE_MASK_FATAL) ==
				NVGPU_SM_EXCEPTION_TYPE_MASK_FATAL) {
			nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gpu_dbg,
				"SM Exception Type Mask set %d,"
				"skip recovery",
				tsg->sm_exception_mask_type);
			return 0;
		}

		nvgpu_tsg_set_error_notifier(g, tsg,
			NVGPU_ERR_NOTIFIER_GR_EXCEPTION);
	}

clear_intr:
	/* clear interrupt */
	offset = nvgpu_gr_gpc_offset(g, gpc) +
			nvgpu_gr_tpc_offset(g, tpc) +
			nvgpu_gr_sm_offset(g, sm);
	nvgpu_writel(g,
		gr_gpc0_tpc0_sm0_hww_warp_esr_r() + offset, 0);

	/* return error so that recovery is triggered by gk20a_gr_isr() */
	return -EFAULT;
}
#endif

/* @brief pre-process work on the SM exceptions to determine if we clear them or not.
 *
 * On Pascal, if we are in CILP preemtion mode, preempt the channel and handle errors with special processing
 */
int gr_gv11b_pre_process_sm_exception(struct gk20a *g,
		u32 gpc, u32 tpc, u32 sm, u32 global_esr, u32 warp_esr,
		bool sm_debugger_attached, struct nvgpu_channel *fault_ch,
		bool *early_exit, bool *ignore_debugger)
{
#ifdef CONFIG_NVGPU_DEBUGGER
	int ret;
	bool cilp_enabled = false;
	u32 warp_esr_error = gr_gpc0_tpc0_sm0_hww_warp_esr_error_v(warp_esr);
	struct nvgpu_tsg *tsg;

	*early_exit = false;
	*ignore_debugger = false;

	/*
	 * We don't need to trigger CILP in case of MMU_NACK
	 * So just handle MMU_NACK and return
	 */
	if (warp_esr_error == gr_gpc0_tpc0_sm0_hww_warp_esr_error_mmu_nack_f()) {
		return gr_gv11b_handle_warp_esr_error_mmu_nack(g, gpc, tpc, sm,
				warp_esr_error, fault_ch);
	}

	/*
	 * Proceed to trigger CILP preemption if the return value
	 * from this function is zero, else proceed to recovery
	 */
	ret = gr_gv11b_handle_all_warp_esr_errors(g, gpc, tpc, sm,
				warp_esr_error, fault_ch);
	if (ret != 0) {
		return ret;
	}

	if (fault_ch != NULL) {
		tsg = nvgpu_tsg_from_ch(fault_ch);
		if (tsg == NULL) {
			return -EINVAL;
		}

		cilp_enabled =
			(nvgpu_gr_ctx_get_compute_preemption_mode(tsg->gr_ctx) ==
				NVGPU_PREEMPTION_MODE_COMPUTE_CILP);
	}

	nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gpu_dbg,
			"SM Exception received on gpc %d tpc %d sm %d = 0x%08x",
			gpc, tpc, sm, global_esr);

	if (cilp_enabled && sm_debugger_attached) {
		u32 global_mask = 0, dbgr_control0, global_esr_copy;
		u32 offset = nvgpu_gr_gpc_offset(g, gpc) +
				nvgpu_gr_tpc_offset(g, tpc) +
				nvgpu_gr_sm_offset(g, sm);

		if ((global_esr &
		     gr_gpc0_tpc0_sm0_hww_global_esr_bpt_int_pending_f()) != 0U) {
			gk20a_writel(g, gr_gpc0_tpc0_sm0_hww_global_esr_r() + offset,
					gr_gpc0_tpc0_sm0_hww_global_esr_bpt_int_pending_f());
		}

		if ((global_esr &
		     gr_gpc0_tpc0_sm0_hww_global_esr_single_step_complete_pending_f()) != 0U) {
			gk20a_writel(g, gr_gpc0_tpc0_sm0_hww_global_esr_r() + offset,
					gr_gpc0_tpc0_sm0_hww_global_esr_single_step_complete_pending_f());
		}

		global_mask = gr_gpc0_tpc0_sm0_hww_global_esr_multiple_warp_errors_pending_f() |
			gr_gpc0_tpc0_sm0_hww_global_esr_bpt_pause_pending_f();

		if (warp_esr != 0U || (global_esr & global_mask) != 0U) {
			*ignore_debugger = true;

			nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gpu_dbg,
				"CILP: starting wait for LOCKED_DOWN on "
				"gpc %d tpc %d sm %d",
				gpc, tpc, sm);

			if (nvgpu_dbg_gpu_broadcast_stop_trigger(fault_ch)) {
				nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gpu_dbg,
					"CILP: Broadcasting STOP_TRIGGER from "
					"gpc %d tpc %d sm %d",
					gpc, tpc, sm);
				g->ops.gr.suspend_all_sms(g,
						global_mask, false);

				nvgpu_dbg_gpu_clear_broadcast_stop_trigger(fault_ch);
			} else {
				nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gpu_dbg,
					"CILP: STOP_TRIGGER from "
					"gpc %d tpc %d sm %d",
					gpc, tpc, sm);
				g->ops.gr.suspend_single_sm(g,
					gpc, tpc, sm, global_mask, true);
			}

			/* reset the HWW errors after locking down */
			global_esr_copy = g->ops.gr.intr.get_sm_hww_global_esr(g,
							gpc, tpc, sm);
			g->ops.gr.intr.clear_sm_hww(g,
					gpc, tpc, sm, global_esr_copy);
			nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gpu_dbg,
					"CILP: HWWs cleared for "
					"gpc %d tpc %d sm %d",
					gpc, tpc, sm);

			nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gpu_dbg, "CILP: Setting CILP preempt pending");
			ret = gr_gp10b_set_cilp_preempt_pending(g, fault_ch);
			if (ret != 0) {
				nvgpu_err(g, "CILP: error while setting CILP preempt pending!");
				return ret;
			}

			dbgr_control0 = gk20a_readl(g, gr_gpc0_tpc0_sm0_dbgr_control0_r() + offset);
			if ((dbgr_control0 &
			     gr_gpc0_tpc0_sm0_dbgr_control0_single_step_mode_enable_f()) != 0U) {
				nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gpu_dbg,
					"CILP: clearing SINGLE_STEP_MODE "
					"before resume for gpc %d tpc %d sm %d",
						gpc, tpc, sm);
				dbgr_control0 = set_field(dbgr_control0,
						gr_gpc0_tpc0_sm0_dbgr_control0_single_step_mode_m(),
						gr_gpc0_tpc0_sm0_dbgr_control0_single_step_mode_disable_f());
				gk20a_writel(g, gr_gpc0_tpc0_sm0_dbgr_control0_r() + offset, dbgr_control0);
			}

			nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gpu_dbg,
				"CILP: resume for gpc %d tpc %d sm %d",
					gpc, tpc, sm);
			g->ops.gr.resume_single_sm(g, gpc, tpc, sm);

			*ignore_debugger = true;
			nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gpu_dbg,
				"CILP: All done on gpc %d, tpc %d sm %d",
				gpc, tpc, sm);
		}

		*early_exit = true;
	}
#endif
	return 0;
}

static void gv11b_gr_sm_stop_trigger_enable(struct gk20a *g)
{
	u32 dbgr_control0;
	u32 gpc, tpc, sm, gpc_offset, tpc_offset, offset;
	struct nvgpu_gr *gr = nvgpu_gr_get_cur_instance_ptr(g);
	u32 sm_per_tpc = nvgpu_get_litter_value(g, GPU_LIT_NUM_SM_PER_TPC);

	/*
	 * dbgr_control0 value can be different for different SMs.
	 *
	 * SINGLE_STEP_MODE: Debugger supports single stepping at warp level
	 * which is implemented by resuming with required PAUSE_MASK and
	 * setting SINGLE_STEP_MODE only for the requested SM.
	 */
	for (gpc = 0U; gpc < nvgpu_gr_config_get_gpc_count(gr->config); gpc++) {
		gpc_offset = nvgpu_gr_gpc_offset(g, gpc);
		for (tpc = 0U;
		     tpc < nvgpu_gr_config_get_gpc_tpc_count(gr->config, gpc);
		     tpc++) {
			tpc_offset = nvgpu_gr_tpc_offset(g, tpc);
			for (sm = 0U; sm < sm_per_tpc; sm++) {
				offset = gpc_offset + tpc_offset +
						nvgpu_gr_sm_offset(g, sm);
				dbgr_control0 = gk20a_readl(g,
					gr_gpc0_tpc0_sm0_dbgr_control0_r() + offset);
				dbgr_control0 |=
					gr_gpc0_tpc0_sm0_dbgr_control0_stop_trigger_enable_f();
				nvgpu_writel(g,
					gr_gpc0_tpc0_sm0_dbgr_control0_r() + offset, dbgr_control0);
				nvgpu_log(g, gpu_dbg_intr | gpu_dbg_gpu_dbg,
					"gpc(%d) tpc(%d) sm(%d) assert stop trigger "
					"dbgr_control0: 0x%08x, "
					, gpc, tpc, sm, dbgr_control0);
			}
		}
	}
}

void gv11b_gr_bpt_reg_info(struct gk20a *g, struct nvgpu_warpstate *w_state)
{
	/* Check if we have at least one valid warp
	 * get paused state on maxwell
	 */
	struct nvgpu_gr *gr = nvgpu_gr_get_cur_instance_ptr(g);
	u32 gpc, tpc, sm, sm_id;
	u32 offset;
	u64 warps_valid = 0, warps_paused = 0, warps_trapped = 0;
	u32 no_of_sm = nvgpu_gr_config_get_no_of_sm(gr->config);

	for (sm_id = 0; sm_id < no_of_sm; sm_id++) {
		struct nvgpu_sm_info *sm_info =
			nvgpu_gr_config_get_sm_info(gr->config, sm_id);
		gpc = nvgpu_gr_config_get_sm_info_gpc_index(sm_info);
		tpc = nvgpu_gr_config_get_sm_info_tpc_index(sm_info);
		sm = nvgpu_gr_config_get_sm_info_sm_index(sm_info);

		offset = nvgpu_gr_gpc_offset(g, gpc) +
			 nvgpu_gr_tpc_offset(g, tpc) +
			 nvgpu_gr_sm_offset(g, sm);

		/* 64 bit read */
		warps_valid = (u64)gk20a_readl(g,
				gr_gpc0_tpc0_sm0_warp_valid_mask_1_r() +
				offset) << 32;
		warps_valid |= gk20a_readl(g,
				gr_gpc0_tpc0_sm0_warp_valid_mask_0_r() +
				offset);

		/* 64 bit read */
		warps_paused = (u64)gk20a_readl(g,
				gr_gpc0_tpc0_sm0_dbgr_bpt_pause_mask_1_r() +
				offset) << 32;
		warps_paused |= gk20a_readl(g,
				gr_gpc0_tpc0_sm0_dbgr_bpt_pause_mask_0_r() +
				offset);

		/* 64 bit read */
		warps_trapped = (u64)gk20a_readl(g,
				gr_gpc0_tpc0_sm0_dbgr_bpt_trap_mask_1_r() +
				offset) << 32;
		warps_trapped |= gk20a_readl(g,
				gr_gpc0_tpc0_sm0_dbgr_bpt_trap_mask_0_r() +
				offset);

		w_state[sm_id].valid_warps[0] = warps_valid;
		w_state[sm_id].trapped_warps[0] = warps_trapped;
		w_state[sm_id].paused_warps[0] = warps_paused;
	}


	/* Only for debug purpose */
	for (sm_id = 0; sm_id < no_of_sm; sm_id++) {
		nvgpu_log_fn(g, "w_state[%d].valid_warps[0]: %llx",
					sm_id, w_state[sm_id].valid_warps[0]);
		nvgpu_log_fn(g, "w_state[%d].valid_warps[1]: %llx",
					sm_id, w_state[sm_id].valid_warps[1]);

		nvgpu_log_fn(g, "w_state[%d].trapped_warps[0]: %llx",
					sm_id, w_state[sm_id].trapped_warps[0]);
		nvgpu_log_fn(g, "w_state[%d].trapped_warps[1]: %llx",
					sm_id, w_state[sm_id].trapped_warps[1]);

		nvgpu_log_fn(g, "w_state[%d].paused_warps[0]: %llx",
					sm_id, w_state[sm_id].paused_warps[0]);
		nvgpu_log_fn(g, "w_state[%d].paused_warps[1]: %llx",
					sm_id, w_state[sm_id].paused_warps[1]);
	}
}

int gv11b_gr_set_sm_debug_mode(struct gk20a *g,
	struct nvgpu_channel *ch, u64 sms, bool enable)
{
	struct nvgpu_dbg_reg_op *ops;
	unsigned int i = 0, sm_id;
	int err;
	struct nvgpu_tsg *tsg = nvgpu_tsg_from_ch(ch);
	u32 flags = NVGPU_REG_OP_FLAG_MODE_ALL_OR_NONE;
	struct nvgpu_gr *gr = nvgpu_gr_get_cur_instance_ptr(g);
	u32 no_of_sm = nvgpu_gr_config_get_no_of_sm(gr->config);

	if (tsg == NULL) {
		nvgpu_err(g, "gv11b_gr_set_sm_debug_mode failed=>tsg NULL");
		return -EINVAL;
	}

	ops = nvgpu_kcalloc(g, no_of_sm, sizeof(*ops));
	if (ops == NULL) {
		return -ENOMEM;
	}
	for (sm_id = 0; sm_id < no_of_sm; sm_id++) {
		u32 gpc, tpc, sm;
		u32 reg_offset, reg_mask, reg_val;
		struct nvgpu_sm_info *sm_info;

		if ((sms & BIT64(sm_id)) == 0ULL) {
			continue;
		}

#ifdef CONFIG_NVGPU_SM_DIVERSITY
		if (nvgpu_gr_ctx_get_sm_diversity_config(tsg->gr_ctx) ==
				NVGPU_DEFAULT_SM_DIVERSITY_CONFIG) {
			sm_info =
				nvgpu_gr_config_get_sm_info(
					gr->config, sm_id);
		} else {
			sm_info =
				nvgpu_gr_config_get_redex_sm_info(
					gr->config, sm_id);
		}
#else
		sm_info = nvgpu_gr_config_get_sm_info(gr->config, sm_id);
#endif
		gpc = nvgpu_gr_config_get_sm_info_gpc_index(sm_info);
		if (g->ops.gr.init.get_nonpes_aware_tpc != NULL) {
			tpc = g->ops.gr.init.get_nonpes_aware_tpc(g,
				nvgpu_gr_config_get_sm_info_gpc_index(sm_info),
				nvgpu_gr_config_get_sm_info_tpc_index(sm_info),
					gr->config);
		} else {
			tpc = nvgpu_gr_config_get_sm_info_tpc_index(sm_info);
		}
		sm = nvgpu_gr_config_get_sm_info_sm_index(sm_info);

		reg_offset = nvgpu_gr_gpc_offset(g, gpc) +
				nvgpu_gr_tpc_offset(g, tpc) +
				nvgpu_gr_sm_offset(g, sm);

		ops[i].op = REGOP(WRITE_32);
		ops[i].type = REGOP(TYPE_GR_CTX);
		ops[i].offset = gr_gpc0_tpc0_sm0_dbgr_control0_r() + reg_offset;

		reg_mask = 0;
		reg_val = 0;
		if (enable) {
			nvgpu_log(g, gpu_dbg_gpu_dbg,
				"SM:%d debuggger mode ON", sm);
			reg_mask |=
			 gr_gpc0_tpc0_sm0_dbgr_control0_debugger_mode_m();
			reg_val |=
			 gr_gpc0_tpc0_sm0_dbgr_control0_debugger_mode_on_f();
		} else {
			nvgpu_log(g, gpu_dbg_gpu_dbg,
				"SM:%d debuggger mode Off", sm);
			reg_mask |=
			 gr_gpc0_tpc0_sm0_dbgr_control0_debugger_mode_m();
			reg_val |=
			 gr_gpc0_tpc0_sm0_dbgr_control0_debugger_mode_off_f();
		}

		ops[i].and_n_mask_lo = reg_mask;
		ops[i].value_lo = reg_val;
		i++;
	}

	err = gr_gk20a_exec_ctx_ops(tsg, ops, i, i, 0, &flags);
	if (err != 0) {
		nvgpu_err(g, "Failed to access register");
	}
	nvgpu_kfree(g, ops);
	return err;
}

static bool gv11b_gr_single_sm_debugger_attached(struct gk20a *g, u32 gpc,
				u32 tpc, u32 sm)
{
	u32 debugger_mode;
	u32 dbgr_control0;
	u32 offset = nvgpu_gr_gpc_offset(g, gpc) +
			nvgpu_gr_tpc_offset(g, tpc) +
			nvgpu_gr_sm_offset(g, sm);

	dbgr_control0 = nvgpu_readl(g, gr_gpc0_tpc0_sm0_dbgr_control0_r() +
					offset);

	debugger_mode =
		gr_gpc0_tpc0_sm0_dbgr_control0_debugger_mode_v(dbgr_control0);

	nvgpu_log(g, gpu_dbg_intr | gpu_dbg_gpu_dbg,
			"gpc(%d) tpc(%d) sm(%d) debugger mode: %d",
			gpc, tpc, sm, debugger_mode);
	if (debugger_mode ==
			gr_gpc0_tpc0_sm0_dbgr_control0_debugger_mode_on_v()) {
		return true;
	}

	return false;
}

bool gv11b_gr_sm_debugger_attached(struct gk20a *g)
{
	u32 gpc, tpc, sm;
	struct nvgpu_gr *gr = nvgpu_gr_get_cur_instance_ptr(g);
	u32 sm_per_tpc = nvgpu_get_litter_value(g, GPU_LIT_NUM_SM_PER_TPC);

	/* return true only if all SMs are in debug mode */
	for (gpc = 0U; gpc < nvgpu_gr_config_get_gpc_count(gr->config); gpc++) {
		for (tpc = 0U;
		     tpc < nvgpu_gr_config_get_gpc_tpc_count(gr->config, gpc);
		     tpc++) {
			for (sm = 0U; sm < sm_per_tpc; sm++) {
				if (!gv11b_gr_single_sm_debugger_attached(g,
							gpc, tpc, sm)) {
					nvgpu_log(g, gpu_dbg_gpu_dbg,
					"gpc(%d) tpc(%d) sm(%d) debugger NOT attached, "
					, gpc, tpc, sm);
					return false;
				}
			}
		}
	}
	return true;
}

void gv11b_gr_suspend_single_sm(struct gk20a *g,
		u32 gpc, u32 tpc, u32 sm,
		u32 global_esr_mask, bool check_errors)
{
	int err;
	u32 dbgr_control0;
	u32 offset = nvgpu_gr_gpc_offset(g, gpc) +
			nvgpu_gr_tpc_offset(g, tpc) +
			nvgpu_gr_sm_offset(g, sm);

	/* If all SMs are not in not in debug mode, skip suspend.
	 * Suspend (STOP_TRIGGER) will cause SM to enter trap handler however
	 * SM can enter into trap handler only if all other SMs are in debug mode.
	 * as all SMs will enter trap handler.
	 */
	if (!g->ops.gr.sm_debugger_attached(g)) {
		nvgpu_err(g,
			"SM debugger not attached, skipping suspend!");
		return;
	}

	nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gpu_dbg,
		"suspending gpc(%d) tpc(%d) sm(%d)", gpc, tpc, sm);

	/* assert stop trigger. */
	dbgr_control0 = gk20a_readl(g,
				gr_gpc0_tpc0_sm0_dbgr_control0_r() + offset);
	dbgr_control0 |= gr_gpc0_tpc0_sm0_dbgr_control0_stop_trigger_enable_f();
	gk20a_writel(g, gr_gpc0_tpc0_sm0_dbgr_control0_r() + offset,
			dbgr_control0);

	err = g->ops.gr.wait_for_sm_lock_down(g, gpc, tpc, sm,
			global_esr_mask, check_errors);
	if (err != 0) {
		nvgpu_err(g, "suspend failed for gpc(%d) tpc(%d) sm(%d)",
				gpc, tpc, sm);
		return;
	}
}

void gv11b_gr_suspend_all_sms(struct gk20a *g,
		u32 global_esr_mask, bool check_errors)
{
	struct nvgpu_gr *gr = nvgpu_gr_get_cur_instance_ptr(g);
	u32 gpc, tpc, sm;
	int err;
	u32 sm_per_tpc = nvgpu_get_litter_value(g, GPU_LIT_NUM_SM_PER_TPC);

	/* If all SMs are not in not in debug mode, skip suspend.
	 * Suspend (STOP_TRIGGER) will cause SM to enter trap handler however
	 * SM can enter into trap handler only if all other SMs are in debug mode.
	 * as all SMs will enter trap handler.
	 */

	if (!g->ops.gr.sm_debugger_attached(g)) {
		nvgpu_err(g,
			"SM debugger not attached, skipping suspend!");
		return;
	}

	nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gpu_dbg, "suspending all sms");

	gv11b_gr_sm_stop_trigger_enable(g);

	for (gpc = 0; gpc < nvgpu_gr_config_get_gpc_count(gr->config); gpc++) {
		for (tpc = 0;
		     tpc < nvgpu_gr_config_get_gpc_tpc_count(gr->config, gpc);
		     tpc++) {
			for (sm = 0; sm < sm_per_tpc; sm++) {
				err = g->ops.gr.wait_for_sm_lock_down(g,
					gpc, tpc, sm,
					global_esr_mask, check_errors);
				if (err != 0) {
					nvgpu_err(g, "suspend failed for "
						"gpc(%d) tpc(%d) sm(%d)",
						gpc, tpc, sm);
					return;
				}
			}
		}
	}
}

static void gv11b_gr_sm_stop_trigger_disable(struct gk20a *g,
		u32 gpc, u32 tpc, u32 sm)
{
	u32 dbgr_control0, dbgr_status0;
	u32 offset;

	/*
	 * The following requires some clarification. Despite the fact that both
	 * RUN_TRIGGER and STOP_TRIGGER have the word "TRIGGER" in their
	 *  names, only one is actually a trigger, and that is the STOP_TRIGGER.
	 * Merely writing a 1(_TASK) to the RUN_TRIGGER is not sufficient to
	 * resume the gpu - the _STOP_TRIGGER must explicitly be set to 0
	 * (_DISABLE) as well.

	* Advice from the arch group:  Disable the stop trigger first, as a
	* separate operation, in order to ensure that the trigger has taken
	* effect, before enabling the run trigger.
	*/

	offset = nvgpu_gr_gpc_offset(g, gpc) + nvgpu_gr_tpc_offset(g, tpc) +
			nvgpu_gr_sm_offset(g, sm);

	nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gpu_dbg,
			"resuming gpc(%d), tpc(%d), sm(%d)", gpc, tpc, sm);
	/*
	 * dbgr_control0 value can be different for different SMs.
	 *
	 * SINGLE_STEP_MODE: Debugger supports single stepping at warp level
	 * which is implemented by resuming with required PAUSE_MASK and
	 * setting SINGLE_STEP_MODE only for the requested SM.
	 */

	dbgr_control0 = nvgpu_readl(g,
			gr_gpc0_tpc0_sm0_dbgr_control0_r() + offset);
	dbgr_status0 = nvgpu_readl(g,
			gr_gpc0_tpc0_sm0_dbgr_status0_r() + offset);

	nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gpu_dbg,
			"before stop trigger disable: "
			"dbgr_control0 = 0x%x dbgr_status0: 0x%x",
			dbgr_control0, dbgr_status0);

	/*De-assert stop trigger */
	dbgr_control0 = set_field(dbgr_control0,
			gr_gpc0_tpc0_sm0_dbgr_control0_stop_trigger_m(),
			gr_gpc0_tpc0_sm0_dbgr_control0_stop_trigger_disable_f());
	gk20a_writel(g, gr_gpc0_tpc0_sm0_dbgr_control0_r() +
				offset, dbgr_control0);

	dbgr_control0 = nvgpu_readl(g,
			gr_gpc0_tpc0_sm0_dbgr_control0_r() + offset);
	dbgr_status0 = nvgpu_readl(g,
			gr_gpc0_tpc0_sm0_dbgr_status0_r() + offset);

	nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gpu_dbg,
			"before run trigger: "
			"dbgr_control0 = 0x%x dbgr_status0: 0x%x",
			dbgr_control0, dbgr_status0);
	/* Run trigger */
	dbgr_control0 |=
		gr_gpc0_tpc0_sm0_dbgr_control0_run_trigger_task_f();
	nvgpu_writel(g,
		gr_gpc0_tpc0_sm0_dbgr_control0_r() +
		offset, dbgr_control0);

	dbgr_control0 = nvgpu_readl(g,
			gr_gpc0_tpc0_sm0_dbgr_control0_r() + offset);
	dbgr_status0 = nvgpu_readl(g,
			gr_gpc0_tpc0_sm0_dbgr_status0_r() + offset);
	/* run trigger is not sticky bit. SM clears it immediately */
	nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gpu_dbg,
			"after run trigger: "
			"dbgr_control0 = 0x%x dbgr_status0: 0x%x",
			dbgr_control0, dbgr_status0);

}

void gv11b_gr_resume_single_sm(struct gk20a *g,
		u32 gpc, u32 tpc, u32 sm)
{
	if (!g->ops.gr.sm_debugger_attached(g)) {
		nvgpu_err(g,
			"SM debugger not attached, do not resume "
			"gpc(%d) tpc(%d) sm(%d)", gpc, tpc, sm);
	}

	gv11b_gr_sm_stop_trigger_disable(g, gpc, tpc, sm);
}

void gv11b_gr_resume_all_sms(struct gk20a *g)
{
	u32 gpc, tpc, sm;
	u32 sm_per_tpc = nvgpu_get_litter_value(g, GPU_LIT_NUM_SM_PER_TPC);
	struct nvgpu_gr *gr = nvgpu_gr_get_cur_instance_ptr(g);

	if (!g->ops.gr.sm_debugger_attached(g)) {
		nvgpu_err(g,
			"SM debugger not attached, do not resume all sm!");
	}

	for (gpc = 0; gpc < nvgpu_gr_config_get_gpc_count(gr->config); gpc++) {
		for (tpc = 0;
		     tpc < nvgpu_gr_config_get_gpc_tpc_count(gr->config, gpc);
		     tpc++) {
			for (sm = 0; sm < sm_per_tpc; sm++) {
				gv11b_gr_sm_stop_trigger_disable(g, gpc, tpc, sm);
			}
		}
	}
}

static void gv11b_gr_sm_dump_warp_bpt_pause_trap_mask_regs(struct gk20a *g,
					u32 offset, bool timeout)
{
	u64 warps_valid = 0, warps_paused = 0, warps_trapped = 0;
	u32 dbgr_control0 = gk20a_readl(g,
				gr_gpc0_tpc0_sm0_dbgr_control0_r() + offset);
	u32 dbgr_status0 = gk20a_readl(g,
				gr_gpc0_tpc0_sm0_dbgr_status0_r() + offset);
	/* 64 bit read */
	warps_valid =
		(u64)gk20a_readl(g, gr_gpc0_tpc0_sm0_warp_valid_mask_1_r() +
					offset) << 32;
	warps_valid |= gk20a_readl(g,
			gr_gpc0_tpc0_sm0_warp_valid_mask_0_r() + offset);

	/* 64 bit read */
	warps_paused =
		(u64)gk20a_readl(g, gr_gpc0_tpc0_sm0_dbgr_bpt_pause_mask_1_r() +
					offset) << 32;
	warps_paused |= gk20a_readl(g,
			gr_gpc0_tpc0_sm0_dbgr_bpt_pause_mask_0_r() + offset);

	/* 64 bit read */
	warps_trapped =
		(u64)gk20a_readl(g, gr_gpc0_tpc0_sm0_dbgr_bpt_trap_mask_1_r() +
					offset) << 32;
	warps_trapped |= gk20a_readl(g,
			gr_gpc0_tpc0_sm0_dbgr_bpt_trap_mask_0_r() + offset);
	if (timeout) {
		nvgpu_err(g,
			  "STATUS0=0x%x CONTROL0=0x%x VALID_MASK=0x%llx "
			  "PAUSE_MASK=0x%llx TRAP_MASK=0x%llx",
			  dbgr_status0, dbgr_control0, warps_valid,
			  warps_paused, warps_trapped);
	} else {
		nvgpu_log(g, gpu_dbg_intr | gpu_dbg_gpu_dbg,
			  "STATUS0=0x%x CONTROL0=0x%x VALID_MASK=0x%llx "
			  "PAUSE_MASK=0x%llx TRAP_MASK=0x%llx",
			  dbgr_status0, dbgr_control0, warps_valid,
			  warps_paused, warps_trapped);
	}
}

int gv11b_gr_wait_for_sm_lock_down(struct gk20a *g,
		u32 gpc, u32 tpc, u32 sm,
		u32 global_esr_mask, bool check_errors)
{
	bool locked_down;
	bool no_error_pending;
	u32 delay = POLL_DELAY_MIN_US;
#ifdef CONFIG_NVGPU_REPLAYABLE_FAULT
	bool mmu_debug_mode_enabled = g->ops.fb.is_debug_mode_enabled(g);
#endif
	u32 dbgr_status0 = 0;
	u32 warp_esr, global_esr;
	struct nvgpu_timeout timeout;
	u32 offset = nvgpu_gr_gpc_offset(g, gpc) +
			nvgpu_gr_tpc_offset(g, tpc) +
			nvgpu_gr_sm_offset(g, sm);

	nvgpu_log(g, gpu_dbg_intr | gpu_dbg_gpu_dbg,
		"GPC%d TPC%d: locking down SM%d", gpc, tpc, sm);

	nvgpu_timeout_init_cpu_timer(g, &timeout, g->poll_timeout_default);

	/* wait for the sm to lock down */
	do {
		global_esr = g->ops.gr.intr.get_sm_hww_global_esr(g, gpc, tpc, sm);
		dbgr_status0 = gk20a_readl(g,
				gr_gpc0_tpc0_sm0_dbgr_status0_r() + offset);

		warp_esr = g->ops.gr.intr.get_sm_hww_warp_esr(g, gpc, tpc, sm);

		locked_down =
		    (gr_gpc0_tpc0_sm0_dbgr_status0_locked_down_v(dbgr_status0) ==
		     gr_gpc0_tpc0_sm0_dbgr_status0_locked_down_true_v());
		no_error_pending =
			check_errors &&
			(gr_gpc0_tpc0_sm0_hww_warp_esr_error_v(warp_esr) ==
			 gr_gpc0_tpc0_sm0_hww_warp_esr_error_none_v()) &&
			((global_esr & global_esr_mask) == 0U);

		if (locked_down) {
		/*
		 * if SM reports locked down, it means that SM is idle and
		 * trapped and also that one of the these conditions are true
		 * 1) sm is nonempty and all valid warps are paused
		 * 2) sm is empty and held in trapped state due to stop trigger
		 * 3) sm is nonempty and some warps are not paused, but are
		 *    instead held at RTT due to an "active" stop trigger
		 * Check for Paused warp mask != Valid
		 * warp mask after SM reports it is locked down in order to
		 * distinguish case 1 from case 3.  When case 3 is detected,
		 * it implies a misprogrammed trap handler code, as all warps
		 * in the handler must promise to BPT.PAUSE instead of RTT
		 * whenever SR64 read in trap mode indicates stop trigger
		 * is asserted.
		 */
			gv11b_gr_sm_dump_warp_bpt_pause_trap_mask_regs(g,
						offset, false);
		}

		if (locked_down || no_error_pending) {
			nvgpu_log(g, gpu_dbg_intr | gpu_dbg_gpu_dbg,
				"GPC%d TPC%d: locked down SM%d", gpc, tpc, sm);
			return 0;
		}
#ifdef CONFIG_NVGPU_REPLAYABLE_FAULT
		if (mmu_debug_mode_enabled &&
		    g->ops.fb.handle_replayable_fault != NULL) {
			g->ops.fb.handle_replayable_fault(g);
		} else {
#endif
			/* if an mmu fault is pending and mmu debug mode is not
			 * enabled, the sm will never lock down.
			 */
			if (g->ops.mc.is_mmu_fault_pending(g)) {
				nvgpu_err(g,
					"GPC%d TPC%d: mmu fault pending,"
					" SM%d will never lock down!",
					gpc, tpc, sm);
				return -EFAULT;
			}
#ifdef CONFIG_NVGPU_REPLAYABLE_FAULT
		}
#endif

		nvgpu_usleep_range(delay, delay * 2U);
		delay = min_t(u32, delay << 1, POLL_DELAY_MAX_US);
	} while (nvgpu_timeout_expired(&timeout) == 0);

	nvgpu_err(g, "GPC%d TPC%d: timed out while trying to "
			"lock down SM%d", gpc, tpc, sm);
	gv11b_gr_sm_dump_warp_bpt_pause_trap_mask_regs(g, offset, true);

	return -ETIMEDOUT;
}

int gv11b_gr_lock_down_sm(struct gk20a *g,
			 u32 gpc, u32 tpc, u32 sm, u32 global_esr_mask,
			 bool check_errors)
{
	u32 dbgr_control0;
	u32 offset = nvgpu_gr_gpc_offset(g, gpc) + nvgpu_gr_tpc_offset(g, tpc) +
			nvgpu_gr_sm_offset(g, sm);

	nvgpu_log(g, gpu_dbg_intr | gpu_dbg_gpu_dbg,
			"GPC%d TPC%d SM%d: assert stop trigger", gpc, tpc, sm);

	/* assert stop trigger */
	dbgr_control0 =
		gk20a_readl(g, gr_gpc0_tpc0_sm0_dbgr_control0_r() + offset);
	dbgr_control0 |= gr_gpc0_tpc0_sm0_dbgr_control0_stop_trigger_enable_f();
	gk20a_writel(g,
		gr_gpc0_tpc0_sm0_dbgr_control0_r() + offset, dbgr_control0);

	return g->ops.gr.wait_for_sm_lock_down(g, gpc, tpc, sm, global_esr_mask,
			check_errors);
}

static const u32 _num_ovr_perf_regs = 20;
static u32 _ovr_perf_regs[20] = { 0, };

void gv11b_gr_init_ovr_sm_dsm_perf(void)
{
	if (_ovr_perf_regs[0] != 0U) {
		return;
	}

	_ovr_perf_regs[0]  = gr_egpc0_etpc0_sm_dsm_perf_counter_control_sel0_r();
	_ovr_perf_regs[1]  = gr_egpc0_etpc0_sm_dsm_perf_counter_control_sel1_r();
	_ovr_perf_regs[2]  = gr_egpc0_etpc0_sm_dsm_perf_counter_control0_r();
	_ovr_perf_regs[3]  = gr_egpc0_etpc0_sm_dsm_perf_counter_control1_r();
	_ovr_perf_regs[4]  = gr_egpc0_etpc0_sm_dsm_perf_counter_control2_r();
	_ovr_perf_regs[5]  = gr_egpc0_etpc0_sm_dsm_perf_counter_control3_r();
	_ovr_perf_regs[6]  = gr_egpc0_etpc0_sm_dsm_perf_counter_control4_r();
	_ovr_perf_regs[7]  = gr_egpc0_etpc0_sm_dsm_perf_counter_control5_r();
	_ovr_perf_regs[8]  = gr_egpc0_etpc0_sm_dsm_perf_counter0_control_r();
	_ovr_perf_regs[9]  = gr_egpc0_etpc0_sm_dsm_perf_counter1_control_r();
	_ovr_perf_regs[10] = gr_egpc0_etpc0_sm_dsm_perf_counter2_control_r();
	_ovr_perf_regs[11] = gr_egpc0_etpc0_sm_dsm_perf_counter3_control_r();
	_ovr_perf_regs[12] = gr_egpc0_etpc0_sm_dsm_perf_counter4_control_r();
	_ovr_perf_regs[13] = gr_egpc0_etpc0_sm_dsm_perf_counter5_control_r();
	_ovr_perf_regs[14] = gr_egpc0_etpc0_sm_dsm_perf_counter6_control_r();
	_ovr_perf_regs[15] = gr_egpc0_etpc0_sm_dsm_perf_counter7_control_r();

	_ovr_perf_regs[16] = gr_egpc0_etpc0_sm0_dsm_perf_counter4_r();
	_ovr_perf_regs[17] = gr_egpc0_etpc0_sm0_dsm_perf_counter5_r();
	_ovr_perf_regs[18] = gr_egpc0_etpc0_sm0_dsm_perf_counter6_r();
	_ovr_perf_regs[19] = gr_egpc0_etpc0_sm0_dsm_perf_counter7_r();
}

/* Following are the blocks of registers that the ucode
 * stores in the extended region.
 */
/* ==  ctxsw_extended_sm_dsm_perf_counter_register_stride_v() ? */
static const u32 _num_sm_dsm_perf_regs;
/* ==  ctxsw_extended_sm_dsm_perf_counter_control_register_stride_v() ?*/
static const u32 _num_sm_dsm_perf_ctrl_regs = 2;
static u32 *_sm_dsm_perf_regs;
static u32 _sm_dsm_perf_ctrl_regs[2];

void gv11b_gr_init_sm_dsm_reg_info(void)
{
	if (_sm_dsm_perf_ctrl_regs[0] != 0U) {
		return;
	}

	_sm_dsm_perf_ctrl_regs[0] =
			      gr_egpc0_etpc0_sm_dsm_perf_counter_control0_r();
	_sm_dsm_perf_ctrl_regs[1] =
			      gr_egpc0_etpc0_sm_dsm_perf_counter_control5_r();
}

void gv11b_gr_get_sm_dsm_perf_regs(struct gk20a *g,
					  u32 *num_sm_dsm_perf_regs,
					  u32 **sm_dsm_perf_regs,
					  u32 *perf_register_stride)
{
	*num_sm_dsm_perf_regs = _num_sm_dsm_perf_regs;
	*sm_dsm_perf_regs = _sm_dsm_perf_regs;
	*perf_register_stride =
		g->ops.gr.ctxsw_prog.hw_get_perf_counter_register_stride();
}

void gv11b_gr_get_sm_dsm_perf_ctrl_regs(struct gk20a *g,
					       u32 *num_sm_dsm_perf_ctrl_regs,
					       u32 **sm_dsm_perf_ctrl_regs,
					       u32 *ctrl_register_stride)
{
	*num_sm_dsm_perf_ctrl_regs = _num_sm_dsm_perf_ctrl_regs;
	*sm_dsm_perf_ctrl_regs = _sm_dsm_perf_ctrl_regs;
	*ctrl_register_stride =
		g->ops.gr.ctxsw_prog.hw_get_perf_counter_control_register_stride();
}

void gv11b_gr_get_ovr_perf_regs(struct gk20a *g, u32 *num_ovr_perf_regs,
					       u32 **ovr_perf_regs)
{
	(void)g;
	*num_ovr_perf_regs = _num_ovr_perf_regs;
	*ovr_perf_regs = _ovr_perf_regs;
}

static bool pri_is_egpc_addr_shared(struct gk20a *g, u32 addr)
{
	u32 egpc_shared_base = EGPC_PRI_SHARED_BASE;
	u32 gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_GPC_STRIDE);

	return (addr >= egpc_shared_base) &&
		(addr < egpc_shared_base + gpc_stride);
}

bool gv11b_gr_pri_is_egpc_addr(struct gk20a *g, u32 addr)
{
	u32 egpc_base = g->ops.gr.get_egpc_base(g);
	u32 gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_GPC_STRIDE);
	u32 num_gpcs = nvgpu_get_litter_value(g, GPU_LIT_NUM_GPCS);

	return	((addr >= egpc_base) &&
		 (addr < egpc_base + num_gpcs * gpc_stride)) ||
		pri_is_egpc_addr_shared(g, addr);
}

static inline u32 pri_smpc_in_etpc_addr_mask(struct gk20a *g, u32 addr)
{
	u32 smpc_stride = nvgpu_get_litter_value(g,
				GPU_LIT_SMPC_PRI_STRIDE);

	return (addr & (smpc_stride - 1U));
}

static u32 pri_smpc_ext_addr(struct gk20a *g, u32 sm_offset, u32 gpc_num,
			u32 tpc_num, u32 sm_num)
{
	u32 gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_GPC_STRIDE);
	u32 tpc_in_gpc_base = nvgpu_get_litter_value(g,
					GPU_LIT_TPC_IN_GPC_BASE);
	u32 tpc_in_gpc_stride = nvgpu_get_litter_value(g,
					GPU_LIT_TPC_IN_GPC_STRIDE);
	u32 egpc_base = g->ops.gr.get_egpc_base(g);
	u32 smpc_unique_base = nvgpu_get_litter_value(g,
				GPU_LIT_SMPC_PRI_UNIQUE_BASE);
	u32 smpc_stride = nvgpu_get_litter_value(g,
				GPU_LIT_SMPC_PRI_STRIDE);

	return (egpc_base + (gpc_num * gpc_stride) + tpc_in_gpc_base +
			(tpc_num * tpc_in_gpc_stride) +
			(sm_num * smpc_stride) +
			(smpc_unique_base + sm_offset));
}

static bool pri_is_smpc_addr_in_etpc_shared(struct gk20a *g, u32 addr)
{
	u32 smpc_shared_base = nvgpu_get_litter_value(g,
				GPU_LIT_SMPC_PRI_SHARED_BASE);
	u32 smpc_stride = nvgpu_get_litter_value(g,
				GPU_LIT_SMPC_PRI_STRIDE);

	return (addr >= smpc_shared_base) &&
		(addr < smpc_shared_base + smpc_stride);
}

bool gv11b_gr_pri_is_etpc_addr(struct gk20a *g, u32 addr)
{
	u32 egpc_addr = 0;

	if (g->ops.gr.is_egpc_addr(g, addr)) {
		egpc_addr = pri_gpccs_addr_mask(g, addr);
		if (nvgpu_gr_is_tpc_addr(g, egpc_addr)) {
			return true;
		}
	}

	return false;
}

static u32 pri_get_egpc_num(struct gk20a *g, u32 addr)
{
	u32 i, start;
	u32 egpc_base = g->ops.gr.get_egpc_base(g);
	u32 num_gpcs = nvgpu_get_litter_value(g, GPU_LIT_NUM_GPCS);
	u32 gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_GPC_STRIDE);

	for (i = 0; i < num_gpcs; i++) {
		start = egpc_base + (i * gpc_stride);
		if ((addr >= start) && (addr < (start + gpc_stride))) {
			return i;
		}
	}
	return 0;
}

static u32 pri_egpc_addr(struct gk20a *g, u32 addr, u32 gpc)
{
	u32 egpc_base = g->ops.gr.get_egpc_base(g);
	u32 gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_GPC_STRIDE);

	return egpc_base + (gpc * gpc_stride) + addr;
}

static u32 pri_etpc_addr(struct gk20a *g, u32 addr, u32 gpc, u32 tpc)
{
	u32 egpc_base = g->ops.gr.get_egpc_base(g);
	u32 gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_GPC_STRIDE);
	u32 tpc_in_gpc_base = nvgpu_get_litter_value(g,
					GPU_LIT_TPC_IN_GPC_BASE);
	u32 tpc_in_gpc_stride = nvgpu_get_litter_value(g,
					GPU_LIT_TPC_IN_GPC_STRIDE);

	return egpc_base + (gpc * gpc_stride) +
		tpc_in_gpc_base + (tpc * tpc_in_gpc_stride) +
		addr;
}

void gv11b_gr_get_egpc_etpc_num(struct gk20a *g, u32 addr,
			u32 *egpc_num, u32 *etpc_num)
{
	u32 egpc_addr = 0;

	*egpc_num = pri_get_egpc_num(g, addr);
	egpc_addr = pri_gpccs_addr_mask(g, addr);
	*etpc_num = nvgpu_gr_get_tpc_num(g, egpc_addr);

	nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gpu_dbg,
			"egpc_num = %d etpc_num = %d", *egpc_num, *etpc_num);
}

int gv11b_gr_decode_egpc_addr(struct gk20a *g, u32 addr,
	enum ctxsw_addr_type *addr_type, u32 *gpc_num, u32 *tpc_num,
	u32 *broadcast_flags)
{
	u32 gpc_addr;
	u32 tpc_addr;

	if (g->ops.gr.is_egpc_addr(g, addr)) {
		nvgpu_log_info(g, "addr=0x%x is egpc", addr);

		*addr_type = CTXSW_ADDR_TYPE_EGPC;
		gpc_addr = pri_gpccs_addr_mask(g, addr);
		if (pri_is_egpc_addr_shared(g, addr)) {
			*broadcast_flags |= PRI_BROADCAST_FLAGS_EGPC;
			*gpc_num = 0;
			nvgpu_log_info(g, "shared egpc");
		} else {
			*gpc_num = pri_get_egpc_num(g, addr);
			nvgpu_log_info(g, "gpc=0x%x", *gpc_num);
		}
		if (nvgpu_gr_is_tpc_addr(g, gpc_addr)) {
			nvgpu_log_info(g, "addr=0x%x is etpc", addr);
			*addr_type = CTXSW_ADDR_TYPE_ETPC;
			if (pri_is_tpc_addr_shared(g, gpc_addr)) {
				*broadcast_flags |= PRI_BROADCAST_FLAGS_ETPC;
				*tpc_num = 0;
				nvgpu_log_info(g, "shared etpc");
			} else {
				*tpc_num = nvgpu_gr_get_tpc_num(g, gpc_addr);
				nvgpu_log_info(g, "tpc=0x%x", *tpc_num);
			}
			tpc_addr = pri_tpccs_addr_mask(g, addr);
			if (pri_is_smpc_addr_in_etpc_shared(g, tpc_addr)) {
				*broadcast_flags |= PRI_BROADCAST_FLAGS_SMPC;
			}
		}

		nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gpu_dbg,
			"addr_type = %d, broadcast_flags = %#08x",
			*addr_type, *broadcast_flags);
		return 0;
	}
	return -EINVAL;
}

static void gv11b_gr_update_priv_addr_table_smpc(struct gk20a *g, u32 gpc_num,
			u32 tpc_num, u32 addr,
			u32 *priv_addr_table, u32 *t)
{
	u32 sm_per_tpc, sm_num;

	nvgpu_log_info(g, "broadcast flags smpc");

	sm_per_tpc = nvgpu_get_litter_value(g, GPU_LIT_NUM_SM_PER_TPC);
	for (sm_num = 0; sm_num < sm_per_tpc; sm_num++) {
		priv_addr_table[*t] = pri_smpc_ext_addr(g,
				pri_smpc_in_etpc_addr_mask(g, addr),
				gpc_num, tpc_num, sm_num);
		nvgpu_log_info(g, "priv_addr_table[%d]:%#08x",
				*t, priv_addr_table[*t]);
		(*t)++;
	}
}

void gv11b_gr_egpc_etpc_priv_addr_table(struct gk20a *g, u32 addr,
	u32 gpc_num, u32 tpc_num, u32 broadcast_flags,
	u32 *priv_addr_table, u32 *t)
{
	u32 priv_addr, gpc_addr;
	struct nvgpu_gr *gr = nvgpu_gr_get_cur_instance_ptr(g);

	nvgpu_log_info(g, "addr=0x%x", addr);

	/* The GPC/TPC unicast registers are included in the compressed PRI
	 * tables. Convert a GPC/TPC broadcast address to unicast addresses so
	 * that we can look up the offsets.
	 */
	if ((broadcast_flags & PRI_BROADCAST_FLAGS_EGPC) != 0U) {
		nvgpu_log_info(g, "broadcast flags egpc");
		for (gpc_num = 0;
		     gpc_num < nvgpu_gr_config_get_gpc_count(gr->config);
		     gpc_num++) {

			if ((broadcast_flags & PRI_BROADCAST_FLAGS_ETPC) != 0U) {
				nvgpu_log_info(g, "broadcast flags etpc");
				for (tpc_num = 0;
				     tpc_num < nvgpu_gr_config_get_gpc_tpc_count(gr->config, gpc_num);
				     tpc_num++) {
					if ((broadcast_flags &
					     PRI_BROADCAST_FLAGS_SMPC) != 0U) {
							gv11b_gr_update_priv_addr_table_smpc(
								g, gpc_num, tpc_num, addr,
								priv_addr_table, t);
					} else {
						priv_addr_table[*t] =
							pri_etpc_addr(g,
							pri_tpccs_addr_mask(g, addr),
							gpc_num, tpc_num);
							nvgpu_log_info(g,
							"priv_addr_table[%d]:%#08x",
							*t, priv_addr_table[*t]);
						(*t)++;
					}
				}
			} else if ((broadcast_flags &
				   PRI_BROADCAST_FLAGS_SMPC) != 0U) {
				gv11b_gr_update_priv_addr_table_smpc(
					g, gpc_num, tpc_num, addr,
					priv_addr_table, t);
			} else {
				priv_addr = pri_egpc_addr(g,
						pri_gpccs_addr_mask(g, addr),
						gpc_num);

				gpc_addr = pri_gpccs_addr_mask(g, priv_addr);
				tpc_num = nvgpu_gr_get_tpc_num(g, gpc_addr);
				if (tpc_num >= nvgpu_gr_config_get_gpc_tpc_count(gr->config, gpc_num)) {
					continue;
				}

				priv_addr_table[*t] = priv_addr;
				nvgpu_log_info(g, "priv_addr_table[%d]:%#08x",
					*t, priv_addr_table[*t]);
				(*t)++;
			}
		}
	} else if ((broadcast_flags & PRI_BROADCAST_FLAGS_EGPC) == 0U) {
		if ((broadcast_flags & PRI_BROADCAST_FLAGS_ETPC) != 0U) {
			nvgpu_log_info(g, "broadcast flags etpc but not egpc");
			for (tpc_num = 0;
			     tpc_num < nvgpu_gr_config_get_gpc_tpc_count(gr->config, gpc_num);
			     tpc_num++) {
				if ((broadcast_flags &
				     PRI_BROADCAST_FLAGS_SMPC) != 0U) {
					gv11b_gr_update_priv_addr_table_smpc(
						g, gpc_num, tpc_num, addr,
						priv_addr_table, t);
				} else {
					priv_addr_table[*t] =
					pri_etpc_addr(g,
					pri_tpccs_addr_mask(g, addr),
					     gpc_num, tpc_num);
					nvgpu_log_info(g,
					"priv_addr_table[%d]:%#08x",
					*t, priv_addr_table[*t]);
					(*t)++;
				}
			}
		} else if ((broadcast_flags & PRI_BROADCAST_FLAGS_SMPC) != 0U) {
			gv11b_gr_update_priv_addr_table_smpc(
				g, gpc_num, tpc_num, addr,
				priv_addr_table, t);
		} else {
			priv_addr_table[*t] = addr;
			nvgpu_log_info(g, "priv_addr_table[%d]:%#08x",
					*t, priv_addr_table[*t]);
			(*t)++;
		}
	}
}

u32 gv11b_gr_get_egpc_base(struct gk20a *g)
{
	(void)g;
	return EGPC_PRI_BASE;
}

/*
 * This function will decode a priv address and return the partition
 * type and numbers
 */
int gr_gv11b_decode_priv_addr(struct gk20a *g, u32 addr,
	enum ctxsw_addr_type *addr_type,
	u32 *gpc_num, u32 *tpc_num, u32 *ppc_num, u32 *rop_num,
	u32 *broadcast_flags)
{
	u32 gpc_addr, tpc_addr;

	nvgpu_log(g, gpu_dbg_gpu_dbg, "addr=0x%x", addr);

	/* setup defaults */
	*addr_type = CTXSW_ADDR_TYPE_SYS;
	*broadcast_flags = PRI_BROADCAST_FLAGS_NONE;
	*gpc_num = 0;
	*tpc_num = 0;
	*ppc_num = 0;
	*rop_num  = 0;

	if (pri_is_gpc_addr(g, addr)) {
		*addr_type = CTXSW_ADDR_TYPE_GPC;
		gpc_addr = pri_gpccs_addr_mask(g, addr);
		if (pri_is_gpc_addr_shared(g, addr)) {
			*addr_type = CTXSW_ADDR_TYPE_GPC;
			*broadcast_flags |= PRI_BROADCAST_FLAGS_GPC;
		} else {
			*gpc_num = pri_get_gpc_num(g, addr);
		}

		if (pri_is_ppc_addr(g, gpc_addr)) {
			*addr_type = CTXSW_ADDR_TYPE_PPC;
			if (pri_is_ppc_addr_shared(g, gpc_addr)) {
				*broadcast_flags |= PRI_BROADCAST_FLAGS_PPC;
				return 0;
			}
		}
		if (nvgpu_gr_is_tpc_addr(g, gpc_addr)) {
			*addr_type = CTXSW_ADDR_TYPE_TPC;
			if (pri_is_tpc_addr_shared(g, gpc_addr)) {
				*broadcast_flags |= PRI_BROADCAST_FLAGS_TPC;
			} else {
				*tpc_num = nvgpu_gr_get_tpc_num(g, gpc_addr);
			}
			/* mask bits other than tpc addr bits */
			tpc_addr = pri_tpccs_addr_mask(g, gpc_addr);
			if (pri_is_sm_addr_shared(g, tpc_addr)) {
				*broadcast_flags |= PRI_BROADCAST_FLAGS_SM;
			}
		}
		return 0;
	} else if (pri_is_rop_addr(g, addr)) {
		*addr_type = CTXSW_ADDR_TYPE_ROP;
		if (pri_is_rop_addr_shared(g, addr)) {
			*broadcast_flags |= PRI_BROADCAST_FLAGS_ROP;
			return 0;
		}
		*rop_num = pri_get_rop_num(g, addr);
		return 0;
	} else if (g->ops.ltc.pri_is_ltc_addr(g, addr)) {
		*addr_type = CTXSW_ADDR_TYPE_LTCS;
		if (g->ops.ltc.is_ltcs_ltss_addr(g, addr)) {
			*broadcast_flags |= PRI_BROADCAST_FLAGS_LTCS;
		} else if (g->ops.ltc.is_ltcn_ltss_addr(g, addr)) {
			*broadcast_flags |= PRI_BROADCAST_FLAGS_LTSS;
		}
		return 0;
	} else if (pri_is_fbpa_addr(g, addr)) {
		*addr_type = CTXSW_ADDR_TYPE_FBPA;
		if (pri_is_fbpa_addr_shared(g, addr)) {
			*broadcast_flags |= PRI_BROADCAST_FLAGS_FBPA;
			return 0;
		}
		return 0;
	} else if ((g->ops.gr.is_egpc_addr != NULL) &&
			g->ops.gr.is_egpc_addr(g, addr)) {
		return g->ops.gr.decode_egpc_addr(g,
				addr, addr_type, gpc_num,
				tpc_num, broadcast_flags);
	} else if (PRI_PMMGS_BASE_ADDR_MASK(addr) ==
			NV_PERF_PMMGPC_GPCGS_GPCTPCA) {
		*broadcast_flags |= (PRI_BROADCAST_FLAGS_PMM_GPCGS_GPCTPCA |
				     PRI_BROADCAST_FLAGS_PMMGPC);
		*addr_type = CTXSW_ADDR_TYPE_GPC;
		return 0;
	} else if (PRI_PMMGS_BASE_ADDR_MASK(addr) ==
			NV_PERF_PMMGPC_GPCGS_GPCTPCB) {
		*broadcast_flags |= (PRI_BROADCAST_FLAGS_PMM_GPCGS_GPCTPCB |
				     PRI_BROADCAST_FLAGS_PMMGPC);
		*addr_type = CTXSW_ADDR_TYPE_GPC;
		return 0;
	} else if (PRI_PMMGS_BASE_ADDR_MASK(addr) == NV_PERF_PMMFBP_FBPGS_LTC) {
		*broadcast_flags |= (PRI_BROADCAST_FLAGS_PMM_FBPGS_LTC |
				     PRI_BROADCAST_FLAGS_PMMFBP);
		*addr_type = CTXSW_ADDR_TYPE_LTCS;
		return 0;
	} else if (PRI_PMMGS_BASE_ADDR_MASK(addr) == NV_PERF_PMMFBP_FBPGS_ROP) {
		*broadcast_flags |= (PRI_BROADCAST_FLAGS_PMM_FBPGS_ROP |
				     PRI_BROADCAST_FLAGS_PMMFBP);
		*addr_type = CTXSW_ADDR_TYPE_PMM_FBPGS_ROP;
		return 0;
	} else if (PRI_PMMS_BASE_ADDR_MASK(addr) == NV_PERF_PMMGPC_GPCS) {
		*broadcast_flags |= (PRI_BROADCAST_FLAGS_PMM_GPCS |
				     PRI_BROADCAST_FLAGS_PMMGPC);
		*addr_type = CTXSW_ADDR_TYPE_GPC;
		return 0;
	} else if (PRI_PMMS_BASE_ADDR_MASK(addr) == NV_PERF_PMMFBP_FBPS) {
		*broadcast_flags |= (PRI_BROADCAST_FLAGS_PMM_FBPS |
				     PRI_BROADCAST_FLAGS_PMMFBP);
		*addr_type = CTXSW_ADDR_TYPE_FBP;
		return 0;
	}

	*addr_type = CTXSW_ADDR_TYPE_SYS;
	return 0;
}

static u32 gr_gv11b_pri_pmmgpc_addr(struct gk20a *g, u32 gpc_num,
	u32 domain_idx, u32 offset)
{
	return perf_pmmgpc_base_v() +
		(gpc_num * g->ops.perf.get_pmmgpc_per_chiplet_offset()) +
		(domain_idx * perf_pmmgpc_perdomain_offset_v()) +
		offset;
}

static void gr_gv11b_split_pmm_fbp_broadcast_address(struct gk20a *g,
	u32 offset, u32 *priv_addr_table, u32 *t,
	u32 domain_start, u32 num_domains)
{
	u32 domain_idx = 0;
	u32 fbp_num = 0;
	u32 base = 0;

	for (fbp_num = 0; fbp_num < nvgpu_fbp_get_num_fbps(g->fbp); fbp_num++) {
		base = perf_pmmfbp_base_v() +
			(fbp_num * g->ops.perf.get_pmmfbp_per_chiplet_offset());

		for (domain_idx = domain_start;
		     domain_idx < (domain_start + num_domains);
		     domain_idx++) {
			priv_addr_table[(*t)++] = base +
				(domain_idx * perf_pmmgpc_perdomain_offset_v())
				+ offset;
		}
	}
}


int gr_gv11b_create_priv_addr_table(struct gk20a *g,
					   u32 addr,
					   u32 *priv_addr_table,
					   u32 *num_registers)
{
	enum ctxsw_addr_type addr_type;
	u32 gpc_num, tpc_num, ppc_num, rop_num, sm_num;
	u32 priv_addr, gpc_addr;
	u32 broadcast_flags;
	u32 t;
	int err;
	struct nvgpu_gr *gr = nvgpu_gr_get_cur_instance_ptr(g);

	t = 0;
	*num_registers = 0;

	nvgpu_log(g, gpu_dbg_gpu_dbg, "addr=0x%x", addr);

	err = g->ops.gr.decode_priv_addr(g, addr, &addr_type,
					&gpc_num, &tpc_num, &ppc_num, &rop_num,
					&broadcast_flags);
	nvgpu_log(g, gpu_dbg_gpu_dbg, "addr_type = %d", addr_type);
	if (err != 0) {
		return err;
	}

	if ((addr_type == CTXSW_ADDR_TYPE_SYS) ||
	    (addr_type == CTXSW_ADDR_TYPE_ROP)) {
		/*
		 * The ROP broadcast registers are included in the compressed PRI
		 * table. Convert a ROP unicast address to a broadcast address
		 * so that we can look up the offset
		 */
		if ((addr_type == CTXSW_ADDR_TYPE_ROP) &&
		    (broadcast_flags & PRI_BROADCAST_FLAGS_ROP) == 0U) {
			priv_addr_table[t++] = pri_rop_shared_addr(g, addr);
		} else {
			priv_addr_table[t++] = addr;
		}

		*num_registers = t;
		return 0;
	}

	/*
	 * The GPC/TPC unicast registers are included in the compressed PRI
	 * tables. Convert a GPC/TPC broadcast address to unicast addresses so
	 * that we can look up the offsets
	 */
	if ((broadcast_flags & PRI_BROADCAST_FLAGS_GPC) != 0U) {
		for (gpc_num = 0;
		     gpc_num < nvgpu_gr_config_get_gpc_count(gr->config);
		     gpc_num++) {

			if ((broadcast_flags & PRI_BROADCAST_FLAGS_TPC) != 0U) {
				for (tpc_num = 0;
				     tpc_num < nvgpu_gr_config_get_gpc_tpc_count(gr->config, gpc_num);
				     tpc_num++) {
					if ((broadcast_flags &
						PRI_BROADCAST_FLAGS_SM) != 0U) {
						for (sm_num = 0;
							sm_num < nvgpu_gr_config_get_sm_count_per_tpc(gr->config);
							sm_num++) {
								priv_addr_table[t++] =
								pri_sm_addr(g,
								pri_sm_in_tpc_addr_mask(g, addr),
								gpc_num, tpc_num, sm_num);
						}
					} else {
						priv_addr_table[t++] =
							pri_tpc_addr(g,
								pri_tpccs_addr_mask(g, addr),
								gpc_num, tpc_num);
					}
				}
			} else if ((broadcast_flags & PRI_BROADCAST_FLAGS_PPC) != 0U) {
				err = gr_gk20a_split_ppc_broadcast_addr(g,
					addr, gpc_num, priv_addr_table, &t);
				if (err != 0) {
					return err;
				}
			} else {
				priv_addr = pri_gpc_addr(g,
						pri_gpccs_addr_mask(g, addr),
						gpc_num);

				gpc_addr = pri_gpccs_addr_mask(g, priv_addr);
				tpc_num = nvgpu_gr_get_tpc_num(g, gpc_addr);
				if (tpc_num >= nvgpu_gr_config_get_gpc_tpc_count(gr->config, gpc_num)) {
					continue;
				}

				priv_addr_table[t++] = priv_addr;
			}
		}
	} else if ((broadcast_flags & PRI_BROADCAST_FLAGS_PMMGPC) != 0U) {
		u32 pmm_domain_start = 0;
		u32 domain_idx = 0;
		u32 num_domains = 0;
		u32 offset = 0;

		if ((broadcast_flags &
		     PRI_BROADCAST_FLAGS_PMM_GPCGS_GPCTPCA) != 0U) {
			pmm_domain_start = nvgpu_get_litter_value(g,
				GPU_LIT_PERFMON_PMMGPCTPCA_DOMAIN_START);
			num_domains = nvgpu_get_litter_value(g,
				GPU_LIT_PERFMON_PMMGPCTPC_DOMAIN_COUNT);
			offset = PRI_PMMGS_OFFSET_MASK(addr);
		} else if ((broadcast_flags &
			    PRI_BROADCAST_FLAGS_PMM_GPCGS_GPCTPCB) != 0U) {
			pmm_domain_start = nvgpu_get_litter_value(g,
				GPU_LIT_PERFMON_PMMGPCTPCB_DOMAIN_START);
			num_domains = nvgpu_get_litter_value(g,
				GPU_LIT_PERFMON_PMMGPCTPC_DOMAIN_COUNT);
			offset = PRI_PMMGS_OFFSET_MASK(addr);
		} else if ((broadcast_flags &
			    PRI_BROADCAST_FLAGS_PMM_GPCS) != 0U) {
			pmm_domain_start = (addr -
			     (NV_PERF_PMMGPC_GPCS + PRI_PMMS_ADDR_MASK(addr)))/
			     perf_pmmgpc_perdomain_offset_v();
			num_domains = 1;
			offset = PRI_PMMS_ADDR_MASK(addr);
		} else {
			return -EINVAL;
		}

		for (gpc_num = 0;
		     gpc_num < nvgpu_gr_config_get_gpc_count(gr->config);
		     gpc_num++) {
			for (domain_idx = pmm_domain_start;
			     domain_idx < (pmm_domain_start + num_domains);
			     domain_idx++) {
				priv_addr_table[t++] =
					gr_gv11b_pri_pmmgpc_addr(g, gpc_num,
					domain_idx, offset);
			}
		}
	} else if (((addr_type == CTXSW_ADDR_TYPE_EGPC) ||
		    (addr_type == CTXSW_ADDR_TYPE_ETPC)) &&
		   (g->ops.gr.egpc_etpc_priv_addr_table != NULL)) {
		nvgpu_log(g, gpu_dbg_gpu_dbg, "addr_type : EGPC/ETPC");
		g->ops.gr.egpc_etpc_priv_addr_table(g, addr, gpc_num, tpc_num,
				broadcast_flags, priv_addr_table, &t);
	} else if ((broadcast_flags & PRI_BROADCAST_FLAGS_LTSS) != 0U) {
		g->ops.ltc.split_lts_broadcast_addr(g, addr,
							priv_addr_table, &t);
	} else if ((broadcast_flags & PRI_BROADCAST_FLAGS_LTCS) != 0U) {
		g->ops.ltc.split_ltc_broadcast_addr(g, addr,
							priv_addr_table, &t);
	} else if ((broadcast_flags & PRI_BROADCAST_FLAGS_FBPA) != 0U) {
		g->ops.gr.split_fbpa_broadcast_addr(g, addr,
				nvgpu_get_litter_value(g, GPU_LIT_NUM_FBPAS),
				priv_addr_table, &t);
	} else if ((addr_type == CTXSW_ADDR_TYPE_LTCS) &&
		   ((broadcast_flags & PRI_BROADCAST_FLAGS_PMM_FBPGS_LTC) != 0U)) {
		gr_gv11b_split_pmm_fbp_broadcast_address(g,
			PRI_PMMGS_OFFSET_MASK(addr),
			priv_addr_table, &t,
			nvgpu_get_litter_value(g, GPU_LIT_PERFMON_PMMFBP_LTC_DOMAIN_START),
			nvgpu_get_litter_value(g, GPU_LIT_PERFMON_PMMFBP_LTC_DOMAIN_COUNT));
	} else if ((addr_type == CTXSW_ADDR_TYPE_PMM_FBPGS_ROP) &&
		   ((broadcast_flags & PRI_BROADCAST_FLAGS_PMM_FBPGS_ROP) != 0U)) {
		gr_gv11b_split_pmm_fbp_broadcast_address(g,
			PRI_PMMGS_OFFSET_MASK(addr),
			priv_addr_table, &t,
			nvgpu_get_litter_value(g, GPU_LIT_PERFMON_PMMFBP_ROP_DOMAIN_START),
			nvgpu_get_litter_value(g, GPU_LIT_PERFMON_PMMFBP_ROP_DOMAIN_COUNT));
	} else if ((addr_type == CTXSW_ADDR_TYPE_FBP) &&
		   ((broadcast_flags & PRI_BROADCAST_FLAGS_PMM_FBPS) != 0U)) {
		u32 domain_start;

		domain_start = (addr -
			(NV_PERF_PMMFBP_FBPS + PRI_PMMS_ADDR_MASK(addr)))/
			perf_pmmgpc_perdomain_offset_v();
		gr_gv11b_split_pmm_fbp_broadcast_address(g,
			PRI_PMMS_ADDR_MASK(addr),
			priv_addr_table, &t,
			domain_start, 1);
	} else if ((broadcast_flags & PRI_BROADCAST_FLAGS_GPC) == 0U) {
		if ((broadcast_flags & PRI_BROADCAST_FLAGS_TPC) != 0U) {
			for (tpc_num = 0;
			     tpc_num < nvgpu_gr_config_get_gpc_tpc_count(gr->config, gpc_num);
			     tpc_num++) {
				priv_addr_table[t++] =
					pri_tpc_addr(g,
						pri_tpccs_addr_mask(g, addr),
						gpc_num, tpc_num);
			}
		} else if ((broadcast_flags & PRI_BROADCAST_FLAGS_PPC) != 0U) {
			err = gr_gk20a_split_ppc_broadcast_addr(g,
					addr, gpc_num, priv_addr_table, &t);
		} else {
			priv_addr_table[t++] = addr;
		}
	}

	*num_registers = t;
	return 0;
}

int gv11b_gr_clear_sm_error_state(struct gk20a *g,
		struct nvgpu_channel *ch, u32 sm_id)
{
	u32 gpc, tpc, sm, offset;
	u32 val;
	struct nvgpu_tsg *tsg;
	struct nvgpu_gr *gr = nvgpu_gr_get_cur_instance_ptr(g);

	int err = 0;

	tsg = nvgpu_tsg_from_ch(ch);
	if (tsg == NULL) {
		return -EINVAL;
	}

	nvgpu_mutex_acquire(&g->dbg_sessions_lock);

	(void)memset(&tsg->sm_error_states[sm_id], 0, sizeof(*tsg->sm_error_states));

	err = nvgpu_gr_disable_ctxsw(g);
	if (err != 0) {
		nvgpu_err(g, "unable to stop gr ctxsw");
		goto fail;
	}

	if (gk20a_is_channel_ctx_resident(ch)) {
		struct nvgpu_sm_info *sm_info;
#ifdef CONFIG_NVGPU_SM_DIVERSITY
		if (nvgpu_gr_ctx_get_sm_diversity_config(tsg->gr_ctx) ==
				NVGPU_DEFAULT_SM_DIVERSITY_CONFIG) {
			sm_info =
				nvgpu_gr_config_get_sm_info(
					gr->config, sm_id);
		} else {
			sm_info =
				nvgpu_gr_config_get_redex_sm_info(
					gr->config, sm_id);
		}
#else
		sm_info = nvgpu_gr_config_get_sm_info(gr->config, sm_id);
#endif

		gpc = nvgpu_gr_config_get_sm_info_gpc_index(sm_info);
		if (g->ops.gr.init.get_nonpes_aware_tpc != NULL) {
			tpc = g->ops.gr.init.get_nonpes_aware_tpc(g,
				nvgpu_gr_config_get_sm_info_gpc_index(sm_info),
				nvgpu_gr_config_get_sm_info_tpc_index(sm_info),
					gr->config);
		} else {
			tpc = nvgpu_gr_config_get_sm_info_tpc_index(sm_info);
		}
		sm = nvgpu_gr_config_get_sm_info_sm_index(sm_info);

		offset = nvgpu_gr_gpc_offset(g, gpc) +
				nvgpu_gr_tpc_offset(g, tpc) +
				nvgpu_gr_sm_offset(g, sm);

		val = gk20a_readl(g, gr_gpc0_tpc0_sm0_hww_global_esr_r() + offset);
		gk20a_writel(g, gr_gpc0_tpc0_sm0_hww_global_esr_r() + offset,
				val);
		gk20a_writel(g, gr_gpc0_tpc0_sm0_hww_warp_esr_r() + offset,
				0);
	}

	err = nvgpu_gr_enable_ctxsw(g);

fail:
	nvgpu_mutex_release(&g->dbg_sessions_lock);
	return err;
}

bool gv11b_gr_esr_bpt_pending_events(u32 global_esr,
				enum nvgpu_event_id_type bpt_event)
{
	bool ret = false;

	if (bpt_event == NVGPU_EVENT_ID_BPT_INT) {
		if ((global_esr &
		 gr_gpc0_tpc0_sm0_hww_global_esr_bpt_int_pending_f()) != 0U) {
			ret = true;
		}
	}

	if (bpt_event == NVGPU_EVENT_ID_BPT_PAUSE) {
		if ((global_esr &
		 gr_gpc0_tpc0_sm0_hww_global_esr_bpt_pause_pending_f()) != 0U) {
			ret = true;
		}
	}

	return ret;
}
