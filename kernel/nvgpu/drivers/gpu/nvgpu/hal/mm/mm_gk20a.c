/*
 * Copyright (c) 2019-2020, NVIDIA CORPORATION.  All rights reserved.
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

#include <nvgpu/gk20a.h>
#include <nvgpu/gmmu.h>
#include <nvgpu/mm.h>

#include "mm_gk20a.h"

void gk20a_mm_init_inst_block(struct nvgpu_mem *inst_block, struct vm_gk20a *vm,
			      u32 big_page_size)
{
	struct gk20a *g = gk20a_from_vm(vm);
	u64 pdb_addr = nvgpu_pd_gpu_addr(g, &vm->pdb);

	nvgpu_log_info(g, "inst block phys = 0x%llx, kv = 0x%p",
		nvgpu_inst_block_addr(g, inst_block), inst_block->cpu_va);

	g->ops.ramin.init_pdb(g, inst_block, pdb_addr, vm->pdb.mem);

	g->ops.ramin.set_adr_limit(g, inst_block, vm->va_limit - 1U);

	if ((big_page_size != 0U) && (g->ops.ramin.set_big_page_size != NULL)) {
		g->ops.ramin.set_big_page_size(g, inst_block, big_page_size);
	}
}

#ifdef CONFIG_NVGPU_USERD
u64 gk20a_mm_bar1_map_userd(struct gk20a *g, struct nvgpu_mem *mem, u32 offset)
{
	struct nvgpu_fifo *f = &g->fifo;
	u64 gpu_va = f->userd_gpu_va + offset;

	return nvgpu_gmmu_map_fixed(g->mm.bar1.vm, mem, gpu_va,
				    NVGPU_CPU_PAGE_SIZE, 0,
				    gk20a_mem_flag_none, false,
				    mem->aperture);
}
#endif
