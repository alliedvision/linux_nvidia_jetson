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

#include <nvgpu/gk20a.h>
#include <nvgpu/utils.h>
#include <nvgpu/nvgpu_mem.h>

#include "ctxsw_prog_gm20b.h"
#include "ctxsw_prog_gp10b.h"

#include <nvgpu/hw/gp10b/hw_ctxsw_prog_gp10b.h>

void gp10b_ctxsw_prog_set_compute_preemption_mode_cta(struct gk20a *g,
	struct nvgpu_mem *ctx_mem)
{
	nvgpu_mem_wr(g, ctx_mem,
		ctxsw_prog_main_image_compute_preemption_options_o(),
#if defined(CONFIG_NVGPU_CTXSW_FW_ERROR_HEADER_TESTING)
		ctxsw_prog_main_image_compute_preemption_options_control_cilp_f());
#else
		ctxsw_prog_main_image_compute_preemption_options_control_cta_f());
#endif
}

#ifdef CONFIG_NVGPU_GFXP
void gp10b_ctxsw_prog_set_graphics_preemption_mode_gfxp(struct gk20a *g,
	struct nvgpu_mem *ctx_mem)
{
	nvgpu_mem_wr(g, ctx_mem,
		ctxsw_prog_main_image_graphics_preemption_options_o(),
		ctxsw_prog_main_image_graphics_preemption_options_control_gfxp_f());
}

void gp10b_ctxsw_prog_set_full_preemption_ptr(struct gk20a *g,
	struct nvgpu_mem *ctx_mem, u64 addr)
{
	addr = addr >> 8;
	nvgpu_mem_wr(g, ctx_mem,
		ctxsw_prog_main_image_full_preemption_ptr_o(), u64_lo32(addr));
}
#endif /* CONFIG_NVGPU_GFXP */
