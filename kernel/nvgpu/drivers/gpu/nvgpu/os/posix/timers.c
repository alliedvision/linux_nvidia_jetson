/*
 * Copyright (c) 2018-2022, NVIDIA CORPORATION.  All rights reserved.
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

#include <sys/time.h>
#include <time.h>

#include <nvgpu/bug.h>
#include <nvgpu/log.h>
#include <nvgpu/timers.h>
#include <nvgpu/soc.h>

#define USEC_PER_MSEC   1000
#define NSEC_PER_USEC   1000
#define NSEC_PER_MSEC   1000000
#define NSEC_PER_SEC    1000000000

#ifdef NVGPU_UNITTEST_FAULT_INJECTION_ENABLEMENT
#include <nvgpu/posix/posix-fault-injection.h>

struct nvgpu_posix_fault_inj *nvgpu_timers_get_fault_injection(void)
{
	struct nvgpu_posix_fault_inj_container *c =
			nvgpu_posix_fault_injection_get_container();

	return &c->timers_fi;
}

int nvgpu_timeout_expired_fault_injection(void)
{
	bool count_set = nvgpu_posix_is_fault_injection_cntr_set(
				nvgpu_timers_get_fault_injection());

	bool fault_enabled = nvgpu_posix_fault_injection_handle_call(
				nvgpu_timers_get_fault_injection());

	if (count_set) {
		return 0;
	}
	if (fault_enabled) {
		return -ETIMEDOUT;
	}
	return -1;
}
#endif /* NVGPU_UNITTEST_FAULT_INJECTION_ENABLEMENT */

s64 nvgpu_current_time_us(void)
{
	struct timeval now;
	s64 time_now;
	int ret;

	ret = gettimeofday(&now, NULL);
	if (ret != 0) {
		BUG();
	}

	time_now = nvgpu_safe_mult_s64((s64)now.tv_sec, (s64)NSEC_PER_MSEC);
	time_now = nvgpu_safe_add_s64(time_now, (s64)now.tv_usec);

	return time_now;
}

#ifdef __NVGPU_POSIX__
void nvgpu_delay_usecs(unsigned int usecs)
{
	(void)usecs;
}

#ifdef CONFIG_NVGPU_NON_FUSA
u64 nvgpu_us_counter(void)
{
	return (u64)nvgpu_current_time_us();
}

u64 nvgpu_get_cycles(void)
{
	return (u64)nvgpu_current_time_us();
}
#endif
#endif

static s64 get_time_ns(void)
{
	struct timespec ts;
	s64 t_ns;
	int ret;

	ret = clock_gettime(CLOCK_MONOTONIC, &ts);
	if (ret != 0) {
		BUG();
	}

	t_ns = nvgpu_safe_mult_s64(ts.tv_sec, NSEC_PER_SEC);
	t_ns = nvgpu_safe_add_s64(t_ns, ts.tv_nsec);

	return t_ns;
}

/*
 * Returns true if a > b;
 */
static bool time_after(s64 a, s64 b)
{
	return (nvgpu_safe_sub_s64(a, b) > 0);
}

int nvgpu_timeout_init_flags(struct gk20a *g, struct nvgpu_timeout *timeout,
		       u32 duration, unsigned long flags)
{
	s64 duration_ns;

#ifdef NVGPU_UNITTEST_FAULT_INJECTION_ENABLEMENT
	if (nvgpu_posix_fault_injection_handle_call(
					nvgpu_timers_get_fault_injection())) {
		return -ETIMEDOUT;
	}
#endif

	if ((flags & ~NVGPU_TIMER_FLAG_MASK) != 0U) {
		return -EINVAL;
	}

	(void) memset(timeout, 0, sizeof(*timeout));

	timeout->g = g;
	timeout->flags = (unsigned int)flags;

	if ((flags & NVGPU_TIMER_RETRY_TIMER) != 0U) {
		timeout->retries.max_attempts = duration;
	} else {
		duration_ns = (s64)duration;
		duration_ns = nvgpu_safe_mult_s64(duration_ns, NSEC_PER_MSEC);
		timeout->time_duration = nvgpu_safe_add_s64(nvgpu_current_time_ns(),
								duration_ns);
	}

	return 0;
}

