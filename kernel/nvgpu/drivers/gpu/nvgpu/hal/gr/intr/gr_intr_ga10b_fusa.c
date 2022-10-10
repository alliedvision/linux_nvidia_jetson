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
#include <nvgpu/class.h>
#include <nvgpu/engines.h>
#include <nvgpu/nvgpu_err.h>
#include <nvgpu/errata.h>
#include <nvgpu/string.h>

#include <nvgpu/gr/config.h>
#include <nvgpu/gr/gr.h>
#include <nvgpu/gr/gr_instances.h>
#include <nvgpu/gr/gr_intr.h>

#include "common/gr/gr_priv.h"
#include "common/gr/gr_intr_priv.h"
#include "hal/gr/intr/gr_intr_gm20b.h"
#include "hal/gr/intr/gr_intr_gp10b.h"
#include "hal/gr/intr/gr_intr_gv11b.h"
#include "gr_intr_ga10b.h"

#include <nvgpu/hw/ga10b/hw_gr_ga10b.h>

static u32 gr_intr_en_mask(void)
{
	u32 mask =
#ifdef CONFIG_NVGPU_NON_FUSA
		gr_intr_en_notify__prod_f() |
		gr_intr_en_semaphore__prod_f() |
		gr_intr_en_debug_method__prod_f() |
		gr_intr_en_buffer_notify__prod_f() |
#endif
		gr_intr_en_illegal_method__prod_f() |
		gr_intr_en_illegal_notify__prod_f() |
		gr_intr_en_firmware_method__prod_f() |
		gr_intr_en_fecs_error__prod_f() |
		gr_intr_en_class_error__prod_f() |
		gr_intr_en_exception__prod_f() |
		gr_intr_en_fe_debug_intr__prod_f();

	return mask;
}

static u32 get_sm_hww_warp_esr_report_mask(void)
{
	u32 mask = gr_gpc0_tpc0_sm0_hww_warp_esr_report_mask_api_stack_error_report_f() |
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
		gr_gpc0_tpc0_sm0_hww_warp_esr_report_mask_mmu_fault_report_f() |
		gr_gpc0_tpc0_sm0_hww_warp_esr_report_mask_tex_format_report_f() |
		gr_gpc0_tpc0_sm0_hww_warp_esr_report_mask_tex_layout_report_f() |
		gr_gpc0_tpc0_sm0_hww_warp_esr_report_mask_mmu_nack_report_f() |
		gr_gpc0_tpc0_sm0_hww_warp_esr_report_mask_arrive_report_f();

	return mask;
}

static u32 get_sm_hww_global_esr_report_mask(void)
{
	u32 mask = gr_gpc0_tpc0_sm0_hww_global_esr_report_mask_multiple_warp_errors_report_f() |
		gr_gpc0_tpc0_sm0_hww_global_esr_report_mask_bpt_int_report_f() |
		gr_gpc0_tpc0_sm0_hww_global_esr_report_mask_bpt_pause_report_f() |
		gr_gpc0_tpc0_sm0_hww_global_esr_report_mask_single_step_complete_report_f() |
		gr_gpc0_tpc0_sm0_hww_global_esr_report_mask_error_in_trap_report_f() |
		gr_gpc0_tpc0_sm0_hww_global_esr_report_mask_poison_data_report_f();

	return mask;
}

u32 ga10b_gr_intr_enable_mask(struct gk20a *g)
{
	(void)g;
	return gr_intr_en_mask();
}

int ga10b_gr_intr_handle_sw_method(struct gk20a *g, u32 addr,
			u32 class_num, u32 offset, u32 data)
{
#if defined(CONFIG_NVGPU_HAL_NON_FUSA) || (defined(CONFIG_NVGPU_DEBUGGER) && defined(CONFIG_NVGPU_GRAPHICS))
	/*
	 * Hardware divides sw_method enum value by 2 before passing as "offset".
	 * Left shift given offset by 2 to obtain sw_method enum value.
	 */
	u32 left_shift_by_2 = 2U;
#endif

	(void)addr;
	(void)class_num;
	(void)offset;
	(void)data;

	nvgpu_log_fn(g, " ");

#ifdef CONFIG_NVGPU_HAL_NON_FUSA
	if (class_num == AMPERE_COMPUTE_B) {
		switch (offset << left_shift_by_2) {
		case NVC7C0_SET_SHADER_EXCEPTIONS:
			g->ops.gr.intr.set_shader_exceptions(g, data);
			return 0;
		case NVC7C0_SET_CB_BASE:
			/*
			 * This method is only implemented for gm107 in resman
			 * code. However, this method has never been defined in
			 * nvgpu code. This case is added for debug purposes.
			 */
			nvgpu_err(g, "Unhandled set_cb_base method");
			return 0;
		case NVC7C0_SET_BES_CROP_DEBUG4:
			g->ops.gr.set_bes_crop_debug4(g, data);
			return 0;
		case NVC7C0_SET_TEX_IN_DBG:
			gv11b_gr_intr_set_tex_in_dbg(g, data);
			return 0;
		case NVC7C0_SET_SKEDCHECK:
			gv11b_gr_intr_set_skedcheck(g, data);
			return 0;
		}
	}
#endif

#if defined(CONFIG_NVGPU_DEBUGGER) && defined(CONFIG_NVGPU_GRAPHICS)
	if (class_num == AMPERE_B) {
		switch (offset << left_shift_by_2) {
		case NVC797_SET_SHADER_EXCEPTIONS:
			g->ops.gr.intr.set_shader_exceptions(g, data);
			return 0;
		case NVC797_SET_GO_IDLE_TIMEOUT:
			gp10b_gr_intr_set_go_idle_timeout(g, data);
			return 0;
		case NVC797_SET_CIRCULAR_BUFFER_SIZE:
			g->ops.gr.set_circular_buffer_size(g, data);
			return 0;
		case NVC797_SET_ALPHA_CIRCULAR_BUFFER_SIZE:
			g->ops.gr.set_alpha_circular_buffer_size(g, data);
			return 0;
		case NVC797_SET_CB_BASE:
			/*
			 * This method is only implemented for gm107 in resman
			 * code. However, this method has never been defined in
			 * nvgpu code. This case is added for debug purposes.
			 */
			nvgpu_err(g, "Unhandled set_cb_base method");
			return 0;
		case NVC797_SET_BES_CROP_DEBUG4:
			g->ops.gr.set_bes_crop_debug4(g, data);
			return 0;
		case NVC797_SET_TEX_IN_DBG:
			gv11b_gr_intr_set_tex_in_dbg(g, data);
			return 0;
		case NVC797_SET_SKEDCHECK:
			gv11b_gr_intr_set_skedcheck(g, data);
			return 0;
		}
	}
#endif

	return -EINVAL;
}

