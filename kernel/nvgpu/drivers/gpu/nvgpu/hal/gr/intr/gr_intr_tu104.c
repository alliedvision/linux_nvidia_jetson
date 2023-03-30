/*
 * Copyright (c) 2019-2021, NVIDIA CORPORATION.  All rights reserved.
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
#include <nvgpu/class.h>
#include <nvgpu/static_analysis.h>

#include <nvgpu/gr/config.h>

#include "gr_intr_gp10b.h"
#include "gr_intr_gv11b.h"
#include "gr_intr_tu104.h"

#include <nvgpu/hw/tu104/hw_gr_tu104.h>

#ifdef CONFIG_NVGPU_HAL_NON_FUSA
static void gr_tu104_set_sm_disp_ctrl(struct gk20a *g, u32 data)
{
	u32 reg_val;

	nvgpu_log_fn(g, " ");

	reg_val = nvgpu_readl(g, gr_gpcs_tpcs_sm_disp_ctrl_r());

	if ((data & NVC5C0_SET_SM_DISP_CTRL_COMPUTE_SHADER_QUAD_MASK)
	       == NVC5C0_SET_SM_DISP_CTRL_COMPUTE_SHADER_QUAD_DISABLE) {
		reg_val = set_field(reg_val,
		     gr_gpcs_tpcs_sm_disp_ctrl_compute_shader_quad_m(),
		     gr_gpcs_tpcs_sm_disp_ctrl_compute_shader_quad_disable_f()
		     );
	} else {
		if ((data & NVC5C0_SET_SM_DISP_CTRL_COMPUTE_SHADER_QUAD_MASK)
		     == NVC5C0_SET_SM_DISP_CTRL_COMPUTE_SHADER_QUAD_ENABLE) {
			reg_val = set_field(reg_val,
			     gr_gpcs_tpcs_sm_disp_ctrl_compute_shader_quad_m(),
			     gr_gpcs_tpcs_sm_disp_ctrl_compute_shader_quad_enable_f()
			     );
		}
	}

	nvgpu_writel(g, gr_gpcs_tpcs_sm_disp_ctrl_r(), reg_val);
}
#endif

int tu104_gr_intr_handle_sw_method(struct gk20a *g, u32 addr,
			      u32 class_num, u32 offset, u32 data)
{
	nvgpu_log_fn(g, " ");

#ifdef CONFIG_NVGPU_HAL_NON_FUSA
	if (class_num == TURING_COMPUTE_A) {
		switch (offset << 2) {
		case NVC5C0_SET_SHADER_EXCEPTIONS:
			g->ops.gr.intr.set_shader_exceptions(g, data);
			return 0;
		case NVC5C0_SET_SKEDCHECK:
			gv11b_gr_intr_set_skedcheck(g, data);
			return 0;
		case NVC5C0_SET_SM_DISP_CTRL:
			gr_tu104_set_sm_disp_ctrl(g, data);
			return 0;
		case NVC5C0_SET_SHADER_CUT_COLLECTOR:
			gv11b_gr_intr_set_shader_cut_collector(g, data);
			return 0;
		}
	}
#endif

#if defined(CONFIG_NVGPU_DEBUGGER) && defined(CONFIG_NVGPU_GRAPHICS)
	if (class_num == TURING_A) {
		switch (offset << 2) {
		case NVC597_SET_SHADER_EXCEPTIONS:
			g->ops.gr.intr.set_shader_exceptions(g, data);
			return 0;
		case NVC597_SET_CIRCULAR_BUFFER_SIZE:
			g->ops.gr.set_circular_buffer_size(g, data);
			return 0;
		case NVC597_SET_ALPHA_CIRCULAR_BUFFER_SIZE:
			g->ops.gr.set_alpha_circular_buffer_size(g, data);
			return 0;
		case NVC597_SET_GO_IDLE_TIMEOUT:
			gp10b_gr_intr_set_go_idle_timeout(g, data);
			return 0;
		case NVC097_SET_COALESCE_BUFFER_SIZE:
			gv11b_gr_intr_set_coalesce_buffer_size(g, data);
			return 0;
		case NVC597_SET_TEX_IN_DBG:
			gv11b_gr_intr_set_tex_in_dbg(g, data);
			return 0;
		case NVC597_SET_SKEDCHECK:
			gv11b_gr_intr_set_skedcheck(g, data);
			return 0;
		case NVC597_SET_BES_CROP_DEBUG3:
			g->ops.gr.set_bes_crop_debug3(g, data);
			return 0;
		case NVC597_SET_BES_CROP_DEBUG4:
			g->ops.gr.set_bes_crop_debug4(g, data);
			return 0;
		case NVC597_SET_SM_DISP_CTRL:
			gr_tu104_set_sm_disp_ctrl(g, data);
			return 0;
		case NVC597_SET_SHADER_CUT_COLLECTOR:
			gv11b_gr_intr_set_shader_cut_collector(g, data);
			return 0;
		}
	}
#endif

	return -EINVAL;
}

void tu104_gr_intr_enable_gpc_exceptions(struct gk20a *g,
					 struct nvgpu_gr_config *gr_config)
{
	u32 tpc_mask, tpc_mask_calc;

	nvgpu_writel(g, gr_gpcs_tpcs_tpccs_tpc_exception_en_r(),
			gr_gpcs_tpcs_tpccs_tpc_exception_en_sm_enabled_f());

	tpc_mask_calc = (u32)BIT32(
			 nvgpu_gr_config_get_max_tpc_per_gpc_count(gr_config));
	tpc_mask =
		gr_gpcs_gpccs_gpc_exception_en_tpc_f(
				nvgpu_safe_sub_u32(tpc_mask_calc, 1U));

	nvgpu_writel(g, gr_gpcs_gpccs_gpc_exception_en_r(),
		(tpc_mask | gr_gpcs_gpccs_gpc_exception_en_gcc_f(1U) |
			    gr_gpcs_gpccs_gpc_exception_en_gpccs_f(1U) |
			    gr_gpcs_gpccs_gpc_exception_en_gpcmmu_f(1U)));
}

static void gr_tu104_check_dma_exception(struct gk20a *g, u32 mme_hww_esr)
{
	if ((mme_hww_esr &
	     gr_mme_hww_esr_dma_dram_access_pending_f()) != 0U) {
		nvgpu_log(g, gpu_dbg_intr | gpu_dbg_gpu_dbg,
			 "GR MME EXCEPTION: DMA_DRAM_ACCESS_OUT_OF_BOUNDS");
	}

	if ((mme_hww_esr &
	     gr_mme_hww_esr_dma_illegal_fifo_pending_f()) != 0U) {
		nvgpu_log(g, gpu_dbg_intr | gpu_dbg_gpu_dbg,
			 "GR MME EXCEPTION: DMA_ILLEGAL_FIFO_CONFIG");
	}

	if ((mme_hww_esr &
	     gr_mme_hww_esr_dma_read_overflow_pending_f()) != 0U) {
		nvgpu_log(g, gpu_dbg_intr | gpu_dbg_gpu_dbg,
			 "GR MME EXCEPTION: DMA_READ_FIFOED_OVERFLOW");
	}

	if ((mme_hww_esr &
	     gr_mme_hww_esr_dma_fifo_resized_pending_f()) != 0U) {
		nvgpu_log(g, gpu_dbg_intr | gpu_dbg_gpu_dbg,
			 "GR MME EXCEPTION: DMA_FIFO_RESIZED_WHEN_NONIDLE");
	}

	if ((mme_hww_esr & gr_mme_hww_esr_dma_read_pb_pending_f()) != 0U) {
		nvgpu_log(g, gpu_dbg_intr | gpu_dbg_gpu_dbg,
			 "GR MME EXCEPTION: DMA_READ_FIFOED_FROM_PB");
	}
}

static void gr_tu104_check_ram_access_exception(struct gk20a *g, u32 mme_hww_esr)
{
	if ((mme_hww_esr & gr_mme_hww_esr_inst_ram_acess_pending_f()) != 0U) {
		nvgpu_log(g, gpu_dbg_intr | gpu_dbg_gpu_dbg,
			 "GR MME EXCEPTION: INSTR_RAM_ACCESS_OUT_OF_BOUNDS");
	}

	if ((mme_hww_esr & gr_mme_hww_esr_data_ram_access_pending_f()) != 0U) {
		nvgpu_log(g, gpu_dbg_intr | gpu_dbg_gpu_dbg,
			 "GR MME EXCEPTION: DATA_RAM_ACCESS_OUT_OF_BOUNDS");
	}
}

void tu104_gr_intr_log_mme_exception(struct gk20a *g)
{
	u32 mme_hww_esr = nvgpu_readl(g, gr_mme_hww_esr_r());
	u32 mme_hww_info = nvgpu_readl(g, gr_mme_hww_esr_info_r());

	gr_tu104_check_dma_exception(g, mme_hww_esr);
	gr_tu104_check_ram_access_exception(g, mme_hww_esr);

	if ((mme_hww_esr &
	     gr_mme_hww_esr_missing_macro_data_pending_f()) != 0U) {
		nvgpu_log(g, gpu_dbg_intr | gpu_dbg_gpu_dbg,
			 "GR MME EXCEPTION: MISSING_MACRO_DATA");
	}

	if ((mme_hww_esr &
	     gr_mme_hww_esr_illegal_mme_method_pending_f()) != 0U) {
		nvgpu_log(g, gpu_dbg_intr | gpu_dbg_gpu_dbg,
			 "GR MME EXCEPTION: ILLEGAL_MME_METHOD");
	}

	if ((mme_hww_esr & gr_mme_hww_esr_illegal_opcode_pending_f()) != 0U) {
		nvgpu_log(g, gpu_dbg_intr | gpu_dbg_gpu_dbg,
			 "GR MME EXCEPTION: ILLEGAL_OPCODE");
	}

	if ((mme_hww_esr & gr_mme_hww_esr_branch_in_delay_pending_f()) != 0U) {
		nvgpu_log(g, gpu_dbg_intr | gpu_dbg_gpu_dbg,
			 "GR MME EXCEPTION: BRANCH_IN_DELAY_SHOT");
	}

	if (gr_mme_hww_esr_info_pc_valid_v(mme_hww_info) == 0x1U) {
		nvgpu_log(g, gpu_dbg_intr | gpu_dbg_gpu_dbg,
			 "GR MME EXCEPTION: INFO2 0x%x, INFO3 0x%x, INFO4 0x%x",
			 nvgpu_readl(g, gr_mme_hww_esr_info2_r()),
			 nvgpu_readl(g, gr_mme_hww_esr_info3_r()),
			 nvgpu_readl(g, gr_mme_hww_esr_info4_r()));
	}
}
