/*
 * GV11B LTC INTR
 *
 * Copyright (c) 2016-2021, NVIDIA CORPORATION.  All rights reserved.
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

#include <nvgpu/io.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/static_analysis.h>
#include <nvgpu/nvgpu_err.h>

#include "ltc_intr_gp10b.h"
#include "ltc_intr_gv11b.h"

#include <nvgpu/hw/gv11b/hw_ltc_gv11b.h>

#include <nvgpu/utils.h>

void gv11b_ltc_intr_configure(struct gk20a *g)
{
	u32 reg;

	/* Disable ltc interrupts to reduce nosie and increase perf */
	reg = nvgpu_readl(g, ltc_ltcs_ltss_intr_r());
	reg &= ~ltc_ltcs_ltss_intr_en_evicted_cb_m();
	reg &= ~ltc_ltcs_ltss_intr_en_illegal_compstat_access_m();
	nvgpu_writel(g, ltc_ltcs_ltss_intr_r(), reg);

	/* illegal_compstat interrupts can be also controlled through
	 * debug_fs, so enable/disable based on g->ltc_intr_en_illegal_compstat
	 * settings
	 */
	if (g->ops.ltc.intr.en_illegal_compstat != NULL) {
		g->ops.ltc.intr.en_illegal_compstat(g,
					g->ltc_intr_en_illegal_compstat);
	}

	/* Enable ECC interrupts */
	reg = nvgpu_readl(g, ltc_ltcs_ltss_intr_r());
	reg |= ltc_ltcs_ltss_intr_en_ecc_sec_error_enabled_f() |
		ltc_ltcs_ltss_intr_en_ecc_ded_error_enabled_f();
	nvgpu_writel(g, ltc_ltcs_ltss_intr_r(), reg);
}

void gv11b_ltc_intr_en_illegal_compstat(struct gk20a *g, bool enable)
{
	u32 val;

	/* disable/enable illegal_compstat interrupt */
	val = nvgpu_readl(g, ltc_ltcs_ltss_intr_r());
	if (enable) {
		val = set_field(val,
			ltc_ltcs_ltss_intr_en_illegal_compstat_m(),
			ltc_ltcs_ltss_intr_en_illegal_compstat_enabled_f());
	} else {
		val = set_field(val,
			ltc_ltcs_ltss_intr_en_illegal_compstat_m(),
			ltc_ltcs_ltss_intr_en_illegal_compstat_disabled_f());
	}
	nvgpu_writel(g, ltc_ltcs_ltss_intr_r(), val);
}

void gv11b_ltc_intr_init_counters(struct gk20a *g,
			u32 corrected_delta, u32 corrected_overflow,
			u32 uncorrected_delta, u32 uncorrected_overflow,
			u32 offset)
{
	if ((corrected_delta > 0U) || (corrected_overflow != 0U)) {
		nvgpu_writel(g,
			nvgpu_safe_add_u32(
			ltc_ltc0_lts0_l2_cache_ecc_corrected_err_count_r(),
			offset), 0);
	}
	if ((uncorrected_delta > 0U) || (uncorrected_overflow != 0U)) {
		nvgpu_writel(g,
			nvgpu_safe_add_u32(
			ltc_ltc0_lts0_l2_cache_ecc_uncorrected_err_count_r(),
			offset), 0);
	}
}

void gv11b_ltc_intr_handle_rstg_ecc_interrupts(struct gk20a *g,
			u32 ltc, u32 slice, u32 ecc_status, u32 ecc_addr)
{
	if ((ecc_status &
		ltc_ltc0_lts0_l2_cache_ecc_status_corrected_err_rstg_m())
								!= 0U) {
		nvgpu_log(g, gpu_dbg_intr, "rstg ecc error corrected");
		/* This error is not expected to occur in gv11b and hence,
		 * this scenario is considered as a fatal error.
		 */
		BUG();
	}
	if ((ecc_status &
		ltc_ltc0_lts0_l2_cache_ecc_status_uncorrected_err_rstg_m())
								!= 0U) {
		nvgpu_log(g, gpu_dbg_intr, "rstg ecc error uncorrected");
		/* This error is not expected to occur in gv11b and hence,
		 * this scenario is considered as a fatal error.
		 */
		BUG();
	}

}

