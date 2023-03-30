/*
 * Copyright (c) 2016-2022, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef NVGPU_TIMERS_H
#define NVGPU_TIMERS_H

#include <nvgpu/types.h>
#include <nvgpu/bitops.h>
#include <nvgpu/utils.h>

#ifndef __KERNEL__
#include <nvgpu/posix/timers.h>
#endif

struct gk20a;

/**
 * struct nvgpu_timeout - define a timeout.
 *
 * There are two types of timer supported:
 *
 *   o  NVGPU_TIMER_CPU_TIMER
 *        Timer uses the CPU to measure the timeout.
 *
 *   o  NVGPU_TIMER_RETRY_TIMER
 *        Instead of measuring a time limit keep track of the number of times
 *        something has been attempted. After said limit, "expire" the timer.
 *
 * Available flags:
 *
 *   o  NVGPU_TIMER_NO_PRE_SI
 *        By default when the system is not running on silicon the timeout
 *        code will ignore the requested timeout. Specifying this flag will
 *        override that behavior and honor the timeout regardless of platform.
 *
 *   o  NVGPU_TIMER_SILENT_TIMEOUT
 *        Do not print any messages on timeout. Normally a simple message is
 *        printed that specifies where the timeout occurred.
 */
struct nvgpu_timeout {
	/**
	 * GPU driver structure.
	 */
	struct gk20a		*g;
	/**
	 * Flags for timer.
	 */
	unsigned int		 flags;
	/**
	 * Timeout duration/count.
	 */
	union {
		s64		 time_duration;
		struct {
			u32	 max_attempts;
			u32	 attempted;
		} retries;
	};
};

/**
 * Value for Bit 0 indicating a CPU timer.
 */
#define NVGPU_TIMER_CPU_TIMER		(0x0U)

/**
 * Value for Bit 0 indicating a retry timer.
 */
#define NVGPU_TIMER_RETRY_TIMER		(0x1U)

/**
 * @defgroup NVRM_GPU_NVGPU_DRIVER_TIMER_FLAGS Timer flags
 *
 * Bits 1 through 7 are reserved; bits 8 and up are flags:
 */

/**
 * @ingroup NVRM_GPU_NVGPU_DRIVER_TIMER_FLAGS
 *
 * Flag to enforce timeout check for pre silicon platforms.
 */
#define NVGPU_TIMER_NO_PRE_SI		BIT32(8)

/**
 * @ingroup NVRM_GPU_NVGPU_DRIVER_TIMER_FLAGS
 *
 * Flag to enforce silent timeout.
 */
#define NVGPU_TIMER_SILENT_TIMEOUT	BIT32(9)

/**
 * Mask value for timer flag bits.
 */
#define NVGPU_TIMER_FLAG_MASK		(NVGPU_TIMER_RETRY_TIMER |	\
					 NVGPU_TIMER_NO_PRE_SI |	\
					 NVGPU_TIMER_SILENT_TIMEOUT)

/**
 * @brief Initialize a timeout.
 *
 * Initialize a timeout object referenced by \a timeout. The \a flags parameter
 * should not have an unsupported bit set, an error value is returned in that
 * case. Refer to struct #nvgpu_timeout for supported flag values. Initialize
 * the memory of struct #nvgpu_timeout using #memset. Populate the variables
 * \a g and \a flags in #nvgpu_timeout with the input parameters \a g and
 * \a flags. Populate the \a max_attempts in #nvgpu_timeout with the input
 * parameter \a duration if the timer is a retry based timer, else, if the
 * timer is a duration based timer, calculate the duration value that has to be
 * populated in \a time_duration variable in #nvgpu_timeout. The duration value
 * is calculated from the input parameter \a duration. The value of the input
 * parameter \a duration is converted in to nanoseconds by invoking the safe
 * multiple function #nvgpu_safe_mult_s64(), the resulting nanosecond value
 * has to be added with the current nanosecond time value which is fetched
 * using the function #nvgpu_current_time_ns(). Uses the safe addition function
 * #nvgpu_safe_add_s64() to add the current nanosecond time value with the
 * duration nanosecond value. The result is populated in the #time_duration
 * variable in #nvgpu_timeout. Function does the validation of the input
 * parameter \a flags to check if it has any bit positions other than the
 * allowed ones set.
 *
 * @param g [in]	GPU driver structure.
 * @param timeout [in]	Timeout object to initialize.
 * @param duration [in]	Timeout duration/count.
 * @param flags [in]	Flags for timeout object. Valid bits are,
 * 			  - NVGPU_TIMER_RETRY_TIMER
 * 			  - NVGPU_TIMER_NO_PRE_SI
 * 			  - NVGPU_TIMER_SILENT_TIMEOUT
 *
 * @return Shall return 0 on success; otherwise, return error number to indicate
 * the error type.
 *
 * @retval -EINVAL invalid input parameter.
 */
