/*
 * Copyright (c) 2020-2022, NVIDIA CORPORATION.  All rights reserved.
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

#include <nvgpu/log2.h>

#include "posix-log2.h"

int test_ilog2(struct unit_module *m,
		struct gk20a *g, void *args)
{
	unsigned long i, test, ret;

	for (i = 0; i < BITS_PER_LONG; i++) {
		test = 1UL << i;
		ret = nvgpu_ilog2(test);
		if (ret != i) {
			unit_return_fail(m,
				"ilog2 failure %ld\n", test);
		}
	}

	for (i = 1; i < (BITS_PER_LONG - 1); i++) {
		test = 1UL << i;
		test += 1;
		ret = nvgpu_ilog2(test);
		if (ret != i) {
			unit_return_fail(m,
				"ilog2 failure %ld\n", test);
		}
	}

	return UNIT_SUCCESS;
}

int test_roundup_powoftwo(struct unit_module *m,
		struct gk20a *g, void *args)
{
	unsigned long i, test, ret;

	test = 0UL;

	if (!EXPECT_BUG(roundup_pow_of_two(test))) {
		unit_return_fail(m,
			"roundup_pow_of_two did not invoke BUG()\n");
	} else {
		unit_info(m, "BUG invoked as expected for input value 0\n");
	}

	for (i = 0; i < BITS_PER_LONG; i++) {
		test = 1UL << i;
		ret = roundup_pow_of_two(test);
		if (ret != test) {
			unit_return_fail(m,
				"roundup_pow_of_two failure.\n");
		}
	}

	for (i = 0; i < (BITS_PER_LONG - 1); i++) {
		test = 1UL << i;
		test += 1;
		ret = roundup_pow_of_two(test);
		if (ret != (1UL << (i + 1))) {
			unit_return_fail(m,
				"roundup_pow_of_two failure.\n");
		}
	}

	return UNIT_SUCCESS;
}

int test_rounddown_powoftwo(struct unit_module *m,
		struct gk20a *g, void *args)
{
	unsigned long i, test, ret;

	test = 0UL;

	if (!EXPECT_BUG(rounddown_pow_of_two(test))) {
		unit_return_fail(m,
			"rounddown_pow_of_two did not invoke BUG()\n");
	} else {
		unit_info(m, "BUG invoked as expected for input value 0\n");
	}

	for (i = 0; i < BITS_PER_LONG; i++) {
		test = 1UL << i;
		ret = rounddown_pow_of_two(test);
		if (ret != test) {
			unit_return_fail(m,
				"rounddown_pow_of_two failure.\n");
		}
	}

	for (i = 1; i < BITS_PER_LONG; i++) {
		test = 1UL << i;
		test -= 1;
		ret = rounddown_pow_of_two(test);
		if (ret != (1UL << (i - 1))) {
			unit_return_fail(m,
				"rounddown_pow_of_two failure.\n");
		}
	}

	return UNIT_SUCCESS;
}

int test_ispow2(struct unit_module *m,
		struct gk20a *g, void *args)
{
	bool ret;
	unsigned long i, test;

	test = 0UL;

	for (i = 0; i < BITS_PER_LONG; i++) {
		test = 1UL << i;
		ret = is_power_of_2(test);
		if (!ret) {
			unit_return_fail(m,
				"is_power_of_2 failure %ld\n", test);
		}
	}

	for (i = 1; i < (BITS_PER_LONG - 1); i++) {
		test = 1UL << i;
		test += 1;
		ret = is_power_of_2(test);
		if (ret) {
			unit_return_fail(m,
				"is_power_of_2 failure %ld\n", test);
		}
	}

	return UNIT_SUCCESS;
}

struct unit_module_test posix_log2_tests[] = {
	UNIT_TEST(integer_log2,      test_ilog2, NULL, 0),
	UNIT_TEST(roundup_pow2,      test_roundup_powoftwo, NULL, 0),
	UNIT_TEST(rounddown_pow2,    test_rounddown_powoftwo, NULL, 0),
	UNIT_TEST(is_powof2,         test_ispow2, NULL, 0),
};

UNIT_MODULE(posix_log2, posix_log2_tests, UNIT_PRIO_POSIX_TEST);
