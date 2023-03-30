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
#include <nvgpu/class.h>
#include <nvgpu/static_analysis.h>
#include <nvgpu/nvgpu_err.h>
#include <nvgpu/string.h>
#include <nvgpu/errata.h>

#include <nvgpu/gr/config.h>
#include <nvgpu/gr/config.h>
#include <nvgpu/gr/gr.h>
#include <nvgpu/gr/gr_instances.h>
#include <nvgpu/gr/gr_intr.h>
#include <nvgpu/gr/gr_falcon.h>


#include "common/gr/gr_priv.h"
#include "gr_intr_gp10b.h"
#include "gr_intr_gv11b.h"

#include <nvgpu/hw/gv11b/hw_gr_gv11b.h>

static u32 get_sm_hww_warp_esr_report_mask(void)
{
	u32 mask = gr_gpc0_tpc0_sm0_hww_warp_esr_report_mask_stack_error_report_f() |
		gr_gpc0_tpc0_sm0_hww_warp_esr_report_mask_api_stack_error_report_f() |
		gr_gpc0_tpc0_sm0_hww_warp_esr_report_mask_pc_wrap_report_f() |
		gr_gpc0_tpc0_sm0_hww_warp_esr_report_mask_misaligned_pc_report_f() |
		gr_gpc0_tpc0_sm0_hww_warp_esr_report_mask_pc_overflow_report_f() |
		gr_gpc0_tpc0_sm0_hww_warp_esr_report_mask_misaligned_reg_report_f() |
		gr_gpc0_tpc0_sm0_hww_warp_esr_report_mask_illegal_instr_encoding_report_f() |
		gr_gpc0_tpc0_sm0_hww_warp_esr_report_mask_illegal_instr_param_report_f() |
		gr_gpc0_tpc0_sm0_hww_warp_esr_report_mask_oor_reg_report_f() |
		gr_gpc0_tpc0_sm0_hww_warp_esr_report_mask_oor_addr_report_f() |
		gr_gpc0_tpc0_sm0_hww_warp_esr_report_mask_misaligned_addr_report_f() |
		gr_gpc0_tpc0_sm0_hww_warp_esr_report_mask_invalid_addr_space_report_f() |
		gr_gpc0_tpc0_sm0_hww_warp_esr_report_mask_invalid_const_addr_ldc_report_f() |
		gr_gpc0_tpc0_sm0_hww_warp_esr_report_mask_stack_overflow_report_f() |
		gr_gpc0_tpc0_sm0_hww_warp_esr_report_mask_mmu_fault_report_f() |
		gr_gpc0_tpc0_sm0_hww_warp_esr_report_mask_mmu_nack_report_f();

	return mask;
}

static u32 get_sm_hww_global_esr_report_mask(void)
{
	u32 mask = gr_gpc0_tpc0_sm0_hww_global_esr_report_mask_multiple_warp_errors_report_f() |
		gr_gpc0_tpc0_sm0_hww_global_esr_report_mask_bpt_int_report_f() |
		gr_gpc0_tpc0_sm0_hww_global_esr_report_mask_bpt_pause_report_f() |
		gr_gpc0_tpc0_sm0_hww_global_esr_report_mask_single_step_complete_report_f() |
		gr_gpc0_tpc0_sm0_hww_global_esr_report_mask_error_in_trap_report_f();

	return mask;
}

static void gv11b_gr_intr_handle_fecs_ecc_error(struct gk20a *g)
{
	struct nvgpu_fecs_ecc_status fecs_ecc_status;

	(void) memset(&fecs_ecc_status, 0, sizeof(fecs_ecc_status));

	g->ops.gr.falcon.handle_fecs_ecc_error(g, &fecs_ecc_status);

	g->ecc.gr.fecs_ecc_corrected_err_count[0].counter =
	   nvgpu_safe_add_u32(
		g->ecc.gr.fecs_ecc_corrected_err_count[0].counter,
		fecs_ecc_status.corrected_delta);
	g->ecc.gr.fecs_ecc_uncorrected_err_count[0].counter =
	   nvgpu_safe_add_u32(
		g->ecc.gr.fecs_ecc_uncorrected_err_count[0].counter,
		fecs_ecc_status.uncorrected_delta);

	if (fecs_ecc_status.imem_corrected_err) {
		nvgpu_report_err_to_sdl(g, NVGPU_ERR_MODULE_FECS,
				GPU_FECS_FALCON_IMEM_ECC_CORRECTED);
		nvgpu_err(g, "imem ecc error corrected - error count:%d",
			g->ecc.gr.fecs_ecc_corrected_err_count[0].counter);
	}
	if (fecs_ecc_status.imem_uncorrected_err) {
		nvgpu_report_err_to_sdl(g, NVGPU_ERR_MODULE_FECS,
				GPU_FECS_FALCON_IMEM_ECC_UNCORRECTED);
		nvgpu_err(g, "imem ecc error uncorrected - error count:%d",
			g->ecc.gr.fecs_ecc_uncorrected_err_count[0].counter);
	}
	if (fecs_ecc_status.dmem_corrected_err) {
		nvgpu_err(g, "unexpected dmem ecc error corrected - count: %d",
			g->ecc.gr.fecs_ecc_corrected_err_count[0].counter);
		/* This error is not expected to occur in gv11b and hence,
		 * this scenario is considered as a fatal error.
		 */
		BUG();
	}
	if (fecs_ecc_status.dmem_uncorrected_err) {
		nvgpu_report_err_to_sdl(g, NVGPU_ERR_MODULE_FECS,
				GPU_FECS_FALCON_DMEM_ECC_UNCORRECTED);
		nvgpu_err(g, "dmem ecc error uncorrected - error count %d",
			g->ecc.gr.fecs_ecc_uncorrected_err_count[0].counter);
	}
}

int gv11b_gr_intr_handle_fecs_error(struct gk20a *g,
				struct nvgpu_channel *ch_ptr,
				struct nvgpu_gr_isr_data *isr_data)
{
	nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gpu_dbg | gpu_dbg_intr, " ");

	/* Handle ECC errors */
	gv11b_gr_intr_handle_fecs_ecc_error(g);

	return gp10b_gr_intr_handle_fecs_error(g, ch_ptr, isr_data);
}

#ifdef CONFIG_NVGPU_HAL_NON_FUSA
void gv11b_gr_intr_set_skedcheck(struct gk20a *g, u32 data)
{
	u32 reg_val;

	reg_val = nvgpu_readl(g, gr_sked_hww_esr_en_r());

	if ((data & NVC397_SET_SKEDCHECK_18_MASK) ==
			NVC397_SET_SKEDCHECK_18_DISABLE) {
		reg_val = set_field(reg_val,
		 gr_sked_hww_esr_en_skedcheck18_l1_config_too_small_m(),
		 gr_sked_hww_esr_en_skedcheck18_l1_config_too_small_disabled_f()
		 );
	} else {
		if ((data & NVC397_SET_SKEDCHECK_18_MASK) ==
				NVC397_SET_SKEDCHECK_18_ENABLE) {
			reg_val = set_field(reg_val,
			 gr_sked_hww_esr_en_skedcheck18_l1_config_too_small_m(),
			 gr_sked_hww_esr_en_skedcheck18_l1_config_too_small_enabled_f()
			 );
		}
	}
	nvgpu_log_info(g, "sked_hww_esr_en = 0x%x", reg_val);
	nvgpu_writel(g, gr_sked_hww_esr_en_r(), reg_val);

}

void gv11b_gr_intr_set_shader_cut_collector(struct gk20a *g, u32 data)
{
	u32 val;

	nvgpu_log_fn(g, "gr_gv11b_set_shader_cut_collector");

	val = nvgpu_readl(g, gr_gpcs_tpcs_sm_l1tag_ctrl_r());
	if ((data & NVC397_SET_SHADER_CUT_COLLECTOR_STATE_ENABLE) != 0U) {
		val = set_field(val,
		 gr_gpcs_tpcs_sm_l1tag_ctrl_always_cut_collector_m(),
		 gr_gpcs_tpcs_sm_l1tag_ctrl_always_cut_collector_enable_f());
	} else {
		val = set_field(val,
		 gr_gpcs_tpcs_sm_l1tag_ctrl_always_cut_collector_m(),
		 gr_gpcs_tpcs_sm_l1tag_ctrl_always_cut_collector_disable_f());
	}
	nvgpu_writel(g, gr_gpcs_tpcs_sm_l1tag_ctrl_r(), val);
}
#endif

int gv11b_gr_intr_handle_sw_method(struct gk20a *g, u32 addr,
				     u32 class_num, u32 offset, u32 data)
{
	int err = -EFAULT;

	(void)addr;
	(void)class_num;
	(void)offset;
	(void)data;

	nvgpu_log_fn(g, " ");

#ifdef CONFIG_NVGPU_HAL_NON_FUSA
	if (class_num == VOLTA_COMPUTE_A) {
		switch (offset << 2) {
		case NVC0C0_SET_SHADER_EXCEPTIONS:
			g->ops.gr.intr.set_shader_exceptions(g, data);
			err = 0;
			break;
		case NVC3C0_SET_SKEDCHECK:
			gv11b_gr_intr_set_skedcheck(g, data);
			err = 0;
			break;
		case NVC3C0_SET_SHADER_CUT_COLLECTOR:
			gv11b_gr_intr_set_shader_cut_collector(g, data);
			err = 0;
			break;
		default:
			err = -EINVAL;
			break;
		}
	}
#endif

#if defined(CONFIG_NVGPU_DEBUGGER) && defined(CONFIG_NVGPU_GRAPHICS)
	if (class_num == VOLTA_A) {
		switch (offset << 2) {
		case NVC397_SET_SHADER_EXCEPTIONS:
			g->ops.gr.intr.set_shader_exceptions(g, data);
			err = 0;
			break;
		case NVC397_SET_CIRCULAR_BUFFER_SIZE:
			g->ops.gr.set_circular_buffer_size(g, data);
			err = 0;
			break;
		case NVC397_SET_ALPHA_CIRCULAR_BUFFER_SIZE:
			g->ops.gr.set_alpha_circular_buffer_size(g, data);
			err = 0;
			break;
		case NVC397_SET_GO_IDLE_TIMEOUT:
			gp10b_gr_intr_set_go_idle_timeout(g, data);
			err = 0;
			break;
		case NVC097_SET_COALESCE_BUFFER_SIZE:
			gv11b_gr_intr_set_coalesce_buffer_size(g, data);
			err = 0;
			break;
		case NVC397_SET_TEX_IN_DBG:
			gv11b_gr_intr_set_tex_in_dbg(g, data);
			err = 0;
			break;
		case NVC397_SET_SKEDCHECK:
			gv11b_gr_intr_set_skedcheck(g, data);
			err = 0;
			break;
		case NVC397_SET_BES_CROP_DEBUG3:
			g->ops.gr.set_bes_crop_debug3(g, data);
			err = 0;
			break;
		case NVC397_SET_BES_CROP_DEBUG4:
			g->ops.gr.set_bes_crop_debug4(g, data);
			err = 0;
			break;
		case NVC397_SET_SHADER_CUT_COLLECTOR:
			gv11b_gr_intr_set_shader_cut_collector(g, data);
			err = 0;
			break;
		default:
			err = -EINVAL;
			break;
		}
	}
#endif

	return err;
}

