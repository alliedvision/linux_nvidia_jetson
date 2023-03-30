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
#include <pthread.h>

#include <unit/io.h>
#include <unit/unit.h>

#include "posix-ossched.h"

int test_current_pid(struct unit_module *m,
			struct gk20a *g, void *args)
{
	int pid;
	int nvgpu_pid;

	pid = (int) getpid();
	nvgpu_pid = nvgpu_current_pid(g);

	if (nvgpu_pid != pid) {
		unit_return_fail(m, "PID mismatch %d %d\n", pid, nvgpu_pid);
	}

	return UNIT_SUCCESS;
}

int test_current_tid(struct unit_module *m,
			struct gk20a *g, void *args)
{
	int tid;
	int nvgpu_tid;

	tid = (int) pthread_self();
	nvgpu_tid = nvgpu_current_tid(g);

	if (nvgpu_tid != tid) {
		unit_return_fail(m, "TID mismatch %d %d\n", tid, nvgpu_tid);
	}

	return UNIT_SUCCESS;
}

int test_print_current(struct unit_module *m,
			struct gk20a *g, void *args)
{
	nvgpu_print_current(g, NULL, NVGPU_INFO);
	nvgpu_print_current(g, NULL, NVGPU_DEBUG);
	nvgpu_print_current(g, NULL, NVGPU_WARNING);
	nvgpu_print_current(g, NULL, NVGPU_ERROR);
	nvgpu_print_current(g, NULL, 10);

	return UNIT_SUCCESS;
}
struct unit_module_test posix_ossched_tests[] = {
	UNIT_TEST(current_pid,    test_current_pid, NULL, 0),
	UNIT_TEST(current_tid,    test_current_tid, NULL, 0),
	UNIT_TEST(print_current,  test_print_current, NULL, 0),
};

UNIT_MODULE(posix_ossched, posix_ossched_tests, UNIT_PRIO_POSIX_TEST);
