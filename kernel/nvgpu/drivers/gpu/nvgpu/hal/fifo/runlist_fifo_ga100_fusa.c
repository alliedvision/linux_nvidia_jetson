/*
 * GA100 Runlist
 *
 * Copyright (c) 2020-2021, NVIDIA CORPORATION.  All rights reserved.
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

#include <nvgpu/soc.h>
#include <nvgpu/fifo.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/engines.h>
#include <nvgpu/runlist.h>

#include <nvgpu/hw/ga100/hw_runlist_ga100.h>

#include "fifo_utils_ga10b.h"
#include "runlist_fifo_ga100.h"

u32 ga100_runlist_count_max(struct gk20a *g)
{
	return nvgpu_get_litter_value(g, GPU_LIT_MAX_RUNLISTS_SUPPORTED);
}

void ga100_runlist_hw_submit(struct gk20a *g, struct nvgpu_runlist *runlist)
{
	u64 runlist_iova;
	u32 runlist_iova_lo, runlist_iova_hi;

	runlist_iova = nvgpu_mem_get_addr(g, &runlist->domain->mem_hw->mem);
	runlist_iova_lo = u64_lo32(runlist_iova) >>
			runlist_submit_base_lo_ptr_align_shift_v();
	runlist_iova_hi = u64_hi32(runlist_iova);

	if (runlist->domain->mem_hw->count != 0U) {
		nvgpu_runlist_writel(g, runlist, runlist_submit_base_lo_r(),
			runlist_submit_base_lo_ptr_lo_f(runlist_iova_lo) |
			nvgpu_aperture_mask(g, &runlist->domain->mem_hw->mem,
			runlist_submit_base_lo_target_sys_mem_noncoherent_f(),
			runlist_submit_base_lo_target_sys_mem_coherent_f(),
			runlist_submit_base_lo_target_vid_mem_f()));

		nvgpu_runlist_writel(g, runlist, runlist_submit_base_hi_r(),
			runlist_submit_base_hi_ptr_hi_f(runlist_iova_hi));
	}

	/* TODO offset in runlist support */
	nvgpu_runlist_writel(g, runlist, runlist_submit_r(),
			runlist_submit_offset_f(0U) |
			runlist_submit_length_f(runlist->domain->mem_hw->count));
}
