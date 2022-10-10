/*
 * GA10B Runlist
 *
 * Copyright (c) 2020-2022, NVIDIA CORPORATION.  All rights reserved.
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

#include <nvgpu/log.h>
#include <nvgpu/soc.h>
#include <nvgpu/fifo.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/power_features/cg.h>
#include <nvgpu/static_analysis.h>
#include <nvgpu/mc.h>
#include <nvgpu/engines.h>
#include <nvgpu/runlist.h>

#include <nvgpu/hw/ga10b/hw_runlist_ga10b.h>

#include "fifo_utils_ga10b.h"
#include "runlist_fifo_ga10b.h"

u32 ga10b_runlist_count_max(struct gk20a *g)
{
	(void)g;
	/* TODO Needs to be read from litter values */
	return 4U;
}

u32 ga10b_runlist_length_max(struct gk20a *g)
{
	(void)g;
	return runlist_submit_length_max_v();
}

void ga10b_runlist_hw_submit(struct gk20a *g, struct nvgpu_runlist *runlist)
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

int ga10b_runlist_wait_pending(struct gk20a *g, struct nvgpu_runlist *runlist)
{
	struct nvgpu_timeout timeout;
	u32 delay = POLL_DELAY_MIN_US;
	int ret;

	nvgpu_timeout_init_cpu_timer(g, &timeout, nvgpu_get_poll_timeout(g));

	ret = -ETIMEDOUT;
	do {
		if ((nvgpu_runlist_readl(g, runlist, runlist_submit_info_r()) &
			runlist_submit_info_pending_true_f()) == 0U) {
			ret = 0;
			break;
		}

		nvgpu_usleep_range(delay, delay * 2U);
		delay = min_t(u32, delay << 1, POLL_DELAY_MAX_US);
	} while (nvgpu_timeout_expired(&timeout) == 0);

	return ret;
}

u32 ga10b_get_runlist_aperture(struct gk20a *g, struct nvgpu_runlist *runlist)
{
	return nvgpu_aperture_mask(g, &runlist->domain->mem_hw->mem,
			runlist_submit_base_lo_target_sys_mem_noncoherent_f(),
			runlist_submit_base_lo_target_sys_mem_coherent_f(),
			runlist_submit_base_lo_target_vid_mem_f());
}

void ga10b_runlist_write_state(struct gk20a *g, u32 runlists_mask,
						u32 runlist_state)
{
	u32 reg_val;
	u32 runlist_id = 0U;
	struct nvgpu_runlist *runlist = NULL;

	if (runlist_state == RUNLIST_DISABLED) {
		reg_val = runlist_sched_disable_runlist_disabled_v();
	} else {
		reg_val = runlist_sched_disable_runlist_enabled_v();
	}

	while (runlists_mask != 0U && (runlist_id < g->fifo.max_runlists)) {
		if ((runlists_mask & BIT32(runlist_id)) != 0U) {
			runlist = g->fifo.runlists[runlist_id];
			nvgpu_runlist_writel(g, runlist,
				runlist_sched_disable_r(), reg_val);
		}
		runlists_mask &= ~BIT32(runlist_id);
		runlist_id++;
	}
}
