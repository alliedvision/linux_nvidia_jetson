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

#include <nvgpu/gr/config.h>
#include <nvgpu/gr/gr.h>
#include <nvgpu/gr/gr_intr.h>
#include <nvgpu/gr/gr_utils.h>

#include "common/gr/gr_intr_priv.h"

#include "gr_intr_gm20b.h"

#include <nvgpu/hw/gm20b/hw_gr_gm20b.h>

static u32 get_sm_hww_warp_esr_report_mask(void)
{
	u32 mask = gr_gpcs_tpcs_sm_hww_warp_esr_report_mask_stack_error_report_f() |
		gr_gpcs_tpcs_sm_hww_warp_esr_report_mask_api_stack_error_report_f() |
		gr_gpcs_tpcs_sm_hww_warp_esr_report_mask_ret_empty_stack_error_report_f() |
		gr_gpcs_tpcs_sm_hww_warp_esr_report_mask_pc_wrap_report_f() |
		gr_gpcs_tpcs_sm_hww_warp_esr_report_mask_misaligned_pc_report_f() |
		gr_gpcs_tpcs_sm_hww_warp_esr_report_mask_pc_overflow_report_f() |
		gr_gpcs_tpcs_sm_hww_warp_esr_report_mask_misaligned_immc_addr_report_f() |
		gr_gpcs_tpcs_sm_hww_warp_esr_report_mask_misaligned_reg_report_f() |
		gr_gpcs_tpcs_sm_hww_warp_esr_report_mask_illegal_instr_encoding_report_f() |
		gr_gpcs_tpcs_sm_hww_warp_esr_report_mask_illegal_sph_instr_combo_report_f() |
		gr_gpcs_tpcs_sm_hww_warp_esr_report_mask_illegal_instr_param_report_f() |
		gr_gpcs_tpcs_sm_hww_warp_esr_report_mask_invalid_const_addr_report_f() |
		gr_gpcs_tpcs_sm_hww_warp_esr_report_mask_oor_reg_report_f() |
		gr_gpcs_tpcs_sm_hww_warp_esr_report_mask_oor_addr_report_f() |
		gr_gpcs_tpcs_sm_hww_warp_esr_report_mask_misaligned_addr_report_f() |
		gr_gpcs_tpcs_sm_hww_warp_esr_report_mask_invalid_addr_space_report_f() |
		gr_gpcs_tpcs_sm_hww_warp_esr_report_mask_illegal_instr_param2_report_f() |
		gr_gpcs_tpcs_sm_hww_warp_esr_report_mask_invalid_const_addr_ldc_report_f() |
		gr_gpcs_tpcs_sm_hww_warp_esr_report_mask_mmu_fault_report_f() |
		gr_gpcs_tpcs_sm_hww_warp_esr_report_mask_stack_overflow_report_f() |
		gr_gpcs_tpcs_sm_hww_warp_esr_report_mask_geometry_sm_error_report_f() |
		gr_gpcs_tpcs_sm_hww_warp_esr_report_mask_divergent_report_f();

	return mask;
}

static u32 get_sm_hww_global_esr_report_mask(void)
{	u32 mask = gr_gpcs_tpcs_sm_hww_global_esr_report_mask_sm_to_sm_fault_report_f() |
		gr_gpcs_tpcs_sm_hww_global_esr_report_mask_l1_error_report_f() |
		gr_gpcs_tpcs_sm_hww_global_esr_report_mask_multiple_warp_errors_report_f() |
		gr_gpcs_tpcs_sm_hww_global_esr_report_mask_physical_stack_overflow_error_report_f() |
		gr_gpcs_tpcs_sm_hww_global_esr_report_mask_bpt_int_report_f() |
		gr_gpcs_tpcs_sm_hww_global_esr_report_mask_bpt_pause_report_f() |
		gr_gpcs_tpcs_sm_hww_global_esr_report_mask_single_step_complete_report_f();

	return mask;
}

