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

#ifndef FIFO_CHANNEL_GA10B_H
#define FIFO_CHANNEL_GA10B_H

#include <nvgpu/types.h>

struct gk20a;
struct nvgpu_channel;
struct nvgpu_channel_hw_state;

u32 ga10b_channel_count(struct gk20a *g);
void ga10b_channel_enable(struct nvgpu_channel *ch);
void ga10b_channel_disable(struct nvgpu_channel *ch);
void ga10b_channel_bind(struct nvgpu_channel *ch);
void ga10b_channel_unbind(struct nvgpu_channel *ch);
void ga10b_channel_read_state(struct gk20a *g, struct nvgpu_channel *ch,
					struct nvgpu_channel_hw_state *state);
void ga10b_channel_reset_faulted(struct gk20a *g, struct nvgpu_channel *ch,
		bool eng, bool pbdma);
void ga10b_channel_force_ctx_reload(struct nvgpu_channel *ch);

#endif /* FIFO_CHANNEL_GA10B_H */
