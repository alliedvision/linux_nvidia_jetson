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
#ifndef NVGPU_GOPS_CE_H
#define NVGPU_GOPS_CE_H

#include <nvgpu/types.h>

/**
 * @file
 *
 * CE HAL interface.
 */
struct gk20a;
struct nvgpu_device;
/**
 * CE HAL operations.
 *
 * @see gpu_ops.
 */
struct gops_ce {
	/**
	 * @brief Handler for CE stalling interrupts.
	 *
	 * @param g [in]	       The GPU driver struct.
	 * @param inst_id [in]	       Copy engine instance id.
	 * @param pri_base [in]	       Start of h/w register address space.
	 * @param needs_rc [out]       Flag indicating if recovery should be
	 *                             triggered as part of CE error handling.
	 * @param needs_quiesce [out]  Flag indicating if SW quiesce should be
	 *                             triggered as part of CE error handling.
	 *
	 * This function is invoked by MC stalling isr handler to handle
	 * the CE stalling interrupt.
	 *
	 * Steps:
	 * - Read ce_intr_status_r corresponding to \a inst_id.
	 * - Check if following error interrupts are pending. If pending,
	 *   report that error to the SDL framework and mark the interrupt
	 *   for clearing.
	 *   - Invalid config interrupt.
	 *   - Method buffer fault interrupt.
	 *   - Blocking pipe interrupt.
	 *   - Launch error interrupt.
	 * - Sets needs_rc / needs_quiesce based on error handling policy.
	 * - Clear the handled interrupts by writing to ce_intr_status_r.
	 */
	void (*isr_stall)(struct gk20a *g, u32 inst_id, u32 pri_base,
				bool *needs_rc, bool *needs_quiesce);

#ifdef CONFIG_NVGPU_NONSTALL_INTR
	/**
	 * @brief Handler for CE non-stalling interrupts.
	 *
	 * @param g [in]	The GPU driver struct.
	 * @param inst_id [in]	Copy engine instance id.
	 * @param pri_base [in]	Start of h/w register address space.
	 *
	 * This function is invoked by MC non-stalling isr handler to handle
	 * the CE non-stalling interrupt.
	 *
	 * Steps:
	 * - Read ce_intr_status_r corresponding to \a inst_id.
	 * - If nonblocking pipe interrupt is pending,
	 *   - Get bitmask of #NVGPU_CIC_NONSTALL_OPS_WAKEUP_SEMAPHORE and
	 *     #NVGPU_CIC_NONSTALL_OPS_POST_EVENTS operations.
	 *   - Clear the interrupt.
	 *
	 * @return Bitmask of operations that will need to be executed on
	 *         non-stall workqueue.
	 */
	u32 (*isr_nonstall)(struct gk20a *g, u32 inst_id, u32 pri_base);

	/*
	 * @brief Get non-stall vectors from hw.
	 *
	 * @param g [in]	The GPU driver struct.
	 *
	 * Steps:
	 * - Get a list of non-stall vectors used by the engine
	 *   from the hw register POR values.
	 */
	void (*init_hw)(struct gk20a *g);

#endif

	/**
	 * @brief Get number of PCEs (Physical Copy Engines).
	 *
	 * @param g [in]	The GPU driver struct.
	 *
	 * This function is called to determine the size of the engine method
	 * buffer during tsg initialization.
	 *
	 * Steps:
	 * - Read ce_pce_map_r register that contains a bitmask indicating which
	 *   physical copy engines are present (and not floorswept).
	 * - Compute and return the Hamming weight of the read value.
	 *
	 * @return Number of PCEs.
	 */
	u32 (*get_num_pce)(struct gk20a *g);

#ifdef CONFIG_NVGPU_HAL_NON_FUSA
	/**
	 * @brief Handler for method buffer fault in BAR2.
	 *
	 * @param g [in]	The GPU driver struct.
	 *
	 * This function is called while handling bar2 fault in the fb
	 * interrupt handler.
	 *
	 * Steps:
	 * - For each LCE, check if method buffer fault interrupt is pending and
	 *   clear if pending.
	 */
	void (*mthd_buffer_fault_in_bar2_fault)(struct gk20a *g);
#endif

	/** @cond DOXYGEN_SHOULD_SKIP_THIS */

	int (*ce_init_support)(struct gk20a *g);
	void (*set_pce2lce_mapping)(struct gk20a *g);
	void (*init_prod_values)(struct gk20a *g);
	void (*halt_engine)(struct gk20a *g, const struct nvgpu_device *dev);
	void (*request_idle)(struct gk20a *g);

	/*
	 * @brief Enable/disable ce interrupts.
	 *
	 * @param g [in]	The GPU driver struct.
	 * @param enable [in]	Enable/disable boolean flag.
	 *
	 * Enable/disable CE stall, nonstall interrupts.
	 */
	void (*intr_enable)(struct gk20a *g, bool enable);

	void (*intr_retrigger)(struct gk20a *g, u32 inst_id);

	u64 (*get_inst_ptr_from_lce)(struct gk20a *g, u32 inst_id);
#ifdef CONFIG_NVGPU_DGPU
	int (*ce_app_init_support)(struct gk20a *g);
	void (*ce_app_suspend)(struct gk20a *g);
	void (*ce_app_destroy)(struct gk20a *g);
#endif
	/** @endcond DOXYGEN_SHOULD_SKIP_THIS */
};

#endif/*NVGPU_GOPS_CE_H*/