#define NVA297_SET_SHADER_EXCEPTIONS_ENABLE_FALSE	U32(0)

void gm20b_gr_intr_handle_class_error(struct gk20a *g, u32 chid,
				       struct nvgpu_gr_isr_data *isr_data)
{
	u32 gr_class_error;

	gr_class_error =
		gr_class_error_code_v(nvgpu_readl(g, gr_class_error_r()));

	nvgpu_err(g, "class error 0x%08x, offset 0x%08x,"
		"sub channel 0x%08x mme generated %d,"
		" mme pc 0x%08xdata high %d priv status %d"
		" unhandled intr 0x%08x for channel %u",
		isr_data->class_num, (isr_data->offset << 2),
		gr_trapped_addr_subch_v(isr_data->addr),
		gr_trapped_addr_mme_generated_v(isr_data->addr),
		gr_trapped_data_mme_pc_v(
			nvgpu_readl(g, gr_trapped_data_mme_r())),
		gr_trapped_addr_datahigh_v(isr_data->addr),
		gr_trapped_addr_priv_v(isr_data->addr),
		gr_class_error, chid);

	nvgpu_err(g, "trapped data low 0x%08x",
		nvgpu_readl(g, gr_trapped_data_lo_r()));
	if (gr_trapped_addr_datahigh_v(isr_data->addr) != 0U) {
		nvgpu_err(g, "trapped data high 0x%08x",
		nvgpu_readl(g, gr_trapped_data_hi_r()));
	}
}

int gm20b_gr_intr_handle_sw_method(struct gk20a *g, u32 addr,
					  u32 class_num, u32 offset, u32 data)
{
	(void)addr;

	nvgpu_log_fn(g, " ");

#ifdef CONFIG_NVGPU_NON_FUSA
	if (class_num == MAXWELL_COMPUTE_B) {
		switch (offset << 2) {
		case NVB1C0_SET_SHADER_EXCEPTIONS:
			g->ops.gr.intr.set_shader_exceptions(g, data);
			return 0;
		case NVB1C0_SET_RD_COALESCE:
			g->ops.gr.init.lg_coalesce(g, data);
			return 0;
		}
	}
#endif


#if defined(CONFIG_NVGPU_DEBUGGER) && defined(CONFIG_NVGPU_GRAPHICS)
	if (class_num == MAXWELL_B) {
		switch (offset << 2) {
		case NVB197_SET_SHADER_EXCEPTIONS:
			g->ops.gr.intr.set_shader_exceptions(g, data);
			return 0;
		case NVB197_SET_CIRCULAR_BUFFER_SIZE:
			g->ops.gr.set_circular_buffer_size(g, data);
			return 0;
		case NVB197_SET_ALPHA_CIRCULAR_BUFFER_SIZE:
			g->ops.gr.set_alpha_circular_buffer_size(g, data);
			return 0;
		case NVB197_SET_RD_COALESCE:
			g->ops.gr.init.lg_coalesce(g, data);
			return 0;
		}
	}
#endif

	return -EINVAL;
}

