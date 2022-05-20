/*
 * GV11B FB ECC
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

#ifndef NVGPU_FB_ECC_GV11B_H
#define NVGPU_FB_ECC_GV11B_H

struct gk20a;

/*
 * @brief Allocate and initialize counters for memories within FB.
 *
 * @param stat [in] Address of pointer to struct nvgpu_ecc_stat.
 *
 */
#define NVGPU_ECC_COUNTER_INIT_FB(stat) \
	nvgpu_ecc_counter_init(g, &g->ecc.fb.stat, #stat)

#define NVGPU_ECC_COUNTER_FREE_FB(stat)	\
	nvgpu_ecc_counter_deinit(g, &g->ecc.fb.stat)

int gv11b_fb_ecc_init(struct gk20a *g);
void gv11b_fb_ecc_free(struct gk20a *g);
void gv11b_fb_ecc_l2tlb_error_mask(u32 *corrected_error_mask,
		u32 *uncorrected_error_mask);

#endif /* NVGPU_FB_ECC_GV11B_H */
