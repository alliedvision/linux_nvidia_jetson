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

#ifndef NVGPU_CTXSW_PROG_GM20B_H
#define NVGPU_CTXSW_PROG_GM20B_H

#include <nvgpu/types.h>

struct gk20a;
struct nvgpu_mem;

u32 gm20b_ctxsw_prog_hw_get_fecs_header_size(void);
u32 gm20b_ctxsw_prog_get_patch_count(struct gk20a *g, struct nvgpu_mem *ctx_mem);
void gm20b_ctxsw_prog_set_patch_count(struct gk20a *g,
	struct nvgpu_mem *ctx_mem, u32 count);
void gm20b_ctxsw_prog_set_patch_addr(struct gk20a *g,
	struct nvgpu_mem *ctx_mem, u64 addr);
#if defined(CONFIG_NVGPU_SET_FALCON_ACCESS_MAP)
void gm20b_ctxsw_prog_set_config_mode_priv_access_map(struct gk20a *g,
	struct nvgpu_mem *ctx_mem, bool allow_all);
void gm20b_ctxsw_prog_set_addr_priv_access_map(struct gk20a *g,
	struct nvgpu_mem *ctx_mem, u64 addr);
#endif
#if defined(CONFIG_NVGPU_HAL_NON_FUSA)
void gm20b_ctxsw_prog_init_ctxsw_hdr_data(struct gk20a *g,
	struct nvgpu_mem *ctx_mem);
void gm20b_ctxsw_prog_disable_verif_features(struct gk20a *g,
	struct nvgpu_mem *ctx_mem);
void gm20b_ctxsw_prog_set_compute_preemption_mode_cta(struct gk20a *g,
	struct nvgpu_mem *ctx_mem);
#endif
#ifdef CONFIG_NVGPU_GRAPHICS
void gm20b_ctxsw_prog_set_zcull_ptr(struct gk20a *g, struct nvgpu_mem *ctx_mem,
	u64 addr);
void gm20b_ctxsw_prog_set_zcull(struct gk20a *g, struct nvgpu_mem *ctx_mem,
	u32 mode);
void gm20b_ctxsw_prog_set_zcull_mode_no_ctxsw(struct gk20a *g,
	struct nvgpu_mem *ctx_mem);
bool gm20b_ctxsw_prog_is_zcull_mode_separate_buffer(u32 mode);
#endif /* CONFIG_NVGPU_GRAPHICS */
#ifdef CONFIG_NVGPU_DEBUGGER

#define  NV_XBAR_MXBAR_PRI_GPC_GNIC_STRIDE 0x20U

u32 gm20b_ctxsw_prog_hw_get_gpccs_header_size(void);
u32 gm20b_ctxsw_prog_hw_get_extended_buffer_segments_size_in_bytes(void);
u32 gm20b_ctxsw_prog_hw_extended_marker_size_in_bytes(void);
u32 gm20b_ctxsw_prog_hw_get_perf_counter_control_register_stride(void);
u32 gm20b_ctxsw_prog_get_main_image_ctx_id(struct gk20a *g, struct nvgpu_mem *ctx_mem);
void gm20b_ctxsw_prog_set_pm_ptr(struct gk20a *g, struct nvgpu_mem *ctx_mem,
	u64 addr);
void gm20b_ctxsw_prog_set_pm_mode(struct gk20a *g,
	struct nvgpu_mem *ctx_mem, u32 mode);
void gm20b_ctxsw_prog_set_pm_smpc_mode(struct gk20a *g,
	struct nvgpu_mem *ctx_mem, bool enable);
u32 gm20b_ctxsw_prog_hw_get_pm_mode_no_ctxsw(void);
u32 gm20b_ctxsw_prog_hw_get_pm_mode_ctxsw(void);
void gm20b_ctxsw_prog_set_cde_enabled(struct gk20a *g,
	struct nvgpu_mem *ctx_mem);
void gm20b_ctxsw_prog_set_pc_sampling(struct gk20a *g,
	struct nvgpu_mem *ctx_mem, bool enable);
bool gm20b_ctxsw_prog_check_main_image_header_magic(u32 *context);
bool gm20b_ctxsw_prog_check_local_header_magic(u32 *context);
u32 gm20b_ctxsw_prog_get_num_gpcs(u32 *context);
u32 gm20b_ctxsw_prog_get_num_tpcs(u32 *context);
void gm20b_ctxsw_prog_get_extended_buffer_size_offset(u32 *context,
	u32 *size, u32 *offset);
void gm20b_ctxsw_prog_get_ppc_info(u32 *context, u32 *num_ppcs, u32 *ppc_mask);
u32 gm20b_ctxsw_prog_get_local_priv_register_ctl_offset(u32 *context);
u32 gm20b_ctxsw_prog_hw_get_pm_gpc_gnic_stride(struct gk20a *g);
#endif /* CONFIG_NVGPU_DEBUGGER */
#ifdef CONFIG_NVGPU_FECS_TRACE
u32 gm20b_ctxsw_prog_hw_get_ts_tag_invalid_timestamp(void);
u32 gm20b_ctxsw_prog_hw_get_ts_tag(u64 ts);
u64 gm20b_ctxsw_prog_hw_record_ts_timestamp(u64 ts);
u32 gm20b_ctxsw_prog_hw_get_ts_record_size_in_bytes(void);
bool gm20b_ctxsw_prog_is_ts_valid_record(u32 magic_hi);
u32 gm20b_ctxsw_prog_get_ts_buffer_aperture_mask(struct gk20a *g,
	struct nvgpu_mem *ctx_mem);
void gm20b_ctxsw_prog_set_ts_num_records(struct gk20a *g,
	struct nvgpu_mem *ctx_mem, u32 num);
void gm20b_ctxsw_prog_set_ts_buffer_ptr(struct gk20a *g,
	struct nvgpu_mem *ctx_mem, u64 addr, u32 aperture_mask);
#endif

#endif /* NVGPU_CTXSW_PROG_GM20B_H */