void gm20b_gr_intr_set_shader_exceptions(struct gk20a *g, u32 data)
{
	nvgpu_log_fn(g, " ");

	if (data == NVA297_SET_SHADER_EXCEPTIONS_ENABLE_FALSE) {
		nvgpu_writel(g,
			gr_gpcs_tpcs_sm_hww_warp_esr_report_mask_r(), 0);
		nvgpu_writel(g,
			gr_gpcs_tpcs_sm_hww_global_esr_report_mask_r(), 0);
	} else {
		/* setup sm warp esr report masks */
		nvgpu_writel(g, gr_gpcs_tpcs_sm_hww_warp_esr_report_mask_r(),
		 gr_gpcs_tpcs_sm_hww_warp_esr_report_mask_stack_error_report_f() |
		 gr_gpcs_tpcs_sm_hww_warp_esr_report_mask_api_stack_error_report_f() |
		 gr_gpcs_tpcs_sm_hww_warp_esr_report_mask_ret_empty_stack_error_report_f() |
		 gr_gpcs_tpcs_sm_hww_warp_esr_report_mask_pc_wrap_report_f() |
		 gr_gpcs_tpcs_sm_hww_warp_esr_report_mask_misaligned_pc_report_f() |
		 gr_gpcs_tpcs_sm_hww_warp_esr_report_mask_pc_overflow_report_f() |
		 gr_gpcs_tpcs_sm_hww_warp_esr_report_mask_misaligned_immc_addr_report_f() |
		 gr_gpcs_tpcs_sm_hww_warp_esr_report_mask_misaligned_reg_report_f() |
		 gr_gpcs_tpcs_sm_hww_warp_esr_report_mask_illegal_instr_encoding_report_f() |
		 gr_gpcs_tpcs_sm_hww_warp_esr_report_mask_illegal_sph_instr_combo_report_f() |
		 gr_gpcs_tpcs_sm_hww_warp_esr_report_mask_illegal_instr_param_report_f() |
		 gr_gpcs_tpcs_sm_hww_warp_esr_report_mask_invalid_const_addr_report_f() |
		 gr_gpcs_tpcs_sm_hww_warp_esr_report_mask_oor_reg_report_f() |
		 gr_gpcs_tpcs_sm_hww_warp_esr_report_mask_oor_addr_report_f() |
		 gr_gpcs_tpcs_sm_hww_warp_esr_report_mask_misaligned_addr_report_f() |
		 gr_gpcs_tpcs_sm_hww_warp_esr_report_mask_invalid_addr_space_report_f() |
		 gr_gpcs_tpcs_sm_hww_warp_esr_report_mask_illegal_instr_param2_report_f() |
		 gr_gpcs_tpcs_sm_hww_warp_esr_report_mask_invalid_const_addr_ldc_report_f() |
		 gr_gpcs_tpcs_sm_hww_warp_esr_report_mask_geometry_sm_error_report_f() |
		 gr_gpcs_tpcs_sm_hww_warp_esr_report_mask_divergent_report_f());

		/* setup sm global esr report mask */
		nvgpu_writel(g, gr_gpcs_tpcs_sm_hww_global_esr_report_mask_r(),
		 gr_gpcs_tpcs_sm_hww_global_esr_report_mask_sm_to_sm_fault_report_f() |
		 gr_gpcs_tpcs_sm_hww_global_esr_report_mask_l1_error_report_f() |
		 gr_gpcs_tpcs_sm_hww_global_esr_report_mask_multiple_warp_errors_report_f() |
		 gr_gpcs_tpcs_sm_hww_global_esr_report_mask_physical_stack_overflow_error_report_f() |
		 gr_gpcs_tpcs_sm_hww_global_esr_report_mask_bpt_int_report_f() |
		 gr_gpcs_tpcs_sm_hww_global_esr_report_mask_bpt_pause_report_f() |
		 gr_gpcs_tpcs_sm_hww_global_esr_report_mask_single_step_complete_report_f());
	}
}

void gm20b_gr_intr_handle_tex_exception(struct gk20a *g, u32 gpc, u32 tpc)
{
	u32 offset = nvgpu_safe_add_u32(
			nvgpu_gr_gpc_offset(g, gpc),
			nvgpu_gr_tpc_offset(g, tpc));
	u32 esr;

	nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gpu_dbg, " ");

	esr = nvgpu_readl(g,
			  nvgpu_safe_add_u32(
				gr_gpc0_tpc0_tex_m_hww_esr_r(), offset));
	nvgpu_log(g, gpu_dbg_intr | gpu_dbg_gpu_dbg, "0x%08x", esr);

	nvgpu_writel(g,
		     nvgpu_safe_add_u32(
			gr_gpc0_tpc0_tex_m_hww_esr_r(), offset), esr);
}