static u32 ga10b_gr_intr_check_gr_mme_fe1_exception(struct gk20a *g,
							u32 exception)
{
	u32 mme_fe1_hww_esr;
	u32 info, info_mthd, info_mthd2;
	u32 mme_fe1_exception = exception & gr_exception_mme_fe1_m();

	if (mme_fe1_exception == 0U) {
		return 0U;
	}

	mme_fe1_hww_esr = nvgpu_readl(g, gr_mme_fe1_hww_esr_r());
	info = nvgpu_readl(g, gr_mme_fe1_hww_esr_info_r());
	info_mthd = nvgpu_readl(g, gr_mme_fe1_hww_esr_info_mthd_r());
	info_mthd2 = nvgpu_readl(g, gr_mme_fe1_hww_esr_info_mthd2_r());

	nvgpu_report_err_to_sdl(g, NVGPU_ERR_MODULE_PGRAPH,
			GPU_PGRAPH_MME_FE1_EXCEPTION);
	nvgpu_err(g, "mme_fe1 exception: esr 0x%08x, info 0x%08x,"
		     "info_mthd 0x%08x, info_mthd2 0x%08x",
		      mme_fe1_hww_esr, info, info_mthd, info_mthd2);

	nvgpu_writel(g, gr_mme_fe1_hww_esr_r(),
			gr_mme_fe1_hww_esr_reset_active_f());

	return mme_fe1_exception;
}

bool ga10b_gr_intr_handle_exceptions(struct gk20a *g, bool *is_gpc_exception)
{
	u32 gpc_reset = 0U;
	u32 exception = nvgpu_readl(g, gr_exception_r());

	nvgpu_log(g, gpu_dbg_intr | gpu_dbg_gpu_dbg,
				"exception 0x%08x", exception);

	gpc_reset = gm20b_gr_intr_check_gr_fe_exception(g, exception);
	gpc_reset |= gm20b_gr_intr_check_gr_memfmt_exception(g, exception);
	gpc_reset |= gm20b_gr_intr_check_gr_pd_exception(g, exception);
	gpc_reset |= gm20b_gr_intr_check_gr_scc_exception(g, exception);
	gpc_reset |= gm20b_gr_intr_check_gr_ds_exception(g, exception);
	if (!nvgpu_is_enabled(g, NVGPU_SUPPORT_MIG)) {
		gpc_reset |= gm20b_gr_intr_check_gr_ssync_exception(g, exception);
	}
	gpc_reset |= gm20b_gr_intr_check_gr_mme_exception(g, exception);
	gpc_reset |= gm20b_gr_intr_check_gr_sked_exception(g, exception);
	gpc_reset |= ga10b_gr_intr_check_gr_mme_fe1_exception(g, exception);

	/* check if a gpc exception has occurred */
	if ((exception & gr_exception_gpc_m()) != 0U) {
		*is_gpc_exception = true;
	}

	return (gpc_reset != 0U)? true: false;
}

void ga10b_gr_intr_set_hww_esr_report_mask(struct gk20a *g)
{
	u32 sm_hww_warp_esr_report_mask;
	u32 sm_hww_global_esr_report_mask;

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

	/*
	 * setup sm warp esr report masks
	 */
	nvgpu_writel(g, gr_gpcs_tpcs_sms_hww_warp_esr_report_mask_r(),
		sm_hww_warp_esr_report_mask | get_sm_hww_warp_esr_report_mask());

	nvgpu_writel(g, gr_gpcs_tpcs_sms_hww_global_esr_report_mask_r(),
		sm_hww_global_esr_report_mask | get_sm_hww_global_esr_report_mask());

	nvgpu_log_info(g,
		"configured (global, warp)_esr_report_mask(0x%x, 0x%x)",
		sm_hww_global_esr_report_mask | get_sm_hww_global_esr_report_mask(),
		sm_hww_warp_esr_report_mask | get_sm_hww_warp_esr_report_mask());
}

u32 ga10b_gr_intr_get_tpc_exception(struct gk20a *g, u32 offset,
				    struct nvgpu_gr_tpc_exception *pending_tpc)
{
	u32 tpc_exception = nvgpu_readl(g, nvgpu_safe_add_u32(
				gr_gpc0_tpc0_tpccs_tpc_exception_r(), offset));

	(void) memset(pending_tpc, 0, sizeof(struct nvgpu_gr_tpc_exception));

	if (gr_gpc0_tpc0_tpccs_tpc_exception_sm_v(tpc_exception) ==
		gr_gpc0_tpc0_tpccs_tpc_exception_sm_pending_v()) {
			pending_tpc->sm_exception = true;
	}

	if ((tpc_exception & gr_gpc0_tpc0_tpccs_tpc_exception_mpc_m()) != 0U) {
		pending_tpc->mpc_exception = true;
	}

	if ((tpc_exception & gr_gpc0_tpc0_tpccs_tpc_exception_pe_m()) != 0U) {
		pending_tpc->pe_exception = true;
	}

	return tpc_exception;
}

void ga10b_gr_intr_enable_gpc_exceptions(struct gk20a *g,
					 struct nvgpu_gr_config *gr_config)
{
	u32 tpc_mask, tpc_mask_calc;

	nvgpu_writel(g, gr_gpcs_tpcs_tpccs_tpc_exception_en_r(),
			gr_gpcs_tpcs_tpccs_tpc_exception_en_sm_enabled_f() |
			gr_gpcs_tpcs_tpccs_tpc_exception_en_pe_enabled_f() |
			gr_gpcs_tpcs_tpccs_tpc_exception_en_mpc_enabled_f());

	tpc_mask_calc = BIT32(
			 nvgpu_gr_config_get_max_tpc_per_gpc_count(gr_config));
	tpc_mask =
		gr_gpcs_gpccs_gpc_exception_en_tpc_f(
			nvgpu_safe_sub_u32(tpc_mask_calc, 1U));

	/*
	 * Enable exceptions from ROP subunits: zrop and crop. The rrh subunit
	 * does not have a subunit level enable.
	 */
	g->ops.gr.intr.enable_gpc_zrop_hww(g);
	g->ops.gr.intr.enable_gpc_crop_hww(g);

