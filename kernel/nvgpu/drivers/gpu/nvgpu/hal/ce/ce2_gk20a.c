/*
 * GK20A Graphics Copy Engine  (gr host)
 *
 * Copyright (c) 2011-2022, NVIDIA CORPORATION.  All rights reserved.
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

#include <nvgpu/kmem.h>
#include <nvgpu/dma.h>
#include <nvgpu/log.h>
#include <nvgpu/enabled.h>
#include <nvgpu/io.h>
#include <nvgpu/utils.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/cic_mon.h>
#include <nvgpu/mc.h>
#include <nvgpu/channel.h>
#include <nvgpu/engines.h>

#include "ce2_gk20a.h"

#include <nvgpu/hw/gk20a/hw_ce2_gk20a.h>

void gk20a_ce2_stall_isr(struct gk20a *g, u32 inst_id, u32 pri_base,
				bool *needs_rc, bool *needs_quiesce)
{
	u32 ce2_intr = nvgpu_readl(g, ce2_intr_status_r());
	u32 clear_intr = 0U;

	(void)inst_id;
	(void)pri_base;

	nvgpu_log(g, gpu_dbg_intr, "ce2 isr %08x", ce2_intr);

	/* clear blocking interrupts: they exibit broken behavior */
	if ((ce2_intr & ce2_intr_status_blockpipe_pending_f()) != 0U) {
		nvgpu_log(g, gpu_dbg_intr, "ce2 blocking pipe interrupt");
		clear_intr |= ce2_intr_status_blockpipe_pending_f();
	}
	if ((ce2_intr & ce2_intr_status_launcherr_pending_f()) != 0U) {
		nvgpu_log(g, gpu_dbg_intr, "ce2 launch error interrupt");
		*needs_rc |= true;
		clear_intr |= ce2_intr_status_launcherr_pending_f();
	}

	*needs_quiesce |= false;
	nvgpu_writel(g, ce2_intr_status_r(), clear_intr);
}

u32 gk20a_ce2_nonstall_isr(struct gk20a *g, u32 inst_id, u32 pri_base)
{
	u32 ops = 0U;
	u32 ce2_intr = nvgpu_readl(g, ce2_intr_status_r());

	(void)inst_id;
	(void)pri_base;

	nvgpu_log(g, gpu_dbg_intr, "ce2 nonstall isr %08x", ce2_intr);

	if ((ce2_intr & ce2_intr_status_nonblockpipe_pending_f()) != 0U) {
		nvgpu_log(g, gpu_dbg_intr, "ce2 non-blocking pipe interrupt");
		nvgpu_writel(g, ce2_intr_status_r(),
			ce2_intr_status_nonblockpipe_pending_f());
		ops |= (NVGPU_CIC_NONSTALL_OPS_WAKEUP_SEMAPHORE |
			NVGPU_CIC_NONSTALL_OPS_POST_EVENTS);
	}
	return ops;
}
