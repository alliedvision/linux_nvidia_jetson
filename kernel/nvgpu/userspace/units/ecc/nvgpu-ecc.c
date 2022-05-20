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

#include <unit/unit.h>
#include <unit/io.h>

#include <nvgpu/gk20a.h>
#include <nvgpu/ecc.h>
#include <nvgpu/ltc.h>
#include <nvgpu/posix/posix-fault-injection.h>
#include <common/gr/gr_priv.h>

#include "nvgpu-ecc.h"

static void mock_ecc_free(struct gk20a *g) {

}

int test_ecc_init_support(struct unit_module *m, struct gk20a *g,
		void *args)
{
	/*
	 * Case #1:
	 *  - First time ecc intialization.
	 *  - "nvgpu_ecc_init_support" should perform init and return 0.
	 */
	g->ecc.initialized = false;
	if (nvgpu_ecc_init_support(g) != 0) {
		return UNIT_FAIL;
	}

	/*
	 * Case #2:
	 *  - Second time ecc intialization.
	 *  - "nvgpu_ecc_init_support" should skip init but return 0.
	 */
	g->ecc.initialized = true;
	if (nvgpu_ecc_init_support(g) != 0) {
		return UNIT_FAIL;
	}

	return UNIT_SUCCESS;
}

int test_ecc_finalize_support(struct unit_module *m, struct gk20a *g,
		void *args)
{
	/*
	 * Case #1:
	 *  - First time ecc intialization.
	 *  - "nvgpu_ecc_finalize_support" should perform init and return 0.
	 */
	g->ecc.initialized = false;
	if (nvgpu_ecc_finalize_support(g) != 0) {
		return UNIT_FAIL;
	}

	/*
	 * Case #2:
	 *  - Second time ecc intialization.
	 *  - "nvgpu_ecc_finalize_support" should skip init but return 0.
	 */
	g->ecc.initialized = true;
	if (nvgpu_ecc_finalize_support(g) != 0) {
		return UNIT_FAIL;
	}

	return UNIT_SUCCESS;
}

int test_ecc_counter_init(struct unit_module *m, struct gk20a *g,
		void *args)
{
	int ret = UNIT_SUCCESS;
	struct nvgpu_ecc_stat *stat = NULL;
	char *name;
	struct nvgpu_posix_fault_inj *kmem_fi =
		nvgpu_kmem_get_fault_injection();

	/*
	 * Test setup:
	 *  - Allocate memory for ecc counter name.
	 *  - Initialize ecc support.
	 */
	name = nvgpu_kzalloc(g, NVGPU_ECC_STAT_NAME_MAX_SIZE + 1);
	if (name == NULL) {
		return UNIT_FAIL;
	}

	if (nvgpu_ecc_init_support(g) != 0) {
		ret = UNIT_FAIL;
		goto cleanup;
	}

	/*
	 * Case #1:
	 *  - Initialize "name" with valid length string.
	 *  - "nvgpu_ecc_counter_init" should return 0.
	 */
	strcpy(name, "test_counter");
	if (nvgpu_ecc_counter_init(g, &stat, name) != 0) {
		ret = UNIT_FAIL;
		goto cleanup;
	}
	nvgpu_ecc_counter_deinit(g, &stat);

	/*
	 * Case #2:
	 *  - Inject SW fault to cause "nvgpu_kzalloc" failure.
	 *  - "nvgpu_ecc_counter_init" should return -ENOMEM.
	 */
	nvgpu_posix_enable_fault_injection(kmem_fi, true, 0);
	if (nvgpu_ecc_counter_init(g, &stat, name) != -ENOMEM) {
		ret = UNIT_FAIL;
		goto cleanup;
	}
	nvgpu_posix_enable_fault_injection(kmem_fi, false, 0);

	/*
	 * Case #3:
	 *  - Intialize "name" with string of length greater than
	 *    NVGPU_ECC_STAT_NAME_MAX_SIZE.
	 *  - "nvgpu_ecc_counter_init" should return 0, with a truncated
	 *     counter name.
	 */
	memset(name, NVGPU_ECC_STAT_NAME_MAX_SIZE, 'a');
	if (nvgpu_ecc_counter_init(g, &stat, name) != 0) {
		ret = UNIT_FAIL;
		goto cleanup;
	}

	nvgpu_ecc_counter_deinit(g, &stat);

	if (!nvgpu_list_empty(&g->ecc.stats_list)) {
		ret = UNIT_FAIL;
		goto cleanup;
	}

cleanup:
	if (stat != NULL) {
		nvgpu_ecc_counter_deinit(g, &stat);
	}
	nvgpu_kfree(g, name);

	return ret;
}