void gv11b_gr_intr_handle_gcc_exception(struct gk20a *g, u32 gpc,
				u32 gpc_exception,
				u32 *corrected_err, u32 *uncorrected_err)
{
	u32 offset = nvgpu_gr_gpc_offset(g, gpc);
	u32 gcc_l15_ecc_status, gcc_l15_ecc_corrected_err_status = 0;
	u32 gcc_l15_ecc_uncorrected_err_status = 0;
	u32 gcc_l15_corrected_err_count_delta = 0;
	u32 gcc_l15_uncorrected_err_count_delta = 0;
	bool is_gcc_l15_ecc_corrected_total_err_overflow = false;
	bool is_gcc_l15_ecc_uncorrected_total_err_overflow = false;

	(void)corrected_err;

	if (gr_gpc0_gpccs_gpc_exception_gcc_v(gpc_exception) == 0U) {
		return;
	}

	/* Check for gcc l15 ECC errors. */
	gcc_l15_ecc_status = nvgpu_readl(g,
		nvgpu_safe_add_u32(
			gr_pri_gpc0_gcc_l15_ecc_status_r(), offset));
	gcc_l15_ecc_corrected_err_status = gcc_l15_ecc_status &
		(gr_pri_gpc0_gcc_l15_ecc_status_corrected_err_bank0_m() |
		 gr_pri_gpc0_gcc_l15_ecc_status_corrected_err_bank1_m());
	gcc_l15_ecc_uncorrected_err_status = gcc_l15_ecc_status &
		(gr_pri_gpc0_gcc_l15_ecc_status_uncorrected_err_bank0_m() |
		 gr_pri_gpc0_gcc_l15_ecc_status_uncorrected_err_bank1_m());

	if ((gcc_l15_ecc_corrected_err_status == 0U) &&
	    (gcc_l15_ecc_uncorrected_err_status == 0U)) {
		return;
	}

	gcc_l15_corrected_err_count_delta =
		gr_pri_gpc0_gcc_l15_ecc_corrected_err_count_total_v(
		 nvgpu_readl(g, nvgpu_safe_add_u32(
			     gr_pri_gpc0_gcc_l15_ecc_corrected_err_count_r(),
			     offset)));
	gcc_l15_uncorrected_err_count_delta =
		gr_pri_gpc0_gcc_l15_ecc_uncorrected_err_count_total_v(
		 nvgpu_readl(g, nvgpu_safe_add_u32(
			gr_pri_gpc0_gcc_l15_ecc_uncorrected_err_count_r(),
			offset)));
	is_gcc_l15_ecc_corrected_total_err_overflow =
	 gr_pri_gpc0_gcc_l15_ecc_status_corrected_err_total_counter_overflow_v(
						gcc_l15_ecc_status) != 0U;
	is_gcc_l15_ecc_uncorrected_total_err_overflow =
	 gr_pri_gpc0_gcc_l15_ecc_status_uncorrected_err_total_counter_overflow_v(
						gcc_l15_ecc_status) != 0U;

	if ((gcc_l15_corrected_err_count_delta > 0U) ||
	    is_gcc_l15_ecc_corrected_total_err_overflow) {
		nvgpu_err(g,
			"unexpected corrected error (SBE) detected in GCC L1.5!"
			"err_mask [%08x] is_overf [%d]",
			gcc_l15_ecc_corrected_err_status,
			is_gcc_l15_ecc_corrected_total_err_overflow);

		/* This error is not expected to occur in gv11b and hence,
		 * this scenario is considered as a fatal error.
		 */
		BUG();
	}
	if ((gcc_l15_uncorrected_err_count_delta > 0U) ||
	    is_gcc_l15_ecc_uncorrected_total_err_overflow) {
		nvgpu_err(g,
			"Uncorrected error (DBE) detected in GCC L1.5!"
			"err_mask [%08x] is_overf [%d]",
			gcc_l15_ecc_uncorrected_err_status,
			is_gcc_l15_ecc_uncorrected_total_err_overflow);

		/* HW uses 16-bits counter */
		if (is_gcc_l15_ecc_uncorrected_total_err_overflow) {
			gcc_l15_uncorrected_err_count_delta =
			   nvgpu_safe_add_u32(
				gcc_l15_uncorrected_err_count_delta,
			BIT32(
			gr_pri_gpc0_gcc_l15_ecc_uncorrected_err_count_total_s()
			));
		}
		*uncorrected_err = nvgpu_safe_add_u32(*uncorrected_err,
					gcc_l15_uncorrected_err_count_delta);
		nvgpu_report_err_to_sdl(g, NVGPU_ERR_MODULE_GCC,
				GPU_GCC_L15_ECC_UNCORRECTED);
		nvgpu_writel(g, nvgpu_safe_add_u32(
		 gr_pri_gpc0_gcc_l15_ecc_uncorrected_err_count_r(), offset),
		 0);
	}

	nvgpu_writel(g, nvgpu_safe_add_u32(
			gr_pri_gpc0_gcc_l15_ecc_status_r(), offset),
			gr_pri_gpc0_gcc_l15_ecc_status_reset_task_f());
}

static void gv11b_gr_intr_report_gpcmmu_ecc_err(struct gk20a *g,
			u32 ecc_status, u32 gpc)
{
	if ((ecc_status &
	     gr_gpc0_mmu_l1tlb_ecc_status_corrected_err_l1tlb_sa_data_m()) !=
									0U) {
		nvgpu_err(g, "unexpected corrected ecc sa data error");
		/* This error is not expected to occur in gv11b and hence,
		 * this scenario is considered as a fatal error.
		 */
		BUG();
	}
	if ((ecc_status &
	     gr_gpc0_mmu_l1tlb_ecc_status_uncorrected_err_l1tlb_sa_data_m()) !=
									 0U) {
		nvgpu_report_err_to_sdl(g, NVGPU_ERR_MODULE_MMU,
				GPU_MMU_L1TLB_SA_DATA_ECC_UNCORRECTED);
		nvgpu_err(g, "uncorrected ecc sa data error. gpc_id(%d)", gpc);
	}
	if ((ecc_status &
	     gr_gpc0_mmu_l1tlb_ecc_status_corrected_err_l1tlb_fa_data_m()) !=
									0U) {
		nvgpu_err(g, "unexpected corrected ecc fa data error");
		/* This error is not expected to occur in gv11b and hence,
		 * this scenario is considered as a fatal error.
		 */
		BUG();
	}
	if ((ecc_status &
	     gr_gpc0_mmu_l1tlb_ecc_status_uncorrected_err_l1tlb_fa_data_m()) !=
									0U) {
		nvgpu_report_err_to_sdl(g, NVGPU_ERR_MODULE_MMU,
				GPU_MMU_L1TLB_FA_DATA_ECC_UNCORRECTED);
		nvgpu_err(g, "uncorrected ecc fa data error. gpc_id(%d)", gpc);
	}
}

void gv11b_gr_intr_handle_gpc_gpcmmu_exception(struct gk20a *g, u32 gpc,
		u32 gpc_exception, u32 *corrected_err, u32 *uncorrected_err)
{
	u32 offset = nvgpu_gr_gpc_offset(g, gpc);
	u32 ecc_status, ecc_addr, corrected_cnt, uncorrected_cnt;
	u32 corrected_delta, uncorrected_delta;
	u32 corrected_overflow, uncorrected_overflow;
	u32 hww_esr;

	if ((gpc_exception & gr_gpc0_gpccs_gpc_exception_gpcmmu_m()) == 0U) {
		return;
	}

	hww_esr = nvgpu_readl(g,
			nvgpu_safe_add_u32(gr_gpc0_mmu_gpcmmu_global_esr_r(),
						offset));

	if ((hww_esr & (gr_gpc0_mmu_gpcmmu_global_esr_ecc_corrected_m() |
		gr_gpc0_mmu_gpcmmu_global_esr_ecc_uncorrected_m())) == 0U) {
		return;
	}

	ecc_status = nvgpu_readl(g, nvgpu_safe_add_u32(
		gr_gpc0_mmu_l1tlb_ecc_status_r(), offset));
	ecc_addr = nvgpu_readl(g, nvgpu_safe_add_u32(
		gr_gpc0_mmu_l1tlb_ecc_address_r(), offset));
	corrected_cnt = nvgpu_readl(g, nvgpu_safe_add_u32(
		gr_gpc0_mmu_l1tlb_ecc_corrected_err_count_r(), offset));
	uncorrected_cnt = nvgpu_readl(g, nvgpu_safe_add_u32(
		gr_gpc0_mmu_l1tlb_ecc_uncorrected_err_count_r(), offset));

	corrected_delta = gr_gpc0_mmu_l1tlb_ecc_corrected_err_count_total_v(
							corrected_cnt);
	uncorrected_delta =
		gr_gpc0_mmu_l1tlb_ecc_uncorrected_err_count_total_v(
							uncorrected_cnt);
	corrected_overflow = ecc_status &
	 gr_gpc0_mmu_l1tlb_ecc_status_corrected_err_total_counter_overflow_m();

	uncorrected_overflow = ecc_status &
	 gr_gpc0_mmu_l1tlb_ecc_status_uncorrected_err_total_counter_overflow_m();

	/* clear the interrupt */
	if ((corrected_delta > 0U) || (corrected_overflow != 0U)) {
		nvgpu_writel(g, nvgpu_safe_add_u32(
			gr_gpc0_mmu_l1tlb_ecc_corrected_err_count_r(),
			offset), 0);
	}
	if ((uncorrected_delta > 0U) || (uncorrected_overflow != 0U)) {
		nvgpu_writel(g, nvgpu_safe_add_u32(
			gr_gpc0_mmu_l1tlb_ecc_uncorrected_err_count_r(),
			offset), 0);
	}

	nvgpu_writel(g, nvgpu_safe_add_u32(
				gr_gpc0_mmu_l1tlb_ecc_status_r(), offset),
				gr_gpc0_mmu_l1tlb_ecc_status_reset_task_f());

	/* Handle overflow */
	if (corrected_overflow != 0U) {
		corrected_delta = nvgpu_safe_add_u32(corrected_delta,
		   BIT32(gr_gpc0_mmu_l1tlb_ecc_corrected_err_count_total_s()));
		nvgpu_err(g, "mmu l1tlb ecc counter corrected overflow!");
	}
	if (uncorrected_overflow != 0U) {
		uncorrected_delta = nvgpu_safe_add_u32(uncorrected_delta,
		  BIT32(gr_gpc0_mmu_l1tlb_ecc_uncorrected_err_count_total_s()));
		nvgpu_err(g, "mmu l1tlb ecc counter uncorrected overflow!");
	}

	*corrected_err = nvgpu_safe_add_u32(*corrected_err, corrected_delta);
	*uncorrected_err = nvgpu_safe_add_u32(
				*uncorrected_err, uncorrected_delta);

	nvgpu_err(g, "mmu l1tlb gpc:%d ecc interrupt intr: 0x%x",
			gpc, hww_esr);

	gv11b_gr_intr_report_gpcmmu_ecc_err(g, ecc_status, gpc);

	nvgpu_err(g, "ecc error address: 0x%x", ecc_addr);
	nvgpu_err(g, "ecc error count corrected: %d, uncorrected %d",
		(u32)*corrected_err, (u32)*uncorrected_err);
}

static void gv11b_gr_intr_report_gpccs_ecc_err(struct gk20a *g,
			u32 ecc_status, u32 ecc_addr, u32 gpc)
{
	if ((ecc_status &
	     gr_gpc0_gpccs_falcon_ecc_status_corrected_err_imem_m()) != 0U) {
		nvgpu_report_err_to_sdl(g, NVGPU_ERR_MODULE_GPCCS,
				GPU_GPCCS_FALCON_IMEM_ECC_CORRECTED);
		nvgpu_err(g, "imem ecc error corrected"
				"ecc_addr(0x%x), gpc_id(%d)", ecc_addr, gpc);
	}
	if ((ecc_status &
	     gr_gpc0_gpccs_falcon_ecc_status_uncorrected_err_imem_m()) != 0U) {
		nvgpu_report_err_to_sdl(g, NVGPU_ERR_MODULE_GPCCS,
				GPU_GPCCS_FALCON_IMEM_ECC_UNCORRECTED);
		nvgpu_err(g, "imem ecc error uncorrected"
				"ecc_addr(0x%x), gpc_id(%d)", ecc_addr, gpc);
	}
	if ((ecc_status &
	     gr_gpc0_gpccs_falcon_ecc_status_corrected_err_dmem_m()) != 0U) {
		nvgpu_err(g, "unexpected dmem ecc error corrected");
		/* This error is not expected to occur in gv11b and hence,
		 * this scenario is considered as a fatal error.
		 */
		BUG();
	}
	if ((ecc_status &
	     gr_gpc0_gpccs_falcon_ecc_status_uncorrected_err_dmem_m()) != 0U) {
		nvgpu_report_err_to_sdl(g, NVGPU_ERR_MODULE_GPCCS,
				GPU_GPCCS_FALCON_DMEM_ECC_UNCORRECTED);
		nvgpu_err(g, "dmem ecc error uncorrected"
				"ecc_addr(0x%x), gpc_id(%d)", ecc_addr, gpc);
	}
}

void gv11b_gr_intr_handle_gpc_prop_exception(struct gk20a *g, u32 gpc,
		u32 gpc_exception)
{
	u32 offset = nvgpu_gr_gpc_offset(g, gpc);
	u32 hww_esr;

	if ((gpc_exception & gr_gpc0_gpccs_gpc_exception_prop_m()) == 0U) {
		return;
	}

	hww_esr = nvgpu_readl(g,
			nvgpu_safe_add_u32(gr_gpc0_prop_hww_esr_r(), offset));

	nvgpu_report_err_to_sdl(g, NVGPU_ERR_MODULE_PGRAPH,
			GPU_PGRAPH_GPC_GFX_PROP_EXCEPTION);

	/*
	 * print additional diagnostic information.
	 */
	nvgpu_err(g,
		"prop hww: (0x%x 0x%x 0x%x 0x%x 0x%x 0x%x)",
		hww_esr,
		nvgpu_readl(g,
			nvgpu_safe_add_u32(gr_gpc0_prop_hww_esr_coord_r(),
				offset)),
		nvgpu_readl(g,
			nvgpu_safe_add_u32(gr_gpc0_prop_hww_esr_format_r(),
				offset)),
		nvgpu_readl(g,
			nvgpu_safe_add_u32(gr_gpc0_prop_hww_esr_state_r(),
				offset)),
		nvgpu_readl(g,
			nvgpu_safe_add_u32(gr_gpc0_prop_hww_esr_state2_r(),
				offset)),
		nvgpu_readl(g,
			nvgpu_safe_add_u32(gr_gpc0_prop_hww_esr_offset_r(),
				offset)));

	/* clear the interrupt */
	nvgpu_writel(g, nvgpu_safe_add_u32(
				gr_gpc0_prop_hww_esr_r(), offset),
				gr_gpc0_prop_hww_esr_reset_active_f());

}

