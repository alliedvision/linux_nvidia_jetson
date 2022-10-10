/*
 * GV11B syncpt cmdbuf
 *
 * Copyright (c) 2018-2022, NVIDIA CORPORATION.  All rights reserved.
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
#include <nvgpu/mm.h>
#include <nvgpu/vm.h>
#include <nvgpu/gmmu.h>
#include <nvgpu/nvgpu_mem.h>
#include <nvgpu/dma.h>
#include <nvgpu/lock.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/channel.h>
#include <nvgpu/nvhost.h>
#include <nvgpu/channel_sync_syncpt.h>

#include "syncpt_cmdbuf_gv11b.h"

static int set_syncpt_ro_map_gpu_va_locked(struct vm_gk20a *vm)
{
	struct gk20a *g = gk20a_from_vm(vm);

	if (vm->syncpt_ro_map_gpu_va != 0ULL) {
		return 0;
	}

	vm->syncpt_ro_map_gpu_va = nvgpu_gmmu_map_partial(vm,
			&g->syncpt_mem, g->syncpt_unit_size,
			0, gk20a_mem_flag_read_only,
			false, APERTURE_SYSMEM);

	if (vm->syncpt_ro_map_gpu_va == 0ULL) {
		nvgpu_err(g, "failed to ro map syncpt buffer");
		return -ENOMEM;
	}

	return 0;
}

int gv11b_syncpt_alloc_buf(struct nvgpu_channel *c,
		u32 syncpt_id, struct nvgpu_mem *syncpt_buf)
{
	u64 nr_pages;
	int err = 0;
	struct gk20a *g = c->g;

	/*
	 * Add ro map for complete sync point shim range in vm
	 * All channels sharing same vm will share same ro mapping.
	 * Create rw map for current channel sync point
	 */
	nvgpu_mutex_acquire(&c->vm->syncpt_ro_map_lock);
	err = set_syncpt_ro_map_gpu_va_locked(c->vm);
	nvgpu_mutex_release(&c->vm->syncpt_ro_map_lock);
	if (err != 0) {
		return err;
	}

	nr_pages = DIV_ROUND_UP(g->syncpt_size, NVGPU_CPU_PAGE_SIZE);
	err = nvgpu_mem_create_from_phys(g, syncpt_buf,
		nvgpu_safe_add_u64(g->syncpt_unit_base,
			nvgpu_nvhost_syncpt_unit_interface_get_byte_offset(g,
				syncpt_id)),
		nr_pages);
	if (err < 0) {
		nvgpu_err(g, "failed to create mem from physical addr");
		return err;
	}

	syncpt_buf->gpu_va = nvgpu_gmmu_map_partial(c->vm, syncpt_buf,
			g->syncpt_size, 0, gk20a_mem_flag_none,
			false, APERTURE_SYSMEM);

	if (syncpt_buf->gpu_va == 0ULL) {
		nvgpu_err(g, "failed to map syncpt buffer");
		nvgpu_dma_free(g, syncpt_buf);
		err = -ENOMEM;
	}
	return err;
}

void gv11b_syncpt_free_buf(struct nvgpu_channel *c,
		struct nvgpu_mem *syncpt_buf)
{
	nvgpu_dma_unmap_free(c->vm, syncpt_buf);
}

int gv11b_syncpt_get_sync_ro_map(struct vm_gk20a *vm,
		u64 *base_gpuva, u32 *sync_size, u32 *num_syncpoints)
{
	struct gk20a *g = gk20a_from_vm(vm);
	int err;
	size_t tmp;

	nvgpu_mutex_acquire(&vm->syncpt_ro_map_lock);
	err = set_syncpt_ro_map_gpu_va_locked(vm);
	nvgpu_mutex_release(&vm->syncpt_ro_map_lock);
	if (err != 0) {
		return err;
	}

	*base_gpuva = vm->syncpt_ro_map_gpu_va;
	*sync_size = g->syncpt_size;

	tmp = (g->syncpt_size != 0UL) ? (g->syncpt_unit_size / g->syncpt_size) : 0U;
	*num_syncpoints = (tmp <= U32_MAX) ? (u32)tmp : U32_MAX;

	return 0;
}
