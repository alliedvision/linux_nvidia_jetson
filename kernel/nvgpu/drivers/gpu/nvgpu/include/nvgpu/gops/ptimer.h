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
#ifndef NVGPU_GOPS_PTIMER_H
#define NVGPU_GOPS_PTIMER_H

#include <nvgpu/types.h>

/**
 * @file
 *
 * ptimer unit HAL interface
 *
 */
struct gk20a;
struct nvgpu_cpu_time_correlation_sample;

/**
 * ptimer unit HAL operations
 *
 * @see gpu_ops
 */
struct gops_ptimer {
	/**
	 * @brief Handles specific types of PRI errors
	 *
	 * @param g [in]		GPU driver struct pointer
	 *                               - No validation is performed on this
	 *                                 parameter.
	 *
	 * - ISR is called when one of the below PRI error occurs:
	 *      - PRI_SQUASH: error due to pri access while target block is
	 *		      in reset
	 *      - PRI_FECSERR: FECS detected an error while processing a PRI
	 * 		      request
	 *      - PRI_TIMEOUT: non-existent host register / timeout waiting for
	 *                    FECS
	 * - Below registers contain information about the first PRI error since
	 *   the previous error was cleared:
	 *    - timer_pri_timeout_save_0_r()
	 *    - timer_pri_timeout_save_1_r()
	 *    - timer_pri_timeout_fecs_errcode_r()
	 *
	 * Algorithm:
	 * - timer_pri_timeout_save_0_r() register contains the dword address
	 *   of the failed PRI access. Read value of register
	 *   timer_pri_timeout_save_0_r() in \a save0.
	 * - Extract the address of the PRI access that resulted in
	 *   error from \a save0 using timer_pri_timeout_save_0_addr_v(save0).
	 *   This address field has 4-byte granularity, so multiply by 4 to
	 *   obtain the byte address and store it in \a error_addr.
	 * - timer_pri_timeout_save_1_r() register contains the PRI write data for
	 *   the failed request. Note data is set to 0 when the failed request
	 *   was a read. Read value of register timer_pri_timeout_save_1_r()
	 *   in \a save1.
	 * - FECS_TGT field in timer_pri_timeout_save_0_r() register indicates
	 *   if fecs was the target of the PRI access. Extract bit FECS_TGT in
	 *   \a save0 using timer_pri_timeout_save_0_fecs_tgt_v(save0). If
	 *   FECS_TGT is not 0(FALSE), only register timer_pri_timeout_fecs_errcode_r()
	 *   has reliable value. Read value of register
	 *   timer_pri_timeout_fecs_errcode_r() in \a fecs_errcode.
	 * - If \a fecs_errcode is not 0,
	 *    - Call \ref gops_priv_ring.decode_error_code() HAL to decode error
	 *      code. The inputs \ref gops_priv_ring.decode_error_code()
	 *      to are \a g and \a fecs_errcode.
	 *    - Print error message with FECS error code \a fecs_errcode.
	 *    - Set \a error_addr to 0, since it is not relevant in case of fecs error.
	 *    - Also set \a inst to 1 as the target of PRI access was FECS.
	 * - Print "PRI timeout" error message along with address (\a error_addr),
	 *   data (\a save1) and if the PRI access was a READ or WRITE operation.
	 *   Find out if the PRI access was a write or a read by extracting
	 *   WRITE field from \a save0 using timer_pri_timeout_save_0_write_v(save0).
	 * - Clear timer_pri_timeout_save_0_r() and timer_pri_timeout_save_1_r()
	 *   registers so that the next pri access error can be recorded. Write
	 *   0 to these two registers to clear the previous error information.
	 * - Report the PRI_TIMEOUT_ERROR to SDL unit using \ref nvgpu_report_err_to_sdl()
	 *   API. The inputs to \ref nvgpu_report_err_to_sdl() are -
	 *    - g,
	 *    - GPU_PRI_TIMEOUT_ERROR.
	 */
	void (*isr)(struct gk20a *g);

	/** @cond DOXYGEN_SHOULD_SKIP_THIS */

	/**
	 * NON-FUSA HAL
	 */
#ifdef CONFIG_NVGPU_IOCTL_NON_FUSA
	int (*read_ptimer)(struct gk20a *g, u64 *value);

	int (*get_timestamps_zipper)(struct gk20a *g,
			u32 source_id, u32 count,
			struct nvgpu_cpu_time_correlation_sample *samples);
#endif
#ifdef CONFIG_NVGPU_DEBUGGER
	int (*config_gr_tick_freq)(struct gk20a *g);
#endif
#ifdef CONFIG_NVGPU_PROFILER
	void (*get_timer_reg_offsets)(u32 *timer0_offset, u32 *timer1_offset);
#endif

	/** @endcond DOXYGEN_SHOULD_SKIP_THIS */
};

#endif /* NVGPU_GOPS_PTIMER_H */
