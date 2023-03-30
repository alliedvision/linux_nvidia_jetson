/*
 * Copyright (c) 2018-2019, NVIDIA CORPORATION.  All rights reserved.
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
#include <unit/unit.h>
#include <unit/io.h>
#include <nvgpu/kmem.h>
#include <nvgpu/posix/posix-fault-injection.h>
#include "posix-fault-injection-kmem.h"


#define TEST_DEFAULT_CACHE_SIZE 1024
#define TEST_DEFAULT_KMALLOC_SIZE 1024

static struct nvgpu_posix_fault_inj *kmem_fi;

/*
 * Used to make sure fault injection is disabled before running test
 * If already enabled, prints warning and disables
 *
 * Returns false if unable to guarantee fault injection is disabled
 */
static bool verify_fi_disabled(struct unit_module *m)
{
	if (nvgpu_posix_is_fault_injection_triggered(kmem_fi)) {
		unit_info(m, "Unexpected fault injection enabled\n");
	}

	/* force disabled in case it was in "delay" mode */
	nvgpu_posix_enable_fault_injection(kmem_fi, false, 0);
	if (nvgpu_posix_is_fault_injection_triggered(kmem_fi)) {
		unit_err(m, "Unable to disable fault injection\n");
		return false;
	}

	return true;
}

int test_kmem_init(struct unit_module *m,
		   struct gk20a *g, void *__args)
{
	kmem_fi = nvgpu_kmem_get_fault_injection();
	if (kmem_fi == NULL) {
		return UNIT_FAIL;
	} else {
		return UNIT_SUCCESS;
	}
}

int test_kmem_cache_fi_default(struct unit_module *m,
			       struct gk20a *g, void *__args)
{
	struct nvgpu_kmem_cache *kmem_cache;
	int ret = UNIT_SUCCESS;

	if (!verify_fi_disabled(m)) {
		ret = UNIT_FAIL;
		/* no reason to go on */
		goto test_exit;
	}

	/* check default case */
	kmem_cache = nvgpu_kmem_cache_create(g, TEST_DEFAULT_CACHE_SIZE);
	if (kmem_cache == NULL) {
		unit_err(m, "nvgpu_kmem_cache_create returned NULL when fault "
			    "injection disabled\n");
		ret = UNIT_FAIL;
	}
	nvgpu_kmem_cache_destroy(kmem_cache);

test_exit:
	/* disable faults upon exit */
	nvgpu_posix_enable_fault_injection(kmem_fi, false, 0);
	if (nvgpu_posix_is_fault_injection_triggered(kmem_fi)) {
		unit_err(m, "unable to disable fault injection\n");
		ret = UNIT_FAIL;
	}

	if (ret != UNIT_SUCCESS) {
		unit_return_fail(m, "kmem cache fault injection test "
				    "failure\n");
	}

	return ret;
}

int test_kmem_cache_fi_enabled(struct unit_module *m,
			       struct gk20a *g, void *__args)
{
	struct nvgpu_kmem_cache *kmem_cache;
	int ret = UNIT_SUCCESS;

	if (!verify_fi_disabled(m)) {
		ret = UNIT_FAIL;
		/* no reason to go on */
		goto test_exit;
	}

	/* enable faults */
	nvgpu_posix_enable_fault_injection(kmem_fi, true, 0);
	if (!nvgpu_posix_is_fault_injection_triggered(kmem_fi)) {
		unit_err(m, "Unable to enable fault injection\n");
		ret = UNIT_FAIL;
		/* no reason to go on */
		goto test_exit;
	}

	/* create cache and verify error */
	kmem_cache = nvgpu_kmem_cache_create(g, TEST_DEFAULT_CACHE_SIZE);
	if (kmem_cache != NULL) {
		unit_err(m, "nvgpu_kmem_cache_create returned pointer when "
			    "fault injection enabled\n");
		ret = UNIT_FAIL;
	}

test_exit:
	/* disable faults upon exit */
	nvgpu_posix_enable_fault_injection(kmem_fi, false, 0);
	if (nvgpu_posix_is_fault_injection_triggered(kmem_fi)) {
		unit_err(m, "unable to disable fault injection\n");
		ret = UNIT_FAIL;
	}

