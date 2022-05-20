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

#include <nvgpu/bug.h>
#include <nvgpu/vidmem.h>
#include <nvgpu/nvgpu_mem.h>
#include <nvgpu/nvgpu_sgt.h>
#include <nvgpu/page_allocator.h>
#include <nvgpu/posix/posix_vidmem.h>

bool nvgpu_addr_is_vidmem_page_alloc(u64 addr)
{
	return ((addr & 1ULL) != 0ULL);
}

void nvgpu_vidmem_set_page_alloc(struct nvgpu_mem_sgl *sgl, u64 addr)
{
	/* set bit 0 to indicate vidmem allocation */
	sgl->dma = (addr | 1ULL);
	sgl->phys = (addr | 1ULL);
}

struct nvgpu_page_alloc *nvgpu_vidmem_get_page_alloc(struct nvgpu_mem_sgl *sgl)
{
	u64 addr;

	addr = sgl->dma;

	if (nvgpu_addr_is_vidmem_page_alloc(addr))
		addr = addr & ~1ULL;
	else
		WARN_ON(true);

	return (struct nvgpu_page_alloc *)(uintptr_t)addr;
}


void nvgpu_mem_free_vidmem_alloc(struct gk20a *g, struct nvgpu_mem *vidmem)
{
	BUG();
}
