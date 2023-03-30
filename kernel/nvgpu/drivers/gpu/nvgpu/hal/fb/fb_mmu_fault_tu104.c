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

#include <nvgpu/log.h>
#include <nvgpu/types.h>
#include <nvgpu/timers.h>
#include <nvgpu/nvgpu_mem.h>
#include <nvgpu/io.h>
#include <nvgpu/utils.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/bug.h>
#include <nvgpu/mmu_fault.h>

#include "hal/mc/mc_tu104.h"
#include "hal/fb/fb_mmu_fault_gv11b.h"
#include "hal/fb/fb_mmu_fault_tu104.h"
#include "hal/mm/mmu_fault/mmu_fault_gv11b.h"

#include "nvgpu/hw/tu104/hw_fb_tu104.h"
#include "nvgpu/hw/tu104/hw_func_tu104.h"

void tu104_fb_handle_mmu_fault(struct gk20a *g)
{
	u32 info_fault = nvgpu_readl(g, fb_mmu_int_vector_info_fault_r());
	u32 nonreplay_fault = nvgpu_readl(g,
		fb_mmu_int_vector_fault_r(NVGPU_MMU_FAULT_NONREPLAY_REG_INDX));
#ifdef CONFIG_NVGPU_REPLAYABLE_FAULT
	u32 replay_fault = nvgpu_readl(g,
		fb_mmu_int_vector_fault_r(NVGPU_MMU_FAULT_REPLAY_REG_INDX));
#endif
	u32 fault_status = g->ops.fb.read_mmu_fault_status(g);

	nvgpu_log(g, gpu_dbg_intr, "mmu_fault_status = 0x%08x", fault_status);

	if (intr_tu104_vector_intr_pending(g,
			fb_mmu_int_vector_info_fault_vector_v(info_fault))) {
		intr_tu104_intr_clear_leaf_vector(g,
			fb_mmu_int_vector_info_fault_vector_v(info_fault));

		gv11b_fb_handle_dropped_mmu_fault(g, fault_status);
		gv11b_mm_mmu_fault_handle_other_fault_notify(g, fault_status);
	}

	if (gv11b_fb_is_fault_buf_enabled(g,
			NVGPU_MMU_FAULT_NONREPLAY_REG_INDX)) {
		if (intr_tu104_vector_intr_pending(g,
				fb_mmu_int_vector_fault_notify_v(
					nonreplay_fault))) {
			intr_tu104_intr_clear_leaf_vector(g,
				fb_mmu_int_vector_fault_notify_v(
					nonreplay_fault));

			gv11b_mm_mmu_fault_handle_nonreplay_replay_fault(g,
					fault_status,
					NVGPU_MMU_FAULT_NONREPLAY_REG_INDX);

			/*
			 * When all the faults are processed,
			 * GET and PUT will have same value and mmu fault status
			 * bit will be reset by HW
			 */
		}

		if (intr_tu104_vector_intr_pending(g,
				fb_mmu_int_vector_fault_error_v(nonreplay_fault))) {
			intr_tu104_intr_clear_leaf_vector(g,
				fb_mmu_int_vector_fault_error_v(nonreplay_fault));

			gv11b_fb_handle_nonreplay_fault_overflow(g,
				 fault_status);
		}
	}

#ifdef CONFIG_NVGPU_REPLAYABLE_FAULT
	if (gv11b_fb_is_fault_buf_enabled(g,
			NVGPU_MMU_FAULT_REPLAY_REG_INDX)) {
		if (intr_tu104_vector_intr_pending(g,
				fb_mmu_int_vector_fault_notify_v(replay_fault))) {
			intr_tu104_intr_clear_leaf_vector(g,
				fb_mmu_int_vector_fault_notify_v(replay_fault));

			gv11b_mm_mmu_fault_handle_nonreplay_replay_fault(g,
					fault_status,
					NVGPU_MMU_FAULT_REPLAY_REG_INDX);
		}

		if (intr_tu104_vector_intr_pending(g,
				fb_mmu_int_vector_fault_error_v(replay_fault))) {
			intr_tu104_intr_clear_leaf_vector(g,
				fb_mmu_int_vector_fault_error_v(replay_fault));

			gv11b_fb_handle_replay_fault_overflow(g,
				 fault_status);
		}
	}
#endif

	nvgpu_log(g, gpu_dbg_intr, "clear mmu fault status");
	g->ops.fb.write_mmu_fault_status(g,
				fb_mmu_fault_status_valid_clear_f());
}

