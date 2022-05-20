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
#include <nvgpu/static_analysis.h>

#include "ctxsw_prog_gm20b.h"

#include <nvgpu/hw/gm20b/hw_ctxsw_prog_gm20b.h>

u32 gm20b_ctxsw_prog_hw_get_fecs_header_size(void)
{
	return ctxsw_prog_fecs_header_v();
}

u32 gm20b_ctxsw_prog_get_patch_count(struct gk20a *g, struct nvgpu_mem *ctx_mem)
{
	return nvgpu_mem_rd(g, ctx_mem, ctxsw_prog_main_image_patch_count_o());
}

void gm20b_ctxsw_prog_set_patch_count(struct gk20a *g,
	struct nvgpu_mem *ctx_mem, u32 count)
{
	nvgpu_mem_wr(g, ctx_mem, ctxsw_prog_main_image_patch_count_o(), count);
}

void gm20b_ctxsw_prog_set_patch_addr(struct gk20a *g,
	struct nvgpu_mem *ctx_mem, u64 addr)
{
	nvgpu_mem_wr(g, ctx_mem,
		ctxsw_prog_main_image_patch_adr_lo_o(), u64_lo32(addr));
	nvgpu_mem_wr(g, ctx_mem,
		ctxsw_prog_main_image_patch_adr_hi_o(), u64_hi32(addr));
}

#ifdef CONFIG_NVGPU_GRAPHICS
void gm20b_ctxsw_prog_set_zcull_ptr(struct gk20a *g, struct nvgpu_mem *ctx_mem,
	u64 addr)
{
	addr = addr >> 8;
	nvgpu_mem_wr(g, ctx_mem, ctxsw_prog_main_image_zcull_ptr_o(),
		u64_lo32(addr));
}

void gm20b_ctxsw_prog_set_zcull(struct gk20a *g, struct nvgpu_mem *ctx_mem,
	u32 mode)
{
	nvgpu_mem_wr(g, ctx_mem, ctxsw_prog_main_image_zcull_o(), mode);
}

void gm20b_ctxsw_prog_set_zcull_mode_no_ctxsw(struct gk20a *g,
	struct nvgpu_mem *ctx_mem)
{
	nvgpu_mem_wr(g, ctx_mem, ctxsw_prog_main_image_zcull_o(),
			ctxsw_prog_main_image_zcull_mode_no_ctxsw_v());
}

bool gm20b_ctxsw_prog_is_zcull_mode_separate_buffer(u32 mode)
{
	return mode == ctxsw_prog_main_image_zcull_mode_separate_buffer_v();
}
#endif /* CONFIG_NVGPU_GRAPHICS */
