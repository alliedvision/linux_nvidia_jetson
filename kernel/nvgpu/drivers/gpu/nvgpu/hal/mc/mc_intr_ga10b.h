/*
 * Copyright (c) 2020, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef NVGPU_MC_INTR_GA10B_H
#define NVGPU_MC_INTR_GA10B_H

#include <nvgpu/types.h>

struct gk20a;

void ga10b_intr_host2soc_0_unit_config(struct gk20a *g, u32 unit, bool enable);
u32  ga10b_intr_host2soc_0(struct gk20a *g);
void ga10b_intr_host2soc_0_pause(struct gk20a *g);
void ga10b_intr_host2soc_0_resume(struct gk20a *g);
u32  ga10b_intr_isr_host2soc_0(struct gk20a *g);

void ga10b_intr_log_pending_intrs(struct gk20a *g);
void ga10b_intr_mask_top(struct gk20a *g);
bool ga10b_intr_is_mmu_fault_pending(struct gk20a *g);

void ga10b_intr_stall_unit_config(struct gk20a *g, u32 unit, bool enable);
u32  ga10b_intr_stall(struct gk20a *g);
void ga10b_intr_stall_pause(struct gk20a *g);
void ga10b_intr_stall_resume(struct gk20a *g);
void ga10b_intr_isr_stall(struct gk20a *g);

bool ga10b_intr_is_stall_and_eng_intr_pending(struct gk20a *g, u32 engine_id,
			u32 *eng_intr_pending);
bool ga10b_mc_intr_get_unit_info(struct gk20a *g, u32 unit);

#endif /* NVGPU_MC_INTR_GA10B_H */
