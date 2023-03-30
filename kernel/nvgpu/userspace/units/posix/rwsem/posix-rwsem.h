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
 * @addtogroup SWUTS-posix-rwsem
 * @{
 *
 * Software Unit Test Specification for posix-rwsem
 */
#ifndef __UNIT_POSIX_RWSEM_H__
#define __UNIT_POSIX_RWSEM_H__

#include <nvgpu/rwsem.h>

/**
 * Test specification for test_rwsem_init.
 *
 * Description: Initialisation of rwsem.
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_rwsem_init
 *
 * Inputs:
 * 1) Global nvgpu_rwsem instance.
 *
 * Steps:
 * 1) Call nvgpu API to initialise the rwsem.
 * 2) Sleep for some time and destroy the rwsem.
 * 3) Return success.
 *
 * Output:
 * Returns success if the rwsem is initialised.
 *
 */
int test_rwsem_init(struct unit_module *m,
		struct gk20a *g, void *args);

/**
 * Test specification for test_rwsem_read.
 *
 * Description: Testing the locking of a rwlock by multiple read threads.
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_rwsem_init, nvgpu_rwsem_down_read,
 *          nvgpu_rwsem_up_read
 *
 * Inputs:
 * 1) Global nvgpu_rwsem instance.
 * 2) Global test_data instance.
 *
 * Steps:
 * There are three threads involved in this test, a main thread which creates
 * multiple reader threads and waits for the reader threads to return.
 *
 * Main thread
 * 1) Initialize the global nvgpu_rwsem instance.
 * 2) Create a thread for first reader.
 * 3) Check for the return status of thread creation.  If thread creation
 *    fails, destroy the rwsem instance and return failure.
 * 4) Create a thread for second reader.
 * 5) Check for the return status of thread creation.  If thread creation
 *    fails, cancel the first reader thread, destroy the rwsem and return
 *    failure.
 * 6) Use pthread_join to wait for both the reader threads to return.
 * 7) Destroy the rwsem and return success.
 *
 * Reader Thread 1
 * 1) Acquires the read lock on rwsem.
 * 2) Set the flag read1_locked  as true.
 * 3) Wait on read2_locked flag till it is true.
 * 4) Release the rwsem once read2_locked is true.
 * 5) Return from the thread handler.
 *
 * Reader thread 2
 * 1) Wait on read1_locked to be true, this ensures that the first reader
 *    thread has acquired the read lock.
 * 2) Acquire the read lock on rwsem.
 * 3) Set read2_locked as true.
 * 4) Release the rwsem.
 * 5) Return from the thread handler.
 *
 * Output:
 * Returns success if both the threads are able to acquire read locks.
 * If any of the thread hangs without acquiring the lock, the test should fail
 * after the global timeout.
 *
 */
int test_rwsem_read(struct unit_module *m,
		struct gk20a *g, void *args);

/**
 * Test specification for test_rwsem_write.
 *
 * Description: Testing the locking of a rwlock by multiple write threads.
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_rwsem_init, nvgpu_rwsem_down_write,
 *          nvgpu_rwsem_up_write
 *
 * Inputs:
 * 1) Global nvgpu_rwsem instance.
 * 2) Global test_data instance.
 *
 * Steps:
 * There are three threads involved in this test, a main thread which
 * creates multiple write threads and waits for the write threads to return.
 *
 * Main thread
 * 1) Initialize the global test_data structure.
 * 2) Initialize the global nvgpu_rwsem instance.
 * 3) Update the write pattern in test_data structure.
 * 4) Create a thread for first writer.
 * 5) Check for the return status of thread creation.  If thread creation
 *    fails, destroy the rwsem instance and return failure.
 * 6) Create a thread for second writer.
 * 7) Check for the return status of thread creation.  If thread creation
 *    fails, cancel the first thread, destroy the rwsem and return
 *    failure.
 * 6) Use pthread_join to wait for both the reader threads to return.
 * 7) Check if the write pattern in test_data matches with the data written
 *    by second thread.  Return FAIL if it doesn't match.
 * 8) Destroy the rwsem and return success.
 *
 * Writer Thread 1
 * 1) Acquires the write lock on rwsem.
 * 2) Set the flag write1_locked as true.
 * 3) Delay the execution using sleep to let the second write thread try
 *    acquire the lock.
 * 4) Update the write pattern.
 * 5) Check if second write thread status indicates an acquired write lock,
 *    and populate error status if so.
 * 4) Release the rwsem.
 * 5) Return from the thread handler.
 *
 * Writer thread 2
 * 1) Wait on write1_locked to be true, this ensures that the first writer
 *    thread has acquired the write lock.
 * 2) Try to acquire the write lock on rwsem, this should put the thread in
 *    inactive state waiting for the lock to be available.
 * 3) Set write2_locked as true once the thread is woken up.
 * 4) Check for the write pattern if it matches with what thread 1 has written.
 * 5) Update the write pattern.
 * 4) Release the rwsem.
 * 5) Return from the thread handler.
 *
 * Output:
 * Return success if the second write thread is able to lock rwsem only after
 * the first write thread releases it, else return failure.
 */
int test_rwsem_write(struct unit_module *m,
		struct gk20a *g, void *args);

/**
 * Test specification for test_rwsem_write_read.
 *
 * Description: Testing the locking of a rwlock by write and read threads.
 *
 * Test Type: Feature
 *
 * Inputs:
 * 1) Global nvgpu_rwsem instance.
 * 2) Global test_data instance.
 *
 * Steps:
 * There are three threads involved in this test, a main thread which
 * creates a write thread and a read thread and waits for the threads to
 * return.
 *
 * Main thread
 * 1) Initialize the global test_data structure.
 * 2) Initialize the global nvgpu_rwsem instance.
 * 3) Update the write pattern in test_data structure.
 * 4) Create a thread for writer.
 * 5) Check for the return status of thread creation.  If thread creation
 *    fails, destroy the rwsem instance and return failure.
 * 6) Create a thread for reader.
 * 7) Check for the return status of thread creation.  If thread creation
 *    fails, cancel the first thread, destroy the rwsem and return
 *    failure.
 * 6) Use pthread_join to wait for both the reader threads to return.
 * 7) Destroy the rwsem and return success.
 *
 * Writer Thread
 * 1) Acquires the write lock on rwsem.
 * 2) Set the flag write1_locked as true.
 * 3) Delay the execution using sleep to let the second thread try
 *    acquire the lock.
 * 4) Update the write pattern.
 * 5) Check if second thread status indicates an acquired lock,
 *    and populate error status if so.
 * 4) Release the rwsem.
 * 5) Return from the thread handler.
 *
 * Reader thread
 * 1) Try to acquire the lock on rwsem, this should put the thread in
 *    inactive state waiting for the lock to be available.
 * 2) Set read1_locked as true once the thread is woken up.
 * 3) Check for the write pattern if it matches with what thread 1 has written.
 *    Update error status if the pattern does not match.
 * 4) Release the rwsem.
 * 5) Return from the thread handler.
 *
 *
 * Output:
 * Returns success if the read thread is able to lock rwsem only after
 * write thread releases it, else returns failure.
 *
 */
int test_rwsem_write_read(struct unit_module *m,
		struct gk20a *g, void *args);
#endif /* __UNIT_POSIX_RWSEM_H__ */
