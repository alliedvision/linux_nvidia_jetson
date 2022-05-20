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

#ifndef NVGPU_TSG_VGPU_H
#define NVGPU_TSG_VGPU_H

#include <nvgpu/types.h>

struct gk20a;
struct nvgpu_channel;
struct nvgpu_tsg;

int vgpu_tsg_open(struct nvgpu_tsg *tsg);
void vgpu_tsg_release(struct nvgpu_tsg *tsg);
void vgpu_tsg_enable(struct nvgpu_tsg *tsg);
int vgpu_tsg_bind_channel(struct nvgpu_tsg *tsg, struct nvgpu_channel *ch);
int vgpu_tsg_unbind_channel(struct nvgpu_tsg *tsg, struct nvgpu_channel *ch);
int vgpu_tsg_set_timeslice(struct nvgpu_tsg *tsg, u32 timeslice);
int vgpu_set_sm_exception_type_mask(struct nvgpu_channel *ch,
		u32 exception_mask);
int vgpu_tsg_set_interleave(struct nvgpu_tsg *tsg, u32 new_level);
int vgpu_tsg_force_reset_ch(struct nvgpu_channel *ch,
					u32 err_code, bool verbose);
u32 vgpu_tsg_default_timeslice_us(struct gk20a *g);
void vgpu_tsg_set_ctx_mmu_error(struct gk20a *g, u32 chid);
void vgpu_tsg_handle_event(struct gk20a *g,
			struct tegra_vgpu_channel_event_info *info);

#endif
