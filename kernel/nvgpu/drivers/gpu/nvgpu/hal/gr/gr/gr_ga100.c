/*
 * GA100 GPU GR
 *
 * Copyright (c) 2020-2021, NVIDIA CORPORATION.  All rights reserved.
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
#include <nvgpu/netlist.h>
#include <nvgpu/gr/obj_ctx.h>

#include "gr_ga100.h"
#include "hal/gr/gr/gr_gk20a.h"
#include "hal/gr/gr/gr_pri_gk20a.h"
#include "common/gr/gr_priv.h"

#include <nvgpu/hw/ga100/hw_gr_ga100.h>
#include <nvgpu/hw/ga100/hw_proj_ga100.h>

static void gr_ga100_dump_gr_per_sm_regs(struct gk20a *g,
			struct nvgpu_debug_context *o,
			u32 gpc, u32 tpc, u32 sm, u32 offset)
{
	gk20a_debug_output(o,
		"NV_PGRAPH_PRI_GPC%d_TPC%d_SM%d_HWW_WARP_ESR: 0x%x\n",
		gpc, tpc, sm, nvgpu_readl(g,
		nvgpu_safe_add_u32(gr_gpc0_tpc0_sm0_hww_warp_esr_r(),
				   offset)));

	gk20a_debug_output(o,
		"NV_PGRAPH_PRI_GPC%d_TPC%d_SM%d_HWW_WARP_ESR_REPORT_MASK: 0x%x\n",
		gpc, tpc, sm, nvgpu_readl(g,
		nvgpu_safe_add_u32(gr_gpc0_tpc0_sm0_hww_warp_esr_report_mask_r(),
				   offset)));

	gk20a_debug_output(o,
		"NV_PGRAPH_PRI_GPC%d_TPC%d_SM%d_HWW_GLOBAL_ESR: 0x%x\n",
		gpc, tpc, sm, nvgpu_readl(g,
		nvgpu_safe_add_u32(gr_gpc0_tpc0_sm0_hww_global_esr_r(),
				   offset)));

	gk20a_debug_output(o,
		"NV_PGRAPH_PRI_GPC%d_TPC%d_SM%d_HWW_GLOBAL_ESR_REPORT_MASK: 0x%x\n",
		gpc, tpc, sm, nvgpu_readl(g,
		nvgpu_safe_add_u32(gr_gpc0_tpc0_sm0_hww_global_esr_report_mask_r(),
				   offset)));

	gk20a_debug_output(o,
		"NV_PGRAPH_PRI_GPC%d_TPC%d_SM%d_DBGR_CONTROL0: 0x%x\n",
		gpc, tpc, sm, nvgpu_readl(g,
		nvgpu_safe_add_u32(gr_gpc0_tpc0_sm0_dbgr_control0_r(),
				   offset)));

	gk20a_debug_output(o,
		"NV_PGRAPH_PRI_GPC%d_TPC%d_SM%d_DBGR_STATUS0: 0x%x\n",
		gpc, tpc, sm, nvgpu_readl(g,
		nvgpu_safe_add_u32(gr_gpc0_tpc0_sm0_dbgr_status0_r(),
				   offset)));
}

static void gr_ga100_dump_gr_sm_regs(struct gk20a *g,
			   struct nvgpu_debug_context *o)
{
	u32 gpc, tpc, sm, sm_per_tpc;
	u32 gpc_offset, tpc_offset, offset;
	struct nvgpu_gr *gr = nvgpu_gr_get_cur_instance_ptr(g);

	gk20a_debug_output(o,
		"NV_PGRAPH_PRI_GPCS_TPCS_SMS_HWW_GLOBAL_ESR_REPORT_MASK: 0x%x\n",
		nvgpu_readl(g,
		gr_gpcs_tpcs_sms_hww_global_esr_report_mask_r()));
	gk20a_debug_output(o,
		"NV_PGRAPH_PRI_GPCS_TPCS_SMS_HWW_WARP_ESR_REPORT_MASK: 0x%x\n",
		nvgpu_readl(g, gr_gpcs_tpcs_sms_hww_warp_esr_report_mask_r()));
	gk20a_debug_output(o,
		"NV_PGRAPH_PRI_GPCS_TPCS_SMS_HWW_GLOBAL_ESR: 0x%x\n",
		nvgpu_readl(g, gr_gpcs_tpcs_sms_hww_global_esr_r()));
	gk20a_debug_output(o,
		"NV_PGRAPH_PRI_GPCS_TPCS_SMS_DBGR_CONTROL0: 0x%x\n",
		nvgpu_readl(g, gr_gpcs_tpcs_sms_dbgr_control0_r()));
	gk20a_debug_output(o,
		"NV_PGRAPH_PRI_GPCS_TPCS_SMS_DBGR_STATUS0: 0x%x\n",
		nvgpu_readl(g, gr_gpcs_tpcs_sms_dbgr_status0_r()));
	gk20a_debug_output(o,
		"NV_PGRAPH_PRI_GPCS_TPCS_SMS_DBGR_BPT_PAUSE_MASK_0: 0x%x\n",
		nvgpu_readl(g, gr_gpcs_tpcs_sms_dbgr_bpt_pause_mask_0_r()));
	gk20a_debug_output(o,
		"NV_PGRAPH_PRI_GPCS_TPCS_SMS_DBGR_BPT_PAUSE_MASK_1: 0x%x\n",
		nvgpu_readl(g, gr_gpcs_tpcs_sms_dbgr_bpt_pause_mask_1_r()));

	sm_per_tpc = nvgpu_get_litter_value(g, GPU_LIT_NUM_SM_PER_TPC);
	for (gpc = 0U;
	     gpc < nvgpu_gr_config_get_gpc_count(gr->config); gpc++) {
		gpc_offset = nvgpu_gr_gpc_offset(g, gpc);

		for (tpc = 0U;
		     tpc < nvgpu_gr_config_get_gpc_tpc_count(gr->config, gpc);
		     tpc++) {
			tpc_offset = nvgpu_gr_tpc_offset(g, tpc);

			for (sm = 0U; sm < sm_per_tpc; sm++) {
				offset = nvgpu_safe_add_u32(
						nvgpu_safe_add_u32(gpc_offset,
								   tpc_offset),
						nvgpu_gr_sm_offset(g, sm));

				gr_ga100_dump_gr_per_sm_regs(g, o,
					gpc, tpc, sm, offset);
			}
		}
	}
}

static void gr_ga100_dump_tpc_activity_regs(struct gk20a *g,
					    struct nvgpu_debug_context *o)
{
	struct nvgpu_gr *gr = nvgpu_gr_get_cur_instance_ptr(g);
	u32 gpc_index = 0U;
	u32 tpc_count = 0U, tpc_stride = 0U;
	u32 reg_index = 0U, offset = 0U;
	u32 i = 0U;

	if (nvgpu_gr_config_get_base_count_gpc_tpc(gr->config) == NULL) {
		return;
	}

	tpc_count = nvgpu_gr_config_get_gpc_tpc_count(gr->config, gpc_index);
	tpc_stride = nvgpu_get_litter_value(g, GPU_LIT_TPC_IN_GPC_STRIDE);

	for (i = 0U; i < tpc_count; i++) {
		offset = nvgpu_safe_mult_u32(tpc_stride, i);
		reg_index = nvgpu_safe_add_u32(offset,
				gr_pri_gpc0_tpc0_tpccs_tpc_activity_0_r());

		gk20a_debug_output(o,
			"NV_PGRAPH_PRI_GPC0_TPC%d_TPCCS_TPC_ACTIVITY0: 0x%x\n",
			i, nvgpu_readl(g, reg_index));
	}
}

int gr_ga100_dump_gr_status_regs(struct gk20a *g,
				 struct nvgpu_debug_context *o)
{
	u32 gr_engine_id;
	struct nvgpu_engine_status_info engine_status;

	gr_engine_id = nvgpu_engine_get_gr_id(g);

	gk20a_debug_output(o, "NV_PGRAPH_STATUS: 0x%x\n",
		nvgpu_readl(g, gr_status_r()));
	gk20a_debug_output(o, "NV_PGRAPH_STATUS1: 0x%x\n",
		nvgpu_readl(g, gr_status_1_r()));
	gk20a_debug_output(o, "NV_PGRAPH_ENGINE_STATUS: 0x%x\n",
		nvgpu_readl(g, gr_engine_status_r()));
	gk20a_debug_output(o, "NV_PGRAPH_GRFIFO_STATUS : 0x%x\n",
		nvgpu_readl(g, gr_gpfifo_status_r()));
	gk20a_debug_output(o, "NV_PGRAPH_GRFIFO_CONTROL : 0x%x\n",
		nvgpu_readl(g, gr_gpfifo_ctl_r()));
	gk20a_debug_output(o, "NV_PGRAPH_PRI_FECS_HOST_INT_STATUS : 0x%x\n",
		nvgpu_readl(g, gr_fecs_host_int_status_r()));
	gk20a_debug_output(o, "NV_PGRAPH_EXCEPTION  : 0x%x\n",
		nvgpu_readl(g, gr_exception_r()));
	gk20a_debug_output(o, "NV_PGRAPH_FECS_INTR  : 0x%x\n",
		nvgpu_readl(g, gr_fecs_intr_r()));
	g->ops.engine_status.read_engine_status_info(g, gr_engine_id,
		&engine_status);
	gk20a_debug_output(o, "NV_PFIFO_ENGINE_STATUS(GR) : 0x%x\n",
		engine_status.reg_data);
	gk20a_debug_output(o, "NV_PGRAPH_ACTIVITY0: 0x%x\n",
		nvgpu_readl(g, gr_activity_0_r()));
	gk20a_debug_output(o, "NV_PGRAPH_ACTIVITY1: 0x%x\n",
		nvgpu_readl(g, gr_activity_1_r()));
	gk20a_debug_output(o, "NV_PGRAPH_ACTIVITY4: 0x%x\n",
		nvgpu_readl(g, gr_activity_4_r()));
	gk20a_debug_output(o, "NV_PGRAPH_PRI_SKED_ACTIVITY: 0x%x\n",
		nvgpu_readl(g, gr_pri_sked_activity_r()));
	gk20a_debug_output(o, "NV_PGRAPH_PRI_GPC0_GPCCS_GPC_ACTIVITY0: 0x%x\n",
		nvgpu_readl(g, gr_pri_gpc0_gpccs_gpc_activity0_r()));
	gk20a_debug_output(o, "NV_PGRAPH_PRI_GPC0_GPCCS_GPC_ACTIVITY1: 0x%x\n",
		nvgpu_readl(g, gr_pri_gpc0_gpccs_gpc_activity1_r()));
	gk20a_debug_output(o, "NV_PGRAPH_PRI_GPC0_GPCCS_GPC_ACTIVITY2: 0x%x\n",
		nvgpu_readl(g, gr_pri_gpc0_gpccs_gpc_activity2_r()));
	gk20a_debug_output(o, "NV_PGRAPH_PRI_GPC0_GPCCS_GPC_ACTIVITY3: 0x%x\n",
		nvgpu_readl(g, gr_pri_gpc0_gpccs_gpc_activity3_r()));

	gr_ga100_dump_tpc_activity_regs(g,o);

	gk20a_debug_output(o, "NV_PGRAPH_PRI_GPCS_GPCCS_GPC_ACTIVITY0: 0x%x\n",
		nvgpu_readl(g, gr_pri_gpcs_gpccs_gpc_activity_0_r()));
	gk20a_debug_output(o, "NV_PGRAPH_PRI_GPCS_GPCCS_GPC_ACTIVITY1: 0x%x\n",
		nvgpu_readl(g, gr_pri_gpcs_gpccs_gpc_activity_1_r()));
	gk20a_debug_output(o, "NV_PGRAPH_PRI_GPCS_GPCCS_GPC_ACTIVITY2: 0x%x\n",
		nvgpu_readl(g, gr_pri_gpcs_gpccs_gpc_activity_2_r()));
	gk20a_debug_output(o, "NV_PGRAPH_PRI_GPCS_GPCCS_GPC_ACTIVITY3: 0x%x\n",
		nvgpu_readl(g, gr_pri_gpcs_gpccs_gpc_activity_3_r()));
	gk20a_debug_output(o, "NV_PGRAPH_PRI_GPCS_TPCS_TPCCS_TPC_ACTIVITY0: 0x%x\n",
		nvgpu_readl(g, gr_pri_gpcs_tpcs_tpccs_tpc_activity_0_r()));
	if (!nvgpu_is_enabled(g, NVGPU_SUPPORT_MIG)) {
		gk20a_debug_output(o, "NV_PGRAPH_PRI_DS_MPIPE_STATUS: 0x%x\n",
			nvgpu_readl(g, gr_pri_ds_mpipe_status_r()));
	}
	gk20a_debug_output(o, "NV_PGRAPH_PRI_FE_GO_IDLE_TIMEOUT : 0x%x\n",
		nvgpu_readl(g, gr_fe_go_idle_timeout_r()));
	gk20a_debug_output(o, "NV_PGRAPH_PRI_FE_GO_IDLE_INFO : 0x%x\n",
		nvgpu_readl(g, gr_pri_fe_go_idle_info_r()));
	gk20a_debug_output(o, "NV_PGRAPH_PRI_GPC0_TPC0_TEX_M_TEX_SUBUNITS_STATUS: 0x%x\n",
		nvgpu_readl(g, gr_pri_gpc0_tpc0_tex_m_tex_subunits_status_r()));
	gk20a_debug_output(o, "NV_PGRAPH_PRI_CWD_FS: 0x%x\n",
		nvgpu_readl(g, gr_cwd_fs_r()));
	gk20a_debug_output(o, "NV_PGRAPH_PRI_FE_TPC_FS(0): 0x%x\n",
		nvgpu_readl(g, gr_fe_tpc_fs_r(0)));
	gk20a_debug_output(o, "NV_PGRAPH_PRI_CWD_GPC_TPC_ID: 0x%x\n",
		nvgpu_readl(g, gr_cwd_gpc_tpc_id_r(0)));
	gk20a_debug_output(o, "NV_PGRAPH_PRI_CWD_SM_ID(0): 0x%x\n",
		nvgpu_readl(g, gr_cwd_sm_id_r(0)));
	gk20a_debug_output(o, "NV_PGRAPH_PRI_FECS_CTXSW_STATUS_FE_0: 0x%x\n",
		g->ops.gr.falcon.read_fecs_ctxsw_status0(g));
	gk20a_debug_output(o, "NV_PGRAPH_PRI_FECS_CTXSW_STATUS_1: 0x%x\n",
		g->ops.gr.falcon.read_fecs_ctxsw_status1(g));
	gk20a_debug_output(o, "NV_PGRAPH_PRI_GPC0_GPCCS_CTXSW_STATUS_GPC_0: 0x%x\n",
		nvgpu_readl(g, gr_gpc0_gpccs_ctxsw_status_gpc_0_r()));
	gk20a_debug_output(o, "NV_PGRAPH_PRI_GPC0_GPCCS_CTXSW_STATUS_1: 0x%x\n",
		nvgpu_readl(g, gr_gpc0_gpccs_ctxsw_status_1_r()));
	gk20a_debug_output(o, "NV_PGRAPH_PRI_FECS_CTXSW_IDLESTATE : 0x%x\n",
		nvgpu_readl(g, gr_fecs_ctxsw_idlestate_r()));
	gk20a_debug_output(o, "NV_PGRAPH_PRI_GPC0_GPCCS_CTXSW_IDLESTATE : 0x%x\n",
		nvgpu_readl(g, gr_gpc0_gpccs_ctxsw_idlestate_r()));
	gk20a_debug_output(o, "NV_PGRAPH_PRI_FECS_CURRENT_CTX : 0x%x\n",
		g->ops.gr.falcon.get_current_ctx(g));
	gk20a_debug_output(o, "NV_PGRAPH_PRI_FECS_NEW_CTX : 0x%x\n",
		nvgpu_readl(g, gr_fecs_new_ctx_r()));
	gk20a_debug_output(o, "NV_PGRAPH_PRI_FECS_HOST_INT_ENABLE : 0x%x\n",
		nvgpu_readl(g, gr_fecs_host_int_enable_r()));
	gk20a_debug_output(o, "NV_PGRAPH_PRI_FECS_HOST_INT_STATUS : 0x%x\n",
		nvgpu_readl(g, gr_fecs_host_int_status_r()));
	gk20a_debug_output(o, "NV_PGRAPH_PRI_GPC0_GPCCS_GPC_EXCEPTION: 0x%x\n",
		nvgpu_readl(g, gr_pri_gpc0_gpccs_gpc_exception_r()));
	gk20a_debug_output(o, "NV_PGRAPH_PRI_GPC0_GPCCS_GPC_EXCEPTION_EN: 0x%x\n",
		nvgpu_readl(g, gr_pri_gpc0_gpccs_gpc_exception_en_r()));
	gk20a_debug_output(o, "NV_PGRAPH_PRI_GPC0_TPC0_TPCCS_TPC_EXCEPTION: 0x%x\n",
		nvgpu_readl(g, gr_pri_gpc0_tpc0_tpccs_tpc_exception_r()));
	gk20a_debug_output(o, "NV_PGRAPH_PRI_GPC0_TPC0_TPCCS_TPC_EXCEPTION_EN: 0x%x\n",
		nvgpu_readl(g, gr_pri_gpc0_tpc0_tpccs_tpc_exception_en_r()));

	gr_ga100_dump_gr_sm_regs(g, o);

	return 0;
}

void gr_ga100_set_circular_buffer_size(struct gk20a *g, u32 data)
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
	if (nvgpu_readl(g, gr_gpc0_ppc0_cbm_beta_cb_size_r()) !=
		nvgpu_readl(g,
			gr_gpc0_ppc0_cbm_beta_steady_state_cb_size_r())) {
		cb_size = cb_size_steady +
			(gr_gpc0_ppc0_cbm_beta_cb_size_v_gfxp_v() -
			 gr_gpc0_ppc0_cbm_beta_cb_size_v_default_v());
	} else {
		cb_size = cb_size_steady;
	}

	nvgpu_writel(g, gr_ds_tga_constraintlogic_beta_r(),
		(nvgpu_readl(g, gr_ds_tga_constraintlogic_beta_r()) &
		 ~gr_ds_tga_constraintlogic_beta_cbsize_f(~U32(0U))) |
		 gr_ds_tga_constraintlogic_beta_cbsize_f(cb_size_steady));

	for (gpc_index = 0;
	     gpc_index < nvgpu_gr_config_get_gpc_count(gr->config);
	     gpc_index++) {
		stride = proj_gpc_stride_v() * gpc_index;

		for (ppc_index = 0;
		     ppc_index < nvgpu_gr_config_get_gpc_ppc_count(gr->config, gpc_index);
		     ppc_index++) {

			val = nvgpu_readl(g, gr_gpc0_ppc0_cbm_beta_cb_size_r() +
				stride +
				proj_ppc_in_gpc_stride_v() * ppc_index);

			val = set_field(val,
				gr_gpc0_ppc0_cbm_beta_cb_size_v_m(),
				gr_gpc0_ppc0_cbm_beta_cb_size_v_f(cb_size *
					nvgpu_gr_config_get_pes_tpc_count(gr->config,
						gpc_index, ppc_index)));

			nvgpu_writel(g, gr_gpc0_ppc0_cbm_beta_cb_size_r() +
				stride +
				proj_ppc_in_gpc_stride_v() * ppc_index, val);

			nvgpu_writel(g, proj_ppc_in_gpc_stride_v() * ppc_index +
				gr_gpc0_ppc0_cbm_beta_steady_state_cb_size_r() +
				stride,
				gr_gpc0_ppc0_cbm_beta_steady_state_cb_size_v_f(
					cb_size_steady));

			val = nvgpu_readl(g, gr_gpcs_swdx_tc_beta_cb_size_r(
						ppc_index + gpc_index));

			val = set_field(val,
				gr_gpcs_swdx_tc_beta_cb_size_v_m(),
				gr_gpcs_swdx_tc_beta_cb_size_v_f(
					cb_size_steady *
					nvgpu_gr_config_get_gpc_ppc_count(gr->config, gpc_index)));

			nvgpu_writel(g, gr_gpcs_swdx_tc_beta_cb_size_r(
						ppc_index + gpc_index), val);
		}
	}
}

#ifdef CONFIG_NVGPU_DEBUGGER
/*
 * The sys, tpc, etpc, ppc and gpc ctxsw_reg bundles are divided into compute
 * and gfx. These registers are stored contigously in a single buffer segment.
 * For example priv_sys_segment contains: sys_compute followed by sys_graphics,
 * similarly gpccs_priv_segment contains: tpc_compute followed by tpc_graphics
 * and so on. However, the indices within the *_compute and *_graphics list are
 * not contiguous i.e the graphics list index start from 0, does not continue
 * from the index of the last register in the compute list. Hence, while
 * calculating the offset of registers witin *_graphics list, the
 * computation should account for *_compute registers that preceed it.
 */
