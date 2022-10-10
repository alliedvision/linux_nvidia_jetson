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

#include <nvgpu/io.h>
#include <nvgpu/ecc.h>
#include <nvgpu/gk20a.h>

#include <nvgpu/hw/gv11b/hw_gr_gv11b.h>

#include "ecc_gv11b.h"

#ifdef CONFIG_NVGPU_INJECT_HWERR
void gv11b_gr_intr_inject_fecs_ecc_error(struct gk20a *g,
		struct nvgpu_hw_err_inject_info *err, u32 error_info)
{
	(void)error_info;
	nvgpu_info(g, "Injecting FECS fault %s", err->name);
	nvgpu_writel(g, err->get_reg_addr(), err->get_reg_val(1U));
}

void gv11b_gr_intr_inject_gpccs_ecc_error(struct gk20a *g,
		struct nvgpu_hw_err_inject_info *err, u32 error_info)
{
	unsigned int gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_GPC_STRIDE);
	unsigned int gpc = (error_info & 0xFFU);
	unsigned int reg_addr = nvgpu_safe_add_u32(err->get_reg_addr(),
					nvgpu_safe_mult_u32(gpc, gpc_stride));

	nvgpu_info(g, "Injecting GPCCS fault %s for gpc: %d", err->name, gpc);
	nvgpu_writel(g, reg_addr, err->get_reg_val(1U));
}

void gv11b_gr_intr_inject_sm_ecc_error(struct gk20a *g,
		struct nvgpu_hw_err_inject_info *err,
		u32 error_info)
{
	unsigned int gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_GPC_STRIDE);
	unsigned int tpc_stride =
		nvgpu_get_litter_value(g, GPU_LIT_TPC_IN_GPC_STRIDE);
	unsigned int gpc = (error_info & 0xFF00U) >> 8U;
	unsigned int tpc = (error_info & 0xFFU);
	unsigned int reg_addr = nvgpu_safe_add_u32(err->get_reg_addr(),
					nvgpu_safe_add_u32(
					nvgpu_safe_mult_u32(gpc, gpc_stride),
					nvgpu_safe_mult_u32(tpc, tpc_stride)));

	nvgpu_info(g, "Injecting SM fault %s for gpc: %d, tpc: %d",
			err->name, gpc, tpc);
	nvgpu_writel(g, reg_addr, err->get_reg_val(1U));
}

void gv11b_gr_intr_inject_mmu_ecc_error(struct gk20a *g,
		struct nvgpu_hw_err_inject_info *err, u32 error_info)
{
	unsigned int gpc_stride = nvgpu_get_litter_value(g, GPU_LIT_GPC_STRIDE);
	unsigned int gpc = (error_info & 0xFFU);
	unsigned int reg_addr = nvgpu_safe_add_u32(err->get_reg_addr(),
					nvgpu_safe_mult_u32(gpc, gpc_stride));

	nvgpu_info(g, "Injecting MMU fault %s for gpc: %d", err->name, gpc);
	nvgpu_writel(g, reg_addr, err->get_reg_val(1U));
}

void gv11b_gr_intr_inject_gcc_ecc_error(struct gk20a *g,
		struct nvgpu_hw_err_inject_info *err, u32 error_info)
{
	unsigned int gpc_stride = nvgpu_get_litter_value(g,
			GPU_LIT_GPC_STRIDE);
	unsigned int gpc = (error_info & 0xFFU);
	unsigned int reg_addr = nvgpu_safe_add_u32(err->get_reg_addr(),
					nvgpu_safe_mult_u32(gpc, gpc_stride));

	nvgpu_info(g, "Injecting GCC fault %s for gpc: %d", err->name, gpc);
	nvgpu_writel(g, reg_addr, err->get_reg_val(1U));
}

static inline u32 fecs_falcon_ecc_control_r(void)
{
	return gr_fecs_falcon_ecc_control_r();
}

static inline u32 fecs_falcon_ecc_control_inject_corrected_err_f(u32 v)
{
	return gr_fecs_falcon_ecc_control_inject_corrected_err_f(v);
}

static inline u32 fecs_falcon_ecc_control_inject_uncorrected_err_f(u32 v)
{
	return gr_fecs_falcon_ecc_control_inject_uncorrected_err_f(v);
}

static struct nvgpu_hw_err_inject_info fecs_ecc_err_desc[] = {
	NVGPU_ECC_ERR("falcon_imem_ecc_corrected",
			gv11b_gr_intr_inject_fecs_ecc_error,
			fecs_falcon_ecc_control_r,
			fecs_falcon_ecc_control_inject_corrected_err_f),
	NVGPU_ECC_ERR("falcon_imem_ecc_uncorrected",
			gv11b_gr_intr_inject_fecs_ecc_error,
			fecs_falcon_ecc_control_r,
			fecs_falcon_ecc_control_inject_uncorrected_err_f),
};

static struct nvgpu_hw_err_inject_info_desc fecs_err_desc;

