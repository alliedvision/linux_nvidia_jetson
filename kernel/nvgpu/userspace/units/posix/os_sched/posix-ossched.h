/*
 * Copyright (c) 2019-2020, NVIDIA CORPORATION.  All rights reserved.
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

/**
 * @addtogroup SWUTS-posix-ossched
 * @{
 *
 * Software Unit Test Specification for posix-ossched
 */

#ifndef __UNIT_POSIX_OSSCHED_H__
#define __UNIT_POSIX_OSSCHED_H__

#include <nvgpu/os_sched.h>

/**
 * Test specification for test_current_pid
 *
 * Description: Test the PID of the current process.
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_current_pid
 *
 * Inputs:
 *
 * Steps:
 * 1) Get the PID of the current process using standard lib call.
 * 2) Get the PID of the current process using NVGPU API.
 * 3) Compare the PIDs obtained in step 1 and 2.
 * 4) Return Fail if the PIDs dont match.
 *
 * Output:
 * Return PASS if the PIDs fetched using standard library call and NVGPU API
 * matches; otherwise, the test returns FAIL.
 */
int test_current_pid(struct unit_module *m,
			struct gk20a *g, void *args);

/**
 * Test specification for test_current_tid
 *
 * Description: Test the TID of the current thread.
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_current_tid
 *
 * Inputs:
 *
 * Steps:
 * 1) Get the TID of the current thread using standard lib call.
 * 2) Get the TID of the current thread using NVGPU API.
 * 3) Compare the TIDs obtained in step 1 and 2.
 * 4) Return Fail if the TIDs dont match.
 *
 * Output:
 * Return PASS if the TIDs fetched using standard library call and NVGPU API
 * matches; otherwise, the test returns FAIL.
 */
int test_current_tid(struct unit_module *m,
			struct gk20a *g, void *args);

/**
 * Test specification for test_print_current
 *
 * Description: Print the current thread name.
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_print_current, nvgpu_print_current_impl
 *
 * Inputs:
 *
 * Steps:
 * 1) Print the current thread name with log type NVGPU_INFO
 * 2) Print the current thread name with log type NVGPU_DEBUG
 * 3) Print the current thread name with log type NVGPU_WARNING
 * 4) Print the current thread name with log type NVGPU_ERROR
 * 5) Print the current thread name with invalid log type which should result
 * in the function using default log type.
 *
 * Output:
 * The test returns PASS if all the print calls gets executed without any
 * crash/hang.  Since the function does not return any value, the only case
 * in which the test can fail is due to an internal hang or crash.
 */
int test_print_current(struct unit_module *m,
			struct gk20a *g, void *args);
#endif /* __UNIT_POSIX_OSSCHED_H__ */
