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

#ifndef NVGPU_POSIX_TIMERS_H
#define NVGPU_POSIX_TIMERS_H

#include <nvgpu/types.h>
#include <nvgpu/log.h>


/**
 * @brief Private handler of CPU timeout, should not be used directly.
 *
 * Posix implementation of the CPU timeout handler. Checks if the timeout
 * duration has expired or not.
 *
 * @param timeout [in]	Timeout object.
 * @param caller [in]	Instruction pointer of the caller.
 * @param fmt [in]	Format of the variable length argument.
 * @param arg... [in]	Variable length arguments.
 *
 * @return Shall return 0 if the timeout has not expired; otherwise, an error
 * number indicating a timeout is returned.
 *
 * @retval 0 if timeout has not expired.
 * @retval -ETIMEDOUT for timeout.
 */
#define nvgpu_timeout_expired_msg_cpu(timeout, caller, fmt, arg...)	\
({									\
	const struct nvgpu_timeout *t_ptr = (timeout);			\
	int ret_cpu = 0;						\
	if (nvgpu_current_time_ns() > t_ptr->time_duration) {			\
		if ((t_ptr->flags & NVGPU_TIMER_SILENT_TIMEOUT) == 0U) { \
			nvgpu_err(t_ptr->g, "Timeout detected @ %p" fmt, \
							caller, ##arg);	\
		}							\
		ret_cpu = -ETIMEDOUT;					\
	}								\
	(int)ret_cpu;							\
})

/**
 * @brief Private handler of retry timeout, should not be used directly.
 *
 * Posix implementation of the retry timeout handler. Checks if the retry limit
 * has reached, return an error value to indicate a timeout if the retry limit
 * has reached else increment the retry count and return.
 *
 * @param timeout [in]	Timeout object.
 * @param caller [in]	Instruction pointer of the caller.
 * @param fmt [in]	Format of the variable length argument.
 * @param arg... [in]	Variable length arguments.
 *
 * @return Shall return 0 if the timeout has not expired; otherwise, an error
 * number indicating a timeout is returned.
 *
 * @retval 0 if timeout has not expired.
 * @retval -ETIMEDOUT for timeout.
 */
#define nvgpu_timeout_expired_msg_retry(timeout, caller, fmt, arg...)	\
({									\
	struct nvgpu_timeout *t_ptr = (timeout);			\
	int ret_retry = 0;						\
	if (t_ptr->retries.attempted >= t_ptr->retries.max_attempts) {	\
		if ((t_ptr->flags & NVGPU_TIMER_SILENT_TIMEOUT) == 0U) { \
			nvgpu_err(t_ptr->g, "No more retries @ %p" fmt,	\
							caller, ##arg);	\
		}							\
		ret_retry = -ETIMEDOUT;					\
	} else {							\
		t_ptr->retries.attempted++;				\
	}								\
	(int)ret_retry;							\
})

/**
 * @brief Private handler of userspace timeout, should not be used directly.
 *
 * Posix implementation of the timeout handler. Differentiates between a CPU
 * timer and a retry timer and handles accordingly. Macro does not perform
 * any validation of the parameters.
 *
 * @param timeout [in]	Timeout object.
 * @param caller [in]	Instruction pointer of the caller.
 * @param fmt [in]	Format of the variable length argument.
 * @param arg... [in]	Variable length arguments.
 *
 * @return Shall return 0 if the timeout has not expired; otherwise, an error
 * number indicating a timeout is returned.
 *
 * @retval 0 if timeout has not expired.
 * @retval -ETIMEDOUT for timeout.
 */
#define nvgpu_timeout_expired_msg_impl(timeout, caller, fmt, arg...)	\
({									\
	int ret_timeout = is_fault_injection_set;			\
	if (ret_timeout == -1) {					\
	if (((timeout)->flags & NVGPU_TIMER_RETRY_TIMER) != 0U) {	\
		ret_timeout = nvgpu_timeout_expired_msg_retry((timeout),\
						caller, fmt, ##arg);	\
	} else {							\
		ret_timeout = nvgpu_timeout_expired_msg_cpu((timeout),	\
						caller,	fmt, ##arg);	\
	}								\
	}								\
	(int)ret_timeout;						\
})

#ifdef NVGPU_UNITTEST_FAULT_INJECTION_ENABLEMENT
struct nvgpu_posix_fault_inj *nvgpu_timers_get_fault_injection(void);
int nvgpu_timeout_expired_fault_injection(void);
#define is_fault_injection_set nvgpu_timeout_expired_fault_injection()
#else
#define is_fault_injection_set -1
#endif /* NVGPU_UNITTEST_FAULT_INJECTION_ENABLEMENT */

#endif