void gm20b_gr_intr_enable_hww_exceptions(struct gk20a *g)
{
	/* enable exceptions */
	nvgpu_writel(g, gr_fe_hww_esr_r(),
		     gr_fe_hww_esr_en_enable_f() |
		     gr_fe_hww_esr_reset_active_f());
	nvgpu_writel(g, gr_memfmt_hww_esr_r(),
		     gr_memfmt_hww_esr_en_enable_f() |
		     gr_memfmt_hww_esr_reset_active_f());
}

void gm20b_gr_intr_enable_exceptions(struct gk20a *g,
				     struct nvgpu_gr_config *gr_config,
				     bool enable)
{
	u32 reg_value = (enable) ? 0xFFFFFFFFU : 0U;

	(void)gr_config;
	nvgpu_writel(g, gr_exception_en_r(), reg_value);
	nvgpu_writel(g, gr_exception1_en_r(), reg_value);
	nvgpu_writel(g, gr_exception2_en_r(), reg_value);
}

void gm20b_gr_intr_enable_gpc_exceptions(struct gk20a *g,
					 struct nvgpu_gr_config *gr_config)
{
	u32 tpc_mask, tpc_mask_calc;

	nvgpu_writel(g, gr_gpcs_tpcs_tpccs_tpc_exception_en_r(),
			gr_gpcs_tpcs_tpccs_tpc_exception_en_tex_enabled_f() |
			gr_gpcs_tpcs_tpccs_tpc_exception_en_sm_enabled_f());

	tpc_mask_calc = (u32)BIT32(
			 nvgpu_gr_config_get_max_tpc_per_gpc_count(gr_config));
	tpc_mask = gr_gpcs_gpccs_gpc_exception_en_tpc_f(
				nvgpu_safe_sub_u32(tpc_mask_calc, 1U));

	nvgpu_writel(g, gr_gpcs_gpccs_gpc_exception_en_r(), tpc_mask);
}

void gm20b_gr_intr_set_hww_esr_report_mask(struct gk20a *g)
{
	u32 sm_hww_warp_esr_report_mask;
	u32 sm_hww_global_esr_report_mask;


	/*
	 * Perform a RMW to the warp, global ESR report mask registers.
	 * This is done in-order to retain the default values loaded from
	 * sw_ctx_load.
	 */
	sm_hww_warp_esr_report_mask = nvgpu_readl(g,
			gr_gpc0_tpc0_sm_hww_warp_esr_report_mask_r());
	sm_hww_global_esr_report_mask = nvgpu_readl(g,
			gr_gpc0_tpc0_sm_hww_global_esr_report_mask_r());

	/* clear hww */
	nvgpu_writel(g, gr_gpcs_tpcs_sm_hww_global_esr_report_mask_r(), U32_MAX);

	/* setup sm warp esr report masks */
	nvgpu_writel(g, gr_gpcs_tpcs_sm_hww_warp_esr_report_mask_r(),
		sm_hww_warp_esr_report_mask | get_sm_hww_warp_esr_report_mask());

	/* setup sm global esr report mask */
	nvgpu_writel(g, gr_gpcs_tpcs_sm_hww_global_esr_report_mask_r(),
		sm_hww_global_esr_report_mask | get_sm_hww_global_esr_report_mask());

	nvgpu_log_info(g,
		"configured (global, warp)_esr_report_mask(0x%x, 0x%x)",
		sm_hww_global_esr_report_mask | get_sm_hww_global_esr_report_mask(),
		sm_hww_warp_esr_report_mask | get_sm_hww_warp_esr_report_mask());
}

void gm20b_gr_intr_get_esr_sm_sel(struct gk20a *g, u32 gpc, u32 tpc,
				u32 *esr_sm_sel)
{
	(void)g;
	(void)gpc;
	(void)tpc;
	*esr_sm_sel = 1;
}

