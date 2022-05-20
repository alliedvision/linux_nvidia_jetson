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

#ifndef NVGPU_GR_INTR_H
#define NVGPU_GR_INTR_H

#include <nvgpu/types.h>

/**
 * @file
 *
 * common.gr.intr unit interface
 */
struct gk20a;
struct nvgpu_channel;
struct nvgpu_gr_intr_info;
struct nvgpu_gr_tpc_exception;
struct nvgpu_gr_isr_data;
struct nvgpu_gr_intr;

/**
 * @brief Handle all FECS error interrupts.
 *
 * @param g [in]		Pointer to GPU driver struct.
 * @param ch [in]		Pointer to GPU channel.
 * @param isr_data [in]		Pointer to GR ISR data.
 *
 * This function handles all error interrupts coming from
 * FECS microcontroller. Errors include:
 * - Fault during ctxsw transaction.
 * - Watchdog trigger.
 * - Unsupported firmware method.
 * - Ctxsw error interrupts i.e. HOST_INT0 interrupts.
 *
 * Context save completion interrupt i.e. HOST_INT1 interrupt is also
 * handled in this function.
 *
 * This function will also report the fatal errors to qnx.sdl unit.
 *
 * @return 0 in case interrupt was handled cleanly, < 0 in case driver
 *         should be moved to quiesce state.
 */
int nvgpu_gr_intr_handle_fecs_error(struct gk20a *g, struct nvgpu_channel *ch,
		struct nvgpu_gr_isr_data *isr_data);

/**
 * @brief Handle all GPC exception interrupts.
 *
 * @param g [in]		Pointer to GPU driver struct.
 * @param post_event [out]	Flag to post event, ignored for safety.
 * @param gr_config [in]	Pointer to GR configuration struct.
 * @param fault_ch [in]		Pointer to faulted GPU channel.
 * @param hww_global_esr [out]	Global Error Status Register value.
 *
 * This function handles all exception interrupts coming from
 * GPC (Graphics Processing Cluster). Interrupts include interrupts
 * triggered from various units within GPC e.g. TPC, GCC, GPCCS, GPCMMU,
 * ZCULL, PES, etc.
 *
 * Interrupts triggered from TPC can be further decomposed into
 * interrupts coming from its subunits e.g. SM, TEX, MPC, PE, etc.
 *
 * This function will also report the fatal errors to qnx.sdl unit.
 *
 * @return 0 in case interrupt was handled cleanly, < 0 in case driver
 *         should be moved to quiesce state.
 */
int nvgpu_gr_intr_handle_gpc_exception(struct gk20a *g, bool *post_event,
	struct nvgpu_gr_config *gr_config, struct nvgpu_channel *fault_ch,
	u32 *hww_global_esr);

/**
 * @brief Handle notification interrupt.
 *
 * @param g [in]		Pointer to GPU driver struct.
 * @param isr_data [in]		Pointer to GR ISR data.
 *
 * This function handles notification interrupt and broadcasts the
 * notification to waiters.
 */
void nvgpu_gr_intr_handle_notify_pending(struct gk20a *g,
		struct nvgpu_gr_isr_data *isr_data);

/**
 * @brief Handle semaphore interrupt.
 *
 * @param g [in]		Pointer to GPU driver struct.
 * @param isr_data [in]		Pointer to GR ISR data.
 *
 * This function handles semaphore release notification interrupt
 * and broadcasts the notification to waiters.
 */
void nvgpu_gr_intr_handle_semaphore_pending(struct gk20a *g,
		struct nvgpu_gr_isr_data *isr_data);

/**
 * @brief Report GR exceptions to qnx.sdl unit.
 *
 * @param g [in]		Pointer to GPU driver struct.
 * @param inst [in]		Unit instance ID.
 * @param err_type [in]		Error type.
 * @param status [in]		Exception status value.
 * @param sub_err_type [in]	Sub error type.
 *
 * This function reports all GR exceptions to qnx.sdl unit.
 *
 * Other interrupt handling functions like #nvgpu_gr_intr_handle_fecs_error()
 * call this function to report exceptions to qnx.sdl.
 */
void nvgpu_gr_intr_report_exception(struct gk20a *g, u32 inst,
		u32 err_type, u32 status, u32 sub_err_type);

/**
 * @brief Translate context to channel ID.
 *
 * @param g [in]		Pointer to GPU driver struct.
 * @param curr_ctx [in]		Context value.
 * @param curr_tsgid [out]	TSG ID.
 *
 * This function translates given context into corresponding
 * #nvgpu_channel and TSG identifier.
 *
 * This function will make use of a small internal TLB to optimize the
 * search. If TLB is not hit, it will loop through all the channels
 * and find channel with given context value.
 *
 * @return Pointer to GPU channel corresponding to given context.
 */
