/*
 * Copyright (c) 2018-2019, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef NVGPU_FB_MMU_FAULT_TU104_H
#define NVGPU_FB_MMU_FAULT_TU104_H

#include <nvgpu/types.h>

struct gk20a;

void tu104_fb_handle_mmu_fault(struct gk20a *g);

void tu104_fb_write_mmu_fault_buffer_lo_hi(struct gk20a *g, u32 index,
	u32 addr_lo, u32 addr_hi);
u32  tu104_fb_read_mmu_fault_buffer_get(struct gk20a *g, u32 index);
void tu104_fb_write_mmu_fault_buffer_get(struct gk20a *g, u32 index,
	u32 reg_val);
u32  tu104_fb_read_mmu_fault_buffer_put(struct gk20a *g, u32 index);
u32  tu104_fb_read_mmu_fault_buffer_size(struct gk20a *g, u32 index);
void tu104_fb_write_mmu_fault_buffer_size(struct gk20a *g, u32 index,
	u32 reg_val);
void tu104_fb_read_mmu_fault_addr_lo_hi(struct gk20a *g,
	u32 *addr_lo, u32 *addr_hi);
void tu104_fb_read_mmu_fault_inst_lo_hi(struct gk20a *g,
	u32 *inst_lo, u32 *inst_hi);
u32  tu104_fb_read_mmu_fault_info(struct gk20a *g);
u32  tu104_fb_read_mmu_fault_status(struct gk20a *g);
void tu104_fb_write_mmu_fault_status(struct gk20a *g, u32 reg_val);

#ifdef CONFIG_NVGPU_REPLAYABLE_FAULT
int  tu104_fb_mmu_invalidate_replay(struct gk20a *g,
	u32 invalidate_replay_val);
#endif

#endif /* NVGPU_FB_MMU_FAULT_TU104_H */

