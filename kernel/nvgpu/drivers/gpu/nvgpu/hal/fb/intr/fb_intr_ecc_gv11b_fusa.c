/*
 * GV11B ECC INTR
 *
 * Copyright (c) 2016-2022, NVIDIA CORPORATION.  All rights reserved.
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
#include <nvgpu/nvgpu_err.h>

#include "fb_intr_ecc_gv11b.h"

#include <nvgpu/hw/gv11b/hw_fb_gv11b.h>

static void gv11b_fb_intr_handle_ecc_l2tlb_errs(struct gk20a *g,
		u32 ecc_status, u32 ecc_addr)
{
	u32 corrected_error_mask = 0U;
	u32 uncorrected_error_mask = 0U;

	g->ops.fb.ecc.l2tlb_error_mask(&corrected_error_mask,
			&uncorrected_error_mask);

	if ((ecc_status & corrected_error_mask) != 0U) {
		nvgpu_log(g, gpu_dbg_intr, "corrected ecc sa data error");
		/* This error is not expected to occur in gv11b and hence,
		 * this scenario is considered as a fatal error.
		 */
		nvgpu_mutex_release(&g->mm.hub_isr_mutex);
		BUG();
	}
	if ((ecc_status & uncorrected_error_mask) != 0U) {
		nvgpu_report_err_to_sdl(g, NVGPU_ERR_MODULE_HUBMMU,
				GPU_HUBMMU_L2TLB_SA_DATA_ECC_UNCORRECTED);
		nvgpu_err(g, "uncorrected ecc sa data error. "
				"ecc_addr(0x%x)", ecc_addr);
	}
}

void gv11b_fb_intr_handle_ecc_l2tlb(struct gk20a *g, u32 ecc_status)
{
	u32 ecc_addr, corrected_cnt, uncorrected_cnt;
	u32 corrected_delta, uncorrected_delta;
	u32 corrected_overflow, uncorrected_overflow;

	ecc_addr = nvgpu_readl(g, fb_mmu_l2tlb_ecc_address_r());
	corrected_cnt = nvgpu_readl(g,
		fb_mmu_l2tlb_ecc_corrected_err_count_r());
	uncorrected_cnt = nvgpu_readl(g,
		fb_mmu_l2tlb_ecc_uncorrected_err_count_r());

	corrected_delta = fb_mmu_l2tlb_ecc_corrected_err_count_total_v(
							corrected_cnt);
	uncorrected_delta = fb_mmu_l2tlb_ecc_uncorrected_err_count_total_v(
							uncorrected_cnt);
	corrected_overflow = ecc_status &
		fb_mmu_l2tlb_ecc_status_corrected_err_total_counter_overflow_m();

	uncorrected_overflow = ecc_status &
		fb_mmu_l2tlb_ecc_status_uncorrected_err_total_counter_overflow_m();

	/* clear the interrupt */
	if ((corrected_delta > 0U) || (corrected_overflow != 0U)) {
		nvgpu_writel(g, fb_mmu_l2tlb_ecc_corrected_err_count_r(), 0);
	}
	if ((uncorrected_delta > 0U) || (uncorrected_overflow != 0U)) {
		nvgpu_writel(g, fb_mmu_l2tlb_ecc_uncorrected_err_count_r(), 0);
	}

	nvgpu_writel(g, fb_mmu_l2tlb_ecc_status_r(),
				fb_mmu_l2tlb_ecc_status_reset_clear_f());

	/* Handle overflow */
	if (corrected_overflow != 0U) {
		corrected_delta +=
			BIT32(fb_mmu_l2tlb_ecc_corrected_err_count_total_s());
	}
	if (uncorrected_overflow != 0U) {
		uncorrected_delta +=
			BIT32(fb_mmu_l2tlb_ecc_uncorrected_err_count_total_s());
	}

	g->ecc.fb.mmu_l2tlb_ecc_corrected_err_count[0].counter =
		nvgpu_safe_add_u32(
			g->ecc.fb.mmu_l2tlb_ecc_corrected_err_count[0].counter,
			corrected_delta);
	g->ecc.fb.mmu_l2tlb_ecc_uncorrected_err_count[0].counter =
		nvgpu_safe_add_u32(
		       g->ecc.fb.mmu_l2tlb_ecc_uncorrected_err_count[0].counter,
		       uncorrected_delta);

	gv11b_fb_intr_handle_ecc_l2tlb_errs(g, ecc_status, ecc_addr);

	if ((corrected_overflow != 0U) || (uncorrected_overflow != 0U)) {
		nvgpu_info(g, "mmu l2tlb ecc counter overflow!");
	}

	nvgpu_log(g, gpu_dbg_intr,
		"ecc error address: 0x%x", ecc_addr);
	nvgpu_log(g, gpu_dbg_intr,
		"ecc error count corrected: %d, uncorrected %d",
		g->ecc.fb.mmu_l2tlb_ecc_corrected_err_count[0].counter,
		g->ecc.fb.mmu_l2tlb_ecc_uncorrected_err_count[0].counter);
}

