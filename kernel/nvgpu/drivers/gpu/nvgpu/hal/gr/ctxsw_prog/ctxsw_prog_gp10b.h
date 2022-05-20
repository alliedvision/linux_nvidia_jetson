/*
 * Copyright (c) 2018-2021, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef NVGPU_CTXSW_PROG_GP10B_H
#define NVGPU_CTXSW_PROG_GP10B_H

#include <nvgpu/types.h>

struct gk20a;
struct nvgpu_mem;

void gp10b_ctxsw_prog_set_compute_preemption_mode_cta(struct gk20a *g,
	struct nvgpu_mem *ctx_mem);
#ifdef CONFIG_NVGPU_HAL_NON_FUSA
void gp10b_ctxsw_prog_init_ctxsw_hdr_data(struct gk20a *g,
	struct nvgpu_mem *ctx_mem);
#endif
#ifdef CONFIG_NVGPU_GFXP
void gp10b_ctxsw_prog_set_graphics_preemption_mode_gfxp(struct gk20a *g,
	struct nvgpu_mem *ctx_mem);
void gp10b_ctxsw_prog_set_full_preemption_ptr(struct gk20a *g,
	struct nvgpu_mem *ctx_mem, u64 addr);
#endif /* CONFIG_NVGPU_GFXP */
#ifdef CONFIG_NVGPU_CILP
void gp10b_ctxsw_prog_set_compute_preemption_mode_cilp(struct gk20a *g,
	struct nvgpu_mem *ctx_mem);
#endif
#ifdef CONFIG_NVGPU_DEBUGGER
void gp10b_ctxsw_prog_set_pmu_options_boost_clock_frequencies(struct gk20a *g,
	struct nvgpu_mem *ctx_mem, u32 boosted_ctx);
#endif /* CONFIG_NVGPU_DEBUGGER */
#ifdef CONFIG_DEBUG_FS
void gp10b_ctxsw_prog_dump_ctxsw_stats(struct gk20a *g,
	struct nvgpu_mem *ctx_mem);
#endif

#endif /* NVGPU_CTXSW_PROG_GP10B_H */
