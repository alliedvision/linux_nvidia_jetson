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
#include <nvgpu/mc.h>
#include <nvgpu/cic_mon.h>
#include <nvgpu/io.h>
#include <nvgpu/class.h>
#include <nvgpu/static_analysis.h>
#include <nvgpu/nvgpu_err.h>
#include <nvgpu/string.h>

#include <nvgpu/gr/config.h>
#include <nvgpu/gr/gr.h>
#include <nvgpu/gr/gr_intr.h>
#include <nvgpu/gr/gr_utils.h>

#include "common/gr/gr_intr_priv.h"

#include "gr_intr_gm20b.h"

#include <nvgpu/hw/gm20b/hw_gr_gm20b.h>

#define NVA297_SET_SHADER_EXCEPTIONS_ENABLE_FALSE	U32(0)

void gm20b_gr_intr_clear_pending_interrupts(struct gk20a *g, u32 gr_intr)
{
	nvgpu_writel(g, gr_intr_r(), gr_intr);
}

u32 gm20b_gr_intr_read_pending_interrupts(struct gk20a *g,
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

u32 gm20b_gr_intr_check_gr_ssync_exception(struct gk20a *g, u32 exception)
{
	u32 reset_gpc = 0U;

	if ((exception & gr_exception_ssync_m()) != 0U) {
		u32 ssync_esr = 0;

		if (g->ops.gr.intr.handle_ssync_hww != NULL) {
			g->ops.gr.intr.handle_ssync_hww(g, &ssync_esr);
			reset_gpc = 1U;
		}
		nvgpu_report_err_to_sdl(g, NVGPU_ERR_MODULE_PGRAPH,
				GPU_PGRAPH_SSYNC_EXCEPTION);
		nvgpu_err (g, "ssync exception");
	}
	return reset_gpc;
}

u32 gm20b_gr_intr_check_gr_mme_exception(struct gk20a *g, u32 exception)
{
	if ((exception & gr_exception_mme_m()) != 0U) {
		u32 mme = nvgpu_readl(g, gr_mme_hww_esr_r());
		u32 info = nvgpu_readl(g, gr_mme_hww_esr_info_r());

		nvgpu_report_err_to_sdl(g, NVGPU_ERR_MODULE_PGRAPH,
				GPU_PGRAPH_MME_EXCEPTION);
		nvgpu_err(g, "mme exception: esr 0x%08x info:0x%08x",
				mme, info);
#ifdef CONFIG_NVGPU_DGPU
		if (g->ops.gr.intr.log_mme_exception != NULL) {
			g->ops.gr.intr.log_mme_exception(g);
		}
#endif
		nvgpu_writel(g, gr_mme_hww_esr_r(),
			gr_mme_hww_esr_reset_active_f());
		return 1U;
	}
	return 0U;
}

u32 gm20b_gr_intr_check_gr_sked_exception(struct gk20a *g, u32 exception)
{
	if ((exception & gr_exception_sked_m()) != 0U) {
		u32 sked = nvgpu_readl(g, gr_sked_hww_esr_r());

		nvgpu_report_err_to_sdl(g, NVGPU_ERR_MODULE_PGRAPH,
				GPU_PGRAPH_SKED_EXCEPTION);
		nvgpu_err(g, "sked exception: esr 0x%08x", sked);
		nvgpu_writel(g, gr_sked_hww_esr_r(),
			gr_sked_hww_esr_reset_active_f());
		return 1U;
	}
	return 0U;
}

static u32 gr_gm20b_intr_check_gr_be_crop_exception(struct gk20a *g,
							u32 exception)
{
	if ((exception & gr_pri_be0_becs_be_exception_crop_m()) != 0U) {
		u32 crop = nvgpu_readl(g, gr_crop_hww_esr_r());

		nvgpu_report_err_to_sdl(g, NVGPU_ERR_MODULE_PGRAPH,
				GPU_PGRAPH_BE_CROP_EXCEPTION);
		nvgpu_err(g, "BE exception: crop exception: esr 0x%08x", crop);
		nvgpu_writel(g, gr_crop_hww_esr_r(),
			gr_crop_hww_esr_reset_active_f());
		return 1U;
	}
	return 0U;
}

static u32 gr_gm20b_intr_check_gr_be_zrop_exception(struct gk20a *g,
							u32 exception)
{
	if ((exception & gr_pri_be0_becs_be_exception_zrop_m()) != 0U) {
		u32 zrop = nvgpu_readl(g, gr_zrop_hww_esr_r());

		nvgpu_report_err_to_sdl(g, NVGPU_ERR_MODULE_PGRAPH,
				GPU_PGRAPH_BE_ZROP_EXCEPTION);
		nvgpu_err(g, "BE exception: zrop exception: esr 0x%08x", zrop);
		nvgpu_writel(g, gr_zrop_hww_esr_r(),
			gr_zrop_hww_esr_reset_active_f());
		return 1U;
	}
	return 0U;
}

u32 gm20b_gr_intr_check_gr_fe_exception(struct gk20a *g, u32 exception)
{
	if ((exception & gr_exception_fe_m()) != 0U) {
		u32 fe = nvgpu_readl(g, gr_fe_hww_esr_r());
		u32 info = nvgpu_readl(g, gr_fe_hww_esr_info_r());

		nvgpu_report_err_to_sdl(g, NVGPU_ERR_MODULE_PGRAPH,
				GPU_PGRAPH_FE_EXCEPTION);
		nvgpu_err(g, "fe exception: esr 0x%08x, info 0x%08x",
				fe, info);
		nvgpu_writel(g, gr_fe_hww_esr_r(),
			gr_fe_hww_esr_reset_active_f());
		return 1U;
	}
	return 0U;
}

u32 gm20b_gr_intr_check_gr_memfmt_exception(struct gk20a *g, u32 exception)
{
	if ((exception & gr_exception_memfmt_m()) != 0U) {
		u32 memfmt = nvgpu_readl(g, gr_memfmt_hww_esr_r());

		nvgpu_report_err_to_sdl(g, NVGPU_ERR_MODULE_PGRAPH,
				GPU_PGRAPH_MEMFMT_EXCEPTION);
		nvgpu_err(g, "memfmt exception: esr %08x", memfmt);
		nvgpu_writel(g, gr_memfmt_hww_esr_r(),
				gr_memfmt_hww_esr_reset_active_f());
		return 1U;
	}
	return 0U;
}

u32 gm20b_gr_intr_check_gr_pd_exception(struct gk20a *g, u32 exception)
{
	if ((exception & gr_exception_pd_m()) != 0U) {
		u32 pd = nvgpu_readl(g, gr_pd_hww_esr_r());

		nvgpu_report_err_to_sdl(g, NVGPU_ERR_MODULE_PGRAPH,
				GPU_PGRAPH_PD_EXCEPTION);
		nvgpu_err(g, "pd exception: esr 0x%08x", pd);
		nvgpu_writel(g, gr_pd_hww_esr_r(),
				gr_pd_hww_esr_reset_active_f());
		return 1U;
	}
	return 0U;
}

u32 gm20b_gr_intr_check_gr_scc_exception(struct gk20a *g, u32 exception)
{
	if ((exception & gr_exception_scc_m()) != 0U) {
		u32 scc = nvgpu_readl(g, gr_scc_hww_esr_r());

		nvgpu_report_err_to_sdl(g, NVGPU_ERR_MODULE_PGRAPH,
				GPU_PGRAPH_SCC_EXCEPTION);
		nvgpu_err(g, "scc exception: esr 0x%08x", scc);
		nvgpu_writel(g, gr_scc_hww_esr_r(),
				gr_scc_hww_esr_reset_active_f());
		return 1U;
	}
	return 0U;
}

u32 gm20b_gr_intr_check_gr_ds_exception(struct gk20a *g, u32 exception)
{
	if ((exception & gr_exception_ds_m()) != 0U) {
		u32 ds = nvgpu_readl(g, gr_ds_hww_esr_r());

		nvgpu_report_err_to_sdl(g, NVGPU_ERR_MODULE_PGRAPH,
				GPU_PGRAPH_DS_EXCEPTION);
		nvgpu_err(g, "ds exception: esr: 0x%08x", ds);
		nvgpu_writel(g, gr_ds_hww_esr_r(),
				 gr_ds_hww_esr_reset_task_f());
		return 1U;
	}
	return 0U;
}

bool gm20b_gr_intr_handle_exceptions(struct gk20a *g, bool *is_gpc_exception)
{
	u32 gpc_reset = 0U;
	u32 exception = nvgpu_readl(g, gr_exception_r());

	nvgpu_log(g, gpu_dbg_intr | gpu_dbg_gpu_dbg,
				"exception %08x\n", exception);

	gpc_reset = gm20b_gr_intr_check_gr_fe_exception(g, exception);
	gpc_reset |= gm20b_gr_intr_check_gr_memfmt_exception(g, exception);
	gpc_reset |= gm20b_gr_intr_check_gr_pd_exception(g, exception);
	gpc_reset |= gm20b_gr_intr_check_gr_scc_exception(g, exception);
	gpc_reset |= gm20b_gr_intr_check_gr_ds_exception(g, exception);
	gpc_reset |= gm20b_gr_intr_check_gr_ssync_exception(g, exception);
	gpc_reset |= gm20b_gr_intr_check_gr_mme_exception(g, exception);
	gpc_reset |= gm20b_gr_intr_check_gr_sked_exception(g, exception);
	gpc_reset |= gr_gm20b_intr_check_gr_be_crop_exception(g, exception);
	gpc_reset |= gr_gm20b_intr_check_gr_be_zrop_exception(g, exception);

	/* check if a gpc exception has occurred */
	if ((exception & gr_exception_gpc_m()) != 0U) {
		*is_gpc_exception = true;
	}

	return (gpc_reset != 0U)? true: false;
}

u32 gm20b_gr_intr_read_gpc_tpc_exception(u32 gpc_exception)
{
	return gr_gpc0_gpccs_gpc_exception_tpc_v(gpc_exception);
}

u32 gm20b_gr_intr_read_gpc_exception(struct gk20a *g, u32 gpc)
{
	u32 gpc_offset = nvgpu_gr_gpc_offset(g, gpc);

	return nvgpu_readl(g,
			nvgpu_safe_add_u32(
				gr_gpc0_gpccs_gpc_exception_r(),
				gpc_offset));
}

u32 gm20b_gr_intr_read_exception1(struct gk20a *g)
{
	return nvgpu_readl(g, gr_exception1_r());
}

void gm20b_gr_intr_get_trapped_method_info(struct gk20a *g,
				    struct nvgpu_gr_isr_data *isr_data)
{
	u32 obj_table;

	isr_data->addr = nvgpu_readl(g, gr_trapped_addr_r());
	isr_data->data_lo = nvgpu_readl(g, gr_trapped_data_lo_r());
	isr_data->data_hi = nvgpu_readl(g, gr_trapped_data_hi_r());
	isr_data->curr_ctx = nvgpu_readl(g, gr_fecs_current_ctx_r());
	isr_data->offset = gr_trapped_addr_mthd_v(isr_data->addr);
	isr_data->sub_chan = gr_trapped_addr_subch_v(isr_data->addr);
	obj_table = (isr_data->sub_chan < 4U) ? nvgpu_readl(g,
		gr_fe_object_table_r(isr_data->sub_chan)) : 0U;
	isr_data->class_num = gr_fe_object_table_nvclass_v(obj_table);
}

u32 gm20b_gr_intr_get_tpc_exception(struct gk20a *g, u32 offset,
				    struct nvgpu_gr_tpc_exception *pending_tpc)
{
	u32 tpc_exception = nvgpu_readl(g, nvgpu_safe_add_u32(
				gr_gpc0_tpc0_tpccs_tpc_exception_r(), offset));

	(void) memset(pending_tpc, 0, sizeof(struct nvgpu_gr_tpc_exception));

	if (gr_gpc0_tpc0_tpccs_tpc_exception_tex_v(tpc_exception) ==
		gr_gpc0_tpc0_tpccs_tpc_exception_tex_pending_v()) {
			pending_tpc->tex_exception = true;
	}

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

void gm20b_gr_intr_enable_interrupts(struct gk20a *g, bool enable)
{
	if (enable) {
		nvgpu_writel(g, gr_intr_r(), 0xFFFFFFFFU);
		nvgpu_writel(g, gr_intr_en_r(), 0xFFFFFFFFU);
	} else {
		nvgpu_writel(g, gr_intr_r(), 0);
		nvgpu_writel(g, gr_intr_en_r(), 0);
	}

}

u32 gm20b_gr_intr_nonstall_isr(struct gk20a *g)
{
	u32 nonstall_ops = 0;
	u32 gr_intr = nvgpu_readl(g, gr_intr_nonstall_r());

	nvgpu_log(g, gpu_dbg_intr, "pgraph nonstall intr %08x", gr_intr);

	if ((gr_intr & gr_intr_nonstall_trap_pending_f()) != 0U) {
		/* Clear the interrupt */
		nvgpu_writel(g, gr_intr_nonstall_r(),
			gr_intr_nonstall_trap_pending_f());
		nonstall_ops |= (NVGPU_CIC_NONSTALL_OPS_WAKEUP_SEMAPHORE |
			NVGPU_CIC_NONSTALL_OPS_POST_EVENTS);
	}
	return nonstall_ops;
}

#ifdef CONFIG_NVGPU_DEBUGGER
void gm20b_gr_intr_tpc_exception_sm_disable(struct gk20a *g, u32 offset)
{
	u32 tpc_exception_en = nvgpu_readl(g, nvgpu_safe_add_u32(
				gr_gpc0_tpc0_tpccs_tpc_exception_en_r(),
				offset));

	tpc_exception_en &=
			~gr_gpc0_tpc0_tpccs_tpc_exception_en_sm_enabled_f();
	nvgpu_writel(g, nvgpu_safe_add_u32(
		     gr_gpc0_tpc0_tpccs_tpc_exception_en_r(), offset),
		     tpc_exception_en);
}
#endif
