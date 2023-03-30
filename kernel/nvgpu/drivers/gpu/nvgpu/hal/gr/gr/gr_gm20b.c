/*
 * GM20B GPC MMU
 *
 * Copyright (c) 2011-2022, NVIDIA CORPORATION.  All rights reserved.
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

#include <nvgpu/kmem.h>
#include <nvgpu/log.h>
#include <nvgpu/enabled.h>
#include <nvgpu/debug.h>
#include <nvgpu/fuse.h>
#include <nvgpu/io.h>
#include <nvgpu/utils.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/channel.h>
#include <nvgpu/regops.h>
#include <nvgpu/gr/ctx.h>
#include <nvgpu/gr/config.h>
#include <nvgpu/gr/gr.h>
#include <nvgpu/gr/gr_instances.h>
#include <nvgpu/gr/warpstate.h>
#include <nvgpu/engines.h>
#include <nvgpu/engine_status.h>
#include <nvgpu/string.h>

#include "gr_gk20a.h"
#include "gr_gm20b.h"

#include "common/gr/gr_priv.h"

#include <nvgpu/hw/gm20b/hw_gr_gm20b.h>
#include <nvgpu/hw/gm20b/hw_perf_gm20b.h>

u32 gr_gm20b_get_gr_status(struct gk20a *g)
{
	return nvgpu_readl(g, gr_status_r());
}

void gr_gm20b_set_alpha_circular_buffer_size(struct gk20a *g, u32 data)
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
	/* if (NO_ALPHA_BETA_TIMESLICE_SUPPORT_DEF)
		return; */

	if (alpha_cb_size > alpha_cb_size_max) {
		alpha_cb_size = alpha_cb_size_max;
	}

	gk20a_writel(g, gr_ds_tga_constraintlogic_r(),
		(gk20a_readl(g, gr_ds_tga_constraintlogic_r()) &
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

void gr_gm20b_set_circular_buffer_size(struct gk20a *g, u32 data)
{
	struct nvgpu_gr *gr = nvgpu_gr_get_cur_instance_ptr(g);
	u32 gpc_index, ppc_index, stride, val;
	u32 cb_size = data * 4U;
	u32 gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_GPC_STRIDE);
	u32 ppc_in_gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_PPC_IN_GPC_STRIDE);
	u32 attrib_cb_size = g->ops.gr.init.get_attrib_cb_size(g,
		nvgpu_gr_config_get_tpc_count(gr->config));

	nvgpu_log_fn(g, " ");

	if (cb_size > attrib_cb_size) {
		cb_size = attrib_cb_size;
	}

	gk20a_writel(g, gr_ds_tga_constraintlogic_r(),
		(gk20a_readl(g, gr_ds_tga_constraintlogic_r()) &
		 ~gr_ds_tga_constraintlogic_beta_cbsize_f(~U32(0U))) |
		 gr_ds_tga_constraintlogic_beta_cbsize_f(cb_size));

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

			val = gk20a_readl(g, gr_gpcs_swdx_tc_beta_cb_size_r(
						ppc_index + gpc_index));

			val = set_field(val,
				gr_gpcs_swdx_tc_beta_cb_size_v_m(),
				gr_gpcs_swdx_tc_beta_cb_size_v_f(cb_size *
					nvgpu_gr_config_get_gpc_ppc_count(gr->config, gpc_index)));
			val = set_field(val,
				gr_gpcs_swdx_tc_beta_cb_size_div3_m(),
				gr_gpcs_swdx_tc_beta_cb_size_div3_f((cb_size *
					nvgpu_gr_config_get_gpc_ppc_count(gr->config, gpc_index))/3U));

			gk20a_writel(g, gr_gpcs_swdx_tc_beta_cb_size_r(
						ppc_index + gpc_index), val);
		}
	}
}

/* Following are the blocks of registers that the ucode
 stores in the extended region.*/
/* ==  ctxsw_extended_sm_dsm_perf_counter_register_stride_v() ? */
static const u32 _num_sm_dsm_perf_regs;
/* ==  ctxsw_extended_sm_dsm_perf_counter_control_register_stride_v() ?*/
static const u32 _num_sm_dsm_perf_ctrl_regs = 2;
static u32 *_sm_dsm_perf_regs;
static u32 _sm_dsm_perf_ctrl_regs[2];

