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

#ifndef NVGPU_THREAD_H
#define NVGPU_THREAD_H

#ifdef __KERNEL__
#include <nvgpu/linux/thread.h>
#else
#include <nvgpu/posix/thread.h>
#endif

#include <nvgpu/types.h>
#include <nvgpu/utils.h>

/**
 * @brief Create and run a new thread.
 *
 * Create a thread and run threadfn in it. The thread stays alive as long as
 * threadfn is running. As soon as threadfn returns the thread is destroyed.
 * The threadfn needs to poll #nvgpu_thread_should_stop() to determine if it
 * should exit. Following steps are executed to create a thread,
 * - Invoke library function #memset to clear the thread structure pointed by
 *   \a thread. The parameters passed are \a thread, 0 and the size of the
 *   thread structure.
 * - If \a name is not #NULL, populate the variable \a tname in #nvgpu_thread
 *   with \a name. Library function #strncpy is used for this with parameters
 *   \a tname in #nvgpu_thread, \a name and length to be copied.
 * - Populate the initial values for variable sin #nvgpu_thread structure and
 *   use the function #nvgpu_atomic_set() with \a running in #nvgpu_thread and
 *   1 as parameters to set the running status of the thread.
 * - Invoke the library function #pthread_attr_init to initialize the
 *   attributes structure i#pthread_attr_t needed for thread creation.
 * - Invoke the library function #pthread_create to create the thread.
 *   Parameters passed are \a thread in #nvgpu_thread, attribute structure,
 *   #nvgpu_posix_thread_wrapper and \a nvgpu in #nvgpu_thread. If the thread
 *   creation returns an error, invoke function #pthread_attr_destroy to
 *   destroy the attributes structure and return the error value from
 *   #pthread_create as it is.
 * - Invoke #pthread_attr_destroy to destroy the attributes structure after
 *   successful creation of thread. If this invocation fails, cancel the created
 *   thread using function #pthread_cancel with \a thread in #nvgpu_thread as
 *   parameter.
 * Function does not perform any validation of the parameters.
 *
 *
 * @param thread [in]	Thread structure to use.
 * @param data [in]	Data to pass to threadfn.
 * @param threadfn [in]	Thread function.
 * @param name [in]	Name of the thread.
 *
 * @return Return 0 on success, otherwise returns error number to indicate the
 * error.
 *
 * @retval EINVAL invalid thread attribute object.
 * @retval EAGAIN insufficient system resources to create thread.
 * @retval EFAULT error occurred trying to access the buffers or the
 * start routine provided for thread creation.
 */
int nvgpu_thread_create(struct nvgpu_thread *thread,
		void *data,
		int (*threadfn)(void *data), const char *name);

/**
 * @brief Stop a thread.
 *
 * A thread can be requested to stop by calling this function. This function
 * places a request to cancel the corresponding thread by setting a status and
 * waits until that thread exits. Uses the function #nvgpu_atomic_cmpxchg()
 * with \a running in #nvgpu_thread as a parameter to check if the running
 * status of the thread is set to true and in case if it is true, replace it
 * with false. If the thread was not stopped already, uses the library function
 * #pthread_cancel to cancel the thread and #pthread_join to join the cancelled
 * thread with parameter as \a thread in #nvgpu_thread. Function does not
 * perform any validation of the parameter.
 *
 * @param thread [in]	Thread to stop.
 */
void nvgpu_thread_stop(struct nvgpu_thread *thread);

/**
 * @brief Request a thread to be stopped gracefully.
 *
 * Request a thread to stop by setting the status of the thread appropriately.
 * In posix implementation, the callback function \a thread_stop_fn is invoked
 * and waits till the thread exits. Uses the function #nvgpu_atomic_cmpxchg()
 * with \a running in #nvgpu_thread as a parameter to check if the running
 * status of the thread is set to true, and in case if it is true, replace it
 * with false. If the thread was not stopped already, invoke the callback
 * function \a thread_stop_fn if it is not #NULL and invoke #pthread_join to
 * join the cancelled thread with parameter as \a thread in #nvgpu_thread.
 * Function does not perform any validation of the parameters.
 *
 * @param thread [in]		Thread to stop.
 * @param thread_stop_fn [in]	Callback function to trigger graceful exit.
 * @param data [in]		Thread data.
 */
void nvgpu_thread_stop_graceful(struct nvgpu_thread *thread,
		void (*thread_stop_fn)(void *data), void *data);

/**
 * @brief Query if thread should stop.
 *
 * Return true if thread should exit. Can be run only in the thread's own
 * context. Uses the function #nvgpu_atomic_read with variable \a running in
 * #nvgpu_thread as parameter to check if the thread has to stop.
 * Function does not perform any validation of the parameter.
 *
 * @param thread [in]	Thread to be queried for stop status.
 *
 * @return Boolean value which indicates if the thread has to stop or not.
 *
 * @retval TRUE if the thread should stop.
 * @retval FALSE if the thread need not stop.
 */
bool nvgpu_thread_should_stop(struct nvgpu_thread *thread);

/**
 * @brief Query if thread is running.
 *
 * Returns a boolean value based on the current running status of the thread.
 * Uses the function #nvgpu_atomic_read with variable \a running in
 * #nvgpu_thread as parameter to fetch the running status of the thread.
 * Function does not perform any validation of the parameter.
 *
 * @param thread [in]	Thread to be queried for running status.
 *
 * @return TRUE if the current running status of the thread is 1, else FALSE.
 *
 * @retval TRUE if the thread is running.
 * @retval FALSE if the thread is not running.
 */
bool nvgpu_thread_is_running(struct nvgpu_thread *thread);

/**
 * @brief Join a thread to reclaim resources after it has exited.
 *
 * The calling thread waits till the thread referenced by \a thread exits.
 * USes the library function #pthread_join with \a thread in #nvgpu_thread and
 * #NULL as parameters to join the thread. Function does not perform any
 * validation of the parameter.
 *
 * @param thread [in] - Thread to join.
 */
void nvgpu_thread_join(struct nvgpu_thread *thread);

#endif /* NVGPU_THREAD_H */
