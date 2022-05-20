/*
 * Copyright (c) 2017-2021, NVIDIA CORPORATION.  All rights reserved.
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
#ifndef NVGPU_PTIMER_H
#define NVGPU_PTIMER_H

/**
 * @file
 *
 * Declares structs, defines and APIs exposed by ptimer unit.
 */
#include <nvgpu/types.h>
#include <nvgpu/static_analysis.h>

struct gk20a;

#ifdef CONFIG_NVGPU_IOCTL_NON_FUSA
struct nvgpu_cpu_time_correlation_sample {
	u64 cpu_timestamp;
	u64 gpu_timestamp;
};
#endif

/**
 * @defgroup NVGPU_PTIMER_DEFINES
 *
 */

/**
 * @ingroup NVGPU_PTIMER_DEFINES
 *
 * PTIMER_REF_FREQ_HZ corresponds to a period of 32 nanoseconds.
 * 32 ns is the resolution of ptimer.
 */
#define PTIMER_REF_FREQ_HZ                      31250000U

/**
 * @brief Scales the \a timeout value as per the ptimer source frequency.
 *        Units like common.fifo use this API to scale the timeouts
 *        and scheduling timeslices enforced by them.
 *
 * @param g [in]		The GPU driver struct pointer
 *                               - No validation is performed on \a g parameter
 * @param timeout [in]		Timeout value assuming ptimer resolution of 32ns
 *                               - Validation is performed to ensure \timeout is
 * 				   within below range:
 *				   MIN: 0
 *				   MAX: U32_MAX / 10
 * @param scaled_timeout [out]	Pointer to store calculated scaled timeout value
 *                               - Validation is performed to ensure the pointer
 *				   is not NULL.
 *
 * 1. The GPU ptimer has a resolution of 32 ns and so expects a source frequency
 *    of 31.25 MHz (\ref #PTIMER_REF_FREQ_HZ):
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * 1 / 32ns = 31.25 MHz
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * 2. The ptimer source frequency may differ from the expected value of
 *    \ref #PTIMER_REF_FREQ_HZ. For example- on Maxwell, the ptimer source
 *    frequency is 19.2 MHz.
 *
 * 3. The \a timeout value assumes ptimer source frequency equal to
 *    \ref #PTIMER_REF_FREQ_HZ. If the ptimer source frequency is different than
 *    \ref #PTIMER_REF_FREQ_HZ, then the timeout value based on
 *    \ref #PTIMER_REF_FREQ_HZ needs to be scaled to the new frequency.
 *
 * 4. The scaling factor \a scale10x is calculated by internal API
 *    ptimer_scalingfactor10x(). This API takes ptimer_src_freq as input and
 *    calculates \a scale10x as output using below equation:
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * scale10x = (PTIMER_REF_FREQ_HZ * 10)/ ptimer_src_freq
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *    Note, the scale10x is calculated with a multiplication of 10 to retain
 *    1-digit decimal precision.
 *    For example,
 *    - On Maxwell, the ptimer source frequency is 19.2 MHz.
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *  scale10x = (31250000  * 10)/ 19200000 = 16
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *    - On Volta, ptimer_source frequency = 31250000 Hz = \ref #PTIMER_REF_FREQ_HZ
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *  scale10x = 10
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * 5. The scale10x is used to calibrate the \a timeout value to the new source
 *    frequency.
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *  *scaled_timeout = (timeout * 10)/ scale10x.
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *    As there is an additonal factor of 10 in \a scale10x (divisor),
 *    the above equation includes a multiplication by 10. This multiplication
 *    result can overflow, so maximum acceptable value of \a timeout can be
 *    (U32_MAX / 10).
 *
 * Algorithm:
 * - Perform validation of input parameters based on the criteria specified in
 *   the parameter list. Return -EINVAL in case of failure.
 * - Check if \ref gk20a.ptimer_src_freq "g->ptimer_src_freq" is non-zero to avoid division
 *   by zero in ptimer_scalingfactor10x() API. If \ref gk20a.ptimer_src_freq
 *   "g->ptimer_src_freq" is 0, then indicate failure by returning -EINVAL.
 * - Get the scale factor \a scale10x by calling ptimer_scalingfactor10x()
 *   internal API. The input to ptimer_scalingfactor10x() is \ref gk20a.ptimer_src_freq
 *   "g->ptimer_src_freq".
 * - Check if \a scale10x is non-zero to avoid division by zero during
 *   \a scaled_timeout calculation. If \a scale10x is 0, then indicate failure
 *   by returning -EINVAL.
 * - To compensate for loss in integer division, add 1 to the result if
 *   the modulo division output is more than half of the \a scale10x (divisor).
 *   The code for calculating \a *scaled_timeout should be -
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * if (timeout * 10) % scale10x) >= (scale10x / 2) {
 * 	 *scaled_timeout = ((timeout * 10)/ scale10x) + 1;
 * } else {
 *	 *scaled_timeout = (timeout * 10)/ scale10x;
 * }
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * - Return 0 to indicate success
 *
 * @retval -EINVAL in case invalid \a timeout value is passed.
 * @retval -EINVAL in case NULL pointer is passed for \a scaled_timeout.
 * @retval -EINVAL in case \ref gk20a.ptimer_src_freq "g->ptimer_src_freq" is 0.
 * @retval -EINVAL in case calculated scaling factor \a scale10x is 0.
 * @retval 0 in case of success.
 */
int nvgpu_ptimer_scale(struct gk20a *g, u32 timeout, u32 *scaled_timeout);

#ifdef CONFIG_NVGPU_IOCTL_NON_FUSA
int nvgpu_get_timestamps_zipper(struct gk20a *g,
		u32 source_id, u32 count,
		struct nvgpu_cpu_time_correlation_sample *samples);
int nvgpu_ptimer_init(struct gk20a *g);
#endif /* CONFIG_NVGPU_IOCTL_NON_FUSA */
#endif
