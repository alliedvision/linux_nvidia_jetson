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

#ifndef NVGPU_GR_INIT_GA100_H
#define NVGPU_GR_INIT_GA100_H

#include <nvgpu/types.h>

#define FECS_CTXSW_RESET_DELAY_US 10U

struct gk20a;
struct nvgpu_gr_ctx;

void ga100_gr_init_override_context_reset(struct gk20a *g);
u32 ga100_gr_init_get_min_gpm_fifo_depth(struct gk20a *g);
u32 ga100_gr_init_get_bundle_cb_token_limit(struct gk20a *g);
u32 ga100_gr_init_get_attrib_cb_default_size(struct gk20a *g);
void ga100_gr_init_commit_global_bundle_cb(struct gk20a *g,
	struct nvgpu_gr_ctx *gr_ctx, u64 addr, u32 size, bool patch);
void ga100_gr_init_set_sm_l1tag_surface_collector(struct gk20a *g);
#ifdef CONFIG_NVGPU_GRAPHICS
u32 ga100_gr_init_get_attrib_cb_gfxp_default_size(struct gk20a *g);
u32 ga100_gr_init_get_attrib_cb_gfxp_size(struct gk20a *g);
u32 ga100_gr_init_get_ctx_spill_size(struct gk20a *g);
u32 ga100_gr_init_get_ctx_betacb_size(struct gk20a *g);
#endif

#endif /* NVGPU_GR_INIT_GA100_H */