	nvgpu_writel(g, gr_gpcs_gpccs_gpc_exception_en_r(),
		(tpc_mask | gr_gpcs_gpccs_gpc_exception_en_gcc_enabled_f() |
			    gr_gpcs_gpccs_gpc_exception_en_gpccs_enabled_f() |
			    gr_gpcs_gpccs_gpc_exception_en_gpcmmu0_enabled_f() |
			    gr_gpcs_gpccs_gpc_exception_en_crop0_enabled_f() |
			    gr_gpcs_gpccs_gpc_exception_en_zrop0_enabled_f() |
			    gr_gpcs_gpccs_gpc_exception_en_rrh0_enabled_f() |
			    gr_gpcs_gpccs_gpc_exception_en_crop1_enabled_f() |
			    gr_gpcs_gpccs_gpc_exception_en_zrop1_enabled_f() |
			    gr_gpcs_gpccs_gpc_exception_en_rrh1_enabled_f()));
}

void ga10b_gr_intr_enable_exceptions(struct gk20a *g,
				struct nvgpu_gr_config *gr_config, bool enable)
{
	u32 reg_val = 0U;

	if (!enable) {
		nvgpu_writel(g, gr_exception_en_r(), reg_val);
		nvgpu_writel(g, gr_exception1_en_r(), reg_val);
		return;
	}

	/*
	 * clear exceptions :
	 * other than SM : hww_esr are reset in *enable_hww_excetpions*
	 * SM            : cleared in *set_hww_esr_report_mask*
	 */

	/* enable exceptions */
	reg_val = BIT32(nvgpu_gr_config_get_gpc_count(gr_config));
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
		  gr_exception_en_mme_fe1_enabled_f() |
		  gr_exception_en_gpc_enabled_f();

	nvgpu_log(g, gpu_dbg_info, "gr_exception_en 0x%08x", reg_val);

	nvgpu_writel(g, gr_exception_en_r(), reg_val);
}

static void ga10b_gr_intr_report_gpcmmu_ecc_err(struct gk20a *g,
		u32 ecc_status, u32 gpc)
{
	if ((ecc_status &
	     gr_gpc0_mmu0_l1tlb_ecc_status_corrected_err_l1tlb_sa_data_m()) != 0U) {
		nvgpu_err(g, "corrected ecc sa data error. "
				"gpc_id(%d)", gpc);
	}
	if ((ecc_status &
	     gr_gpc0_mmu0_l1tlb_ecc_status_uncorrected_err_l1tlb_sa_data_m()) != 0U) {
		nvgpu_report_err_to_sdl(g, NVGPU_ERR_MODULE_MMU,
				GPU_MMU_L1TLB_SA_DATA_ECC_UNCORRECTED);
		nvgpu_err(g, "uncorrected ecc sa data error"
				"gpc_id(%d)", gpc);
	}
	if ((ecc_status &
	     gr_gpc0_mmu0_l1tlb_ecc_status_corrected_err_l1tlb_fa_data_m()) != 0U) {
		nvgpu_err(g, "corrected ecc fa data error"
				"gpc_id(%d)", gpc);
	}
	if ((ecc_status &
	     gr_gpc0_mmu0_l1tlb_ecc_status_uncorrected_err_l1tlb_fa_data_m()) != 0U) {
		nvgpu_report_err_to_sdl(g, NVGPU_ERR_MODULE_MMU,
				GPU_MMU_L1TLB_FA_DATA_ECC_UNCORRECTED);
		nvgpu_err(g, "uncorrected ecc fa data error"
				"gpc_id(%d)", gpc);
	}
}

void ga10b_gr_intr_handle_gpc_gpcmmu_exception(struct gk20a *g, u32 gpc,
		u32 gpc_exception, u32 *corrected_err, u32 *uncorrected_err)
{
	u32 offset = nvgpu_gr_gpc_offset(g, gpc);
	u32 ecc_status, ecc_addr, corrected_cnt, uncorrected_cnt;
	u32 corrected_delta, uncorrected_delta;
	u32 corrected_overflow, uncorrected_overflow;
	u32 hww_esr;

	if ((gpc_exception & gr_gpc0_gpccs_gpc_exception_gpcmmu0_m()) == 0U) {
		return;
	}

	hww_esr = nvgpu_readl(g,
			nvgpu_safe_add_u32(gr_gpc0_mmu0_gpcmmu_global_esr_r(),
						offset));

	if ((hww_esr & (gr_gpc0_mmu0_gpcmmu_global_esr_ecc_corrected_m() |
		gr_gpc0_mmu0_gpcmmu_global_esr_ecc_uncorrected_m())) == 0U) {
		return;
	}

	ecc_status = nvgpu_readl(g, nvgpu_safe_add_u32(
			gr_gpc0_mmu0_l1tlb_ecc_status_r(), offset));
	ecc_addr = nvgpu_readl(g, nvgpu_safe_add_u32(
			gr_gpc0_mmu0_l1tlb_ecc_address_r(), offset));
	corrected_cnt = nvgpu_readl(g, nvgpu_safe_add_u32(
			gr_gpc0_mmu0_l1tlb_ecc_corrected_err_count_r(),	offset));
	uncorrected_cnt = nvgpu_readl(g, nvgpu_safe_add_u32(
			gr_gpc0_mmu0_l1tlb_ecc_uncorrected_err_count_r(), offset));
	corrected_delta =
	 gr_gpc0_mmu0_l1tlb_ecc_corrected_err_count_total_v(corrected_cnt);
	uncorrected_delta =
	 gr_gpc0_mmu0_l1tlb_ecc_uncorrected_err_count_total_v(uncorrected_cnt);
	corrected_overflow = ecc_status &
	 gr_gpc0_mmu0_l1tlb_ecc_status_corrected_err_total_counter_overflow_m();
	uncorrected_overflow = ecc_status &
	 gr_gpc0_mmu0_l1tlb_ecc_status_uncorrected_err_total_counter_overflow_m();

	/* clear the interrupt */
	if ((corrected_delta > 0U) || (corrected_overflow != 0U)) {
		nvgpu_writel(g, nvgpu_safe_add_u32(
			     gr_gpc0_mmu0_l1tlb_ecc_corrected_err_count_r(),
			     offset), 0U);
	}
	if ((uncorrected_delta > 0U) || (uncorrected_overflow != 0U)) {
		nvgpu_writel(g, nvgpu_safe_add_u32(
			     gr_gpc0_mmu0_l1tlb_ecc_uncorrected_err_count_r(),
			     offset), 0U);
	}

	nvgpu_writel(g, nvgpu_safe_add_u32(
				gr_gpc0_mmu0_l1tlb_ecc_status_r(), offset),
				gr_gpc0_mmu0_l1tlb_ecc_status_reset_task_f());

	/* Handle overflow */
	if (corrected_overflow != 0U) {
		corrected_delta = nvgpu_safe_add_u32(corrected_delta,
		   BIT32(gr_gpc0_mmu0_l1tlb_ecc_corrected_err_count_total_s()));
		nvgpu_info(g, "mmu l1tlb ecc counter corrected overflow!");
	}
	if (uncorrected_overflow != 0U) {
		uncorrected_delta = nvgpu_safe_add_u32(uncorrected_delta,
		  BIT32(gr_gpc0_mmu0_l1tlb_ecc_uncorrected_err_count_total_s()));
		nvgpu_info(g, "mmu l1tlb ecc counter uncorrected overflow!");
	}

	*corrected_err = nvgpu_safe_add_u32(*corrected_err, corrected_delta);
	*uncorrected_err = nvgpu_safe_add_u32(*uncorrected_err, uncorrected_delta);

	nvgpu_log(g, gpu_dbg_intr,
		"mmu l1tlb gpc:%d ecc interrupt intr: 0x%x", gpc, hww_esr);

	ga10b_gr_intr_report_gpcmmu_ecc_err(g, ecc_status, gpc);

	nvgpu_log(g, gpu_dbg_intr,
		"ecc error address: 0x%x", ecc_addr);
	nvgpu_log(g, gpu_dbg_intr,
		"ecc error count corrected: %d, uncorrected %d",
		(u32)*corrected_err, (u32)*uncorrected_err);
}

