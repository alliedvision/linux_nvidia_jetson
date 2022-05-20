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
#ifndef NVGPU_RWSEM_H
#define NVGPU_RWSEM_H

#ifdef __KERNEL__
#include <nvgpu/linux/rwsem.h>
#else
#include <nvgpu/posix/rwsem.h>
#endif

/*
 * struct nvgpu_rwsem
 *
 * Should be implemented per-OS in a separate library
 * But implementation should adhere to rw_semaphore implementation
 * as specified in Linux Documentation
 */
struct nvgpu_rwsem;

/**
 * @brief Initialize read write lock.
 *
 * Initializes the read,write lock referenced by \a rwsem. Uses the library
 * function #pthread_rwlock_init with \a rw_sem in #nvgpu_rwsem and NULL as
 * parameters to initialize the lock. Assert using function #nvgpu_assert()
 * if the return value from #pthread_rwlock_init is not 0.
 *
 * @param rwsem [in]	Read,write lock to initialize.
 */
void nvgpu_rwsem_init(struct nvgpu_rwsem *rwsem);

/**
 * @brief Release read lock.
 *
 * Releases the lock held on \a rwsem. Uses the library function
 * #pthread_rwlock_unlock with \a rw_sem in #nvgpu_rwsem as parameter to
 * release the read lock. Assert using #nvgpu_assert() if the return value
 * from #pthread_rwlock_unlock is not 0.
 *
 * @param rwsem [in]	Read,write lock to be released.
 */
void nvgpu_rwsem_up_read(struct nvgpu_rwsem *rwsem);

/**
 * @brief Acquire read lock.
 *
 * Acquires a read lock on the read,write lock object referenced by \a rwsem.
 * Uses the library function #pthread_rwlock_rdlock with \a rw_sem in
 * #nvgpu_rwsem as parameter to acquire the read lock. The calling thread
 * acquires a read lock if no writer is holding the lock on \a rwsem. Assert
 * using #nvgpu_assert() if the return value from #pthread_rwlock_rdlock is
 * not 0.
 *
 * @param rwsem [in]	Read,write lock to be locked for read.
 */
void nvgpu_rwsem_down_read(struct nvgpu_rwsem *rwsem);

/**
 * @brief Release the write lock.
 *
 * Releases the write lock held on \a rwsem. Uses the library function
 * #pthread_rwlock_unlock with \a rw_sem in #nvgpu_rwsem as parameter to
 * release the write lock. Assert using #nvgpu_assert() if the return value
 * from #pthread_rwlock_unlock is not 0.
 *
 * @param rwsem [in]	Read,write lock to be released.
 */
void nvgpu_rwsem_up_write(struct nvgpu_rwsem *rwsem);

/**
 * @brief Acquire write lock.
 *
 * Acquires the write lock on \a rwsem. The calling thread acquires write lock
 * if no other thread is holding a lock on rwsem. Uses the library function
 * #pthread_rwlock_wrlock with \a rw_sem in #nvgpu_rwsem as parameter to
 * acquire the write lock. Assert using #nvgpu_assert() if the return value
 * from #pthread_rwlock_wrlock is not 0.
 *
 * @param rwsem [in]	Read,write lock to be locked for write.
 */
void nvgpu_rwsem_down_write(struct nvgpu_rwsem *rwsem);

#endif /* NVGPU_RWSEM_H */