int test_ecc_free(struct unit_module *m, struct gk20a *g,
		void *args)
{
	int ret = UNIT_SUCCESS;

	if (nvgpu_ecc_init_support(g) != 0) {
		return UNIT_FAIL;
	}

	/*
	 * Setup:
	 *  - allocate memory for gr and clear it to zero.
	 *  - set gr->config to NULL to return immediately from
	 *    nvgpu_gr_ecc_free.
	 *  - Allocate memory for ltc and clear it to zero, this should set
	 *    the ltc_Count and slice_per_ltc to 0.
	 */
	g->gr = nvgpu_kzalloc(g, sizeof(*g->gr));
	if (g->gr == NULL) {
		return UNIT_FAIL;
	}
	g->ltc = nvgpu_kzalloc(g, sizeof(*g->ltc));
	if (g->ltc == NULL) {
		nvgpu_kfree(g, g->gr);
		return UNIT_FAIL;
	}

	/*
	 * Case #1:
	 *  - fb, fpba and pmu ecc HALs have ecc free handles set to NULL.
	 *  - "nvgpu_ecc_free" should skip freeing ecc counters for fb, fpba,
	 *    pmu and return without faulting.
	 */
	g->ops.fb.ecc.free = NULL;
	g->ops.pmu.ecc_free = NULL;
	g->ecc.ltc.ecc_sec_count = nvgpu_kzalloc(g,
			sizeof(*g->ecc.ltc.ecc_sec_count));
	g->ecc.ltc.ecc_ded_count = nvgpu_kzalloc(g,
		sizeof(*g->ecc.ltc.ecc_ded_count));
	if (g->ecc.ltc.ecc_sec_count == NULL
			|| g->ecc.ltc.ecc_ded_count == NULL) {
		ret = UNIT_FAIL;
		goto cleanup;
	}
	nvgpu_ecc_free(g);

	/*
	 * Case #2:
	 *  - fb and pmu ecc HALs have ecc free handles are set.
	 *  - "nvgpu_ecc_free" should return without faulting.
	 */
	g->ops.fb.ecc.free = mock_ecc_free;
	g->ops.pmu.ecc_free = mock_ecc_free;
	g->ecc.ltc.ecc_sec_count = nvgpu_kzalloc(g,
			sizeof(*g->ecc.ltc.ecc_sec_count));
	g->ecc.ltc.ecc_ded_count = nvgpu_kzalloc(g,
		sizeof(*g->ecc.ltc.ecc_ded_count));
	if (g->ecc.ltc.ecc_sec_count == NULL
			|| g->ecc.ltc.ecc_ded_count == NULL) {
		ret = UNIT_FAIL;
		goto cleanup;
	}
	nvgpu_ecc_free(g);
	g->ecc.ltc.ecc_sec_count = NULL;
	g->ecc.ltc.ecc_ded_count = NULL;

cleanup:
	if (g->ecc.ltc.ecc_sec_count != NULL) {
		nvgpu_kfree(g, g->ecc.ltc.ecc_sec_count);
	}
	if (g->ecc.ltc.ecc_ded_count != NULL) {
		nvgpu_kfree(g, g->ecc.ltc.ecc_ded_count);
	}
	nvgpu_kfree(g, g->gr);
	nvgpu_kfree(g, g->ltc);

	return ret;
}

struct unit_module_test ecc_tests[] = {
	UNIT_TEST(ecc_init_support,	test_ecc_init_support,		NULL, 0),
	UNIT_TEST(ecc_finalize_support,	test_ecc_finalize_support,	NULL, 0),
	UNIT_TEST(ecc_counter_init,	test_ecc_counter_init,		NULL, 0),
	UNIT_TEST(ecc_free,		test_ecc_free,			NULL, 0),
};

UNIT_MODULE(ecc, ecc_tests, UNIT_PRIO_NVGPU_TEST);
