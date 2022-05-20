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

#include <unit/unit.h>
#include <unit/io.h>
#include <nvgpu/posix/io.h>
#include <nvgpu/posix/posix-fault-injection.h>

#include <nvgpu/gk20a.h>
#include <nvgpu/sizes.h>
#include <nvgpu/io.h>
#include <nvgpu/nvgpu_init.h>
#include "hal/mc/mc_gp10b.h"
#include "hal/fb/fb_gm20b.h"
#include "hal/fb/fb_gv11b.h"
#include "hal/fb/fb_mmu_fault_gv11b.h"
#include "hal/fb/ecc/fb_ecc_gv11b.h"
#include "hal/fb/intr/fb_intr_gv11b.h"
#include "hal/fb/intr/fb_intr_ecc_gv11b.h"
#include <nvgpu/hw/gv11b/hw_fb_gv11b.h>
#include <nvgpu/hw/gv11b/hw_mc_gv11b.h>

#include "fb_fusa.h"

/* Arbitrary number of errors */
#define ECC_ERRORS 15U

int fb_intr_gv11b_init_test(struct unit_module *m, struct gk20a *g, void *args)
{
	/* HALs under test */
	g->ops.fb.ecc.init = gv11b_fb_ecc_init;
	g->ops.fb.ecc.free = gv11b_fb_ecc_free;
	g->ops.fb.ecc.l2tlb_error_mask = gv11b_fb_ecc_l2tlb_error_mask;
	g->ops.fb.intr.handle_ecc = gv11b_fb_intr_handle_ecc;
	g->ops.fb.intr.handle_ecc_l2tlb = gv11b_fb_intr_handle_ecc_l2tlb;
	g->ops.fb.intr.handle_ecc_hubtlb = gv11b_fb_intr_handle_ecc_hubtlb;
	g->ops.fb.intr.handle_ecc_fillunit = gv11b_fb_intr_handle_ecc_fillunit;

	return UNIT_SUCCESS;
}

int fb_intr_gv11b_isr_test(struct unit_module *m, struct gk20a *g, void *args)
{
	/* Mask all interrupts */
	nvgpu_writel(g, fb_niso_intr_en_set_r(0), 0);
	/* Enable interrupts */
	gv11b_fb_intr_enable(g);
	if (nvgpu_readl(g, fb_niso_intr_en_set_r(0)) == 0) {
		unit_return_fail(m, "FB_INTR not unmasked\n");
	}

	/* Set INTR status register to 0, i.e. no interrupt */
	nvgpu_writel(g, fb_niso_intr_r(), 0);
	if (gv11b_fb_intr_is_mmu_fault_pending(g)) {
		unit_return_fail(m, "MMU fault should NOT be pending\n");
	}
	gv11b_fb_intr_isr(g, 0U);

	/* Hub access counter notify/error: just causes a nvgpu_info call */
	nvgpu_writel(g, fb_niso_intr_r(),
		fb_niso_intr_hub_access_counter_notify_m());
	gv11b_fb_intr_isr(g, 0U);

	/* MMU fault: testing of MMU fault handling is done in other tests */
	nvgpu_writel(g, fb_niso_intr_r(),
		fb_niso_intr_mmu_other_fault_notify_m());
	if (!gv11b_fb_intr_is_mmu_fault_pending(g)) {
		unit_return_fail(m, "MMU fault should be pending\n");
	}
	gv11b_fb_intr_isr(g, 0U);

	/* ECC fault: testing of ECC fault handling is done in other tests */
	nvgpu_writel(g, fb_niso_intr_r(),
		fb_niso_intr_mmu_ecc_uncorrected_error_notify_pending_f());
	gv11b_fb_intr_isr(g, 0U);

	/* Disable interrupts */
	gv11b_fb_intr_disable(g);
	/*
	 * In real HW it may not be possible to read the set/clear registers but
	 * here we can, and what was programmed in the set register should be
	 * the same as what was programmed in the clear register.
	 */
	if (nvgpu_readl(g, fb_niso_intr_en_set_r(0)) !=
		nvgpu_readl(g, fb_niso_intr_en_clr_r(0))) {

		unit_return_fail(m, "FB_INTR set/clear mismatch\n");
	}

	return UNIT_SUCCESS;
}

struct gv11b_ecc_test_parameters {
	u32 status_reg;
	u32 corrected_err_reg;
	u32 uncorrected_err_reg;
	u32 corrected_status, uncorrected_status;
	u32 corrected_overflow, uncorrected_overflow;
};

static struct gv11b_ecc_test_parameters l2tlb_parameters = {
	.status_reg = fb_mmu_l2tlb_ecc_status_r(),
	.corrected_err_reg = fb_mmu_l2tlb_ecc_corrected_err_count_r(),
	.uncorrected_err_reg = fb_mmu_l2tlb_ecc_uncorrected_err_count_r(),
	.corrected_status = fb_mmu_l2tlb_ecc_status_corrected_err_l2tlb_sa_data_m(),
	.uncorrected_status = fb_mmu_l2tlb_ecc_status_uncorrected_err_l2tlb_sa_data_m(),
	.corrected_overflow = fb_mmu_l2tlb_ecc_status_corrected_err_total_counter_overflow_m(),
	.uncorrected_overflow = fb_mmu_l2tlb_ecc_status_uncorrected_err_total_counter_overflow_m(),
};