void gm20b_gr_intr_clear_sm_hww(struct gk20a *g, u32 gpc, u32 tpc, u32 sm,
			u32 global_esr)
{
	u32 offset = nvgpu_safe_add_u32(nvgpu_gr_gpc_offset(g, gpc),
					nvgpu_gr_tpc_offset(g, tpc));

	(void)sm;

	gk20a_writel(g, nvgpu_safe_add_u32(
				gr_gpc0_tpc0_sm_hww_global_esr_r(), offset),
			global_esr);

	/* clear the warp hww */
	gk20a_writel(g, nvgpu_safe_add_u32(
				gr_gpc0_tpc0_sm_hww_warp_esr_r(), offset),
			0);
}

static int gm20b_gr_intr_read_sm_error_state(struct gk20a *g,
		struct nvgpu_tsg *tsg, u32 offset, u32 sm_id)
{
	u32 hww_global_esr = gk20a_readl(g, nvgpu_safe_add_u32(
			gr_gpc0_tpc0_sm_hww_global_esr_r(), offset));
	u32 hww_warp_esr = gk20a_readl(g, nvgpu_safe_add_u32(
			gr_gpc0_tpc0_sm_hww_warp_esr_r(), offset));
	u64 hww_warp_esr_pc = (u64)(gk20a_readl(g, nvgpu_safe_add_u32(
			gr_gpc0_tpc0_sm_hww_warp_esr_pc_r(), offset)));
	u32 hww_global_esr_report_mask = gk20a_readl(g, nvgpu_safe_add_u32(
		       gr_gpc0_tpc0_sm_hww_global_esr_report_mask_r(), offset));
	u32 hww_warp_esr_report_mask = gk20a_readl(g, nvgpu_safe_add_u32(
			gr_gpc0_tpc0_sm_hww_warp_esr_report_mask_r(), offset));

	return nvgpu_tsg_store_sm_error_state(tsg, sm_id,
		hww_global_esr, hww_warp_esr,
		hww_warp_esr_pc, hww_global_esr_report_mask,
		hww_warp_esr_report_mask);
}

u32 gm20b_gr_intr_record_sm_error_state(struct gk20a *g, u32 gpc, u32 tpc, u32 sm,
				struct nvgpu_channel *fault_ch)
{
	u32 sm_id;
	u32 gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_GPC_STRIDE);
	u32 tpc_in_gpc_stride = nvgpu_get_litter_value(g,
					       GPU_LIT_TPC_IN_GPC_STRIDE);
	u32 offset;
	struct nvgpu_tsg *tsg = NULL;
	int err = 0;

	(void)sm;

	offset = nvgpu_safe_add_u32(
			nvgpu_safe_mult_u32(gpc_stride, gpc),
			nvgpu_safe_mult_u32(tpc_in_gpc_stride, tpc));

#ifdef CONFIG_NVGPU_DEBUGGER
	nvgpu_mutex_acquire(&g->dbg_sessions_lock);
#endif

	sm_id = gr_gpc0_tpc0_sm_cfg_sm_id_v(
			gk20a_readl(g, nvgpu_safe_add_u32(
				gr_gpc0_tpc0_sm_cfg_r(), offset)));

	if (fault_ch != NULL) {
		tsg = nvgpu_tsg_from_ch(fault_ch);
	}

	if (tsg == NULL) {
		nvgpu_err(g, "no valid tsg");
		goto record_fail;
	}

	err = gm20b_gr_intr_read_sm_error_state(g, tsg, offset, sm_id);
	if (err != 0) {
		nvgpu_err(g, "error writing sm_error_state");
	}

record_fail:
#ifdef CONFIG_NVGPU_DEBUGGER
	nvgpu_mutex_release(&g->dbg_sessions_lock);
#endif

	return sm_id;
}

u32 gm20b_gr_intr_get_sm_hww_global_esr(struct gk20a *g, u32 gpc, u32 tpc,
		u32 sm)
{
	u32 offset = nvgpu_safe_add_u32(nvgpu_gr_gpc_offset(g, gpc),
					nvgpu_gr_tpc_offset(g, tpc));

	u32 hww_global_esr = gk20a_readl(g, nvgpu_safe_add_u32(
				 gr_gpc0_tpc0_sm_hww_global_esr_r(), offset));

	(void)sm;
	return hww_global_esr;
}

