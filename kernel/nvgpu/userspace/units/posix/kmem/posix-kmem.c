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

#include <unit/io.h>
#include <unit/unit.h>

#include <nvgpu/kmem.h>
#include "posix-kmem.h"

#define KMEM_TEST_CACHE_SIZE 512
#define KMEM_TEST_ALLOC_SIZE 256
#define KMEM_TEST_CALLOC_COUNT 4

struct nvgpu_kmem_cache {
	struct gk20a *g;
	size_t size;
	char name[128];
};

int test_kmem_cache_create(struct unit_module *m,
				struct gk20a *g, void *args)
{
	struct nvgpu_kmem_cache *test_cache;

	test_cache = nvgpu_kmem_cache_create(g, KMEM_TEST_CACHE_SIZE);

	if (test_cache == NULL) {
		unit_return_fail(m, "Kmem cache create failed\n");
	}

	if (test_cache->size != KMEM_TEST_CACHE_SIZE) {
		nvgpu_kmem_cache_destroy(test_cache);
		unit_return_fail(m, "Kmem cache size mismatch\n");
	} else if (test_cache->g != g) {
		nvgpu_kmem_cache_destroy(test_cache);
		unit_return_fail(m, "Kmem cache g structure mismatch\n");
	}

	nvgpu_kmem_cache_destroy(test_cache);

	return UNIT_SUCCESS;
}

int test_kmem_cache_alloc(struct unit_module *m,
				struct gk20a *g, void *args)
{
	struct nvgpu_kmem_cache *test_cache;
	void *test_ptr = NULL;

	test_cache = nvgpu_kmem_cache_create(g, KMEM_TEST_CACHE_SIZE);

	if (test_cache == NULL) {
		unit_return_fail(m, "Kmem alloc cache create failed\n");
	}

	if (test_cache->size != KMEM_TEST_CACHE_SIZE) {
		nvgpu_kmem_cache_destroy(test_cache);
		unit_return_fail(m, "Kmem alloc cache size mismatch\n");
	} else if (test_cache->g != g) {
		nvgpu_kmem_cache_destroy(test_cache);
		unit_return_fail(m, "Kmem alloc cache g structure mismatch\n");
	}

	test_ptr = nvgpu_kmem_cache_alloc(test_cache);

	if (test_ptr == NULL) {
		nvgpu_kmem_cache_destroy(test_cache);
		unit_return_fail(m, "Kmem cache alloc failed\n");
	}

	nvgpu_kmem_cache_free(test_cache, test_ptr);
	nvgpu_kmem_cache_destroy(test_cache);

	return UNIT_SUCCESS;
}

int test_kmem_kmalloc(struct unit_module *m,
				struct gk20a *g, void *args)
{
	void *test_ptr;

	test_ptr = nvgpu_kmalloc_impl(g, KMEM_TEST_ALLOC_SIZE, NULL);

	if (test_ptr == NULL) {
		unit_return_fail(m, "Kmalloc failed\n");
	}

	nvgpu_kfree_impl(g, test_ptr);

	return UNIT_SUCCESS;
}

int test_kmem_kzalloc(struct unit_module *m,
				struct gk20a *g, void *args)
{
	void *test_ptr;
	char *check_ptr;
	int i;

	test_ptr = nvgpu_kzalloc_impl(g, KMEM_TEST_ALLOC_SIZE, NULL);

	if (test_ptr == NULL) {
		unit_return_fail(m, "Kzalloc failed\n");
	}

	check_ptr = (char *)test_ptr;

	for (i = 0; i < KMEM_TEST_ALLOC_SIZE; i++) {
		if (*check_ptr != 0) {
			nvgpu_kfree_impl(g, (void *)test_ptr);
			unit_return_fail(m, "Non zero memory in Kzalloc\n");
		}

		check_ptr++;
	}

	nvgpu_kfree_impl(g, test_ptr);

	return UNIT_SUCCESS;
}

