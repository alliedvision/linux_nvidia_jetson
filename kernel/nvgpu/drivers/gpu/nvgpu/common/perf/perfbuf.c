/*
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
#include <nvgpu/sizes.h>
#include <nvgpu/perfbuf.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/nvgpu_init.h>
#include <nvgpu/vm_area.h>
#include <nvgpu/utils.h>

int nvgpu_perfbuf_enable_locked(struct gk20a *g, u64 offset, u32 size)
{
	int err;

	err = gk20a_busy(g);
	if (err != 0) {
		nvgpu_err(g, "failed to poweron");
		return err;
	}

	g->ops.perf.membuf_reset_streaming(g);
	g->ops.perf.enable_membuf(g, size, offset);

	gk20a_idle(g);

	return 0;
}

int nvgpu_perfbuf_disable_locked(struct gk20a *g)
{
	int err = gk20a_busy(g);
	if (err != 0) {
		nvgpu_err(g, "failed to poweron");
		return err;
	}

	g->ops.perf.membuf_reset_streaming(g);
	g->ops.perf.disable_membuf(g);

	gk20a_idle(g);

	return 0;
}

int nvgpu_perfbuf_init_inst_block(struct gk20a *g)
{
	struct mm_gk20a *mm = &g->mm;
	int err;

	err = nvgpu_alloc_inst_block(g, &mm->perfbuf.inst_block);
	if (err != 0) {
		return err;
	}

	g->ops.mm.init_inst_block(&mm->perfbuf.inst_block, mm->perfbuf.vm, 0);
	g->ops.perf.init_inst_block(g, &mm->perfbuf.inst_block);

	return 0;
}

int nvgpu_perfbuf_init_vm(struct gk20a *g)
{
	struct mm_gk20a *mm = &g->mm;
	u32 big_page_size = g->ops.mm.gmmu.get_default_big_page_size();
	int err;
	u64 user_size, kernel_size;

	g->ops.mm.get_default_va_sizes(NULL, &user_size, &kernel_size);

	mm->perfbuf.vm = nvgpu_vm_init(g, big_page_size, SZ_4K,
			nvgpu_safe_sub_u64(user_size, SZ_4K),
			kernel_size,
			0ULL,
			false, false, false, "perfbuf");
	if (mm->perfbuf.vm == NULL) {
		return -ENOMEM;
	}

	/*
	 * The PMA unit can only access GPU VAs within a 4GB window which
	 * includes PMA_BUF + PMA_AVAILABLE_BYTES_BUF, hence carveout and
	 * reserved a 4GB window from the perfbuf.vm VA space and use this
	 * VA while binding the buffers.
	 */
	mm->perfbuf.pma_buffer_gpu_va = 0;

	err = nvgpu_vm_area_alloc(mm->perfbuf.vm,
			PERFBUF_PMA_MEM_WINDOW_SIZE / SZ_4K,
			SZ_4K, &mm->perfbuf.pma_buffer_gpu_va, 0);
	if (err != 0) {
		nvgpu_vm_put(mm->perfbuf.vm);
		return err;
	}
	mm->perfbuf.pma_bytes_available_buffer_gpu_va = nvgpu_safe_add_u64(
			mm->perfbuf.pma_buffer_gpu_va,
			PERFBUF_PMA_BUF_MAX_SIZE);

	if (u64_hi32(mm->perfbuf.pma_bytes_available_buffer_gpu_va) !=
			u64_hi32(mm->perfbuf.pma_buffer_gpu_va)) {
		nvgpu_err(g, "perfbuf: 0x%llx, 0x%llx, crosses 4GB boundary",
				mm->perfbuf.pma_buffer_gpu_va,
				mm->perfbuf.pma_bytes_available_buffer_gpu_va);

		nvgpu_vm_area_free(mm->perfbuf.vm,
				mm->perfbuf.pma_buffer_gpu_va);
		nvgpu_vm_put(mm->perfbuf.vm);
		return -ENOMEM;
	}
	nvgpu_log(g, gpu_dbg_prof, "perfbuf: 0x%llx, 0x%llx",
			mm->perfbuf.pma_buffer_gpu_va,
			mm->perfbuf.pma_bytes_available_buffer_gpu_va);

	err = g->ops.perfbuf.init_inst_block(g);
	if (err != 0) {
		nvgpu_vm_put(mm->perfbuf.vm);
		return err;
	}

	return 0;
}

void nvgpu_perfbuf_deinit_inst_block(struct gk20a *g)
{
	g->ops.perf.deinit_inst_block(g);
	nvgpu_free_inst_block(g, &g->mm.perfbuf.inst_block);
}

void nvgpu_perfbuf_deinit_vm(struct gk20a *g)
{
	struct mm_gk20a *mm = &g->mm;

	g->ops.perfbuf.deinit_inst_block(g);

	nvgpu_vm_area_free(mm->perfbuf.vm, mm->perfbuf.pma_buffer_gpu_va);
	nvgpu_vm_put(g->mm.perfbuf.vm);
}

int nvgpu_perfbuf_update_get_put(struct gk20a *g, u64 bytes_consumed,
		u64 *bytes_available, void *cpuva, bool wait,
		u64 *put_ptr, bool *overflowed)
{
	struct nvgpu_timeout timeout;
	int err;
	bool update_available_bytes = (bytes_available == NULL) ? false : true;
	volatile u32 *available_bytes_va = (u32 *)cpuva;

	if (update_available_bytes && available_bytes_va != NULL) {
		*available_bytes_va = 0xffffffff;
	}

	err = g->ops.perf.update_get_put(g, bytes_consumed,
			update_available_bytes, put_ptr, overflowed);
	if (err != 0) {
		return err;
	}

	if (update_available_bytes && wait && available_bytes_va != NULL) {
		nvgpu_timeout_init_cpu_timer(g, &timeout, 10000);

		do {
			if (*available_bytes_va != 0xffffffff) {
				break;
			}

			nvgpu_msleep(10);
		} while (nvgpu_timeout_expired(&timeout) == 0);

		if (*available_bytes_va == 0xffffffff) {
			nvgpu_err(g, "perfbuf update get put timed out");
			return -ETIMEDOUT;
		}

		*bytes_available = *available_bytes_va;
	}

	return 0;
}
