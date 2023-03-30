/*
 * Copyright (c) 2017-2020, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef NVGPU_PMU_ALLOCATOR_H
#define NVGPU_PMU_ALLOCATOR_H

#include <nvgpu/types.h>

struct gk20a;
struct nvgpu_pmu;
struct nvgpu_mem;
struct nvgpu_allocator;
union pmu_init_msg_pmu;
struct flcn_mem_desc_v0;

void nvgpu_pmu_allocator_dmem_init(struct gk20a *g,
	struct nvgpu_pmu *pmu, struct nvgpu_allocator *dmem,
	union pmu_init_msg_pmu *init);
void nvgpu_pmu_allocator_dmem_destroy(struct nvgpu_allocator *dmem);

void nvgpu_pmu_allocator_surface_free(struct gk20a *g, struct nvgpu_mem *mem);
void nvgpu_pmu_allocator_surface_describe(struct gk20a *g, struct nvgpu_mem *mem,
		struct flcn_mem_desc_v0 *fb);
int nvgpu_pmu_allocator_sysmem_surface_alloc(struct gk20a *g,
		struct nvgpu_mem *mem, u32 size);
#endif /* NVGPU_PMU_ALLOCATOR_H */
