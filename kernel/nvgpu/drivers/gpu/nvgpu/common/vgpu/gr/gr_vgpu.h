/*
 * Copyright (c) 2017-2021, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef NVGPU_GR_VGPU_H
#define NVGPU_GR_VGPU_H

#include <nvgpu/types.h>

struct gk20a;
struct nvgpu_channel;
struct gr_gk20a;
#ifdef CONFIG_NVGPU_GRAPHICS
struct nvgpu_gr_zcull_info;
struct nvgpu_gr_zcull;
struct nvgpu_gr_zbc;
struct nvgpu_gr_zbc_entry;
struct nvgpu_gr_zbc_query_params;
#endif
struct dbg_session_gk20a;
struct nvgpu_tsg;
struct vm_gk20a;
struct nvgpu_gr_ctx;
struct tegra_vgpu_gr_intr_info;
struct tegra_vgpu_sm_esr_info;
struct nvgpu_gr_falcon_query_sizes;

void vgpu_gr_detect_sm_arch(struct gk20a *g);
int vgpu_gr_init_ctx_state(struct gk20a *g,
		struct nvgpu_gr_falcon_query_sizes *sizes);
int vgpu_gr_alloc_global_ctx_buffers(struct gk20a *g);
void vgpu_gr_free_channel_ctx(struct nvgpu_channel *c, bool is_tsg);
void vgpu_gr_free_tsg_ctx(struct nvgpu_tsg *tsg);
int vgpu_gr_alloc_obj_ctx(struct nvgpu_channel  *c, u32 class_num, u32 flags);
u32 vgpu_gr_get_gpc_tpc_mask(struct gk20a *g, struct nvgpu_gr_config *config,
	u32 gpc_index);
u32 vgpu_gr_get_max_fbps_count(struct gk20a *g);
u32 vgpu_gr_get_max_ltc_per_fbp(struct gk20a *g);
u32 vgpu_gr_get_max_lts_per_ltc(struct gk20a *g);
#ifdef CONFIG_NVGPU_GRAPHICS
int vgpu_gr_bind_ctxsw_zcull(struct gk20a *g, struct nvgpu_channel *c,
			u64 zcull_va, u32 mode);
int vgpu_gr_get_zcull_info(struct gk20a *g,
			struct nvgpu_gr_config *gr_config,
			struct nvgpu_gr_zcull *zcull,
			struct nvgpu_gr_zcull_info *zcull_params);
int vgpu_gr_add_zbc(struct gk20a *g, struct nvgpu_gr_zbc *zbc,
			   struct nvgpu_gr_zbc_entry *zbc_val);
int vgpu_gr_query_zbc(struct gk20a *g, struct nvgpu_gr_zbc *zbc,
			struct nvgpu_gr_zbc_query_params *query_params);
#endif
int vgpu_gr_update_smpc_ctxsw_mode(struct gk20a *g,
	struct nvgpu_tsg *tsg, bool enable);
int vgpu_gr_set_sm_debug_mode(struct gk20a *g,
	struct nvgpu_channel *ch, u64 sms, bool enable);
int vgpu_gr_update_hwpm_ctxsw_mode(struct gk20a *g,
	u32 gr_instance_id, struct nvgpu_tsg *tsg, u32 mode);
int vgpu_gr_clear_sm_error_state(struct gk20a *g,
		struct nvgpu_channel *ch, u32 sm_id);
int vgpu_gr_suspend_contexts(struct gk20a *g,
		struct dbg_session_gk20a *dbg_s,
		int *ctx_resident_ch_fd);
int vgpu_gr_resume_contexts(struct gk20a *g,
		struct dbg_session_gk20a *dbg_s,
		int *ctx_resident_ch_fd);
int vgpu_gr_init_sm_id_table(struct gk20a *g,
	 struct nvgpu_gr_config *gr_config);
int vgpu_gr_update_pc_sampling(struct nvgpu_channel *ch, bool enable);
void vgpu_gr_init_cyclestats(struct gk20a *g);
int vgpu_gr_set_preemption_mode(struct nvgpu_channel *ch,
		u32 graphics_preempt_mode, u32 compute_preempt_mode,
		u32 gr_instance_id);
int vgpu_gr_isr(struct gk20a *g, struct tegra_vgpu_gr_intr_info *info);
void vgpu_gr_handle_sm_esr_event(struct gk20a *g,
			struct tegra_vgpu_sm_esr_info *info);
int vgpu_init_gr_support(struct gk20a *g);
u32 vgpu_gr_get_max_gpc_count(struct gk20a *g);
u32 vgpu_gr_get_gpc_count(struct gk20a *g);
u32 vgpu_gr_get_gpc_mask(struct gk20a *g);
#ifdef CONFIG_NVGPU_DEBUGGER
u64 vgpu_gr_gk20a_tpc_enabled_exceptions(struct gk20a *g);
int vgpu_gr_set_mmu_debug_mode(struct gk20a *g,
		struct nvgpu_channel *ch, bool enable);
#endif

#endif /* NVGPU_GR_VGPU_H */
