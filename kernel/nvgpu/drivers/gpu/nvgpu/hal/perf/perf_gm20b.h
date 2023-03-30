/*
 * Copyright (c) 2018-2020, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef NVGPU_GM20B_PERF
#define NVGPU_GM20B_PERF

#ifdef CONFIG_NVGPU_DEBUGGER

#include <nvgpu/types.h>

struct gk20a;
struct nvgpu_mem;

bool gm20b_perf_get_membuf_overflow_status(struct gk20a *g);
u32 gm20b_perf_get_membuf_pending_bytes(struct gk20a *g);
void gm20b_perf_set_membuf_handled_bytes(struct gk20a *g,
	u32 entries, u32 entry_size);

void gm20b_perf_membuf_reset_streaming(struct gk20a *g);

void gm20b_perf_enable_membuf(struct gk20a *g, u32 size, u64 buf_addr);
void gm20b_perf_disable_membuf(struct gk20a *g);

void gm20b_perf_init_inst_block(struct gk20a *g, struct nvgpu_mem *inst_block);
void gm20b_perf_deinit_inst_block(struct gk20a *g);

u32 gm20b_perf_get_pmmsys_per_chiplet_offset(void);
u32 gm20b_perf_get_pmmgpc_per_chiplet_offset(void);
u32 gm20b_perf_get_pmmfbp_per_chiplet_offset(void);

#endif /* CONFIG_NVGPU_DEBUGGER */
#endif