struct nvgpu_channel *nvgpu_gr_intr_get_channel_from_ctx(struct gk20a *g,
		u32 curr_ctx, u32 *curr_tsgid);

/**
 * @brief Set error notifier for GR errors/exceptions.
 *
 * @param g [in]		Pointer to GPU driver struct.
 * @param isr_data [in]		Pointer to GR ISR data.
 * @param error_notifier [in]	Error notifier value to be set.
 *
 * This function will set #error_notifier error code into TSG's error
 * notifier (if configured by user application).
 *
 * This error notifier is used to convey GR errors/exceptions to user
 * space applications.
 */
void nvgpu_gr_intr_set_error_notifier(struct gk20a *g,
		struct nvgpu_gr_isr_data *isr_data, u32 error_notifier);

/**
 * @brief Handle all SM exception interrupts.
 *
 * @param g [in]		Pointer to GPU driver struct.
 * @param gpc [in]		Index of GPC on which exception is received.
 * @param tpc [in]		Index of TPC on which exception is received.
 * @param sm [in]		Index of SM on which exception is received.
 * @param post_event [out]	Flag to post event, ignored for safety.
 * @param fault_ch [in]		Pointer to faulted GPU channel.
 * @param hww_global_esr [out]	Global Error Status Register value.
 *
 * This function handles all exception interrupts coming from
 * SM (Streaming Multiprocessor). This function will read global and
 * warp error status registers and pass on this information to qnx.sdl
 * unit while reporting SM exceptions.
 *
 * This function will also capture all SM exception meta data so that
 * user application can query this data when required.
 *
 * @return 0 in case interrupt was handled cleanly, < 0 in case driver
 *         should be moved to quiesce state.
 */
int nvgpu_gr_intr_handle_sm_exception(struct gk20a *g, u32 gpc, u32 tpc, u32 sm,
		bool *post_event, struct nvgpu_channel *fault_ch,
		u32 *hww_global_esr);

/**
 * @brief ISR to handle any pending GR engine stalling interrupts reported
 *        by HW and to report them to qnx.sdl unit.
 *
 * @param g [in]		Pointer to GPU driver struct.
 *
 * This is the entry point to handle all GR engine stalling interrupts.
 * This includes:
 * - Check for any pending exceptions/errors interrupts.
 * - Disable fifo access.
 * - Read information for trapped methods
 *   If any trapped context, find the corresponding channel
 *   and tsg for error reporting.
 * - Handle any pending interrupts.
 * - Handle any pending illegal interrupts.
 * - Handle any pending error interrupts
 * - Handle any pending GPC exception interrupts.
 * - Clearing all pending interrupts once they are handled.
 * - Enable back fifo access.
 *
 * This function will take care of clearing all pending interrupts
 * once they are handled.
 *
 * Finally, this function will also take care of moving the driver to
 * quiesce state in case of uncorrected errors.
 *
 * @return 0 in case of success, < 0 in case of failure.
 *
 * @see nvgpu_gr_intr_handle_fecs_error
 * @see nvgpu_gr_intr_handle_gpc_exception
 * @see nvgpu_gr_intr_handle_notify_pending
 * @see nvgpu_gr_intr_handle_semaphore_pending
 * @see nvgpu_gr_intr_handle_sm_exception
 * @see nvgpu_gr_intr_report_exception
 * @see nvgpu_gr_intr_set_error_notifier
 */
int nvgpu_gr_intr_stall_isr(struct gk20a *g);

/**
 * @brief Flush channel lookup TLB.
 *
 * @param g [in]		Pointer to GPU driver struct.
 *
 * common.gr.intr unit maintains a TLB to translate context into GPU
 * channel ID. See #nvgpu_gr_intr_get_channel_from_ctx() for reference.
 *
 * This function will flush the TLB. Flushing of TLB is typically
 * needed while closing a channel.
 */
void nvgpu_gr_intr_flush_channel_tlb(struct gk20a *g);

/**
 * @brief Initialize GR interrupt data structure.
 *
 * @param g [in]		Pointer to GPU driver struct.
 *
 * This function allocates memory for #nvgpu_gr_intr structure, and
 * initializes various fields in the structure.
 *
 * @return pointer to #nvgpu_gr_intr struct in case of success,
 *         NULL in case of failure.
 */
struct nvgpu_gr_intr *nvgpu_gr_intr_init_support(struct gk20a *g);

/**
 * @brief Free GR interrupt data structure.
 *
 * @param g [in]		Pointer to GPU driver struct.
 * @param intr [in]		Pointer to GR interrupt data struct.
 *
 * This function will free memory allocated for #nvgpu_gr_intr structure.
 */
void nvgpu_gr_intr_remove_support(struct gk20a *g, struct nvgpu_gr_intr *intr);

#endif /* NVGPU_GR_INTR_H */
