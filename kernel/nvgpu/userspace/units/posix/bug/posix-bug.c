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
#include <stdint.h>
#include <setjmp.h>

#include <unit/io.h>
#include <unit/unit.h>
#include <nvgpu/bug.h>

#include <nvgpu/types.h>

#include "posix-bug.h"

/*
 * Simple wrapper function to call BUG() or not. It was not strictly necessary
 * to wrap the call to BUG() in a function but it better ressembles the way
 * EXPECT_BUG is to be used in unit tests.
 */
static void bug_caller(struct unit_module *m, bool call)
{
	if (call == true) {
		unit_info(m, "Calling BUG()\n");
		BUG();
	} else {
		unit_info(m, "Not calling BUG()\n");
	}
}

/*
 * Simple wrapper function to call BUG_ON() with condition.
 */
static void bug_on_caller(struct unit_module *m, bool cond)
{
	BUG_ON(cond);
}
/*
 * Test to ensure the EXPECT_BUG construct works as intended by making sure it
 * behaves properly when BUG is called or not.
 * In the event that EXPECT_BUG is completely broken, the call to BUG() would
 * cause the unit to crash and report a failure correctly.
 */
int test_expect_bug(struct unit_module *m,
				  struct gk20a *g, void *args)
{

	/* Make sure calls to BUG() are caught as intended. */
	if (!EXPECT_BUG(bug_caller(m, true))) {
		unit_err(m, "BUG() was not called but it was expected.\n");
		return UNIT_FAIL;
	} else {
		unit_info(m, "BUG() was called as expected\n");
	}

	/* Make sure there are no false positives when BUG is not called. */
	if (!EXPECT_BUG(bug_caller(m, false))) {
		unit_info(m, "BUG() was not called as expected.\n");
	} else {
		unit_err(m, "BUG() was called but it was not expected.\n");
		return UNIT_FAIL;
	}

	if (!EXPECT_BUG(bug_on_caller(m, true))) {
		unit_err(m, "BUG_ON expected to invoke BUG()\n");
		return UNIT_FAIL;
	} else {
		unit_info(m, "BUG_ON invoked BUG() as expected\n");
	}

	if (!EXPECT_BUG(bug_on_caller(m, false))) {
		unit_info(m, "BUG_ON() skipped BUG invocation as expected\n");
	} else {
		unit_err(m, "BUG_ON invoked BUG but it was not expected()\n");
		return UNIT_FAIL;
	}

	return UNIT_SUCCESS;
}

static bool cb_called = false;

static void bug_cb(void *arg)
{
	jmp_buf *jmp_handler = arg;

	cb_called = true;
	longjmp(*jmp_handler, 1);
}

static bool other_cb_called = false;

static void other_bug_cb(void *arg)
{
	other_cb_called = true;
}

int test_bug_cb(struct unit_module *m,
		struct gk20a *g, void *args)
{
	struct nvgpu_bug_cb callback = {0};
	struct nvgpu_bug_cb other_callback = {0};
	jmp_buf handler;


	/* two callbacks */

	callback.cb = bug_cb;
	callback.arg = &handler;

	other_callback.cb = other_bug_cb;
	other_callback.arg = NULL;

	nvgpu_bug_register_cb(&other_callback);
	nvgpu_bug_register_cb(&callback);
	if (setjmp(handler) == 0) {
		BUG();
	}

	if (!other_cb_called || !cb_called) {
		unit_err(m, "BUG() callback was not called.\n");
		return UNIT_FAIL;
	}

	/* one callback */
	other_cb_called = false;
	nvgpu_bug_register_cb(&other_callback);
	nvgpu_bug_register_cb(&callback);
	nvgpu_bug_unregister_cb(&other_callback);

	if (setjmp(handler) == 0) {
		BUG();
	}

	if (other_cb_called) {
		unit_err(m, "callback unregistration failed.\n");
		return UNIT_FAIL;
	}

	return UNIT_SUCCESS;
}

int test_warn_msg(struct unit_module *m,
		struct gk20a *g, void *args)
{
	bool ret;

	ret = nvgpu_posix_warn(__func__, __LINE__, 0, "");
	if (ret != 0) {
		unit_return_fail(m, "nvgpu_posix_warn failed for cond 0\n");
	}

	ret = nvgpu_posix_warn(__func__, __LINE__, 1, "");
	if (ret != 1) {
		unit_return_fail(m, "nvgpu_posix_warn failed for cond 1\n");
	}

	return UNIT_SUCCESS;
}

struct unit_module_test posix_bug_tests[] = {
	UNIT_TEST(expect_bug, test_expect_bug, NULL, 0),
	UNIT_TEST(bug_cb, test_bug_cb, NULL, 0),
	UNIT_TEST(warn_msg, test_warn_msg, NULL, 0),
};

UNIT_MODULE(posix_bug, posix_bug_tests, UNIT_PRIO_POSIX_TEST);