void gv11b_gr_intr_handle_gpc_zcull_exception(struct gk20a *g, u32 gpc,
		u32 gpc_exception)
{
	u32 offset = nvgpu_gr_gpc_offset(g, gpc);
	u32 hww_esr;

	if ((gpc_exception & gr_gpc0_gpccs_gpc_exception_zcull_m()) == 0U) {
		return;
	}

	hww_esr = nvgpu_readl(g,
			nvgpu_safe_add_u32(gr_gpc0_zcull_hww_esr_r(), offset));

	nvgpu_report_err_to_sdl(g, NVGPU_ERR_MODULE_PGRAPH,
			GPU_PGRAPH_GPC_GFX_ZCULL_EXCEPTION);

	/* clear the interrupt */
	nvgpu_writel(g, nvgpu_safe_add_u32(
				gr_gpc0_zcull_hww_esr_r(), offset),
				gr_gpc0_zcull_hww_esr_reset_active_f());

	nvgpu_err(g, "gpc:%d zcull interrupt intr: 0x%x", gpc, hww_esr);
}

void gv11b_gr_intr_handle_gpc_setup_exception(struct gk20a *g, u32 gpc,
		u32 gpc_exception)
{
	u32 offset = nvgpu_gr_gpc_offset(g, gpc);
	u32 hww_esr;

	if ((gpc_exception & gr_gpc0_gpccs_gpc_exception_setup_m()) == 0U) {
		return;
	}

	hww_esr = nvgpu_readl(g,
			nvgpu_safe_add_u32(gr_gpc0_setup_hww_esr_r(), offset));

	nvgpu_report_err_to_sdl(g, NVGPU_ERR_MODULE_PGRAPH,
			GPU_PGRAPH_GPC_GFX_SETUP_EXCEPTION);

	/* clear the interrupt */
	nvgpu_writel(g, nvgpu_safe_add_u32(
				gr_gpc0_setup_hww_esr_r(), offset),
				gr_gpc0_setup_hww_esr_reset_active_f());

	nvgpu_err(g, "gpc:%d setup interrupt intr: 0x%x", gpc, hww_esr);
}

void gv11b_gr_intr_handle_gpc_pes_exception(struct gk20a *g, u32 gpc,
		u32 gpc_exception)
{
	u32 gpc_offset = nvgpu_gr_gpc_offset(g, gpc);
	u32 ppc_in_gpc_stride =
		nvgpu_get_litter_value(g, GPU_LIT_PPC_IN_GPC_STRIDE);
	u32 reg_offset = 0U;
	u32 hww_esr;
	u32 pes_pending_masks[] = {
		gr_gpc0_gpccs_gpc_exception_pes0_m(),
		gr_gpc0_gpccs_gpc_exception_pes1_m()
	};
	u32 num_pes_pending_masks =
		sizeof(pes_pending_masks)/sizeof(*pes_pending_masks);
	u32 i = 0U;
	struct nvgpu_gr *gr = nvgpu_gr_get_cur_instance_ptr(g);
	struct nvgpu_gr_config *config = gr->config;
	u32 pes_id;

	if (((gpc_exception & gr_gpc0_gpccs_gpc_exception_pes0_m()) == 0U) &&
			((gpc_exception & gr_gpc0_gpccs_gpc_exception_pes1_m())
			 == 0U)) {
		return;
	}

	for (i = 0U; i < num_pes_pending_masks; i++) {
		pes_id = i;
		if ((gpc_exception & pes_pending_masks[i]) == 0U) {
			continue;
		}
		if (nvgpu_is_errata_present(g, NVGPU_ERRATA_3524791)) {
			pes_id = gr_config_get_gpc_pes_logical_id_map(
					config, gpc)[i];
			nvgpu_assert(pes_id != UINT_MAX);
		}
		reg_offset = nvgpu_safe_add_u32(gr_gpc0_ppc0_pes_hww_esr_r(),
				gpc_offset);
		reg_offset = nvgpu_safe_add_u32(reg_offset, nvgpu_safe_mult_u32(
					ppc_in_gpc_stride, pes_id));
		hww_esr = nvgpu_readl(g, reg_offset);

		nvgpu_report_err_to_sdl(g, NVGPU_ERR_MODULE_PGRAPH,
			GPU_PGRAPH_GPC_GFX_PES_EXCEPTION);

		/* clear the interrupt */
		nvgpu_writel(g, reg_offset,
				gr_gpc0_ppc0_pes_hww_esr_reset_task_f());

		nvgpu_err(g, "gpc:%d pes:%d interrupt intr: 0x%x", gpc, i,
				hww_esr);
	}
}

void gv11b_gr_intr_handle_gpc_gpccs_exception(struct gk20a *g, u32 gpc,
		u32 gpc_exception, u32 *corrected_err, u32 *uncorrected_err)
{
	u32 offset = nvgpu_gr_gpc_offset(g, gpc);
	u32 ecc_status, ecc_addr, corrected_cnt, uncorrected_cnt;
	u32 corrected_delta, uncorrected_delta;
	u32 corrected_overflow, uncorrected_overflow;
	u32 hww_esr;

	if ((gpc_exception & gr_gpc0_gpccs_gpc_exception_gpccs_m()) == 0U) {
		return;
	}

	hww_esr = nvgpu_readl(g,
			nvgpu_safe_add_u32(gr_gpc0_gpccs_hww_esr_r(),
						offset));

	if ((hww_esr & (gr_gpc0_gpccs_hww_esr_ecc_uncorrected_m() |
			gr_gpc0_gpccs_hww_esr_ecc_corrected_m())) == 0U) {
		return;
	}

	ecc_status = nvgpu_readl(g, nvgpu_safe_add_u32(
		gr_gpc0_gpccs_falcon_ecc_status_r(), offset));
	ecc_addr = nvgpu_readl(g, nvgpu_safe_add_u32(
		gr_gpc0_gpccs_falcon_ecc_address_r(), offset));
	corrected_cnt = nvgpu_readl(g, nvgpu_safe_add_u32(
		gr_gpc0_gpccs_falcon_ecc_corrected_err_count_r(), offset));
	uncorrected_cnt = nvgpu_readl(g, nvgpu_safe_add_u32(
		gr_gpc0_gpccs_falcon_ecc_uncorrected_err_count_r(), offset));

	corrected_delta =
		gr_gpc0_gpccs_falcon_ecc_corrected_err_count_total_v(
							corrected_cnt);
	uncorrected_delta =
		gr_gpc0_gpccs_falcon_ecc_uncorrected_err_count_total_v(
							uncorrected_cnt);
	corrected_overflow = ecc_status &
	 gr_gpc0_gpccs_falcon_ecc_status_corrected_err_total_counter_overflow_m();

	uncorrected_overflow = ecc_status &
	 gr_gpc0_gpccs_falcon_ecc_status_uncorrected_err_total_counter_overflow_m();


	/* clear the interrupt */
	if ((corrected_delta > 0U) || (corrected_overflow != 0U)) {
		nvgpu_writel(g, nvgpu_safe_add_u32(
			gr_gpc0_gpccs_falcon_ecc_corrected_err_count_r(),
			offset), 0);
	}
	if ((uncorrected_delta > 0U) || (uncorrected_overflow != 0U)) {
		nvgpu_writel(g, nvgpu_safe_add_u32(
			gr_gpc0_gpccs_falcon_ecc_uncorrected_err_count_r(),
			offset), 0);
	}

	nvgpu_writel(g, nvgpu_safe_add_u32(
			gr_gpc0_gpccs_falcon_ecc_status_r(), offset),
			gr_gpc0_gpccs_falcon_ecc_status_reset_task_f());

	*corrected_err = nvgpu_safe_add_u32(*corrected_err, corrected_delta);
	*uncorrected_err = nvgpu_safe_add_u32(
				*uncorrected_err, uncorrected_delta);

	nvgpu_err(g, "gppcs gpc:%d ecc interrupt intr: 0x%x", gpc, hww_esr);

	gv11b_gr_intr_report_gpccs_ecc_err(g, ecc_status, ecc_addr, gpc);

	if ((corrected_overflow != 0U) || (uncorrected_overflow != 0U)) {
		nvgpu_err(g, "gpccs ecc counter overflow!");
	}

	nvgpu_err(g, "ecc error row address: 0x%x",
		gr_gpc0_gpccs_falcon_ecc_address_row_address_v(ecc_addr));

	nvgpu_err(g, "ecc error count corrected: %d, uncorrected %d",
			(u32)*corrected_err, (u32)*uncorrected_err);
}

void gv11b_gr_intr_handle_tpc_mpc_exception(struct gk20a *g, u32 gpc, u32 tpc)
{
	u32 esr;
	u32 gpc_offset, tpc_offset, offset;

	gpc_offset = nvgpu_gr_gpc_offset(g, gpc);
	tpc_offset = nvgpu_gr_tpc_offset(g, tpc);
	offset = nvgpu_safe_add_u32(gpc_offset, tpc_offset);

	esr = nvgpu_readl(g,
			nvgpu_safe_add_u32(gr_gpc0_tpc0_mpc_hww_esr_r(),
						offset));
	nvgpu_err(g, "mpc hww esr 0x%08x", esr);

	nvgpu_report_err_to_sdl(g, NVGPU_ERR_MODULE_PGRAPH,
			GPU_PGRAPH_MPC_EXCEPTION);

	esr = nvgpu_readl(g,
			nvgpu_safe_add_u32(gr_gpc0_tpc0_mpc_hww_esr_info_r(),
						offset));
	nvgpu_log(g, gpu_dbg_intr | gpu_dbg_gpu_dbg,
			"mpc hww esr info: veid 0x%08x",
			gr_gpc0_tpc0_mpc_hww_esr_info_veid_v(esr));

	nvgpu_writel(g,
		     nvgpu_safe_add_u32(gr_gpc0_tpc0_mpc_hww_esr_r(),
						offset),
		     gr_gpc0_tpc0_mpc_hww_esr_reset_trigger_f());
}

void gv11b_gr_intr_handle_tpc_pe_exception(struct gk20a *g, u32 gpc, u32 tpc)
{
	u32 esr;
	u32 gpc_offset, tpc_offset, offset;

	gpc_offset = nvgpu_gr_gpc_offset(g, gpc);
	tpc_offset = nvgpu_gr_tpc_offset(g, tpc);
	offset = nvgpu_safe_add_u32(gpc_offset, tpc_offset);

	esr = nvgpu_readl(g, nvgpu_safe_add_u32(gr_gpc0_tpc0_pe_hww_esr_r(),
						offset));
	nvgpu_report_err_to_sdl(g, NVGPU_ERR_MODULE_PGRAPH,
			GPU_PGRAPH_GPC_GFX_TPC_PE_EXCEPTION);
	nvgpu_err (g, "Gpc Gfx tpc pe exception");

	nvgpu_log(g, gpu_dbg_intr | gpu_dbg_gpu_dbg, "pe hww esr 0x%08x", esr);

	nvgpu_writel(g, nvgpu_safe_add_u32(gr_gpc0_tpc0_pe_hww_esr_r(), offset),
		     gr_gpc0_tpc0_pe_hww_esr_reset_task_f());
}

void gv11b_gr_intr_enable_hww_exceptions(struct gk20a *g)
{
	/* enable exceptions */

	nvgpu_writel(g, gr_fe_hww_esr_r(),
		     gr_fe_hww_esr_en_enable_f() |
		     gr_fe_hww_esr_reset_active_f());
	nvgpu_writel(g, gr_memfmt_hww_esr_r(),
		     gr_memfmt_hww_esr_en_enable_f() |
		     gr_memfmt_hww_esr_reset_active_f());
	/*
	 * PD, SCC, DS, SSYNC - SYS Graphics Units.
	 * Accessible only in legacy mode (graphics+compute).
	 */
	if (!nvgpu_is_enabled(g, NVGPU_SUPPORT_MIG)) {
		nvgpu_writel(g, gr_pd_hww_esr_r(),
				 gr_pd_hww_esr_en_enable_f() |
				 gr_pd_hww_esr_reset_active_f());
		nvgpu_writel(g, gr_scc_hww_esr_r(),
				 gr_scc_hww_esr_en_enable_f() |
				 gr_scc_hww_esr_reset_active_f());
		nvgpu_writel(g, gr_ds_hww_esr_r(),
				 gr_ds_hww_esr_en_enabled_f() |
				 gr_ds_hww_esr_reset_task_f());
		nvgpu_writel(g, gr_ssync_hww_esr_r(),
		        gr_ssync_hww_esr_en_enable_f() |
		        gr_ssync_hww_esr_reset_active_f());
	}
	nvgpu_writel(g, gr_mme_hww_esr_r(),
		     gr_mme_hww_esr_en_enable_f() |
		     gr_mme_hww_esr_reset_active_f());

	/* For now leave POR values */
	nvgpu_log(g, gpu_dbg_info, "gr_sked_hww_esr_en_r 0x%08x",
			nvgpu_readl(g, gr_sked_hww_esr_en_r()));
}

