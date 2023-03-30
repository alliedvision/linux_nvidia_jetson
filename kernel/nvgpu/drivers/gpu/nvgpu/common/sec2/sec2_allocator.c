/*
 * Copyright (c) 2018-2021, NVIDIA CORPORATION.  All rights reserved.
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

#include <nvgpu/sec2/allocator.h>
#include <nvgpu/allocator.h>
#include <nvgpu/sec2/msg.h>

int nvgpu_sec2_dmem_allocator_init(struct gk20a *g,
				struct nvgpu_allocator *dmem,
				struct sec2_init_msg_sec2_init *sec2_init)
{
	int err = 0;
	if (!nvgpu_alloc_initialized(dmem)) {
		/* Align start and end addresses */
		u32 start = NVGPU_ALIGN(sec2_init->nv_managed_area_offset,
			PMU_DMEM_ALLOC_ALIGNMENT);

		u32 end = (sec2_init->nv_managed_area_offset +
			sec2_init->nv_managed_area_size) &
			~(PMU_DMEM_ALLOC_ALIGNMENT - 1U);
		u32 size = end - start;

		err = nvgpu_allocator_init(g, dmem, NULL, "sec2_dmem", start,
				size, PMU_DMEM_ALLOC_ALIGNMENT, 0ULL, 0ULL,
				BITMAP_ALLOCATOR);
		if (err != 0) {
			nvgpu_err(g, "Couldn't init sec2_dmem allocator\n");
		}
	}
	return err;
}

void nvgpu_sec2_dmem_allocator_destroy(struct nvgpu_allocator *dmem)
{
	if (nvgpu_alloc_initialized(dmem)) {
		nvgpu_alloc_destroy(dmem);
	}
}
