/*
 * Copyright (c) 2020-2022, NVIDIA CORPORATION.  All rights reserved.
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

#include "ctxsw_prog_ga10b.h"

#include <nvgpu/hw/ga10b/hw_ctxsw_prog_ga10b.h>

#define CTXSWBUF_SEGMENT_BLKSIZE	(256U)

u32 ga10b_ctxsw_prog_hw_get_fecs_header_size(void)
{
	return ctxsw_prog_fecs_header_size_in_bytes_v();
}

#ifdef CONFIG_NVGPU_DEBUGGER
u32 ga10b_ctxsw_prog_hw_get_main_header_size(void)
{
	return ctxsw_prog_ctxsw_header_size_in_bytes_v();
}

u32 ga10b_ctxsw_prog_hw_get_gpccs_header_stride(void)
{
	return ctxsw_prog_gpccs_header_stride_v();
}

u32 ga10b_ctxsw_prog_get_tpc_segment_pri_layout(struct gk20a *g, u32 *main_hdr)
{
	(void)g;
	return ctxsw_prog_main_tpc_segment_pri_layout_v_v(
			main_hdr[ctxsw_prog_main_tpc_segment_pri_layout_o() >>
			BYTE_TO_DW_SHIFT]);
}

u32 ga10b_ctxsw_prog_get_compute_sysreglist_offset(u32 *fecs_hdr)
{
	return ctxsw_prog_local_sys_reglist_offset_compute_v(
		fecs_hdr[ctxsw_prog_local_sys_reglist_offset_o() >>
		BYTE_TO_DW_SHIFT]) * CTXSWBUF_SEGMENT_BLKSIZE;
}

u32 ga10b_ctxsw_prog_get_gfx_sysreglist_offset(u32 *fecs_hdr)
{
	return ctxsw_prog_local_sys_reglist_offset_graphics_v(
		fecs_hdr[ctxsw_prog_local_sys_reglist_offset_o() >>
		BYTE_TO_DW_SHIFT]) * CTXSWBUF_SEGMENT_BLKSIZE;
}

u32 ga10b_ctxsw_prog_get_ltsreglist_offset(u32 *fecs_hdr)
{
	return ctxsw_prog_local_lts_reglist_offset_v_v(
		fecs_hdr[ctxsw_prog_local_lts_reglist_offset_o() >>
		BYTE_TO_DW_SHIFT]) * CTXSWBUF_SEGMENT_BLKSIZE;
}

u32 ga10b_ctxsw_prog_get_compute_gpcreglist_offset(u32 *gpccs_hdr)
{
	return ctxsw_prog_local_gpc_reglist_offset_compute_v(
		gpccs_hdr[ctxsw_prog_local_gpc_reglist_offset_o() >>
		BYTE_TO_DW_SHIFT]) * CTXSWBUF_SEGMENT_BLKSIZE;
}

u32 ga10b_ctxsw_prog_get_gfx_gpcreglist_offset(u32 *gpccs_hdr)
{
	return ctxsw_prog_local_gpc_reglist_offset_graphics_v(
		gpccs_hdr[ctxsw_prog_local_gpc_reglist_offset_o() >>
		BYTE_TO_DW_SHIFT]) * CTXSWBUF_SEGMENT_BLKSIZE;
}

u32 ga10b_ctxsw_prog_get_compute_ppcreglist_offset(u32 *gpccs_hdr)
{
	return ctxsw_prog_local_ppc_reglist_offset_compute_v(
		gpccs_hdr[ctxsw_prog_local_ppc_reglist_offset_o() >>
		BYTE_TO_DW_SHIFT]) * CTXSWBUF_SEGMENT_BLKSIZE;
}

u32 ga10b_ctxsw_prog_get_gfx_ppcreglist_offset(u32 *gpccs_hdr)
{
	return ctxsw_prog_local_ppc_reglist_offset_graphics_v(
		gpccs_hdr[ctxsw_prog_local_ppc_reglist_offset_o() >>
		BYTE_TO_DW_SHIFT]) * CTXSWBUF_SEGMENT_BLKSIZE;
}

u32 ga10b_ctxsw_prog_get_compute_tpcreglist_offset(u32 *gpccs_hdr, u32 tpc_num)
{
	return ctxsw_prog_local_tpc_reglist_offset_compute_v(
		gpccs_hdr[ctxsw_prog_local_tpc_reglist_offset_r(tpc_num) >>
		BYTE_TO_DW_SHIFT]) * CTXSWBUF_SEGMENT_BLKSIZE;
}

u32 ga10b_ctxsw_prog_get_gfx_tpcreglist_offset(u32 *gpccs_hdr, u32 tpc_num)
{
	return ctxsw_prog_local_tpc_reglist_offset_graphics_v(
		gpccs_hdr[ctxsw_prog_local_tpc_reglist_offset_r(tpc_num) >>
		BYTE_TO_DW_SHIFT]) * CTXSWBUF_SEGMENT_BLKSIZE;
}

u32 ga10b_ctxsw_prog_get_compute_etpcreglist_offset(u32 *gpccs_hdr)
{
	return ctxsw_prog_local_ext_tpc_reglist_offset_compute_v(
		gpccs_hdr[ctxsw_prog_local_ext_tpc_reglist_offset_o() >>
		BYTE_TO_DW_SHIFT]) * CTXSWBUF_SEGMENT_BLKSIZE;
}

u32 ga10b_ctxsw_prog_get_gfx_etpcreglist_offset(u32 *gpccs_hdr)
{
	return ctxsw_prog_local_ext_tpc_reglist_offset_graphics_v(
		gpccs_hdr[ctxsw_prog_local_ext_tpc_reglist_offset_o() >>
		BYTE_TO_DW_SHIFT]) * CTXSWBUF_SEGMENT_BLKSIZE;
}
#endif /* CONFIG_NVGPU_DEBUGGER */