void gr_gm20b_init_sm_dsm_reg_info(void)
{
	if (_sm_dsm_perf_ctrl_regs[0] != 0U) {
		return;
	}

	_sm_dsm_perf_ctrl_regs[0] =
			      gr_pri_gpc0_tpc0_sm_dsm_perf_counter_control0_r();
	_sm_dsm_perf_ctrl_regs[1] =
			      gr_pri_gpc0_tpc0_sm_dsm_perf_counter_control5_r();
}

void gr_gm20b_get_sm_dsm_perf_regs(struct gk20a *g,
					  u32 *num_sm_dsm_perf_regs,
					  u32 **sm_dsm_perf_regs,
					  u32 *perf_register_stride)
{
	(void)g;
	*num_sm_dsm_perf_regs = _num_sm_dsm_perf_regs;
	*sm_dsm_perf_regs = _sm_dsm_perf_regs;
	*perf_register_stride = 0;
}

void gr_gm20b_get_sm_dsm_perf_ctrl_regs(struct gk20a *g,
					       u32 *num_sm_dsm_perf_ctrl_regs,
					       u32 **sm_dsm_perf_ctrl_regs,
					       u32 *ctrl_register_stride)
{
	*num_sm_dsm_perf_ctrl_regs = _num_sm_dsm_perf_ctrl_regs;
	*sm_dsm_perf_ctrl_regs = _sm_dsm_perf_ctrl_regs;

	*ctrl_register_stride =
	    g->ops.gr.ctxsw_prog.hw_get_perf_counter_control_register_stride();
}

#ifdef CONFIG_NVGPU_TEGRA_FUSE
void gr_gm20b_set_gpc_tpc_mask(struct gk20a *g, u32 gpc_index)
{
	struct nvgpu_gr *gr = nvgpu_gr_get_cur_instance_ptr(g);

	nvgpu_tegra_fuse_write_bypass(g, 0x1);
	nvgpu_tegra_fuse_write_access_sw(g, 0x0);

	if (nvgpu_gr_config_get_gpc_tpc_mask(gr->config, gpc_index) == 0x1U) {
		nvgpu_tegra_fuse_write_opt_gpu_tpc0_disable(g, 0x0);
		nvgpu_tegra_fuse_write_opt_gpu_tpc1_disable(g, 0x1);
	} else if (nvgpu_gr_config_get_gpc_tpc_mask(gr->config, gpc_index) ==
			0x2U) {
		nvgpu_tegra_fuse_write_opt_gpu_tpc0_disable(g, 0x1);
		nvgpu_tegra_fuse_write_opt_gpu_tpc1_disable(g, 0x0);
	} else {
		nvgpu_tegra_fuse_write_opt_gpu_tpc0_disable(g, 0x0);
		nvgpu_tegra_fuse_write_opt_gpu_tpc1_disable(g, 0x0);
	}
}
#endif

