/*
 * GA10B FB INTR ECC
 *
 * Copyright (c) 2020, NVIDIA CORPORATION.  All rights reserved.
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

#include <nvgpu/log.h>
#include <nvgpu/io.h>
#include <nvgpu/gk20a.h>

#include "hal/fb/intr/fb_intr_ecc_gv11b.h"
#include "fb_intr_ecc_ga10b.h"

#include <nvgpu/hw/ga10b/hw_fb_ga10b.h>

void ga10b_fb_intr_handle_ecc_l2tlb(struct gk20a *g, u32 ecc_status)
{
	u32 corrected_cnt, uncorrected_cnt;
	u32 unique_corrected_delta, unique_uncorrected_delta;
	u32 unique_corrected_overflow, unique_uncorrected_overflow;

	/*
	 * The unique counters tracks the instances of ecc (un)corrected errors
	 * where the present, previous error addresses are different.
	 */
	corrected_cnt = nvgpu_readl(g,
		fb_mmu_l2tlb_ecc_corrected_err_count_r());
	uncorrected_cnt = nvgpu_readl(g,
		fb_mmu_l2tlb_ecc_uncorrected_err_count_r());

	unique_corrected_delta =
		fb_mmu_l2tlb_ecc_corrected_err_count_unique_v(corrected_cnt);
	unique_uncorrected_delta =
		fb_mmu_l2tlb_ecc_uncorrected_err_count_unique_v(uncorrected_cnt);
	unique_corrected_overflow = ecc_status &
		fb_mmu_l2tlb_ecc_status_corrected_err_unique_counter_overflow_m();

	unique_uncorrected_overflow = ecc_status &
		fb_mmu_l2tlb_ecc_status_uncorrected_err_unique_counter_overflow_m();

	/* Handle overflow */
	if (unique_corrected_overflow != 0U) {
		unique_corrected_delta +=
			BIT32(fb_mmu_l2tlb_ecc_corrected_err_count_unique_s());
	}
	if (unique_uncorrected_overflow != 0U) {
		unique_uncorrected_delta +=
			BIT32(fb_mmu_l2tlb_ecc_uncorrected_err_count_unique_s());
	}

	g->ecc.fb.mmu_l2tlb_ecc_corrected_unique_err_count[0].counter =
	nvgpu_safe_add_u32(
		g->ecc.fb.mmu_l2tlb_ecc_corrected_unique_err_count[0].counter,
		unique_corrected_delta);
	g->ecc.fb.mmu_l2tlb_ecc_uncorrected_unique_err_count[0].counter =
	nvgpu_safe_add_u32(
	       g->ecc.fb.mmu_l2tlb_ecc_uncorrected_unique_err_count[0].counter,
	       unique_uncorrected_delta);

	if ((unique_corrected_overflow != 0U) || (unique_uncorrected_overflow != 0U)) {
		nvgpu_info(g, "mmu l2tlb ecc counter overflow!");
	}

	/*
	 * Handle the legacy counters.
	 */
	gv11b_fb_intr_handle_ecc_l2tlb(g, ecc_status);
}

