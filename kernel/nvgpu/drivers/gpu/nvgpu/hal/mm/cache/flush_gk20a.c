/*
 * Copyright (c) 2019-2021, NVIDIA CORPORATION.  All rights reserved.
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

#include <nvgpu/trace.h>
#include <nvgpu/mm.h>
#include <nvgpu/io.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/nvgpu_init.h>
#include <nvgpu/timers.h>

#include <nvgpu/hw/gk20a/hw_flush_gk20a.h>

#include "flush_gk20a.h"

#ifdef CONFIG_NVGPU_COMPRESSION
void gk20a_mm_cbc_clean(struct gk20a *g)
{
	struct mm_gk20a *mm = &g->mm;
	u32 data;
	struct nvgpu_timeout timeout;
	u32 retries = 200;

	nvgpu_log_fn(g, " ");

	gk20a_busy_noresume(g);
	if (nvgpu_is_powered_off(g)) {
		goto hw_was_off;
	}

	if (g->ops.mm.get_flush_retries != NULL) {
		retries = g->ops.mm.get_flush_retries(g, NVGPU_FLUSH_CBC_CLEAN);
	}

	nvgpu_timeout_init_retry(g, &timeout, retries);

	nvgpu_mutex_acquire(&mm->l2_op_lock);

	/* Flush all dirty lines from the CBC to L2 */
	nvgpu_writel(g, flush_l2_clean_comptags_r(),
		flush_l2_clean_comptags_pending_busy_f());

	do {
		data = nvgpu_readl(g, flush_l2_clean_comptags_r());

		if (flush_l2_clean_comptags_outstanding_v(data) ==
			flush_l2_clean_comptags_outstanding_true_v() ||
		    flush_l2_clean_comptags_pending_v(data) ==
			flush_l2_clean_comptags_pending_busy_v()) {
			nvgpu_log_info(g, "l2_clean_comptags 0x%x", data);
			nvgpu_udelay(5);
		} else {
			break;
		}
	} while (nvgpu_timeout_expired_msg(&timeout,
				"l2_clean_comptags too many retries") == 0);

	nvgpu_mutex_release(&mm->l2_op_lock);

hw_was_off:
	gk20a_idle_nosuspend(g);
}
#endif
