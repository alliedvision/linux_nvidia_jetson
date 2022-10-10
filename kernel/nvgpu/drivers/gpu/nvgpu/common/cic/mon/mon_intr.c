/*
 * Copyright (c) 2021-2022, NVIDIA CORPORATION.  All rights reserved.
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

#include <nvgpu/cic_mon.h>
#include <nvgpu/cic_rm.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/bug.h>
#include <nvgpu/nvgpu_init.h>
#include <nvgpu/trace.h>

#include "cic_mon_priv.h"

void nvgpu_cic_mon_intr_mask(struct gk20a *g)
{
	unsigned long flags = 0;

	if (g->ops.mc.intr_mask != NULL) {
		nvgpu_spinlock_irqsave(&g->mc.intr_lock, flags);
		g->ops.mc.intr_mask(g);
		nvgpu_spinunlock_irqrestore(&g->mc.intr_lock, flags);
	}
}

void nvgpu_cic_mon_intr_stall_unit_config(struct gk20a *g, u32 unit, bool enable)
{
	unsigned long flags = 0;

	nvgpu_spinlock_irqsave(&g->mc.intr_lock, flags);
	g->ops.mc.intr_stall_unit_config(g, unit, enable);
	nvgpu_spinunlock_irqrestore(&g->mc.intr_lock, flags);
}

#ifdef CONFIG_NVGPU_NONSTALL_INTR
void nvgpu_cic_mon_intr_nonstall_unit_config(struct gk20a *g, u32 unit, bool enable)
{
	unsigned long flags = 0;

	nvgpu_spinlock_irqsave(&g->mc.intr_lock, flags);
	g->ops.mc.intr_nonstall_unit_config(g, unit, enable);
	nvgpu_spinunlock_irqrestore(&g->mc.intr_lock, flags);
}
#endif

void nvgpu_cic_mon_intr_stall_pause(struct gk20a *g)
{
	unsigned long flags = 0;

	nvgpu_spinlock_irqsave(&g->mc.intr_lock, flags);
	g->ops.mc.intr_stall_pause(g);
	nvgpu_spinunlock_irqrestore(&g->mc.intr_lock, flags);
}

void nvgpu_cic_mon_intr_stall_resume(struct gk20a *g)
{
	unsigned long flags = 0;

	nvgpu_spinlock_irqsave(&g->mc.intr_lock, flags);
	g->ops.mc.intr_stall_resume(g);
	nvgpu_spinunlock_irqrestore(&g->mc.intr_lock, flags);
}

#ifdef CONFIG_NVGPU_NONSTALL_INTR
void nvgpu_cic_mon_intr_nonstall_pause(struct gk20a *g)
{
	unsigned long flags = 0;

	nvgpu_spinlock_irqsave(&g->mc.intr_lock, flags);
	g->ops.mc.intr_nonstall_pause(g);
	nvgpu_spinunlock_irqrestore(&g->mc.intr_lock, flags);
}

void nvgpu_cic_mon_intr_nonstall_resume(struct gk20a *g)
{
	unsigned long flags = 0;

	nvgpu_spinlock_irqsave(&g->mc.intr_lock, flags);
	g->ops.mc.intr_nonstall_resume(g);
	nvgpu_spinunlock_irqrestore(&g->mc.intr_lock, flags);
}

static void nvgpu_cic_mon_intr_nonstall_work(struct gk20a *g, u32 work_ops)
{
	bool semaphore_wakeup, post_events;

	semaphore_wakeup =
		(((work_ops & NVGPU_CIC_NONSTALL_OPS_WAKEUP_SEMAPHORE) != 0U) ?
					true : false);
	post_events = (((work_ops & NVGPU_CIC_NONSTALL_OPS_POST_EVENTS) != 0U) ?
					true : false);

	if (semaphore_wakeup) {
		g->ops.semaphore_wakeup(g, post_events);
	}
}

u32 nvgpu_cic_mon_intr_nonstall_isr(struct gk20a *g)
{
	u32 non_stall_intr_val = 0U;

	if (nvgpu_is_powered_off(g)) {
		return NVGPU_CIC_INTR_UNMASK;
	}

	/* not from gpu when sharing irq with others */
	non_stall_intr_val = g->ops.mc.intr_nonstall(g);
	if (non_stall_intr_val == 0U) {
		return NVGPU_CIC_INTR_NONE;
	}

	nvgpu_cic_mon_intr_nonstall_pause(g);
	if (g->sw_quiesce_pending) {
		return NVGPU_CIC_INTR_QUIESCE_PENDING;
	}

	nvgpu_cic_rm_set_irq_nonstall(g, 1);

	return NVGPU_CIC_INTR_HANDLE;
}

