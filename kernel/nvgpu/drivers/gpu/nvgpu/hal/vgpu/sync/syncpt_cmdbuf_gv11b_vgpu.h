/*
 * Copyright (c) 2017-2019, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef NVGPU_SYNCPT_CMDBUF_GV11B_VGPU_H
#define NVGPU_SYNCPT_CMDBUF_GV11B_VGPU_H

#include <nvgpu/types.h>

struct gk20a;
struct nvgpu_channel;
struct nvgpu_mem;
struct vm_gk20a;

int vgpu_gv11b_syncpt_alloc_buf(struct nvgpu_channel *c,
			u32 syncpt_id, struct nvgpu_mem *syncpt_buf);
void vgpu_gv11b_syncpt_free_buf(struct nvgpu_channel *c,
					struct nvgpu_mem *syncpt_buf);
int vgpu_gv11b_syncpt_get_sync_ro_map(struct vm_gk20a *vm,
	u64 *base_gpuva, u32 *sync_size, u32 *num_syncpoints);
#endif /* NVGPU_SYNCPT_CMDBUF_GV11B_VGPU_H */
