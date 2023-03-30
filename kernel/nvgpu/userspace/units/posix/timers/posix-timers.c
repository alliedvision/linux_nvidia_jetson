/*
 * Copyright (c) 2019-2021, NVIDIA CORPORATION.  All rights reserved.
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

#include <unit/io.h>
#include <unit/unit.h>

#include <nvgpu/timers.h>
#include <nvgpu/posix/posix-fault-injection.h>
#include "posix-timers.h"

struct test_timer_args {
	bool counter_timer;
};

static struct test_timer_args init_args = {
	.counter_timer = true,
};

#define TEST_TIMER_COUNT	10

/* The value should be kept below 999 since it is used to calculate
 * the duration paramater to usleep.  This will ensure that the duration
 * value passed to usleep is less than 1000000.
 */
#define TEST_TIMER_DURATION	10

static struct nvgpu_timeout test_timeout;

int test_timer_init(struct unit_module *m,
			      struct gk20a *g, void *args)
{
	int ret;
	unsigned int duration;
	unsigned long flags;
	struct test_timer_args *test_args = (struct test_timer_args *)args;

	if (test_args->counter_timer == true) {
		duration = TEST_TIMER_COUNT;
		flags = NVGPU_TIMER_RETRY_TIMER;
	} else {
		duration = TEST_TIMER_DURATION;
		flags = NVGPU_TIMER_CPU_TIMER;
	}

	ret = nvgpu_timeout_init_flags(g, &test_timeout,
			duration,
			flags);

	if (ret != 0) {
		unit_return_fail(m, "Timer init failed %d\n", ret);
	}

	if (test_timeout.g != g) {
		unit_return_fail(m, "Timer g struct mismatch %d\n", ret);
	}

	if (test_timeout.flags != flags) {
		unit_return_fail(m, "Timer flags mismatch %d\n", ret);
	}

	return UNIT_SUCCESS;
}

int test_timer_init_err(struct unit_module *m,
			      struct gk20a *g, void *args)
{
	int ret, i;

	for (i = 0; i < 12; i++) {
		memset(&test_timeout, 0, sizeof(struct nvgpu_timeout));
		/* nvgpu_tiemout_init accepts only BIT(0), BIT(8), and BIT(9) as
		 * valid flag bits.  So ret should be EINVAL */
		ret = nvgpu_timeout_init_flags(g, &test_timeout, 10, (1 << i));

		if ((i == 0) || (i == 8) || (i == 9)) {
			if (ret != 0) {
				unit_return_fail(m,
					"Timer init failed %d\n",
					ret);
			}
		} else {
			if (ret != -EINVAL) {
				unit_return_fail(m,
					"Timer init with invalid flag %d\n",
					ret);
			}
		}
	}

	/* BIT(0), BIT(8) and BIT(9) set.  Return value should be 0 */
	ret = nvgpu_timeout_init_flags(g, &test_timeout, 10, 0x301);
	if (ret != 0) {
		unit_return_fail(m,"Timer init failed with flag 0x301\n");
	}

	/* BIT(8) and BIT(9) set.  Return value should be 0 */
	ret = nvgpu_timeout_init_flags(g, &test_timeout, 10, 0x300);
	if (ret != 0) {
		unit_return_fail(m,"Timer init failed with flag 0x300\n");
	}

	/* BIT(0) and BIT(8) set.  Return value should be 0 */
	ret = nvgpu_timeout_init_flags(g, &test_timeout, 10, 0x101);
	if (ret != 0) {
		unit_return_fail(m,"Timer init failed with flag 0x101\n");
	}

	/* BIT(0) and BIT(9) set.  Return value should be 0 */
	ret = nvgpu_timeout_init_flags(g, &test_timeout, 10, 0x201);
	if (ret != 0) {
		unit_return_fail(m,"Timer init failed with flag 0x201\n");
	}

	/* BIT(0), BIT(7) and BIT(9) set.  Return value should be -EINVAL */
	ret = nvgpu_timeout_init_flags(g, &test_timeout, 10, 0x281);
	if (ret != -EINVAL) {
		unit_return_fail(m,"Timer init failed with flag 0x281\n");
	}