int gr_gm20b_dump_gr_status_regs(struct gk20a *g,
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
	gk20a_debug_output(o, "NV_PGRAPH_PRI_FE_GO_IDLE_ON_STATUS: 0x%x",
		gk20a_readl(g, gr_pri_fe_go_idle_on_status_r()));
	gk20a_debug_output(o, "NV_PGRAPH_PRI_FE_GO_IDLE_TIMEOUT : 0x%x",
		gk20a_readl(g, gr_fe_go_idle_timeout_r()));
	gk20a_debug_output(o, "NV_PGRAPH_PRI_FE_GO_IDLE_CHECK : 0x%x",
		gk20a_readl(g, gr_pri_fe_go_idle_check_r()));
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

int gr_gm20b_update_pc_sampling(struct nvgpu_channel *c,
				       bool enable)
{
	struct nvgpu_tsg *tsg;
	struct nvgpu_gr_ctx *gr_ctx;
	struct nvgpu_mem *mem;

	nvgpu_log_fn(c->g, " ");

	tsg = nvgpu_tsg_from_ch(c);
	if (tsg == NULL) {
		return -EINVAL;
	}

	gr_ctx = tsg->gr_ctx;
	mem = nvgpu_gr_ctx_get_ctx_mem(gr_ctx);
	if (!nvgpu_mem_is_valid(mem) || c->vpr) {
		return -EINVAL;
	}

	/* Pascal+ chips do not support updating PC sampling using register
	 * NV_CTXSW_MAIN_IMAGE_PM. We are setting the set_pc_sampling HAL
	 * to NULL. If this API is called for Pascal+, then return error.
	 */
	if (c->g->ops.gr.ctxsw_prog.set_pc_sampling) {
		c->g->ops.gr.ctxsw_prog.set_pc_sampling(c->g, mem, enable);
	} else {
		return -EINVAL;
	}

	nvgpu_log_fn(c->g, "done");

	return 0;
}

void gr_gm20b_init_cyclestats(struct gk20a *g)
{
#if defined(CONFIG_NVGPU_CYCLESTATS)
	nvgpu_set_enabled(g, NVGPU_SUPPORT_CYCLE_STATS, true);
	nvgpu_set_enabled(g, NVGPU_SUPPORT_CYCLE_STATS_SNAPSHOT, true);
#else
	(void)g;
#endif
}

void gr_gm20b_bpt_reg_info(struct gk20a *g, struct nvgpu_warpstate *w_state)
{
	/* Check if we have at least one valid warp */
	/* get paused state on maxwell */
	struct nvgpu_gr *gr = nvgpu_gr_get_cur_instance_ptr(g);
	u32 gpc, tpc, sm_id;
	u32  tpc_offset, gpc_offset, reg_offset;
	u64 warps_valid = 0, warps_paused = 0, warps_trapped = 0;
	u32 gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_GPC_STRIDE);
	u32 tpc_in_gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_TPC_IN_GPC_STRIDE);
	u32 no_of_sm = nvgpu_gr_config_get_no_of_sm(gr->config);

	/* for maxwell & kepler */
	u32 numSmPerTpc = 1;
	u32 numWarpPerTpc = g->params.sm_arch_warp_count * numSmPerTpc;

	for (sm_id = 0; sm_id < no_of_sm; sm_id++) {
		struct nvgpu_sm_info *sm_info =
			nvgpu_gr_config_get_sm_info(gr->config, sm_id);
		gpc = nvgpu_gr_config_get_sm_info_gpc_index(sm_info);
		tpc = nvgpu_gr_config_get_sm_info_tpc_index(sm_info);

		tpc_offset = tpc_in_gpc_stride * tpc;
		gpc_offset = gpc_stride * gpc;
		reg_offset = tpc_offset + gpc_offset;

		/* 64 bit read */
		warps_valid = (u64)gk20a_readl(g, gr_gpc0_tpc0_sm_warp_valid_mask_r() + reg_offset + 4U) << 32;
		warps_valid |= gk20a_readl(g, gr_gpc0_tpc0_sm_warp_valid_mask_r() + reg_offset);

		/* 64 bit read */
		warps_paused = (u64)gk20a_readl(g, gr_gpc0_tpc0_sm_dbgr_bpt_pause_mask_r() + reg_offset + 4U) << 32;
		warps_paused |= gk20a_readl(g, gr_gpc0_tpc0_sm_dbgr_bpt_pause_mask_r() + reg_offset);

		/* 64 bit read */
		warps_trapped = (u64)gk20a_readl(g, gr_gpc0_tpc0_sm_dbgr_bpt_trap_mask_r() + reg_offset + 4U) << 32;
		warps_trapped |= gk20a_readl(g, gr_gpc0_tpc0_sm_dbgr_bpt_trap_mask_r() + reg_offset);

		w_state[sm_id].valid_warps[0] = warps_valid;
		w_state[sm_id].trapped_warps[0] = warps_trapped;
		w_state[sm_id].paused_warps[0] = warps_paused;


		if (numWarpPerTpc > 64U) {
			/* 64 bit read */
			warps_valid = (u64)gk20a_readl(g, gr_gpc0_tpc0_sm_warp_valid_mask_2_r() + reg_offset + 4U) << 32;
			warps_valid |= gk20a_readl(g, gr_gpc0_tpc0_sm_warp_valid_mask_2_r() + reg_offset);

			/* 64 bit read */
			warps_paused = (u64)gk20a_readl(g, gr_gpc0_tpc0_sm_dbgr_bpt_pause_mask_2_r() + reg_offset + 4U) << 32;
			warps_paused |= gk20a_readl(g, gr_gpc0_tpc0_sm_dbgr_bpt_pause_mask_2_r() + reg_offset);

			/* 64 bit read */
			warps_trapped = (u64)gk20a_readl(g, gr_gpc0_tpc0_sm_dbgr_bpt_trap_mask_2_r() + reg_offset + 4U) << 32;
			warps_trapped |= gk20a_readl(g, gr_gpc0_tpc0_sm_dbgr_bpt_trap_mask_2_r() + reg_offset);

			w_state[sm_id].valid_warps[1] = warps_valid;
			w_state[sm_id].trapped_warps[1] = warps_trapped;
			w_state[sm_id].paused_warps[1] = warps_paused;
		}
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

int gm20b_gr_clear_sm_error_state(struct gk20a *g,
		struct nvgpu_channel *ch, u32 sm_id)
{
	u32 gpc, tpc, offset;
	u32 val;
	struct nvgpu_tsg *tsg;
	u32 gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_GPC_STRIDE);
	u32 tpc_in_gpc_stride = nvgpu_get_litter_value(g,
					       GPU_LIT_TPC_IN_GPC_STRIDE);
	int err = 0;
	struct nvgpu_gr *gr = nvgpu_gr_get_cur_instance_ptr(g);

	tsg = nvgpu_tsg_from_ch(ch);
	if (tsg == NULL) {
		return -EINVAL;
	}

	nvgpu_mutex_acquire(&g->dbg_sessions_lock);

	(void) memset(&tsg->sm_error_states[sm_id], 0,
		sizeof(*tsg->sm_error_states));

	err = nvgpu_gr_disable_ctxsw(g);
	if (err != 0) {
		nvgpu_err(g, "unable to stop gr ctxsw");
		goto fail;
	}

	if (gk20a_is_channel_ctx_resident(ch)) {
		struct nvgpu_sm_info *sm_info =
			nvgpu_gr_config_get_sm_info(gr->config, sm_id);
		gpc = nvgpu_gr_config_get_sm_info_gpc_index(sm_info);
		tpc = nvgpu_gr_config_get_sm_info_tpc_index(sm_info);

		offset = gpc_stride * gpc + tpc_in_gpc_stride * tpc;

		val = gk20a_readl(g, gr_gpc0_tpc0_sm_hww_global_esr_r() + offset);
		gk20a_writel(g, gr_gpc0_tpc0_sm_hww_global_esr_r() + offset,
				val);
		gk20a_writel(g, gr_gpc0_tpc0_sm_hww_warp_esr_r() + offset,
				0);
	}

	err = nvgpu_gr_enable_ctxsw(g);

fail:
	nvgpu_mutex_release(&g->dbg_sessions_lock);
	return err;
}

