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

void gm20b_ctxsw_prog_set_compute_preemption_mode_cta(struct gk20a *g,
	struct nvgpu_mem *ctx_mem)
{
	nvgpu_mem_wr(g, ctx_mem,
		ctxsw_prog_main_image_preemption_options_o(),
		ctxsw_prog_main_image_preemption_options_control_cta_enabled_f());
}

void gm20b_ctxsw_prog_set_config_mode_priv_access_map(struct gk20a *g,
	struct nvgpu_mem *ctx_mem, bool allow_all)
{
	if (allow_all) {
		nvgpu_mem_wr(g, ctx_mem,
			ctxsw_prog_main_image_priv_access_map_config_o(),
			ctxsw_prog_main_image_priv_access_map_config_mode_allow_all_f());
	} else {
		nvgpu_mem_wr(g, ctx_mem,
			ctxsw_prog_main_image_priv_access_map_config_o(),
			ctxsw_prog_main_image_priv_access_map_config_mode_use_map_f());
	}
}

void gm20b_ctxsw_prog_set_addr_priv_access_map(struct gk20a *g,
	struct nvgpu_mem *ctx_mem, u64 addr)
{
	nvgpu_mem_wr(g, ctx_mem,
		ctxsw_prog_main_image_priv_access_map_addr_lo_o(),
		u64_lo32(addr));
	nvgpu_mem_wr(g, ctx_mem,
		ctxsw_prog_main_image_priv_access_map_addr_hi_o(),
		u64_hi32(addr));
}

#ifdef CONFIG_NVGPU_FECS_TRACE
u32 gm20b_ctxsw_prog_hw_get_ts_tag_invalid_timestamp(void)
{
	return ctxsw_prog_record_timestamp_timestamp_hi_tag_invalid_timestamp_v();
}

u32 gm20b_ctxsw_prog_hw_get_ts_tag(u64 ts)
{
	return ctxsw_prog_record_timestamp_timestamp_hi_tag_v(
		nvgpu_safe_cast_u64_to_u32(ts >> 32));
}

u64 gm20b_ctxsw_prog_hw_record_ts_timestamp(u64 ts)
{
	return ts &
	       ~(((u64)ctxsw_prog_record_timestamp_timestamp_hi_tag_m()) << 32);
}

u32 gm20b_ctxsw_prog_hw_get_ts_record_size_in_bytes(void)
{
	return ctxsw_prog_record_timestamp_record_size_in_bytes_v();
}

bool gm20b_ctxsw_prog_is_ts_valid_record(u32 magic_hi)
{
	return magic_hi ==
		ctxsw_prog_record_timestamp_magic_value_hi_v_value_v();
}

u32 gm20b_ctxsw_prog_get_ts_buffer_aperture_mask(struct gk20a *g,
	struct nvgpu_mem *ctx_mem)
{
	return nvgpu_aperture_mask(g, ctx_mem,
		ctxsw_prog_main_image_context_timestamp_buffer_ptr_hi_target_sys_mem_noncoherent_f(),
		ctxsw_prog_main_image_context_timestamp_buffer_ptr_hi_target_sys_mem_coherent_f(),
		ctxsw_prog_main_image_context_timestamp_buffer_ptr_hi_target_vid_mem_f());
}

void gm20b_ctxsw_prog_set_ts_num_records(struct gk20a *g,
	struct nvgpu_mem *ctx_mem, u32 num)
{
	nvgpu_mem_wr(g, ctx_mem,
		ctxsw_prog_main_image_context_timestamp_buffer_control_o(),
		ctxsw_prog_main_image_context_timestamp_buffer_control_num_records_f(num));
}

void gm20b_ctxsw_prog_set_ts_buffer_ptr(struct gk20a *g,
	struct nvgpu_mem *ctx_mem, u64 addr, u32 aperture_mask)
{
	nvgpu_mem_wr(g, ctx_mem,
		ctxsw_prog_main_image_context_timestamp_buffer_ptr_o(),
		u64_lo32(addr));
	nvgpu_mem_wr(g, ctx_mem,
		ctxsw_prog_main_image_context_timestamp_buffer_ptr_hi_o(),
		ctxsw_prog_main_image_context_timestamp_buffer_ptr_v_f(u64_hi32(addr)) |
		aperture_mask);
}
#endif

#ifdef CONFIG_NVGPU_HAL_NON_FUSA
void gm20b_ctxsw_prog_init_ctxsw_hdr_data(struct gk20a *g,
	struct nvgpu_mem *ctx_mem)
{
	nvgpu_mem_wr(g, ctx_mem,
		ctxsw_prog_main_image_num_save_ops_o(), 0);
	nvgpu_mem_wr(g, ctx_mem,
		ctxsw_prog_main_image_num_restore_ops_o(), 0);
}

void gm20b_ctxsw_prog_disable_verif_features(struct gk20a *g,
	struct nvgpu_mem *ctx_mem)
{
	u32 data;

	data = nvgpu_mem_rd(g, ctx_mem, ctxsw_prog_main_image_misc_options_o());

	data = data & ~ctxsw_prog_main_image_misc_options_verif_features_m();
	data = data | ctxsw_prog_main_image_misc_options_verif_features_disabled_f();

	nvgpu_mem_wr(g, ctx_mem, ctxsw_prog_main_image_misc_options_o(), data);
}
#endif
