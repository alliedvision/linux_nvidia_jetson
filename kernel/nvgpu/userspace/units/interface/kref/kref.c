/*
 * Copyright (c) 2020, NVIDIA CORPORATION.  All rights reserved.
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

#include <unit/io.h>
#include <unit/unit.h>

#include <nvgpu/kref.h>

#include "kref.h"

#define LOOP_COUNT 10

int release_count = 0;

static void test_ref_release(struct nvgpu_ref *ref)
{
	release_count += 1;
}

int test_kref_init(struct unit_module *m, struct gk20a *g, void *args)
{
	int ret = 0;
	struct nvgpu_ref test_ref;

	nvgpu_ref_init(&test_ref);

	ret = nvgpu_atomic_read(&test_ref.refcount);

	if (ret != 1) {
		unit_return_fail(m, "nvgpu_ref_init failure\n");
	}

	return UNIT_SUCCESS;
}

int test_kref_get(struct unit_module *m, struct gk20a *g, void *args)
{
	int ret, loop;
	struct nvgpu_ref test_ref;

	nvgpu_ref_init(&test_ref);

	for (loop = 1; loop < LOOP_COUNT; loop++) {
		nvgpu_ref_get(&test_ref);
	}

	ret = nvgpu_atomic_read(&test_ref.refcount);

	if (ret != LOOP_COUNT) {
		unit_return_fail(m, "nvgpu_ref_get failure %d\n", ret);
	}

	return UNIT_SUCCESS;
}

int test_kref_get_unless(struct unit_module *m, struct gk20a *g, void *args)
{
	int ret;
	struct nvgpu_ref test_ref;

	nvgpu_atomic_set(&test_ref.refcount, 0);
	ret = nvgpu_atomic_read(&test_ref.refcount);
	if (ret != 0)  {
		unit_return_fail(m, "nvgpu_ref set to 0 failure %d\n", ret);
	}

	ret = nvgpu_ref_get_unless_zero(&test_ref);
	if (ret) {
		unit_return_fail(m,
			"nvgpu_ref_get_unless_zero failure %d\n", ret);
	}

	nvgpu_ref_init(&test_ref);
	ret = nvgpu_ref_get_unless_zero(&test_ref);
	if (!ret) {
		unit_return_fail(m,
			"nvgpu_ref_get_unless_zero failure\n");
	}

	return UNIT_SUCCESS;
}

int test_kref_put(struct unit_module *m, struct gk20a *g, void *args)
{
	int ret, loop;
	struct nvgpu_ref test_ref;

	release_count = 0;
	nvgpu_ref_init(&test_ref);

	for (loop = 1; loop < LOOP_COUNT; loop++) {
		nvgpu_ref_get(&test_ref);
	}

	ret = nvgpu_atomic_read(&test_ref.refcount);

	if (ret != LOOP_COUNT) {
		unit_return_fail(m, "nvgpu_ref_get failure %d\n", ret);
	}

	for (loop = 0; loop < LOOP_COUNT; loop++) {
		nvgpu_ref_put(&test_ref, test_ref_release);
	}

	if (!release_count) {
		unit_return_fail(m,
			"release function not invoked\n");
	} else if ((release_count > 1)) {
		unit_return_fail(m,
			"release function invoked more than once\n");
	}

	nvgpu_ref_get(&test_ref);

	nvgpu_ref_put(&test_ref, NULL);

	ret = nvgpu_atomic_read(&test_ref.refcount);
	if (ret) {
		unit_return_fail(m,
			"nvgpu_ref_put with NULL callback failure %d\n", ret);
	}

	release_count = 0;

	return UNIT_SUCCESS;
}

int test_kref_put_return(struct unit_module *m, struct gk20a *g, void *args)
{
	int ret, loop;
	struct nvgpu_ref test_ref;

	release_count = 0;
	nvgpu_ref_init(&test_ref);

	for (loop = 1; loop < LOOP_COUNT; loop++) {
		nvgpu_ref_get(&test_ref);
	}

	ret = nvgpu_atomic_read(&test_ref.refcount);

	if (ret != LOOP_COUNT) {
		unit_return_fail(m, "refcount not updated%d\n", ret);
	}

	for (loop = 0; loop < (LOOP_COUNT - 1); loop++) {
		ret = nvgpu_ref_put_return(&test_ref, test_ref_release);
		if (ret) {
			unit_return_fail(m,
				"nvgpu_ref_put_return failure\n");
		}
	}

	ret = nvgpu_ref_put_return(&test_ref, test_ref_release);
	if (!ret) {
			unit_return_fail(m,
				"nvgpu_ref_put_return failure\n");
	}

	if (!release_count) {
		unit_return_fail(m,
			"release function not invoked\n");
	} else if ((release_count > 1)) {
		unit_return_fail(m,
			"release function invoked more than once\n");
	}

	release_count = 0;

	nvgpu_ref_get(&test_ref);

	ret = nvgpu_ref_put_return(&test_ref, NULL);
	if (!ret) {
		unit_return_fail(m,
			"nvgpu_ref_put_return with NULL callback failure\n");
	}

	return UNIT_SUCCESS;
}

struct unit_module_test interface_kref_tests[] = {
	UNIT_TEST(kref_init,       test_kref_init, NULL, 0),
	UNIT_TEST(kref_get,        test_kref_get, NULL, 0),
	UNIT_TEST(kref_get_unless, test_kref_get_unless, NULL, 0),
	UNIT_TEST(kref_put,        test_kref_put, NULL, 0),
	UNIT_TEST(kref_put_return, test_kref_put_return, NULL, 0),
};

UNIT_MODULE(interface_kref, interface_kref_tests, UNIT_PRIO_NVGPU_TEST);