	if (ret != UNIT_SUCCESS) {
		unit_return_fail(m, "kmem cache fault injection test failure"
				    "\n");
	}

	return ret;
}

int test_kmem_cache_fi_delayed_enable(struct unit_module *m,
				      struct gk20a *g, void *__args)
{
	struct nvgpu_kmem_cache *kmem_cache;
	int *ptr1, *ptr2;
	int ret = UNIT_SUCCESS;

	if (!verify_fi_disabled(m)) {
		ret = UNIT_FAIL;
		/* no reason to go on */
		goto test_exit;
	}

	/* enable faults after 2 calls */
	nvgpu_posix_enable_fault_injection(kmem_fi, true, 2);
	if (nvgpu_posix_is_fault_injection_triggered(kmem_fi)) {
		unit_err(m, "Fault injection errantly enabled too soon\n");
		ret = UNIT_FAIL;
		/* no reason to go on */
		goto test_exit;
	}

	/* first call should pass */
	kmem_cache = nvgpu_kmem_cache_create(g, TEST_DEFAULT_CACHE_SIZE);
	if (kmem_cache == NULL) {
		unit_err(m, "nvgpu_kmem_cache_create returned NULL when fault "
			    "injection disabled\n");
		/* no reason to go on */
		goto test_exit;
		ret = UNIT_FAIL;
	}

	/* second call should pass */
	ptr1 = (int *)nvgpu_kmem_cache_alloc(kmem_cache);
	if (ptr1 == NULL) {
		unit_err(m, "nvgpu_kmem_cache_alloc returned NULL when fault "
			    "injection disabled\n");
		ret = UNIT_FAIL;
	}

	/* third call should fail */
	ptr2 = (int *)nvgpu_kmem_cache_alloc(kmem_cache);
	if (ptr2 != NULL) {
		unit_err(m, "nvgpu_kmem_cache_alloc returned pointer when "
			    "fault injection enabled\n");
		nvgpu_kmem_cache_free(kmem_cache, ptr2);
		ret = UNIT_FAIL;
	}

	/* good housekeeping */
	nvgpu_kmem_cache_free(kmem_cache, ptr1);
	nvgpu_kmem_cache_destroy(kmem_cache);

test_exit:
	/* disable faults upon exit */
	nvgpu_posix_enable_fault_injection(kmem_fi, false, 0);
	if (nvgpu_posix_is_fault_injection_triggered(kmem_fi)) {
		unit_err(m, "unable to disable fault injection\n");
		ret = UNIT_FAIL;
	}

	if (ret != UNIT_SUCCESS) {
		unit_return_fail(m, "kmem cache fault injection test failure"
				    "\n");
	}

	return ret;
}

int test_kmem_cache_fi_delayed_disable(struct unit_module *m,
				       struct gk20a *g, void *__args)
{
	struct nvgpu_kmem_cache *kmem_cache;
	int ret = UNIT_SUCCESS;

	if (!verify_fi_disabled(m)) {
		ret = UNIT_FAIL;
		/* no reason to go on */
		goto test_exit;
	}

	/* enable faults now */
	nvgpu_posix_enable_fault_injection(kmem_fi, true, 0);
	if (!nvgpu_posix_is_fault_injection_triggered(kmem_fi)) {
		unit_err(m, "Unable to enable fault injection\n");
		ret = UNIT_FAIL;
		/* no reason to go on */
		goto test_exit;
	}

	/* disable faults after 1 call */
	nvgpu_posix_enable_fault_injection(kmem_fi, false, 1);
	if (!nvgpu_posix_is_fault_injection_triggered(kmem_fi)) {
		unit_err(m, "Fault injection errantly disabled too soon\n");
		ret = UNIT_FAIL;
		/* no reason to go on */
		goto test_exit;
	}

	/* first call should fail */
	kmem_cache = nvgpu_kmem_cache_create(g, TEST_DEFAULT_CACHE_SIZE);
	if (kmem_cache != NULL) {
		unit_err(m, "nvgpu_kmem_cache_create returned pointer when "
			    "fault injection enabled\n");
		ret = UNIT_FAIL;
	}

