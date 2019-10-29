/*
 * GK20A memory interface
 *
 * Copyright (c) 2014-2018, NVIDIA CORPORATION.  All rights reserved.
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

#include <trace/events/gk20a.h>

#include "gk20a.h"
#include "fb_gk20a.h"

#include <nvgpu/timers.h>

#include <nvgpu/hw/gk20a/hw_mc_gk20a.h>
#include <nvgpu/hw/gk20a/hw_fb_gk20a.h>

void fb_gk20a_reset(struct gk20a *g)
{
	u32 val;

	nvgpu_log_info(g, "reset gk20a fb");

	g->ops.mc.reset(g, mc_enable_pfb_enabled_f() |
			mc_enable_l2_enabled_f() |
			mc_enable_xbar_enabled_f() |
			mc_enable_hub_enabled_f());

	val = gk20a_readl(g, mc_elpg_enable_r());
	val |= mc_elpg_enable_xbar_enabled_f()
		| mc_elpg_enable_pfb_enabled_f()
		| mc_elpg_enable_hub_enabled_f();
	gk20a_writel(g, mc_elpg_enable_r(), val);
}

void gk20a_fb_init_hw(struct gk20a *g)
{
	u32 addr = nvgpu_mem_get_addr(g, &g->mm.sysmem_flush) >> 8;

	gk20a_writel(g, fb_niso_flush_sysmem_addr_r(), addr);
}

void gk20a_fb_tlb_invalidate(struct gk20a *g, struct nvgpu_mem *pdb)
{
	struct nvgpu_timeout timeout;
	u32 addr_lo;
	u32 data;

	nvgpu_log_fn(g, " ");

	/* pagetables are considered sw states which are preserved after
	   prepare_poweroff. When gk20a deinit releases those pagetables,
	   common code in vm unmap path calls tlb invalidate that touches
	   hw. Use the power_on flag to skip tlb invalidation when gpu
	   power is turned off */

	if (!g->power_on)
		return;

	addr_lo = u64_lo32(nvgpu_mem_get_addr(g, pdb) >> 12);

	nvgpu_mutex_acquire(&g->mm.tlb_lock);

	trace_gk20a_mm_tlb_invalidate(g->name);

	nvgpu_timeout_init(g, &timeout, 1000, NVGPU_TIMER_RETRY_TIMER);

	do {
		data = gk20a_readl(g, fb_mmu_ctrl_r());
		if (fb_mmu_ctrl_pri_fifo_space_v(data) != 0)
			break;
		nvgpu_udelay(2);
	} while (!nvgpu_timeout_expired_msg(&timeout,
					 "wait mmu fifo space"));

	if (nvgpu_timeout_peek_expired(&timeout))
		goto out;

	nvgpu_timeout_init(g, &timeout, 1000, NVGPU_TIMER_RETRY_TIMER);

	gk20a_writel(g, fb_mmu_invalidate_pdb_r(),
		fb_mmu_invalidate_pdb_addr_f(addr_lo) |
		nvgpu_aperture_mask(g, pdb,
				fb_mmu_invalidate_pdb_aperture_sys_mem_f(),
				fb_mmu_invalidate_pdb_aperture_sys_mem_f(),
				fb_mmu_invalidate_pdb_aperture_vid_mem_f()));

	gk20a_writel(g, fb_mmu_invalidate_r(),
		fb_mmu_invalidate_all_va_true_f() |
		fb_mmu_invalidate_trigger_true_f());

	do {
		data = gk20a_readl(g, fb_mmu_ctrl_r());
		if (fb_mmu_ctrl_pri_fifo_empty_v(data) !=
			fb_mmu_ctrl_pri_fifo_empty_false_f())
			break;
		nvgpu_udelay(2);
	} while (!nvgpu_timeout_expired_msg(&timeout,
					 "wait mmu invalidate"));

	trace_gk20a_mm_tlb_invalidate_done(g->name);

out:
	nvgpu_mutex_release(&g->mm.tlb_lock);
}
