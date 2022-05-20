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

#include <nvgpu/io.h>
#include <nvgpu/runlist.h>
#include <nvgpu/gk20a.h>

#include "runlist_fifo_tu104.h"

#include <nvgpu/hw/tu104/hw_fifo_tu104.h>
struct gk20a;

u32 tu104_runlist_count_max(struct gk20a *g)
{
	return fifo_runlist_base_lo__size_1_v();
}

void tu104_runlist_hw_submit(struct gk20a *g, struct nvgpu_runlist *runlist)
{
	u64 runlist_iova;
	u32 runlist_iova_lo, runlist_iova_hi;

	runlist_iova = nvgpu_mem_get_addr(g, &runlist->domain->mem_hw->mem);

	runlist_iova_lo = u64_lo32(runlist_iova) >>
				fifo_runlist_base_lo_ptr_align_shift_v();
	runlist_iova_hi = u64_hi32(runlist_iova);

	if (runlist->domain->mem_hw->count != 0U) {
		nvgpu_writel(g, fifo_runlist_base_lo_r(runlist->id),
			fifo_runlist_base_lo_ptr_lo_f(runlist_iova_lo) |
			nvgpu_aperture_mask(g, &runlist->domain->mem_hw->mem,
				fifo_runlist_base_lo_target_sys_mem_ncoh_f(),
				fifo_runlist_base_lo_target_sys_mem_coh_f(),
				fifo_runlist_base_lo_target_vid_mem_f()));

		nvgpu_writel(g, fifo_runlist_base_hi_r(runlist->id),
			fifo_runlist_base_hi_ptr_hi_f(runlist_iova_hi));
	}

	nvgpu_writel(g, fifo_runlist_submit_r(runlist->id),
		fifo_runlist_submit_length_f(runlist->domain->mem_hw->count));
}

int tu104_runlist_wait_pending(struct gk20a *g, struct nvgpu_runlist *runlist)
{
	struct nvgpu_timeout timeout;
	u32 delay = POLL_DELAY_MIN_US;
	int ret;

	nvgpu_timeout_init_cpu_timer(g, &timeout, nvgpu_get_poll_timeout(g));

	ret = -ETIMEDOUT;
	do {
		if ((nvgpu_readl(g, fifo_runlist_submit_info_r(runlist->id)) &
			fifo_runlist_submit_info_pending_true_f()) == 0U) {
			ret = 0;
			break;
		}

		nvgpu_usleep_range(delay, delay * 2U);
		delay = min_t(u32, delay << 1, POLL_DELAY_MAX_US);
	} while (nvgpu_timeout_expired(&timeout) == 0);

	return ret;
}