static void ga10b_gr_intr_set_l1_tag_uncorrected_err(struct gk20a *g,
	u32 l1_tag_ecc_status, struct nvgpu_gr_sm_ecc_status *ecc_status)
{
	(void)g;

	if ((l1_tag_ecc_status &
	    gr_pri_gpc0_tpc0_sm_l1_tag_ecc_status_uncorrected_err_el1_0_m()) != 0U) {
		nvgpu_report_err_to_sdl(g, NVGPU_ERR_MODULE_SM,
				GPU_SM_L1_TAG_ECC_UNCORRECTED);
		nvgpu_err(g, "sm_l1_tag_ecc_uncorrected");
		ecc_status->err_id[ecc_status->err_count] =
				GPU_SM_L1_TAG_ECC_UNCORRECTED;
		ecc_status->err_count =
				nvgpu_safe_add_u32(ecc_status->err_count, 1U);
	}

	if ((l1_tag_ecc_status &
	     gr_pri_gpc0_tpc0_sm_l1_tag_ecc_status_uncorrected_err_miss_fifo_m()) != 0U) {
		nvgpu_report_err_to_sdl(g, NVGPU_ERR_MODULE_SM,
				GPU_SM_L1_TAG_MISS_FIFO_ECC_UNCORRECTED);
		nvgpu_err(g, "sm_l1_tag_miss_fifo_ecc_uncorrected");
		ecc_status->err_id[ecc_status->err_count] =
				GPU_SM_L1_TAG_MISS_FIFO_ECC_UNCORRECTED;
		ecc_status->err_count =
				nvgpu_safe_add_u32(ecc_status->err_count, 1U);
	}

	if ((l1_tag_ecc_status &
	     gr_pri_gpc0_tpc0_sm_l1_tag_ecc_status_uncorrected_err_pixrpf_m()) != 0U) {
		nvgpu_report_err_to_sdl(g, NVGPU_ERR_MODULE_SM,
				GPU_SM_L1_TAG_S2R_PIXPRF_ECC_UNCORRECTED);
		nvgpu_err(g, "sm_l1_tag_s2r_pixprf_ecc_uncorrected");
		ecc_status->err_id[ecc_status->err_count] =
				GPU_SM_L1_TAG_S2R_PIXPRF_ECC_UNCORRECTED;
		ecc_status->err_count =
				nvgpu_safe_add_u32(ecc_status->err_count, 1U);
	}
}

static void ga10b_gr_intr_set_l1_tag_corrected_err(struct gk20a *g,
	u32 l1_tag_ecc_status, struct nvgpu_gr_sm_ecc_status *ecc_status)
{
	(void)g;

	if ((l1_tag_ecc_status &
	    gr_pri_gpc0_tpc0_sm_l1_tag_ecc_status_corrected_err_el1_0_m()) != 0U) {
		nvgpu_report_err_to_sdl(g, NVGPU_ERR_MODULE_SM,
				GPU_SM_L1_TAG_ECC_CORRECTED);
		nvgpu_err(g, "sm_l1_tag_ecc_corrected");
		ecc_status->err_id[ecc_status->err_count] =
				GPU_SM_L1_TAG_ECC_CORRECTED;
		ecc_status->err_count =
				nvgpu_safe_add_u32(ecc_status->err_count, 1U);
	}
}

static bool ga10b_gr_intr_sm_l1_tag_ecc_status_errors(struct gk20a *g,
	u32 l1_tag_ecc_status, struct nvgpu_gr_sm_ecc_status *ecc_status)
{
	u32 corr_err, uncorr_err;
	bool err_status = true;

	corr_err =  l1_tag_ecc_status &
		gr_pri_gpc0_tpc0_sm_l1_tag_ecc_status_corrected_err_el1_0_m();

	uncorr_err = l1_tag_ecc_status &
		(gr_pri_gpc0_tpc0_sm_l1_tag_ecc_status_uncorrected_err_el1_0_m() |
		 gr_pri_gpc0_tpc0_sm_l1_tag_ecc_status_uncorrected_err_pixrpf_m() |
		 gr_pri_gpc0_tpc0_sm_l1_tag_ecc_status_uncorrected_err_miss_fifo_m());

	if ((corr_err == 0U) && (uncorr_err == 0U)) {
		err_status = false;
	}

	ecc_status->err_count = 0U;
	ecc_status->corrected_err_status = corr_err;
	ecc_status->uncorrected_err_status = uncorr_err;

	ga10b_gr_intr_set_l1_tag_corrected_err(g, l1_tag_ecc_status, ecc_status);
	ga10b_gr_intr_set_l1_tag_uncorrected_err(g, l1_tag_ecc_status, ecc_status);

	return err_status;
}

