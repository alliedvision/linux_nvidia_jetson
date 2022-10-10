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

#ifndef NVGPU_TOP_GA10B_H
#define NVGPU_TOP_GA10B_H

#include <nvgpu/types.h>

struct gk20a;

u32 ga10b_get_num_engine_type_entries(struct gk20a *g, u32 engine_type);
bool ga10b_is_engine_gr(struct gk20a *g, u32 engine_type);
bool ga10b_is_engine_ce(struct gk20a *g, u32 engine_type);

struct nvgpu_device *ga10b_top_parse_next_dev(struct gk20a *g, u32 *i);

u32 ga10b_top_get_max_rop_per_gpc(struct gk20a *g);

#endif /* NVGPU_TOP_GA10B_H */
