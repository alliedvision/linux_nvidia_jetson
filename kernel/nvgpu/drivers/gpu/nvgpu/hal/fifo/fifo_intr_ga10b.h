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

#ifndef NVGPU_FIFO_INTR_GA10B_H
#define NVGPU_FIFO_INTR_GA10B_H

#include <nvgpu/types.h>

struct gk20a;

void ga10b_fifo_runlist_intr_vectorid_init(struct gk20a *g);
void ga10b_fifo_intr_top_enable(struct gk20a *g, bool enable);
void ga10b_fifo_intr_0_enable(struct gk20a *g, bool enable);
void ga10b_fifo_intr_1_enable(struct gk20a *g, bool enable);
void ga10b_fifo_intr_0_isr(struct gk20a *g);

void ga10b_fifo_intr_set_recover_mask(struct gk20a *g);
void ga10b_fifo_intr_unset_recover_mask(struct gk20a *g);

void ga10b_fifo_pbdma_isr(struct gk20a *g, struct nvgpu_runlist *runlist, u32 pbdma_idx);
void ga10b_fifo_runlist_intr_retrigger(struct gk20a *g, u32 intr_tree);

#endif /* NVGPU_FIFO_INTR_GA10B_H */
