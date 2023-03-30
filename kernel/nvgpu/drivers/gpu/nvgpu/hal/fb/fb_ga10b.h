/*
 * GA10B FB
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

#ifndef NVGPU_FB_GA10B_H
#define NVGPU_FB_GA10B_H

#define VPR_INFO_FETCH_POLL_MS	5U
#define ALIGN_HI32(x)	nvgpu_safe_sub_u32(32U, (x))

struct gk20a;

void ga10b_fb_init_fs_state(struct gk20a *g);
void ga10b_fb_init_hw(struct gk20a *g);
u32 ga10b_fb_get_num_active_ltcs(struct gk20a *g);
void ga10b_fb_dump_vpr_info(struct gk20a *g);
void ga10b_fb_dump_wpr_info(struct gk20a *g);
void ga10b_fb_read_wpr_info(struct gk20a *g, u64 *wpr_base, u64 *wpr_size);
int ga10b_fb_vpr_info_fetch(struct gk20a *g);

#ifdef CONFIG_NVGPU_COMPRESSION
void ga10b_fb_cbc_configure(struct gk20a *g, struct nvgpu_cbc *cbc);
#endif

#ifdef CONFIG_NVGPU_MIG
int ga10b_fb_config_veid_smc_map(struct gk20a *g, bool enable);
int ga10b_fb_set_smc_eng_config(struct gk20a *g, bool enable);
int ga10b_fb_set_remote_swizid(struct gk20a *g, bool enable);
#endif

int ga10b_fb_set_atomic_mode(struct gk20a *g);

#endif /* NVGPU_FB_GA10B_H */