void ga10b_fb_intr_handle_ecc_hubtlb(struct gk20a *g, u32 ecc_status)
{
	u32 corrected_cnt, uncorrected_cnt;
	u32 unique_corrected_delta, unique_uncorrected_delta;
	u32 unique_corrected_overflow, unique_uncorrected_overflow;

	/*
	 * The unique counters tracks the instances of ecc (un)corrected errors
	 * where the present, previous error addresses are different.
	 */
	corrected_cnt = nvgpu_readl(g,
		fb_mmu_hubtlb_ecc_corrected_err_count_r());
	uncorrected_cnt = nvgpu_readl(g,
		fb_mmu_hubtlb_ecc_uncorrected_err_count_r());

	unique_corrected_delta =
		fb_mmu_hubtlb_ecc_corrected_err_count_unique_v(corrected_cnt);
	unique_uncorrected_delta =
		fb_mmu_hubtlb_ecc_uncorrected_err_count_unique_v(uncorrected_cnt);
	unique_corrected_overflow = ecc_status &
		fb_mmu_hubtlb_ecc_status_corrected_err_unique_counter_overflow_m();

	unique_uncorrected_overflow = ecc_status &
		fb_mmu_hubtlb_ecc_status_uncorrected_err_unique_counter_overflow_m();

	/* Handle overflow */
	if (unique_corrected_overflow != 0U) {
		unique_corrected_delta +=
			BIT32(fb_mmu_hubtlb_ecc_corrected_err_count_unique_s());
	}
	if (unique_uncorrected_overflow != 0U) {
		unique_uncorrected_delta +=
			BIT32(fb_mmu_hubtlb_ecc_uncorrected_err_count_unique_s());
	}

	g->ecc.fb.mmu_hubtlb_ecc_corrected_unique_err_count[0].counter =
	nvgpu_safe_add_u32(
		g->ecc.fb.mmu_hubtlb_ecc_corrected_unique_err_count[0].counter,
		unique_corrected_delta);
	g->ecc.fb.mmu_hubtlb_ecc_uncorrected_unique_err_count[0].counter =
	nvgpu_safe_add_u32(
	       g->ecc.fb.mmu_hubtlb_ecc_uncorrected_unique_err_count[0].counter,
	       unique_uncorrected_delta);

	if ((unique_corrected_overflow != 0U) || (unique_uncorrected_overflow != 0U)) {
		nvgpu_info(g, "mmu hubtlb ecc counter overflow!");
	}

	/*
	 * Handle the legacy counters.
	 */
	gv11b_fb_intr_handle_ecc_hubtlb(g, ecc_status);
}

void ga10b_fb_intr_handle_ecc_fillunit(struct gk20a *g, u32 ecc_status)
{
	u32 corrected_cnt, uncorrected_cnt;
	u32 unique_corrected_delta, unique_uncorrected_delta;
	u32 unique_corrected_overflow, unique_uncorrected_overflow;

	/*
	 * The unique counters tracks the instances of ecc (un)corrected errors
	 * where the present, previous error addresses are different.
	 */
	corrected_cnt = nvgpu_readl(g,
		fb_mmu_fillunit_ecc_corrected_err_count_r());
	uncorrected_cnt = nvgpu_readl(g,
		fb_mmu_fillunit_ecc_uncorrected_err_count_r());

	unique_corrected_delta =
		fb_mmu_fillunit_ecc_corrected_err_count_unique_v(corrected_cnt);
	unique_uncorrected_delta =
		fb_mmu_fillunit_ecc_uncorrected_err_count_unique_v(uncorrected_cnt);
	unique_corrected_overflow = ecc_status &
		fb_mmu_fillunit_ecc_status_corrected_err_unique_counter_overflow_m();

	unique_uncorrected_overflow = ecc_status &
		fb_mmu_fillunit_ecc_status_uncorrected_err_unique_counter_overflow_m();

	/* Handle overflow */
	if (unique_corrected_overflow != 0U) {
		unique_corrected_delta +=
			BIT32(fb_mmu_fillunit_ecc_corrected_err_count_unique_s());
	}
	if (unique_uncorrected_overflow != 0U) {
		unique_uncorrected_delta +=
			BIT32(fb_mmu_fillunit_ecc_uncorrected_err_count_unique_s());
	}

	g->ecc.fb.mmu_fillunit_ecc_corrected_unique_err_count[0].counter =
	nvgpu_safe_add_u32(
		g->ecc.fb.mmu_fillunit_ecc_corrected_unique_err_count[0].counter,
		unique_corrected_delta);
	g->ecc.fb.mmu_fillunit_ecc_uncorrected_unique_err_count[0].counter =
	nvgpu_safe_add_u32(
	       g->ecc.fb.mmu_fillunit_ecc_uncorrected_unique_err_count[0].counter,
	       unique_uncorrected_delta);

	if ((unique_corrected_overflow != 0U) || (unique_uncorrected_overflow != 0U)) {
		nvgpu_info(g, "mmu fillunit ecc counter overflow!");
	}

	/*
	 * Handle the legacy counters.
	 */
	gv11b_fb_intr_handle_ecc_fillunit(g, ecc_status);
}
