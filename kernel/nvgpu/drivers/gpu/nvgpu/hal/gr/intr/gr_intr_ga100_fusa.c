/*
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

#include <nvgpu/gk20a.h>
#include <nvgpu/io.h>
#include <nvgpu/class.h>
#include <nvgpu/engines.h>
#include <nvgpu/nvgpu_err.h>

#include <nvgpu/gr/config.h>
#include <nvgpu/gr/gr.h>
#include <nvgpu/gr/gr_intr.h>

#include "common/gr/gr_intr_priv.h"
#include "hal/gr/intr/gr_intr_gm20b.h"
#include "hal/gr/intr/gr_intr_gp10b.h"
#include "hal/gr/intr/gr_intr_gv11b.h"
#include "gr_intr_ga100.h"

#include <nvgpu/hw/ga100/hw_gr_ga100.h>

u32 ga100_gr_intr_enable_mask(struct gk20a *g)
{
	u32 mask =
#ifdef CONFIG_NVGPU_NON_FUSA
	gr_intr_en_notify__prod_f() |
	gr_intr_en_semaphore__prod_f() |
	gr_intr_en_buffer_notify__prod_f() |
	gr_intr_en_debug_method__prod_f() |
#endif
	gr_intr_en_illegal_method__prod_f() |
	gr_intr_en_illegal_class__prod_f() |
	gr_intr_en_illegal_notify__prod_f() |
	gr_intr_en_firmware_method__prod_f() |
	gr_intr_en_fecs_error__prod_f() |
	gr_intr_en_class_error__prod_f() |
	gr_intr_en_exception__prod_f() |
	gr_intr_en_fe_debug_intr__prod_f();

	return mask;
}

u32 ga100_gr_intr_read_pending_interrupts(struct gk20a *g,
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

	if ((gr_intr & gr_intr_illegal_class_pending_f()) != 0U) {
		intr_info->illegal_class = gr_intr_illegal_class_pending_f();
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

int ga100_gr_intr_handle_sw_method(struct gk20a *g, u32 addr,
			u32 class_num, u32 offset, u32 data)
{
	nvgpu_log_fn(g, " ");

#ifdef CONFIG_NVGPU_HAL_NON_FUSA
	if (class_num == AMPERE_COMPUTE_A) {
		switch (offset << NVGPU_GA100_SW_METHOD_SHIFT) {
		case NVC6C0_SET_BES_CROP_DEBUG4:
			g->ops.gr.set_bes_crop_debug4(g, data);
			return 0;
		case NVC6C0_SET_SHADER_EXCEPTIONS:
			g->ops.gr.intr.set_shader_exceptions(g, data);
			return 0;
		case NVC6C0_SET_TEX_IN_DBG:
			gv11b_gr_intr_set_tex_in_dbg(g, data);
			return 0;
		case NVC6C0_SET_SKEDCHECK:
			gv11b_gr_intr_set_skedcheck(g, data);
			return 0;
		}
	}
#endif

#if defined(CONFIG_NVGPU_DEBUGGER) && defined(CONFIG_NVGPU_GRAPHICS)
	if (class_num == AMPERE_A) {
		switch (offset << NVGPU_GA100_SW_METHOD_SHIFT) {
		case NVC697_SET_SHADER_EXCEPTIONS:
			g->ops.gr.intr.set_shader_exceptions(g, data);
			return 0;
		case NVC697_SET_CIRCULAR_BUFFER_SIZE:
			g->ops.gr.set_circular_buffer_size(g, data);
			return 0;
		case NVC697_SET_ALPHA_CIRCULAR_BUFFER_SIZE:
			g->ops.gr.set_alpha_circular_buffer_size(g, data);
			return 0;
		}
	}
#endif
	return -EINVAL;
}

bool ga100_gr_intr_handle_exceptions(struct gk20a *g, bool *is_gpc_exception)
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
		gpc_reset |=
			gm20b_gr_intr_check_gr_ssync_exception(g, exception);
	}
	gpc_reset |= gm20b_gr_intr_check_gr_mme_exception(g, exception);
	gpc_reset |= gm20b_gr_intr_check_gr_sked_exception(g, exception);

	/* check if a gpc exception has occurred */
	if ((exception & gr_exception_gpc_m()) != 0U) {
		*is_gpc_exception = true;
	}

	return (gpc_reset != 0U)? true: false;
}


void ga100_gr_intr_enable_exceptions(struct gk20a *g,
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
		  gr_exception_en_gpc_enabled_f();

	nvgpu_log(g, gpu_dbg_info, "gr_exception_en 0x%08x", reg_val);

	nvgpu_writel(g, gr_exception_en_r(), reg_val);
}

void ga100_gr_intr_enable_gpc_exceptions(struct gk20a *g,
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

	nvgpu_writel(g, gr_gpcs_gpccs_gpc_exception_en_r(),
		(tpc_mask | gr_gpcs_gpccs_gpc_exception_en_gcc_enabled_f() |
			    gr_gpcs_gpccs_gpc_exception_en_gpccs_enabled_f() |
			    gr_gpcs_gpccs_gpc_exception_en_gpcmmu0_enabled_f()));
}