void gv11b_gr_intr_enable_exceptions(struct gk20a *g,
				     struct nvgpu_gr_config *gr_config,
				     bool enable)
{
	u32 reg_val = 0U;

	if (!enable) {
		nvgpu_writel(g, gr_exception_en_r(), reg_val);
		nvgpu_writel(g, gr_exception1_en_r(), reg_val);
		nvgpu_writel(g, gr_exception2_en_r(), reg_val);
		return;
	}

	/*
	 * clear exceptions :
	 * other than SM : hww_esr are reset in *enable_hww_excetpions*
	 * SM            : cleared in *set_hww_esr_report_mask*
	 */

	/* enable exceptions */
	reg_val = gr_exception2_en_be_enabled_f();
	nvgpu_log(g, gpu_dbg_info, "gr_exception2_en 0x%08x", reg_val);
	nvgpu_writel(g, gr_exception2_en_r(), reg_val);

	reg_val = (u32)BIT32(nvgpu_gr_config_get_gpc_count(gr_config));
	nvgpu_writel(g, gr_exception1_en_r(),
				nvgpu_safe_sub_u32(reg_val, 1U));

	reg_val = gr_exception_en_fe_enabled_f() |
			gr_exception_en_memfmt_enabled_f() |
			gr_exception_en_pd_enabled_f() |
			gr_exception_en_scc_enabled_f() |
			gr_exception_en_ds_enabled_f() |
			gr_exception_en_ssync_enabled_f() |
			gr_exception_en_mme_enabled_f() |
			gr_exception_en_sked_enabled_f() |
			gr_exception_en_gpc_enabled_f();

	nvgpu_log(g, gpu_dbg_info, "gr_exception_en 0x%08x", reg_val);

	nvgpu_writel(g, gr_exception_en_r(), reg_val);
}

void gv11b_gr_intr_enable_gpc_exceptions(struct gk20a *g,
					 struct nvgpu_gr_config *gr_config)
{
	u32 tpc_mask, tpc_mask_calc;

	nvgpu_writel(g, gr_gpcs_tpcs_tpccs_tpc_exception_en_r(),
			gr_gpcs_tpcs_tpccs_tpc_exception_en_sm_enabled_f() |
			gr_gpcs_tpcs_tpccs_tpc_exception_en_pe_enabled_f() |
			gr_gpcs_tpcs_tpccs_tpc_exception_en_mpc_enabled_f());

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

void gv11b_gr_intr_set_hww_esr_report_mask(struct gk20a *g)
{
	u32 sm_hww_global_esr_report_mask;
	u32 sm_hww_warp_esr_report_mask;

	/*
	 * Perform a RMW to the warp, global ESR report mask registers.
	 * This is done in-order to retain the default values loaded from
	 * sw_ctx_load.
	 */
	sm_hww_warp_esr_report_mask = nvgpu_readl(g,
		gr_gpc0_tpc0_sm0_hww_warp_esr_report_mask_r());
	sm_hww_global_esr_report_mask = nvgpu_readl(g,
		gr_gpc0_tpc0_sm0_hww_global_esr_report_mask_r());

	/* clear hww */
	nvgpu_writel(g, gr_gpcs_tpcs_sms_hww_global_esr_r(), U32_MAX);


	/* setup sm warp esr report masks */
	nvgpu_writel(g, gr_gpcs_tpcs_sms_hww_warp_esr_report_mask_r(),
		sm_hww_warp_esr_report_mask | get_sm_hww_warp_esr_report_mask());

	nvgpu_writel(g, gr_gpcs_tpcs_sms_hww_global_esr_report_mask_r(),
		sm_hww_global_esr_report_mask | get_sm_hww_global_esr_report_mask());

	nvgpu_log_info(g,
		"configured (global, warp)_esr_report_mask(0x%x, 0x%x)",
		sm_hww_global_esr_report_mask | get_sm_hww_global_esr_report_mask(),
		sm_hww_warp_esr_report_mask | get_sm_hww_warp_esr_report_mask());
}

static void gv11b_gr_intr_report_l1_tag_uncorrected_err(struct gk20a *g,
		struct nvgpu_gr_sm_ecc_status *ecc_status, u32 gpc, u32 tpc)
{
	u32 i;

	/* This check has been added to ensure that the TPC id is less than
	 * 8-bits and hence, it can be packed as part of LSB 8-bits along with
	 * the GPC id while reporting SM related ECC errors.
	 */
	tpc = tpc & U8_MAX;

	for (i = 0U; i < ecc_status->err_count; i++) {
		if (ecc_status->err_id[i] == GPU_SM_L1_TAG_ECC_UNCORRECTED) {
			nvgpu_err(g, "sm_l1_tag_ecc_uncorrected "
					"gpc_id(%d), tpc_id(%d)", gpc, tpc);
		}

		if (ecc_status->err_id[i] == GPU_SM_L1_TAG_MISS_FIFO_ECC_UNCORRECTED) {
			nvgpu_err(g, "sm_l1_tag_miss_fifo_ecc_uncorrected "
					"gpc_id(%d), tpc_id(%d)", gpc, tpc);
		}

		if (ecc_status->err_id[i] == GPU_SM_L1_TAG_S2R_PIXPRF_ECC_UNCORRECTED) {
			nvgpu_err(g, "sm_l1_tag_s2r_pixprf_ecc_uncorrected "
					"gpc_id(%d), tpc_id(%d)", gpc, tpc);
		}
	}
}

static void gv11b_gr_intr_report_l1_tag_corrected_err(struct gk20a *g,
		struct nvgpu_gr_sm_ecc_status *ecc_status, u32 gpc, u32 tpc)
{
	u32 i;

	/* This check has been added to ensure that the TPC id is less than
	 * 8-bits and hence, it can be packed as part of LSB 8-bits along with
	 * the GPC id while reporting SM related ECC errors.
	 */
	tpc = tpc & U8_MAX;

	for (i = 0U; i < ecc_status->err_count; i++) {
		if (ecc_status->err_id[i] == GPU_SM_L1_TAG_ECC_CORRECTED) {
			nvgpu_err(g, "sm_l1_tag_ecc_corrected "
					"gpc_id(%d), tpc_id(%d)", gpc, tpc);
		}
	}
}

static void gv11b_gr_intr_set_l1_tag_uncorrected_err(struct gk20a *g,
	u32 l1_tag_ecc_status, struct nvgpu_gr_sm_ecc_status *ecc_status)
{
	(void)g;

	if ((l1_tag_ecc_status &
	    (gr_pri_gpc0_tpc0_sm_l1_tag_ecc_status_uncorrected_err_el1_0_m() |
	     gr_pri_gpc0_tpc0_sm_l1_tag_ecc_status_uncorrected_err_el1_1_m())) != 0U) {
		nvgpu_report_err_to_sdl(g, NVGPU_ERR_MODULE_SM,
				GPU_SM_L1_TAG_ECC_UNCORRECTED);
		ecc_status->err_id[ecc_status->err_count] =
				GPU_SM_L1_TAG_ECC_UNCORRECTED;
		ecc_status->err_count =
				nvgpu_safe_add_u32(ecc_status->err_count, 1U);
	}

	if ((l1_tag_ecc_status &
	     gr_pri_gpc0_tpc0_sm_l1_tag_ecc_status_uncorrected_err_miss_fifo_m()) != 0U) {
		ecc_status->err_id[ecc_status->err_count] =
				GPU_SM_L1_TAG_MISS_FIFO_ECC_UNCORRECTED;
		ecc_status->err_count =
				nvgpu_safe_add_u32(ecc_status->err_count, 1U);
	}

	if ((l1_tag_ecc_status &
	     gr_pri_gpc0_tpc0_sm_l1_tag_ecc_status_uncorrected_err_pixrpf_m()) != 0U) {
		ecc_status->err_id[ecc_status->err_count] =
				GPU_SM_L1_TAG_S2R_PIXPRF_ECC_UNCORRECTED;
		ecc_status->err_count =
				nvgpu_safe_add_u32(ecc_status->err_count, 1U);
	}
}

static void gv11b_gr_intr_set_l1_tag_corrected_err(struct gk20a *g,
	u32 l1_tag_ecc_status, struct nvgpu_gr_sm_ecc_status *ecc_status)
{
	(void)g;

	if ((l1_tag_ecc_status &
	    (gr_pri_gpc0_tpc0_sm_l1_tag_ecc_status_corrected_err_el1_0_m() |
	     gr_pri_gpc0_tpc0_sm_l1_tag_ecc_status_corrected_err_el1_1_m())) != 0U) {
		nvgpu_report_err_to_sdl(g, NVGPU_ERR_MODULE_SM,
				GPU_SM_L1_TAG_ECC_CORRECTED);
		ecc_status->err_id[ecc_status->err_count] =
				GPU_SM_L1_TAG_ECC_CORRECTED;
		ecc_status->err_count =
				nvgpu_safe_add_u32(ecc_status->err_count, 1U);
	}

	if ((l1_tag_ecc_status &
	     gr_pri_gpc0_tpc0_sm_l1_tag_ecc_status_corrected_err_miss_fifo_m()) != 0U) {
		/* This error is not expected to occur in gv11b and hence,
		 * this scenario is considered as a fatal error.
		 */
		BUG();
	}

	if ((l1_tag_ecc_status &
	     gr_pri_gpc0_tpc0_sm_l1_tag_ecc_status_corrected_err_pixrpf_m()) != 0U) {
		/* This error is not expected to occur in gv11b and hence,
		 * this scenario is considered as a fatal error.
		 */
		BUG();
	}
}

static bool gv11b_gr_intr_sm_l1_tag_ecc_status_errors(struct gk20a *g,
	u32 l1_tag_ecc_status, struct nvgpu_gr_sm_ecc_status *ecc_status)
{
	u32 corr_err, uncorr_err;
	bool err_status = true;

	corr_err = l1_tag_ecc_status &
		(gr_pri_gpc0_tpc0_sm_l1_tag_ecc_status_corrected_err_el1_0_m() |
		 gr_pri_gpc0_tpc0_sm_l1_tag_ecc_status_corrected_err_el1_1_m() |
		 gr_pri_gpc0_tpc0_sm_l1_tag_ecc_status_corrected_err_pixrpf_m() |
		 gr_pri_gpc0_tpc0_sm_l1_tag_ecc_status_corrected_err_miss_fifo_m());

	uncorr_err = l1_tag_ecc_status &
		(gr_pri_gpc0_tpc0_sm_l1_tag_ecc_status_uncorrected_err_el1_0_m() |
		 gr_pri_gpc0_tpc0_sm_l1_tag_ecc_status_uncorrected_err_el1_1_m() |
		 gr_pri_gpc0_tpc0_sm_l1_tag_ecc_status_uncorrected_err_pixrpf_m() |
		 gr_pri_gpc0_tpc0_sm_l1_tag_ecc_status_uncorrected_err_miss_fifo_m());

	if ((corr_err == 0U) && (uncorr_err == 0U)) {
		err_status = false;
	}

	ecc_status->err_count = 0U;
	ecc_status->corrected_err_status = corr_err;
	ecc_status->uncorrected_err_status = uncorr_err;

	gv11b_gr_intr_set_l1_tag_corrected_err(g, l1_tag_ecc_status, ecc_status);
	gv11b_gr_intr_set_l1_tag_uncorrected_err(g, l1_tag_ecc_status, ecc_status);

	return err_status;
}

static void gv11b_gr_intr_handle_l1_tag_exception(struct gk20a *g, u32 gpc, u32 tpc)
{
	u32 gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_GPC_STRIDE);
	u32 tpc_in_gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_TPC_IN_GPC_STRIDE);
	u32 offset;
	u32 l1_tag_ecc_status;
	u32 l1_tag_corrected_err_count_delta = 0U;
	u32 l1_tag_uncorrected_err_count_delta = 0U;
	bool is_l1_tag_ecc_corrected_total_err_overflow = false;
	bool is_l1_tag_ecc_uncorrected_total_err_overflow = false;
	struct nvgpu_gr_sm_ecc_status ecc_status;

	offset = nvgpu_safe_add_u32(
			nvgpu_safe_mult_u32(gpc_stride, gpc),
			nvgpu_safe_mult_u32(tpc_in_gpc_stride, tpc));

	/* Check for L1 tag ECC errors. */
	l1_tag_ecc_status = nvgpu_readl(g, nvgpu_safe_add_u32(
		gr_pri_gpc0_tpc0_sm_l1_tag_ecc_status_r(), offset));

	(void) memset(&ecc_status, 0, sizeof(struct nvgpu_gr_sm_ecc_status));

	if (g->ops.gr.intr.sm_ecc_status_errors(g, l1_tag_ecc_status,
				SM_L1_TAG_ERROR, &ecc_status) == false) {
		return;
	}

	l1_tag_corrected_err_count_delta =
		gr_pri_gpc0_tpc0_sm_l1_tag_ecc_corrected_err_count_total_v(
			nvgpu_readl(g, nvgpu_safe_add_u32(
				gr_pri_gpc0_tpc0_sm_l1_tag_ecc_corrected_err_count_r(),
				offset)));
	l1_tag_uncorrected_err_count_delta =
		gr_pri_gpc0_tpc0_sm_l1_tag_ecc_uncorrected_err_count_total_v(
			nvgpu_readl(g, nvgpu_safe_add_u32(
				gr_pri_gpc0_tpc0_sm_l1_tag_ecc_uncorrected_err_count_r(),
				offset)));
	is_l1_tag_ecc_corrected_total_err_overflow =
		gr_pri_gpc0_tpc0_sm_l1_tag_ecc_status_corrected_err_total_counter_overflow_v(l1_tag_ecc_status) != 0U;
	is_l1_tag_ecc_uncorrected_total_err_overflow =
		gr_pri_gpc0_tpc0_sm_l1_tag_ecc_status_uncorrected_err_total_counter_overflow_v(l1_tag_ecc_status) != 0U;

	if ((l1_tag_corrected_err_count_delta > 0U) || is_l1_tag_ecc_corrected_total_err_overflow) {
		nvgpu_err(g,
			"corrected error (SBE) detected in SM L1 tag! err_mask [%08x] is_overf [%d]",
			ecc_status.corrected_err_status, is_l1_tag_ecc_corrected_total_err_overflow);

		/* HW uses 16-bits counter */
		if (is_l1_tag_ecc_corrected_total_err_overflow) {
			l1_tag_corrected_err_count_delta =
			   nvgpu_safe_add_u32(
				l1_tag_corrected_err_count_delta,
				BIT32(gr_pri_gpc0_tpc0_sm_l1_tag_ecc_corrected_err_count_total_s()));
		}
		g->ecc.gr.sm_l1_tag_ecc_corrected_err_count[gpc][tpc].counter =
		   nvgpu_safe_add_u32(
			g->ecc.gr.sm_l1_tag_ecc_corrected_err_count[gpc][tpc].counter,
			l1_tag_corrected_err_count_delta);
		gv11b_gr_intr_report_l1_tag_corrected_err(g, &ecc_status, gpc, tpc);
		nvgpu_writel(g, nvgpu_safe_add_u32(
			gr_pri_gpc0_tpc0_sm_l1_tag_ecc_corrected_err_count_r(), offset),
			0U);
	}
	if ((l1_tag_uncorrected_err_count_delta > 0U) || is_l1_tag_ecc_uncorrected_total_err_overflow) {
		nvgpu_err(g,
			"Uncorrected error (DBE) detected in SM L1 tag! err_mask [%08x] is_overf [%d]",
			ecc_status.uncorrected_err_status, is_l1_tag_ecc_uncorrected_total_err_overflow);

		/* HW uses 16-bits counter */
		if (is_l1_tag_ecc_uncorrected_total_err_overflow) {
			l1_tag_uncorrected_err_count_delta =
			    nvgpu_safe_add_u32(
				l1_tag_uncorrected_err_count_delta,
				BIT32(gr_pri_gpc0_tpc0_sm_l1_tag_ecc_uncorrected_err_count_total_s()));
		}
		g->ecc.gr.sm_l1_tag_ecc_uncorrected_err_count[gpc][tpc].counter =
		   nvgpu_safe_add_u32(
			g->ecc.gr.sm_l1_tag_ecc_uncorrected_err_count[gpc][tpc].counter,
			l1_tag_uncorrected_err_count_delta);
		gv11b_gr_intr_report_l1_tag_uncorrected_err(g, &ecc_status, gpc, tpc);
		nvgpu_writel(g, nvgpu_safe_add_u32(
			gr_pri_gpc0_tpc0_sm_l1_tag_ecc_uncorrected_err_count_r(), offset),
			0U);
	}

	nvgpu_writel(g, nvgpu_safe_add_u32(
			gr_pri_gpc0_tpc0_sm_l1_tag_ecc_status_r(), offset),
			gr_pri_gpc0_tpc0_sm_l1_tag_ecc_status_reset_task_f());
}