	/* BIT(5), BIT(7) and BIT(9) set.  Return value should be -EINVAL */
	ret = nvgpu_timeout_init_flags(g, &test_timeout, 10, 0x2A0);
	if (ret != -EINVAL) {
		unit_return_fail(m,"Timer init failed with flag 0x2A0\n");
	}

	/* BIT(1), BIT(2) and BIT(3) set.  Return value should be -EINVAL */
	ret = nvgpu_timeout_init_flags(g, &test_timeout, 10, 0x00E);
	if (ret != -EINVAL) {
		unit_return_fail(m,"Timer init failed with flag 0x00E\n");
	}

	/* BIT(1) to BIT(7) set.  Return value should be -EINVAL */
	ret = nvgpu_timeout_init_flags(g, &test_timeout, 10, 0x07E);
	if (ret != -EINVAL) {
		unit_return_fail(m,"Timer init failed with flag 0x07E\n");
	}

	/* All bits set.  Return value should be -EINVAL */
	ret = nvgpu_timeout_init_flags(g, &test_timeout, 10, 0xFFFFFFFFFFFFFFFF);
	if (ret != -EINVAL) {
		unit_return_fail(m,"Timer init failed with flag all 1s\n");
	}

	return UNIT_SUCCESS;
}

int test_timer_counter(struct unit_module *m,
			      struct gk20a *g, void *args)
{
	memset(&test_timeout, 0, sizeof(struct nvgpu_timeout));

	nvgpu_timeout_init_retry(g, &test_timeout, TEST_TIMER_COUNT);

	do {
		usleep(1);
	} while (nvgpu_timeout_expired(&test_timeout) == 0);

	if (!nvgpu_timeout_peek_expired(&test_timeout)) {
		unit_return_fail(m, "Counter mismatch %d\n",
				test_timeout.retries.attempted);
	}

	return UNIT_SUCCESS;
}

int test_timer_duration(struct unit_module *m,
			      struct gk20a *g, void *args)
{
	int ret;

	memset(&test_timeout, 0, sizeof(struct nvgpu_timeout));

	nvgpu_timeout_init_cpu_timer(g, &test_timeout, TEST_TIMER_DURATION);

	/*
	 * Timer should not be expired.
	 * However, test execution may not be atomic and might get preempted.
	 * In that scenario, the return value might not be zero.
	 * Reading timer value also takes many cycles, hence it is difficult
	 * to confirm if timer timedout before set timeout value.
	 * So, here we print an error message if return value is not zero.
	 */
	ret = nvgpu_timeout_expired(&test_timeout);
	if (ret != 0) {
		unit_err(m,
			"Duration timer expired when not expected %d\n", ret);
	}

	/* Sleep for TEST_TIMER_DURATION */
	usleep((TEST_TIMER_DURATION * 1000));

	do {
		usleep(10);
		ret = nvgpu_timeout_expired(&test_timeout);

	} while (ret == 0);

	if (ret != -ETIMEDOUT) {
		unit_return_fail(m, "Duration timer not expired %d\n", ret);
	}

	if (!nvgpu_timeout_peek_expired(&test_timeout)) {
		unit_return_fail(m, "Duration failure\n");
	}

	return UNIT_SUCCESS;
}

int test_timer_fault_injection(struct unit_module *m,
			      struct gk20a *g, void *args)
{
	int ret;
	struct nvgpu_posix_fault_inj *timers_fi =
					nvgpu_timers_get_fault_injection();

	memset(&test_timeout, 0, sizeof(struct nvgpu_timeout));

	ret = nvgpu_timeout_init_flags(g, &test_timeout,
			TEST_TIMER_DURATION,
			NVGPU_TIMER_CPU_TIMER);

	if (ret != 0) {
		unit_return_fail(m, "Timer init failed %d\n", ret);
	}

	nvgpu_posix_enable_fault_injection(timers_fi, true, 1);

	/* Timer should not be expired */
	ret = nvgpu_timeout_expired(&test_timeout);
	if (ret != 0) {
		unit_return_fail(m,
			"Fault injected timer expired when not expected %d\n",
			ret);
	}

