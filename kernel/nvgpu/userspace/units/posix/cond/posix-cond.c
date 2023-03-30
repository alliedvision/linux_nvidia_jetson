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

#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

#include <unit/io.h>
#include <unit/unit.h>

#include <nvgpu/cond.h>
#include <nvgpu/thread.h>
#include <nvgpu/timers.h>

#include "posix-cond.h"

#define MISMATCH_ERROR 1000

struct test_cond_args {
        bool use_broadcast;
        bool use_int;
        bool use_wait_int;
        bool use_wait_locked;
        bool use_signal_locked;
        bool use_timedwait;
        bool use_condition;
};

struct unit_test_cond_data {
        int use_broadcast;
        int use_int;
        int use_wait_int;
        int use_wait_locked;
        int use_signal_locked;
        int use_timedwait;
        int use_condition;
};

struct unit_test_cond_data test_data;
struct nvgpu_cond test_cond;

static struct test_cond_args signal_normal = {
        .use_broadcast = false,
        .use_int = false,
        .use_wait_int = false,
        .use_wait_locked = false,
        .use_signal_locked = false,
        .use_timedwait = false,
        .use_condition = false
};

static struct test_cond_args signal_int = {
        .use_broadcast = false,
        .use_int = true,
        .use_wait_int = false,
        .use_wait_locked = false,
        .use_signal_locked = false,
        .use_timedwait = false,
        .use_condition = false
};

static struct test_cond_args signal_locked = {
        .use_broadcast = false,
        .use_int = false,
        .use_wait_int = false,
        .use_wait_locked = false,
        .use_signal_locked = true,
        .use_timedwait = false,
        .use_condition = false
};

static struct test_cond_args broadcast_normal = {
        .use_broadcast = true,
        .use_int = false,
        .use_wait_int = false,
        .use_wait_locked = false,
        .use_signal_locked = false,
        .use_timedwait = false,
        .use_condition = false
};

static struct test_cond_args broadcast_int = {
        .use_broadcast = true,
        .use_int = true,
        .use_wait_int = false,
        .use_wait_locked = false,
        .use_signal_locked = false,
        .use_timedwait = false,
        .use_condition = false
};

static struct test_cond_args broadcast_locked = {
        .use_broadcast = true,
        .use_int = false,
        .use_wait_int = false,
        .use_wait_locked = false,
        .use_signal_locked = true,
        .use_timedwait = false,
        .use_condition = false
};

static struct test_cond_args timed_wait = {
        .use_broadcast = false,
        .use_int = false,
        .use_wait_int = false,
        .use_wait_locked = false,
        .use_signal_locked = false,
        .use_timedwait = true,
        .use_condition = false
};

static struct test_cond_args condition_wait = {
        .use_broadcast = false,
        .use_int = false,
        .use_wait_int = false,
        .use_wait_locked = false,
        .use_signal_locked = false,
        .use_timedwait = false,
        .use_condition = true
};

static struct test_cond_args condition_wait_locked = {
        .use_broadcast = false,
        .use_int = false,
        .use_wait_int = false,
        .use_wait_locked = true,
        .use_signal_locked = false,
        .use_timedwait = false,
        .use_condition = true
};

static struct test_cond_args condition_wait_int = {
        .use_broadcast = false,
        .use_int = false,
        .use_wait_int = true,
        .use_wait_locked = false,
        .use_signal_locked = false,
        .use_timedwait = false,
        .use_condition = true
};

bool read_wait = 0;
bool bcst_read_wait = 0;
int read_status = 0;
int bcst_read_status = 0;

char test_code[4];

