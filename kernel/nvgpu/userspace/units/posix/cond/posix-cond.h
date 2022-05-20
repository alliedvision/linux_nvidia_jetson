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
 * @addtogroup SWUTS-posix-cond
 * @{
 *
 * Software Unit Test Specification for posix-cond
 */

#ifndef __UNIT_POSIX_COND_H__
#define __UNIT_POSIX_COND_H__

/**
 * Test specification for test_cond_init_destroy
 *
 * Description: Test cond init and cleanup routine.
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_cond_init, nvgpu_cond_destroy
 *
 * Inputs:
 * 1) Global instance of struct nvgpu_cond.
 *
 * Steps:
 * 1) Reset the global instance of struct nvgpu_cond with 0s.
 * 2) Call nvgpu_cond_init to initialise the condition variable.
 * 3) Check the return value for any error.
 * 4) If step 3 passes, confirm the initialisation of cond variable
 *    by checking the value of variable in struct nvgpu_cond.
 * 5) Cleanup the condition variable by calling function nvgpu_cond_destroy.
 * 6) Confirm the cleanup action by checking the value of variable inside
 *    struct nvgpu_cond
 *
 * Output:
 * The test returns PASS if cond variable initialization and cleanup functions
 * returns expected success values and internal variables in cond variable
 * structure is initialised with proper values.
 * The test returns FAIL if either initialisation or cleanup routine fails.
 * It also returns FAIL if the internal variables in cond variable structure
 * is not set with corresponding value for init and cleanup.
 *
 */
int test_cond_init_destroy(struct unit_module *m,
			struct gk20a *g, void *args);

/**
 * Test specification for test_cond_bug
 *
 * Description: Test NULL and uninitialized cond vars.
 *
 * Test Type: Feature, Error injection
 *
 * Inputs:
 * 1) Global instance of struct nvgpu_cond.
 *
 * Steps:
 * 1) Call the condition variable functions with NULL as input parameter.
 * 2) Make sure that all the called functions either invoke BUG or return
 *    an error value for NULL value as input parameter.
 * 3) Call the condition variable function with uninitialized condition
 *    variable as input parameter.
 * 4) Make sure that all the called functions either invokes BUG or returns
 *    an error value for uninitialized condition variable passed as input
 *    parameter.
 *
 * Output:
 * The test returns PASS if all the NULL and uninitialized input parameters
 * are handled by the condition variable functions by either calling BUG or by
 * returning an error value.
 * The test returns FAIL if any of the NULL or uninitialized condition variable
 * passed as input parameter is not handled as expected.
 *
 */
int test_cond_bug(struct unit_module *m,
			struct gk20a *g, void *args);

/**
 * Test specification for test_cond_signal
 *
 * Description: Functionalities of cond unit that are tested as
 * part of this function are as follows,
 * - Waiting and signaling using normal signaling, interruptible signaling
 *   and signaling protected by explicit acquire/release of the locks.
 * - Waiting and signaling using normal broadcast, interruptible broadcast
 *   and broadcast protected by explicit acquire/release of the locks.
 * - Waiting and signaling using a condition check.
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_cond_signal, nvgpu_cond_signal_locked,
 *          nvgpu_cond_broadcast, nvgpu_cond_broadcast_locked,
 *          nvgpu_cond_signal_interruptible,
 *          nvgpu_cond_broadcast_interruptible,
 *          nvgpu_cond_lock, nvgpu_cond_unlock, nvgpu_cond_timedwait,
 *          NVGPU_COND_WAIT, NVGPU_COND_WAIT_LOCKED,
 *          NVGPU_COND_WAIT_INTERRUPTIBLE,
 *          NVGPU_COND_WAIT_TIMEOUT_LOCKED
 *
 * Inputs:
 * 1) Global instance of struct nvgpu_cond.
 * 2) Global array test_code.
 * 3) Global instance of struct unit_test_cond_data.
 * 4) Global variables read_status, bcst_read_status.
 * 5) Global variables read_wait, bcst_read_wait.
 * 6) Function argument of type pointer to struct test_cond_args.
 *
 * Steps:
 * All the above mentioned functionalities are tested by this function based
 * on the input arguments.  Steps for various tests are as mentioned below,
 *
 * a) Wait and Signal
 *    Three threads are involved in this test case.
 *    A main thread which creates a write thread and a read thread and then
 *    waits for the created threads to exit.
 *
 *    - Main Thread:
 *      1) Main thread resets the global variables test_code, test_cond
 *         and test_data.
 *      2) Initialise the condition variable by calling nvgpu_cond_init.
 *      3) Return failure if the init function returns error.
 *      4) Copy the test args into global instance of unit_test_cond_data.
 *      5) Reset global variables read_status and bcst_read_status to 0.
 *      6) Create the read thread.
 *      7) Cleanup the initialised cond variable and return failure if read
 *         thread creation fails.
 *      8) Create the write thread.
 *      9) Cleanup the initialised cond variable, cancel the read thread and
 *         return failure if write thread creation fails.
 *     10) Wait for both read and write thread to exit using pthread_join.
 *     11) Check for global variable read_status and return FAIL if the value
 *         indicates an error.
 *     12) Return test PASS.
 *
 *    - Read Thread:
 *      1) Set global variable read_wait as true.  This is used by write thread
 *         to continue further.
 *      2) Wait on the condition variable.
 *      3) On getting signalled, check for the pattern in test_code.
 *      4) If the data does not match the written value, update read_status
 *         with error code.
 *      5) Return from the thread handler.
 *
 *    - Write Thread:
 *      1) Wait on global variable read_wait to be true before proceeding
 *         further.
 *      2) Update the global array test_code with a defined value.
 *      3) Reset read_wait to 0.
 *      4) Signal the condition variable.
 *      5) Return from the thread handler.
 *
 * b) Wait and Signal interruptible
 *    The steps followed are the same as case a.  But the signaling API
 *    used by write thread in step 4 is nvgpu_cond_signal_interruptible.
 *    Although functionality wise both nvgpu_cond_signal and
 *    nvgpu_cond_signal_interruptible are same, this test just ensures
 *    better code coverage.
 *
 * c) Wait and Signal locked
 *    The steps followed are the same as case a.  But the write thread
 *    needs to explicitly acquire the mutex lock before signalling the
 *    read thread.  The lock has to be released explicitly once the signal
 *    API is called.
 *
 * d) Timed Wait and Signal
 *    The test differs from case a on the duration of time used to wait for
 *    the signal.  In this case the wait is limited to a predefined duration of
 *    time rather than wait forever as it is in case a.
 *
 * e) Wait and Broadcast
 *    In broadcast test cases an extra read thread is created by the main
 *    thread.  Both the read threads will get blocked on the condition variable.
 *    The write thread has to broadcast the signal, which should bring both
 *    the read threads out of blocked state. The main thread needs to wait for
 *    the extra read thread also to exit in this case.
 *
 * f) Wait and Broadcast interruptible
 *    The write thread uses the nvgpu_cond_broadcast_interruptible API to
 *    broadcast the signal.
 *
 * g) Wait and Broadcast locked
 *    The write thread has to explicitly acquire the lock before broadcasting
 *    the signal and needs to release the lock explicitly after broadcast.
 *
 * h) Wait on condition
 *    The read thread waits for a particular condition to be met, rather than
 *    just blocking on the condition variable.
 *
 * i) Wait on condition interruptible
 *    The read thread uses the interruptible version of wait in this scenario.
 *
 * j) Wait on condition locked
 *    The read thread needs to explicitly acquire the lock before issuing a wait
 *    on the condition variable.  And also needs to explicitly release the lock
 *    after getting unblocked.
 *
 * Output:
 * All the tests return PASS if the condition variable is properly signalled
 * by the write thread and further verification of shared data shows a
 * successful update from write thread with a predefined value.
 * The tests return FAIL, if any of the above conditions are not met.
 *
 */
int test_cond_signal(struct unit_module *m,
			struct gk20a *g, void *args);

/**
 * Test specification for test_cond_timeout
 *
 * Description: Test time out for a condition variable wait.
 *
 * Test Type: Feature, Error injection
 *
 * Inputs:
 * 1) Global instance of struct nvgpu_cond.
 *
 * Steps:
 * 1) Initialize the condition variable.
 * 2) Call the function nvgpu_cond_timedwait with a timeout value.
 * 3) Check the return value from the function nvgpu_cond_timedwait. If the
 *    return value is not ETIMEDOUT, unlock the mutex associated with the
 *    condition variable then destroy the condition variable and return fail.
 * 4) If the return value is ETIMEDOUT, check the actual duration of timed
 *    wait. If it is less than the requested timeout value, unlock the mutex
 *    associated with the condition variable then destroy the condition
 *    variable and return FAIL.
 * 5) Unlock the mutex associated with the condition variable then destroy the
 *    condition variable and return pass.
 *
 * Output:
 * The test returns PASS if the nvgpu_cond_timedwait function returns
 * ETIMEDOUT error.
 * The test returns FAIL if the return value from nvgpu_cond_timedwait function
 * is not ETIMEDOUT.
 *
 */
int test_cond_timeout(struct unit_module *m,
			struct gk20a *g, void *args);
#endif /* __UNIT_POSIX_COND_H__ */
