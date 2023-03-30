/*
 * Copyright (c) 2019-2020, NVIDIA CORPORATION.  All rights reserved.
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
#include <nvgpu/hw/gk20a/hw_ram_gk20a.h>

#include "hal/fifo/ramin_gk20a.h"

#include "../../nvgpu-fifo-common.h"
#include "ramin-gk20a-fusa.h"

int test_gk20a_ramin_base_shift(struct unit_module *m, struct gk20a *g,
								void *args)
{
	int ret = UNIT_FAIL;
	u32 base_shift = 0U;

	base_shift = gk20a_ramin_base_shift();
	unit_assert(base_shift == ram_in_base_shift_v(), goto done);

	ret = UNIT_SUCCESS;
done:
	if (ret != UNIT_SUCCESS) {
		unit_err(m, "%s failed\n", __func__);
	}
	return ret;
}

int test_gk20a_ramin_alloc_size(struct unit_module *m, struct gk20a *g,
								void *args)
{
	int ret = UNIT_FAIL;
	u32 alloc_size = 0U;

	alloc_size = gk20a_ramin_alloc_size();
	unit_assert(alloc_size == ram_in_alloc_size_v(), goto done);

	ret = UNIT_SUCCESS;
done:
	if (ret != UNIT_SUCCESS) {
		unit_err(m, "%s failed\n", __func__);
	}
	return ret;
}

struct unit_module_test ramin_gk20a_fusa_tests[] = {
	UNIT_TEST(base_shift, test_gk20a_ramin_base_shift, NULL, 0),
	UNIT_TEST(alloc_size, test_gk20a_ramin_alloc_size, NULL, 0),
};

UNIT_MODULE(ramin_gk20a_fusa, ramin_gk20a_fusa_tests, UNIT_PRIO_NVGPU_TEST);
