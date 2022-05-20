/*
 * Copyright (c) 2018-2020, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef NVGPU_MC_TU104_H
#define NVGPU_MC_TU104_H

#include <nvgpu/types.h>

#define NV_CPU_INTR_SUBTREE_TO_TOP_IDX(i)	((i) / 32U)
#define NV_CPU_INTR_SUBTREE_TO_TOP_BIT(i)	((i) % 32U)
#define NV_CPU_INTR_SUBTREE_TO_LEAF_REG0(i)	((i)*2U)
#define NV_CPU_INTR_SUBTREE_TO_LEAF_REG1(i)	(((i)*2U) + 1U)

#define NV_CPU_INTR_GPU_VECTOR_TO_LEAF_REG(i)	((i) / 32U)
#define NV_CPU_INTR_GPU_VECTOR_TO_LEAF_BIT(i)	((i) % 32U)
#define NV_CPU_INTR_GPU_VECTOR_TO_SUBTREE(i)	((NV_CPU_INTR_GPU_VECTOR_TO_LEAF_REG(i)) / 2U)

#define NV_CPU_INTR_TOP_NONSTALL_SUBTREE	0U

struct gk20a;

void intr_tu104_leaf_en_set(struct gk20a *g, u32 leaf_reg_index,
	u32 leaf_reg_bit);
void intr_tu104_leaf_en_clear(struct gk20a *g, u32 leaf_reg_index,
	u32 leaf_reg_bit);
void intr_tu104_top_en_set(struct gk20a *g, u32 top_reg_index,
	u32 top_reg_bit);
void intr_tu104_vector_en_set(struct gk20a *g, u32 intr_vector);
void intr_tu104_vector_en_clear(struct gk20a *g, u32 intr_vector);
bool intr_tu104_vector_intr_pending(struct gk20a *g, u32 intr_vector);
void intr_tu104_intr_clear_leaf_vector(struct gk20a *g, u32 intr_vector);

void intr_tu104_stall_unit_config(struct gk20a *g, u32 unit, bool enable);
void intr_tu104_nonstall_unit_config(struct gk20a *g, u32 unit, bool enable);
void intr_tu104_mask(struct gk20a *g);
u32  intr_tu104_stall(struct gk20a *g);
void intr_tu104_stall_pause(struct gk20a *g);
void intr_tu104_stall_resume(struct gk20a *g);
u32  intr_tu104_nonstall(struct gk20a *g);
void intr_tu104_nonstall_pause(struct gk20a *g);
void intr_tu104_nonstall_resume(struct gk20a *g);
u32  intr_tu104_isr_nonstall(struct gk20a *g);
bool intr_tu104_is_intr_hub_pending(struct gk20a *g, u32 mc_intr_0);
void intr_tu104_log_pending_intrs(struct gk20a *g);
void mc_tu104_fbpa_isr(struct gk20a *g);
void mc_tu104_ltc_isr(struct gk20a *g);
void mc_tu104_isr_stall(struct gk20a *g);

#endif /* NVGPU_MC_TU104_H */
