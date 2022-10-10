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

#ifndef LTC_GA10B_H
#define LTC_GA10B_H

#include <nvgpu/types.h>

struct gk20a;

#ifdef CONFIG_NVGPU_HAL_NON_FUSA
u32 ga10b_ltc_zbc_table_size(struct gk20a *g);
#endif
#ifdef CONFIG_NVGPU_GRAPHICS
void ga10b_ltc_set_zbc_stencil_entry(struct gk20a *g, u32 stencil_depth,
			u32 index);
void ga10b_ltc_set_zbc_color_entry(struct gk20a *g, u32 *color_l2, u32 index);
void ga10b_ltc_set_zbc_depth_entry(struct gk20a *g, u32 depth_val, u32 index);
#endif

void ga10b_ltc_init_fs_state(struct gk20a *g);
int ga10b_ltc_lts_set_mgmt_setup(struct gk20a *g);
u64 ga10b_determine_L2_size_bytes(struct gk20a *g);
int ga10b_lts_ecc_init(struct gk20a *g);

#ifdef CONFIG_NVGPU_DEBUGGER
u32 ga10b_ltc_pri_shared_addr(struct gk20a *g, u32 addr);
int ga10b_set_l2_max_ways_evict_last(struct gk20a *g, struct nvgpu_tsg *tsg,
		u32 num_ways);
int ga10b_get_l2_max_ways_evict_last(struct gk20a *g, struct nvgpu_tsg *tsg,
		u32 *num_ways);
#endif

#endif /* LTC_GA10B_H */
