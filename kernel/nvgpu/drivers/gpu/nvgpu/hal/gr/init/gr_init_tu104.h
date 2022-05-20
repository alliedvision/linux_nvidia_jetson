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

#ifndef NVGPU_GR_INIT_TU104_H
#define NVGPU_GR_INIT_TU104_H

#include <nvgpu/types.h>

struct gk20a;
struct nvgpu_gr_ctx;
struct netlist_av64_list;

u32 tu104_gr_init_get_bundle_cb_default_size(struct gk20a *g);
u32 tu104_gr_init_get_min_gpm_fifo_depth(struct gk20a *g);
u32 tu104_gr_init_get_bundle_cb_token_limit(struct gk20a *g);
u32 tu104_gr_init_get_attrib_cb_default_size(struct gk20a *g);
u32 tu104_gr_init_get_alpha_cb_default_size(struct gk20a *g);

int tu104_gr_init_load_sw_bundle64(struct gk20a *g,
		struct netlist_av64_list *sw_bundle64_init);

#ifdef CONFIG_NVGPU_GRAPHICS
u32 tu104_gr_init_get_rtv_cb_size(struct gk20a *g);
void tu104_gr_init_commit_rtv_cb(struct gk20a *g, u64 addr,
	struct nvgpu_gr_ctx *gr_ctx, bool patch);

void tu104_gr_init_commit_gfxp_rtv_cb(struct gk20a *g,
	struct nvgpu_gr_ctx *gr_ctx, bool patch);

u32 tu104_gr_init_get_attrib_cb_gfxp_default_size(struct gk20a *g);
u32 tu104_gr_init_get_attrib_cb_gfxp_size(struct gk20a *g);
u32 tu104_gr_init_get_ctx_spill_size(struct gk20a *g);
u32 tu104_gr_init_get_ctx_betacb_size(struct gk20a *g);

u32 tu104_gr_init_get_gfxp_rtv_cb_size(struct gk20a *g);
#endif /* CONFIG_NVGPU_GRAPHICS */

#endif /* NVGPU_GR_INIT_TU104_H */
