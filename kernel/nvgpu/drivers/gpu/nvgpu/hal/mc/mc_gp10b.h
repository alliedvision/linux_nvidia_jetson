/*
 * Copyright (c) 2014-2020, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef NVGPU_MC_GP10B_H
#define NVGPU_MC_GP10B_H

#include <nvgpu/types.h>

#ifdef CONFIG_NVGPU_NON_FUSA
#define MAX_MC_INTR_REGS	2U
#endif

struct gk20a;
struct nvgpu_device;
enum nvgpu_fifo_engine;

void mc_gp10b_intr_mask(struct gk20a *g);
void mc_gp10b_intr_stall_unit_config(struct gk20a *g, u32 unit, bool enable);
void mc_gp10b_intr_nonstall_unit_config(struct gk20a *g, u32 unit, bool enable);
void mc_gp10b_isr_stall_secondary_1(struct gk20a *g, u32 mc_intr_0);
void mc_gp10b_isr_stall_secondary_0(struct gk20a *g, u32 mc_intr_0);
void mc_gp10b_isr_stall_engine(struct gk20a *g, const struct nvgpu_device *dev);
void mc_gp10b_isr_stall(struct gk20a *g);
bool mc_gp10b_is_intr1_pending(struct gk20a *g, u32 unit, u32 mc_intr_1);

#ifdef CONFIG_NVGPU_NON_FUSA
void mc_gp10b_log_pending_intrs(struct gk20a *g);
#endif
u32  mc_gp10b_intr_stall(struct gk20a *g);
void mc_gp10b_intr_stall_pause(struct gk20a *g);
void mc_gp10b_intr_stall_resume(struct gk20a *g);
u32  mc_gp10b_intr_nonstall(struct gk20a *g);
void mc_gp10b_intr_nonstall_pause(struct gk20a *g);
void mc_gp10b_intr_nonstall_resume(struct gk20a *g);

/** @cond DOXYGEN_SHOULD_SKIP_THIS */
void mc_gp10b_ltc_isr(struct gk20a *g);
/** @endcond DOXYGEN_SHOULD_SKIP_THIS */

#endif /* NVGPU_MC_GP10B_H */