static bool gv11b_gr_intr_sm_lrf_ecc_status_errors(struct gk20a *g,
	u32 lrf_ecc_status, struct nvgpu_gr_sm_ecc_status *ecc_status)
{
	u32 corr_err, uncorr_err;
	bool err_status = true;

	(void)g;

	corr_err = lrf_ecc_status &
		(gr_pri_gpc0_tpc0_sm_lrf_ecc_status_corrected_err_qrfdp0_m() |
		 gr_pri_gpc0_tpc0_sm_lrf_ecc_status_corrected_err_qrfdp1_m() |
		 gr_pri_gpc0_tpc0_sm_lrf_ecc_status_corrected_err_qrfdp2_m() |
		 gr_pri_gpc0_tpc0_sm_lrf_ecc_status_corrected_err_qrfdp3_m() |
		 gr_pri_gpc0_tpc0_sm_lrf_ecc_status_corrected_err_qrfdp4_m() |
		 gr_pri_gpc0_tpc0_sm_lrf_ecc_status_corrected_err_qrfdp5_m() |
		 gr_pri_gpc0_tpc0_sm_lrf_ecc_status_corrected_err_qrfdp6_m() |
		 gr_pri_gpc0_tpc0_sm_lrf_ecc_status_corrected_err_qrfdp7_m());

	uncorr_err = lrf_ecc_status &
		(gr_pri_gpc0_tpc0_sm_lrf_ecc_status_uncorrected_err_qrfdp0_m() |
		 gr_pri_gpc0_tpc0_sm_lrf_ecc_status_uncorrected_err_qrfdp1_m() |
		 gr_pri_gpc0_tpc0_sm_lrf_ecc_status_uncorrected_err_qrfdp2_m() |
		 gr_pri_gpc0_tpc0_sm_lrf_ecc_status_uncorrected_err_qrfdp3_m() |
		 gr_pri_gpc0_tpc0_sm_lrf_ecc_status_uncorrected_err_qrfdp4_m() |
		 gr_pri_gpc0_tpc0_sm_lrf_ecc_status_uncorrected_err_qrfdp5_m() |
		 gr_pri_gpc0_tpc0_sm_lrf_ecc_status_uncorrected_err_qrfdp6_m() |
		 gr_pri_gpc0_tpc0_sm_lrf_ecc_status_uncorrected_err_qrfdp7_m());

	if ((corr_err == 0U) && (uncorr_err == 0U)) {
		err_status = false;
	}

	ecc_status->err_count = 0U;

	if (corr_err != 0U) {
		/* This error is not expected to occur in gv11b and hence,
		 * this scenario is considered as a fatal error.
		 */
		BUG();
	}

	if (uncorr_err != 0U) {
		ecc_status->err_id[ecc_status->err_count] =
				GPU_SM_LRF_ECC_UNCORRECTED;
		ecc_status->err_count =
			nvgpu_safe_add_u32(ecc_status->err_count, 1U);
	}

	ecc_status->corrected_err_status = corr_err;
	ecc_status->uncorrected_err_status = uncorr_err;

	return err_status;
}

static void gv11b_gr_intr_handle_lrf_exception(struct gk20a *g, u32 gpc, u32 tpc)
{
	u32 gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_GPC_STRIDE);
	u32 tpc_in_gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_TPC_IN_GPC_STRIDE);
	u32 offset;
	u32 lrf_ecc_status;
	u32 lrf_corrected_err_count_delta = 0U;
	u32 lrf_uncorrected_err_count_delta = 0U;
	bool is_lrf_ecc_corrected_total_err_overflow = false;
	bool is_lrf_ecc_uncorrected_total_err_overflow = false;
	struct nvgpu_gr_sm_ecc_status ecc_status;

	offset = nvgpu_safe_add_u32(
			nvgpu_safe_mult_u32(gpc_stride, gpc),
			nvgpu_safe_mult_u32(tpc_in_gpc_stride, tpc));

	/* Check for LRF ECC errors. */
	lrf_ecc_status = nvgpu_readl(g,
		nvgpu_safe_add_u32(gr_pri_gpc0_tpc0_sm_lrf_ecc_status_r(),
				   offset));

	(void) memset(&ecc_status, 0, sizeof(struct nvgpu_gr_sm_ecc_status));

	if (g->ops.gr.intr.sm_ecc_status_errors(g, lrf_ecc_status,
			SM_LRF_ECC_ERROR, &ecc_status) == false) {
		return;
	}

	lrf_corrected_err_count_delta =
		gr_pri_gpc0_tpc0_sm_lrf_ecc_corrected_err_count_total_v(
			nvgpu_readl(g, nvgpu_safe_add_u32(
				gr_pri_gpc0_tpc0_sm_lrf_ecc_corrected_err_count_r(),
				offset)));
	lrf_uncorrected_err_count_delta =
		gr_pri_gpc0_tpc0_sm_lrf_ecc_uncorrected_err_count_total_v(
			nvgpu_readl(g, nvgpu_safe_add_u32(
				gr_pri_gpc0_tpc0_sm_lrf_ecc_uncorrected_err_count_r(),
				offset)));
	is_lrf_ecc_corrected_total_err_overflow =
		gr_pri_gpc0_tpc0_sm_lrf_ecc_status_corrected_err_total_counter_overflow_v(lrf_ecc_status) != 0U;
	is_lrf_ecc_uncorrected_total_err_overflow =
		gr_pri_gpc0_tpc0_sm_lrf_ecc_status_uncorrected_err_total_counter_overflow_v(lrf_ecc_status) != 0U;

	/* This check has been added to ensure that the TPC id is less than
	 * 8-bits and hence, it can be packed as part of LSB 8-bits along with
	 * the GPC id while reporting SM related ECC errors.
	 */
	tpc = tpc & U8_MAX;

	if ((lrf_corrected_err_count_delta > 0U) ||
			is_lrf_ecc_corrected_total_err_overflow) {
		nvgpu_err(g, "unexpected corrected error (SBE) detected in SM LRF!"
			" err_mask [%08x] is_overf [%d]",
			ecc_status.corrected_err_status,
			is_lrf_ecc_corrected_total_err_overflow);

		/* This error is not expected to occur in gv11b and hence,
		 * this scenario is considered as a fatal error.
		 */
		BUG();
	}
	if ((lrf_uncorrected_err_count_delta > 0U) || is_lrf_ecc_uncorrected_total_err_overflow) {
		nvgpu_err(g,
			"Uncorrected error (DBE) detected in SM LRF! err_mask [%08x] is_overf [%d]",
			ecc_status.uncorrected_err_status, is_lrf_ecc_uncorrected_total_err_overflow);

		/* HW uses 16-bits counter */
		if (is_lrf_ecc_uncorrected_total_err_overflow) {
			lrf_uncorrected_err_count_delta =
			   nvgpu_safe_add_u32(
				lrf_uncorrected_err_count_delta,
				BIT32(gr_pri_gpc0_tpc0_sm_lrf_ecc_uncorrected_err_count_total_s()));
		}
		g->ecc.gr.sm_lrf_ecc_double_err_count[gpc][tpc].counter =
		   nvgpu_safe_add_u32(
			g->ecc.gr.sm_lrf_ecc_double_err_count[gpc][tpc].counter,
			lrf_uncorrected_err_count_delta);
		nvgpu_writel(g, nvgpu_safe_add_u32(
			gr_pri_gpc0_tpc0_sm_lrf_ecc_uncorrected_err_count_r(), offset),
			0U);
	}

	nvgpu_writel(g, nvgpu_safe_add_u32(
			gr_pri_gpc0_tpc0_sm_lrf_ecc_status_r(), offset),
			gr_pri_gpc0_tpc0_sm_lrf_ecc_status_reset_task_f());
}