static void gv11b_fb_intr_handle_ecc_hubtlb_errs(struct gk20a *g,
		u32 ecc_status, u32 ecc_addr)
{
	if ((ecc_status &
	     fb_mmu_hubtlb_ecc_status_corrected_err_sa_data_m()) != 0U) {
		nvgpu_log(g, gpu_dbg_intr, "corrected ecc sa data error");
		/* This error is not expected to occur in gv11b and hence,
		 * this scenario is considered as a fatal error.
		 */
		nvgpu_mutex_release(&g->mm.hub_isr_mutex);
		BUG();
	}
	if ((ecc_status &
	     fb_mmu_hubtlb_ecc_status_uncorrected_err_sa_data_m()) != 0U) {
		nvgpu_report_err_to_sdl(g, NVGPU_ERR_MODULE_HUBMMU,
				GPU_HUBMMU_TLB_SA_DATA_ECC_UNCORRECTED);
		nvgpu_err(g, "uncorrected ecc sa data error. "
				"ecc_addr(0x%x)", ecc_addr);
	}

}

void gv11b_fb_intr_handle_ecc_hubtlb(struct gk20a *g, u32 ecc_status)
{
	u32 ecc_addr, corrected_cnt, uncorrected_cnt;
	u32 corrected_delta, uncorrected_delta;
	u32 corrected_overflow, uncorrected_overflow;

	ecc_addr = nvgpu_readl(g, fb_mmu_hubtlb_ecc_address_r());
	corrected_cnt = nvgpu_readl(g,
		fb_mmu_hubtlb_ecc_corrected_err_count_r());
	uncorrected_cnt = nvgpu_readl(g,
		fb_mmu_hubtlb_ecc_uncorrected_err_count_r());

	corrected_delta = fb_mmu_hubtlb_ecc_corrected_err_count_total_v(
							corrected_cnt);
	uncorrected_delta = fb_mmu_hubtlb_ecc_uncorrected_err_count_total_v(
							uncorrected_cnt);
	corrected_overflow = ecc_status &
		fb_mmu_hubtlb_ecc_status_corrected_err_total_counter_overflow_m();

	uncorrected_overflow = ecc_status &
		fb_mmu_hubtlb_ecc_status_uncorrected_err_total_counter_overflow_m();

	/* clear the interrupt */
	if ((corrected_delta > 0U) || (corrected_overflow != 0U)) {
		nvgpu_writel(g, fb_mmu_hubtlb_ecc_corrected_err_count_r(), 0);
	}
	if ((uncorrected_delta > 0U) || (uncorrected_overflow != 0U)) {
		nvgpu_writel(g, fb_mmu_hubtlb_ecc_uncorrected_err_count_r(), 0);
	}

	nvgpu_writel(g, fb_mmu_hubtlb_ecc_status_r(),
				fb_mmu_hubtlb_ecc_status_reset_clear_f());

	/* Handle overflow */
	if (corrected_overflow != 0U) {
		corrected_delta +=
			BIT32(fb_mmu_hubtlb_ecc_corrected_err_count_total_s());
	}
	if (uncorrected_overflow != 0U) {
		uncorrected_delta +=
			BIT32(fb_mmu_hubtlb_ecc_uncorrected_err_count_total_s());
	}

	g->ecc.fb.mmu_hubtlb_ecc_corrected_err_count[0].counter =
		nvgpu_safe_add_u32(
			g->ecc.fb.mmu_hubtlb_ecc_corrected_err_count[0].counter,
			corrected_delta);
	g->ecc.fb.mmu_hubtlb_ecc_uncorrected_err_count[0].counter =
		nvgpu_safe_add_u32(
		      g->ecc.fb.mmu_hubtlb_ecc_uncorrected_err_count[0].counter,
		      uncorrected_delta);


	gv11b_fb_intr_handle_ecc_hubtlb_errs(g, ecc_status, ecc_addr);

	if ((corrected_overflow != 0U) || (uncorrected_overflow != 0U)) {
		nvgpu_info(g, "mmu hubtlb ecc counter overflow!");
	}

	nvgpu_log(g, gpu_dbg_intr,
		"ecc error address: 0x%x", ecc_addr);
	nvgpu_log(g, gpu_dbg_intr,
		"ecc error count corrected: %d, uncorrected %d",
		g->ecc.fb.mmu_hubtlb_ecc_corrected_err_count[0].counter,
		g->ecc.fb.mmu_hubtlb_ecc_uncorrected_err_count[0].counter);
}

