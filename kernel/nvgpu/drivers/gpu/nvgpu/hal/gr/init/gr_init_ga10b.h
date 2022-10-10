/*
 * Copyright (c) 2020-2021, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef NVGPU_GR_INIT_GA10B_H
#define NVGPU_GR_INIT_GA10B_H

#include <nvgpu/types.h>

#define FECS_CTXSW_RESET_DELAY_US 10U

struct gk20a;
struct nvgpu_gr_ctx;
struct nvgpu_gr_config;

#ifdef CONFIG_NVGPU_MIG
struct nvgpu_gr_gfx_reg_range {
	u32 start_addr;
	u32 end_addr;
};
#endif

void ga10b_gr_init_override_context_reset(struct gk20a *g);
void ga10b_gr_init_fe_go_idle_timeout(struct gk20a *g, bool enable);
void ga10b_gr_init_auto_go_idle(struct gk20a *g, bool enable);
void ga10b_gr_init_gpc_mmu(struct gk20a *g);
void ga10b_gr_init_sm_id_numbering(struct gk20a *g, u32 gpc, u32 tpc, u32 smid,
				struct nvgpu_gr_config *gr_config,
				struct nvgpu_gr_ctx *gr_ctx,
				bool patch);

void ga10b_gr_init_commit_global_bundle_cb(struct gk20a *g,
	struct nvgpu_gr_ctx *gr_ctx, u64 addr, u32 size, bool patch);

u32 ga10b_gr_init_get_min_gpm_fifo_depth(struct gk20a *g);
u32 ga10b_gr_init_get_bundle_cb_token_limit(struct gk20a *g);
u32 ga10b_gr_init_get_attrib_cb_default_size(struct gk20a *g);

#ifdef CONFIG_NVGPU_GRAPHICS

u32 ga10b_gr_init_get_attrib_cb_gfxp_default_size(struct gk20a *g);
u32 ga10b_gr_init_get_attrib_cb_gfxp_size(struct gk20a *g);

u32 ga10b_gr_init_get_ctx_spill_size(struct gk20a *g);
u32 ga10b_gr_init_get_ctx_betacb_size(struct gk20a *g);
void ga10b_gr_init_commit_rops_crop_override(struct gk20a *g,
			struct nvgpu_gr_ctx *gr_ctx, bool patch);

#endif /* CONFIG_NVGPU_GRAPHICS */

#ifdef CONFIG_NVGPU_SET_FALCON_ACCESS_MAP
void ga10b_gr_init_get_access_map(struct gk20a *g,
					u32 **whitelist, u32 *num_entries);
#endif

void ga10b_gr_init_fs_state(struct gk20a *g);
void ga10b_gr_init_commit_global_timeslice(struct gk20a *g);

int ga10b_gr_init_wait_idle(struct gk20a *g);
void ga10b_gr_init_eng_config(struct gk20a *g);
int ga10b_gr_init_reset_gpcs(struct gk20a *g);
int ga10b_gr_init_wait_empty(struct gk20a *g);
#ifndef CONFIG_NVGPU_NON_FUSA
void ga10b_gr_init_set_default_compute_regs(struct gk20a *g,
		struct nvgpu_gr_ctx *gr_ctx);
#endif
#ifdef CONFIG_NVGPU_MIG
bool ga10b_gr_init_is_allowed_reg(struct gk20a *g, u32 addr);
#endif
#endif /* NVGPU_GR_INIT_GA10B_H */