static bool gv11b_gr_intr_sm_cbu_ecc_status_errors(struct gk20a *g,
	u32 cbu_ecc_status, struct nvgpu_gr_sm_ecc_status *ecc_status)
{
	u32 corr_err, uncorr_err;
	bool err_status = true;

	(void)g;

	corr_err = cbu_ecc_status &
		(gr_pri_gpc0_tpc0_sm_cbu_ecc_status_corrected_err_warp_sm0_m() |
		 gr_pri_gpc0_tpc0_sm_cbu_ecc_status_corrected_err_warp_sm1_m() |
		 gr_pri_gpc0_tpc0_sm_cbu_ecc_status_corrected_err_barrier_sm0_m() |
		 gr_pri_gpc0_tpc0_sm_cbu_ecc_status_corrected_err_barrier_sm1_m());

	uncorr_err = cbu_ecc_status &
		(gr_pri_gpc0_tpc0_sm_cbu_ecc_status_uncorrected_err_warp_sm0_m() |
		 gr_pri_gpc0_tpc0_sm_cbu_ecc_status_uncorrected_err_warp_sm1_m() |
		 gr_pri_gpc0_tpc0_sm_cbu_ecc_status_uncorrected_err_barrier_sm0_m() |
		 gr_pri_gpc0_tpc0_sm_cbu_ecc_status_uncorrected_err_barrier_sm1_m());

	if ((corr_err == 0U) && (uncorr_err == 0U)) {
		err_status = false;
	}

	ecc_status->err_count = 0U;

	if (corr_err != 0U) {
		/* This error is not expected to occur in gv11b and hence,
		 * this scenario is considered as a fatal error.
		 */
		BUG();
	}

	if (uncorr_err != 0U) {
		ecc_status->err_id[ecc_status->err_count] =
				GPU_SM_CBU_ECC_UNCORRECTED;
		ecc_status->err_count =
			nvgpu_safe_add_u32(ecc_status->err_count, 1U);
	}

	ecc_status->corrected_err_status = corr_err;
	ecc_status->uncorrected_err_status = uncorr_err;

	return err_status;
}

static void gv11b_gr_intr_handle_cbu_exception(struct gk20a *g, u32 gpc, u32 tpc)
{
	u32 gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_GPC_STRIDE);
	u32 tpc_in_gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_TPC_IN_GPC_STRIDE);
	u32 offset;
	u32 cbu_ecc_status;
	u32 cbu_corrected_err_count_delta = 0U;
	u32 cbu_uncorrected_err_count_delta = 0U;
	bool is_cbu_ecc_corrected_total_err_overflow = false;
	bool is_cbu_ecc_uncorrected_total_err_overflow = false;
	struct nvgpu_gr_sm_ecc_status ecc_status;

	offset = nvgpu_safe_add_u32(
			nvgpu_safe_mult_u32(gpc_stride, gpc),
			nvgpu_safe_mult_u32(tpc_in_gpc_stride, tpc));

	/* Check for CBU ECC errors. */
	cbu_ecc_status = nvgpu_readl(g, nvgpu_safe_add_u32(
		gr_pri_gpc0_tpc0_sm_cbu_ecc_status_r(), offset));

	(void) memset(&ecc_status, 0, sizeof(struct nvgpu_gr_sm_ecc_status));

	if (g->ops.gr.intr.sm_ecc_status_errors(g, cbu_ecc_status,
				SM_CBU_ECC_ERROR, &ecc_status) == false) {
		return;
	}

	cbu_corrected_err_count_delta =
		gr_pri_gpc0_tpc0_sm_cbu_ecc_corrected_err_count_total_v(
			nvgpu_readl(g, nvgpu_safe_add_u32(
				gr_pri_gpc0_tpc0_sm_cbu_ecc_corrected_err_count_r(),
				offset)));
	cbu_uncorrected_err_count_delta =
		gr_pri_gpc0_tpc0_sm_cbu_ecc_uncorrected_err_count_total_v(
			nvgpu_readl(g, nvgpu_safe_add_u32(
				gr_pri_gpc0_tpc0_sm_cbu_ecc_uncorrected_err_count_r(),
				offset)));
	is_cbu_ecc_corrected_total_err_overflow =
		gr_pri_gpc0_tpc0_sm_cbu_ecc_status_corrected_err_total_counter_overflow_v(cbu_ecc_status) != 0U;
	is_cbu_ecc_uncorrected_total_err_overflow =
		gr_pri_gpc0_tpc0_sm_cbu_ecc_status_uncorrected_err_total_counter_overflow_v(cbu_ecc_status) != 0U;

	/* This check has been added to ensure that the TPC id is less than
	 * 8-bits and hence, it can be packed as part of LSB 8-bits along with
	 * the GPC id while reporting SM related ECC errors.
	 */
	tpc = tpc & U8_MAX;

	if ((cbu_corrected_err_count_delta > 0U) ||
		is_cbu_ecc_corrected_total_err_overflow) {
		nvgpu_err(g, "unexpected corrected error (SBE) detected in SM CBU!"
			" err_mask [%08x] is_overf [%d]",
			ecc_status.corrected_err_status,
			is_cbu_ecc_corrected_total_err_overflow);

		/* This error is not expected to occur in gv11b and hence,
		 * this scenario is considered as a fatal error.
		 */
		BUG();
	}
	if ((cbu_uncorrected_err_count_delta > 0U) || is_cbu_ecc_uncorrected_total_err_overflow) {
		nvgpu_err(g,
			"Uncorrected error (DBE) detected in SM CBU! err_mask [%08x] is_overf [%d]",
			ecc_status.uncorrected_err_status, is_cbu_ecc_uncorrected_total_err_overflow);

		/* HW uses 16-bits counter */
		if (is_cbu_ecc_uncorrected_total_err_overflow) {
			cbu_uncorrected_err_count_delta =
			   nvgpu_safe_add_u32(cbu_uncorrected_err_count_delta,
				BIT32(gr_pri_gpc0_tpc0_sm_cbu_ecc_uncorrected_err_count_total_s()));
		}
		g->ecc.gr.sm_cbu_ecc_uncorrected_err_count[gpc][tpc].counter =
		   nvgpu_safe_add_u32(
			g->ecc.gr.sm_cbu_ecc_uncorrected_err_count[gpc][tpc].counter,
			cbu_uncorrected_err_count_delta);
		nvgpu_writel(g, nvgpu_safe_add_u32(
			gr_pri_gpc0_tpc0_sm_cbu_ecc_uncorrected_err_count_r(), offset),
			0U);
	}

	nvgpu_writel(g, nvgpu_safe_add_u32(
				gr_pri_gpc0_tpc0_sm_cbu_ecc_status_r(), offset),
			gr_pri_gpc0_tpc0_sm_cbu_ecc_status_reset_task_f());
}

static bool gv11b_gr_intr_sm_l1_data_ecc_status_errors(struct gk20a *g,
	u32 l1_data_ecc_status, struct nvgpu_gr_sm_ecc_status *ecc_status)
{
	u32 corr_err, uncorr_err;
	bool err_status = true;

	(void)g;

	corr_err = l1_data_ecc_status &
		(gr_pri_gpc0_tpc0_sm_l1_data_ecc_status_corrected_err_el1_0_m() |
		 gr_pri_gpc0_tpc0_sm_l1_data_ecc_status_corrected_err_el1_1_m());
	uncorr_err = l1_data_ecc_status &
		(gr_pri_gpc0_tpc0_sm_l1_data_ecc_status_uncorrected_err_el1_0_m() |
		 gr_pri_gpc0_tpc0_sm_l1_data_ecc_status_uncorrected_err_el1_1_m());

	if ((corr_err == 0U) && (uncorr_err == 0U)) {
		err_status = false;
	}

	ecc_status->err_count = 0U;

	if (corr_err != 0U) {
		/* This error is not expected to occur in gv11b and hence,
		 * this scenario is considered as a fatal error.
		 */
		BUG();
	}

	if (uncorr_err != 0U) {
		ecc_status->err_id[ecc_status->err_count] =
				GPU_SM_L1_DATA_ECC_UNCORRECTED;
		ecc_status->err_count =
			nvgpu_safe_add_u32(ecc_status->err_count, 1U);
	}

	ecc_status->corrected_err_status = corr_err;
	ecc_status->uncorrected_err_status = uncorr_err;

	return err_status;
}

static void gv11b_gr_intr_handle_l1_data_exception(struct gk20a *g, u32 gpc, u32 tpc)
{
	u32 gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_GPC_STRIDE);
	u32 tpc_in_gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_TPC_IN_GPC_STRIDE);
	u32 offset;
	u32 l1_data_ecc_status;
	u32 l1_data_corrected_err_count_delta = 0U;
	u32 l1_data_uncorrected_err_count_delta = 0U;
	bool is_l1_data_ecc_corrected_total_err_overflow = false;
	bool is_l1_data_ecc_uncorrected_total_err_overflow = false;
	struct nvgpu_gr_sm_ecc_status ecc_status;

	offset = nvgpu_safe_add_u32(
			nvgpu_safe_mult_u32(gpc_stride, gpc),
			nvgpu_safe_mult_u32(tpc_in_gpc_stride, tpc));

	/* Check for L1 data ECC errors. */
	l1_data_ecc_status = nvgpu_readl(g, nvgpu_safe_add_u32(
		gr_pri_gpc0_tpc0_sm_l1_data_ecc_status_r(), offset));

	(void) memset(&ecc_status, 0, sizeof(struct nvgpu_gr_sm_ecc_status));

	if (g->ops.gr.intr.sm_ecc_status_errors(g, l1_data_ecc_status,
				SM_L1_DATA_ECC_ERROR, &ecc_status) == false) {
		return;
	}

	l1_data_corrected_err_count_delta =
		gr_pri_gpc0_tpc0_sm_l1_data_ecc_corrected_err_count_total_v(
			nvgpu_readl(g, nvgpu_safe_add_u32(
				gr_pri_gpc0_tpc0_sm_l1_data_ecc_corrected_err_count_r(),
				offset)));
	l1_data_uncorrected_err_count_delta =
		gr_pri_gpc0_tpc0_sm_l1_data_ecc_uncorrected_err_count_total_v(
			nvgpu_readl(g, nvgpu_safe_add_u32(
				gr_pri_gpc0_tpc0_sm_l1_data_ecc_uncorrected_err_count_r(),
				offset)));
	is_l1_data_ecc_corrected_total_err_overflow =
		gr_pri_gpc0_tpc0_sm_l1_data_ecc_status_corrected_err_total_counter_overflow_v(l1_data_ecc_status) != 0U;
	is_l1_data_ecc_uncorrected_total_err_overflow =
		gr_pri_gpc0_tpc0_sm_l1_data_ecc_status_uncorrected_err_total_counter_overflow_v(l1_data_ecc_status) != 0U;

	/* This check has been added to ensure that the TPC id is less than
	 * 8-bits and hence, it can be packed as part of LSB 8-bits along with
	 * the GPC id while reporting SM related ECC errors.
	 */
	tpc = tpc & U8_MAX;

	if ((l1_data_corrected_err_count_delta > 0U) ||
		is_l1_data_ecc_corrected_total_err_overflow) {
		nvgpu_err(g, "unexpected corrected error (SBE) detected in SM L1 data!"
			" err_mask [%08x] is_overf [%d]",
			ecc_status.corrected_err_status,
			is_l1_data_ecc_corrected_total_err_overflow);

		/* This error is not expected to occur in gv11b and hence,
		 * this scenario is considered as a fatal error.
		 */
		BUG();
	}

	if ((l1_data_uncorrected_err_count_delta > 0U) || is_l1_data_ecc_uncorrected_total_err_overflow) {
		nvgpu_err(g,
			"Uncorrected error (DBE) detected in SM L1 data! err_mask [%08x] is_overf [%d]",
			ecc_status.uncorrected_err_status, is_l1_data_ecc_uncorrected_total_err_overflow);

		/* HW uses 16-bits counter */
		if (is_l1_data_ecc_uncorrected_total_err_overflow) {
			l1_data_uncorrected_err_count_delta =
			   nvgpu_safe_add_u32(l1_data_uncorrected_err_count_delta,
				BIT32(gr_pri_gpc0_tpc0_sm_l1_data_ecc_uncorrected_err_count_total_s()));
		}
		g->ecc.gr.sm_l1_data_ecc_uncorrected_err_count[gpc][tpc].counter =
		   nvgpu_safe_add_u32(
			g->ecc.gr.sm_l1_data_ecc_uncorrected_err_count[gpc][tpc].counter,
			l1_data_uncorrected_err_count_delta);
		nvgpu_writel(g, nvgpu_safe_add_u32(
			gr_pri_gpc0_tpc0_sm_l1_data_ecc_uncorrected_err_count_r(), offset),
			0U);
	}
	nvgpu_writel(g, nvgpu_safe_add_u32(
				gr_pri_gpc0_tpc0_sm_l1_data_ecc_status_r(), offset),
			gr_pri_gpc0_tpc0_sm_l1_data_ecc_status_reset_task_f());
}

