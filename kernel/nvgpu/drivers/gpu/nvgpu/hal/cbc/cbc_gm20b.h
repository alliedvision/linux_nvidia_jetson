/*
 * GM20B CBC
 *
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

#ifndef NVGPU_CBC_GM20B
#define NVGPU_CBC_GM20B

#ifdef CONFIG_NVGPU_COMPRESSION

#include <nvgpu/types.h>

struct gk20a;
struct gpu_ops;
struct nvgpu_cbc;
enum nvgpu_cbc_op;

int gm20b_cbc_alloc_comptags(struct gk20a *g, struct nvgpu_cbc *cbc);
void gm20b_cbc_init(struct gk20a *g, struct nvgpu_cbc *cbc, bool is_resume);
int gm20b_cbc_ctrl(struct gk20a *g, enum nvgpu_cbc_op op,
		       u32 min, u32 max);
u32 gm20b_cbc_fix_config(struct gk20a *g, int base);
int gm20b_cbc_alloc_phys(struct gk20a *g,
			     size_t compbit_backing_size);
int gm20b_cbc_alloc_virtc(struct gk20a *g,
			     size_t compbit_backing_size);

#endif
#endif