static struct gv11b_ecc_test_parameters hubtlb_parameters = {
	.status_reg = fb_mmu_hubtlb_ecc_status_r(),
	.corrected_err_reg = fb_mmu_hubtlb_ecc_corrected_err_count_r(),
	.uncorrected_err_reg = fb_mmu_hubtlb_ecc_uncorrected_err_count_r(),
	.corrected_status = fb_mmu_hubtlb_ecc_status_corrected_err_sa_data_m(),
	.uncorrected_status = fb_mmu_hubtlb_ecc_status_uncorrected_err_sa_data_m(),
	.corrected_overflow = fb_mmu_hubtlb_ecc_status_corrected_err_total_counter_overflow_m(),
	.uncorrected_overflow = fb_mmu_hubtlb_ecc_status_uncorrected_err_total_counter_overflow_m(),
};

static struct gv11b_ecc_test_parameters fillunit_parameters = {
	.status_reg = fb_mmu_fillunit_ecc_status_r(),
	.corrected_err_reg = fb_mmu_fillunit_ecc_corrected_err_count_r(),
	.uncorrected_err_reg = fb_mmu_fillunit_ecc_uncorrected_err_count_r(),
	.corrected_status = fb_mmu_fillunit_ecc_status_corrected_err_pte_data_m(),
	.uncorrected_status = fb_mmu_fillunit_ecc_status_uncorrected_err_pte_data_m(),
	.corrected_overflow = fb_mmu_fillunit_ecc_status_corrected_err_total_counter_overflow_m(),
	.uncorrected_overflow = fb_mmu_fillunit_ecc_status_uncorrected_err_total_counter_overflow_m(),
};

int fb_intr_gv11b_ecc_test(struct unit_module *m, struct gk20a *g, void *args)
{
	struct gv11b_ecc_test_parameters *p;
	u64 subcase = (u64) args;

	switch (subcase) {
	case TEST_ECC_L2TLB:
		p = &l2tlb_parameters;
		break;
	case TEST_ECC_HUBTLB:
		p = &hubtlb_parameters;
		break;
	case TEST_ECC_FILLUNIT:
		p = &fillunit_parameters;
		break;
	default:
		unit_return_fail(m, "Invalid subcase\n");
	}

	g->ops.ecc.ecc_init_support(g);

	g->ops.fb.ecc.init(g);

	/* Set the interrupt status as corrected */
	nvgpu_writel(g, p->status_reg, p->corrected_status);
	EXPECT_BUG(gv11b_fb_intr_isr(g, 0U));

	/* Set the interrupt status as uncorrected */
	nvgpu_writel(g, p->status_reg, p->uncorrected_status);
	gv11b_fb_intr_isr(g, 0U);

	/* Set arbitrary number of corrected and uncorrected errors */
	nvgpu_writel(g, p->corrected_err_reg, ECC_ERRORS);
	nvgpu_writel(g, p->uncorrected_err_reg, ECC_ERRORS);
	gv11b_fb_intr_isr(g, 0U);

	/* Same but with corrected overflow bit set */
	nvgpu_writel(g, p->status_reg, 1 | p->corrected_overflow);
	nvgpu_writel(g, p->corrected_err_reg, ECC_ERRORS);
	nvgpu_writel(g, p->uncorrected_err_reg, ECC_ERRORS);
	EXPECT_BUG(gv11b_fb_intr_isr(g, 0U));

	/* Same but with uncorrected overflow bit set */
	nvgpu_writel(g, p->status_reg, 1 | p->uncorrected_overflow);
	nvgpu_writel(g, p->corrected_err_reg, ECC_ERRORS);
	nvgpu_writel(g, p->uncorrected_err_reg, ECC_ERRORS);
	EXPECT_BUG(gv11b_fb_intr_isr(g, 0U));

	/* Both overflow but error counts at 0 */
	nvgpu_writel(g, p->status_reg, 1 | p->corrected_overflow |
		p->uncorrected_overflow);
	nvgpu_writel(g, p->corrected_err_reg, 0);
	nvgpu_writel(g, p->uncorrected_err_reg, 0);
	EXPECT_BUG(gv11b_fb_intr_isr(g, 0U));

	/* Extra case for fillunit */
	if (subcase == TEST_ECC_FILLUNIT) {
		/* PDE0 */
		nvgpu_writel(g, p->status_reg,
			fb_mmu_fillunit_ecc_status_corrected_err_pde0_data_m() |
			fb_mmu_fillunit_ecc_status_uncorrected_err_pde0_data_m());
		EXPECT_BUG(gv11b_fb_intr_isr(g, 0U));
	}

	/* Clear interrupt status */
	nvgpu_writel(g, p->status_reg, 0);

	g->ops.fb.ecc.free(g);

	return UNIT_SUCCESS;
}
