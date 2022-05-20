/*
 * Copyright (c) 2019-2020, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef NVGPU_GR_ZBC_GM20B_H
#define NVGPU_GR_ZBC_GM20B_H

#include <nvgpu/types.h>

struct gk20a;
struct nvgpu_gr_zbc_table_indices;
struct nvgpu_gr_zbc_entry;

void gm20b_gr_zbc_init_table_indices(struct gk20a *g,
			struct nvgpu_gr_zbc_table_indices *zbc_indices);
void gm20b_gr_zbc_add_color(struct gk20a *g,
			struct nvgpu_gr_zbc_entry *color_val, u32 index);
void gm20b_gr_zbc_add_depth(struct gk20a *g,
			struct nvgpu_gr_zbc_entry *depth_val, u32 index);
#endif /* NVGPU_GR_ZBC_GM20B_H */
