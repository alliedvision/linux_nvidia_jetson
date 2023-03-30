/*
 * Copyright (c) 2018-2021, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef NVGPU_LOCK_H
#define NVGPU_LOCK_H

#ifdef __KERNEL__
#include <nvgpu/linux/lock.h>
#else
#include <nvgpu/posix/lock.h>
#endif

/*
 * struct nvgpu_mutex
 *
 * Should be implemented per-OS in a separate library
 * But implementation should adhere to mutex implementation
 * as specified in Linux Documentation
 */
struct nvgpu_mutex;

/*
 * struct nvgpu_spinlock
 *
 * Should be implemented per-OS in a separate library
 * But implementation should adhere to spinlock implementation
 * as specified in Linux Documentation
 */
struct nvgpu_spinlock;

/*
 * struct nvgpu_raw_spinlock
 *
 * Should be implemented per-OS in a separate library
 * But implementation should adhere to raw_spinlock implementation
 * as specified in Linux Documentation
 */
struct nvgpu_raw_spinlock;

/**
 * @brief Initialize the mutex object.
 *
 * Initialize the mutex object. Invokes the library function
 * #pthread_mutex_init with \a lock.mutex in #nvgpu_mutex as parameter to
 * initialize the mutex. Assert using #nvgpu_assert if the initialization
 * returns error. Function does not perform any validation of the parameter.
 *
 * @param mutex [in]	Mutex to initialize.
 */
void nvgpu_mutex_init(struct nvgpu_mutex *mutex);

/**
 * @brief Acquire the mutex object.
 *
 * Function locks the mutex object. If the mutex is already locked, then the
 * calling thread blocks until it has acquired the mutex. When the function
 * returns, the mutex object is locked and owned by the calling thread.
 * Uses the function #nvgpu_posix_lock_acquire() with \a lock in #nvgpu_mutex
 * as parameter to acquire the lock. Function does not perform any validation
 * of the parameter.
 *
 * @param mutex [in]	Mutex to acquire.
 */
void nvgpu_mutex_acquire(struct nvgpu_mutex *mutex);

/**
 * @brief Release the mutex object.
 *
 * Release the mutex object. The mutex object referenced by \a mutex will be
 * unlocked. The mutex should be owned by the calling thread to unlock it.
 * Uses the function #nvgpu_posix_lock_release() with \a lock in #nvgpu_mutex
 * as parameter to release the lock. Function does not perform any validation
 * of the parameter.
 *
 * @param mutex [in]	Mutex to release.
 */
void nvgpu_mutex_release(struct nvgpu_mutex *mutex);

/**
 * @brief Attempt to lock the mutex object.
 *
 * Attempts to lock the mutex object. If the mutex is already locked, this
 * function returns an error code and hence the calling thread is not blocked.
 * Uses the function #nvgpu_posix_lock_try_acquire() with \a lock in
 * #nvgpu_mutex as parameter to acquire the lock. Function does not perform
 * any validation of the parameter.
 *
 * @param mutex [in]	Mutex to try acquire.
 *
 * @return Integer value to indicate whether the mutex was successfully acquired
 * or not.
 *
 * @retval 1 if mutex is acquired.
 * @retval 0 if mutex is not acquired.
 */
int nvgpu_mutex_tryacquire(struct nvgpu_mutex *mutex);

/**
 * @brief Destroy the mutex object.
 *
 * The function shall destroy the mutex object referenced by \a mutex. The
 * mutex object becomes uninitialized in effect. Uses the library function
 * #pthread_mutex_destroy with \a lock.mutex in #nvgpu_mutex as parameter
 * to destroy. Assert using #nvgpu_assert if the library function returns
 * error. Function does not perform any validation of the parameter.
 *
 * @param mutex [in]	Mutex to destroy.
 */
void nvgpu_mutex_destroy(struct nvgpu_mutex *mutex);

/**
 * @brief Initialize the spinlock object.
 *
 * Initialize the spinlock object. Underlying implementation and behaviour is
 * dependent on the OS. Posix implementation internally uses mutex to support
 * spinlock APIs. There is no reason to have a real spinlock implementation in
 * userspace as the contexts which use the spinlock APIs are permitted to sleep
 * in userspace execution mode, Hence all the spinlock APIs internally uses
 * mutex logic in Posix implementation. Invokes the library function
 * #pthread_mutex_init with \a lock.mutex in #nvgpu_spinlock as parameter to
 * initialize. Assert using #nvgpu_assert if the initialization returns error.
 * Function does not perform any validation of the parameter.
 *
 * @param spinlock [in]	Spinlock to initialize.
 */
void nvgpu_spinlock_init(struct nvgpu_spinlock *spinlock);

/**
 * @brief Acquire the spinlock object.
 *
 * Acquire the spinlock object. Underlying implementation and behaviour is
 * dependent on the OS. Since pthread mutex is used internally for posix
 * implementation, this function might put the calling thread in sleep state
 * if the lock is not available. Uses the function #nvgpu_posix_lock_acquire()
 * with \a lock in #nvgpu_spinlock as parameter to acquire the lock.
 * Function does not perform any validation of the parameter.
 *
 * @param spinlock [in]	Spinlock to acquire.
 */
void nvgpu_spinlock_acquire(struct nvgpu_spinlock *spinlock);

/**
 * @brief Release the spinlock object.
 *
 * Releases the spinlock object referenced by \a spinlock. Uses the function
 * #nvgpu_posix_lock_release() with \a lock in #nvgpu_spinlock as parameter to
 * release the lock. Function does not perform any validation of the parameter.
 *
 * @param spinlock [in]	Spinlock to release.
 */
void nvgpu_spinlock_release(struct nvgpu_spinlock *spinlock);

/**
 * @brief Initialize the raw spinlock object.
 *
 * Initialize the raw spinlock object. Underlying implementation and behaviour
 * is dependent on the OS. Posix implementation internally uses mutex to support
 * raw spinlock APIs. Invokes the library function #pthread_mutex_init with
 * \a lock.mutex in #nvgpu_raw_spinlock as parameter to initialize. Assert
 * using #nvgpu_assert if the initialization returns error. Function does not
 * perform any validation of the parameter.
 *
 * @param spinlock [in]	Raw spinlock structure to initialize.
 */
void nvgpu_raw_spinlock_init(struct nvgpu_raw_spinlock *spinlock);

/**
 * @brief Acquire the raw spinlock object.
 *
 * Acquire the raw spinlock object. Underlying implementation and behaviour is
 * dependent on the OS. Since pthread mutex is used internally for posix
 * implementation, this function might put the calling thread in sleep state if
 * the lock is not available. Uses the function #nvgpu_posix_lock_acquire()
 * with \a lock in #nvgpu_raw_spinlock as parameter to acquire the lock.
 * Function does not perform any validation of the parameter.
 *
 * @param spinlock [in]	Raw spinlock to acquire.
 */
void nvgpu_raw_spinlock_acquire(struct nvgpu_raw_spinlock *spinlock);

/**
 * @brief Release the raw spinlock object.
 *
 * Release the raw spinlock object. Uses the function
 * #nvgpu_posix_lock_release() with \a lock in #nvgpu_raw_spinlock as
 * parameter to release the lock. Function does not perform any validation
 * of the parameter.
 *
 * @param spinlock [in]	Raw spinlock to release.
 */
void nvgpu_raw_spinlock_release(struct nvgpu_raw_spinlock *spinlock);

#endif /* NVGPU_LOCK_H */
