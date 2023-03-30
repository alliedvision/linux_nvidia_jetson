/*
 * Copyright (c) 2019, NVIDIA CORPORATION.  All rights reserved.
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

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>

#include <unit/io.h>
#include <unit/unit.h>

#include "posix-rwsem.h"

struct unit_test_rwsem_data {
	unsigned int read_pattern;
	unsigned int write_pattern;
	bool read1_locked;
	bool read2_locked;
	bool write1_locked;
	bool write2_locked;
	bool wrlock_err;
	bool wrpattern_err;
	bool write_read_test;
	bool read_lock_fail;
};

static struct unit_test_rwsem_data test_data;
static struct nvgpu_rwsem test_rwsem;

static void *test_rwsem_read1_thread(void *args)
{
	int ret;
	struct unit_test_rwsem_data *data;

	data = (struct unit_test_rwsem_data *)args;

	if (data->write_read_test == true) {
		while (data->write1_locked == false) {
			usleep(1);
		}

		ret = pthread_rwlock_tryrdlock(&test_rwsem.rw_sem);
		if (ret != 0) {
			data->read_lock_fail = true;
			usleep(2);
		}
	}

	nvgpu_rwsem_down_read(&test_rwsem);

	data->read1_locked = true;

	while (data->read2_locked == false) {
		if (data->write1_locked) {
			if (data->write_pattern != 0xCCCCCCCC) {
				data->wrpattern_err = true;
			}
			break;
		}
		usleep(1);
	}

	nvgpu_rwsem_up_read(&test_rwsem);

	return NULL;
}

static void *test_rwsem_read2_thread(void *args)
{
	struct unit_test_rwsem_data *data;

	data = (struct unit_test_rwsem_data *)args;

	while (data->read1_locked == false) {
		usleep(1);
	}

	nvgpu_rwsem_down_read(&test_rwsem);

	data->read2_locked = true;

	nvgpu_rwsem_up_read(&test_rwsem);

	return NULL;
}

static void *test_rwsem_write1_thread(void *args)
{
	struct unit_test_rwsem_data *data;

	data = (struct unit_test_rwsem_data *)args;

	nvgpu_rwsem_down_write(&test_rwsem);

	data->write_pattern = 0xCCCCCCCC;
	data->write1_locked = true;

	usleep(5);

	if (data->write_read_test == true) {
		while (data->read_lock_fail == false) {
			usleep(1);
		}
	}

	if ((data->write2_locked == true) || (data->read1_locked == true)) {
		data->wrlock_err = true;
	}

	nvgpu_rwsem_up_write(&test_rwsem);

	return NULL;
}

static void *test_rwsem_write2_thread(void *args)
{
	struct unit_test_rwsem_data *data;

	data = (struct unit_test_rwsem_data *)args;

	while (data->write1_locked == false) {
		usleep(1);
	}

	nvgpu_rwsem_down_write(&test_rwsem);

	data->write2_locked = true;

	if (data->write_pattern != 0xCCCCCCCC) {
		data->wrpattern_err = true;
	}

	data->write_pattern = 0xDDDDDDDD;

	nvgpu_rwsem_up_write(&test_rwsem);

	return NULL;
}

int test_rwsem_init(struct unit_module *m, struct gk20a *g, void *args)
{
	nvgpu_rwsem_init(&test_rwsem);

	usleep(1);

	pthread_rwlock_destroy(&test_rwsem.rw_sem);

	return UNIT_SUCCESS;
}

int test_rwsem_read(struct unit_module *m, struct gk20a *g, void *args)
{
	int ret;
	pthread_t thread_read1, thread_read2;

	(void) memset(&test_data, 0, sizeof(struct unit_test_rwsem_data));

	nvgpu_rwsem_init(&test_rwsem);

	ret = pthread_create(&thread_read1, NULL,
			&test_rwsem_read1_thread, &test_data);
	if (ret != 0) {
		pthread_rwlock_destroy(&test_rwsem.rw_sem);
		unit_return_fail(m, "Read1 thread creation failed\n");
	}

	ret = pthread_create(&thread_read2, NULL,
			&test_rwsem_read2_thread, &test_data);
	if (ret != 0) {
		pthread_cancel(thread_read1);
		pthread_join(thread_read1, NULL);
		pthread_rwlock_destroy(&test_rwsem.rw_sem);
		unit_return_fail(m, "Read2 thread creation failed\n");
	}

	pthread_join(thread_read1, NULL);
	pthread_join(thread_read2, NULL);

	pthread_rwlock_destroy(&test_rwsem.rw_sem);

	return UNIT_SUCCESS;
}

int test_rwsem_write(struct unit_module *m, struct gk20a *g, void *args)
{
	int ret;
	pthread_t thread_write1, thread_write2;

	(void) memset(&test_data, 0, sizeof(struct unit_test_rwsem_data));

	nvgpu_rwsem_init(&test_rwsem);

	test_data.write_pattern = 0xABABABAB;

	ret = pthread_create(&thread_write1, NULL,
			&test_rwsem_write1_thread, &test_data);
	if (ret != 0) {
		pthread_rwlock_destroy(&test_rwsem.rw_sem);
		unit_return_fail(m, "Write1 thread creation failed\n");
	}

	ret = pthread_create(&thread_write2, NULL,
			&test_rwsem_write2_thread, &test_data);
	if (ret != 0) {
		pthread_cancel(thread_write1);
		pthread_join(thread_write1, NULL);
		pthread_rwlock_destroy(&test_rwsem.rw_sem);
		unit_return_fail(m, "Write2 thread creation failed\n");
	}

	pthread_join(thread_write1, NULL);
	pthread_join(thread_write2, NULL);

	if (test_data.wrpattern_err == true) {
		pthread_rwlock_destroy(&test_rwsem.rw_sem);
		unit_return_fail(m, "Write pattern from write1 mismatch\n");
	} else if (test_data.wrlock_err == true) {
		pthread_rwlock_destroy(&test_rwsem.rw_sem);
		unit_return_fail(m, "Lock error observed by write1 thread\n");
	} else if (test_data.write_pattern != 0xDDDDDDDD) {
		pthread_rwlock_destroy(&test_rwsem.rw_sem);
		unit_return_fail(m, "Write pattern from write2 mismatch\n");
	}

	pthread_rwlock_destroy(&test_rwsem.rw_sem);

	return UNIT_SUCCESS;
}

int test_rwsem_write_read(struct unit_module *m, struct gk20a *g, void *args)
{
	int ret;
	pthread_t thread_write, thread_read;

	(void) memset(&test_data, 0, sizeof(struct unit_test_rwsem_data));

	nvgpu_rwsem_init(&test_rwsem);

	test_data.write_pattern = 0xABABABAB;
	test_data.write_read_test = true;

	ret = pthread_create(&thread_write, NULL,
			&test_rwsem_write1_thread, &test_data);
	if (ret != 0) {
		pthread_rwlock_destroy(&test_rwsem.rw_sem);
		unit_return_fail(m, "Write thread creation failed\n");
	}

	ret = pthread_create(&thread_read, NULL,
			&test_rwsem_read1_thread, &test_data);
	if (ret != 0) {
		pthread_rwlock_destroy(&test_rwsem.rw_sem);
		pthread_cancel(thread_write);
		pthread_join(thread_write, NULL);
		unit_return_fail(m, "Read thread creation failed\n");
	}

	pthread_join(thread_write, NULL);
	pthread_join(thread_read, NULL);

	if (test_data.wrpattern_err == true) {
		pthread_rwlock_destroy(&test_rwsem.rw_sem);
		unit_return_fail(m, "Write pattern mismatch\n");
	} else if (test_data.wrlock_err == true) {
		pthread_rwlock_destroy(&test_rwsem.rw_sem);
		unit_return_fail(m, "Lock error observed by write thread\n");
	}

	pthread_rwlock_destroy(&test_rwsem.rw_sem);

	return UNIT_SUCCESS;
}

struct unit_module_test posix_rwsem_tests[] = {
	UNIT_TEST(init,   test_rwsem_init, NULL, 0),
	UNIT_TEST(read,   test_rwsem_read, NULL, 0),
	UNIT_TEST(write,   test_rwsem_write, NULL, 0),
	UNIT_TEST(write_read,   test_rwsem_write_read, NULL, 0),
};

UNIT_MODULE(posix_rwsem, posix_rwsem_tests, UNIT_PRIO_POSIX_TEST);
