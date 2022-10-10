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

#ifdef CONFIG_NVGPU_INJECT_HWERR
#include <nvgpu/io.h>
#include <nvgpu/gk20a.h>

#include "hal/gr/ecc/ecc_gv11b.h"
#include "ecc_ga10b.h"

#include <nvgpu/hw/ga10b/hw_gr_ga10b.h>

static inline u32 pri_gpc0_mmu0_l1tlb_ecc_control_r(void)
{
	return gr_gpc0_mmu0_l1tlb_ecc_control_r();
}

static inline u32 pri_gpc0_mmu0_l1tlb_ecc_control_inject_uncorrected_err_f(u32 v)
{
	return gr_gpc0_mmu0_l1tlb_ecc_control_inject_uncorrected_err_f(v);
}

struct nvgpu_hw_err_inject_info mmu_ecc_err_desc[] = {
	/*
	 * NV_SCAL_LITTER_NUM_GPCMMU_PER_GPC only shows 1 GPCMMU per GPC.
	 * Add support to GPC_MMU0_L1TLB, GPC_MMU L1TLB not handled here.
	 */

	NVGPU_ECC_ERR("l1tlb_sa_data_ecc_uncorrected",
		gv11b_gr_intr_inject_mmu_ecc_error,
		pri_gpc0_mmu0_l1tlb_ecc_control_r,
		pri_gpc0_mmu0_l1tlb_ecc_control_inject_uncorrected_err_f),
};

struct nvgpu_hw_err_inject_info_desc mmu_err_desc;

struct nvgpu_hw_err_inject_info_desc *
		ga10b_gr_ecc_get_mmu_err_desc(struct gk20a *g)
{
	(void)g;

	mmu_err_desc.info_ptr = mmu_ecc_err_desc;
	mmu_err_desc.info_size = nvgpu_safe_cast_u64_to_u32(
				sizeof(mmu_ecc_err_desc) /
				sizeof(struct nvgpu_hw_err_inject_info));

	return &mmu_err_desc;
}
#endif /* CONFIG_NVGPU_INJECT_HWERR */