void gv11b_ltc_intr_handle_tstg_ecc_interrupts(struct gk20a *g,
			u32 ltc, u32 slice, u32 ecc_status, u32 ecc_addr)
{
	if ((ecc_status &
		ltc_ltc0_lts0_l2_cache_ecc_status_corrected_err_tstg_m())
								!= 0U) {
		nvgpu_log(g, gpu_dbg_intr, "tstg ecc error corrected");
		/* This error is not expected to occur in gv11b and hence,
		 * this scenario is considered as a fatal error.
		 */
		BUG();
	}
	if ((ecc_status &
		ltc_ltc0_lts0_l2_cache_ecc_status_uncorrected_err_tstg_m())
								!= 0U) {
		nvgpu_report_ecc_err(g,
		NVGPU_ERR_MODULE_LTC,
		(ltc << 8U) | slice,
		GPU_LTC_CACHE_TSTG_ECC_UNCORRECTED, ecc_addr,
		g->ecc.ltc.ecc_ded_count[ltc][slice].counter);
		nvgpu_log(g, gpu_dbg_intr, "tstg ecc error uncorrected");
	}
}

void gv11b_ltc_intr_handle_dstg_ecc_interrupts(struct gk20a *g,
			u32 ltc, u32 slice, u32 ecc_status, u32 dstg_ecc_addr,
			u32 ecc_addr)
{

	if ((ecc_status &
		ltc_ltc0_lts0_l2_cache_ecc_status_corrected_err_dstg_m())
								!= 0U) {
		if ((dstg_ecc_addr &
				ltc_ltc0_lts0_dstg_ecc_address_info_ram_m())
								== 0U) {
			nvgpu_report_ecc_err(g,
				NVGPU_ERR_MODULE_LTC,
				(ltc << 8U) | slice,
				GPU_LTC_CACHE_DSTG_ECC_CORRECTED, ecc_addr,
				g->ecc.ltc.ecc_sec_count[ltc][slice].counter);
		} else {
			/* This error is not expected to occur in gv11b and
			 * hence, this scenario is considered as a fatal error.
			 */
			BUG();
		}
		nvgpu_log(g, gpu_dbg_intr, "dstg ecc error corrected");
	}
	if ((ecc_status &
		ltc_ltc0_lts0_l2_cache_ecc_status_uncorrected_err_dstg_m())
								!= 0U) {
		if ((dstg_ecc_addr &
			ltc_ltc0_lts0_dstg_ecc_address_info_ram_m()) == 0U) {
			nvgpu_report_ecc_err(g,
				NVGPU_ERR_MODULE_LTC,
				(ltc << 8U) | slice,
				GPU_LTC_CACHE_DSTG_ECC_UNCORRECTED, ecc_addr,
				g->ecc.ltc.ecc_ded_count[ltc][slice].counter);
		} else {
			nvgpu_report_ecc_err(g,
				NVGPU_ERR_MODULE_LTC,
				(ltc << 8U) | slice,
				GPU_LTC_CACHE_DSTG_BE_ECC_UNCORRECTED, ecc_addr,
				g->ecc.ltc.ecc_ded_count[ltc][slice].counter);
		}
		nvgpu_log(g, gpu_dbg_intr, "dstg ecc error uncorrected");
	}
}

