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

#ifndef NVGPU_GR_INIT_GM20B_H
#define NVGPU_GR_INIT_GM20B_H

#include <nvgpu/types.h>

#ifndef GR_GO_IDLE_BUNDLE
#define GR_GO_IDLE_BUNDLE	0x0000e100U /* --V-B */
#endif

#define GR_PIPE_MODE_BUNDLE		0x1000U
#define GR_PIPE_MODE_MAJOR_COMPUTE	0x00000008U

struct gk20a;
struct nvgpu_gr_ctx;
struct nvgpu_gr_config;
struct netlist_av_list;
struct nvgpu_gr_config;

void gm20b_gr_init_lg_coalesce(struct gk20a *g, u32 data);
void gm20b_gr_init_su_coalesce(struct gk20a *g, u32 data);
void gm20b_gr_init_pes_vsc_stream(struct gk20a *g);

void gm20b_gr_init_fifo_access(struct gk20a *g, bool enable);
void gm20b_gr_init_pd_tpc_per_gpc(struct gk20a *g,
			      struct nvgpu_gr_config *gr_config);
void gm20b_gr_init_pd_skip_table_gpc(struct gk20a *g,
			      struct nvgpu_gr_config *gr_config);
void gm20b_gr_init_cwd_gpcs_tpcs_num(struct gk20a *g,
				     u32 gpc_count, u32 tpc_count);
void gm20b_gr_init_load_tpc_mask(struct gk20a *g,
				struct nvgpu_gr_config *gr_config);
int gm20b_gr_init_wait_idle(struct gk20a *g);
int gm20b_gr_init_wait_fe_idle(struct gk20a *g);
int gm20b_gr_init_fe_pwr_mode_force_on(struct gk20a *g, bool force_on);
void gm20b_gr_init_override_context_reset(struct gk20a *g);
#ifdef CONFIG_NVGPU_HAL_NON_FUSA
void gm20b_gr_init_fe_go_idle_timeout(struct gk20a *g, bool enable);
#endif
void gm20b_gr_init_pipe_mode_override(struct gk20a *g, bool enable);
void gm20b_gr_init_load_method_init(struct gk20a *g,
		struct netlist_av_list *sw_method_init);

#ifndef CONFIG_NVGPU_GR_GOLDEN_CTX_VERIFICATION
int gm20b_gr_init_load_sw_bundle_init(struct gk20a *g,
		struct netlist_av_list *sw_bundle_init);
#endif

u32 gm20b_gr_init_get_global_ctx_cb_buffer_size(struct gk20a *g);
u32 gm20b_gr_init_get_global_ctx_pagepool_buffer_size(struct gk20a *g);

void gm20b_gr_init_commit_global_attrib_cb(struct gk20a *g,
	struct nvgpu_gr_ctx *gr_ctx, u32 tpc_count, u32 max_tpc, u64 addr,
	bool patch);

u32 gm20b_gr_init_get_patch_slots(struct gk20a *g,
	struct nvgpu_gr_config *config);

bool gm20b_gr_init_is_allowed_sw_bundle(struct gk20a *g,
	u32 bundle_addr, u32 bundle_value, int *context);

#ifdef CONFIG_NVGPU_HAL_NON_FUSA
void gm20b_gr_init_gpc_mmu(struct gk20a *g);
#ifdef CONFIG_NVGPU_SET_FALCON_ACCESS_MAP
void gm20b_gr_init_get_access_map(struct gk20a *g,
				   u32 **whitelist, u32 *num_entries);
#endif
void gm20b_gr_init_sm_id_numbering(struct gk20a *g, u32 gpc, u32 tpc, u32 smid,
				struct nvgpu_gr_config *gr_config,
				struct nvgpu_gr_ctx *gr_ctx,
				bool patch);
u32 gm20b_gr_init_get_sm_id_size(void);
int gm20b_gr_init_sm_id_config(struct gk20a *g, u32 *tpc_sm_id,
				struct nvgpu_gr_config *gr_config,
				struct nvgpu_gr_ctx *gr_ctx,
				bool patch);
void gm20b_gr_init_tpc_mask(struct gk20a *g, u32 gpc_index, u32 pes_tpc_mask);
void gm20b_gr_init_fs_state(struct gk20a *g);
void gm20b_gr_init_commit_global_timeslice(struct gk20a *g);

u32 gm20b_gr_init_get_bundle_cb_default_size(struct gk20a *g);
u32 gm20b_gr_init_get_min_gpm_fifo_depth(struct gk20a *g);
u32 gm20b_gr_init_get_bundle_cb_token_limit(struct gk20a *g);
u32 gm20b_gr_init_get_attrib_cb_default_size(struct gk20a *g);
u32 gm20b_gr_init_get_alpha_cb_default_size(struct gk20a *g);
u32 gm20b_gr_init_get_attrib_cb_size(struct gk20a *g, u32 tpc_count);
u32 gm20b_gr_init_get_alpha_cb_size(struct gk20a *g, u32 tpc_count);
u32 gm20b_gr_init_get_global_attr_cb_size(struct gk20a *g, u32 tpc_count,
	u32 max_tpc);

void gm20b_gr_init_commit_global_bundle_cb(struct gk20a *g,
	struct nvgpu_gr_ctx *gr_ctx, u64 addr, u32 size, bool patch);
u32 gm20b_gr_init_pagepool_default_size(struct gk20a *g);
void gm20b_gr_init_commit_global_pagepool(struct gk20a *g,
	struct nvgpu_gr_ctx *gr_ctx, u64 addr, size_t size, bool patch,
	bool global_ctx);
void gm20b_gr_init_commit_global_cb_manager(struct gk20a *g,
	struct nvgpu_gr_config *config, struct nvgpu_gr_ctx *gr_ctx,
	bool patch);

void gm20b_gr_init_detect_sm_arch(struct gk20a *g);

void gm20b_gr_init_get_supported_preemption_modes(
	u32 *graphics_preemption_mode_flags, u32 *compute_preemption_mode_flags);
void gm20b_gr_init_get_default_preemption_modes(
	u32 *default_graphics_preempt_mode, u32 *default_compute_preempt_mode);

#ifdef CONFIG_NVGPU_GRAPHICS
void gm20b_gr_init_rop_mapping(struct gk20a *g,
			      struct nvgpu_gr_config *gr_config);
#endif
#endif /* CONFIG_NVGPU_HAL_NON_FUSA */

#endif /* NVGPU_GR_INIT_GM20B_H */