int nvgpu_timeout_init_flags(struct gk20a *g, struct nvgpu_timeout *timeout,
		       u32 duration, unsigned long flags);


/**
 * @brief Initialize a timeout.
 *
 * Init a cpu clock based timeout. See nvgpu_timeout_init_flags() and
 * NVGPU_TIMER_CPU_TIMER for full explanation.
 *
 * @param g [in]	GPU driver structure.
 * @param timeout [in]	Timeout object to initialize.
 * @param duration [in]	Timeout duration in milliseconds.
 */
void nvgpu_timeout_init_cpu_timer(struct gk20a *g, struct nvgpu_timeout *timeout,
		       u32 duration_ms);

/**
 * @brief Initialize a pure software timeout.
 *
 * Init a cpu clock based timeout with no pre-si override. See
 * nvgpu_timeout_init_flags() and NVGPU_TIMER_CPU_TIMER for full explanation.
 *
 * This builds a timer that has the NVGPU_TIMER_NO_PRE_SI flag. Most often,
 * hardware polling related loops are preferred to be infinite in presilicon
 * simulation mode. That's not the case in some timers for only software logic,
 * which this function is for.
 *
 * @param g [in]	GPU driver structure.
 * @param timeout [in]	Timeout object to initialize.
 * @param duration [in]	Timeout duration in milliseconds.
 */
void nvgpu_timeout_init_cpu_timer_sw(struct gk20a *g, struct nvgpu_timeout *timeout,
		       u32 duration_ms);

/**
 * @brief Initialize a timeout.
 *
 * Init a retry based timeout. See nvgpu_timeout_init_flags() and
 * NVGPU_TIMER_RETRY_TIMER for full explanation.
 *
 * @param g [in]	GPU driver structure.
 * @param timeout [in]	Timeout object to initialize.
 * @param duration [in]	Timeout duration in number of retries.
 */
void nvgpu_timeout_init_retry(struct gk20a *g, struct nvgpu_timeout *timeout,
		       u32 duration_count);

/**
 * @brief Check the timeout status.
 *
 * Checks the status of \a timeout and returns if the timeout has expired or
 * not. Uses the function #time_after() with current time value in nanoseconds
 * and \a time_duration in #nvgpu_timeout as parameters to check the timeout
 * status of a duration timer. Function does not perform any validation of the
 * parameters.
 *
 * @param timeout [in]	Timeout object to check the status.
 *
 * @return Boolean value to indicate the status of timeout.
 *
 * @retval TRUE if the timeout has expired.
 * @retval FALSE if the timeout has not expired.
 */
bool nvgpu_timeout_peek_expired(struct nvgpu_timeout *timeout);

/**
 * @brief Checks the timeout expiry according to the timer type.
 *
 * This macro checks to see if a timeout has expired. For retry based timers,
 * a call of this macro increases the retry count by one and checks if the
 * retry count has reached maximum allowed retry limit. For CPU based timers,
 * a call of this macro checks whether the required duration has elapsed.
 * Refer to the documentation of the function #nvgpu_timeout_expired_msg_impl
 * for underlying implementation. Macro does not perform any validation of the
 * parameter.
 *
 * @param __timeout [in]	Timeout object to handle.
 */
#define nvgpu_timeout_expired(__timeout)				\
	nvgpu_timeout_expired_msg_impl(__timeout, NVGPU_GET_IP, "")

/**
 * @brief Checks the timeout expiry and uses the input params for debug message.
 *
 * Along with handling the timeout, this macro also takes in a variable list
 * of arguments which is used in constructing the debug message for a timeout.
 * Refer to #nvgpu_timeout_expired_msg_impl for further details. Macro does
 * not perform any validation of the input parameters.
 *
 * @param __timeout [in]	Timeout object to handle.
 * @param fmt [in]		Format of the variable arguments.
 * @param args... [in]		Variable arguments.
 */
