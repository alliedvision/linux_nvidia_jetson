/*
 * Copyright (c) 2021-2022, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef HAL_FB_VAB_GA10B_H
#define HAL_FB_VAB_GA10B_H

struct gk20a;
struct nvgpu_vab_range_checker;

int ga10b_fb_vab_init(struct gk20a *g);
void ga10b_fb_vab_set_vab_buffer_address(struct gk20a *g, u64 buf_addr);
int ga10b_fb_vab_reserve(struct gk20a *g, u32 vab_mode, u32 num_range_checkers,
	struct nvgpu_vab_range_checker *vab_range_checker);
int ga10b_fb_vab_dump_and_clear(struct gk20a *g, u8 *user_buf,
	u64 user_buf_size);
int ga10b_fb_vab_release(struct gk20a *g);
int ga10b_fb_vab_teardown(struct gk20a *g);
void ga10b_fb_vab_recover(struct gk20a *g);

#endif /* HAL_FB_VAB_GA10B_H */