static bool ga10b_gr_intr_sm_lrf_ecc_status_errors(struct gk20a *g,
	u32 lrf_ecc_status, struct nvgpu_gr_sm_ecc_status *ecc_status)
{
	u32 uncorr_err;
	bool err_status = true;

	(void)g;

	uncorr_err = lrf_ecc_status &
		(gr_pri_gpc0_tpc0_sm_lrf_ecc_status_uncorrected_err_qrfdp0_m() |
		 gr_pri_gpc0_tpc0_sm_lrf_ecc_status_uncorrected_err_qrfdp1_m() |
		 gr_pri_gpc0_tpc0_sm_lrf_ecc_status_uncorrected_err_qrfdp2_m() |
		 gr_pri_gpc0_tpc0_sm_lrf_ecc_status_uncorrected_err_qrfdp3_m());

	if (uncorr_err == 0U) {
		err_status = false;
	}

	ecc_status->err_count = 0U;

	if (uncorr_err != 0U) {
		nvgpu_report_err_to_sdl(g, NVGPU_ERR_MODULE_SM,
				GPU_SM_LRF_ECC_UNCORRECTED);
		nvgpu_err(g, "sm_lrf_ecc_uncorrected");
		ecc_status->err_id[ecc_status->err_count] =
				GPU_SM_LRF_ECC_UNCORRECTED;
		ecc_status->err_count =
			nvgpu_safe_add_u32(ecc_status->err_count, 1U);
	}

	ecc_status->corrected_err_status = 0U;
	ecc_status->uncorrected_err_status = uncorr_err;

	return err_status;
}

static bool ga10b_gr_intr_sm_cbu_ecc_status_errors(struct gk20a *g,
	u32 cbu_ecc_status, struct nvgpu_gr_sm_ecc_status *ecc_status)
{
	u32 corr_err, uncorr_err;
	bool err_status = true;

	(void)g;

	corr_err = cbu_ecc_status &
	  (gr_pri_gpc0_tpc0_sm_cbu_ecc_status_corrected_err_warp_sm0_m() |
	   gr_pri_gpc0_tpc0_sm_cbu_ecc_status_corrected_err_barrier_sm0_m());

	uncorr_err = cbu_ecc_status &
	  (gr_pri_gpc0_tpc0_sm_cbu_ecc_status_uncorrected_err_warp_sm0_m() |
	   gr_pri_gpc0_tpc0_sm_cbu_ecc_status_uncorrected_err_barrier_sm0_m());

	if ((corr_err == 0U) && (uncorr_err == 0U)) {
		err_status = false;
	}

	ecc_status->err_count = 0;

	if (uncorr_err != 0U) {
		nvgpu_report_err_to_sdl(g, NVGPU_ERR_MODULE_SM,
				GPU_SM_CBU_ECC_UNCORRECTED);
		nvgpu_err(g, "sm_cbu_ecc_uncorrected");
		ecc_status->err_id[ecc_status->err_count] =
				GPU_SM_CBU_ECC_UNCORRECTED;
		ecc_status->err_count =
			nvgpu_safe_add_u32(ecc_status->err_count, 1U);
	}

	ecc_status->corrected_err_status = corr_err;
	ecc_status->uncorrected_err_status = uncorr_err;

	return err_status;
}

static bool ga10b_gr_intr_sm_l1_data_ecc_status_errors(struct gk20a *g,
	u32 l1_data_ecc_status, struct nvgpu_gr_sm_ecc_status *ecc_status)
{
	u32 corr_err, uncorr_err;
	bool err_status = true;

	(void)g;

	corr_err = l1_data_ecc_status &
		gr_pri_gpc0_tpc0_sm_l1_data_ecc_status_corrected_err_el1_0_m();
	uncorr_err = l1_data_ecc_status &
		gr_pri_gpc0_tpc0_sm_l1_data_ecc_status_uncorrected_err_el1_0_m();

	if ((corr_err == 0U) && (uncorr_err == 0U)) {
		err_status = false;
	}

	ecc_status->err_count = 0U;

	if (uncorr_err != 0U) {
		nvgpu_report_err_to_sdl(g, NVGPU_ERR_MODULE_SM,
				GPU_SM_L1_DATA_ECC_UNCORRECTED);
		nvgpu_err(g, "sm_l1_data_ecc_uncorrected");
		ecc_status->err_id[ecc_status->err_count] =
				GPU_SM_L1_DATA_ECC_UNCORRECTED;
		ecc_status->err_count =
			nvgpu_safe_add_u32(ecc_status->err_count, 1U);
	}

	ecc_status->corrected_err_status = corr_err;
	ecc_status->uncorrected_err_status = uncorr_err;

	return err_status;
}

static void ga10b_gr_intr_set_rams_uncorrected_err(struct gk20a *g,
	u32 rams_ecc_status, struct nvgpu_gr_sm_ecc_status *ecc_status)
{
	(void)g;

	if ((rams_ecc_status &
	    gr_pri_gpc0_tpc0_sm_rams_ecc_status_uncorrected_err_l0ic_data_m()) != 0U) {
		ecc_status->err_id[ecc_status->err_count] =
				GPU_SM_ICACHE_L0_DATA_ECC_UNCORRECTED;
		ecc_status->err_count =
				nvgpu_safe_add_u32(ecc_status->err_count, 1U);
	}

	if ((rams_ecc_status &
	    gr_pri_gpc0_tpc0_sm_rams_ecc_status_uncorrected_err_l0ic_predecode_m()) != 0U) {
		ecc_status->err_id[ecc_status->err_count] =
				GPU_SM_ICACHE_L0_PREDECODE_ECC_UNCORRECTED;
		ecc_status->err_count =
				nvgpu_safe_add_u32(ecc_status->err_count, 1U);
	}

	if ((rams_ecc_status &
	     gr_pri_gpc0_tpc0_sm_rams_ecc_status_uncorrected_err_urf_data_m()) != 0U) {
		ecc_status->err_id[ecc_status->err_count] =
				GPU_SM_RAMS_URF_ECC_UNCORRECTED;
		ecc_status->err_count =
				nvgpu_safe_add_u32(ecc_status->err_count, 1U);
	}
}

static bool ga10b_gr_intr_sm_rams_ecc_status_errors(struct gk20a *g,
	u32 rams_ecc_status, struct nvgpu_gr_sm_ecc_status *ecc_status)
{
	u32 uncorr_err;
	bool err_status = true;

	(void)g;

	uncorr_err = rams_ecc_status &\
		(gr_pri_gpc0_tpc0_sm_rams_ecc_status_uncorrected_err_l0ic_data_m() |\
		 gr_pri_gpc0_tpc0_sm_rams_ecc_status_uncorrected_err_l0ic_predecode_m() |\
		 gr_pri_gpc0_tpc0_sm_rams_ecc_status_uncorrected_err_urf_data_m());

	if (uncorr_err == 0U) {
		err_status = false;
	}

	ecc_status->err_count = 0U;

	ecc_status->corrected_err_status = 0U;
	ecc_status->uncorrected_err_status = uncorr_err;

	ga10b_gr_intr_set_rams_uncorrected_err(g, rams_ecc_status, ecc_status);

	return err_status;
}


