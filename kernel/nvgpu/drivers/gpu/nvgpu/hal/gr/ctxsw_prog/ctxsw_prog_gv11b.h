/*
 * Copyright (c) 2018-2019, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef NVGPU_CTXSW_PROG_GV11B_H
#define NVGPU_CTXSW_PROG_GV11B_H

#include <nvgpu/types.h>

void gv11b_ctxsw_prog_set_context_buffer_ptr(struct gk20a *g,
	struct nvgpu_mem *ctx_mem, u64 addr);
void gv11b_ctxsw_prog_set_type_per_veid_header(struct gk20a *g,
	struct nvgpu_mem *ctx_mem);
#ifdef CONFIG_NVGPU_GRAPHICS
void gv11b_ctxsw_prog_set_zcull_ptr(struct gk20a *g, struct nvgpu_mem *ctx_mem,
	u64 addr);
void gv11b_ctxsw_prog_set_full_preemption_ptr(struct gk20a *g,
	struct nvgpu_mem *ctx_mem, u64 addr);
void gv11b_ctxsw_prog_set_full_preemption_ptr_veid0(struct gk20a *g,
	struct nvgpu_mem *ctx_mem, u64 addr);
#endif /* CONFIG_NVGPU_GRAPHICS */
#ifdef CONFIG_NVGPU_DEBUGGER
void gv11b_ctxsw_prog_set_pm_ptr(struct gk20a *g, struct nvgpu_mem *ctx_mem,
	u64 addr);
u32 gv11b_ctxsw_prog_hw_get_pm_mode_stream_out_ctxsw(void);
u32 gv11b_ctxsw_prog_hw_get_perf_counter_register_stride(void);
#endif /* CONFIG_NVGPU_DEBUGGER */

#endif /* NVGPU_CTXSW_PROG_GV11B_H */
