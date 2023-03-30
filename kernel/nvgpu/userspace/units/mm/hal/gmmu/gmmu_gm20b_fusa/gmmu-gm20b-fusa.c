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

#include <nvgpu/gk20a.h>
#include <nvgpu/types.h>

#include "hal/mm/gmmu/gmmu_gm20b.h"

#include "gmmu-gm20b-fusa.h"


int test_gm20b_mm_get_big_page_sizes(struct unit_module *m, struct gk20a *g,
								void *args)
{
	u32 ret_pgsz;
	int ret = UNIT_FAIL;

	ret_pgsz = gm20b_mm_get_big_page_sizes();
	unit_assert(ret_pgsz == (SZ_64K | SZ_128K), goto done);

	ret = UNIT_SUCCESS;

done:
	if (ret != UNIT_SUCCESS) {
		unit_err(m, "%s pde_pgsz != GMMU_PAGE_SIZE_SMALL as expected\n",
				__func__);
	}

	return ret;
}

struct unit_module_test mm_gmmu_gm20b_fusa_tests[] = {
	UNIT_TEST(get_big_pgsz, test_gm20b_mm_get_big_page_sizes, NULL, 0),
};

UNIT_MODULE(gmmu_gm20b_fusa, mm_gmmu_gm20b_fusa_tests, UNIT_PRIO_NVGPU_TEST);
