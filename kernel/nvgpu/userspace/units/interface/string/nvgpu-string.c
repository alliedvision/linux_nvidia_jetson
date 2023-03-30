/*
 * Copyright (c) 2020-2021, NVIDIA CORPORATION.  All rights reserved.
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

#include <nvgpu/string.h>

#include "nvgpu-string.h"

#include <string.h>

int test_memcpy_memcmp(struct unit_module *m, struct gk20a *g, void *args)
{
	const size_t len = 10;
	u8 dest[len];
	u8 src[len];
	u8 i;

	for (i = 0; i < len; i++) {
		dest[i] = 0;
		src[i] = i;
	}
	nvgpu_memcpy(dest, src, len);
	unit_assert(nvgpu_memcmp(dest, src, len) == 0, return UNIT_FAIL);

	for (i = 0; i < len; i++) {
		dest[i] = 0;
	}
	nvgpu_memcpy(dest, src, len - 1U);
	unit_assert(nvgpu_memcmp(dest, src, len - 1U) == 0, return UNIT_FAIL);
	unit_assert(dest[len - 1] == 0, return UNIT_FAIL);
	unit_assert(nvgpu_memcmp(dest, src, len) != 0, return UNIT_FAIL);

	/* test size==0 */
	unit_assert(nvgpu_memcmp(dest, src, 0) == 0, return UNIT_FAIL);

	return UNIT_SUCCESS;
}

int test_strnadd_u32(struct unit_module *m, struct gk20a *g, void *args)
{
	const size_t len = 40;
	char dest[len];
	const char *max_str = "11111111111111111111111111111111";

	/* test invalid radices */
	unit_assert(nvgpu_strnadd_u32(dest, 10U, len, 0U) == 0,
							return UNIT_FAIL);
	unit_assert(nvgpu_strnadd_u32(dest, 10U, len, 1U) == 0,
							return UNIT_FAIL);
	unit_assert(nvgpu_strnadd_u32(dest, 10U, len, 17U) == 0,
							return UNIT_FAIL);
	unit_assert(nvgpu_strnadd_u32(dest, 10U, len, 100U) == 0,
							return UNIT_FAIL);
	unit_assert(nvgpu_strnadd_u32(dest, 10U, len, UINT32_MAX) == 0,
							return UNIT_FAIL);

	/* test insufficient space */
	unit_assert(nvgpu_strnadd_u32(dest, 1000U, 0, 10U) == 0,
							return UNIT_FAIL);
	unit_assert(nvgpu_strnadd_u32(dest, 1000U, 2, 10U) == 0,
							return UNIT_FAIL);
	unit_assert(nvgpu_strnadd_u32(dest, 1000U, 4, 10U) == 0,
							return UNIT_FAIL);

	unit_assert(nvgpu_strnadd_u32(dest, 1U, len, 2U) == 1,
							return UNIT_FAIL);
	unit_assert(strncmp(dest, "1", 4) == 0, return UNIT_FAIL);

	unit_assert(nvgpu_strnadd_u32(dest, 0xffffffff, len, 2U) == 32,
							return UNIT_FAIL);
	unit_assert(strncmp(dest, max_str, 32) == 0, return UNIT_FAIL);

	unit_assert(nvgpu_strnadd_u32(dest, 1000U, len, 10U) == 4,
							return UNIT_FAIL);
	unit_assert(strncmp(dest, "1000", 4) == 0, return UNIT_FAIL);

	unit_assert(nvgpu_strnadd_u32(dest, 0xdeadbeef, len, 16U) == 8,
							return UNIT_FAIL);

	unit_assert(strncmp(dest, "deadbeef", 8) == 0, return UNIT_FAIL);

	return UNIT_SUCCESS;
}

int test_mem_is_word_aligned(struct unit_module *m, struct gk20a *g, void *args)
{
	unit_assert(nvgpu_mem_is_word_aligned(g, (u8 *)0x1000),
							return UNIT_FAIL);
	unit_assert(!nvgpu_mem_is_word_aligned(g, (u8 *)0x1001),
							return UNIT_FAIL);
	unit_assert(!nvgpu_mem_is_word_aligned(g, (u8 *)0x1002),
							return UNIT_FAIL);
	unit_assert(!nvgpu_mem_is_word_aligned(g, (u8 *)0x1003),
							return UNIT_FAIL);
	unit_assert(nvgpu_mem_is_word_aligned(g, (u8 *)0x1004),
							return UNIT_FAIL);

	return UNIT_SUCCESS;
}

struct unit_module_test string_tests[] = {
	UNIT_TEST(memcpy_memcmp,	test_memcpy_memcmp,		NULL, 0),
	UNIT_TEST(strnadd_u32,		test_strnadd_u32,		NULL, 0),
	UNIT_TEST(mem_is_word_aligned,	test_mem_is_word_aligned,	NULL, 0),
};

UNIT_MODULE(string, string_tests, UNIT_PRIO_NVGPU_TEST);