static bool ga10b_gr_intr_sm_icache_ecc_status_errors(struct gk20a *g,
	u32 icache_ecc_status, struct nvgpu_gr_sm_ecc_status *ecc_status)
{
	u32 corr_err, uncorr_err;
	bool err_status = true;

	(void)g;

	corr_err = icache_ecc_status &
		gr_pri_gpc0_tpc0_sm_icache_ecc_status_corrected_err_l1_data_m();

	uncorr_err = icache_ecc_status &
		gr_pri_gpc0_tpc0_sm_icache_ecc_status_uncorrected_err_l1_data_m();

	if ((corr_err == 0U) && (uncorr_err == 0U)) {
		err_status = false;
	}

	ecc_status->err_count = 0U;

	if (uncorr_err != 0U) {
		nvgpu_report_err_to_sdl(g, NVGPU_ERR_MODULE_SM,
					GPU_SM_ICACHE_L1_DATA_ECC_UNCORRECTED);
		nvgpu_err(g, "sm_icache_l1_data_ecc_uncorrected");
		ecc_status->err_id[ecc_status->err_count] =
				GPU_SM_ICACHE_L1_DATA_ECC_UNCORRECTED;
		ecc_status->err_count =
			nvgpu_safe_add_u32(ecc_status->err_count, 1U);
	}

	ecc_status->corrected_err_status = corr_err;
	ecc_status->uncorrected_err_status = uncorr_err;

	return err_status;
}

static void ga10b_gr_intr_report_tpc_sm_rams_ecc_err(struct gk20a *g,
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
			nvgpu_report_err_to_sdl(g, NVGPU_ERR_MODULE_SM,
					GPU_SM_ICACHE_L0_DATA_ECC_UNCORRECTED);
			nvgpu_err(g, "sm_icache_l0_data_ecc_uncorrected. "
					"gpc_id(%d), tpc_id(%d)", gpc, tpc);
		}

		if (ecc_status->err_id[i] == GPU_SM_ICACHE_L0_PREDECODE_ECC_UNCORRECTED) {
			nvgpu_report_err_to_sdl(g, NVGPU_ERR_MODULE_SM,
					GPU_SM_ICACHE_L0_PREDECODE_ECC_UNCORRECTED);
			nvgpu_err(g, "sm_icache_l0_predecode_ecc_uncorrected. "
					"gpc_id(%d), tpc_id(%d)", gpc, tpc);
		}

		if (ecc_status->err_id[i] == GPU_SM_RAMS_URF_ECC_UNCORRECTED) {
			nvgpu_report_err_to_sdl(g, NVGPU_ERR_MODULE_SM,
					GPU_SM_RAMS_URF_ECC_UNCORRECTED);
			nvgpu_err(g, "sm_rams_urf_ecc_corrected. "
					"gpc_id(%d), tpc_id(%d)", gpc, tpc);
		}
	}
}

static void ga10b_gr_intr_handle_tpc_sm_rams_ecc_exception(struct gk20a *g,
		u32 gpc, u32 tpc)
{
	u32 gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_GPC_STRIDE);
	u32 tpc_in_gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_TPC_IN_GPC_STRIDE);
	u32 offset;
	u32 rams_ecc_status;
	u32 rams_uncorrected_err_count_delta = 0U;
	bool is_rams_ecc_uncorrected_total_err_overflow = false;
	struct nvgpu_gr_sm_ecc_status ecc_status;

	offset = nvgpu_safe_add_u32(
			nvgpu_safe_mult_u32(gpc_stride, gpc),
			nvgpu_safe_mult_u32(tpc_in_gpc_stride, tpc));


	/* Check for SM RAMS ECC errors. */
	rams_ecc_status = nvgpu_readl(g, nvgpu_safe_add_u32(
		gr_pri_gpc0_tpc0_sm_rams_ecc_status_r(), offset));

	(void) memset(&ecc_status, 0, sizeof(struct nvgpu_gr_sm_ecc_status));

	if (g->ops.gr.intr.sm_ecc_status_errors(g, rams_ecc_status,
				SM_RAMS_ECC_ERROR, &ecc_status) == false) {
		return;
	}

	rams_uncorrected_err_count_delta =
		gr_pri_gpc0_tpc0_sm_rams_ecc_uncorrected_err_count_total_v(
			nvgpu_readl(g, nvgpu_safe_add_u32(
				gr_pri_gpc0_tpc0_sm_rams_ecc_uncorrected_err_count_r(),
				offset)));
	is_rams_ecc_uncorrected_total_err_overflow =
		gr_pri_gpc0_tpc0_sm_rams_ecc_status_uncorrected_err_total_counter_overflow_v(rams_ecc_status) != 0U;

	if ((rams_uncorrected_err_count_delta > 0U) || is_rams_ecc_uncorrected_total_err_overflow) {
		nvgpu_log(g, gpu_dbg_fn | gpu_dbg_intr,
			"Uncorrected error (DBE) detected in SM RAMS! err_mask [%08x] is_overf [%d]",
			ecc_status.uncorrected_err_status, is_rams_ecc_uncorrected_total_err_overflow);

		/* HW uses 16-bits counter */
		if (is_rams_ecc_uncorrected_total_err_overflow) {
			rams_uncorrected_err_count_delta =
			   nvgpu_safe_add_u32(
				rams_uncorrected_err_count_delta,
				BIT32(gr_pri_gpc0_tpc0_sm_rams_ecc_uncorrected_err_count_total_s()));
		}
		g->ecc.gr.sm_rams_ecc_uncorrected_err_count[gpc][tpc].counter =
		  nvgpu_safe_add_u32(
			g->ecc.gr.sm_rams_ecc_uncorrected_err_count[gpc][tpc].counter,
			rams_uncorrected_err_count_delta);
		nvgpu_writel(g, nvgpu_safe_add_u32(
			gr_pri_gpc0_tpc0_sm_rams_ecc_uncorrected_err_count_r(), offset),
			0U);
	}

	ga10b_gr_intr_report_tpc_sm_rams_ecc_err(g, &ecc_status, gpc, tpc);
	nvgpu_writel(g, nvgpu_safe_add_u32(
			gr_pri_gpc0_tpc0_sm_rams_ecc_status_r(), offset),
			gr_pri_gpc0_tpc0_sm_rams_ecc_status_reset_task_f());
}

bool ga10b_gr_intr_sm_ecc_status_errors(struct gk20a *g,
	u32 ecc_status_reg, enum nvgpu_gr_sm_ecc_error_types err_type,
	struct nvgpu_gr_sm_ecc_status *ecc_status)
{
	bool err_status = false;

