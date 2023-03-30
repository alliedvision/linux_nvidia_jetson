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

#ifndef NVGPU_COND_H
#define NVGPU_COND_H

#ifdef __KERNEL__
#include <nvgpu/linux/cond.h>
#else
#include <nvgpu/posix/cond.h>
#endif

/*
 * struct nvgpu_cond
 *
 * Should be implemented per-OS in a separate library
 */
struct nvgpu_cond;

/**
 * @brief Initialize a condition variable.
 *
 * Initializes a condition variable pointed by \a cond before using it.
 * Invoke the following functions before setting the \a initialized variable
 * in #nvgpu_cond as true.
 * - #pthread_condattr_init with \a attr in #nvgpu_cond as parameter to
 *   initialise the attributes associated with the condition variable.
 * - #pthread_condattr_setclock is invoked to set \a CLOCK_MONOTONIC in \a attr
 *   in #nvgpu_cond. If the function returns an error value, invoke the function
 *   #pthread_condattr_destroy with \a attr in #nvgpu_cond as parameter to
 *   destroy the attributes and return the error value.
 * - #nvgpu_mutex_init() with \a mutex in #nvgpu_cond as parameter to initialise
 *   the mutex.
 * - #pthread_cond_init with \a cond in #nvgpu_cond and \a attr in #nvgpu_cond
 *   as parameters to initialize the condition variable. If the function
 *   returns an error value, invoke the function #pthread_condattr_destroy with
 *   \a attr in #nvgpu_cond as parameter to destroy the attributes followed by
 *   #nvgpu_mutex_destroy() with \a mutex in #nvgpu_cond as parameter to
 *   destroy the mutex. Return the error value from #pthread_cond_init.
 * Set the variable \a initialized in #nvgpu_cond as true upon successful
 * initialization of \a cond.
 * Function does not perform any validation of the parameter.
 *
 * @param cond [in]	The condition variable to initialize.
 *
 * @return If successful, this function returns 0. Otherwise, an error number
 * is returned to indicate the error.
 *
 * @retval 0 for success.
 * @retval ENOMEM for insufficient memory.
 * @retval EINVAL for invalid value.
 * @retval EBUSY for a pthread condition variable pointer to a previously
 * initialized condition variable that hasn't been destroyed.
 * @retval EFAULT for any faults that occurred while trying to access the
 * condition variable or attribute.
 */
int nvgpu_cond_init(struct nvgpu_cond *cond);

/**
 * @brief Signal a condition variable.
 *
 * This function is used to unblock a thread blocked on a condition variable.
 * Wakes up at least one of the threads that are blocked on the specified
 * condition variable \a cond. Following sequence of functions are invoked
 * internally to signal a condition variable,
 * - #nvgpu_mutex_acquire() with \a mutex in #nvgpu_cond as parameter to
 *   acquire the mutex associated with the condition variable.
 * - #pthread_cond_signal with \a cond in #nvgpu_cond as parameter to signal.
 * - #nvgpu_mutex_release() with \a mutex in #nvgpu_cond as parameter to
 *   release the mutex.
 * - Invoke #nvgpu_assert() if the function #pthread_cond_signal returns an
 *   error.
 *
 * @param cond [in]	The condition variable to signal.
 * 			  - Should not to be equal to NULL.
 * 			  - Structure pointed by \a cond should be initialized
 * 			    before invoking this function.
 */
void nvgpu_cond_signal(struct nvgpu_cond *cond);

/**
 * @brief Signal a condition variable.
 *
 * Wakes up at least one of the threads that are blocked on the specified
 * condition variable \a cond. In Posix implementation, the function provides
 * the same functionality as #nvgpu_cond_signal, but this function is being
 * provided to be congruent with kernel implementations having interruptible
 * and uninterruptible waits. Following sequence of functions are invoked
 * internally to signal a condition variable,
 * - #nvgpu_mutex_acquire() with \a mutex in #nvgpu_cond as parameter to
 *   acquire the mutex associated with the condition variable.
 * - #pthread_cond_signal with \a cond in #nvgpu_cond as parameter to signal.
 * - #nvgpu_mutex_release() with \a mutex in #nvgpu_cond as parameter to
 *   release the mutex.
 * - Invoke #nvgpu_assert() if the function #pthread_cond_signal returns an
 *   error.
 *
 * @param cond [in]	The condition variable to signal.
 * 			  - Should not to be equal to NULL.
 * 			  - Structure pointed by \a cond should be initialized
 * 			    before invoking this function.
 */
