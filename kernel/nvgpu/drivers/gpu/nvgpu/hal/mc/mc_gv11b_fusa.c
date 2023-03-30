/*
 * GV11B master
 *
 * Copyright (c) 2016-2022, NVIDIA CORPORATION.  All rights reserved.
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

#include <nvgpu/types.h>
#include <nvgpu/io.h>
#include <nvgpu/mc.h>
#include <nvgpu/cic_mon.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/engines.h>

#include "mc_gv11b.h"

#include <nvgpu/hw/gv11b/hw_mc_gv11b.h>

bool gv11b_mc_is_intr_hub_pending(struct gk20a *g, u32 mc_intr_0)
{
	(void)g;
	return ((mc_intr_0 & mc_intr_hub_pending_f()) != 0U);
}

bool gv11b_mc_is_stall_and_eng_intr_pending(struct gk20a *g, u32 engine_id,
			u32 *eng_intr_pending)
{
	u32 mc_intr_0 = nvgpu_readl(g, mc_intr_r(NVGPU_CIC_INTR_STALLING));
	u32 stall_intr, eng_intr_mask;

	eng_intr_mask = nvgpu_engine_act_interrupt_mask(g, engine_id);
	*eng_intr_pending = mc_intr_0 & eng_intr_mask;

	stall_intr = mc_intr_pfifo_pending_f() |
			mc_intr_hub_pending_f() |
			mc_intr_priv_ring_pending_f() |
			mc_intr_ltc_pending_f();

	nvgpu_log(g, gpu_dbg_info | gpu_dbg_intr,
		"mc_intr_0 = 0x%08x, eng_intr = 0x%08x",
		mc_intr_0 & stall_intr, *eng_intr_pending);

	return (mc_intr_0 & (eng_intr_mask | stall_intr)) != 0U;
}

bool gv11b_mc_is_mmu_fault_pending(struct gk20a *g)
{
	return g->ops.fb.intr.is_mmu_fault_pending(g);
}
