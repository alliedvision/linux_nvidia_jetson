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

#ifndef NVGPU_POSIX_NVGPU_MEM_H
#define NVGPU_POSIX_NVGPU_MEM_H

struct nvgpu_mem_priv {
	struct nvgpu_sgt *sgt;
};

struct nvgpu_mem;
struct nvgpu_mem_sgl;

struct nvgpu_mem_sgl *nvgpu_mem_sgl_posix_create_from_list(struct gk20a *g,
				struct nvgpu_mem_sgl *sgl_list, u32 nr_sgls,
				u64 *total_size);
void nvgpu_mem_sgl_free(struct gk20a *g, struct nvgpu_mem_sgl *sgl);
struct nvgpu_sgt *nvgpu_mem_sgt_posix_create_from_list(struct gk20a *g,
				struct nvgpu_mem_sgl *sgl_list, u32 nr_sgls,
				u64 *total_size);
int nvgpu_mem_posix_create_from_list(struct gk20a *g, struct nvgpu_mem *mem,
				struct nvgpu_mem_sgl *sgl_list, u32 nr_sgls);
int __nvgpu_mem_create_from_phys(struct gk20a *g, struct nvgpu_mem *dest,
				  u64 src_phys, int nr_pages);

#endif /* NVGPU_POSIX_NVGPU_MEM_H */