u32 gm20b_gr_intr_get_sm_hww_warp_esr(struct gk20a *g, u32 gpc, u32 tpc, u32 sm)
{
	u32 offset = nvgpu_safe_add_u32(nvgpu_gr_gpc_offset(g, gpc),
					nvgpu_gr_tpc_offset(g, tpc));
	u32 hww_warp_esr = gk20a_readl(g, nvgpu_safe_add_u32(
			 gr_gpc0_tpc0_sm_hww_warp_esr_r(), offset));
	(void)sm;
	return hww_warp_esr;
}

u32 gm20b_gr_intr_get_sm_no_lock_down_hww_global_esr_mask(struct gk20a *g)
{
	/*
	 * These three interrupts don't require locking down the SM. They can
	 * be handled by usermode clients as they aren't fatal. Additionally,
	 * usermode clients may wish to allow some warps to execute while others
	 * are at breakpoints, as opposed to fatal errors where all warps should
	 * halt.
	 */
	u32 global_esr_mask =
		gr_gpc0_tpc0_sm_hww_global_esr_bpt_int_pending_f() |
		gr_gpc0_tpc0_sm_hww_global_esr_bpt_pause_pending_f() |
		gr_gpc0_tpc0_sm_hww_global_esr_single_step_complete_pending_f();

	(void)g;
	return global_esr_mask;
}

void gm20b_gr_intr_tpc_exception_sm_enable(struct gk20a *g)
{
	u32 tpc_exception_en = nvgpu_readl(g,
				gr_gpc0_tpc0_tpccs_tpc_exception_en_r());

	tpc_exception_en &=
			~gr_gpc0_tpc0_tpccs_tpc_exception_en_sm_enabled_f();
	tpc_exception_en |= gr_gpc0_tpc0_tpccs_tpc_exception_en_sm_enabled_f();
	nvgpu_writel(g,
		     gr_gpcs_tpcs_tpccs_tpc_exception_en_r(),
		     tpc_exception_en);
}

#ifdef CONFIG_NVGPU_DEBUGGER
u64 gm20b_gr_intr_tpc_enabled_exceptions(struct gk20a *g)
{
	u32 sm_id;
	u64 tpc_exception_en = 0;
	u32 sm_bit_in_tpc = 0U;
	u32 offset, regval, tpc_offset, gpc_offset;
	u32 gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_GPC_STRIDE);
	u32 tpc_in_gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_TPC_IN_GPC_STRIDE);
	u32 no_of_sm = g->ops.gr.init.get_no_of_sm(g);
	struct nvgpu_gr_config *config = nvgpu_gr_get_config_ptr(g);

	for (sm_id = 0; sm_id < no_of_sm; sm_id++) {
		struct nvgpu_sm_info *sm_info =
			nvgpu_gr_config_get_sm_info(config, sm_id);
		tpc_offset = nvgpu_safe_mult_u32(tpc_in_gpc_stride,
			nvgpu_gr_config_get_sm_info_tpc_index(sm_info));
		gpc_offset = nvgpu_safe_mult_u32(gpc_stride,
			nvgpu_gr_config_get_sm_info_gpc_index(sm_info));
		offset = nvgpu_safe_add_u32(tpc_offset, gpc_offset);

		regval = gk20a_readl(g,	nvgpu_safe_add_u32(
			      gr_gpc0_tpc0_tpccs_tpc_exception_en_r(), offset));
		/*
		 * Each bit represents corresponding enablement state, bit 0
		 * corrsponds to SM0.
		 */
		sm_bit_in_tpc =
			gr_gpc0_tpc0_tpccs_tpc_exception_en_sm_v(regval);
		tpc_exception_en |= (u64)sm_bit_in_tpc << sm_id;
	}

	return tpc_exception_en;
}
#endif