	/* Timer should be expired */
	ret = nvgpu_timeout_expired(&test_timeout);
	if (ret != -ETIMEDOUT) {
		unit_return_fail(m,
			"Fault injected timer expired when not expected %d\n",
			ret);
	}

	nvgpu_posix_enable_fault_injection(timers_fi, false, 0);

	/* Sleep for TEST_TIMER_DURATION */
	usleep((TEST_TIMER_DURATION * 1000));

	do {
		usleep(10);
		ret = nvgpu_timeout_expired(&test_timeout);

	} while (ret == 0);

	if (ret != -ETIMEDOUT) {
		unit_return_fail(m, "Fault injected timer not expired %d\n",
			ret);
	}

	return UNIT_SUCCESS;
}

int test_timer_delay(struct unit_module *m,
			      struct gk20a *g, void *args)
{
	signed long ts_before, ts_after, delay;

	ts_before = nvgpu_current_time_us();
	nvgpu_udelay(5000);
	ts_after = nvgpu_current_time_us();

	delay = ts_after - ts_before;
	delay /= 1000;

	if (delay < 5) {
		unit_return_fail(m,
			"Delay Duration incorrect\n");
	}

	ts_before = nvgpu_current_time_us();
	nvgpu_usleep_range(5000, 10000);
	ts_after = nvgpu_current_time_us();

	delay = ts_after - ts_before;
	delay /= 1000;

	if (delay < 5) {
		unit_return_fail(m,
			"Delay Duration incorrect\n");
	}

	return UNIT_SUCCESS;
}

int test_timer_msleep(struct unit_module *m,
			      struct gk20a *g, void *args)
{
	signed long ts_before, ts_after, delay;

	delay = 0;

	ts_before = nvgpu_current_time_ms();
	nvgpu_msleep(5);
	ts_after = nvgpu_current_time_ms();

	delay = ts_after - ts_before;

	if (delay < 5) {
		unit_return_fail(m, "Sleep Duration incorrect\n");
	}

	return UNIT_SUCCESS;
}

#ifdef CONFIG_NVGPU_NON_FUSA
int test_timer_hrtimestamp(struct unit_module *m,
			      struct gk20a *g, void *args)
{
	unsigned long cycles_read, cycles_bkp;
	int i;

	cycles_read = 0;
	cycles_bkp = 0;

	for (i = 0; i < 50; i++) {
		cycles_read = nvgpu_hr_timestamp();

		if (cycles_read < cycles_bkp) {
			unit_return_fail(m,
				"HR cycle value error %ld < %ld\n",
				cycles_read, cycles_bkp);
		}

		cycles_bkp = cycles_read;
		usleep(1);
	}

	return UNIT_SUCCESS;
}
#endif

int test_timer_compare(struct unit_module *m,
			      struct gk20a *g, void *args)
{
	int i;
	signed long time_ms, time_ns;

	i = 0;
	time_ms = 0;
	time_ns = 0;

	while (i < 10) {
		time_ms = nvgpu_current_time_ms();
		time_ns = nvgpu_current_time_ns();

		time_ns /= 1000000;

		if (time_ns < time_ms) {
			unit_return_fail(m,
				"Err, ms and ns mismatch\n");
		}
		i++;
		usleep(1000);
	}

	return UNIT_SUCCESS;
}

struct unit_module_test posix_timers_tests[] = {
	UNIT_TEST(init,      test_timer_init, &init_args, 0),
	UNIT_TEST(init_err,  test_timer_init_err, NULL, 0),
	UNIT_TEST(counter,   test_timer_counter, NULL, 0),
	UNIT_TEST(duration,  test_timer_duration, NULL, 0),
	UNIT_TEST(fault_injection,  test_timer_fault_injection, NULL, 0),
	UNIT_TEST(delay,     test_timer_delay, NULL, 0),
	UNIT_TEST(msleep,    test_timer_msleep, NULL, 0),
#ifdef CONFIG_NVGPU_NON_FUSA
	UNIT_TEST(hr_cycles, test_timer_hrtimestamp, NULL, 0),
#endif
	UNIT_TEST(compare,   test_timer_compare, NULL, 0),
};

UNIT_MODULE(posix_timers, posix_timers_tests, UNIT_PRIO_POSIX_TEST);