void tu104_fb_write_mmu_fault_buffer_lo_hi(struct gk20a *g, u32 index,
	u32 addr_lo, u32 addr_hi)
{
	nvgpu_func_writel(g,
		func_priv_mmu_fault_buffer_lo_r(index), addr_lo);
	nvgpu_func_writel(g,
		func_priv_mmu_fault_buffer_hi_r(index), addr_hi);
}

u32 tu104_fb_read_mmu_fault_buffer_get(struct gk20a *g, u32 index)
{
	return nvgpu_func_readl(g,
		func_priv_mmu_fault_buffer_get_r(index));
}

void tu104_fb_write_mmu_fault_buffer_get(struct gk20a *g, u32 index,
	u32 reg_val)
{
	nvgpu_func_writel(g,
		func_priv_mmu_fault_buffer_get_r(index),
			reg_val);
}

u32 tu104_fb_read_mmu_fault_buffer_put(struct gk20a *g, u32 index)
{
	return nvgpu_func_readl(g,
		func_priv_mmu_fault_buffer_put_r(index));
}

u32 tu104_fb_read_mmu_fault_buffer_size(struct gk20a *g, u32 index)
{
	return nvgpu_func_readl(g,
		func_priv_mmu_fault_buffer_size_r(index));
}

void tu104_fb_write_mmu_fault_buffer_size(struct gk20a *g, u32 index,
	u32 reg_val)
{
	nvgpu_func_writel(g,
		func_priv_mmu_fault_buffer_size_r(index),
			reg_val);
}

void tu104_fb_read_mmu_fault_addr_lo_hi(struct gk20a *g,
	u32 *addr_lo, u32 *addr_hi)
{
	*addr_lo = nvgpu_func_readl(g,
			func_priv_mmu_fault_addr_lo_r());
	*addr_hi = nvgpu_func_readl(g,
			func_priv_mmu_fault_addr_hi_r());
}

void tu104_fb_read_mmu_fault_inst_lo_hi(struct gk20a *g,
	u32 *inst_lo, u32 *inst_hi)
{
	*inst_lo = nvgpu_func_readl(g,
			func_priv_mmu_fault_inst_lo_r());
	*inst_hi = nvgpu_func_readl(g,
			func_priv_mmu_fault_inst_hi_r());
}

u32 tu104_fb_read_mmu_fault_info(struct gk20a *g)
{
	return nvgpu_func_readl(g,
			func_priv_mmu_fault_info_r());
}

u32 tu104_fb_read_mmu_fault_status(struct gk20a *g)
{
	return nvgpu_func_readl(g,
			func_priv_mmu_fault_status_r());
}

void tu104_fb_write_mmu_fault_status(struct gk20a *g, u32 reg_val)
{
	nvgpu_func_writel(g, func_priv_mmu_fault_status_r(),
			   reg_val);
}

#ifdef CONFIG_NVGPU_REPLAYABLE_FAULT
int tu104_fb_mmu_invalidate_replay(struct gk20a *g,
			 u32 invalidate_replay_val)
{
	int err = -ETIMEDOUT;
	u32 reg_val;
	struct nvgpu_timeout timeout;

	nvgpu_log_fn(g, " ");

	nvgpu_timeout_init_retry(g, &timeout, 200U);

	nvgpu_mutex_acquire(&g->mm.tlb_lock);

	reg_val = nvgpu_func_readl(g, func_priv_mmu_invalidate_r());

	reg_val |= fb_mmu_invalidate_all_va_true_f() |
		fb_mmu_invalidate_all_pdb_true_f() |
		invalidate_replay_val |
		fb_mmu_invalidate_trigger_true_f();

	nvgpu_func_writel(g, func_priv_mmu_invalidate_r(), reg_val);

	do {
		reg_val = nvgpu_func_readl(g,
				func_priv_mmu_invalidate_r());
		if (fb_mmu_invalidate_trigger_v(reg_val) !=
				fb_mmu_invalidate_trigger_true_v()) {
			err = 0;
			break;
		}
		nvgpu_udelay(5);
	} while (nvgpu_timeout_expired_msg(&timeout,
			"invalidate replay failed on 0x%x",
			invalidate_replay_val) == 0);
	if (err != 0) {
		nvgpu_err(g, "invalidate replay timedout");
	}

	nvgpu_mutex_release(&g->mm.tlb_lock);
	return err;
}
#endif
