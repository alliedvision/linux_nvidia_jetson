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

#ifndef UNIT_LOCK_H
#define UNIT_LOCK_H

struct gk20a;
struct unit_module;

/** @addtogroup SWUTS-interface-lock
 *  @{
 *
 * Software Unit Test Specification for interface.lock
 */

/**
 * \name Lock Types
 * There are 3 types of locks supported by this unit test, defined as below.
 */
/** @{ */
#define TYPE_MUTEX		0
#define TYPE_SPINLOCK		1
#define TYPE_RAW_SPINLOCK	2
/** @} */

/**
 * Test specification for: test_mutex_init
 *
 * Description: Simple test to check mutex init routine.
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_mutex_init, nvgpu_mutex_destroy
 *
 * Input: None
 *
 * Steps:
 * - Initialize a mutex via the nvgpu_mutex_init function.
 * - Destroy the mutex via the nvgpu_mutex_destroy function.
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_mutex_init(struct unit_module *m, struct gk20a *g, void *args);

/**
 * Test specification for: test_mutex_tryacquire
 *
 * Description: Test to verify the behavior of mutex tryacquire function.
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_mutex_init, nvgpu_mutex_acquire, nvgpu_mutex_tryacquire,
 *          nvgpu_mutex_release, nvgpu_mutex_destroy,
 *          nvgpu_posix_lock_try_acquire, nvgpu_posix_lock_release
 *
 * Input: None
 *
 * Steps:
 * - Initialize a mutex via the nvgpu_mutex_init function.
 * - Acquire the mutex.
 * - Perform a nvgpu_mutex_tryacquire on the mutex and ensure that the operation
 *   returned a failure code.
 * - Release the mutex.
 * - Perform a nvgpu_mutex_tryacquire again and ensure that the operation
 *   succeeded.
 * - Release and destroy the mutex.
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_mutex_tryacquire(struct unit_module *m, struct gk20a *g, void *args);

/**
 * Test specification for: test_lock_acquire_release
 *
 * Description: Test to verify the behavior of mutex, regular and raw spinlocks
 * acquire and release functions. For this purpose, there are 2 threads
 * involved: the regular main thread, and a worker thread.
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_mutex_init, nvgpu_spinlock_init, nvgpu_raw_spinlock_init,
 *          nvgpu_mutex_acquire, nvgpu_spinlock_acquire,
 *          nvgpu_raw_spinlock_acquire, nvgpu_mutex_release,
 *          nvgpu_spinlock_release, nvgpu_raw_spinlock_release,
 *          nvgpu_posix_lock_acquire, nvgpu_posix_lock_release
 *
 * Input: @param args [in] Type of lock as defined by TYPE_* macros.
 *
 * Steps:
 * - Initialize the lock using the corresponding init function.
 * - Create a semaphore \a worker_sem and set the \a test_shared_flag to false.
 * - Acquire the lock using its corresponding acquire function.
 * - Create the worker thread and wait for it to signal that it is ready thanks
 *   to the \a worker_sem.
 * - The worker thread then blocks trying to acquire the lock.
 * - The main thread then releases the lock and wait for a signal from the
 *   worker thread via \a worker_sem.
 * - The worker thread should now be able to acquire the lock and update the
 *   \a test_shared_flag.
 * - The main thread ensures that the \a test_shared_flag was updated.
 * - Release and destroy the lock and the worker thread.
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_lock_acquire_release(struct unit_module *m, struct gk20a *g,
					void *args);

/** }@ */
#endif /* UNIT_LOCK_H */