	if (err_type == SM_ICACHE_ECC_ERROR) {
		err_status = ga10b_gr_intr_sm_icache_ecc_status_errors(g,
						ecc_status_reg, ecc_status);
	} else if (err_type == SM_LRF_ECC_ERROR) {
		err_status = ga10b_gr_intr_sm_lrf_ecc_status_errors(g,
						ecc_status_reg, ecc_status);
	} else if (err_type == SM_L1_TAG_ERROR) {
		err_status = ga10b_gr_intr_sm_l1_tag_ecc_status_errors(g,
						ecc_status_reg, ecc_status);
	} else if (err_type == SM_CBU_ECC_ERROR) {
		err_status = ga10b_gr_intr_sm_cbu_ecc_status_errors(g,
						ecc_status_reg, ecc_status);
	} else if (err_type == SM_L1_DATA_ECC_ERROR) {
		err_status = ga10b_gr_intr_sm_l1_data_ecc_status_errors(g,
						ecc_status_reg, ecc_status);
	} else if (err_type == SM_RAMS_ECC_ERROR) {
		err_status = ga10b_gr_intr_sm_rams_ecc_status_errors(g,
				ecc_status_reg, ecc_status);
	}

	return err_status;
}
void ga10b_gr_intr_handle_tpc_sm_ecc_exception(struct gk20a *g, u32 gpc,
					       u32 tpc)
{
	gv11b_gr_intr_handle_tpc_sm_ecc_exception(g, gpc, tpc);
	/* Check for RAMS ECC errors. */
	ga10b_gr_intr_handle_tpc_sm_rams_ecc_exception(g, gpc, tpc);
}

void ga10b_gr_intr_enable_gpc_crop_hww(struct gk20a *g)
{
	nvgpu_log_fn(g, " ");
	nvgpu_writel(g, gr_gpcs_rops_crop_hww_esr_r(),
			gr_gpcs_rops_crop_hww_esr_reset_active_f() |
			gr_gpcs_rops_crop_hww_esr_en_enable_f());
}

void ga10b_gr_intr_enable_gpc_zrop_hww(struct gk20a *g)
{
	nvgpu_log_fn(g, " ");
	nvgpu_writel(g, gr_gpcs_rops_zrop_hww_esr_r(),
			gr_gpcs_rops_zrop_hww_esr_reset_active_f() |
			gr_gpcs_rops_zrop_hww_esr_en_enable_f());
}

void ga10b_gr_intr_handle_gpc_crop_hww(struct gk20a *g, u32 gpc,
		u32 gpc_exception)
{
	u32 gpc_offset = 0U;
	u32 reg_offset = 0U;
	u32 hww_esr = 0U;
	u32 crop_pending_masks[] = {
		gr_gpc0_gpccs_gpc_exception_crop0_pending_f(),
		gr_gpc0_gpccs_gpc_exception_crop1_pending_f()
	};
	u32 num_crop_pending_masks =
		sizeof(crop_pending_masks)/sizeof(*crop_pending_masks);
	u32 i = 0U;
	struct nvgpu_gr *gr = nvgpu_gr_get_cur_instance_ptr(g);
	struct nvgpu_gr_config *config = gr->config;
	u32 rop_id;

	if ((gpc_exception & (gr_gpc0_gpccs_gpc_exception_crop0_pending_f() |
			gr_gpc0_gpccs_gpc_exception_crop1_pending_f())) == 0U) {
		return;
	}

	gpc_offset = nvgpu_gr_gpc_offset(g, gpc);
	for (i = 0U; i < num_crop_pending_masks; i++) {
		rop_id = i;
		if ((gpc_exception & crop_pending_masks[i]) == 0U) {
			continue;
		}
		if (nvgpu_is_errata_present(g, NVGPU_ERRATA_3524791)) {
			rop_id = gr_config_get_gpc_rop_logical_id_map(
					config, gpc)[i];
			nvgpu_assert(rop_id != UINT_MAX);
		}
		reg_offset = nvgpu_safe_add_u32(gpc_offset,
				nvgpu_gr_rop_offset(g, rop_id));
		reg_offset = nvgpu_safe_add_u32(
				gr_gpc0_rop0_crop_hww_esr_r(),
				reg_offset);
		hww_esr = nvgpu_readl(g, reg_offset);

		nvgpu_err(g,
			"gpc(%u) rop(%u) crop_hww_esr(0x%08x)", gpc, rop_id,
			hww_esr);
		nvgpu_writel(g, reg_offset,
			gr_gpc0_rop0_crop_hww_esr_reset_active_f() |
			gr_gpc0_rop0_crop_hww_esr_en_enable_f());
	}
}

void ga10b_gr_intr_handle_gpc_zrop_hww(struct gk20a *g, u32 gpc,
		u32 gpc_exception)
{
	u32 gpc_offset = 0U;
	u32 reg_offset = 0U;
	u32 hww_esr = 0U;
	u32 zrop_pending_masks[] = {
		gr_gpc0_gpccs_gpc_exception_zrop0_pending_f(),
		gr_gpc0_gpccs_gpc_exception_zrop1_pending_f()
	};
	u32 num_zrop_pending_masks =
		sizeof(zrop_pending_masks)/sizeof(*zrop_pending_masks);
	u32 i = 0U;
	struct nvgpu_gr *gr = nvgpu_gr_get_cur_instance_ptr(g);
	struct nvgpu_gr_config *config = gr->config;
	u32 rop_id;

	if ((gpc_exception & (gr_gpc0_gpccs_gpc_exception_zrop0_pending_f() |
			gr_gpc0_gpccs_gpc_exception_zrop1_pending_f())) == 0U) {
		return;
	}

	gpc_offset = nvgpu_gr_gpc_offset(g, gpc);
	for (i = 0U; i < num_zrop_pending_masks; i++) {
		rop_id = i;
		if ((gpc_exception & zrop_pending_masks[i]) == 0U) {
			continue;
		}
		if (nvgpu_is_errata_present(g, NVGPU_ERRATA_3524791)) {
			rop_id = gr_config_get_gpc_rop_logical_id_map(
					config, gpc)[i];
			nvgpu_assert(rop_id != UINT_MAX);
		}
		reg_offset = nvgpu_safe_add_u32(gpc_offset,
				nvgpu_gr_rop_offset(g, rop_id));
		reg_offset = nvgpu_safe_add_u32(
				gr_gpc0_rop0_zrop_hww_esr_r(),
				reg_offset);
		hww_esr = nvgpu_readl(g, reg_offset);

		nvgpu_err(g,
			"gpc(%u) rop(%u) zrop_hww_esr(0x%08x)", gpc, rop_id,
			hww_esr);

		nvgpu_writel(g, reg_offset,
			gr_gpc0_rop0_zrop_hww_esr_reset_active_f() |
			gr_gpc0_rop0_zrop_hww_esr_en_enable_f());
	}

}