struct nvgpu_hw_err_inject_info_desc *
gv11b_gr_intr_get_fecs_err_desc(struct gk20a *g)
{
	(void)g;
	fecs_err_desc.info_ptr = fecs_ecc_err_desc;
	fecs_err_desc.info_size = nvgpu_safe_cast_u64_to_u32(
			sizeof(fecs_ecc_err_desc) /
			sizeof(struct nvgpu_hw_err_inject_info));

	return &fecs_err_desc;
}

static inline u32 gpccs_falcon_ecc_control_r(void)
{
	return gr_gpccs_falcon_ecc_control_r();
}

static inline u32 gpccs_falcon_ecc_control_inject_corrected_err_f(u32 v)
{
	return gr_gpccs_falcon_ecc_control_inject_corrected_err_f(v);
}

static inline u32 gpccs_falcon_ecc_control_inject_uncorrected_err_f(u32 v)
{
	return gr_gpccs_falcon_ecc_control_inject_uncorrected_err_f(v);
}

static struct nvgpu_hw_err_inject_info gpccs_ecc_err_desc[] = {
	NVGPU_ECC_ERR("falcon_imem_ecc_corrected",
			gv11b_gr_intr_inject_gpccs_ecc_error,
			gpccs_falcon_ecc_control_r,
			gpccs_falcon_ecc_control_inject_corrected_err_f),
	NVGPU_ECC_ERR("falcon_imem_ecc_uncorrected",
			gv11b_gr_intr_inject_gpccs_ecc_error,
			gpccs_falcon_ecc_control_r,
			gpccs_falcon_ecc_control_inject_uncorrected_err_f),
};

static struct nvgpu_hw_err_inject_info_desc gpccs_err_desc;

struct nvgpu_hw_err_inject_info_desc *
gv11b_gr_intr_get_gpccs_err_desc(struct gk20a *g)
{
	(void)g;
	gpccs_err_desc.info_ptr = gpccs_ecc_err_desc;
	gpccs_err_desc.info_size = nvgpu_safe_cast_u64_to_u32(
			sizeof(gpccs_ecc_err_desc) /
			sizeof(struct nvgpu_hw_err_inject_info));

	return &gpccs_err_desc;
}

static inline u32 pri_gpc0_tpc0_sm_l1_tag_ecc_control_r(void)
{
	return gr_pri_gpc0_tpc0_sm_l1_tag_ecc_control_r();
}

static inline u32 pri_gpc0_tpc0_sm_l1_tag_ecc_control_inject_corrected_err_f(u32 v)
{
	return gr_pri_gpc0_tpc0_sm_l1_tag_ecc_control_inject_corrected_err_f(v);
}

static inline u32 pri_gpc0_tpc0_sm_l1_tag_ecc_control_inject_uncorrected_err_f(u32 v)
{
	return gr_pri_gpc0_tpc0_sm_l1_tag_ecc_control_inject_uncorrected_err_f(v);
}

static inline u32 pri_gpc0_tpc0_sm_cbu_ecc_control_r(void)
{
	return gr_pri_gpc0_tpc0_sm_cbu_ecc_control_r();
}

static inline u32 pri_gpc0_tpc0_sm_cbu_ecc_control_inject_uncorrected_err_f(u32 v)
{
	return gr_pri_gpc0_tpc0_sm_cbu_ecc_control_inject_uncorrected_err_f(v);
}

static inline u32 pri_gpc0_tpc0_sm_lrf_ecc_control_r(void)
{
	return gr_pri_gpc0_tpc0_sm_lrf_ecc_control_r();
}

static inline u32 pri_gpc0_tpc0_sm_lrf_ecc_control_inject_uncorrected_err_f(u32 v)
{
	return gr_pri_gpc0_tpc0_sm_lrf_ecc_control_inject_uncorrected_err_f(v);
}

static inline u32 pri_gpc0_tpc0_sm_l1_data_ecc_control_r(void)
{
	return gr_pri_gpc0_tpc0_sm_l1_data_ecc_control_r();
}

static inline u32 pri_gpc0_tpc0_sm_l1_data_ecc_control_inject_uncorrected_err_f(u32 v)
{
	return gr_pri_gpc0_tpc0_sm_l1_data_ecc_control_inject_uncorrected_err_f(v);
}

static inline u32 pri_gpc0_tpc0_sm_icache_ecc_control_r(void)
{
	return gr_pri_gpc0_tpc0_sm_icache_ecc_control_r();
}

static inline u32 pri_gpc0_tpc0_sm_icache_ecc_control_inject_uncorrected_err_f(u32 v)
{
	return gr_pri_gpc0_tpc0_sm_icache_ecc_control_inject_uncorrected_err_f(v);
}

static inline u32 pri_gpc0_mmu_l1tlb_ecc_control_r(void)
{
	return gr_gpc0_mmu_l1tlb_ecc_control_r();
}

static inline u32 pri_gpc0_mmu_l1tlb_ecc_control_inject_uncorrected_err_f(u32 v)
{
	return gr_gpc0_mmu_l1tlb_ecc_control_inject_uncorrected_err_f(v);
}

