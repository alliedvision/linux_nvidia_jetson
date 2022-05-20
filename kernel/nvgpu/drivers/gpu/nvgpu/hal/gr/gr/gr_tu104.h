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

#ifndef NVGPU_GR_TU104_H
#define NVGPU_GR_TU104_H

#ifdef CONFIG_NVGPU_DEBUGGER

#include <nvgpu/types.h>

struct gk20a;

int gr_tu104_init_sw_bundle64(struct gk20a *g);
void gr_tu10x_create_sysfs(struct gk20a *g);
void gr_tu10x_remove_sysfs(struct gk20a *g);
int gr_tu104_get_offset_in_gpccs_segment(struct gk20a *g,
	enum ctxsw_addr_type addr_type, u32 num_tpcs, u32 num_ppcs,
	u32 reg_list_ppc_count, u32 *__offset_in_segment);
void gr_tu104_init_sm_dsm_reg_info(void);
void gr_tu104_get_sm_dsm_perf_ctrl_regs(struct gk20a *g,
	u32 *num_sm_dsm_perf_ctrl_regs, u32 **sm_dsm_perf_ctrl_regs,
	u32 *ctrl_register_stride);
int tu104_gr_update_smpc_global_mode(struct gk20a *g, bool enable);

void tu104_gr_disable_cau(struct gk20a *g);
void tu104_gr_disable_smpc(struct gk20a *g);
const u32 *tu104_gr_get_hwpm_cau_init_data(u32 *count);
void tu104_gr_init_cau(struct gk20a *g);

int gr_tu104_decode_priv_addr(struct gk20a *g, u32 addr,
	enum ctxsw_addr_type *addr_type,
	u32 *gpc_num, u32 *tpc_num, u32 *ppc_num, u32 *be_num,
	u32 *broadcast_flags);

#endif /* CONFIG_NVGPU_DEBUGGER */
#endif /* NVGPU_GR_TU104_H */
