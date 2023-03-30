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
#include <nvgpu/errata.h>
#include <nvgpu/channel.h>
#include <nvgpu/static_analysis.h>

#include <nvgpu/gr/config.h>
#include <nvgpu/gr/gr.h>
#include <nvgpu/gr/ctx.h>
#include <nvgpu/gr/gr_falcon.h>
#include <nvgpu/gr/gr_intr.h>
#include <nvgpu/gr/gr_utils.h>

#include "gr_intr_gp10b.h"

#include <nvgpu/hw/gp10b/hw_gr_gp10b.h>

int gp10b_gr_intr_handle_sw_method(struct gk20a *g, u32 addr,
				     u32 class_num, u32 offset, u32 data)
{
	(void)addr;

	nvgpu_log_fn(g, " ");

#ifdef CONFIG_NVGPU_NON_FUSA
	if (class_num == PASCAL_COMPUTE_A) {
		switch (offset << 2) {
		case NVC0C0_SET_SHADER_EXCEPTIONS:
			g->ops.gr.intr.set_shader_exceptions(g, data);
			return 0;
		case NVC0C0_SET_RD_COALESCE:
			g->ops.gr.init.lg_coalesce(g, data);
			return 0;
		}
	}
#endif


#if defined(CONFIG_NVGPU_DEBUGGER) && defined(CONFIG_NVGPU_GRAPHICS)
	if (class_num == PASCAL_A) {
		switch (offset << 2) {
		case NVC097_SET_SHADER_EXCEPTIONS:
			g->ops.gr.intr.set_shader_exceptions(g, data);
			return 0;
		case NVC097_SET_CIRCULAR_BUFFER_SIZE:
			g->ops.gr.set_circular_buffer_size(g, data);
			return 0;
		case NVC097_SET_ALPHA_CIRCULAR_BUFFER_SIZE:
			g->ops.gr.set_alpha_circular_buffer_size(g, data);
			return 0;
		case NVC097_SET_GO_IDLE_TIMEOUT:
			gp10b_gr_intr_set_go_idle_timeout(g, data);
			return 0;
		case NVC097_SET_COALESCE_BUFFER_SIZE:
			gp10b_gr_intr_set_coalesce_buffer_size(g, data);
			return 0;
		case NVC097_SET_RD_COALESCE:
			g->ops.gr.init.lg_coalesce(g, data);
			return 0;
		case NVC097_SET_BES_CROP_DEBUG3:
			g->ops.gr.set_bes_crop_debug3(g, data);
			return 0;
		case NVC097_SET_BES_CROP_DEBUG4:
			g->ops.gr.set_bes_crop_debug4(g, data);
			return 0;
		}
	}
#endif

	return -EINVAL;
}

static void gr_gp10b_sm_lrf_ecc_overcount_errata(bool single_err,
						u32 sed_status,
						u32 ded_status,
						u32 *count_to_adjust,
						u32 opposite_count)
{
	u32 over_count = 0;

	sed_status >>= gr_pri_gpc0_tpc0_sm_lrf_ecc_status_single_err_detected_qrfdp0_b();
	ded_status >>= gr_pri_gpc0_tpc0_sm_lrf_ecc_status_double_err_detected_qrfdp0_b();

	/* One overcount for each partition on which a SBE occurred but not a
	   DBE (or vice-versa) */
	if (single_err) {
		over_count = (u32)hweight32(sed_status & ~ded_status);
	} else {
		over_count = (u32)hweight32(ded_status & ~sed_status);
	}

	/* If both a SBE and a DBE occur on the same partition, then we have an
	   overcount for the subpartition if the opposite error counts are
	   zero. */
	if (((sed_status & ded_status) != 0U) && (opposite_count == 0U)) {
		over_count = nvgpu_safe_add_u32(over_count,
				(u32)hweight32(sed_status & ded_status));
	}

	if (*count_to_adjust > over_count) {
		*count_to_adjust = nvgpu_safe_sub_u32(
					*count_to_adjust, over_count);
	} else {
		*count_to_adjust = 0;
	}
}