int test_kmem_kcalloc(struct unit_module *m,
				struct gk20a *g, void *args)
{
	void *test_ptr;
	char *check_ptr;
	int i;

	test_ptr = nvgpu_kcalloc_impl(g, KMEM_TEST_ALLOC_SIZE,
				KMEM_TEST_CALLOC_COUNT, NULL);

	if (test_ptr == NULL) {
		unit_return_fail(m, "Kcalloc failed\n");
	}

	check_ptr = (char *)test_ptr;

	for (i = 0; i < (KMEM_TEST_ALLOC_SIZE * KMEM_TEST_CALLOC_COUNT); i++) {
		if (*check_ptr != 0) {
			nvgpu_kfree_impl(g, (void *)test_ptr);
			unit_return_fail(m, "Non zero memory in Kcalloc\n");
		}
		check_ptr++;
	}

	nvgpu_kfree_impl(g, test_ptr);

	return UNIT_SUCCESS;
}

int test_kmem_virtual_alloc(struct unit_module *m,
				struct gk20a *g, void *args)
{
	void *test_ptr;
	char *check_ptr;
	int i;

	test_ptr = nvgpu_vmalloc_impl(g, KMEM_TEST_ALLOC_SIZE, NULL);

	if (test_ptr == NULL) {
		unit_return_fail(m, "Vmalloc failed\n");
	}

	nvgpu_vfree_impl(g, test_ptr);

	test_ptr = nvgpu_vzalloc_impl(g, KMEM_TEST_ALLOC_SIZE, NULL);

	if (test_ptr == NULL) {
		unit_return_fail(m, "Vzalloc failed\n");
	}

	check_ptr = (char *)test_ptr;

	for (i = 0; i < KMEM_TEST_ALLOC_SIZE; i++) {
		if (*check_ptr != 0) {
			nvgpu_vfree_impl(g, (void *)test_ptr);
			unit_return_fail(m,
				"Non Zero entry in vzalloc memory\n");
		}
		check_ptr++;
	}

	nvgpu_vfree_impl(g, test_ptr);

	return UNIT_SUCCESS;
}

int test_kmem_big_alloc(struct unit_module *m,
				struct gk20a *g, void *args)
{
	void *test_ptr;
	char *check_ptr;
	int i;

	test_ptr = nvgpu_big_alloc_impl(g, KMEM_TEST_ALLOC_SIZE, false);

	if (test_ptr == NULL) {
		unit_return_fail(m, "Big alloc failed\n");
	}

	nvgpu_big_free(g, test_ptr);

	test_ptr = nvgpu_big_alloc_impl(g, KMEM_TEST_ALLOC_SIZE, true);

	if (test_ptr == NULL) {
		unit_return_fail(m, "Big clear alloc failed\n");
	}

	check_ptr = (char *)test_ptr;

	for (i = 0; i < KMEM_TEST_ALLOC_SIZE; i++) {
		if (*check_ptr != 0) {
			nvgpu_big_free(g, (void *)test_ptr);
			unit_return_fail(m,
				"Non Zero entry in big clear alloc memory\n");
		}
		check_ptr++;
	}

	nvgpu_big_free(g, test_ptr);

	return UNIT_SUCCESS;
}

struct unit_module_test posix_kmem_tests[] = {
	UNIT_TEST(cache_create,   test_kmem_cache_create, NULL, 0),
	UNIT_TEST(cache_alloc,    test_kmem_cache_alloc, NULL, 0),
	UNIT_TEST(kmalloc_test,   test_kmem_kmalloc, NULL, 0),
	UNIT_TEST(kzalloc_test,   test_kmem_kzalloc, NULL, 0),
	UNIT_TEST(kcalloc_test,   test_kmem_kcalloc, NULL, 0),
	UNIT_TEST(virtual_alloc,  test_kmem_virtual_alloc, NULL, 0),
	UNIT_TEST(big_alloc,      test_kmem_big_alloc, NULL, 0),
};

UNIT_MODULE(posix_kmem, posix_kmem_tests, UNIT_PRIO_POSIX_TEST);
