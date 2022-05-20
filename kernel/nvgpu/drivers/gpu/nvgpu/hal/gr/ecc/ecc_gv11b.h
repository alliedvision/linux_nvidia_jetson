/*
 * Copyright (c) 2018-2021, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef NVGPU_ECC_GV11B_H
#define NVGPU_ECC_GV11B_H

#include <nvgpu/nvgpu_err.h>
#include <nvgpu/types.h>

struct gk20a;
struct nvgpu_hw_err_inject_info;
struct nvgpu_hw_err_inject_info_desc;

void gv11b_ecc_detect_enabled_units(struct gk20a *g);

int gv11b_gr_gpc_tpc_ecc_init(struct gk20a *g);
int gv11b_gr_fecs_ecc_init(struct gk20a *g);

void gv11b_gr_gpc_tpc_ecc_deinit(struct gk20a *g);
void gv11b_gr_fecs_ecc_deinit(struct gk20a *g);

#ifdef CONFIG_NVGPU_INJECT_HWERR
void gv11b_gr_intr_inject_fecs_ecc_error(struct gk20a *g,
		struct nvgpu_hw_err_inject_info *err, u32 error_info);
struct nvgpu_hw_err_inject_info_desc *
gv11b_gr_intr_get_fecs_err_desc(struct gk20a *g);
void gv11b_gr_intr_inject_gpccs_ecc_error(struct gk20a *g,
		struct nvgpu_hw_err_inject_info *err, u32 error_info);
struct nvgpu_hw_err_inject_info_desc *
gv11b_gr_intr_get_gpccs_err_desc(struct gk20a *g);
void gv11b_gr_intr_inject_sm_ecc_error(struct gk20a *g,
		struct nvgpu_hw_err_inject_info *err, u32 error_info);
struct nvgpu_hw_err_inject_info_desc *
gv11b_gr_intr_get_sm_err_desc(struct gk20a *g);
void gv11b_gr_intr_inject_mmu_ecc_error(struct gk20a *g,
		struct nvgpu_hw_err_inject_info *err, u32 error_info);
struct nvgpu_hw_err_inject_info_desc *
gv11b_gr_intr_get_mmu_err_desc(struct gk20a *g);
void gv11b_gr_intr_inject_gcc_ecc_error(struct gk20a *g,
		struct nvgpu_hw_err_inject_info *err, u32 error_info);
struct nvgpu_hw_err_inject_info_desc *
gv11b_gr_intr_get_gcc_err_desc(struct gk20a *g);
#endif /* CONFIG_NVGPU_INJECT_HWERR */

#endif /* NVGPU_ECC_GV11B_H */
