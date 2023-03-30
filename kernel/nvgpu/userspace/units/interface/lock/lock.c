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

#include <unit/io.h>
#include <unit/unit.h>
#include <unit/unit-requirement-ids.h>

#include <nvgpu/io.h>
#include <nvgpu/posix/io.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/lock.h>

#include <pthread.h>
#include <unistd.h>

#include "lock.h"

/* Parameter structure to pass to worker threads */
struct worker_parameters {
	u64 type;
	struct nvgpu_mutex *mutex;
	struct nvgpu_spinlock *lock;
	struct nvgpu_raw_spinlock *raw_lock;
};

int worker_sem = -1;
/* Mutex for atomicity of operations on "worker_sem" */
pthread_mutex_t nvgpu_sem_mutex = PTHREAD_MUTEX_INITIALIZER;
bool test_shared_flag;

static void nvgpu_sem_init(int *sem)
{
	(*sem) = -1;
	pthread_mutex_init(&nvgpu_sem_mutex, NULL);

	return;
}

static void nvgpu_sem_post(int *sem)
{
	pthread_mutex_lock(&nvgpu_sem_mutex);
	(*sem) = (*sem) + 1;
	pthread_mutex_unlock(&nvgpu_sem_mutex);

	return;
}

static void nvgpu_sem_wait(int *sem)
{
	pthread_mutex_lock(&nvgpu_sem_mutex);
	while ((*sem) == -1) {
		pthread_mutex_unlock(&nvgpu_sem_mutex);
		/* Wait for 1 msec */
		usleep(1000);
		pthread_mutex_lock(&nvgpu_sem_mutex);
	}

	(*sem) = (*sem) - 1;
	pthread_mutex_unlock(&nvgpu_sem_mutex);

	return;
}

int test_mutex_init(struct unit_module *m, struct gk20a *g, void *args)
{
	struct nvgpu_mutex mutex;

	nvgpu_mutex_init(&mutex);
	nvgpu_mutex_destroy(&mutex);

	return UNIT_SUCCESS;
}

int test_mutex_tryacquire(struct unit_module *m, struct gk20a *g, void *args)
{
	struct nvgpu_mutex mutex;
	int status;

	nvgpu_mutex_init(&mutex);

	nvgpu_mutex_acquire(&mutex);

	status = nvgpu_mutex_tryacquire(&mutex);

	if (status == 1) {
		unit_return_fail(m, "tryacquire did not fail as expected: %d\n",
			status);
	}

	nvgpu_mutex_release(&mutex);

	status = nvgpu_mutex_tryacquire(&mutex);

	if (status != 1) {
		unit_return_fail(m,
			"tryacquire did not succeed as expected: %d\n", status);
	}

	nvgpu_mutex_release(&mutex);
	nvgpu_mutex_destroy(&mutex);

	return UNIT_SUCCESS;
}

/*
 * Worker function to be used in a separate thread to test acquire of one of
 * the supported lock types, provided as an argument.
 */
static void *lock_worker(void *args)
{
	struct worker_parameters *params = (struct worker_parameters *) args;

	/* Signal main testing function that the worker thread has started. */
	nvgpu_sem_post(&worker_sem);

	/*
	 * Lock should already be held by the main test function, so execution
	 * should block here.
	 */
	switch (params->type) {
	case TYPE_MUTEX:
		nvgpu_mutex_acquire(params->mutex);
		break;
	case TYPE_SPINLOCK:
		nvgpu_spinlock_acquire(params->lock);
		break;
	case TYPE_RAW_SPINLOCK:
		nvgpu_raw_spinlock_acquire(params->raw_lock);
		break;
	default:
		break;
	}

	/*
	 * Update the shared_flag to indicate that the spinlock_acquire
	 * succeeded and signal the main thread.
	 */
	test_shared_flag = true;
	nvgpu_sem_post(&worker_sem);

	/* Cleanup */
	switch (params->type) {
	case TYPE_MUTEX:
		nvgpu_mutex_release(params->mutex);
		break;
	case TYPE_SPINLOCK:
		nvgpu_spinlock_release(params->lock);
		break;
	case TYPE_RAW_SPINLOCK:
		nvgpu_raw_spinlock_release(params->raw_lock);
		break;
	default:
		break;
	}
	return NULL;
}