int gr_ga100_process_context_buffer_priv_segment(struct gk20a *g,
					     enum ctxsw_addr_type addr_type,
					     u32 pri_addr,
					     u32 gpc_num, u32 num_tpcs,
					     u32 num_ppcs, u32 ppc_mask,
					     u32 *priv_offset)
{
	u32 i;
	u32 address, base_address;
	u32 sys_offset, gpc_offset, tpc_offset, ppc_offset;
	u32 ppc_num, tpc_num, tpc_addr, gpc_addr, ppc_addr;
	struct netlist_aiv_list *list;
	struct netlist_aiv *reg;
	u32 gpc_base = nvgpu_get_litter_value(g, GPU_LIT_GPC_BASE);
	u32 gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_GPC_STRIDE);
	u32 ppc_in_gpc_base = nvgpu_get_litter_value(g, GPU_LIT_PPC_IN_GPC_BASE);
	u32 ppc_in_gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_PPC_IN_GPC_STRIDE);
	u32 tpc_in_gpc_base = nvgpu_get_litter_value(g, GPU_LIT_TPC_IN_GPC_BASE);
	u32 tpc_in_gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_TPC_IN_GPC_STRIDE);

	nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gpu_dbg, "pri_addr=0x%x", pri_addr);

	if (!g->netlist_valid) {
		return -EINVAL;
	}

	/* Process the SYS/BE segment. */
	if ((addr_type == CTXSW_ADDR_TYPE_SYS) ||
	    (addr_type == CTXSW_ADDR_TYPE_ROP)) {
		list = nvgpu_netlist_get_sys_compute_ctxsw_regs(g);
		for (i = 0; i < list->count; i++) {
			reg = &list->l[i];
			address    = reg->addr;
			sys_offset = reg->index;

			if (pri_addr == address) {
				*priv_offset = sys_offset;
				return 0;
			}
		}
#ifdef CONFIG_NVGPU_GRAPHICS
		list = nvgpu_netlist_get_sys_gfx_ctxsw_regs(g);
		for (i = 0; i < list->count; i++) {
			reg = &list->l[i];
			address    = reg->addr;
			sys_offset = reg->index;

			if (pri_addr == address) {
				*priv_offset = sys_offset +
					       nvgpu_netlist_get_sys_compute_ctxsw_regs(g)->count * 4U;
				return 0;
			}
		}
#endif
	}

	/*
	 * Process the LTS segment.
	 *
	 * The LTS registers are stored after the ctx_regs_compute/graphics.
	 * Hence, compute the sysoffset taking into account their count. Each
	 * count represents an entry of 4 bytes.
	 */
	if (addr_type == CTXSW_ADDR_TYPE_LTS_MAIN) {
		sys_offset = nvgpu_netlist_get_sys_ctxsw_regs_count(g);
		sys_offset <<= 2;
		list = nvgpu_netlist_get_lts_ctxsw_regs(g);
		for (i = 0; i < list->count; i++) {
			reg = &list->l[i];
			address = reg->addr;
			sys_offset += reg->index;

			if (pri_addr == address) {
				*priv_offset = sys_offset;
				return 0;
			}
		}
	}

	/* Process the TPC segment. */
	if (addr_type == CTXSW_ADDR_TYPE_TPC) {
		for (tpc_num = 0; tpc_num < num_tpcs; tpc_num++) {
			list = nvgpu_netlist_get_tpc_compute_ctxsw_regs(g);
			for (i = 0; i < list->count; i++) {
				reg = &list->l[i];
				address = reg->addr;
				tpc_addr = pri_tpccs_addr_mask(g, address);
				base_address = gpc_base +
					(gpc_num * gpc_stride) +
					tpc_in_gpc_base +
					(tpc_num * tpc_in_gpc_stride);
				address = base_address + tpc_addr;
				/*
				 * The data for the TPCs is interleaved in the context buffer.
				 * Example with num_tpcs = 2
				 * 0    1    2    3    4    5    6    7    8    9    10   11 ...
				 * 0-0  1-0  0-1  1-1  0-2  1-2  0-3  1-3  0-4  1-4  0-5  1-5 ...
				 */
				tpc_offset = (reg->index * num_tpcs) + (tpc_num * 4U);

				if (pri_addr == address) {
					*priv_offset = tpc_offset;
					return 0;
				}
			}
#ifdef CONFIG_NVGPU_GRAPHICS
			list = nvgpu_netlist_get_tpc_gfx_ctxsw_regs(g);
			for (i = 0; i < list->count; i++) {
				reg = &list->l[i];
				address = reg->addr;
				tpc_addr = pri_tpccs_addr_mask(g, address);
				base_address = gpc_base +
					(gpc_num * gpc_stride) +
					tpc_in_gpc_base +
					(tpc_num * tpc_in_gpc_stride);
				address = base_address + tpc_addr;
				/*
				 * The data for the TPCs is interleaved in the context buffer.
				 * Example with num_tpcs = 2
				 * 0    1    2    3    4    5    6    7    8    9    10   11 ...
				 * 0-0  1-0  0-1  1-1  0-2  1-2  0-3  1-3  0-4  1-4  0-5  1-5 ...
				 */
				tpc_offset = (reg->index * num_tpcs) + (tpc_num * 4U);

				if (pri_addr == address) {
					*priv_offset = tpc_offset +
						nvgpu_netlist_get_tpc_compute_ctxsw_regs(g)->count * num_tpcs * 4U;
					return 0;
				}
			}
#endif
		}
	} else if ((addr_type == CTXSW_ADDR_TYPE_EGPC) ||
		(addr_type == CTXSW_ADDR_TYPE_ETPC)) {
		if (g->ops.gr.get_egpc_base == NULL) {
			return -EINVAL;
		}

		for (tpc_num = 0; tpc_num < num_tpcs; tpc_num++) {
			list = nvgpu_netlist_get_etpc_compute_ctxsw_regs(g);
			for (i = 0; i < list->count; i++) {
				reg = &list->l[i];
				address = reg->addr;
				tpc_addr = pri_tpccs_addr_mask(g, address);
				base_address = g->ops.gr.get_egpc_base(g) +
					(gpc_num * gpc_stride) +
					tpc_in_gpc_base +
					(tpc_num * tpc_in_gpc_stride);
				address = base_address + tpc_addr;
				/*
				 * The data for the TPCs is interleaved in the context buffer.
				 * Example with num_tpcs = 2
				 * 0    1    2    3    4    5    6    7    8    9    10   11 ...
				 * 0-0  1-0  0-1  1-1  0-2  1-2  0-3  1-3  0-4  1-4  0-5  1-5 ...
				 */
				tpc_offset = (reg->index * num_tpcs) + (tpc_num * 4U);

				if (pri_addr == address) {
					*priv_offset = tpc_offset;
					nvgpu_log(g,
						gpu_dbg_fn | gpu_dbg_gpu_dbg,
						"egpc/etpc compute priv_offset=0x%#08x",
						*priv_offset);
					return 0;
				}
			}
#ifdef CONFIG_NVGPU_GRAPHICS
			list = nvgpu_netlist_get_etpc_gfx_ctxsw_regs(g);
			for (i = 0; i < list->count; i++) {
				reg = &list->l[i];
				address = reg->addr;
				tpc_addr = pri_tpccs_addr_mask(g, address);
				base_address = g->ops.gr.get_egpc_base(g) +
					(gpc_num * gpc_stride) +
					tpc_in_gpc_base +
					(tpc_num * tpc_in_gpc_stride);
				address = base_address + tpc_addr;
				/*
				 * The data for the TPCs is interleaved in the context buffer.
				 * Example with num_tpcs = 2
				 * 0    1    2    3    4    5    6    7    8    9    10   11 ...
				 * 0-0  1-0  0-1  1-1  0-2  1-2  0-3  1-3  0-4  1-4  0-5  1-5 ...
				 */
				tpc_offset = (reg->index * num_tpcs) + (tpc_num * 4U);

				if (pri_addr == address) {
					*priv_offset = tpc_offset +
						nvgpu_netlist_get_etpc_compute_ctxsw_regs(g)->count * num_tpcs * 4U;
					nvgpu_log(g,
						gpu_dbg_fn | gpu_dbg_gpu_dbg,
						"egpc/etpc gfx priv_offset=0x%#08x",
						*priv_offset);
					return 0;
				}
			}
#endif
		}
	}


	/* Process the PPC segment. */
	if (addr_type == CTXSW_ADDR_TYPE_PPC) {
		for (ppc_num = 0; ppc_num < num_ppcs; ppc_num++) {
			list = nvgpu_netlist_get_ppc_compute_ctxsw_regs(g);
			for (i = 0; i < list->count; i++) {
				reg = &list->l[i];
				address = reg->addr;
				ppc_addr = pri_ppccs_addr_mask(address);
				base_address = gpc_base +
					(gpc_num * gpc_stride) +
					ppc_in_gpc_base +
					(ppc_num * ppc_in_gpc_stride);
				address = base_address + ppc_addr;
				/*
				 * The data for the PPCs is interleaved in the context buffer.
				 * Example with numPpcs = 2
				 * 0    1    2    3    4    5    6    7    8    9    10   11 ...
				 * 0-0  1-0  0-1  1-1  0-2  1-2  0-3  1-3  0-4  1-4  0-5  1-5 ...
				 */
				ppc_offset = (reg->index * num_ppcs) + (ppc_num * 4U);

				if (pri_addr == address)  {
					*priv_offset = ppc_offset;
					return 0;
				}
			}
#ifdef CONFIG_NVGPU_GRAPHICS
			list = nvgpu_netlist_get_ppc_gfx_ctxsw_regs(g);
			for (i = 0; i < list->count; i++) {
				reg = &list->l[i];
				address = reg->addr;
				ppc_addr = pri_ppccs_addr_mask(address);
				base_address = gpc_base +
					(gpc_num * gpc_stride) +
					ppc_in_gpc_base +
					(ppc_num * ppc_in_gpc_stride);
				address = base_address + ppc_addr;
				/*
				 * The data for the PPCs is interleaved in the context buffer.
				 * Example with numPpcs = 2
				 * 0    1    2    3    4    5    6    7    8    9    10   11 ...
				 * 0-0  1-0  0-1  1-1  0-2  1-2  0-3  1-3  0-4  1-4  0-5  1-5 ...
				 */
				ppc_offset = (reg->index * num_ppcs) + (ppc_num * 4U);

				if (pri_addr == address)  {
					*priv_offset = ppc_offset +
						nvgpu_netlist_get_ppc_compute_ctxsw_regs(g)->count * num_ppcs * 4U;
					return 0;
				}
			}
#endif
		}
	}

	/* Process the GPC segment. */
	if (addr_type == CTXSW_ADDR_TYPE_GPC) {
		list = nvgpu_netlist_get_gpc_compute_ctxsw_regs(g);
		for (i = 0; i < list->count; i++) {
			reg = &list->l[i];

			address = reg->addr;
			gpc_addr = pri_gpccs_addr_mask(g, address);
			gpc_offset = reg->index;

			base_address = gpc_base + (gpc_num * gpc_stride);
			address = base_address + gpc_addr;

			if (pri_addr == address) {
				*priv_offset = gpc_offset;
				return 0;
			}
		}
#ifdef CONFIG_NVGPU_GRAPHICS
		list = nvgpu_netlist_get_gpc_gfx_ctxsw_regs(g);
		for (i = 0; i < list->count; i++) {
			reg = &list->l[i];

			address = reg->addr;
			gpc_addr = pri_gpccs_addr_mask(g, address);
			gpc_offset = reg->index;

			base_address = gpc_base + (gpc_num * gpc_stride);
			address = base_address + gpc_addr;

			if (pri_addr == address) {
				*priv_offset = gpc_offset +
					nvgpu_netlist_get_gpc_compute_ctxsw_regs(g)->count * 4U;
				return 0;
			}
		}
#endif
	}
	return -EINVAL;
}
#endif
