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

#include <unit/unit.h>
#include <unit/io.h>
#include <nvgpu/posix/io.h>

#include <nvgpu/gk20a.h>

#include "nvgpu_gk20a.h"

int test_get_poll_timeout(struct unit_module *m, struct gk20a *g, void *args)
{
	const u32 default_timeout = 123456;

	g->poll_timeout_default = default_timeout;
	unit_assert(nvgpu_get_poll_timeout(g) == default_timeout,
							return UNIT_FAIL);

	return  UNIT_SUCCESS;
}

struct unit_module_test nvgpu_gk20a_tests[] = {
	UNIT_TEST(get_poll_timeout,	test_get_poll_timeout,	NULL, 0),
};

UNIT_MODULE(nvgpu_gk20a, nvgpu_gk20a_tests, UNIT_PRIO_NVGPU_TEST);
