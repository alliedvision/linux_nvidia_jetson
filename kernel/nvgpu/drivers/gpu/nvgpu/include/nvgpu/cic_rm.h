/*
 * Copyright (c) 2021, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef NVGPU_CIC_RM_H
#define NVGPU_CIC_RM_H

#include <nvgpu/log.h>

/**
 * @brief Setup the CIC-RM subunit's data structures
 *
 * @param g [in]	- The GPU driver struct.
 *
 * - Check if CIC-RM subunit is already initialized by checking its
 *   reference in struct gk20a.
 * - If not initialized, allocate memory for CIC-RM's private data
 *   structure.
 * - Store a reference pointer to the CIC struct in struct gk20a.
 * - This API should to be called before any of the CIC-RM
 *   functionality is requested.
 *
 * @return 0 if Initialization had already happened or was
 *           successful in this call.
 *	  < 0 if any steps in initialization fail.
 *
 * @retval -ENOMEM if sufficient memory is not available for CIC-RM
 *            struct.
 *
 */
int nvgpu_cic_rm_setup(struct gk20a *g);

/**
 * @brief Remove the CIC-RM subunit's data structures
 *
 * @param g [in]	- The GPU driver struct.
 *
 * - Check if CIC-RM  subunit is already removed by checking its
 *   reference in struct gk20a.
 * - If not removed already, free the memory allocated for CIC-RM's
 *   private data structure.
 * - Invalidate reference pointer to the CIC-RM struct in struct gk20a.
 * - No CIC-RM functionality will be available after this API is
 *   called.
 *
 * @return 0 if Deinitialization had already happened or was
 *           successful in this call.
 *
 * @retval None.
 */
int nvgpu_cic_rm_remove(struct gk20a *g);

/**
 * @brief Initialize the CIC-RM subunit's data structures
 *
 * @param g [in]	- The GPU driver struct.
 *
 * - Check if CIC-RM subunit's setup is pending.
 * - Initialize the condition variables used to keep track of
 *   deferred interrupts.
 *
 * @return 0 if Initialization is successful.
 *	  < 0 if any steps in initialization fail.
 *
 * @retval -EINVAL if CIC-RM setup is pending.
 *
 */
int nvgpu_cic_rm_init_vars(struct gk20a *g);

/**
 * @brief De-initialize the CIC-RM subunit's data structures
 *
 * @param g [in]	- The GPU driver struct.
 *
 * - Check if CIC-RM  subunit is already removed by checking its
 *   reference in struct gk20a.
 * - Destroy the condition variables used to keep track of
 *   deferred interrupts.
 *
 * @return 0 if Deinitialization had already happened or was
 *           successful in this call.
 *
 * @retval None.
 */
int nvgpu_cic_rm_deinit_vars(struct gk20a *g);

/**
 * @brief Set the stalling interrupt status counter.
 *
 * @param g [in]	- The GPU driver struct.
 * @param value[in]     - Counter value to be set.
 *
 * - Sets the stalling interrupt status counter atomically
 *   to \a value.
 * - This API is called to set the counter to 1 on entering the
 *   stalling interrupt handler and reset to 0 on exit.
 */
void nvgpu_cic_rm_set_irq_stall(struct gk20a *g, u32 value);

/**
 * @brief Set the non-stalling interrupt status counter.
 *
 * @param g [in]	- The GPU driver struct.
 * @param value[in]     - Counter value to be set.
 *
 * - Sets the non-stalling interrupt status counter atomically
 *   to \a value.
 * - This API is called to set the counter to 1 on entering the
 *   non-stalling interrupt handler and reset to 0 on exit.
 */
void nvgpu_cic_rm_set_irq_nonstall(struct gk20a *g, u32 value);

/**
 * @brief Signal the completion of stall interrupt handling.
 *
 * @param g [in]	- The GPU driver struct.
 *
 * - Signal the waitqueues waiting on condition variable that keeps
 *   track of deferred stalling interrupts.
 * - This API is called on completion of stall interrupt handling.
 *
 * @return 0 if waitqueue broadcast call is successful.
 *	  < 0 if broadcast call fails
 */
int nvgpu_cic_rm_broadcast_last_irq_stall(struct gk20a *g);

/**
 * @brief Signal the completion of non-stall interrupt handling.
 *
 * @param g [in]	- The GPU driver struct.
 *
 * - Signal the waitqueues waiting on condition variable that keeps
 *   track of deferred non-stalling interrupts.
 * - This API is called on completion of non-stall interrupt handling.
 *
 * @return 0 if waitqueue broadcast call is successful.
 *	  < 0 if broadcast call fails
 */
int nvgpu_cic_rm_broadcast_last_irq_nonstall(struct gk20a *g);

/**
 * @brief Wait for the stalling interrupts to complete.
 *
 * @param g [in]	The GPU driver struct.
 * @param timeout [in]  Timeout
 *
 * Steps:
 * - Get the stalling interrupts atomic count.
 * - Wait for #timeout duration on the condition variable
 *   #sw_irq_stall_last_handled_cond until #sw_irq_stall_last_handled
 *   becomes greater than or equal to previously read stalling
 *   interrupt atomic count.
 *
 * @retval 0 if wait completes successfully.
 * @retval -ETIMEDOUT if wait completes without stalling interrupts
 * completing.
 */
int nvgpu_cic_rm_wait_for_stall_interrupts(struct gk20a *g, u32 timeout);


/**
 * @brief Wait for the non-stalling interrupts to complete.
 *
 * @param g [in]	The GPU driver struct.
 * @param timeout [in]  Timeout
 *
 * Steps:
 * - Get the non-stalling interrupts atomic count.
 * - Wait for #timeout duration on the condition variable
 *   #sw_irq_nonstall_last_handled_cond until #sw_irq_nonstall_last_handled
 *   becomes greater than or equal to previously read non-stalling
 *   interrupt atomic count.
 *
 * @retval 0 if wait completes successfully.
 * @retval -ETIMEDOUT if wait completes without nonstalling interrupts
 * completing.
 */
int  nvgpu_cic_rm_wait_for_nonstall_interrupts(struct gk20a *g, u32 timeout);

/**
 * @brief Wait for the interrupts to complete.
 *
 * @param g [in]	The GPU driver struct.
 *
 * While freeing the channel or entering SW quiesce state, nvgpu driver needs
 * to wait until all scheduled interrupt handlers have completed. This is
 * because the interrupt handlers could access data structures after freeing.
 * Steps:
 * - Wait for stalling interrupts to complete with timeout disabled.
 * - Wait for non-stalling interrupts to complete with timeout disabled.
 */
void nvgpu_cic_rm_wait_for_deferred_interrupts(struct gk20a *g);

#ifdef CONFIG_NVGPU_NON_FUSA
void nvgpu_cic_rm_log_pending_intrs(struct gk20a *g);
#endif

#endif /* NVGPU_CIC_RM_H */
