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

#ifndef CIC_RM_PRIV_H
#define CIC_RM_PRIV_H

#include <nvgpu/types.h>
#include <nvgpu/cond.h>
#include <nvgpu/atomic.h>

struct nvgpu_cic_rm {
	/**
	 * One of the condition variables needed to keep track of deferred
	 * interrupts.
	 * The condition variable that is signalled upon handling of the
	 * stalling interrupt. Function #nvgpu_cic_wait_for_stall_interrupts
	 * waits on this condition variable.
	 */
	struct nvgpu_cond sw_irq_stall_last_handled_cond;

	/**
	 * One of the counters needed to keep track of deferred interrupts.
	 * Stalling interrupt status counter - Set to 1 on entering stalling
	 * interrupt handler and reset to 0 on exit.
	 */
	nvgpu_atomic_t sw_irq_stall_pending;

#ifdef CONFIG_NVGPU_NONSTALL_INTR
	/**
	 * One of the condition variables needed to keep track of deferred
	 * interrupts.
	 * The condition variable that is signalled upon handling of the
	 * non-stalling interrupt. Function #nvgpu_cic_wait_for_nonstall_interrupts
	 * waits on this condition variable.
	 */
	struct nvgpu_cond sw_irq_nonstall_last_handled_cond;

	/**
	 * One of the counters needed to keep track of deferred interrupts.
	 * Non-stalling interrupt status counter - Set to 1 on entering
	 * non-stalling interrupt handler and reset to 0 on exit.
	 */
	nvgpu_atomic_t sw_irq_nonstall_pending;
#endif
};

#endif /* CIC_RM_PRIV_H */
