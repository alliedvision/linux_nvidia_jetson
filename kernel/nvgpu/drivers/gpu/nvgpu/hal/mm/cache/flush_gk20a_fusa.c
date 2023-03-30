/*
 * Copyright (c) 2019-2022, NVIDIA CORPORATION.  All rights reserved.
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

int gk20a_mm_fb_flush(struct gk20a *g)
{
	struct mm_gk20a *mm = &g->mm;
	u32 data;
	int ret = 0;
	struct nvgpu_timeout timeout;
	u32 retries;

	nvgpu_log(g, gpu_dbg_mm, " ");

	gk20a_busy_noresume(g);
	if (nvgpu_is_powered_off(g)) {
		gk20a_idle_nosuspend(g);
		return 0;
	}

	retries = 100;

	if (g->ops.mm.get_flush_retries != NULL) {
		retries = g->ops.mm.get_flush_retries(g, NVGPU_FLUSH_FB);
	}

	nvgpu_timeout_init_retry(g, &timeout, retries);

	nvgpu_mutex_acquire(&mm->l2_op_lock);

	/* Make sure all previous writes are committed to the L2. There's no
	   guarantee that writes are to DRAM. This will be a sysmembar internal
	   to the L2. */

#ifdef CONFIG_NVGPU_TRACE
	trace_gk20a_mm_fb_flush(g->name);
#endif

	nvgpu_writel(g, flush_fb_flush_r(),
		flush_fb_flush_pending_busy_f());

	do {
		data = nvgpu_readl(g, flush_fb_flush_r());

		if ((flush_fb_flush_outstanding_v(data) ==
				flush_fb_flush_outstanding_true_v()) ||
			(flush_fb_flush_pending_v(data) ==
				flush_fb_flush_pending_busy_v())) {
				nvgpu_log(g, gpu_dbg_mm, "fb_flush 0x%x", data);
				nvgpu_udelay(5);
		} else {
			break;
		}
	} while (nvgpu_timeout_expired(&timeout) == 0);

	if (nvgpu_timeout_peek_expired(&timeout)) {
		if (g->ops.fb.dump_vpr_info != NULL) {
			g->ops.fb.dump_vpr_info(g);
		}
		if (g->ops.fb.dump_wpr_info != NULL) {
			g->ops.fb.dump_wpr_info(g);
		}
		ret = -EBUSY;
	}

#ifdef CONFIG_NVGPU_TRACE
	trace_gk20a_mm_fb_flush_done(g->name);
#endif

	nvgpu_mutex_release(&mm->l2_op_lock);

	gk20a_idle_nosuspend(g);

	return ret;
}

static void gk20a_mm_l2_invalidate_locked(struct gk20a *g)
{
	u32 data;
	struct nvgpu_timeout timeout;
	u32 retries = 200;

#ifdef CONFIG_NVGPU_TRACE
	trace_gk20a_mm_l2_invalidate(g->name);
#endif

	if (g->ops.mm.get_flush_retries != NULL) {
		retries = g->ops.mm.get_flush_retries(g, NVGPU_FLUSH_L2_INV);
	}

	nvgpu_timeout_init_retry(g, &timeout, retries);

	/* Invalidate any clean lines from the L2 so subsequent reads go to
	   DRAM. Dirty lines are not affected by this operation. */
	nvgpu_writel(g, flush_l2_system_invalidate_r(),
		flush_l2_system_invalidate_pending_busy_f());

	do {
		data = nvgpu_readl(g, flush_l2_system_invalidate_r());

		if ((flush_l2_system_invalidate_outstanding_v(data) ==
			flush_l2_system_invalidate_outstanding_true_v()) ||
			(flush_l2_system_invalidate_pending_v(data) ==
				flush_l2_system_invalidate_pending_busy_v())) {
				nvgpu_log(g, gpu_dbg_mm,
					"l2_system_invalidate 0x%x", data);
				nvgpu_udelay(5);
		} else {
			break;
		}
	} while (nvgpu_timeout_expired(&timeout) == 0);

	if (nvgpu_timeout_peek_expired(&timeout)) {
		nvgpu_warn(g, "l2_system_invalidate too many retries");
	}

#ifdef CONFIG_NVGPU_TRACE
	trace_gk20a_mm_l2_invalidate_done(g->name);
#endif
}

void gk20a_mm_l2_invalidate(struct gk20a *g)
{
	struct mm_gk20a *mm = &g->mm;
	gk20a_busy_noresume(g);
	if (!nvgpu_is_powered_off(g)) {
		nvgpu_mutex_acquire(&mm->l2_op_lock);
		gk20a_mm_l2_invalidate_locked(g);
		nvgpu_mutex_release(&mm->l2_op_lock);
	}
	gk20a_idle_nosuspend(g);
}

int gk20a_mm_l2_flush(struct gk20a *g, bool invalidate)
{
	struct mm_gk20a *mm = &g->mm;
	u32 data;
	struct nvgpu_timeout timeout;
	u32 retries = 2000;
	int err = -ETIMEDOUT;

	nvgpu_log(g, gpu_dbg_mm, " ");

	gk20a_busy_noresume(g);
	if (nvgpu_is_powered_off(g)) {
		gk20a_idle_nosuspend(g);
		return 0;
	}

	if (g->ops.mm.get_flush_retries != NULL) {
		retries = g->ops.mm.get_flush_retries(g, NVGPU_FLUSH_L2_FLUSH);
	}

	nvgpu_timeout_init_retry(g, &timeout, retries);

	nvgpu_mutex_acquire(&mm->l2_op_lock);

#ifdef CONFIG_NVGPU_TRACE
	trace_gk20a_mm_l2_flush(g->name);
#endif

	/* Flush all dirty lines from the L2 to DRAM. Lines are left in the L2
	   as clean, so subsequent reads might hit in the L2. */
	nvgpu_writel(g, flush_l2_flush_dirty_r(),
		flush_l2_flush_dirty_pending_busy_f());

	do {
		data = nvgpu_readl(g, flush_l2_flush_dirty_r());

		if ((flush_l2_flush_dirty_outstanding_v(data) ==
				flush_l2_flush_dirty_outstanding_true_v()) ||
			(flush_l2_flush_dirty_pending_v(data) ==
				flush_l2_flush_dirty_pending_busy_v())) {
				nvgpu_log(g, gpu_dbg_mm, "l2_flush_dirty 0x%x",
					data);
				nvgpu_udelay(5);
		} else {
			err = 0;
			break;
		}
	} while (nvgpu_timeout_expired_msg(&timeout,
				"l2_flush_dirty too many retries") == 0);

#ifdef CONFIG_NVGPU_TRACE
	trace_gk20a_mm_l2_flush_done(g->name);
#endif

	if (invalidate) {
		gk20a_mm_l2_invalidate_locked(g);
	}

	nvgpu_mutex_release(&mm->l2_op_lock);
	gk20a_idle_nosuspend(g);

	return err;
}

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