#define nvgpu_timeout_expired_msg(__timeout, fmt, args...)		\
	nvgpu_timeout_expired_msg_impl(__timeout, NVGPU_GET_IP,	fmt, ##args)

/*
 * Waits and delays.
 */

/**
 * @brief Sleep for millisecond intervals.
 *
 * Function sleeps for requested millisecond duration. The current time value
 * is fetched using the function \a clock_gettime and converted into nanosecond
 * equivalent using safe operations. The input parameter \a msecs is converted
 * into equivalent nanosecond value using the function #nvgpu_safe_mult_s64().
 * Add the current time value with the duration value in nanosecond using the
 * safe add function #nvgpu_safe_add_s64(). The resulting nanosecond value is
 * populated into a \a timespec structure, which is then used as a parameter
 * in the invocation of the function \a clock_nanosleep() to sleep for the
 * requested duration. The clock id paramater is passed as CLOCK_MONOTONIC and
 * TIMER_ABSTIME is used as the flags parameter for the invocation of the
 * function \a clock_nanosleep. A NULL pointer is passed as parameter for the
 * remaining time interval  parameter. Function does not perform any validation
 * of the parameter.
 *
 * @param msecs [in]	Milliseconds to sleep.
 */
void nvgpu_msleep(unsigned int msecs);

/**
 * @brief Sleep for a duration in the range of input parameters.
 *
 * Sleeps for a value in the range between min_us and max_us. The underlying
 * implementation is OS dependent. In nvgpu Posix implementation, the sleep is
 * always for min_us duration and the function sleeps only if the input min_us
 * value is greater than or equal to 1000 microseconds, else the function just
 * delays execution for requested duration of time without sleeping. Invokes
 * the function #nvgpu_udelay() internally with \a min_us as parameter.
 * Function does not perform any validation of the parameters.
 *
 * @param min_us [in]	Minimum duration to sleep in microseconds.
 * @param max_us [in]	Maximum duration to sleep in microseconds.
 */
void nvgpu_usleep_range(unsigned int min_us, unsigned int max_us);

/**
 * @brief Delay execution.
 *
 * Delays the execution for requested duration of time in microseconds. In
 * Posix implementation, if the requested duration is greater than or equal to
 * 1000 microseconds, the function sleeps by invoking the function
 * #nvgpu_usleep() with \a usecs as parameter, else invokes the function
 * #nvgpu_delay_usecs() with \a usecs as parameter to delay the execution.
 * Function does not perform any validation of the parameter.
 *
 * @param usecs [in]	Delay duration in microseconds.
 */
void nvgpu_udelay(unsigned int usecs);

/*
 * Timekeeping.
 */

/**
 * @brief Current time in milliseconds.
 *
 * Invokes the function \a clock_gettime internally to fetch the current time
 * value and returns the value in milliseconds.
 *
 * @return Current time value as milliseconds.
 */
s64 nvgpu_current_time_ms(void);

/**
 * @brief Current time in nanoseconds.
 *
 * Invokes the function \a clock_gettime internally to fetch the current time.
 * The value is converted into nanosecond equivalent using safe operations.
 *
 * @return Current time value as nanoseconds.
 */
s64 nvgpu_current_time_ns(void);

/**
 * @brief Current time in microseconds.
 *
 * Invokes the library function \a gettimeofday internally to get the current
 * time value. The received current time value is converted into microsecond
 * equivalent using safe multiplication function #nvgpu_safe_mult_s64() and
 * safe addition #nvgpu_safe_add_s64().
 *
 * @return Current time value as microseconds.
 */
s64 nvgpu_current_time_us(void);
#ifdef CONFIG_NVGPU_NON_FUSA
u64 nvgpu_us_counter(void);

/**
 * @brief Get GPU cycles.
 *
 * @param None.
 *
 * @return 64 bit number which has GPU cycles.
 */
u64 nvgpu_get_cycles(void);

/**
 * @brief Current high resolution time stamp in microseconds.
 *
 * @return Returns the high resolution time stamp value converted into
 * microseconds.
 */
u64 nvgpu_hr_timestamp_us(void);

/**
 * @brief Current high resolution time stamp.
 *
 * @return High resolution time stamp value.
 */
u64 nvgpu_hr_timestamp(void);
#endif

/**
 * @brief OS specific implementation to provide precise microsecond delay
 *
 * Wait using nanospin_ns until usecs expires. Log error if API returns non
 * zero value once wait time expires.
 *
 * @param usecs [in]		Delay in microseconds.
 *				Range: 0 - 500ms.
 *
 * @return None.
 */
void nvgpu_delay_usecs(unsigned int usecs);

#ifdef __KERNEL__
/**
 * @brief Private handler of kernel timeout, should not be used directly.
 *
 * @param timeout [in]	Timeout object.
 * @param caller [in]	Instruction pointer of the caller.
 * @param fmt [in]	Format of the variable length argument.
 * @param ... [in]	Variable length arguments.
 *
 * Implements the timeout handler functionality for kernel. Differentiates
 * between a CPU timer and a retry timer and handles accordingly.
 *
 * @return Shall return 0 if the timeout has not expired; otherwise, an error
 * number indicating a timeout is returned.
 */
int nvgpu_timeout_expired_msg_impl(struct nvgpu_timeout *timeout,
			      void *caller, const char *fmt, ...);
#endif

#endif /* NVGPU_TIMERS_H */
