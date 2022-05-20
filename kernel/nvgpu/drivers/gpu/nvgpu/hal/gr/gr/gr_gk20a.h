/*
 * GK20A Graphics Engine
 *
 * Copyright (c) 2011-2021, NVIDIA CORPORATION.  All rights reserved.
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
#ifndef GR_GK20A_H
#define GR_GK20A_H

#ifdef CONFIG_NVGPU_DEBUGGER

#include <nvgpu/types.h>

struct gk20a;
struct nvgpu_channel;
struct nvgpu_tsg;
struct nvgpu_warpstate;
struct dbg_session_gk20a;
struct nvgpu_dbg_reg_op;
struct nvgpu_gr_ctx;

enum ctxsw_addr_type;

/* sm */
bool gk20a_gr_sm_debugger_attached(struct gk20a *g);
int gr_gk20a_exec_ctx_ops(struct nvgpu_tsg *tsg,
			  struct nvgpu_dbg_reg_op *ctx_ops, u32 num_ops,
			  u32 num_ctx_wr_ops, u32 num_ctx_rd_ops,
			  u32 *flags);
int gr_gk20a_get_ctx_buffer_offsets(struct gk20a *g,
				    u32 addr, u32 max_offsets,
				    u32 *offsets, u32 *offset_addrs,
				    u32 *num_offsets);
int gr_gk20a_get_pm_ctx_buffer_offsets(struct gk20a *g,
				       u32 addr, u32 max_offsets,
				       u32 *offsets, u32 *offset_addrs,
				       u32 *num_offsets);
int gr_gk20a_update_smpc_ctxsw_mode(struct gk20a *g,
				    struct nvgpu_tsg *tsg,
				    bool enable_smpc_ctxsw);
int gr_gk20a_update_hwpm_ctxsw_mode(struct gk20a *g,
				  u32 gr_instance_id,
				  struct nvgpu_tsg *tsg,
				  u32 mode);
int gr_gk20a_ctx_patch_smpc(struct gk20a *g,
			    u32 addr, u32 data,
			    struct nvgpu_gr_ctx *gr_ctx);
void gk20a_gr_resume_single_sm(struct gk20a *g,
				u32 gpc, u32 tpc, u32 sm);
void gk20a_gr_resume_all_sms(struct gk20a *g);
void gk20a_gr_suspend_single_sm(struct gk20a *g,
		u32 gpc, u32 tpc, u32 sm,
		u32 global_esr_mask, bool check_errors);
void gk20a_gr_suspend_all_sms(struct gk20a *g,
		u32 global_esr_mask, bool check_errors);
int gr_gk20a_set_sm_debug_mode(struct gk20a *g,
	struct nvgpu_channel *ch, u64 sms, bool enable);
bool gk20a_is_channel_ctx_resident(struct nvgpu_channel *ch);
bool gk20a_is_tsg_ctx_resident(struct nvgpu_tsg *tsg);
int gk20a_gr_lock_down_sm(struct gk20a *g,
			 u32 gpc, u32 tpc, u32 sm, u32 global_esr_mask,
			 bool check_errors);
int gk20a_gr_wait_for_sm_lock_down(struct gk20a *g, u32 gpc, u32 tpc, u32 sm,
		u32 global_esr_mask, bool check_errors);
bool gr_gk20a_suspend_context(struct nvgpu_channel *ch);
bool gr_gk20a_resume_context(struct nvgpu_channel *ch);
int gr_gk20a_suspend_contexts(struct gk20a *g,
			      struct dbg_session_gk20a *dbg_s,
			      int *ctx_resident_ch_fd);
int gr_gk20a_resume_contexts(struct gk20a *g,
			      struct dbg_session_gk20a *dbg_s,
			      int *ctx_resident_ch_fd);
int gr_gk20a_trigger_suspend(struct gk20a *g);
int gr_gk20a_wait_for_pause(struct gk20a *g, struct nvgpu_warpstate *w_state);
int gr_gk20a_resume_from_pause(struct gk20a *g);
int gr_gk20a_clear_sm_errors(struct gk20a *g);
void gk20a_gr_init_ovr_sm_dsm_perf(void);
void gk20a_gr_get_ovr_perf_regs(struct gk20a *g, u32 *num_ovr_perf_regs,
					       u32 **ovr_perf_regs);
int gr_gk20a_decode_priv_addr(struct gk20a *g, u32 addr,
	enum ctxsw_addr_type *addr_type,
	u32 *gpc_num, u32 *tpc_num, u32 *ppc_num, u32 *be_num,
	u32 *broadcast_flags);
int gr_gk20a_split_ppc_broadcast_addr(struct gk20a *g, u32 addr,
	u32 gpc_num,
	u32 *priv_addr_table, u32 *t);
int gr_gk20a_create_priv_addr_table(struct gk20a *g,
	u32 addr,
	u32 *priv_addr_table,
	u32 *num_registers);
void gr_gk20a_split_fbpa_broadcast_addr(struct gk20a *g, u32 addr,
	u32 num_fbpas,
	u32 *priv_addr_table, u32 *t);
int gr_gk20a_get_offset_in_gpccs_segment(struct gk20a *g,
	enum ctxsw_addr_type addr_type, u32 num_tpcs, u32 num_ppcs,
	u32 reg_list_ppc_count, u32 *__offset_in_segment);
int
gr_gk20a_process_context_buffer_priv_segment(struct gk20a *g,
					     enum ctxsw_addr_type addr_type,
					     u32 pri_addr,
					     u32 gpc_num, u32 num_tpcs,
					     u32 num_ppcs, u32 ppc_mask,
					     u32 *priv_offset);
int gr_gk20a_find_priv_offset_in_buffer(struct gk20a *g,
					       u32 addr,
					       u32 *context_buffer,
					       u32 context_buffer_size,
					       u32 *priv_offset);
int gr_gk20a_find_priv_offset_in_ext_buffer(struct gk20a *g,
						u32 addr,
						u32 *context_buffer,
						u32 context_buffer_size,
						u32 *priv_offset);
int gr_gk20a_determine_ppc_configuration(struct gk20a *g,
					u32 *context,
					u32 *num_ppcs, u32 *ppc_mask,
					u32 *reg_ppc_count);
#endif /* CONFIG_NVGPU_DEBUGGER */
#endif /*__GR_GK20A_H__*/
