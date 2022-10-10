/*
 * Copyright (c) 2018-2022, NVIDIA CORPORATION.  All rights reserved.
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

#ifdef CONFIG_NVGPU_DEBUGGER
u32 gm20b_ctxsw_prog_hw_get_gpccs_header_size(void)
{
	return ctxsw_prog_gpccs_header_stride_v();
}

u32 gm20b_ctxsw_prog_hw_get_extended_buffer_segments_size_in_bytes(void)
{
	return ctxsw_prog_extended_buffer_segments_size_in_bytes_v();
}

u32 gm20b_ctxsw_prog_hw_extended_marker_size_in_bytes(void)
{
	return ctxsw_prog_extended_marker_size_in_bytes_v();
}

u32 gm20b_ctxsw_prog_hw_get_perf_counter_control_register_stride(void)
{
	return ctxsw_prog_extended_sm_dsm_perf_counter_control_register_stride_v();
}

u32 gm20b_ctxsw_prog_get_main_image_ctx_id(struct gk20a *g,
	struct nvgpu_mem *ctx_mem)
{
	return nvgpu_mem_rd(g, ctx_mem, ctxsw_prog_main_image_context_id_o());
}

void gm20b_ctxsw_prog_set_pm_ptr(struct gk20a *g, struct nvgpu_mem *ctx_mem,
	u64 addr)
{
	addr = addr >> 8;
	nvgpu_mem_wr(g, ctx_mem, ctxsw_prog_main_image_pm_ptr_o(),
		u64_lo32(addr));
}

void gm20b_ctxsw_prog_set_pm_mode(struct gk20a *g,
	struct nvgpu_mem *ctx_mem, u32 mode)
{
	u32 data;

	data = nvgpu_mem_rd(g, ctx_mem, ctxsw_prog_main_image_pm_o());

	data = data & ~ctxsw_prog_main_image_pm_mode_m();
	data |= mode;

	nvgpu_mem_wr(g, ctx_mem, ctxsw_prog_main_image_pm_o(), data);
}

void gm20b_ctxsw_prog_set_pm_smpc_mode(struct gk20a *g,
	struct nvgpu_mem *ctx_mem, bool enable)
{
	u32 data;

	data = nvgpu_mem_rd(g, ctx_mem, ctxsw_prog_main_image_pm_o());

	data = data & ~ctxsw_prog_main_image_pm_smpc_mode_m();
	data |= enable ?
		ctxsw_prog_main_image_pm_smpc_mode_ctxsw_f() :
		ctxsw_prog_main_image_pm_smpc_mode_no_ctxsw_f();

	nvgpu_mem_wr(g, ctx_mem, ctxsw_prog_main_image_pm_o(), data);
}

u32 gm20b_ctxsw_prog_hw_get_pm_mode_no_ctxsw(void)
{
	return ctxsw_prog_main_image_pm_mode_no_ctxsw_f();
}

u32 gm20b_ctxsw_prog_hw_get_pm_mode_ctxsw(void)
{
	return ctxsw_prog_main_image_pm_mode_ctxsw_f();
}

void gm20b_ctxsw_prog_set_cde_enabled(struct gk20a *g,
	struct nvgpu_mem *ctx_mem)
{
	u32 data = nvgpu_mem_rd(g, ctx_mem, ctxsw_prog_main_image_ctl_o());

	data |=  ctxsw_prog_main_image_ctl_cde_enabled_f();
	nvgpu_mem_wr(g, ctx_mem, ctxsw_prog_main_image_ctl_o(), data);
}

void gm20b_ctxsw_prog_set_pc_sampling(struct gk20a *g,
	struct nvgpu_mem *ctx_mem, bool enable)
{
	u32 data = nvgpu_mem_rd(g, ctx_mem, ctxsw_prog_main_image_pm_o());

	data &= ~ctxsw_prog_main_image_pm_pc_sampling_m();
	data |= ctxsw_prog_main_image_pm_pc_sampling_f(
		nvgpu_safe_cast_bool_to_u32(enable));

	nvgpu_mem_wr(g, ctx_mem, ctxsw_prog_main_image_pm_o(), data);
}

bool gm20b_ctxsw_prog_check_main_image_header_magic(u32 *context)
{
	u32 magic = *(context + (ctxsw_prog_main_image_magic_value_o() >> 2));
	return magic == ctxsw_prog_main_image_magic_value_v_value_v();
}

bool gm20b_ctxsw_prog_check_local_header_magic(u32 *context)
{
	u32 magic = *(context + (ctxsw_prog_local_magic_value_o() >> 2));
	return magic == ctxsw_prog_local_magic_value_v_value_v();
}

u32 gm20b_ctxsw_prog_get_num_gpcs(u32 *context)
{
	return *(context + (ctxsw_prog_main_image_num_gpcs_o() >> 2));
}

u32 gm20b_ctxsw_prog_get_num_tpcs(u32 *context)
{
	return *(context + (ctxsw_prog_local_image_num_tpcs_o() >> 2));
}

void gm20b_ctxsw_prog_get_extended_buffer_size_offset(u32 *context,
	u32 *size, u32 *offset)
{
	u32 data = *(context + (ctxsw_prog_main_extended_buffer_ctl_o() >> 2));

	*size = ctxsw_prog_main_extended_buffer_ctl_size_v(data);
	*offset = ctxsw_prog_main_extended_buffer_ctl_offset_v(data);
}

void gm20b_ctxsw_prog_get_ppc_info(u32 *context, u32 *num_ppcs, u32 *ppc_mask)
{
	u32 data = *(context + (ctxsw_prog_local_image_ppc_info_o() >> 2));

	*num_ppcs = ctxsw_prog_local_image_ppc_info_num_ppcs_v(data);
	*ppc_mask = ctxsw_prog_local_image_ppc_info_ppc_mask_v(data);
}

u32 gm20b_ctxsw_prog_get_local_priv_register_ctl_offset(u32 *context)
{
	u32 data = *(context + (ctxsw_prog_local_priv_register_ctl_o() >> 2));
	return ctxsw_prog_local_priv_register_ctl_offset_v(data);
}

u32 gm20b_ctxsw_prog_hw_get_pm_gpc_gnic_stride(struct gk20a *g)
{
	(void)g;
	return  NV_XBAR_MXBAR_PRI_GPC_GNIC_STRIDE;
}
#endif /* CONFIG_NVGPU_DEBUGGER */