/*
 * Test to verify the behavior of mutex, regular and raw spinlocks acquire and
 * release functions.
 * The "args" argument is an int indicating the lock type.
 */
int test_lock_acquire_release(struct unit_module *m, struct gk20a *g,
					void *args)
{
	struct nvgpu_mutex mutex;
	struct nvgpu_spinlock lock;
	struct nvgpu_raw_spinlock raw_lock;
	pthread_t worker_thread;
	struct worker_parameters worker_params;
	u64 type = (u64) args;
	int result = UNIT_FAIL;

	worker_params.type = type;

	switch (type) {
	case TYPE_MUTEX:
		nvgpu_mutex_init(&mutex);
		worker_params.mutex = &mutex;
		break;
	case TYPE_SPINLOCK:
		nvgpu_spinlock_init(&lock);
		worker_params.lock = &lock;
		break;
	case TYPE_RAW_SPINLOCK:
		nvgpu_raw_spinlock_init(&raw_lock);
		worker_params.raw_lock = &raw_lock;
		break;
	default:
		unit_return_fail(m, "Unexpected lock type.\n");
		break;
	}

	/*
	 * The semaphore is used to synchronize things when needed between the
	 * current thread and the worker thread
	 * (*_acquire_release_worker)
	 */
	nvgpu_sem_init(&worker_sem);
	test_shared_flag = false;

	/*
	 * Acquire the lock so that the worker thread will block when it tries
	 * to acquire it too.
	 */
	switch (type) {
	case TYPE_MUTEX:
		nvgpu_mutex_acquire(&mutex);
		break;
	case TYPE_SPINLOCK:
		nvgpu_spinlock_acquire(&lock);
		break;
	case TYPE_RAW_SPINLOCK:
		nvgpu_raw_spinlock_acquire(&raw_lock);
		break;
	default:
		break;
	}

	/* Start the thread and wait for its signal */
	pthread_create(&worker_thread, NULL, lock_worker,
		(void *) &worker_params);

	nvgpu_sem_wait(&worker_sem);

	/*
	 * Worker thread is initialized and running. It should be waiting on the
	 * lock, if not (i.e. the flag was updated) then it's a failure
	 */
	if (test_shared_flag) {
		unit_err(m, "Worker thread did not block on lock\n");
		goto cleanup;
	}

	/*
	 * If the flag was not updated, release the lock and check that the
	 * flag was updated this time
	 */
	switch (type) {
	case TYPE_MUTEX:
		nvgpu_mutex_release(&mutex);
		break;
	case TYPE_SPINLOCK:
		nvgpu_spinlock_release(&lock);
		break;
	case TYPE_RAW_SPINLOCK:
		nvgpu_raw_spinlock_release(&raw_lock);
		break;
	default:
		break;
	}

	nvgpu_sem_wait(&worker_sem);

	if (!test_shared_flag) {
		unit_err(m, "Lock did not get released in worker thread\n");
		goto cleanup;
	}

	result = UNIT_SUCCESS;

cleanup:
	pthread_join(worker_thread, NULL);
	if (type == TYPE_MUTEX) {
		nvgpu_mutex_destroy(&mutex);
	}
	return result;
}

struct unit_module_test interface_lock_tests[] = {
	UNIT_TEST(mutex_init, test_mutex_init, NULL, 0),
	UNIT_TEST(mutex_acquire_release, test_lock_acquire_release,
		(u64 *) 0, 0),
	UNIT_TEST(spinlock_acquire_release, test_lock_acquire_release,
		(u64 *) 1, 0),
	UNIT_TEST(raw_spinlock_acquire_release, test_lock_acquire_release,
		(u64 *) 2, 0),
	UNIT_TEST(mutex_tryacquire, test_mutex_tryacquire, NULL, 0),
};

UNIT_MODULE(interface_lock, interface_lock_tests, UNIT_PRIO_NVGPU_TEST);
