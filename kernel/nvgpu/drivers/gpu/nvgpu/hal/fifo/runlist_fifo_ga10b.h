/*
 * GA10B runlist
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

#ifndef NVGPU_RUNLIST_FIFO_GA10B_H
#define NVGPU_RUNLIST_FIFO_GA10B_H

#include <nvgpu/types.h>

struct gk20a;
struct nvgpu_runlist;

u32 ga10b_runlist_count_max(struct gk20a *g);
u32 ga10b_runlist_length_max(struct gk20a *g);
void ga10b_runlist_hw_submit(struct gk20a *g, struct nvgpu_runlist *runlist);
int ga10b_runlist_wait_pending(struct gk20a *g, struct nvgpu_runlist *runlist);
void ga10b_runlist_write_state(struct gk20a *g, u32 runlists_mask,
				u32 runlist_state);
u32 ga10b_get_runlist_aperture(struct gk20a *g, struct nvgpu_runlist *runlist);
#ifdef CONFIG_NVGPU_CHANNEL_TSG_SCHEDULING
int ga10b_fifo_reschedule_preempt_next(struct nvgpu_channel *ch,
		bool wait_preempt);
#endif

#endif /* NVGPU_RUNLIST_FIFO_GA10B_H */
