/*
 * Copyright (c) 2019, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef NVGPU_CHANNEL_VGPU_H
#define NVGPU_CHANNEL_VGPU_H

struct gk20a;
struct nvgpu_channel;

void vgpu_channel_bind(struct nvgpu_channel *ch);
void vgpu_channel_unbind(struct nvgpu_channel *ch);
int vgpu_channel_alloc_inst(struct gk20a *g, struct nvgpu_channel *ch);
void vgpu_channel_free_inst(struct gk20a *g, struct nvgpu_channel *ch);
void vgpu_channel_enable(struct nvgpu_channel *ch);
void vgpu_channel_disable(struct nvgpu_channel *ch);
u32 vgpu_channel_count(struct gk20a *g);
void vgpu_channel_set_ctx_mmu_error(struct gk20a *g, struct nvgpu_channel *ch);
void vgpu_channel_set_error_notifier(struct gk20a *g,
			struct tegra_vgpu_channel_set_error_notifier *p);
void vgpu_channel_abort_cleanup(struct gk20a *g, u32 chid);

#endif
