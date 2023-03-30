/*
 * Copyright (c) 2019-2021, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef NVGPU_GR_INIT_GP10B_H
#define NVGPU_GR_INIT_GP10B_H

#include <nvgpu/types.h>

struct gk20a;
struct nvgpu_gr_ctx;
struct nvgpu_gr_config;

u32 gp10b_gr_init_get_sm_id_size(void);
int gp10b_gr_init_wait_empty(struct gk20a *g);

void gp10b_gr_init_commit_global_bundle_cb(struct gk20a *g,
	struct nvgpu_gr_ctx *gr_ctx, u64 addr, u32 size, bool patch);
u32 gp10b_gr_init_pagepool_default_size(struct gk20a *g);
void gp10b_gr_init_commit_global_pagepool(struct gk20a *g,
	struct nvgpu_gr_ctx *gr_ctx, u64 addr, size_t size, bool patch,
	bool global_ctx);
void gp10b_gr_init_commit_global_cb_manager(struct gk20a *g,
	struct nvgpu_gr_config *config, struct nvgpu_gr_ctx *gr_ctx,
	bool patch);

void gp10b_gr_init_get_supported_preemption_modes(
	u32 *graphics_preemption_mode_flags, u32 *compute_preemption_mode_flags);
void gp10b_gr_init_get_default_preemption_modes(
	u32 *default_graphics_preempt_mode, u32 *default_compute_preempt_mode);

#ifdef CONFIG_NVGPU_HAL_NON_FUSA
#ifdef CONFIG_NVGPU_SET_FALCON_ACCESS_MAP
void gp10b_gr_init_get_access_map(struct gk20a *g,
				   u32 **whitelist, u32 *num_entries);
#endif
int gp10b_gr_init_sm_id_config(struct gk20a *g, u32 *tpc_sm_id,
				struct nvgpu_gr_config *gr_config,
				struct nvgpu_gr_ctx *gr_ctx,
				bool patch);
void gp10b_gr_init_fs_state(struct gk20a *g);
int gp10b_gr_init_preemption_state(struct gk20a *g);

u32 gp10b_gr_init_get_attrib_cb_default_size(struct gk20a *g);
u32 gp10b_gr_init_get_alpha_cb_default_size(struct gk20a *g);
u32 gp10b_gr_init_get_attrib_cb_size(struct gk20a *g, u32 tpc_count);
u32 gp10b_gr_init_get_alpha_cb_size(struct gk20a *g, u32 tpc_count);
u32 gp10b_gr_init_get_global_attr_cb_size(struct gk20a *g, u32 tpc_count,
	u32 max_tpc);

void gp10b_gr_init_commit_global_attrib_cb(struct gk20a *g,
	struct nvgpu_gr_ctx *gr_ctx, u32 tpc_count, u32 max_tpc, u64 addr,
	bool patch);

void gp10b_gr_init_commit_cbes_reserve(struct gk20a *g,
	struct nvgpu_gr_ctx *gr_ctx, bool patch);

u32 gp10b_gr_init_get_attrib_cb_gfxp_default_size(struct gk20a *g);
u32 gp10b_gr_init_get_attrib_cb_gfxp_size(struct gk20a *g);

u32 gp10b_gr_init_get_ctx_spill_size(struct gk20a *g);
u32 gp10b_gr_init_get_ctx_betacb_size(struct gk20a *g);

void gp10b_gr_init_commit_ctxsw_spill(struct gk20a *g,
	struct nvgpu_gr_ctx *gr_ctx, u64 addr, u32 size, bool patch);
#endif /* CONFIG_NVGPU_HAL_NON_FUSA */

#ifdef CONFIG_NVGPU_GRAPHICS
u32 gp10b_gr_init_get_ctx_attrib_cb_size(struct gk20a *g, u32 betacb_size,
	u32 tpc_count, u32 max_tpc);
u32 gp10b_gr_init_get_ctx_pagepool_size(struct gk20a *g);
#endif /* CONFIG_NVGPU_GRAPHICS */

#endif /* NVGPU_GR_INIT_GP10B_H */