void nvgpu_cond_signal_interruptible(struct nvgpu_cond *cond);

/**
 * @brief Signal all waiters of a condition variable.
 *
 * This function is used to unblock threads blocked on a condition variable.
 * Wakes up all the threads that are blocked on the specified condition variable
 * \a cond. Following sequence of functions are invoked internally to broadcast
 * a condition variable,
 * - #nvgpu_mutex_acquire() with \a mutex in #nvgpu_cond as parameter to
 *   acquire the mutex associated with the condition variable.
 * - #pthread_cond_broadcast with \a cond in #nvgpu_cond as parameter to
 *   broadcast.
 * - #nvgpu_mutex_release() with \a mutex in #nvgpu_cond as parameter to
 *   release the mutex.
 *
 * @param cond [in]	The condition variable to broadcast.
 * 			  - Should not to be equal to NULL.
 * 			  - Structure pointed by \a cond should be initialized
 * 			    before invoking this function.
 *
 * @return If successful, this function returns 0. Otherwise, an error number
 * is returned to indicate the error.
 *
 * @retval 0 for success.
 * @retval -EINVAL if \a cond is NULL or not initialized.
 * @retval EFAULT a fault occurred while trying to access the buffers provided.
 * @retval EINVAL for invalid condition variable. This error value is generated
 * by the OS API used inside this function, which is returned as it is.
 */
int nvgpu_cond_broadcast(struct nvgpu_cond *cond);

/**
 * @brief Signal all waiters of a condition variable.
 *
 * This function is used to unblock threads blocked on a condition variable.
 * Wakes up all the threads that are blocked on the specified condition variable
 * \a cond. In Posix implementation this API is same as #nvgpu_cond_broadcast in
 * functionality, but the API is provided to be congruent with implementations
 * having interruptible and uninterruptible waits. Following sequence of
 * functions are invoked internally to broadcast a condition variable,
 * - #nvgpu_mutex_acquire() with \a mutex in #nvgpu_cond as parameter to
 *   acquire the mutex associated with the condition variable.
 * - #pthread_cond_broadcast with \a cond in #nvgpu_cond as parameter to
 *   broadcast.
 * - #nvgpu_mutex_release() with \a mutex in #nvgpu_cond as parameter to
 *   release the mutex.
 *
 * @param cond [in]	The condition variable to broadcast.
 * 			  - Should not to be equal to NULL.
 * 			  - Structure pointed by \a cond should be initialized
 * 			    before invoking this function.
 *
 * @return If successful, this function returns 0. Otherwise, an error number
 * is returned to indicate the error.
 *
 * @retval 0 for success.
 * @retval -EINVAL if \a cond is NULL or not initialized.
 * @retval EFAULT a fault occurred while trying to access the buffers provided.
 * @retval EINVAL for invalid condition variable. This error value is generated
 * by the OS API used inside this function, which is returned as it is.
 */
int nvgpu_cond_broadcast_interruptible(struct nvgpu_cond *cond);

/**
 * @brief Destroy a condition variable.
 *
 * Destroys the condition variable pointed by \a cond along with the
 * associated attributes and mutex. Following sequence of functions are invoked
 * for the same,
 * - #pthread_cond_destroy with \a cond in #nvgpu_cond as parameter to destroy
 *   the condition variable. Invoke #nvgpu_assert() if the function returns an
 *   error.
 * - #nvgpu_mutex_destroy() with \a mutex in #nvgpu_cond as parameter to
 *   destroy the mutex.
 * - #pthread_condattr_destroy with \a attr in #nvpgu_cond as parameter to
 *   destroy the attributes associated with the condition variable.
 * Set \a initialized variable in #nvgpu_cond as false to indicate that the
 * condition variable is not initialized.
 *
 * @param cond [in]	The condition variable to destroy.
 * 			  - Should not to be equal to NULL.
 */
void nvgpu_cond_destroy(struct nvgpu_cond *cond);

#endif /* NVGPU_COND_H */