static int test_cond_write_thread(void *args)
{
	int i;
	struct unit_test_cond_data *data;

	data = (struct unit_test_cond_data *)args;

	while (!read_wait) {
		usleep(10);
	}

	if (data->use_broadcast) {
		while (!bcst_read_wait) {
			usleep(10);
		}
	}

	for (i = 0; i < 4; i++) {
		test_code[i] = 0x55;
	}

	usleep(50);

	read_wait = false;

	if (data->use_broadcast) {
		bcst_read_wait = false;

		if (data->use_int) {
			nvgpu_cond_broadcast_interruptible(&test_cond);
		} else {
			if (data->use_signal_locked) {
				nvgpu_cond_lock(&test_cond);
				nvgpu_cond_broadcast_locked(&test_cond);
				nvgpu_cond_unlock(&test_cond);
			} else {
				nvgpu_cond_broadcast(&test_cond);
			}
		}
	} else {
		if (data->use_int) {
			nvgpu_cond_signal_interruptible(&test_cond);
		} else {
			if (data->use_signal_locked) {
				nvgpu_cond_lock(&test_cond);
				nvgpu_cond_signal_locked(&test_cond);
				nvgpu_cond_unlock(&test_cond);
			} else {
				nvgpu_cond_signal(&test_cond);
			}
		}
	}

	return 0;
}

static int test_cond_read_thread(void *args)
{
	int ret;
	int i;
	unsigned int timeout;
	struct unit_test_cond_data *data;

	data = (struct unit_test_cond_data *)args;

	if (data->use_timedwait) {
		timeout = 50;
	} else {
		timeout = NVGPU_COND_WAIT_TIMEOUT_MAX_MS;
	}

	if (data->use_condition) {
		read_wait = true;
		if (data->use_wait_int) {
			ret = NVGPU_COND_WAIT_INTERRUPTIBLE(&test_cond,
				test_code[3] == 0x55, 0);
		} else if (data->use_wait_locked) {
			nvgpu_cond_lock(&test_cond);
			ret = NVGPU_COND_WAIT_LOCKED(&test_cond,
				test_code[3] == 0x55, 0);
			nvgpu_cond_unlock(&test_cond);
		} else {
			ret = NVGPU_COND_WAIT(&test_cond,
				test_code[3] == 0x55, 0);
		}

		if (ret != 0) {
			read_status = ret;
		}
	} else {
		nvgpu_cond_lock(&test_cond);

		read_wait = true;
		ret = nvgpu_cond_timedwait(&test_cond, &timeout);
		if (ret != 0) {
			read_status = ret;
			nvgpu_cond_unlock(&test_cond);
			return -1;
		}

		nvgpu_cond_unlock(&test_cond);

		for (i = 0; i < 4; i++) {
			if (test_code[i] != 0x55) {
				read_status = MISMATCH_ERROR;
				return -1;
			}
		}
	}

	return 0;
}

static int test_cond_bcst_read_thread(void *args)
{
	int ret;
	int i;
	unsigned int timeout;
	struct unit_test_cond_data *data;

	data = (struct unit_test_cond_data *)args;

	if (data->use_timedwait) {
		timeout = 50;
	} else {
		timeout = NVGPU_COND_WAIT_TIMEOUT_MAX_MS;
	}

	nvgpu_cond_lock(&test_cond);

	bcst_read_wait = true;
	ret = nvgpu_cond_timedwait(&test_cond, &timeout);
	if (ret != 0) {
		bcst_read_status = ret;
		nvgpu_cond_unlock(&test_cond);
		return -1;
	}

	nvgpu_cond_unlock(&test_cond);

	for (i = 0; i < 4; i++) {
		if (test_code[i] != 0x55) {
			bcst_read_status = MISMATCH_ERROR;
			return -1;
		}
	}

	return 0;
}

int test_cond_init_destroy(struct unit_module *m,
				struct gk20a *g, void *args)
{
	int ret;

	memset(&test_cond, 0, sizeof(struct nvgpu_cond));

	ret = nvgpu_cond_init(&test_cond);
	if (ret != 0) {
		unit_return_fail(m, "Cond init failed\n");
	}

	if (test_cond.initialized != true) {
		unit_return_fail(m, "Init flag not set\n");
	}

	nvgpu_cond_destroy(&test_cond);

	if (test_cond.initialized != false) {
		unit_return_fail(m, "Cond destroy failed\n");
	}

	return UNIT_SUCCESS;
}

