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

#ifdef CONFIG_NVGPU_DEBUGGER
void gv11b_ctxsw_prog_set_pm_ptr(struct gk20a *g, struct nvgpu_mem *ctx_mem,
	u64 addr)
{
	addr = addr >> 8;
	nvgpu_mem_wr(g, ctx_mem, ctxsw_prog_main_image_pm_ptr_o(),
		u64_lo32(addr));
	nvgpu_mem_wr(g, ctx_mem, ctxsw_prog_main_image_pm_ptr_hi_o(),
		u64_hi32(addr));
}

u32 gv11b_ctxsw_prog_hw_get_pm_mode_stream_out_ctxsw(void)
{
	return ctxsw_prog_main_image_pm_mode_stream_out_ctxsw_f();
}

u32 gv11b_ctxsw_prog_hw_get_perf_counter_register_stride(void)
{
	return ctxsw_prog_extended_sm_dsm_perf_counter_register_stride_v();
}
#endif /* CONFIG_NVGPU_DEBUGGER */