static void gv11b_gr_intr_report_icache_uncorrected_err(struct gk20a *g,
		struct nvgpu_gr_sm_ecc_status *ecc_status, u32 gpc, u32 tpc)
{
	u32 i;

	/* This check has been added to ensure that the TPC id is less than
	 * 8-bits and hence, it can be packed as part of LSB 8-bits along with
	 * the GPC id while reporting SM related ECC errors.
	 */
	tpc = tpc & U8_MAX;

	for (i = 0U; i < ecc_status->err_count; i++) {
		if (ecc_status->err_id[i] == GPU_SM_ICACHE_L0_DATA_ECC_UNCORRECTED) {
			nvgpu_err(g, "sm_icache_l0_data_ecc_uncorrected. "
					"gpc_id(%d), tpc_id(%d)", gpc, tpc);
		}

		if (ecc_status->err_id[i] == GPU_SM_ICACHE_L0_PREDECODE_ECC_UNCORRECTED) {
			nvgpu_err(g, "sm_icache_l0_predecode_ecc_uncorrected. "
					"gpc_id(%d), tpc_id(%d)", gpc, tpc);
		}

		if (ecc_status->err_id[i] == GPU_SM_ICACHE_L1_DATA_ECC_UNCORRECTED) {
			nvgpu_err(g, "sm_icache_l1_data_ecc_uncorrected. "
					"gpc_id(%d), tpc_id(%d)", gpc, tpc);
		}
	}
}

static void gv11b_set_icache_ecc_status_uncorrected_errors(struct gk20a *g,
				u32 icache_ecc_status,
				struct nvgpu_gr_sm_ecc_status *ecc_status)
{
	(void)g;

	if ((icache_ecc_status &
	     gr_pri_gpc0_tpc0_sm_icache_ecc_status_uncorrected_err_l0_data_m()) != 0U) {
		ecc_status->err_id[ecc_status->err_count] =
				GPU_SM_ICACHE_L0_DATA_ECC_UNCORRECTED;
		ecc_status->err_count = nvgpu_safe_add_u32(ecc_status->err_count, 1U);
		nvgpu_report_err_to_sdl(g, NVGPU_ERR_MODULE_SM,
					GPU_SM_ICACHE_L0_DATA_ECC_UNCORRECTED);
	}

	if ((icache_ecc_status &
	     gr_pri_gpc0_tpc0_sm_icache_ecc_status_uncorrected_err_l0_predecode_m()) != 0U) {
		ecc_status->err_id[ecc_status->err_count] =
				GPU_SM_ICACHE_L0_PREDECODE_ECC_UNCORRECTED;
		ecc_status->err_count = nvgpu_safe_add_u32(ecc_status->err_count, 1U);
		nvgpu_report_err_to_sdl(g, NVGPU_ERR_MODULE_SM,
					GPU_SM_ICACHE_L0_PREDECODE_ECC_UNCORRECTED);
	}

	if ((icache_ecc_status  &
	     gr_pri_gpc0_tpc0_sm_icache_ecc_status_uncorrected_err_l1_data_m()) != 0U) {
		ecc_status->err_id[ecc_status->err_count] =
				GPU_SM_ICACHE_L1_DATA_ECC_UNCORRECTED;
		ecc_status->err_count = nvgpu_safe_add_u32(ecc_status->err_count, 1U);
		nvgpu_report_err_to_sdl(g, NVGPU_ERR_MODULE_SM,
					GPU_SM_ICACHE_L1_DATA_ECC_UNCORRECTED);
	}
}

static bool gv11b_gr_intr_sm_icache_ecc_status_errors(struct gk20a *g,
	u32 icache_ecc_status, struct nvgpu_gr_sm_ecc_status *ecc_status)
{
	u32 corr_err, uncorr_err;
	bool err_status = true;

	corr_err = icache_ecc_status &
		(gr_pri_gpc0_tpc0_sm_icache_ecc_status_corrected_err_l0_data_m() |
		 gr_pri_gpc0_tpc0_sm_icache_ecc_status_corrected_err_l0_predecode_m() |
		 gr_pri_gpc0_tpc0_sm_icache_ecc_status_corrected_err_l1_data_m() |
		 gr_pri_gpc0_tpc0_sm_icache_ecc_status_corrected_err_l1_predecode_m());
	uncorr_err = icache_ecc_status &
		(gr_pri_gpc0_tpc0_sm_icache_ecc_status_uncorrected_err_l0_data_m() |
		 gr_pri_gpc0_tpc0_sm_icache_ecc_status_uncorrected_err_l0_predecode_m() |
		 gr_pri_gpc0_tpc0_sm_icache_ecc_status_uncorrected_err_l1_data_m() |
		 gr_pri_gpc0_tpc0_sm_icache_ecc_status_uncorrected_err_l1_predecode_m());

	if ((corr_err == 0U) && (uncorr_err == 0U)) {
		err_status = false;
	}

	ecc_status->err_count = 0U;

	if (corr_err != 0U) {
		/* This error is not expected to occur in gv11b and hence,
		 * this scenario is considered as a fatal error.
		 */
		BUG();
	}

	gv11b_set_icache_ecc_status_uncorrected_errors(g, icache_ecc_status,
						     ecc_status);

	ecc_status->corrected_err_status = corr_err;
	ecc_status->uncorrected_err_status = uncorr_err;

	return err_status;
}

static void gv11b_gr_intr_handle_icache_exception(struct gk20a *g, u32 gpc, u32 tpc)
{
	u32 gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_GPC_STRIDE);
	u32 tpc_in_gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_TPC_IN_GPC_STRIDE);
	u32 offset;
	u32 icache_ecc_status;
	u32 icache_corrected_err_count_delta = 0U;
	u32 icache_uncorrected_err_count_delta = 0U;
	bool is_icache_ecc_corrected_total_err_overflow = false;
	bool is_icache_ecc_uncorrected_total_err_overflow = false;
	struct nvgpu_gr_sm_ecc_status ecc_status;

	offset = nvgpu_safe_add_u32(
			nvgpu_safe_mult_u32(gpc_stride, gpc),
			nvgpu_safe_mult_u32(tpc_in_gpc_stride, tpc));

	/* Check for L0 && L1 icache ECC errors. */
	icache_ecc_status = nvgpu_readl(g, nvgpu_safe_add_u32(
		gr_pri_gpc0_tpc0_sm_icache_ecc_status_r(), offset));

	(void) memset(&ecc_status, 0, sizeof(struct nvgpu_gr_sm_ecc_status));

	if (g->ops.gr.intr.sm_ecc_status_errors(g, icache_ecc_status,
				SM_ICACHE_ECC_ERROR, &ecc_status) == false) {
		return;
	}

	icache_corrected_err_count_delta =
		gr_pri_gpc0_tpc0_sm_icache_ecc_corrected_err_count_total_v(
			nvgpu_readl(g, nvgpu_safe_add_u32(
				gr_pri_gpc0_tpc0_sm_icache_ecc_corrected_err_count_r(),
				offset)));
	icache_uncorrected_err_count_delta =
		gr_pri_gpc0_tpc0_sm_icache_ecc_uncorrected_err_count_total_v(
			nvgpu_readl(g, nvgpu_safe_add_u32(
				gr_pri_gpc0_tpc0_sm_icache_ecc_uncorrected_err_count_r(),
				offset)));
	is_icache_ecc_corrected_total_err_overflow =
		gr_pri_gpc0_tpc0_sm_icache_ecc_status_corrected_err_total_counter_overflow_v(icache_ecc_status) != 0U;
	is_icache_ecc_uncorrected_total_err_overflow =
		gr_pri_gpc0_tpc0_sm_icache_ecc_status_uncorrected_err_total_counter_overflow_v(icache_ecc_status) != 0U;

	if ((icache_corrected_err_count_delta > 0U) || is_icache_ecc_corrected_total_err_overflow) {
		nvgpu_err(g,
			"corrected error (SBE) detected in SM L0 && L1 icache! err_mask [%08x] is_overf [%d]",
			ecc_status.corrected_err_status, is_icache_ecc_corrected_total_err_overflow);

		/* HW uses 16-bits counter */
		if (is_icache_ecc_corrected_total_err_overflow) {
			icache_corrected_err_count_delta =
			   nvgpu_safe_add_u32(icache_corrected_err_count_delta,
				BIT32(gr_pri_gpc0_tpc0_sm_icache_ecc_corrected_err_count_total_s()));
		}
		g->ecc.gr.sm_icache_ecc_corrected_err_count[gpc][tpc].counter =
		   nvgpu_safe_add_u32(
			g->ecc.gr.sm_icache_ecc_corrected_err_count[gpc][tpc].counter,
			icache_corrected_err_count_delta);
		nvgpu_writel(g, nvgpu_safe_add_u32(
			gr_pri_gpc0_tpc0_sm_icache_ecc_corrected_err_count_r(), offset),
			0U);
	}

	if ((icache_uncorrected_err_count_delta > 0U) || is_icache_ecc_uncorrected_total_err_overflow) {
		nvgpu_err(g,
			"Uncorrected error (DBE) detected in SM L0 && L1 icache! err_mask [%08x] is_overf [%d]",
			ecc_status.uncorrected_err_status, is_icache_ecc_uncorrected_total_err_overflow);

		/* HW uses 16-bits counter */
		if (is_icache_ecc_uncorrected_total_err_overflow) {
			icache_uncorrected_err_count_delta =
			   nvgpu_safe_add_u32(
				icache_uncorrected_err_count_delta,
				BIT32(gr_pri_gpc0_tpc0_sm_icache_ecc_uncorrected_err_count_total_s()));
		}
		g->ecc.gr.sm_icache_ecc_uncorrected_err_count[gpc][tpc].counter =
		  nvgpu_safe_add_u32(
			g->ecc.gr.sm_icache_ecc_uncorrected_err_count[gpc][tpc].counter,
			icache_uncorrected_err_count_delta);
		nvgpu_writel(g, nvgpu_safe_add_u32(
			gr_pri_gpc0_tpc0_sm_icache_ecc_uncorrected_err_count_r(), offset),
			0U);
		gv11b_gr_intr_report_icache_uncorrected_err(g, &ecc_status, gpc, tpc);
	}

	nvgpu_writel(g, nvgpu_safe_add_u32(
			gr_pri_gpc0_tpc0_sm_icache_ecc_status_r(), offset),
			gr_pri_gpc0_tpc0_sm_icache_ecc_status_reset_task_f());
}

bool gv11b_gr_intr_sm_ecc_status_errors(struct gk20a *g,
	u32 ecc_status_reg, enum nvgpu_gr_sm_ecc_error_types err_type,
	struct nvgpu_gr_sm_ecc_status *ecc_status)
{
	bool err_status = false;

	if (err_type == SM_ICACHE_ECC_ERROR) {
		err_status = gv11b_gr_intr_sm_icache_ecc_status_errors(g,
						ecc_status_reg, ecc_status);
	} else if (err_type == SM_LRF_ECC_ERROR) {
		err_status = gv11b_gr_intr_sm_lrf_ecc_status_errors(g,
						ecc_status_reg, ecc_status);
	} else if (err_type == SM_L1_TAG_ERROR) {
		err_status = gv11b_gr_intr_sm_l1_tag_ecc_status_errors(g,
						ecc_status_reg, ecc_status);
	} else if (err_type == SM_CBU_ECC_ERROR) {
		err_status = gv11b_gr_intr_sm_cbu_ecc_status_errors(g,
						ecc_status_reg, ecc_status);
	} else if (err_type == SM_L1_DATA_ECC_ERROR) {
		err_status = gv11b_gr_intr_sm_l1_data_ecc_status_errors(g,
						ecc_status_reg, ecc_status);
	}

	return err_status;
}

void gv11b_gr_intr_handle_tpc_sm_ecc_exception(struct gk20a *g,
					u32 gpc, u32 tpc)
{
	/* Check for L1 tag ECC errors. */
	gv11b_gr_intr_handle_l1_tag_exception(g, gpc, tpc);

	/* Check for LRF ECC errors. */
	gv11b_gr_intr_handle_lrf_exception(g, gpc, tpc);

	/* Check for CBU ECC errors. */
	gv11b_gr_intr_handle_cbu_exception(g, gpc, tpc);

	/* Check for L1 data ECC errors. */
	gv11b_gr_intr_handle_l1_data_exception(g, gpc, tpc);

	/* Check for L0 && L1 icache ECC errors. */
	gv11b_gr_intr_handle_icache_exception(g, gpc, tpc);
}