int test_cond_bug(struct unit_module *m,
			struct gk20a *g, void *args)
{
	int ret;

	memset(&test_cond, 0, sizeof(struct nvgpu_cond));

	if(!EXPECT_BUG(nvgpu_cond_signal(NULL))) {
		unit_return_fail(m, "BUG not called for NULL cond var\n");
	}

	if(!EXPECT_BUG(nvgpu_cond_signal(&test_cond))) {
		unit_return_fail(m,
			"BUG not called for uninitialized cond var\n");
	}

	if(!EXPECT_BUG(nvgpu_cond_signal_interruptible(NULL))) {
		unit_return_fail(m, "BUG not called for NULL cond var\n");
	}

	if(!EXPECT_BUG(nvgpu_cond_signal_interruptible(&test_cond))) {
		unit_return_fail(m,
			"BUG not called for uninitialized cond var\n");
	}

	if(!EXPECT_BUG(nvgpu_cond_destroy(NULL))) {
		unit_return_fail(m, "BUG not called for NULL cond var\n");
	}

	if(!EXPECT_BUG(nvgpu_cond_signal_locked(NULL))) {
		unit_return_fail(m, "BUG not called for NULL cond var\n");
	}

	if(!EXPECT_BUG(nvgpu_cond_signal_locked(&test_cond))) {
		unit_return_fail(m,
			"BUG not called for uninitialized cond var\n");
	}

	ret = nvgpu_cond_broadcast(NULL);
	if (ret != -EINVAL) {
		unit_return_fail(m, "NULL cond var not handled\n");
	}

	ret = nvgpu_cond_broadcast(&test_cond);
	if (ret != -EINVAL) {
		unit_return_fail(m, "Uninitialized cond var not handled\n");
	}

	ret = nvgpu_cond_broadcast_interruptible(NULL);
	if (ret != -EINVAL) {
		unit_return_fail(m, "NULL cond var not handled\n");
	}

	ret = nvgpu_cond_broadcast_interruptible(&test_cond);
	if (ret != -EINVAL) {
		unit_return_fail(m, "Uninitialized cond var not handled\n");
	}

	ret = nvgpu_cond_broadcast_locked(&test_cond);
	if (ret != -EINVAL) {
		unit_return_fail(m, "Uninitialized cond var not handled\n");
	}

	return UNIT_SUCCESS;
}

int test_cond_signal(struct unit_module *m,
			struct gk20a *g, void *args)
{
	int ret;
	struct nvgpu_thread thread_write, thread_read, thread_bcst_read;
	struct test_cond_args *test_args = (struct test_cond_args *)args;

	memset(&test_cond, 0, sizeof(struct nvgpu_cond));
	memset(&test_code, 0, sizeof(test_code));
	memset(&test_data, 0, sizeof(struct unit_test_cond_data));

	ret = nvgpu_cond_init(&test_cond);
	if (ret != 0) {
		unit_return_fail(m, "Cond init failed\n");
	}

	test_data.use_broadcast = test_args->use_broadcast;
	test_data.use_int = test_args->use_int;
	test_data.use_wait_int = test_args->use_wait_int;
	test_data.use_wait_locked = test_args->use_wait_locked;
	test_data.use_signal_locked = test_args->use_signal_locked;
	test_data.use_timedwait = test_args->use_timedwait;
	test_data.use_condition = test_args->use_condition;

	read_status = 0;
	bcst_read_status = 0;

	ret = nvgpu_thread_create(&thread_read, (void *)&test_data,
				  &test_cond_read_thread, "");
	if (ret != 0) {
		nvgpu_cond_destroy(&test_cond);
		unit_return_fail(m, "Cond read thread fail\n");
	}

	if (test_args->use_broadcast) {
		ret = nvgpu_thread_create(&thread_bcst_read, &test_data,
				&test_cond_bcst_read_thread, "");
		if (ret != 0) {
			nvgpu_cond_destroy(&test_cond);
			nvgpu_thread_stop(&thread_read);
			nvgpu_thread_join(&thread_read);
			unit_return_fail(m, "Cond bcst read thread fail\n");
		}
	}

