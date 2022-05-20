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
#include <nvgpu/dma.h>
#include <nvgpu/nvgpu_mem.h>
#include <nvgpu/posix/posix-fault-injection.h>

#include "posix-fault-injection-dma-alloc.h"

#define TEST_DEFAULT_SIZE 4096

static struct nvgpu_posix_fault_inj *dma_fi;

/*
 * Used to make sure fault injection is disabled before running test
 * If already enabled, prints warning and disables
 *
 * Returns false if unable to guarantee fault injection is disabled
 */
static bool verify_fi_disabled(struct unit_module *m)
{
	if (nvgpu_posix_is_fault_injection_triggered(dma_fi)) {
		unit_info(m, "Unexpected fault injection enabled\n");
	}

	/* force disabled in case it was in "delay" mode */
	nvgpu_posix_enable_fault_injection(dma_fi, false, 0);
	if (nvgpu_posix_is_fault_injection_triggered(dma_fi)) {
		unit_err(m, "Unable to disable fault injection\n");
		return false;
	}

	return true;
}

int test_dma_alloc_init(struct unit_module *m,
			struct gk20a *g, void *__args)
{
	dma_fi = nvgpu_dma_alloc_get_fault_injection();
	if (dma_fi == NULL) {
		return UNIT_FAIL;
	} else {
		return UNIT_SUCCESS;
	}
}

int test_dma_alloc_fi_default(struct unit_module *m,
			      struct gk20a *g, void *__args)
{
	struct nvgpu_mem mem = { };
	int result;
	int ret = UNIT_SUCCESS;

	if (!verify_fi_disabled(m)) {
		ret = UNIT_FAIL;
		/* no reason to go on */
		goto test_exit;
	}

	/* check default case, no fault injection */
	result = nvgpu_dma_alloc(g, TEST_DEFAULT_SIZE, &mem);
	if (result != 0) {
		unit_err(m, "nvgpu_dma_alloc returned error when fault "
			    "injection disabled\n");
		ret = UNIT_FAIL;
	}

	/* good housekeeping */
	nvgpu_dma_free(g, &mem);

test_exit:
	/* disable faults upon exit */
	nvgpu_posix_enable_fault_injection(dma_fi, false, 0);
	if (nvgpu_posix_is_fault_injection_triggered(dma_fi)) {
		unit_err(m, "unable to disable fault injection\n");
		ret = UNIT_FAIL;
	}

	if (ret != UNIT_SUCCESS) {
		unit_return_fail(m, "dma alloc fault injection test "
				    "failure\n");
	}

	return ret;
}

int test_dma_alloc_fi_enabled(struct unit_module *m,
			      struct gk20a *g, void *__args)
{
	struct nvgpu_mem mem = { };
	int result;
	int ret = UNIT_SUCCESS;

	if (!verify_fi_disabled(m)) {
		ret = UNIT_FAIL;
		/* no reason to go on */
		goto test_exit;
	}

	/* enable fault injection immediately */
	nvgpu_posix_enable_fault_injection(dma_fi, true, 0);
	if (!nvgpu_posix_is_fault_injection_triggered(dma_fi)) {
		unit_err(m, "unable to enable fault injection\n");
		ret = UNIT_FAIL;
		goto test_exit;
	}

	result = nvgpu_dma_alloc(g, TEST_DEFAULT_SIZE, &mem);
	if (result == 0) {
		unit_err(m, "nvgpu_dma_alloc returned success when fault "
			    "injection enabled\n");
		nvgpu_dma_free(g, &mem);
		ret = UNIT_FAIL;
	}

test_exit:
	/* disable faults upon exit */
	nvgpu_posix_enable_fault_injection(dma_fi, false, 0);
	if (nvgpu_posix_is_fault_injection_triggered(dma_fi)) {
		unit_err(m, "unable to disable fault injection\n");
		ret = UNIT_FAIL;
	}

	if (ret != UNIT_SUCCESS) {
		unit_return_fail(m, "dma alloc fault injection test "
				    "failure\n");
	}

	return ret;
}

int test_dma_alloc_fi_delayed_enable(struct unit_module *m,
				     struct gk20a *g, void *__args)
{
#define FAIL_AFTER 2
	unsigned int call_count = 0;
	struct nvgpu_mem mem[FAIL_AFTER+1] = { };
	int result = 0;
	int ret = UNIT_SUCCESS;

	if (!verify_fi_disabled(m)) {
		ret = UNIT_FAIL;
		/* no reason to go on */
		goto test_exit;
	}

	/* enable fault injection after delay */
	nvgpu_posix_enable_fault_injection(dma_fi, true, FAIL_AFTER);
	if (nvgpu_posix_is_fault_injection_triggered(dma_fi)) {
		unit_err(m, "Fault injection errantly enabled too soon\n");
		ret = UNIT_FAIL;
		goto test_exit;
	}

	call_count = 1;
	while (call_count <= (FAIL_AFTER+1)) {
		result = nvgpu_dma_alloc(g, TEST_DEFAULT_SIZE,
					 &mem[call_count-1]);

		if ((call_count <= FAIL_AFTER) && (result != 0U)) {
			unit_err(m, "nvgpu_dma_alloc returned error when fault "
				    "injection disabled\n");
			ret = UNIT_FAIL;
			/* no reason to go on */
			break;
		} else if ((call_count > FAIL_AFTER) && (result == 0U)) {
			unit_err(m, "nvgpu_dma_alloc returned success when "
				    "fault injection enabled\n");
			ret = UNIT_FAIL;
			/* no reason to go on */
			break;
		}
		call_count++;
	}

test_exit:
	/* free allocations */
	if (result != 0) {
		/* the last alloc failed, so decrement before we start
		 * freeing
		 */
		call_count--;
	}
	while (call_count > 1) {
		nvgpu_dma_free(g, &mem[call_count-1]);
		call_count--;
	}

	/* disable faults upon exit */
	nvgpu_posix_enable_fault_injection(dma_fi, false, 0);
	if (nvgpu_posix_is_fault_injection_triggered(dma_fi)) {
		unit_err(m, "unable to disable fault injection\n");
		ret = UNIT_FAIL;
	}

	if (ret != UNIT_SUCCESS) {
		unit_return_fail(m, "dma alloc fault injection test "
				    "failure\n");
	}

	return ret;
}