static inline u32 pri_gpc0_gcc_l15_ecc_control_r(void)
{
	return gr_pri_gpc0_gcc_l15_ecc_control_r();
}

static inline u32 pri_gpc0_gcc_l15_ecc_control_inject_uncorrected_err_f(u32 v)
{
	return gr_pri_gpc0_gcc_l15_ecc_control_inject_uncorrected_err_f(v);
}

static struct nvgpu_hw_err_inject_info sm_ecc_err_desc[] = {
	NVGPU_ECC_ERR("l1_tag_ecc_corrected",
			gv11b_gr_intr_inject_sm_ecc_error,
			pri_gpc0_tpc0_sm_l1_tag_ecc_control_r,
			pri_gpc0_tpc0_sm_l1_tag_ecc_control_inject_corrected_err_f),
	NVGPU_ECC_ERR("l1_tag_ecc_uncorrected",
			gv11b_gr_intr_inject_sm_ecc_error,
			pri_gpc0_tpc0_sm_l1_tag_ecc_control_r,
			pri_gpc0_tpc0_sm_l1_tag_ecc_control_inject_uncorrected_err_f),
	NVGPU_ECC_ERR("cbu_ecc_uncorrected",
			gv11b_gr_intr_inject_sm_ecc_error,
			pri_gpc0_tpc0_sm_cbu_ecc_control_r,
			pri_gpc0_tpc0_sm_cbu_ecc_control_inject_uncorrected_err_f),
	NVGPU_ECC_ERR("lrf_ecc_uncorrected",
			gv11b_gr_intr_inject_sm_ecc_error,
			pri_gpc0_tpc0_sm_lrf_ecc_control_r,
			pri_gpc0_tpc0_sm_lrf_ecc_control_inject_uncorrected_err_f),
	NVGPU_ECC_ERR("l1_data_ecc_uncorrected",
			gv11b_gr_intr_inject_sm_ecc_error,
			pri_gpc0_tpc0_sm_l1_data_ecc_control_r,
			pri_gpc0_tpc0_sm_l1_data_ecc_control_inject_uncorrected_err_f),
	NVGPU_ECC_ERR("icache_l0_data_ecc_uncorrected",
			gv11b_gr_intr_inject_sm_ecc_error,
			pri_gpc0_tpc0_sm_icache_ecc_control_r,
			pri_gpc0_tpc0_sm_icache_ecc_control_inject_uncorrected_err_f),
};

static struct nvgpu_hw_err_inject_info_desc sm_err_desc;

struct nvgpu_hw_err_inject_info_desc *
gv11b_gr_intr_get_sm_err_desc(struct gk20a *g)
{
	(void)g;
	sm_err_desc.info_ptr = sm_ecc_err_desc;
	sm_err_desc.info_size = nvgpu_safe_cast_u64_to_u32(
			sizeof(sm_ecc_err_desc) /
			sizeof(struct nvgpu_hw_err_inject_info));

	return &sm_err_desc;
}

static struct nvgpu_hw_err_inject_info mmu_ecc_err_desc[] = {
	NVGPU_ECC_ERR("l1tlb_sa_data_ecc_uncorrected",
			gv11b_gr_intr_inject_mmu_ecc_error,
			pri_gpc0_mmu_l1tlb_ecc_control_r,
			pri_gpc0_mmu_l1tlb_ecc_control_inject_uncorrected_err_f),
};

static struct nvgpu_hw_err_inject_info_desc mmu_err_desc;

struct nvgpu_hw_err_inject_info_desc *
gv11b_gr_intr_get_mmu_err_desc(struct gk20a *g)
{
	(void)g;
	mmu_err_desc.info_ptr = mmu_ecc_err_desc;
	mmu_err_desc.info_size = nvgpu_safe_cast_u64_to_u32(
			sizeof(mmu_ecc_err_desc) /
			sizeof(struct nvgpu_hw_err_inject_info));

	return &mmu_err_desc;
}

static struct nvgpu_hw_err_inject_info gcc_ecc_err_desc[] = {
	NVGPU_ECC_ERR("l15_ecc_uncorrected",
			gv11b_gr_intr_inject_gcc_ecc_error,
			pri_gpc0_gcc_l15_ecc_control_r,
			pri_gpc0_gcc_l15_ecc_control_inject_uncorrected_err_f),
};

static struct nvgpu_hw_err_inject_info_desc gcc_err_desc;

struct nvgpu_hw_err_inject_info_desc *
gv11b_gr_intr_get_gcc_err_desc(struct gk20a *g)
{
	(void)g;
	gcc_err_desc.info_ptr = gcc_ecc_err_desc;
	gcc_err_desc.info_size = nvgpu_safe_cast_u64_to_u32(
			sizeof(gcc_ecc_err_desc) /
			sizeof(struct nvgpu_hw_err_inject_info));

	return &gcc_err_desc;
}
#endif /* CONFIG_NVGPU_INJECT_HWERR */
