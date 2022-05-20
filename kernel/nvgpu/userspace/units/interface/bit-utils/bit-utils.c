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
#include <nvgpu/utils.h>

#include "bit-utils.h"

int test_hi_lo(struct unit_module *m, struct gk20a *g, void *args)
{
	u64 val_hi = 0xfedcba01;
	u64 val_lo = 0x12345678;
	u64 full_val = (val_hi << 32) | val_lo;

	unit_assert(u64_hi32(full_val) == val_hi, return UNIT_FAIL);
	unit_assert(u64_lo32(full_val) == val_lo, return UNIT_FAIL);
	unit_assert(hi32_lo32_to_u64(val_hi, val_lo) == full_val,
							return UNIT_FAIL);

	return UNIT_SUCCESS;
}

int test_fields(struct unit_module *m, struct gk20a *g, void *args)
{
	unit_assert(set_field(0U, 0x000ff000, 0x00055000) == 0x00055000,
							return UNIT_FAIL);
	unit_assert(set_field(0U, 0xffffffff, 0x00055000) == 0x00055000,
							return UNIT_FAIL);
	unit_assert(set_field(0xffffffff, 0xffffffff, 0x00055000) == 0x00055000,
							return UNIT_FAIL);
	unit_assert(set_field(0xffffffff, 0x000ff000, 0x00055000) == 0xfff55fff,
							return UNIT_FAIL);

	unit_assert(get_field(0U, 0xffffffff) == 0x0, return UNIT_FAIL);
	unit_assert(get_field(0xffffffff, 0xffffffff) == 0xffffffff,
							return UNIT_FAIL);
	unit_assert(get_field(0xffffffff, 0x000ff000) == 0x000ff000,
							return UNIT_FAIL);

	return UNIT_SUCCESS;
}

struct unit_module_test bit_utils_tests[] = {
	UNIT_TEST(hi_lo,	test_hi_lo,		NULL, 0),
	UNIT_TEST(fields,	test_fields,		NULL, 0),
};

UNIT_MODULE(bit_utils, bit_utils_tests, UNIT_PRIO_NVGPU_TEST);