static void gv11b_ltc_intr_handle_lts_interrupts(struct gk20a *g,
				u32 ltc, u32 slice)
{
	u32 offset;
	u32 ltc_intr3;
	u32 ecc_status, ecc_addr, dstg_ecc_addr, corrected_cnt, uncorrected_cnt;
	u32 corrected_delta, uncorrected_delta;
	u32 corrected_overflow, uncorrected_overflow;
	u32 ltc_stride = nvgpu_get_litter_value(g, GPU_LIT_LTC_STRIDE);
	u32 lts_stride = nvgpu_get_litter_value(g, GPU_LIT_LTS_STRIDE);

	offset = nvgpu_safe_add_u32(nvgpu_safe_mult_u32(ltc_stride, ltc),
					nvgpu_safe_mult_u32(lts_stride, slice));
	ltc_intr3 = nvgpu_readl(g, nvgpu_safe_add_u32(
					ltc_ltc0_lts0_intr3_r(), offset));

	/* Detect and handle ECC PARITY errors */
	if ((ltc_intr3 &
		(ltc_ltcs_ltss_intr3_ecc_uncorrected_m() |
		 ltc_ltcs_ltss_intr3_ecc_corrected_m())) != 0U) {

		ecc_status = nvgpu_readl(g,
			nvgpu_safe_add_u32(
				ltc_ltc0_lts0_l2_cache_ecc_status_r(), offset));
		ecc_addr = nvgpu_readl(g, nvgpu_safe_add_u32(
			ltc_ltc0_lts0_l2_cache_ecc_address_r(), offset));
		dstg_ecc_addr = nvgpu_readl(g,
			nvgpu_safe_add_u32(
				ltc_ltc0_lts0_dstg_ecc_address_r(), offset));
		corrected_cnt = nvgpu_readl(g, nvgpu_safe_add_u32(
			ltc_ltc0_lts0_l2_cache_ecc_corrected_err_count_r(),
			offset));
		uncorrected_cnt = nvgpu_readl(g, nvgpu_safe_add_u32(
			ltc_ltc0_lts0_l2_cache_ecc_uncorrected_err_count_r(),
			offset));

		corrected_delta =
			ltc_ltc0_lts0_l2_cache_ecc_corrected_err_count_total_v(
					corrected_cnt);
		uncorrected_delta =
			ltc_ltc0_lts0_l2_cache_ecc_uncorrected_err_count_total_v(uncorrected_cnt);
		corrected_overflow = ecc_status &
			ltc_ltc0_lts0_l2_cache_ecc_status_corrected_err_total_counter_overflow_m();

		uncorrected_overflow = ecc_status &
			ltc_ltc0_lts0_l2_cache_ecc_status_uncorrected_err_total_counter_overflow_m();

		gv11b_ltc_intr_init_counters(g,
			corrected_delta, corrected_overflow,
			uncorrected_delta, uncorrected_overflow, offset);

		nvgpu_writel(g,
			nvgpu_safe_add_u32(
				ltc_ltc0_lts0_l2_cache_ecc_status_r(), offset),
			ltc_ltc0_lts0_l2_cache_ecc_status_reset_task_f());

		/* update counters per slice */
		if (corrected_overflow != 0U) {
			corrected_delta += BIT32(
				ltc_ltc0_lts0_l2_cache_ecc_corrected_err_count_total_s());
		}
		if (uncorrected_overflow != 0U) {
			uncorrected_delta += BIT32(
				ltc_ltc0_lts0_l2_cache_ecc_uncorrected_err_count_total_s());
		}

		g->ecc.ltc.ecc_sec_count[ltc][slice].counter =
				nvgpu_safe_add_u32(
				g->ecc.ltc.ecc_sec_count[ltc][slice].counter,
					corrected_delta);
		g->ecc.ltc.ecc_ded_count[ltc][slice].counter =
				nvgpu_safe_add_u32(
				g->ecc.ltc.ecc_ded_count[ltc][slice].counter,
					uncorrected_delta);
		nvgpu_log(g, gpu_dbg_intr,
			"ltc:%d lts: %d cache ecc interrupt intr: 0x%x",
			ltc, slice, ltc_intr3);

		/* This check has been added to ensure that the slice id is less
		 * than 8-bits and hence, it can be packed as part of LSB 8-bits
		 * along with the LTC id while reporting LTC related ECC errors.
		 */
		if (slice > U8_MAX) {
			nvgpu_log(g, gpu_dbg_intr, "Invalid slice id=%d",
					slice);
			slice = slice & 0xFFU;
		}

		gv11b_ltc_intr_handle_rstg_ecc_interrupts(g, ltc, slice,
						ecc_status, ecc_addr);

		gv11b_ltc_intr_handle_tstg_ecc_interrupts(g, ltc, slice,
						ecc_status, ecc_addr);

		gv11b_ltc_intr_handle_dstg_ecc_interrupts(g, ltc, slice,
						ecc_status, dstg_ecc_addr,
								ecc_addr);

		if ((corrected_overflow != 0U) ||
				(uncorrected_overflow != 0U)) {
			nvgpu_info(g, "ecc counter overflow!");
		}

		nvgpu_log(g, gpu_dbg_intr, "ecc error address: 0x%x", ecc_addr);

		nvgpu_writel(g,
			nvgpu_safe_add_u32(ltc_ltc0_lts0_intr3_r(), offset),
			ltc_intr3);
	}

	gp10b_ltc_intr_handle_lts_interrupts(g, ltc, slice);
}

void gv11b_ltc_intr_isr(struct gk20a *g, u32 ltc)
{
	u32 slice;

	for (slice = 0U; slice < g->ltc->slices_per_ltc; slice++) {
		gv11b_ltc_intr_handle_lts_interrupts(g, ltc, slice);
	}
}