	/* second call should pass */
	kmem_cache = nvgpu_kmem_cache_create(g, TEST_DEFAULT_CACHE_SIZE);
	if (kmem_cache == NULL) {
		unit_err(m, "nvgpu_kmem_cache_create returned NULL when fault "
			    "injection disabled\n");
		ret = UNIT_FAIL;
	}

	/* good housekeeping */
	nvgpu_kmem_cache_destroy(kmem_cache);


test_exit:
	/* disable faults upon exit */
	nvgpu_posix_enable_fault_injection(kmem_fi, false, 0);
	if (nvgpu_posix_is_fault_injection_triggered(kmem_fi)) {
		unit_err(m, "unable to disable fault injection\n");
		ret = UNIT_FAIL;
	}

	if (ret != UNIT_SUCCESS) {
		unit_return_fail(m, "kmem cache fault injection test failure"
				    "\n");
	}

	return ret;
}

int test_kmem_kmalloc_fi_default(struct unit_module *m,
				 struct gk20a *g, void *__args)
{
	int *ptr;
	int ret = UNIT_SUCCESS;

	if (!verify_fi_disabled(m)) {
		ret = UNIT_FAIL;
		/* no reason to go on */
		goto test_exit;
	}

	/* check default case */
	ptr = (int *)nvgpu_kmalloc(g, TEST_DEFAULT_KMALLOC_SIZE);
	if (ptr == NULL) {
		unit_err(m, "nvgpu_kmalloc returned NULL when fault injection "
			    "disabled\n");
		ret = UNIT_FAIL;
	}
	/* good housekeeping */
	nvgpu_kfree(g, ptr);

test_exit:
	/* disable faults upon exit */
	nvgpu_posix_enable_fault_injection(kmem_fi, false, 0);
	if (nvgpu_posix_is_fault_injection_triggered(kmem_fi)) {
		unit_err(m, "unable to disable fault injection\n");
		ret = UNIT_FAIL;
	}

	if (ret != UNIT_SUCCESS) {
		unit_return_fail(m, "kmem cache fault injection test failure"
				    "\n");
	}

	return ret;
}

int test_kmem_kmalloc_fi_enabled(struct unit_module *m,
				 struct gk20a *g, void *__args)
{
	int *ptr;
	int ret = UNIT_SUCCESS;

	if (!verify_fi_disabled(m)) {
		ret = UNIT_FAIL;
		/* no reason to go on */
		goto test_exit;
	}

	/* enable faults */
	nvgpu_posix_enable_fault_injection(kmem_fi, true, 0);
	if (!nvgpu_posix_is_fault_injection_triggered(kmem_fi)) {
		unit_err(m, "Unable to enable fault injection\n");
		ret = UNIT_FAIL;
		/* no reason to go on */
		goto test_exit;
	}

	/* call kmalloc and verify error */
	ptr = (int *)nvgpu_kmalloc(g, TEST_DEFAULT_KMALLOC_SIZE);
	if (ptr != NULL) {
		unit_err(m, "nvgpu_kmalloc returned pointer when fault "
			    "injection enabled\n");
		ret = UNIT_FAIL;
	}

test_exit:
	/* disable faults upon exit */
	nvgpu_posix_enable_fault_injection(kmem_fi, false, 0);
	if (nvgpu_posix_is_fault_injection_triggered(kmem_fi)) {
		unit_err(m, "unable to disable fault injection\n");
		ret = UNIT_FAIL;
	}

	if (ret != UNIT_SUCCESS) {
		unit_return_fail(m, "kmem cache fault injection test failure"
				    "\n");
	}

	return ret;
}

int test_kmem_kmalloc_fi_delayed_enable(struct unit_module *m,
					struct gk20a *g, void *__args)
{
	const unsigned int fail_after = 2;
	int *ptrs[fail_after+1];
	unsigned int call_count;
	int ret = UNIT_SUCCESS;

	if (!verify_fi_disabled(m)) {
		ret = UNIT_FAIL;
		/* no reason to go on */
		goto test_exit;
	}

	/* enable faults after "fail_after" calls */
	nvgpu_posix_enable_fault_injection(kmem_fi, true, fail_after);
	if (nvgpu_posix_is_fault_injection_triggered(kmem_fi)) {
		unit_err(m, "Fault injection errantly enabled too soon\n");
		ret = UNIT_FAIL;
		/* no reason to go on */
		goto test_exit;
	}

