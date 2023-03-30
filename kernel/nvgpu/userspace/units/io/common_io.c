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

#include <unit/io.h>
#include <unit/unit.h>

#include <include/nvgpu/posix/io.h>
#include <nvgpu/types.h>
#include <nvgpu/io.h>
#include <nvgpu/bug.h>

#include "common_io.h"

#define USER_MODE_BASE (0x00810000U)
#define NVGPU_READ_VAL (0xD007U)

static void readl_access_reg_fn(struct gk20a *g,
	struct nvgpu_reg_access *access)
{
	access->value = NVGPU_READ_VAL;
}

static void writel_access_reg_fn(struct gk20a *g,
	struct nvgpu_reg_access *access)
{

}

static struct nvgpu_posix_io_callbacks ut_common_io_reg_callbacks = {
	.readl         = readl_access_reg_fn,
	.writel          = writel_access_reg_fn,
};

int test_writel_check(struct unit_module *m, struct gk20a *g, void *args)
{
	nvgpu_posix_register_io(g, &ut_common_io_reg_callbacks);

	/* Value 0 will force to fail readback call as read API returns
	 * NVGPU_READ_VAL. */
	EXPECT_BUG(nvgpu_writel_check(g, USER_MODE_BASE, 0));

	/* Value NVGPU_READ_VAL will pass the readback call as read API returns
	 * NVGPU_READ_VAL. */
	nvgpu_writel_check(g, USER_MODE_BASE, NVGPU_READ_VAL);

	return UNIT_SUCCESS;
}

struct unit_module_test io_tests[] = {
	UNIT_TEST(writel_check, test_writel_check, NULL, 0),
};

UNIT_MODULE(io, io_tests, UNIT_PRIO_NVGPU_TEST);
