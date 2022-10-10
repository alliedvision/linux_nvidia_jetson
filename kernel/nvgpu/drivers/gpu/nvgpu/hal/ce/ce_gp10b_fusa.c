/*
 * Pascal GPU series Copy Engine.
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

#include <nvgpu/io.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/mc.h>
#include <nvgpu/cic_mon.h>
#include <nvgpu/nvgpu_err.h>

#include "ce_gp10b.h"

#include <nvgpu/hw/gp10b/hw_ce_gp10b.h>

void gp10b_ce_stall_isr(struct gk20a *g, u32 inst_id, u32 pri_base,
				bool *needs_rc, bool *needs_quiesce)
{
	u32 ce_intr = nvgpu_readl(g, ce_intr_status_r(inst_id));
	u32 clear_intr = 0U;

	(void)pri_base;

	nvgpu_log(g, gpu_dbg_intr, "ce isr %08x %08x", ce_intr, inst_id);

	/* clear blocking interrupts: they exibit broken behavior */
	if ((ce_intr & ce_intr_status_blockpipe_pending_f()) != 0U) {
		nvgpu_err(g, "ce blocking pipe interrupt");
		clear_intr |= ce_intr_status_blockpipe_pending_f();
	}

	*needs_quiesce |= false;
	*needs_rc |= false;
	nvgpu_writel(g, ce_intr_status_r(inst_id), clear_intr);
	return;
}