void gv11b_gr_intr_get_esr_sm_sel(struct gk20a *g, u32 gpc, u32 tpc,
				u32 *esr_sm_sel)
{
	u32 reg_val;
	u32 gpc_offset, tpc_offset, offset;

	gpc_offset = nvgpu_gr_gpc_offset(g, gpc);
	tpc_offset = nvgpu_gr_tpc_offset(g, tpc);
	offset = nvgpu_safe_add_u32(gpc_offset, tpc_offset);

	reg_val = nvgpu_readl(g, nvgpu_safe_add_u32(
			gr_gpc0_tpc0_sm_tpc_esr_sm_sel_r(), offset));
	nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gpu_dbg,
			"sm tpc esr sm sel reg val: 0x%x", reg_val);
	*esr_sm_sel = 0;
	if (gr_gpc0_tpc0_sm_tpc_esr_sm_sel_sm0_error_v(reg_val) != 0U) {
		*esr_sm_sel = 1;
	}
	if (gr_gpc0_tpc0_sm_tpc_esr_sm_sel_sm1_error_v(reg_val) != 0U) {
		*esr_sm_sel |= BIT32(1);
	}
	nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gpu_dbg,
			"esr_sm_sel bitmask: 0x%x", *esr_sm_sel);
}

void gv11b_gr_intr_clear_sm_hww(struct gk20a *g, u32 gpc, u32 tpc, u32 sm,
				u32 global_esr)
{
	u32 offset;
	u32 gpc_offset, tpc_offset, sm_offset;

	gpc_offset = nvgpu_gr_gpc_offset(g, gpc);
	tpc_offset = nvgpu_gr_tpc_offset(g, tpc);
	sm_offset = nvgpu_gr_sm_offset(g, sm);

	offset = nvgpu_safe_add_u32(gpc_offset,
			nvgpu_safe_add_u32(tpc_offset, sm_offset));

	nvgpu_writel(g, nvgpu_safe_add_u32(
				gr_gpc0_tpc0_sm0_hww_global_esr_r(), offset),
			global_esr);
	nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gpu_dbg,
			"Cleared HWW global esr, current reg val: 0x%x",
			nvgpu_readl(g, nvgpu_safe_add_u32(
				gr_gpc0_tpc0_sm0_hww_global_esr_r(), offset)));

	nvgpu_writel(g, nvgpu_safe_add_u32(
			gr_gpc0_tpc0_sm0_hww_warp_esr_r(), offset), 0);
	nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gpu_dbg,
			"Cleared HWW warp esr, current reg val: 0x%x",
			nvgpu_readl(g, nvgpu_safe_add_u32(
				gr_gpc0_tpc0_sm0_hww_warp_esr_r(), offset)));
}

void gv11b_gr_intr_handle_ssync_hww(struct gk20a *g, u32 *ssync_esr)
{
	u32 ssync = nvgpu_readl(g, gr_ssync_hww_esr_r());

	*ssync_esr = ssync;
	nvgpu_err(g, "ssync exception: esr 0x%08x", ssync);
	nvgpu_writel(g, gr_ssync_hww_esr_r(),
			 gr_ssync_hww_esr_reset_active_f());
}

static int gv11b_gr_intr_read_sm_error_state(struct gk20a *g,
		struct nvgpu_tsg *tsg, u32 offset, u32 sm_id)
{
	u32 hww_global_esr = nvgpu_readl(g, nvgpu_safe_add_u32(
		gr_gpc0_tpc0_sm0_hww_global_esr_r(), offset));

	u32 hww_warp_esr = nvgpu_readl(g, nvgpu_safe_add_u32(
		gr_gpc0_tpc0_sm0_hww_warp_esr_r(), offset));

	u32 addr_hi = nvgpu_readl(g, nvgpu_safe_add_u32(
			gr_gpc0_tpc0_sm0_hww_warp_esr_pc_hi_r(), offset));
	u32 addr_lo = nvgpu_readl(g, nvgpu_safe_add_u32(
			gr_gpc0_tpc0_sm0_hww_warp_esr_pc_r(), offset));

	u64 hww_warp_esr_pc = hi32_lo32_to_u64(addr_hi, addr_lo);

	u32 hww_global_esr_report_mask = nvgpu_readl(g,
		nvgpu_safe_add_u32(
			gr_gpc0_tpc0_sm0_hww_global_esr_report_mask_r(),
			offset));

	u32 hww_warp_esr_report_mask = nvgpu_readl(g,
		nvgpu_safe_add_u32(
			gr_gpc0_tpc0_sm0_hww_warp_esr_report_mask_r(),
			offset));

	return nvgpu_tsg_store_sm_error_state(tsg, sm_id,
		hww_global_esr, hww_warp_esr,
		hww_warp_esr_pc, hww_global_esr_report_mask,
		hww_warp_esr_report_mask);
}

u32 gv11b_gr_intr_record_sm_error_state(struct gk20a *g, u32 gpc, u32 tpc, u32 sm,
				struct nvgpu_channel *fault_ch)
{
	u32 sm_id;
	u32 offset, sm_per_tpc, tpc_id;
	u32 gpc_offset, gpc_tpc_offset;
	struct nvgpu_tsg *tsg = NULL;
	int err = 0;

#ifdef CONFIG_NVGPU_DEBUGGER
	nvgpu_mutex_acquire(&g->dbg_sessions_lock);
#endif

	sm_per_tpc = nvgpu_get_litter_value(g, GPU_LIT_NUM_SM_PER_TPC);
	gpc_offset = nvgpu_gr_gpc_offset(g, gpc);
	gpc_tpc_offset = nvgpu_safe_add_u32(gpc_offset,
				nvgpu_gr_tpc_offset(g, tpc));

	tpc_id = nvgpu_readl(g, nvgpu_safe_add_u32(
			gr_gpc0_gpm_pd_sm_id_r(tpc), gpc_offset));
	sm_id = nvgpu_safe_add_u32(
			nvgpu_safe_mult_u32(tpc_id, sm_per_tpc),
			sm);

	offset = nvgpu_safe_add_u32(gpc_tpc_offset,
			nvgpu_gr_sm_offset(g, sm));

	if (fault_ch != NULL) {
		tsg = nvgpu_tsg_from_ch(fault_ch);
	}

	if (tsg == NULL) {
		nvgpu_err(g, "no valid tsg");
		goto record_fail;
	}

	err = gv11b_gr_intr_read_sm_error_state(g, tsg, offset, sm_id);
	if (err != 0) {
		nvgpu_err(g, "error writing sm_error_state");
	}

record_fail:
#ifdef CONFIG_NVGPU_DEBUGGER
	nvgpu_mutex_release(&g->dbg_sessions_lock);
#endif

	return sm_id;
}

u32 gv11b_gr_intr_get_warp_esr_sm_hww(struct gk20a *g,
			u32 gpc, u32 tpc, u32 sm)
{
	u32 gpc_offset, tpc_offset, sm_offset, offset;
	u32 hww_warp_esr;

	gpc_offset = nvgpu_gr_gpc_offset(g, gpc);
	tpc_offset = nvgpu_gr_tpc_offset(g, tpc);
	sm_offset = nvgpu_gr_sm_offset(g, sm);

	offset = nvgpu_safe_add_u32(gpc_offset,
			nvgpu_safe_add_u32(tpc_offset, sm_offset));

	hww_warp_esr = nvgpu_readl(g, nvgpu_safe_add_u32(
				gr_gpc0_tpc0_sm0_hww_warp_esr_r(), offset));
	return hww_warp_esr;
}

u32 gv11b_gr_intr_get_sm_hww_global_esr(struct gk20a *g,
			u32 gpc, u32 tpc, u32 sm)
{
	u32 gpc_offset, tpc_offset, sm_offset, offset;
	u32 hww_global_esr;

	gpc_offset = nvgpu_gr_gpc_offset(g, gpc);
	tpc_offset = nvgpu_gr_tpc_offset(g, tpc);
	sm_offset = nvgpu_gr_sm_offset(g, sm);

	offset = nvgpu_safe_add_u32(gpc_offset,
			nvgpu_safe_add_u32(tpc_offset, sm_offset));

	hww_global_esr = nvgpu_readl(g, nvgpu_safe_add_u32(
				 gr_gpc0_tpc0_sm0_hww_global_esr_r(), offset));

	return hww_global_esr;
}

u32 gv11b_gr_intr_get_sm_no_lock_down_hww_global_esr_mask(struct gk20a *g)
{
	/*
	 * These three interrupts don't require locking down the SM. They can
	 * be handled by usermode clients as they aren't fatal. Additionally,
	 * usermode clients may wish to allow some warps to execute while others
	 * are at breakpoints, as opposed to fatal errors where all warps should
	 * halt.
	 */
	u32 global_esr_mask =
		gr_gpc0_tpc0_sm0_hww_global_esr_bpt_int_pending_f()   |
		gr_gpc0_tpc0_sm0_hww_global_esr_bpt_pause_pending_f() |
		gr_gpc0_tpc0_sm0_hww_global_esr_single_step_complete_pending_f();

	(void)g;

	return global_esr_mask;
}

u64 gv11b_gr_intr_get_warp_esr_pc_sm_hww(struct gk20a *g, u32 offset)
{
	u64 hww_warp_esr_pc;
	u32 addr_hi = nvgpu_readl(g, nvgpu_safe_add_u32(
			gr_gpc0_tpc0_sm0_hww_warp_esr_pc_hi_r(), offset));
	u32 addr_lo = nvgpu_readl(g, nvgpu_safe_add_u32(
			gr_gpc0_tpc0_sm0_hww_warp_esr_pc_r(), offset));
	hww_warp_esr_pc = hi32_lo32_to_u64(addr_hi, addr_lo);

	return hww_warp_esr_pc;
}

u32 gv11b_gr_intr_ctxsw_checksum_mismatch_mailbox_val(void)
{
	return gr_fecs_ctxsw_mailbox_value_ctxsw_checksum_mismatch_v();
}

#ifdef CONFIG_NVGPU_HAL_NON_FUSA
void gv11b_gr_intr_set_shader_exceptions(struct gk20a *g, u32 data)
{
	nvgpu_log_fn(g, " ");

	if (data == NVA297_SET_SHADER_EXCEPTIONS_ENABLE_FALSE) {
		nvgpu_writel(g, gr_gpcs_tpcs_sms_hww_warp_esr_report_mask_r(),
				 0);
		nvgpu_writel(g, gr_gpcs_tpcs_sms_hww_global_esr_report_mask_r(),
				 0);
	} else {
		g->ops.gr.intr.set_hww_esr_report_mask(g);
	}
}
#endif

#if defined(CONFIG_NVGPU_DEBUGGER) && defined(CONFIG_NVGPU_GRAPHICS)
void gv11b_gr_intr_set_tex_in_dbg(struct gk20a *g, u32 data)
{
	u32 val;
	u32 flag;

	nvgpu_log_fn(g, " ");

	val = nvgpu_readl(g, gr_gpcs_tpcs_tex_in_dbg_r());
	flag = (data & NVC397_SET_TEX_IN_DBG_TSL1_RVCH_INVALIDATE) != 0U
		? 1U : 0U;
	val = set_field(val, gr_gpcs_tpcs_tex_in_dbg_tsl1_rvch_invalidate_m(),
			gr_gpcs_tpcs_tex_in_dbg_tsl1_rvch_invalidate_f(flag));
	nvgpu_writel(g, gr_gpcs_tpcs_tex_in_dbg_r(), val);

	val = nvgpu_readl(g, gr_gpcs_tpcs_sm_l1tag_ctrl_r());
	flag = (data &
		NVC397_SET_TEX_IN_DBG_SM_L1TAG_CTRL_CACHE_SURFACE_LD) != 0U
		? 1U : 0U;
	val = set_field(val, gr_gpcs_tpcs_sm_l1tag_ctrl_cache_surface_ld_m(),
			gr_gpcs_tpcs_sm_l1tag_ctrl_cache_surface_ld_f(flag));
	flag = (data &
		NVC397_SET_TEX_IN_DBG_SM_L1TAG_CTRL_CACHE_SURFACE_ST) != 0U
		? 1U : 0U;

	val = set_field(val, gr_gpcs_tpcs_sm_l1tag_ctrl_cache_surface_st_m(),
			gr_gpcs_tpcs_sm_l1tag_ctrl_cache_surface_st_f(flag));
	nvgpu_writel(g, gr_gpcs_tpcs_sm_l1tag_ctrl_r(), val);
}

void gv11b_gr_intr_set_coalesce_buffer_size(struct gk20a *g, u32 data)
{
	u32 val;

	nvgpu_log_fn(g, " ");

	val = nvgpu_readl(g, gr_gpcs_tc_debug0_r());
	val = set_field(val, gr_gpcs_tc_debug0_limit_coalesce_buffer_size_m(),
			gr_gpcs_tc_debug0_limit_coalesce_buffer_size_f(data));
	nvgpu_writel(g, gr_gpcs_tc_debug0_r(), val);

	nvgpu_log_fn(g, "done");
}
#endif
