/*
 * GA10B GPU GR
 *
 * Copyright (c) 2020-2022, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef NVGPU_GR_GA10B_H
#define NVGPU_GR_GA10B_H

#ifdef CONFIG_NVGPU_DEBUGGER
#define FOUND_IN_CTXSWBUF_PRIV_REGLIST		(0)
#define FOUND_IN_CTXSWBUF_PRIV_COMPUTE_REGLIST	(1)
#define FOUND_IN_CTXSWBUF_PRIV_GFX_REGLIST	(2)
#endif /* CONFIG_NVGPU_DEBUGGER */

struct gk20a;
struct nvgpu_debug_context;
enum ctxsw_addr_type;
struct nvgpu_vab_range_checker;

int gr_ga10b_dump_gr_status_regs(struct gk20a *g,
				 struct nvgpu_debug_context *o);
void gr_ga10b_set_circular_buffer_size(struct gk20a *g, u32 data);
void ga10b_gr_set_gpcs_rops_crop_debug4(struct gk20a *g, u32 data);
#ifdef CONFIG_NVGPU_HAL_NON_FUSA
void ga10b_gr_vab_reserve(struct gk20a *g, u32 vab_reg, u32 num_range_checkers,
	struct nvgpu_vab_range_checker *vab_range_checker);
void ga10b_gr_vab_configure(struct gk20a *g, u32 vab_reg);
#endif /* CONFIG_NVGPU_HAL_NON_FUSA */

#ifdef CONFIG_NVGPU_DEBUGGER
int gr_ga10b_create_priv_addr_table(struct gk20a *g,
					   u32 addr,
					   u32 *priv_addr_table,
					   u32 *num_registers);
int gr_ga10b_decode_priv_addr(struct gk20a *g, u32 addr,
	enum ctxsw_addr_type *addr_type,
	u32 *gpc_num, u32 *tpc_num, u32 *ppc_num, u32 *be_num,
	u32 *broadcast_flags);
int gr_ga10b_process_context_buffer_priv_segment(struct gk20a *g,
					     enum ctxsw_addr_type addr_type,
					     u32 pri_addr,
					     u32 gpc_num, u32 num_tpcs,
					     u32 num_ppcs, u32 ppc_mask,
					     u32 *priv_offset);
bool ga10b_gr_check_warp_esr_error(struct gk20a *g, u32 warp_esr_error);
int gr_ga10b_find_priv_offset_in_buffer(struct gk20a *g, u32 addr,
					u32 *context_buffer,
					u32 context_buffer_size,
					u32 *priv_offset);
const u32 *ga10b_gr_get_hwpm_cau_init_data(u32 *count);
#endif /* CONFIG_NVGPU_DEBUGGER */
#endif /* NVGPU_GR_GA10B_H */