void nvgpu_cic_mon_intr_nonstall_handle(struct gk20a *g)
{
	u32 nonstall_ops = 0;

	nonstall_ops = g->ops.mc.isr_nonstall(g);
	if (nonstall_ops != 0U) {
		nvgpu_cic_mon_intr_nonstall_work(g, nonstall_ops);
	}

	/* sync handled irq counter before re-enabling interrupts */
	nvgpu_cic_rm_set_irq_nonstall(g, 0);

	nvgpu_cic_mon_intr_nonstall_resume(g);

	(void)nvgpu_cic_rm_broadcast_last_irq_nonstall(g);
}
#endif

u32 nvgpu_cic_mon_intr_stall_isr(struct gk20a *g)
{
	u32 mc_intr_0 = 0U;

	nvgpu_trace_intr_stall_start(g);

	if (nvgpu_is_powered_off(g)) {
		return NVGPU_CIC_INTR_UNMASK;
	}

	/* not from gpu when sharing irq with others */
	mc_intr_0 = g->ops.mc.intr_stall(g);
	if (mc_intr_0 == 0U) {
		return NVGPU_CIC_INTR_NONE;
	}

	nvgpu_cic_mon_intr_stall_pause(g);

	if (g->sw_quiesce_pending) {
		return NVGPU_CIC_INTR_QUIESCE_PENDING;
	}

	nvgpu_cic_rm_set_irq_stall(g, 1);

	nvgpu_trace_intr_stall_done(g);

	return NVGPU_CIC_INTR_HANDLE;
}

void nvgpu_cic_mon_intr_stall_handle(struct gk20a *g)
{
	g->ops.mc.isr_stall(g);

	/* sync handled irq counter before re-enabling interrupts */
	nvgpu_cic_rm_set_irq_stall(g, 0);

	nvgpu_cic_mon_intr_stall_resume(g);

	(void)nvgpu_cic_rm_broadcast_last_irq_stall(g);
}

void nvgpu_cic_mon_intr_enable(struct gk20a *g)
{
	unsigned long flags = 0;

	if (g->ops.mc.intr_enable != NULL) {
		nvgpu_spinlock_irqsave(&g->mc.intr_lock, flags);
		g->ops.mc.intr_enable(g);
		nvgpu_spinunlock_irqrestore(&g->mc.intr_lock, flags);
	}
}

void nvgpu_cic_mon_intr_unit_vectorid_init(struct gk20a *g, u32 unit, u32 *vectorid,
		u32 num_entries)
{
	unsigned long flags = 0;
	u32 i = 0U;
	struct nvgpu_intr_unit_info *intr_unit_info;

	nvgpu_assert(num_entries <= NVGPU_CIC_INTR_VECTORID_SIZE_MAX);

	nvgpu_log(g, gpu_dbg_intr, "UNIT=%d, nvecs=%d", unit, num_entries);

	intr_unit_info = g->mc.intr_unit_info;

	nvgpu_spinlock_irqsave(&g->mc.intr_lock, flags);

	if (intr_unit_info[unit].valid == false) {
		for (i = 0U; i < num_entries; i++) {
			nvgpu_log(g, gpu_dbg_intr, " vec[%d] = %d", i,
					*(vectorid + i));
			intr_unit_info[unit].vectorid[i] = *(vectorid + i);
		}
		intr_unit_info[unit].vectorid_size = num_entries;
	}
	nvgpu_spinunlock_irqrestore(&g->mc.intr_lock, flags);
}

bool nvgpu_cic_mon_intr_is_unit_info_valid(struct gk20a *g, u32 unit)
{
	struct nvgpu_intr_unit_info *intr_unit_info;
	bool info_valid = false;

	if (unit >= NVGPU_CIC_INTR_UNIT_MAX) {
		nvgpu_err(g, "invalid unit(%d)", unit);
		return false;
	}

	intr_unit_info = g->mc.intr_unit_info;

	if (intr_unit_info[unit].valid == true) {
		info_valid = true;
	}

	return info_valid;
}

bool nvgpu_cic_mon_intr_get_unit_info(struct gk20a *g, u32 unit, u32 *subtree,
		u64 *subtree_mask)
{
	if (unit >= NVGPU_CIC_INTR_UNIT_MAX) {
		nvgpu_err(g, "invalid unit(%d)", unit);
		return false;
	}
	if (nvgpu_cic_mon_intr_is_unit_info_valid(g, unit) != true) {
		if (g->ops.mc.intr_get_unit_info(g, unit) != true) {
			nvgpu_err(g, "failed to fetch info for unit(%d)", unit);
			return false;
		}
	}
	*subtree = g->mc.intr_unit_info[unit].subtree;
	*subtree_mask = g->mc.intr_unit_info[unit].subtree_mask;
	nvgpu_log(g, gpu_dbg_intr, "subtree(%d) subtree_mask(%llx)",
			*subtree, *subtree_mask);

	return true;
}