int gp10b_gr_intr_handle_sm_exception(struct gk20a *g,
			u32 gpc, u32 tpc, u32 sm,
			bool *post_event, struct nvgpu_channel *fault_ch,
			u32 *hww_global_esr)
{
	int ret = 0;
	u32 offset = nvgpu_safe_add_u32(nvgpu_gr_gpc_offset(g, gpc),
					  nvgpu_gr_tpc_offset(g, tpc));
	u32 lrf_ecc_status, lrf_ecc_sed_status, lrf_ecc_ded_status;
	u32 lrf_single_count_delta, lrf_double_count_delta;
	u32 shm_ecc_status;

	ret = nvgpu_gr_intr_handle_sm_exception(g,
		gpc, tpc, sm, post_event, fault_ch, hww_global_esr);

	/* Check for LRF ECC errors. */
        lrf_ecc_status = nvgpu_readl(g,
				nvgpu_safe_add_u32(
					gr_pri_gpc0_tpc0_sm_lrf_ecc_status_r(),
					offset));
	lrf_ecc_sed_status =
		lrf_ecc_status &
		(gr_pri_gpc0_tpc0_sm_lrf_ecc_status_single_err_detected_qrfdp0_pending_f() |
		 gr_pri_gpc0_tpc0_sm_lrf_ecc_status_single_err_detected_qrfdp1_pending_f() |
		 gr_pri_gpc0_tpc0_sm_lrf_ecc_status_single_err_detected_qrfdp2_pending_f() |
		 gr_pri_gpc0_tpc0_sm_lrf_ecc_status_single_err_detected_qrfdp3_pending_f());
	lrf_ecc_ded_status =
		lrf_ecc_status &
		(gr_pri_gpc0_tpc0_sm_lrf_ecc_status_double_err_detected_qrfdp0_pending_f() |
		 gr_pri_gpc0_tpc0_sm_lrf_ecc_status_double_err_detected_qrfdp1_pending_f() |
		 gr_pri_gpc0_tpc0_sm_lrf_ecc_status_double_err_detected_qrfdp2_pending_f() |
		 gr_pri_gpc0_tpc0_sm_lrf_ecc_status_double_err_detected_qrfdp3_pending_f());
	lrf_single_count_delta =
		nvgpu_readl(g, nvgpu_safe_add_u32(
			    gr_pri_gpc0_tpc0_sm_lrf_ecc_single_err_count_r(),
			    offset));
	lrf_double_count_delta =
		nvgpu_readl(g, nvgpu_safe_add_u32(
			    gr_pri_gpc0_tpc0_sm_lrf_ecc_double_err_count_r(),
			    offset));
	nvgpu_writel(g, nvgpu_safe_add_u32(
		gr_pri_gpc0_tpc0_sm_lrf_ecc_single_err_count_r(), offset), 0);
	nvgpu_writel(g, nvgpu_safe_add_u32(
		gr_pri_gpc0_tpc0_sm_lrf_ecc_double_err_count_r(), offset), 0);
	if (lrf_ecc_sed_status != 0U) {
		nvgpu_log(g, gpu_dbg_fn | gpu_dbg_intr,
			"Single bit error detected in SM LRF!");

		if (nvgpu_is_errata_present(g,
				NVGPU_ERRATA_LRF_ECC_OVERCOUNT)) {
			gr_gp10b_sm_lrf_ecc_overcount_errata(true,
							lrf_ecc_sed_status,
							lrf_ecc_ded_status,
							&lrf_single_count_delta,
							lrf_double_count_delta);
		}
		g->ecc.gr.sm_lrf_ecc_single_err_count[gpc][tpc].counter =
			   nvgpu_safe_add_u32(
				g->ecc.gr.sm_lrf_ecc_single_err_count[gpc][tpc].counter,
				lrf_single_count_delta);
	}
	if (lrf_ecc_ded_status != 0U) {
		nvgpu_log(g, gpu_dbg_fn | gpu_dbg_intr,
			"Double bit error detected in SM LRF!");

		if (nvgpu_is_errata_present(g,
				NVGPU_ERRATA_LRF_ECC_OVERCOUNT)) {
			gr_gp10b_sm_lrf_ecc_overcount_errata(false,
							lrf_ecc_sed_status,
							lrf_ecc_ded_status,
							&lrf_double_count_delta,
							lrf_single_count_delta);
		}
		g->ecc.gr.sm_lrf_ecc_double_err_count[gpc][tpc].counter =
			   nvgpu_safe_add_u32(
				g->ecc.gr.sm_lrf_ecc_double_err_count[gpc][tpc].counter,
				lrf_double_count_delta);
	}
	nvgpu_writel(g, nvgpu_safe_add_u32(
			gr_pri_gpc0_tpc0_sm_lrf_ecc_status_r(), offset),
			lrf_ecc_status);

	/* Check for SHM ECC errors. */
	shm_ecc_status = nvgpu_readl(g, nvgpu_safe_add_u32(
				     gr_pri_gpc0_tpc0_sm_shm_ecc_status_r(),
				     offset));
	if ((shm_ecc_status &
		gr_pri_gpc0_tpc0_sm_shm_ecc_status_single_err_corrected_shm0_pending_f()) != 0U ||
		(shm_ecc_status &
		gr_pri_gpc0_tpc0_sm_shm_ecc_status_single_err_corrected_shm1_pending_f()) != 0U ||
		(shm_ecc_status &
		gr_pri_gpc0_tpc0_sm_shm_ecc_status_single_err_detected_shm0_pending_f()) != 0U ||
		(shm_ecc_status &
		gr_pri_gpc0_tpc0_sm_shm_ecc_status_single_err_detected_shm1_pending_f()) != 0U ) {
		u32 ecc_stats_reg_val;

		nvgpu_log(g, gpu_dbg_fn | gpu_dbg_intr,
			"Single bit error detected in SM SHM!");

		ecc_stats_reg_val =
			nvgpu_readl(g, nvgpu_safe_add_u32(
				    gr_pri_gpc0_tpc0_sm_shm_ecc_err_count_r(),
				    offset));
		g->ecc.gr.sm_shm_ecc_sec_count[gpc][tpc].counter =
		   nvgpu_safe_add_u32(
			g->ecc.gr.sm_shm_ecc_sec_count[gpc][tpc].counter,
			gr_pri_gpc0_tpc0_sm_shm_ecc_err_count_single_corrected_v(ecc_stats_reg_val));
		g->ecc.gr.sm_shm_ecc_sed_count[gpc][tpc].counter =
		   nvgpu_safe_add_u32(
			g->ecc.gr.sm_shm_ecc_sed_count[gpc][tpc].counter,
			gr_pri_gpc0_tpc0_sm_shm_ecc_err_count_single_detected_v(ecc_stats_reg_val));
		ecc_stats_reg_val &= ~(gr_pri_gpc0_tpc0_sm_shm_ecc_err_count_single_corrected_m() |
					gr_pri_gpc0_tpc0_sm_shm_ecc_err_count_single_detected_m());
		nvgpu_writel(g, nvgpu_safe_add_u32(
			     gr_pri_gpc0_tpc0_sm_shm_ecc_err_count_r(),
			     offset), ecc_stats_reg_val);
	}
	if ((shm_ecc_status &
		gr_pri_gpc0_tpc0_sm_shm_ecc_status_double_err_detected_shm0_pending_f()) != 0U ||
		(shm_ecc_status &
		gr_pri_gpc0_tpc0_sm_shm_ecc_status_double_err_detected_shm1_pending_f()) != 0U) {
		u32 ecc_stats_reg_val;

		nvgpu_log(g, gpu_dbg_fn | gpu_dbg_intr,
			"Double bit error detected in SM SHM!");

		ecc_stats_reg_val =
			nvgpu_readl(g, nvgpu_safe_add_u32(
				    gr_pri_gpc0_tpc0_sm_shm_ecc_err_count_r(),
				    offset));
		g->ecc.gr.sm_shm_ecc_ded_count[gpc][tpc].counter =
		   nvgpu_safe_add_u32(
			g->ecc.gr.sm_shm_ecc_ded_count[gpc][tpc].counter,
			gr_pri_gpc0_tpc0_sm_shm_ecc_err_count_double_detected_v(ecc_stats_reg_val));
		ecc_stats_reg_val &= ~(gr_pri_gpc0_tpc0_sm_shm_ecc_err_count_double_detected_m());
		nvgpu_writel(g, nvgpu_safe_add_u32(
			     gr_pri_gpc0_tpc0_sm_shm_ecc_err_count_r(),
			     offset), ecc_stats_reg_val);
	}
	nvgpu_writel(g, nvgpu_safe_add_u32(
				gr_pri_gpc0_tpc0_sm_shm_ecc_status_r(),
				offset), shm_ecc_status);

	return ret;
}

