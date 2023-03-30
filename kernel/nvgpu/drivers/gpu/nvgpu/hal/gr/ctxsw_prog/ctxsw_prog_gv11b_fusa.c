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
#include "ctxsw_prog_gv11b.h"

#include <nvgpu/hw/gv11b/hw_ctxsw_prog_gv11b.h>

void gv11b_ctxsw_prog_set_context_buffer_ptr(struct gk20a *g,
	struct nvgpu_mem *ctx_mem, u64 addr)
{
	nvgpu_mem_wr(g, ctx_mem,
		ctxsw_prog_main_image_context_buffer_ptr_hi_o(),
		u64_hi32(addr));
	nvgpu_mem_wr(g, ctx_mem,
		ctxsw_prog_main_image_context_buffer_ptr_o(),
		u64_lo32(addr));
}

void gv11b_ctxsw_prog_set_type_per_veid_header(struct gk20a *g,
	struct nvgpu_mem *ctx_mem)
{
	nvgpu_mem_wr(g, ctx_mem,
		ctxsw_prog_main_image_ctl_o(),
		ctxsw_prog_main_image_ctl_type_per_veid_header_v());
}

#ifdef CONFIG_NVGPU_GRAPHICS
void gv11b_ctxsw_prog_set_zcull_ptr(struct gk20a *g, struct nvgpu_mem *ctx_mem,
	u64 addr)
{
	addr = addr >> 8;
	nvgpu_mem_wr(g, ctx_mem, ctxsw_prog_main_image_zcull_ptr_o(),
		u64_lo32(addr));
	nvgpu_mem_wr(g, ctx_mem, ctxsw_prog_main_image_zcull_ptr_hi_o(),
		u64_hi32(addr));
}
#endif /* CONFIG_NVGPU_GRAPHICS */

#ifdef CONFIG_NVGPU_GFXP
void gv11b_ctxsw_prog_set_full_preemption_ptr(struct gk20a *g,
	struct nvgpu_mem *ctx_mem, u64 addr)
{
	addr = addr >> 8;
	nvgpu_mem_wr(g, ctx_mem,
		ctxsw_prog_main_image_full_preemption_ptr_o(),
		u64_lo32(addr));
	nvgpu_mem_wr(g, ctx_mem,
		ctxsw_prog_main_image_full_preemption_ptr_hi_o(),
		u64_hi32(addr));
}

void gv11b_ctxsw_prog_set_full_preemption_ptr_veid0(struct gk20a *g,
	struct nvgpu_mem *ctx_mem, u64 addr)
{
	addr = addr >> 8;
	nvgpu_mem_wr(g, ctx_mem,
		ctxsw_prog_main_image_full_preemption_ptr_veid0_o(),
		u64_lo32(addr));
	nvgpu_mem_wr(g, ctx_mem,
		ctxsw_prog_main_image_full_preemption_ptr_veid0_hi_o(),
		u64_hi32(addr));
}
#endif /* CONFIG_NVGPU_GFXP */