bool nvgpu_timeout_peek_expired(struct nvgpu_timeout *timeout)
{
	if ((timeout->flags & NVGPU_TIMER_RETRY_TIMER) != 0U) {
		return (timeout->retries.attempted >=
			timeout->retries.max_attempts);
	} else {
		return time_after(get_time_ns(), timeout->time_duration);
	}
}

static void nvgpu_usleep(unsigned int usecs)
{
	int ret;
	struct timespec rqtp;
	s64 t_currentns, t_ns;

	t_currentns = get_time_ns();
	t_ns = (s64)usecs;
	t_ns = nvgpu_safe_mult_s64(t_ns, NSEC_PER_USEC);
	t_ns = nvgpu_safe_add_s64(t_ns, t_currentns);

	rqtp.tv_sec = t_ns / NSEC_PER_SEC;
	rqtp.tv_nsec = t_ns % NSEC_PER_SEC;

NVGPU_COV_WHITELIST_BLOCK_BEGIN(deviate, 1, NVGPU_MISRA(Rule, 10_3), "SWE-NVGPU-204-SWSADR.docx")
NVGPU_COV_WHITELIST_BLOCK_BEGIN(deviate, 1, NVGPU_CERT(INT31_C), "SWE-NVGPU-209-SWSADR.docx")
	ret = clock_nanosleep(CLOCK_MONOTONIC, (int)TIMER_ABSTIME, &rqtp, NULL);
NVGPU_COV_WHITELIST_BLOCK_END(NVGPU_CERT(INT31_C))
NVGPU_COV_WHITELIST_BLOCK_END(NVGPU_MISRA(Rule, 10_3))
	if (ret != 0) {
		nvgpu_err(NULL, "Error %d return from clock_nanosleep", ret);
	}
}

void nvgpu_udelay(unsigned int usecs)
{
	if (usecs >= (unsigned int) USEC_PER_MSEC) {
		nvgpu_usleep(usecs);
	} else {
		nvgpu_delay_usecs(usecs);
	}
}

void nvgpu_usleep_range(unsigned int min_us, unsigned int max_us)
{
	(void)max_us;
	nvgpu_udelay(min_us);
}

void nvgpu_msleep(unsigned int msecs)
{
	int ret;
	struct timespec rqtp;
	s64 t_currentns, t_ns;

	t_currentns = get_time_ns();
	t_ns = (s64)msecs;
	t_ns = nvgpu_safe_mult_s64(t_ns, NSEC_PER_MSEC);

	t_ns = nvgpu_safe_add_s64(t_ns, t_currentns);

	rqtp.tv_sec = t_ns / NSEC_PER_SEC;
	rqtp.tv_nsec = t_ns % NSEC_PER_SEC;

NVGPU_COV_WHITELIST_BLOCK_BEGIN(deviate, 1, NVGPU_MISRA(Rule, 10_3), "SWE-NVGPU-204-SWSADR.docx")
NVGPU_COV_WHITELIST_BLOCK_BEGIN(deviate, 1, NVGPU_CERT(INT31_C), "SWE-NVGPU-209-SWSADR.docx")
	ret = clock_nanosleep(CLOCK_MONOTONIC, (int)TIMER_ABSTIME, &rqtp, NULL);
NVGPU_COV_WHITELIST_BLOCK_END(NVGPU_MISRA(Rule, 10_3))
NVGPU_COV_WHITELIST_BLOCK_END(NVGPU_CERT(INT31_C))
	if (ret != 0) {
		nvgpu_err(NULL, "Error %d return from clock_nanosleep", ret);
	}
}

s64 nvgpu_current_time_ms(void)
{
	return (s64)(get_time_ns() / NSEC_PER_MSEC);
}

s64 nvgpu_current_time_ns(void)
{
	return get_time_ns();
}

#ifdef CONFIG_NVGPU_NON_FUSA
u64 nvgpu_hr_timestamp(void)
{
	return nvgpu_get_cycles();
}

u64 nvgpu_hr_timestamp_us(void)
{
	return nvgpu_us_counter();
}
#endif
