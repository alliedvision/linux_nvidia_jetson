/*
 * Copyright (c) 2019-2021, NVIDIA CORPORATION.  All rights reserved.
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
#ifndef NVGPU_GOPS_FIFO_H
#define NVGPU_GOPS_FIFO_H

#include <nvgpu/types.h>

/**
 * @file
 *
 * FIFO HAL interface.
 */
struct gk20a;
struct nvgpu_channel;
struct nvgpu_tsg;
struct mmu_fault_info;

struct gops_fifo {
	/**
 	 * @brief Initialize FIFO unit.
 	 *
 	 * @param g [in]	Pointer to GPU driver struct.
 	 *
	 * This HAL is used to initialize FIFO software context,
	 * then perform GPU h/w initializations. It always maps to
	 * \ref nvpgu_fifo_init_support "nvpgu_fifo_init_support(g)",
	 * except for vgpu case.
	 *
 	 * @return 0 in case of success, < 0 in case of failure.
 	 */
	int (*fifo_init_support)(struct gk20a *g);

	/**
 	 * @brief Suspend FIFO unit.
 	 *
 	 * @param g [in]	Pointer to GPU driver struct.
 	 *
	 * - Disable BAR1 snooping when supported.
	 * - Disable FIFO interrupts
	 *   - Disable FIFO stalling interrupts
	 *   - Disable ctxsw timeout detection, and clear any pending
	 *     ctxsw timeout interrupt.
	 *   - Disable PBDMA interrupts.
	 *   - Disable FIFO non-stalling interrupts.
	 *
 	 * @return 0 in case of success, < 0 in case of failure.
 	 */
	int (*fifo_suspend)(struct gk20a *g);

	/**
 	 * @brief Preempt TSG.
 	 *
 	 * @param g [in]	Pointer to GPU driver struct.
	 * @param tsg [in]	Pointer to TSG struct.
 	 *
	 * - Acquire lock for active runlist.
	 * - Write h/w register to trigger TSG preempt for \a tsg.
	 * - Preemption mode (e.g. CTA or WFI) depends on the preemption
	 *   mode configured in the GR context.
	 * - Release lock acquired for active runlist.
	 * - Poll PBDMAs and engines status until preemption is complete,
	 *   or poll timeout occurs.
	 *
	 * On some chips, it is also needed to disable scheduling
	 * before preempting TSG.
	 *
	 * @see #nvgpu_preempt_get_timeout
	 * @see nvgpu_gr_ctx::compute_preempt_mode
	 *
 	 * @return 0 in case preemption succeeded, < 0 in case of failure.
	 * @retval -ETIMEDOUT when preemption was triggered, but did not
	 *         complete within preemption poll timeout.
 	 */
	int (*preempt_tsg)(struct gk20a *g, struct nvgpu_tsg *tsg);

	/**
	 * @brief Enable and configure FIFO.
	 *
	 * @param g [in]		Pointer to GPU driver struct.
	 *
	 * Reset FIFO unit and configure FIFO h/w settings.
	 * - Enable PMC FIFO.
	 * - Configure clock gating:
	 *   - Set SLCG settings for CE2 and FIFO.
	 *   - Set BLCG settings for FIFO.
	 * - Set FB timeout for FIFO initiated requests.
	 * - Setup PBDMA timeouts.
	 * - Enable FIFO unit stalling and non-stalling interrupts at MC level.
	 * - Enable FIFO stalling and non-stalling interrupts.
	 *
	 * @return 0 in case of success, < 0 in case of failure.
	 */
	int (*reset_enable_hw)(struct gk20a *g);

	/**
	 * @brief ISR for stalling interrupts.
	 *
	 * @param g [in]		Pointer to GPU driver struct.
	 *
	 * Interrupt Service Routine for FIFO stalling interrupts:
	 * - Read interrupt status.
	 * - If sw_ready is false, clear interrupts and return, else
	 * - Acquire FIFO ISR mutex
	 * - Handle interrupts:
	 *  - Handle error interrupts:
	 *    - Report bind, chw, memop timeout and lb errors.
	 *  - Handle runlist event interrupts:
	 *    - Log and clear runlist events.
	 *  - Handle PBDMA interrupts:
	 *    - Set error notifier and reset method (if needed).
	 *    - Report timeout, extra, pb, method, signature, hce and
	 *      preempt errors.
	 *  - Handle scheduling errors interrupts:
	 *    - Log and report sched error.
	 *  - Handle ctxsw timeout interrupts:
	 *    - Get engines with ctxsw timeout.
	 *    - Report error for TSGs on those engines.
	 * - Release FIFO ISR mutex.
	 * - Clear interrupts.
	 *
	 * @note: This HAL is called from a threaded interrupt context.
	 */
	void (*intr_0_isr)(struct gk20a *g);