int gm20b_gr_set_mmu_debug_mode(struct gk20a *g,
		struct nvgpu_channel *ch, bool enable)
{
	struct nvgpu_dbg_reg_op ctx_ops = {
		.op = REGOP(WRITE_32),
		.type = REGOP(TYPE_GR_CTX),
		.offset = gr_gpcs_pri_mmu_debug_ctrl_r(),
		.value_lo = enable ?
			gr_gpcs_pri_mmu_debug_ctrl_debug_enabled_f() :
			gr_gpcs_pri_mmu_debug_ctrl_debug_disabled_f(),
	};
	int err;
	struct nvgpu_tsg *tsg = nvgpu_tsg_from_ch(ch);
	u32 flags = NVGPU_REG_OP_FLAG_MODE_ALL_OR_NONE;

	if (tsg == NULL) {
		return enable ? -EINVAL : 0;
	}

	err = gr_gk20a_exec_ctx_ops(tsg, &ctx_ops, 1, 1, 0, &flags);
	if (err != 0) {
		nvgpu_err(g, "update MMU debug mode failed");
	}
	return err;
}

void gm20b_gr_set_debug_mode(struct gk20a *g, bool enable)
{
	u32 reg_val, gpc_debug_ctrl;

	if (enable) {
		gpc_debug_ctrl = gr_gpcs_pri_mmu_debug_ctrl_debug_enabled_f();
	} else {
		gpc_debug_ctrl = gr_gpcs_pri_mmu_debug_ctrl_debug_disabled_f();
	}

	reg_val = gk20a_readl(g, gr_gpcs_pri_mmu_debug_ctrl_r());
	reg_val = set_field(reg_val,
			gr_gpcs_pri_mmu_debug_ctrl_debug_m(), gpc_debug_ctrl);
	gk20a_writel(g, gr_gpcs_pri_mmu_debug_ctrl_r(), reg_val);
}

bool gm20b_gr_esr_bpt_pending_events(u32 global_esr,
				     enum nvgpu_event_id_type bpt_event)
{
	bool ret = false;

	if (bpt_event == NVGPU_EVENT_ID_BPT_INT) {
		if ((global_esr &
		 gr_gpc0_tpc0_sm_hww_global_esr_bpt_int_pending_f()) != 0U) {
			ret = true;
		}
	}

	if (bpt_event == NVGPU_EVENT_ID_BPT_PAUSE) {
		if ((global_esr &
		 gr_gpc0_tpc0_sm_hww_global_esr_bpt_pause_pending_f()) != 0U) {
			ret = true;
		}
	}

	return ret;
}
