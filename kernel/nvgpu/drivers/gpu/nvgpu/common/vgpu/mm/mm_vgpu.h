/*
 * Copyright (c) 2017-2021, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef NVGPU_MM_VGPU_H
#define NVGPU_MM_VGPU_H

struct nvgpu_mem;
struct nvgpu_channel;
struct vm_gk20a_mapping_batch;
struct gk20a_as_share;
struct vm_gk20a;
enum gk20a_mem_rw_flag;
struct nvgpu_sgt;
enum nvgpu_aperture;

void vgpu_locked_gmmu_unmap(struct vm_gk20a *vm,
				u64 vaddr,
				u64 size,
				u32 pgsz_idx,
				bool va_allocated,
				enum gk20a_mem_rw_flag rw_flag,
				bool sparse,
				struct vm_gk20a_mapping_batch *batch);
int vgpu_vm_bind_channel(struct vm_gk20a *vm,
				struct nvgpu_channel *ch);
int vgpu_mm_fb_flush(struct gk20a *g);
void vgpu_mm_l2_invalidate(struct gk20a *g);
int vgpu_mm_l2_flush(struct gk20a *g, bool invalidate);
int vgpu_mm_tlb_invalidate(struct gk20a *g, struct nvgpu_mem *pdb);
#ifdef CONFIG_NVGPU_DEBUGGER
void vgpu_mm_mmu_set_debug_mode(struct gk20a *g, bool enable);
#endif
u64 vgpu_locked_gmmu_map(struct vm_gk20a *vm,
				u64 map_offset,
				struct nvgpu_sgt *sgt,
				u64 buffer_offset,
				u64 size,
				u32 pgsz_idx,
				u8 kind_v,
				u32 ctag_offset,
				u32 flags,
				enum gk20a_mem_rw_flag rw_flag,
				bool clear_ctags,
				bool sparse,
				bool priv,
				struct vm_gk20a_mapping_batch *batch,
				enum nvgpu_aperture aperture);
int vgpu_init_mm_support(struct gk20a *g);

#endif /* NVGPU_MM_VGPU_H */
