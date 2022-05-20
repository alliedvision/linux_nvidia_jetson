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
#include <nvgpu/bitops.h>
#include <nvgpu/mm.h>
#include <nvgpu/vm.h>

#include "mm_gp10b.h"

int gp10b_mm_init_bar2_vm(struct gk20a *g)
{
	int err;
	struct mm_gk20a *mm = &g->mm;
	struct nvgpu_mem *inst_block = &mm->bar2.inst_block;
	u32 big_page_size = g->ops.mm.gmmu.get_default_big_page_size();

	/* BAR2 aperture size is 32MB */
	mm->bar2.aperture_size = U32(32) << 20U;
	nvgpu_log_info(g, "bar2 vm size = 0x%x", mm->bar2.aperture_size);

	mm->bar2.vm = nvgpu_vm_init(g, big_page_size, SZ_4K,
		0ULL, nvgpu_safe_sub_u64(mm->bar2.aperture_size, SZ_4K), 0ULL,
		false, false, false, "bar2");
	if (mm->bar2.vm == NULL) {
		return -ENOMEM;
	}

	/* allocate instance mem for bar2 */
	err = nvgpu_alloc_inst_block(g, inst_block);
	if (err != 0) {
		goto clean_up_va;
	}

	g->ops.mm.init_inst_block(inst_block, mm->bar2.vm, big_page_size);

	return 0;

clean_up_va:
	nvgpu_vm_put(mm->bar2.vm);
	return err;
}


void gp10b_mm_remove_bar2_vm(struct gk20a *g)
{
	struct mm_gk20a *mm = &g->mm;

	nvgpu_free_inst_block(g, &mm->bar2.inst_block);
	nvgpu_vm_put(mm->bar2.vm);
}

void gp10b_mm_get_default_va_sizes(u64 *aperture_size,
			u64 *user_size, u64 *kernel_size)
{
	/*
	 * The maximum GPU VA range supported.
	 * Max VA Bits = 49, refer dev_mmu.ref.
	 */
	if (aperture_size != NULL) {
		*aperture_size = BIT64(49);
	}

	/*
	 * The default userspace-visible GPU VA size.
	 */
	if (user_size != NULL) {
		*user_size = BIT64(37);
	}

	/*
	 * The default kernel-reserved GPU VA size.
	 */
	if (kernel_size != NULL) {
		*kernel_size = BIT64(32);
	}
}