	/**
	 * @brief ISR for non-stalling interrupts.
	 *
	 * @param g [in]		Pointer to GPU driver struct.
	 *
	 * Interrupt Service Routine for FIFO non-stalling interrupts:
	 * - Read interrupt status.
	 * - Clear channel interrupt if pending.
	 *
	 * @return: #NVGPU_CIC_NONSTALL_OPS_WAKEUP_SEMAPHORE
	 */
	u32  (*intr_1_isr)(struct gk20a *g);

	/**
	 * @brief Initialize and read chip specific HW data.
	 *
	 * @param g [in]	The GPU driver struct.
	 *                      - The function does not perform g parameter validation.
	 *
	 * For gv11b, this pointer is mapped to \ref gv11b_init_fifo_setup_hw(g).
	 *
	 * @return error as an integer.
	 */
	int (*init_fifo_setup_hw)(struct gk20a *g);

	/**
	 * @brief Initialize FIFO software metadata and mark it ready to be used.
	 *
	 * @param g [in]	The GPU driver struct.
	 *                      - The function does not perform g parameter validation.
	 *
	 * - Check if #nvgpu_fifo.sw_ready is set to true i.e. s/w setup is
	 * already done (pointer to nvgpu_fifo is obtained using g->fifo).
	 * In case setup is ready, return 0, else continue to setup.
	 * - Invoke \ref nvgpu_fifo_setup_sw_common(g) to perform sw setup.
	 * - Mark FIFO sw setup ready by setting #nvgpu_fifo.sw_ready to true.
	 *
	 * @retval 0 in case of success.
	 * @retval -ENOMEM in case there is not enough memory available.
	 * @retval -EINVAL in case condition variable has invalid value.
	 * @retval -EBUSY in case reference condition variable pointer isn't NULL.
	 * @retval -EFAULT in case any faults occurred while accessing condition
	 * variable or attribute.
	 */
	int (*setup_sw)(struct gk20a *g);


	/** @cond DOXYGEN_SHOULD_SKIP_THIS */


	void (*cleanup_sw)(struct gk20a *g);

	int (*preempt_channel)(struct gk20a *g, struct nvgpu_channel *ch);
	/**
	 * @brief Preempt requested channel,tsg or runlist.
	 *
	 * @param g [in]		Pointer to GPU driver struct.
	 * @param id [in]		Tsg or channel or hardware runlist id
	 * @param id_type [in]		channel,tsg or runlist type
	 *
	 * Depending on given \a id_type:
	 * - Preempt channel
	 * - Preempt tsg
	 * - Preempt runlist
	 *
	 * @return: None
	 */
	void (*preempt_trigger)(struct gk20a *g, u32 id, unsigned int id_type);
	int (*preempt_poll_pbdma)(struct gk20a *g, u32 tsgid,
			 u32 pbdma_id);
	int (*is_preempt_pending)(struct gk20a *g, u32 id,
		unsigned int id_type, bool preempt_retries_left);
	void (*intr_set_recover_mask)(struct gk20a *g);
	void (*intr_unset_recover_mask)(struct gk20a *g);
	void (*intr_top_enable)(struct gk20a *g, bool enable);
	void (*intr_0_enable)(struct gk20a *g, bool enable);
	void (*intr_1_enable)(struct gk20a *g, bool enable);
	bool (*handle_sched_error)(struct gk20a *g);
	void (*ctxsw_timeout_enable)(struct gk20a *g, bool enable);
	bool (*handle_ctxsw_timeout)(struct gk20a *g);
	void (*trigger_mmu_fault)(struct gk20a *g,
			unsigned long engine_ids_bitmask);
	void (*get_mmu_fault_info)(struct gk20a *g, u32 mmu_fault_id,
		struct mmu_fault_info *mmfault);
	void (*get_mmu_fault_desc)(struct mmu_fault_info *mmfault);
	void (*get_mmu_fault_client_desc)(
				struct mmu_fault_info *mmfault);
	void (*get_mmu_fault_gpc_desc)(struct mmu_fault_info *mmfault);
	u32 (*get_runlist_timeslice)(struct gk20a *g);
	u32 (*get_pb_timeslice)(struct gk20a *g);
	bool (*is_mmu_fault_pending)(struct gk20a *g);
	u32  (*mmu_fault_id_to_pbdma_id)(struct gk20a *g,
				u32 mmu_fault_id);
	void (*bar1_snooping_disable)(struct gk20a *g);
	bool (*find_pbdma_for_runlist)(struct gk20a *g,
				       u32 runlist_id, u32 *pbdma_id);
	void (*runlist_intr_retrigger)(struct gk20a *g, u32 intr_tree);

#ifdef CONFIG_NVGPU_RECOVERY
	void (*recover)(struct gk20a *g, u32 act_eng_bitmask,
		u32 id, unsigned int id_type, unsigned int rc_type,
		 struct mmu_fault_info *mmfault);
#endif

#ifdef CONFIG_NVGPU_DEBUGGER
	int (*set_sm_exception_type_mask)(struct nvgpu_channel *ch,
			u32 exception_mask);
#endif
	/** @endcond DOXYGEN_SHOULD_SKIP_THIS */

};

#endif
