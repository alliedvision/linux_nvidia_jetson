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

#include <nvgpu/cic_rm.h>
#include <nvgpu/gk20a.h>

#include "cic_rm_priv.h"

void nvgpu_cic_rm_set_irq_stall(struct gk20a *g, u32 value)
{
	nvgpu_atomic_set(&g->cic_rm->sw_irq_stall_pending, (int)value);
}

#ifdef CONFIG_NVGPU_NONSTALL_INTR
void nvgpu_cic_rm_set_irq_nonstall(struct gk20a *g, u32 value)
{
	nvgpu_atomic_set(&g->cic_rm->sw_irq_nonstall_pending, (int)value);
}
#endif

int nvgpu_cic_rm_broadcast_last_irq_stall(struct gk20a *g)
{
	int err = 0;

	err = nvgpu_cond_broadcast(
		&g->cic_rm->sw_irq_stall_last_handled_cond);
	if (err != 0) {
		nvgpu_err(g,
			"Last IRQ stall cond_broadcast failed err=%d",
			err);
	}

	return err;
}

#ifdef CONFIG_NVGPU_NONSTALL_INTR
int nvgpu_cic_rm_broadcast_last_irq_nonstall(struct gk20a *g)
{
	int err = 0;

	err = nvgpu_cond_broadcast(
		&g->cic_rm->sw_irq_nonstall_last_handled_cond);
	if (err != 0) {
		nvgpu_err(g,
			"Last IRQ nonstall cond_broadcast failed err=%d",
			err);
	}

	return err;
}
#endif

int nvgpu_cic_rm_wait_for_stall_interrupts(struct gk20a *g, u32 timeout)
{
	/* wait until all stalling irqs are handled */
	return NVGPU_COND_WAIT(&g->cic_rm->sw_irq_stall_last_handled_cond,
			nvgpu_atomic_read(&g->cic_rm->sw_irq_stall_pending) == 0,
			timeout);
}

#ifdef CONFIG_NVGPU_NONSTALL_INTR
int nvgpu_cic_rm_wait_for_nonstall_interrupts(struct gk20a *g, u32 timeout)
{
	/* wait until all non-stalling irqs are handled */
	return NVGPU_COND_WAIT(&g->cic_rm->sw_irq_nonstall_last_handled_cond,
			nvgpu_atomic_read(&g->cic_rm->sw_irq_nonstall_pending) == 0,
			timeout);
}
#endif

void nvgpu_cic_rm_wait_for_deferred_interrupts(struct gk20a *g)
{
	int ret;

	ret = nvgpu_cic_rm_wait_for_stall_interrupts(g, 0U);
	if (ret != 0) {
		nvgpu_err(g, "wait for stall interrupts failed %d", ret);
	}

#ifdef CONFIG_NVGPU_NONSTALL_INTR
	ret = nvgpu_cic_rm_wait_for_nonstall_interrupts(g, 0U);
	if (ret != 0) {
		nvgpu_err(g, "wait for nonstall interrupts failed %d", ret);
	}
#endif
}

#ifdef CONFIG_NVGPU_NON_FUSA
void nvgpu_cic_rm_log_pending_intrs(struct gk20a *g)
{
	if (g->ops.mc.log_pending_intrs != NULL) {
		g->ops.mc.log_pending_intrs(g);
	}
}
#endif
