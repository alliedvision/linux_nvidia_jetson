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

#include <unit/io.h>
#include <unit/unit.h>

#include <nvgpu/io.h>
#include <nvgpu/posix/io.h>

#include <nvgpu/gk20a.h>
#include <nvgpu/gmmu.h>
#include "hal/mm/gmmu/gmmu_gk20a.h"

#include "gmmu-gk20a-fusa.h"

int test_gk20a_get_pde_pgsz(struct unit_module *m, struct gk20a *g, void *args)
{
	struct gk20a_mmu_level l;
	struct nvgpu_gmmu_pd pd;
	u32 ret_pgsz;
	int ret = UNIT_FAIL;

	ret_pgsz = gk20a_get_pde_pgsz(g, &l, &pd, 0U);
	unit_assert(ret_pgsz == GMMU_PAGE_SIZE_SMALL, goto done);

	ret = UNIT_SUCCESS;

done:
	if (ret != UNIT_SUCCESS) {
		unit_err(m, "%s pde_pgsz != GMMU_PAGE_SIZE_SMALL as expected\n",
				__func__);
	}

	return ret;
}

int test_gk20a_get_pte_pgsz(struct unit_module *m, struct gk20a *g, void *args)
{
	struct gk20a_mmu_level l;
	struct nvgpu_gmmu_pd pd;
	u32 ret_pgsz;
	int ret = UNIT_FAIL;

	ret_pgsz = gk20a_get_pte_pgsz(g, &l, &pd, 0U);
	unit_assert(ret_pgsz == GMMU_NR_PAGE_SIZES, goto done);

	ret = UNIT_SUCCESS;

done:
	if (ret != UNIT_SUCCESS) {
		unit_err(m, "%s pte_pgsz != GMMU_NR_PAGE_SIZES as expected\n",
				__func__);
	}

	return ret;
}

struct unit_module_test mm_gmmu_gk20a_fusa_tests[] = {
	UNIT_TEST(pde_pgsz, test_gk20a_get_pde_pgsz, NULL, 0),
	UNIT_TEST(pte_pgsz, test_gk20a_get_pte_pgsz, NULL, 0),
};

UNIT_MODULE(gmmu_gk20a_fusa, mm_gmmu_gk20a_fusa_tests, UNIT_PRIO_NVGPU_TEST);
