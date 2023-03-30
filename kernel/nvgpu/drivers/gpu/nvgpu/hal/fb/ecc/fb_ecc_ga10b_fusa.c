/*
 * GA10B FB ECC
 *
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

#include <nvgpu/log.h>
#include <nvgpu/io.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/static_analysis.h>

#include "hal/fb/ecc/fb_ecc_gv11b.h"
#include "fb_ecc_ga10b.h"

#include <nvgpu/hw/ga10b/hw_fb_ga10b.h>

static u32 fb_ecc_l2tlb_corrected_error_mask(void)
{
	u32 mask = fb_mmu_l2tlb_ecc_status_corrected_err_l2tlb_sa_data_m() |
	 fb_mmu_l2tlb_ecc_status_corrected_err_l2tlb1_sa_data_m();

	return mask;
}

static u32 fb_ecc_l2tlb_uncorrected_error_mask(void)
{
	u32 mask = fb_mmu_l2tlb_ecc_status_uncorrected_err_l2tlb_sa_data_m() |
	 fb_mmu_l2tlb_ecc_status_uncorrected_err_l2tlb1_sa_data_m();

	return mask;
}

int ga10b_fb_ecc_init(struct gk20a *g)
{
	int err = 0;

	err = gv11b_fb_ecc_init(g);
	if (err != 0) {
		goto init_fb_ecc_err;
	}
	err = NVGPU_ECC_COUNTER_INIT_FB(mmu_l2tlb_ecc_uncorrected_unique_err_count);
	if (err != 0) {
		goto init_fb_ecc_err;
	}
	err = NVGPU_ECC_COUNTER_INIT_FB(mmu_l2tlb_ecc_corrected_unique_err_count);
	if (err != 0) {
		goto init_fb_ecc_err;
	}
	err = NVGPU_ECC_COUNTER_INIT_FB(mmu_hubtlb_ecc_uncorrected_unique_err_count);
	if (err != 0) {
		goto init_fb_ecc_err;
	}
	err = NVGPU_ECC_COUNTER_INIT_FB(mmu_hubtlb_ecc_corrected_unique_err_count);
	if (err != 0) {
		goto init_fb_ecc_err;
	}
	err = NVGPU_ECC_COUNTER_INIT_FB(mmu_fillunit_ecc_uncorrected_unique_err_count);
	if (err != 0) {
		goto init_fb_ecc_err;
	}
	err = NVGPU_ECC_COUNTER_INIT_FB(mmu_fillunit_ecc_corrected_unique_err_count);
	if (err != 0) {
		goto init_fb_ecc_err;
	}

init_fb_ecc_err:

	if (err != 0) {
		nvgpu_err(g, "ecc counter allocate failed, err=%d", err);
		ga10b_fb_ecc_free(g);
	}

	return err;
}

void ga10b_fb_ecc_free(struct gk20a *g)
{
	NVGPU_ECC_COUNTER_FREE_FB(mmu_l2tlb_ecc_corrected_unique_err_count);
	NVGPU_ECC_COUNTER_FREE_FB(mmu_l2tlb_ecc_uncorrected_unique_err_count);
	NVGPU_ECC_COUNTER_FREE_FB(mmu_hubtlb_ecc_corrected_unique_err_count);
	NVGPU_ECC_COUNTER_FREE_FB(mmu_hubtlb_ecc_uncorrected_unique_err_count);
	NVGPU_ECC_COUNTER_FREE_FB(mmu_fillunit_ecc_corrected_unique_err_count);
	NVGPU_ECC_COUNTER_FREE_FB(mmu_fillunit_ecc_uncorrected_unique_err_count);

	gv11b_fb_ecc_free(g);
}

void ga10b_fb_ecc_l2tlb_error_mask(u32 *corrected_error_mask,
		u32 *uncorrected_error_mask)
{
	*corrected_error_mask = fb_ecc_l2tlb_corrected_error_mask();

	*uncorrected_error_mask = fb_ecc_l2tlb_uncorrected_error_mask();

	return;
}
