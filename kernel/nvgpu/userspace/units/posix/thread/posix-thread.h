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

/**
 * @addtogroup SWUTS-posix-thread
 * @{
 *
 * Software Unit Test Specification for posix-thread
 */
#ifndef __UNIT_POSIX_THREAD_H__
#define __UNIT_POSIX_THREAD_H__

#include <nvgpu/thread.h>

/**
 * Test specification for test_thread_cycle
 *
 * Description: Test the various functionalities provided by Threads unit.
 * Main functionalities that has to be tested in Threads unit are as follows,
 * 1) Thread creation
 * 2) Thread creation with a priority value
 * 3) Thread stop
 * 4) Stop thread gracefully
 * test_thread_cycle function tests all the above mentioned functionalities
 * based on the input arguments.
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_thread_create, nvgpu_thread_create_priority,
 *          nvgpu_thread_is_running, nvgpu_thread_stop,
 *          nvgpu_thread_stop_graceful, nvgpu_thread_should_stop,
 *          nvgpu_thread_join
 *
 * Inputs:
 * 1) Pointer to test_thread_args as function parameter
 * 2) Global instance of struct nvgpu_thread
 * 3) Global instance of struct unit_test_thread_data
 *
 * Steps:
 * Thread creation
 * 1) Reset all global and shared variables to 0.
 * 2) Create thread using nvgpu_thread_create.
 * 3) Check the return value from nvgpu_thread_create for error.
 * 4) Wait for the thread to be created by polling for a shared variable.
 * 5) Return Success once the thread function is called and the shared
 *    variable is set which indicates a successful thread creation.
 * 6) Above steps are done for thread with a name, thread without a name
 *    and thread function which returns an error value.
 * 7) For code coverage, based on a passed argument the created thread tries
 *    to join with itself expecting a BUG callback. This should trigger a BUG
 *    as expected by the calling thread. This test is run only if QNX is not
 *    defined, as there is a difference in the return values.
 *
 * Thread creation with a priority value
 * 1) Reset all global and shared variables to 0.
 * 2) Create thread using nvgpu_thread_create_priority
 * 3) Check the return value from nvgpu_thread_create_priority for error.
 * 4) Wait for the thread to be created by polling for a shared variable.
 * 5) Upon successful creation of the thread, confirm the priority of the
 *    thread to be same as requested priority.
 * 6) In some host machines, permission is not granted to create threads
 *    with priority.  In that case skip the test by returning PASS.
 * 7) Return PASS if the thread is created with requested priority.
 * 8) Above steps are done for thread with a name and without a name.
 *
 * Thread stop
 * 1) Follow steps 1 - 4 of Thread creation scenario.
 * 2) The created thread does not exit unconditionally in this case.
 * 3) It polls for the stop flag to be set.
 * 4) The main thread checks the status of the created thread and confirms
 *    it to be running.
 * 5) Request the thread to stop by calling nvgpu_thread_stop.
 * 6) Created thread detects this inside the poll loop and exits.
 * 7) Main thread continues once the created thread exits.
 * 8) If stop_repeat flag is set, invoke nvgpu_thread_stop again. This is done
 *    to increase branch coverage.
 * 9) Return PASS.
 *
 * Stop thread gracefully
 * 1) Follow steps 1 -  4 of Thread stop scenario.
 * 2) Call the function nvgpu_thread_stop_graceful. Depending on the flag
 *    skip_callback, either NULL or a function is passed as callback parameter
 *    to be called for graceful exit.
 * 3) Created thread detects the stop request and exits.
 * 4) Main thread continues after the created thread exits.
 * 5) If skip_callback flag is set to false, confirm if the call back function
 *    was called by checking a shared variable value.
 * 6) If stop_repeat flag is set, invoke nvgpu_thread_stop_graceful function
 *    again. This invocation should not call the callback function as the
 *    thread was already stopped in step 5.
 * 7) Return PASS.
 *
 * Output:
 * The output for each test scenario is as follows,
 * 1) Thread creation
 * Return PASS if thread creation is successful else return FAIL.
 *
 * 2) Thread creation with a priority value
 * Return PASS if thread creation with priority is successful
 * else return FAIL.  Also return PASS if permission is denied for creating
 * a thread with priority.
 *
 * 3) Thread stop
 * Return PASS if the created thread is stopped based on the request from
 * main thread.  Else return FAIL.
 *
 * 4) Stop thread gracefully
 * Return PASS if the callback function is called and the created thread is
 * stopped based on the request from main thread.
 *
 */
int test_thread_cycle(struct unit_module *m,
		struct gk20a *g, void *args);

#endif /* __UNIT_POSIX_THREAD_H__ */
