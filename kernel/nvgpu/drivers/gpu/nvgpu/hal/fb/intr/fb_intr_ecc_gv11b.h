/*
 * GV11B FB INTR ECC
 *
 * Copyright (c) 2016-2020, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef NVGPU_FB_INTR_ECC_GV11B_H
#define NVGPU_FB_INTR_ECC_GV11B_H

#include <nvgpu/types.h>
#include <nvgpu/nvgpu_err.h>

struct gk20a;
struct nvgpu_hw_err_inject_info;
struct nvgpu_hw_err_inject_info_desc;

void gv11b_fb_intr_handle_ecc(struct gk20a *g);
void gv11b_fb_intr_handle_ecc_l2tlb(struct gk20a *g, u32 ecc_status);
void gv11b_fb_intr_handle_ecc_fillunit(struct gk20a *g, u32 ecc_status);
void gv11b_fb_intr_handle_ecc_hubtlb(struct gk20a *g, u32 ecc_status);

#ifdef CONFIG_NVGPU_INJECT_HWERR
struct nvgpu_hw_err_inject_info_desc *
		gv11b_fb_intr_get_hubmmu_err_desc(struct gk20a *g);
void gv11b_fb_intr_inject_hubmmu_ecc_error(struct gk20a *g,
		struct nvgpu_hw_err_inject_info *err, u32 error_info);
#endif

#endif /* NVGPU_FB_INTR_ECC_GV11B_H */
