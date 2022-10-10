/*
 * Copyright (c) 2022, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef NVGPU_PERIODIC_TIMER_H
#define NVGPU_PERIODIC_TIMER_H

#ifdef __KERNEL__
#include <nvgpu/linux/periodic_timer.h>
#else
#include <nvgpu/posix/periodic_timer.h>
#endif

#include <nvgpu/types.h>

/**
 * @brief Initialize a nvgpu_periodic_timer
 *
 * @param timer The timer to be initialized
 * @param fn Timer callback function, must not be NULL
 * @param arg The argument of the timer callback function
 * @return 0 Success
 * @return -EAGAIN Temporary error during kernel allocation of timer structure
 * @return -EINVAL OS specific implementation error
 * @return -ENOMEM out of memory
 */
int nvgpu_periodic_timer_init(struct nvgpu_periodic_timer *timer,
			void (*fn)(void *arg), void *arg);
/**
 * @brief start a nvgpu_periodic_timer. Not thread safe.
 *
 * @param timer timer to start, must not be NULL
 * @param interval_ns periodic timer interval in the unit of ns
 * @return 0 Success
 * @return -EINVAL invalid timer or interval_ns
 */
int nvgpu_periodic_timer_start(struct nvgpu_periodic_timer *timer,
				u64 interval_ns);
/**
 * @brief stop a timer and wait for any timer callback to be finished.
 * Not thread safe.
 *
 * @param timer timer to stop
 * @return 0 Success
 * @return -EINVAL invalid timer
 */
int nvgpu_periodic_timer_stop(struct nvgpu_periodic_timer *timer);
/**
 * @brief destroy a nvgpu_periodic_timer
 *
 * @param timer timer to destroy
 * @return 0 Success
 * @return -EINVAL invalid timer
 */
int nvgpu_periodic_timer_destroy(struct nvgpu_periodic_timer *timer);

#endif