	call_count = 1;
	while (call_count <= (fail_after + 1)) {
		ptrs[call_count-1] = nvgpu_kmalloc(g,
						   TEST_DEFAULT_KMALLOC_SIZE);
		if ((call_count <= fail_after) &&
		    (ptrs[call_count-1] == NULL)) {
			unit_err(m, "nvgpu_kmalloc returned NULL when fault "
				    "injection disabled\n");
			ret = UNIT_FAIL;
			/* no reason to go on */
			break;
		} else if ((call_count > fail_after) &&
			   (ptrs[call_count-1] != NULL)) {
			unit_err(m, "nvgpu_kmalloc returned pointer when fault "
				    "injection enabled\n");
			ret = UNIT_FAIL;
			/* no reason to go on */
			break;
		}
		call_count++;
	}

	/* good housekeeping */
	while (--call_count >= 1) {
		if (ptrs[call_count-1] != NULL) {
			nvgpu_kfree(g, ptrs[call_count-1]);
		}
	}

test_exit:
	/* disable faults upon exit */
	nvgpu_posix_enable_fault_injection(kmem_fi, false, 0);
	if (nvgpu_posix_is_fault_injection_triggered(kmem_fi)) {
		unit_err(m, "unable to disable fault injection\n");
		ret = UNIT_FAIL;
	}

	if (ret != UNIT_SUCCESS) {
		unit_return_fail(m, "kmem cache fault injection test failure"
				    "\n");
	}

	return ret;
}

int test_kmem_kmalloc_fi_delayed_disable(struct unit_module *m,
					 struct gk20a *g, void *__args)
{
	const unsigned int pass_after = 2;
	int *ptrs[pass_after+1];
	unsigned int call_count;
	int ret = UNIT_SUCCESS;

	if (!verify_fi_disabled(m)) {
		ret = UNIT_FAIL;
		/* no reason to go on */
		goto test_exit;
	}

	/* enable faults now */
	nvgpu_posix_enable_fault_injection(kmem_fi, true, 0);
	if (!nvgpu_posix_is_fault_injection_triggered(kmem_fi)) {
		unit_err(m, "Unable to enable fault injection\n");
		ret = UNIT_FAIL;
		/* no reason to go on */
		goto test_exit;
	}

	/* disable faults after "pass_after" calls */
	nvgpu_posix_enable_fault_injection(kmem_fi, false, pass_after);
	if (!nvgpu_posix_is_fault_injection_triggered(kmem_fi)) {
		unit_err(m, "Fault injection errantly disabled too soon\n");
		ret = UNIT_FAIL;
		/* no reason to go on */
		goto test_exit;
	}

	call_count = 1;
	while (call_count <= (pass_after + 1)) {
		ptrs[call_count-1] = nvgpu_kmalloc(g,
						   TEST_DEFAULT_KMALLOC_SIZE);
		if ((call_count <= pass_after) &&
		    (ptrs[call_count-1] != NULL)) {
			unit_err(m, "nvgpu_kmalloc returned pointer when fault "
				    "injection enabled\n");
			ret = UNIT_FAIL;
			/* no reason to go on */
			break;
		} else if ((call_count > pass_after) &&
			   (ptrs[call_count-1] == NULL)) {
			unit_err(m, "nvgpu_kmalloc returned NULL when fault "
				    "injection disabled\n");
			ret = UNIT_FAIL;
			/* no reason to go on */
			break;
		}
		call_count++;
	}

	/* good housekeeping */
	while (--call_count >= 1) {
		if (ptrs[call_count-1] != NULL) {
			nvgpu_kfree(g, ptrs[call_count-1]);
		}
	}

test_exit:
	/* disable faults upon exit */
	nvgpu_posix_enable_fault_injection(kmem_fi, false, 0);
	if (nvgpu_posix_is_fault_injection_triggered(kmem_fi)) {
		unit_err(m, "unable to disable fault injection\n");
		ret = UNIT_FAIL;
	}

	if (ret != UNIT_SUCCESS) {
		unit_return_fail(m, "kmem cache fault injection test failure"
				    "\n");
	}

	return ret;
}