	ret = nvgpu_thread_create(&thread_write, &test_data,
					&test_cond_write_thread, "");
	if (ret != 0) {
		nvgpu_cond_destroy(&test_cond);
		nvgpu_thread_stop(&thread_read);
		nvgpu_thread_join(&thread_read);
		if (test_args->use_broadcast) {
			nvgpu_thread_stop(&thread_bcst_read);
			nvgpu_thread_stop(&thread_bcst_read);
		}
		unit_return_fail(m, "Cond write thread fail\n");
	}

	nvgpu_thread_join(&thread_write);
	nvgpu_thread_join(&thread_read);

	if (test_args->use_broadcast) {
		nvgpu_thread_join(&thread_bcst_read);
	}

	if (read_status != 0) {
		nvgpu_cond_destroy(&test_cond);
		unit_return_fail(m, "Cond read status fail %d\n",
					read_status);
	}

	if (test_args->use_broadcast) {
		if (bcst_read_status != 0) {
			nvgpu_cond_destroy(&test_cond);
			unit_return_fail(m,
				"Cond bcst read status fail %d\n",
				bcst_read_status);
		}
	}

	nvgpu_cond_destroy(&test_cond);

	return UNIT_SUCCESS;
}

int test_cond_timeout(struct unit_module *m,
			struct gk20a *g, void *args)
{
	int ret;
	unsigned int timeout;
	signed long ts_before, ts_after, delay;

	memset(&test_cond, 0, sizeof(struct nvgpu_cond));
	timeout = 10;

	ret = nvgpu_cond_init(&test_cond);
	if (ret != 0) {
		unit_return_fail(m, "Cond init failed\n");
	}

	ts_before = nvgpu_current_time_us();
	nvgpu_cond_lock(&test_cond);

	ret = nvgpu_cond_timedwait(&test_cond, &timeout);
	if (ret != ETIMEDOUT) {
		nvgpu_cond_unlock(&test_cond);
		nvgpu_cond_destroy(&test_cond);
		unit_return_fail(m, "Cond timed wait return error %d\n", ret);
	}

	ts_after = nvgpu_current_time_us();
	delay = ts_after - ts_before;
	delay /= 1000;

	if (delay < 10) {
		nvgpu_cond_unlock(&test_cond);
		nvgpu_cond_destroy(&test_cond);
		unit_return_fail(m,
			"Timed wait Duration incorrect\n");
	}

	nvgpu_cond_unlock(&test_cond);
	nvgpu_cond_destroy(&test_cond);

	return UNIT_SUCCESS;
}

struct unit_module_test posix_cond_tests[] = {
	UNIT_TEST(init,                  test_cond_init_destroy, NULL, 0),
	UNIT_TEST(bug_cond,              test_cond_bug, NULL, 0),
	UNIT_TEST(wait_signal,           test_cond_signal, &signal_normal, 0),
	UNIT_TEST(wait_signal_int,       test_cond_signal, &signal_int, 0),
	UNIT_TEST(wait_signal_locked,    test_cond_signal, &signal_locked, 0),
	UNIT_TEST(timedwait_signal,      test_cond_signal, &timed_wait, 0),
	UNIT_TEST(wait_broadcast,        test_cond_signal, &broadcast_normal, 0),
	UNIT_TEST(wait_broadcast_int,    test_cond_signal, &broadcast_int, 0),
	UNIT_TEST(wait_broadcast_locked, test_cond_signal, &broadcast_locked, 0),
	UNIT_TEST(wait_condition,        test_cond_signal, &condition_wait, 0),
	UNIT_TEST(wait_condition_int,    test_cond_signal, &condition_wait_int, 0),
	UNIT_TEST(wait_condition_locked, test_cond_signal, &condition_wait_locked, 0),
	UNIT_TEST(wait_timeout,          test_cond_timeout, NULL, 0),
};

UNIT_MODULE(posix_cond, posix_cond_tests, UNIT_PRIO_POSIX_TEST);