static void gv11b_fb_intr_handle_ecc_fillunit_errors(struct gk20a *g,
		u32 ecc_status, u32 ecc_addr)
{
	if ((ecc_status &
		fb_mmu_fillunit_ecc_status_corrected_err_pte_data_m()) != 0U) {
		nvgpu_log(g, gpu_dbg_intr, "corrected ecc pte data error");
		/* This error is not expected to occur in gv11b and hence,
		 * this scenario is considered as a fatal error.
		 */
		nvgpu_mutex_release(&g->mm.hub_isr_mutex);
		BUG();
	}
	if ((ecc_status &
		fb_mmu_fillunit_ecc_status_uncorrected_err_pte_data_m())
									!= 0U) {
		nvgpu_report_err_to_sdl(g, NVGPU_ERR_MODULE_HUBMMU,
				GPU_HUBMMU_PTE_DATA_ECC_UNCORRECTED);
		nvgpu_err(g, "uncorrected ecc pte data error. "
				"ecc_addr(0x%x)", ecc_addr);
	}
	if ((ecc_status &
		fb_mmu_fillunit_ecc_status_corrected_err_pde0_data_m()) != 0U) {
		nvgpu_log(g, gpu_dbg_intr, "corrected ecc pde0 data error"
				"ecc_addr(0x%x)", ecc_addr);
		/* This error is not expected to occur in gv11b and hence,
		 * this scenario is considered as a fatal error.
		 */
		nvgpu_mutex_release(&g->mm.hub_isr_mutex);
		BUG();
	}
	if ((ecc_status &
		fb_mmu_fillunit_ecc_status_uncorrected_err_pde0_data_m())
									!= 0U) {
		nvgpu_report_err_to_sdl(g, NVGPU_ERR_MODULE_HUBMMU,
				GPU_HUBMMU_PDE0_DATA_ECC_UNCORRECTED);
		nvgpu_err(g, "uncorrected ecc pde0 data error. "
				"ecc_addr(0x%x)", ecc_addr);
	}
}