void gp10b_gr_intr_handle_tex_exception(struct gk20a *g, u32 gpc, u32 tpc)
{
	u32 offset = nvgpu_safe_add_u32(nvgpu_gr_gpc_offset(g, gpc),
					nvgpu_gr_tpc_offset(g, tpc));
	u32 esr;
	u32 ecc_stats_reg_val;

	nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gpu_dbg, " ");

	esr = nvgpu_readl(g, nvgpu_safe_add_u32(
			 gr_gpc0_tpc0_tex_m_hww_esr_r(), offset));
	nvgpu_log(g, gpu_dbg_intr | gpu_dbg_gpu_dbg, "0x%08x", esr);

	if ((esr & gr_gpc0_tpc0_tex_m_hww_esr_ecc_sec_pending_f()) != 0U) {
		nvgpu_log(g, gpu_dbg_fn | gpu_dbg_intr,
			"Single bit error detected in TEX!");

		/* Pipe 0 counters */
		nvgpu_writel(g, nvgpu_safe_add_u32(
			gr_pri_gpc0_tpc0_tex_m_routing_r(), offset),
			gr_pri_gpc0_tpc0_tex_m_routing_sel_pipe0_f());

		ecc_stats_reg_val = nvgpu_readl(g, nvgpu_safe_add_u32(
			gr_pri_gpc0_tpc0_tex_m_ecc_cnt_total_r(), offset));
		g->ecc.gr.tex_ecc_total_sec_pipe0_count[gpc][tpc].counter =
		   nvgpu_safe_add_u32(
			g->ecc.gr.tex_ecc_total_sec_pipe0_count[gpc][tpc].counter,
			gr_pri_gpc0_tpc0_tex_m_ecc_cnt_total_sec_v(
							ecc_stats_reg_val));
		ecc_stats_reg_val &=
			~gr_pri_gpc0_tpc0_tex_m_ecc_cnt_total_sec_m();
		nvgpu_writel(g, nvgpu_safe_add_u32(
			gr_pri_gpc0_tpc0_tex_m_ecc_cnt_total_r(), offset),
			ecc_stats_reg_val);

		ecc_stats_reg_val = nvgpu_readl(g, nvgpu_safe_add_u32(
			gr_pri_gpc0_tpc0_tex_m_ecc_cnt_unique_r(), offset));
		g->ecc.gr.tex_unique_ecc_sec_pipe0_count[gpc][tpc].counter =
		   nvgpu_safe_add_u32(
			g->ecc.gr.tex_unique_ecc_sec_pipe0_count[gpc][tpc].counter,
			gr_pri_gpc0_tpc0_tex_m_ecc_cnt_unique_sec_v(
							ecc_stats_reg_val));
		ecc_stats_reg_val &=
			~gr_pri_gpc0_tpc0_tex_m_ecc_cnt_unique_sec_m();
		nvgpu_writel(g, nvgpu_safe_add_u32(
			gr_pri_gpc0_tpc0_tex_m_ecc_cnt_unique_r(), offset),
			ecc_stats_reg_val);

		/* Pipe 1 counters */
		nvgpu_writel(g, nvgpu_safe_add_u32(
			gr_pri_gpc0_tpc0_tex_m_routing_r(), offset),
			gr_pri_gpc0_tpc0_tex_m_routing_sel_pipe1_f());

		ecc_stats_reg_val = nvgpu_readl(g, nvgpu_safe_add_u32(
			gr_pri_gpc0_tpc0_tex_m_ecc_cnt_total_r(), offset));
		g->ecc.gr.tex_ecc_total_sec_pipe1_count[gpc][tpc].counter =
		   nvgpu_safe_add_u32(
			g->ecc.gr.tex_ecc_total_sec_pipe1_count[gpc][tpc].counter,
			gr_pri_gpc0_tpc0_tex_m_ecc_cnt_total_sec_v(
							ecc_stats_reg_val));
		ecc_stats_reg_val &=
			~gr_pri_gpc0_tpc0_tex_m_ecc_cnt_total_sec_m();
		nvgpu_writel(g, nvgpu_safe_add_u32(
			gr_pri_gpc0_tpc0_tex_m_ecc_cnt_total_r(), offset),
			ecc_stats_reg_val);

		ecc_stats_reg_val = nvgpu_readl(g, nvgpu_safe_add_u32(
			gr_pri_gpc0_tpc0_tex_m_ecc_cnt_unique_r(), offset));
		g->ecc.gr.tex_unique_ecc_sec_pipe1_count[gpc][tpc].counter =
		   nvgpu_safe_add_u32(
			g->ecc.gr.tex_unique_ecc_sec_pipe1_count[gpc][tpc].counter,
			gr_pri_gpc0_tpc0_tex_m_ecc_cnt_unique_sec_v(
							ecc_stats_reg_val));
		ecc_stats_reg_val &=
			~gr_pri_gpc0_tpc0_tex_m_ecc_cnt_unique_sec_m();
		nvgpu_writel(g, nvgpu_safe_add_u32(
			gr_pri_gpc0_tpc0_tex_m_ecc_cnt_unique_r(), offset),
			ecc_stats_reg_val);

		nvgpu_writel(g, nvgpu_safe_add_u32(
			gr_pri_gpc0_tpc0_tex_m_routing_r(), offset),
			gr_pri_gpc0_tpc0_tex_m_routing_sel_default_f());
	}

	if ((esr & gr_gpc0_tpc0_tex_m_hww_esr_ecc_ded_pending_f()) != 0U) {
		nvgpu_log(g, gpu_dbg_fn | gpu_dbg_intr,
			"Double bit error detected in TEX!");

		/* Pipe 0 counters */
		nvgpu_writel(g, nvgpu_safe_add_u32(
			gr_pri_gpc0_tpc0_tex_m_routing_r(), offset),
			gr_pri_gpc0_tpc0_tex_m_routing_sel_pipe0_f());

		ecc_stats_reg_val = nvgpu_readl(g, nvgpu_safe_add_u32(
			gr_pri_gpc0_tpc0_tex_m_ecc_cnt_total_r(), offset));
		g->ecc.gr.tex_ecc_total_ded_pipe0_count[gpc][tpc].counter =
		   nvgpu_safe_add_u32(
			g->ecc.gr.tex_ecc_total_ded_pipe0_count[gpc][tpc].counter,
			gr_pri_gpc0_tpc0_tex_m_ecc_cnt_total_ded_v(
							ecc_stats_reg_val));
		ecc_stats_reg_val &=
			~gr_pri_gpc0_tpc0_tex_m_ecc_cnt_total_ded_m();
		nvgpu_writel(g, nvgpu_safe_add_u32(
			gr_pri_gpc0_tpc0_tex_m_ecc_cnt_total_r(), offset),
			ecc_stats_reg_val);

		ecc_stats_reg_val = nvgpu_readl(g, nvgpu_safe_add_u32(
			gr_pri_gpc0_tpc0_tex_m_ecc_cnt_unique_r(), offset));
		g->ecc.gr.tex_unique_ecc_ded_pipe0_count[gpc][tpc].counter =
		   nvgpu_safe_add_u32(
			g->ecc.gr.tex_unique_ecc_ded_pipe0_count[gpc][tpc].counter,
			gr_pri_gpc0_tpc0_tex_m_ecc_cnt_unique_ded_v(
							ecc_stats_reg_val));
		ecc_stats_reg_val &=
			~gr_pri_gpc0_tpc0_tex_m_ecc_cnt_unique_ded_m();
		nvgpu_writel(g, nvgpu_safe_add_u32(
			gr_pri_gpc0_tpc0_tex_m_ecc_cnt_unique_r(), offset),
			ecc_stats_reg_val);

		/* Pipe 1 counters */
		nvgpu_writel(g, nvgpu_safe_add_u32(
			gr_pri_gpc0_tpc0_tex_m_routing_r(), offset),
			gr_pri_gpc0_tpc0_tex_m_routing_sel_pipe1_f());

		ecc_stats_reg_val = nvgpu_readl(g, nvgpu_safe_add_u32(
			gr_pri_gpc0_tpc0_tex_m_ecc_cnt_total_r(), offset));
		g->ecc.gr.tex_ecc_total_ded_pipe1_count[gpc][tpc].counter =
		   nvgpu_safe_add_u32(
			g->ecc.gr.tex_ecc_total_ded_pipe1_count[gpc][tpc].counter,
			gr_pri_gpc0_tpc0_tex_m_ecc_cnt_total_ded_v(
							ecc_stats_reg_val));
		ecc_stats_reg_val &=
			~gr_pri_gpc0_tpc0_tex_m_ecc_cnt_total_ded_m();
		nvgpu_writel(g, nvgpu_safe_add_u32(
			gr_pri_gpc0_tpc0_tex_m_ecc_cnt_total_r(), offset),
			ecc_stats_reg_val);

		ecc_stats_reg_val = nvgpu_readl(g, nvgpu_safe_add_u32(
			gr_pri_gpc0_tpc0_tex_m_ecc_cnt_unique_r(), offset));
		g->ecc.gr.tex_unique_ecc_ded_pipe1_count[gpc][tpc].counter =
		   nvgpu_safe_add_u32(
			g->ecc.gr.tex_unique_ecc_ded_pipe1_count[gpc][tpc].counter,
			gr_pri_gpc0_tpc0_tex_m_ecc_cnt_unique_ded_v(
							ecc_stats_reg_val));
		ecc_stats_reg_val &=
			~gr_pri_gpc0_tpc0_tex_m_ecc_cnt_unique_ded_m();
		nvgpu_writel(g, nvgpu_safe_add_u32(
				gr_pri_gpc0_tpc0_tex_m_ecc_cnt_unique_r(),
				offset),
			ecc_stats_reg_val);

		nvgpu_writel(g, nvgpu_safe_add_u32(
			gr_pri_gpc0_tpc0_tex_m_routing_r(), offset),
			gr_pri_gpc0_tpc0_tex_m_routing_sel_default_f());
	}

	nvgpu_writel(g, nvgpu_safe_add_u32(
				gr_gpc0_tpc0_tex_m_hww_esr_r(), offset),
		     esr | gr_gpc0_tpc0_tex_m_hww_esr_reset_active_f());

}