void ga10b_gr_intr_handle_gpc_rrh_hww(struct gk20a *g, u32 gpc,
		u32 gpc_exception)
{
	u32 gpc_offset = 0U;
	u32 reg_offset = 0U;
	u32 status = 0U;
	u32 rrh_pending_masks[] = {
		gr_gpc0_gpccs_gpc_exception_rrh0_pending_f(),
		gr_gpc0_gpccs_gpc_exception_rrh1_pending_f()
	};
	u32 num_rrh_pending_masks =
		sizeof(rrh_pending_masks)/sizeof(*rrh_pending_masks);
	u32 i = 0U;
	struct nvgpu_gr *gr = nvgpu_gr_get_cur_instance_ptr(g);
	struct nvgpu_gr_config *config = gr->config;
	u32 rop_id;

	if ((gpc_exception & (gr_gpc0_gpccs_gpc_exception_rrh0_pending_f() |
			gr_gpc0_gpccs_gpc_exception_rrh1_pending_f())) == 0U) {
		return;
	}

	gpc_offset = nvgpu_gr_gpc_offset(g, gpc);
	for (i = 0U; i < num_rrh_pending_masks; i++) {
		rop_id = i;
		if ((gpc_exception & rrh_pending_masks[i]) == 0U) {
			continue;
		}
		if (nvgpu_is_errata_present(g, NVGPU_ERRATA_3524791)) {
			rop_id = gr_config_get_gpc_rop_logical_id_map(
					config, gpc)[i];
			nvgpu_assert(rop_id != UINT_MAX);
		}
		reg_offset = nvgpu_safe_add_u32(gpc_offset,
				nvgpu_gr_rop_offset(g, rop_id));
		reg_offset = nvgpu_safe_add_u32(
				gr_gpc0_rop0_rrh_status_r(),
				reg_offset);
		status = nvgpu_readl(g, reg_offset);

		nvgpu_err(g, "gpc(%u) rop(%u) rrh exception status(0x%08x)",
				gpc, rop_id, status);
	}
}

void ga10b_gr_intr_enable_interrupts(struct gk20a *g, bool enable)
{
	u32 intr_notify_ctrl;

	/*
	 * The init value for the notify vector is retained and only
	 * the cpu, gsp enable fields are updated here.
	 */
	intr_notify_ctrl = nvgpu_readl(g, gr_intr_notify_ctrl_r());

	if (enable) {
		nvgpu_log(g, gpu_dbg_intr, "gr intr notify vector(%d)",
				gr_intr_notify_ctrl_vector_f(intr_notify_ctrl));
		/* Mask intr */
		nvgpu_writel(g, gr_intr_en_r(), 0U);
		/* Clear interrupt */
		nvgpu_writel(g, gr_intr_r(), U32_MAX);
		/* Enable notifying interrupt to cpu */
		intr_notify_ctrl |= gr_intr_notify_ctrl_cpu_enable_f();
		/* Disable notifying interrupt to gsp */
		intr_notify_ctrl &= ~gr_intr_notify_ctrl_gsp_enable_f();
		nvgpu_writel(g, gr_intr_notify_ctrl_r(), intr_notify_ctrl);
		/* Enable gr interrupts */
		nvgpu_writel(g, gr_intr_en_r(), g->ops.gr.intr.enable_mask(g));
	} else {
		/* Mask intr */
		nvgpu_writel(g, gr_intr_en_r(), 0U);
		/* Disable notifying interrupt to cpu */
		intr_notify_ctrl &= ~gr_intr_notify_ctrl_cpu_enable_f();
		/* Disable notifying interrupt to gsp */
		intr_notify_ctrl &= ~gr_intr_notify_ctrl_gsp_enable_f();
		nvgpu_writel(g, gr_intr_notify_ctrl_r(), intr_notify_ctrl);
		/* Clear intr */
		nvgpu_writel(g, gr_intr_r(), U32_MAX);
	}
}

int ga10b_gr_intr_retrigger(struct gk20a *g)
{
	nvgpu_writel(g, gr_intr_retrigger_r(),
			gr_intr_retrigger_trigger_true_f());

	return 0;
}

u32 ga10b_gr_intr_read_pending_interrupts(struct gk20a *g,
					struct nvgpu_gr_intr_info *intr_info)
{
	u32 gr_intr = nvgpu_readl(g, gr_intr_r());

	(void) memset(intr_info, 0, sizeof(struct nvgpu_gr_intr_info));

#ifdef CONFIG_NVGPU_NON_FUSA
	if ((gr_intr & gr_intr_notify_pending_f()) != 0U) {
		intr_info->notify = gr_intr_notify_pending_f();
	}

	if ((gr_intr & gr_intr_semaphore_pending_f()) != 0U) {
		intr_info->semaphore = gr_intr_semaphore_pending_f();
	}

	if ((gr_intr & gr_intr_buffer_notify_pending_f()) != 0U) {
		intr_info->buffer_notify = gr_intr_buffer_notify_pending_f();
	}

	if ((gr_intr & gr_intr_debug_method_pending_f()) != 0U) {
		intr_info->debug_method = gr_intr_debug_method_pending_f();
	}
#endif

	if ((gr_intr & gr_intr_illegal_notify_pending_f()) != 0U) {
		intr_info->illegal_notify = gr_intr_illegal_notify_pending_f();
	}

	if ((gr_intr & gr_intr_illegal_method_pending_f()) != 0U) {
		intr_info->illegal_method = gr_intr_illegal_method_pending_f();
	}

	if ((gr_intr & gr_intr_fecs_error_pending_f()) != 0U) {
		intr_info->fecs_error = gr_intr_fecs_error_pending_f();
	}

	if ((gr_intr & gr_intr_class_error_pending_f()) != 0U) {
		intr_info->class_error = gr_intr_class_error_pending_f();
	}

	/* this one happens if someone tries to hit a non-whitelisted
	 * register using set_falcon[4] */
	if ((gr_intr & gr_intr_firmware_method_pending_f()) != 0U) {
		intr_info->fw_method = gr_intr_firmware_method_pending_f();
	}

	if ((gr_intr & gr_intr_exception_pending_f()) != 0U) {
		intr_info->exception = gr_intr_exception_pending_f();
	}

	return gr_intr;
}