void gv11b_fb_intr_handle_ecc_fillunit(struct gk20a *g, u32 ecc_status)
{
	u32 ecc_addr, corrected_cnt, uncorrected_cnt;
	u32 corrected_delta, uncorrected_delta;
	u32 corrected_overflow, uncorrected_overflow;

	ecc_addr = nvgpu_readl(g, fb_mmu_fillunit_ecc_address_r());
	corrected_cnt = nvgpu_readl(g,
		fb_mmu_fillunit_ecc_corrected_err_count_r());
	uncorrected_cnt = nvgpu_readl(g,
		fb_mmu_fillunit_ecc_uncorrected_err_count_r());

	corrected_delta = fb_mmu_fillunit_ecc_corrected_err_count_total_v(
							corrected_cnt);
	uncorrected_delta = fb_mmu_fillunit_ecc_uncorrected_err_count_total_v(
							uncorrected_cnt);
	corrected_overflow = ecc_status &
		fb_mmu_fillunit_ecc_status_corrected_err_total_counter_overflow_m();

	uncorrected_overflow = ecc_status &
		fb_mmu_fillunit_ecc_status_uncorrected_err_total_counter_overflow_m();

	/* clear the interrupt */
	if ((corrected_delta > 0U) || (corrected_overflow != 0U)) {
		nvgpu_writel(g, fb_mmu_fillunit_ecc_corrected_err_count_r(), 0);
	}
	if ((uncorrected_delta > 0U) || (uncorrected_overflow != 0U)) {
		nvgpu_writel(g,
			fb_mmu_fillunit_ecc_uncorrected_err_count_r(), 0);
	}

	nvgpu_writel(g, fb_mmu_fillunit_ecc_status_r(),
				fb_mmu_fillunit_ecc_status_reset_clear_f());

	/* Handle overflow */
	if (corrected_overflow != 0U) {
		corrected_delta +=
			BIT32(fb_mmu_fillunit_ecc_corrected_err_count_total_s());
	}
	if (uncorrected_overflow != 0U) {
		uncorrected_delta +=
			BIT32(fb_mmu_fillunit_ecc_uncorrected_err_count_total_s());
	}

	g->ecc.fb.mmu_fillunit_ecc_corrected_err_count[0].counter =
		nvgpu_safe_add_u32(
		      g->ecc.fb.mmu_fillunit_ecc_corrected_err_count[0].counter,
		      corrected_delta);
	g->ecc.fb.mmu_fillunit_ecc_uncorrected_err_count[0].counter =
		nvgpu_safe_add_u32(
		    g->ecc.fb.mmu_fillunit_ecc_uncorrected_err_count[0].counter,
		    uncorrected_delta);

	gv11b_fb_intr_handle_ecc_fillunit_errors(g, ecc_status, ecc_addr);

	if ((corrected_overflow != 0U) || (uncorrected_overflow != 0U)) {
		nvgpu_info(g, "mmu fillunit ecc counter overflow!");
	}

	nvgpu_log(g, gpu_dbg_intr,
		"ecc error address: 0x%x", ecc_addr);
	nvgpu_log(g, gpu_dbg_intr,
		"ecc error count corrected: %d, uncorrected %d",
		g->ecc.fb.mmu_fillunit_ecc_corrected_err_count[0].counter,
		g->ecc.fb.mmu_fillunit_ecc_uncorrected_err_count[0].counter);
}

void gv11b_fb_intr_handle_ecc(struct gk20a *g)
{
	u32 status;

	nvgpu_info(g, "ecc uncorrected error notify");

	status = nvgpu_readl(g, fb_mmu_l2tlb_ecc_status_r());
	if (status != 0U) {
		g->ops.fb.intr.handle_ecc_l2tlb(g, status);
	}

	status = nvgpu_readl(g, fb_mmu_hubtlb_ecc_status_r());
	if (status != 0U) {
		g->ops.fb.intr.handle_ecc_hubtlb(g, status);
	}

	status = nvgpu_readl(g, fb_mmu_fillunit_ecc_status_r());
	if (status != 0U) {
		g->ops.fb.intr.handle_ecc_fillunit(g, status);
	}
}
