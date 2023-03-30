/*
 * Copyright (c) 2020, NVIDIA CORPORATION.  All rights reserved.
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

#include <nvgpu/hw/ga10b/hw_ctxsw_prog_ga10b.h>

#ifdef CONFIG_DEBUG_FS
void ga10b_ctxsw_prog_dump_ctxsw_stats(struct gk20a *g,
	struct nvgpu_mem *ctx_mem)
{
	nvgpu_err(g, "ctxsw_prog_main_image_magic_value_1_o: %x (expect %x)",
		nvgpu_mem_rd(g, ctx_mem,
				ctxsw_prog_main_image_magic_value_1_o()),
		ctxsw_prog_main_image_magic_value_1_v_value_v());

	nvgpu_err(g, "ctxsw_prog_main_image_magic_value_2_o: %x (expect %x)",
		nvgpu_mem_rd(g, ctx_mem,
				ctxsw_prog_main_image_magic_value_2_o()),
		ctxsw_prog_main_image_magic_value_2_v_value_v());

	nvgpu_err(g, "ctxsw_prog_main_image_magic_value_3_o: %x (expect %x)",
		nvgpu_mem_rd(g, ctx_mem,
				ctxsw_prog_main_image_magic_value_3_o()),
		ctxsw_prog_main_image_magic_value_3_v_value_v());

	nvgpu_err(g, "ctxsw_prog_main_image_magic_value_4_o: %x (expect %x)",
		nvgpu_mem_rd(g, ctx_mem,
				ctxsw_prog_main_image_magic_value_4_o()),
		ctxsw_prog_main_image_magic_value_4_v_value_v());

	nvgpu_err(g, "ctxsw_prog_main_image_magic_value_5_o: %x (expect %x)",
		nvgpu_mem_rd(g, ctx_mem,
				ctxsw_prog_main_image_magic_value_5_o()),
		ctxsw_prog_main_image_magic_value_5_v_value_v());

	nvgpu_err(g, "ctxsw_prog_main_image_context_timestamp_buffer_ptr_hi: %x",
		nvgpu_mem_rd(g, ctx_mem,
				ctxsw_prog_main_image_context_timestamp_buffer_ptr_hi_o()));

	nvgpu_err(g, "ctxsw_prog_main_image_context_timestamp_buffer_ptr: %x",
		nvgpu_mem_rd(g, ctx_mem,
				ctxsw_prog_main_image_context_timestamp_buffer_ptr_o()));

	nvgpu_err(g, "ctxsw_prog_main_image_context_timestamp_buffer_control: %x",
		nvgpu_mem_rd(g, ctx_mem,
				ctxsw_prog_main_image_context_timestamp_buffer_control_o()));

	nvgpu_err(g, "NUM_SAVE_OPERATIONS: %d",
		nvgpu_mem_rd(g, ctx_mem,
			ctxsw_prog_main_image_num_save_ops_o()));
	nvgpu_err(g, "WFI_SAVE_OPERATIONS: %d",
		nvgpu_mem_rd(g, ctx_mem,
			ctxsw_prog_main_image_num_wfi_save_ops_o()));
	nvgpu_err(g, "CTA_SAVE_OPERATIONS: %d",
		nvgpu_mem_rd(g, ctx_mem,
			ctxsw_prog_main_image_num_cta_save_ops_o()));
	nvgpu_err(g, "GFXP_SAVE_OPERATIONS: %d",
		nvgpu_mem_rd(g, ctx_mem,
			ctxsw_prog_main_image_num_gfxp_save_ops_o()));
	nvgpu_err(g, "CILP_SAVE_OPERATIONS: %d",
		nvgpu_mem_rd(g, ctx_mem,
			ctxsw_prog_main_image_num_cilp_save_ops_o()));
	nvgpu_err(g,
		"image gfx preemption option (GFXP is 1) %x",
		nvgpu_mem_rd(g, ctx_mem,
			ctxsw_prog_main_image_graphics_preemption_options_o()));
	nvgpu_err(g,
		"image compute preemption option (CTA is 1) %x",
		nvgpu_mem_rd(g, ctx_mem,
			ctxsw_prog_main_image_compute_preemption_options_o()));
}
#endif
