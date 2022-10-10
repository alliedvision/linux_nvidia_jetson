/*
 * Copyright (c) 2020-2022, NVIDIA CORPORATION.  All rights reserved.
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
#include <stdint.h>

#include <unit/io.h>
#include <unit/unit.h>

#include <nvgpu/utils.h>
#include <nvgpu/barrier.h>

#include "posix-utils.h"

#define KHZ (1000U)
#define MHZ (1000000U)

#define ARRAY1_SIZE	4
#define ARRAY2_SIZE	10

#define PAGE_ALIGN_TEST_VALUE	0x3fffffff
#define ALIGN_TEST_VALUE	0xffff
#define ALIGN_WITH_VALUE	0x10
#define ALIGN_WITH_MASK		0x3

#define TO_ROUND_VALUE		11U
#define ROUND_BY_VALUE		4U
#define ROUND_UP_RESULT		12U
#define ROUND_DOWN_RESULT	8U

struct test_container {
	uint32_t var1;
	uint32_t var2;
};

struct test_container cont = {20, 30};

/*
 * Test to ensure the EXPECT_BUG construct works as intended by making sure it
 * behaves properly when BUG is called or not.
 * In the event that EXPECT_BUG is completely broken, the call to BUG() would
 * cause the unit to crash and report a failure correctly.
 */
int test_hamming_weight(struct unit_module *m,
				struct gk20a *g, void *args)
{
	unsigned int result;
	unsigned int i;
	uint8_t hwt_8bit;
	uint16_t hwt_16bit;
	uint32_t hwt_32bit;
	uint64_t hwt_64bit;

	for (i = 0; i < 8; i++) {
		hwt_8bit = (unsigned int) 1 << i;
		result = nvgpu_posix_hweight8(hwt_8bit);
		if (result != 1) {
			unit_return_fail(m,
				"8 bit hwt failed for %d\n", hwt_8bit);
		}
	}

	for (i = 0; i < 16; i++) {
		hwt_16bit = (unsigned int) 1 << i;
		result = nvgpu_posix_hweight16(hwt_16bit);
		if (result != 1) {
			unit_return_fail(m,
				"16 bit hwt failed for %d\n", hwt_16bit);
		}
	}

	for (i = 0; i < 32; i++) {
		hwt_32bit = (unsigned int) 1 << i;
		result = nvgpu_posix_hweight32(hwt_32bit);
		if (result != 1) {
			unit_return_fail(m,
				"32 bit hwt failed for %d\n", hwt_32bit);
		}
	}

	for (i = 0; i < 32; i++) {
		hwt_32bit = (unsigned int) 1 << i;
		result = hweight32(hwt_32bit);
		if (result != 1) {
			unit_return_fail(m,
				"hweight32 failed for %d\n", hwt_32bit);
		}
	}

	for (i = 0; i < 64; i++) {
		hwt_64bit = (unsigned long) 1 << i;
		result = nvgpu_posix_hweight64(hwt_64bit);
		if (result != 1) {
			unit_return_fail(m,
				"64 bit hwt failed for %lx\n", hwt_64bit);
		}
	}

	for (i = 0; i < 64; i++) {
		hwt_64bit = (unsigned long) 1 << i;
		result = hweight_long(hwt_64bit);
		if (result != 1) {
			unit_return_fail(m,
				"hweight_long failed for %lx\n", hwt_64bit);
		}
	}

	hwt_8bit = 0x0;
	result = nvgpu_posix_hweight8(hwt_8bit);
	if (result != 0) {
		unit_return_fail(m,
			"8 bit hwt failed for %d\n", hwt_8bit);
	}

	hwt_8bit = 0xff;
	result = nvgpu_posix_hweight8(hwt_8bit);
	if (result != 8) {
		unit_return_fail(m,
			"8 bit hwt failed for %d\n", hwt_8bit);
	}

	hwt_16bit = 0x0;
	result = nvgpu_posix_hweight16(hwt_16bit);
	if (result != 0) {
		unit_return_fail(m,
			"16 bit hwt failed for %d\n", hwt_16bit);
	}

	hwt_16bit = 0xffff;
	result = nvgpu_posix_hweight16(hwt_16bit);
	if (result != 16) {
		unit_return_fail(m,
			"16 bit hwt failed for %d\n", hwt_16bit);
	}

	hwt_32bit = 0x0;
	result = nvgpu_posix_hweight32(hwt_32bit);
	if (result != 0) {
		unit_return_fail(m,
			"32 bit hwt failed for %d\n", hwt_32bit);
	}

	hwt_32bit = 0xffffffff;
	result = nvgpu_posix_hweight32(hwt_32bit);
	if (result != 32) {
		unit_return_fail(m,
			"32 bit hwt failed for %d\n", hwt_32bit);
	}

	hwt_32bit = 0x0;
	result = hweight32(hwt_32bit);
	if (result != 0) {
		unit_return_fail(m,
			"hweight32 failed for %d\n", hwt_32bit);
	}

	hwt_32bit = 0xffffffff;
	result = hweight32(hwt_32bit);
	if (result != 32) {
		unit_return_fail(m,
			"hweight32 failed for %d\n", hwt_32bit);
	}

	hwt_64bit = 0x0;
	result = nvgpu_posix_hweight64(hwt_64bit);
	if (result != 0) {
		unit_return_fail(m,
			"64 bit hwt failed for %ld\n", hwt_64bit);
	}

	hwt_64bit = 0xffffffffffffffff;
	result = nvgpu_posix_hweight64(hwt_64bit);
	if (result != 64) {
		unit_return_fail(m,
			"64 bit hwt failed for %ld\n", hwt_64bit);
	}

	hwt_64bit = 0x0;
	result = hweight_long(hwt_64bit);
	if (result != 0) {
		unit_return_fail(m,
			"hweight_long failed for %ld\n", hwt_64bit);
	}

	hwt_64bit = 0xffffffffffffffff;
	result = hweight_long(hwt_64bit);
	if (result != 64) {
		unit_return_fail(m,
			"hweight_long failed for %ld\n", hwt_64bit);
	}

	return UNIT_SUCCESS;
}

int test_be32tocpu(struct unit_module *m,
			struct gk20a *g, void *args)
{
	uint32_t pattern;
	uint32_t result;
	uint8_t *ptr;

	pattern = 0xaabbccdd;

	ptr = (uint8_t*)&pattern;

	result = be32_to_cpu(pattern);

	if (*ptr == 0xdd) {
		if (result != 0xddccbbaa) {
			unit_return_fail(m,
				"be32tocpu failed for %x %x\n",
				pattern, result);
		}
	}

	return UNIT_SUCCESS;
}

int test_minmax(struct unit_module *m,
			struct gk20a *g, void *args)
{
	uint32_t i;
	uint32_t a;
	uint32_t b;
	uint32_t c;
	uint32_t result;

	a = 10;
	b = 20;
	c = 30;
	for (i = 0; i < 10; i++) {
		result = min(a, b);
		if (result != a) {
			unit_return_fail(m, "min failure %d\n", result);
		}

		result = min(b, a);
		if (result != a) {
			unit_return_fail(m, "min failure %d\n", result);
		}

		a += 5;
		b += 5;
	}

	a = 100;
	b = 200;
	c = 300;
	for (i = 0; i < 10; i++) {
		result = min3(a, b, c);
		if (result != a) {
			unit_return_fail(m, "min3 failure %d\n", result);
		}

		result = min3(a, c, b);
		if (result != a) {
			unit_return_fail(m, "min3 failure %d\n", result);
		}

		result = min3(b, a, c);
		if (result != a) {
			unit_return_fail(m, "min3 failure %d\n", result);
		}

		result = min3(b, c, a);
		if (result != a) {
			unit_return_fail(m, "min3 failure %d\n", result);
		}

		result = min3(c, a, b);
		if (result != a) {
			unit_return_fail(m, "min3 failure %d\n", result);
		}

		result = min3(c, b, a);
		if (result != a) {
			unit_return_fail(m, "min3 failure %d\n", result);
		}

		a += 5;
		b += 5;
		c += 5;
	}

	b = 2000;
	c = 3000;
	for (i = 0; i < 10; i++) {
		result = min_t(uint32_t, b, c);
		if (result != b) {
			unit_return_fail(m, "min_t failure %d\n", result);
		}

		result = min_t(uint32_t, c, b);
		if (result != b) {
			unit_return_fail(m, "min_t failure %d\n", result);
		}

		b += 100;
		c += 100;
	}

	a = 1000;
	b = 2000;
	for (i = 0; i < 10; i++) {
		result = max(a, b);
		if (result != b) {
			unit_return_fail(m, "max failure %d\n", result);
		}

		result = max(b, a);
		if (result != b) {
			unit_return_fail(m, "max failure %d\n", result);
		}

		a += 100;
		b += 100;
	}

	return UNIT_SUCCESS;
}

int test_arraysize(struct unit_module *m,
			struct gk20a *g, void *args)
{
	uint32_t result;
	uint32_t array1[ARRAY1_SIZE] = {0};
	uint64_t array2[ARRAY2_SIZE] = {0};

	result = ARRAY_SIZE(array1);
	if (result != ARRAY1_SIZE) {
		unit_return_fail(m, "ARRAY SIZE failure %d\n", result);
	}

	result = ARRAY_SIZE(array2);
	if (result != ARRAY2_SIZE) {
		unit_return_fail(m, "ARRAY SIZE failure %d\n", result);
	}

	return UNIT_SUCCESS;
}

int test_typecheck(struct unit_module *m,
			struct gk20a *g, void *args)
{
	uint32_t result;
	unsigned int test1;
	unsigned long  test2;
	signed int test3;
	signed long test4;

	result = IS_UNSIGNED_TYPE(test1);
	if (!result) {
		unit_return_fail(m,
			"IS_UNSIGNED_TYPE failure for uint %d\n", result);
	}

	result = IS_UNSIGNED_TYPE(test2);
	if (!result) {
		unit_return_fail(m,
			"IS_UNSIGNED_TYPE failure for ulong %d\n", result);
	}

	result = IS_UNSIGNED_TYPE(test3);
	if (result) {
		unit_return_fail(m,
			"IS_UNSIGNED_TYPE failure for int %d\n", result);
	}

	result = IS_UNSIGNED_LONG_TYPE(test2);
	if (!result) {
		unit_return_fail(m,
			"IS_UNSIGNED_LONG_TYPE failure for ulong %d\n",
			result);
	}

	result = IS_UNSIGNED_LONG_TYPE(test4);
	if (result) {
		unit_return_fail(m,
			"IS_UNSIGNED_LONG_TYPE failure for long %d\n",
			result);
	}

	result = IS_SIGNED_LONG_TYPE(test2);
	if (result) {
		unit_return_fail(m,
			"IS_SIGNED_LONG_TYPE failure for ulong %d\n",
			result);
	}

	result = IS_SIGNED_LONG_TYPE(test4);
	if (!result) {
		unit_return_fail(m,
			"IS_SIGNED_LONG_TYPE failure for long %d\n",
			result);
	}

	return UNIT_SUCCESS;
}

int test_align_macros(struct unit_module *m,
			struct gk20a *g, void *args)
{
	uint32_t result;
	unsigned int test1;

	test1 = ALIGN_TEST_VALUE;
	result = ALIGN_WITH_VALUE;
	test1 = NVGPU_ALIGN(test1, result);
	if (test1 & (ALIGN_WITH_VALUE - 1)) {
		unit_return_fail(m,
			"ALIGN failure %x\n", test1);
	}

	test1 = ALIGN_TEST_VALUE;
	result = ALIGN_WITH_MASK;
	test1 = ALIGN_MASK(test1, result);
	if (test1 & ALIGN_WITH_MASK) {
		unit_return_fail(m,
			"ALIGN_MASK failure %x\n", test1);
	}

	test1 = PAGE_ALIGN_TEST_VALUE;
	result = PAGE_ALIGN(test1);
	if (result & (NVGPU_CPU_PAGE_SIZE - 1)) {
		unit_return_fail(m,
			"PAGE_ALIGN failure %x\n", result);
	}

	return UNIT_SUCCESS;
}

int test_round_macros(struct unit_module *m,
			struct gk20a *g, void *args)
{
	uint32_t result, i, test1;

	for (i = 1; i < 8; i++) {
		result = 1U << i;
		if (round_mask(test1, result) != (result - 1U)) {
			unit_return_fail(m,
				"round_mask failure %d\n", result);
		}
	}
#ifdef CONFIG_NVGPU_NON_FUSA
	result = ROUND_BY_VALUE;
	for (i = 0; i < ROUND_BY_VALUE; i++) {
		test1 = (ROUND_DOWN_RESULT + 1U) + i;
		if (round_up(test1, result) != ROUND_UP_RESULT) {
			unit_return_fail(m, "round_up failure %d %d\n", test1, i);
		}
	}
#endif

	result = ROUND_BY_VALUE;
	for (i = 0; i < ROUND_BY_VALUE; i++) {
		test1 = (ROUND_UP_RESULT - 1U) - i;
		if (round_down(test1, result) != ROUND_DOWN_RESULT) {
			unit_return_fail(m, "round_down failure\n");
		}
	}

	return UNIT_SUCCESS;
}

int test_write_once(struct unit_module *m,
			struct gk20a *g, void *args)
{
	uint32_t result, i, test1;

	test1 = 20;
	for (i = 0 ; i < 10; i++) {
		test1 += 1;
		NV_WRITE_ONCE(result, test1);
		if (result != test1) {
			unit_return_fail(m,
				"NV_WRITE_ONCE failure %d\n", result);
		}
	}

	return UNIT_SUCCESS;
}

int test_div_macros(struct unit_module *m,
			struct gk20a *g, void *args)
{
	uint32_t result, test1;
	uint64_t test2, test5;

	test1 = 199/20;
	test2 = DIV_ROUND_UP_U64(199, 20);
	if (test2 != (test1 + 1)) {
		unit_return_fail(m,
			"DIV_ROUND_UP_U64 failure %ld\n", test2);
	}

	test1 = 239/40;
	result = DIV_ROUND_UP((uint32_t) 239, (uint32_t) 40);
	if (result != (test1 + 1)) {
		unit_return_fail(m,
			"DIV_ROUND_UP failure %d\n", result);
	}

	test1 = 640;
	result = 100;
	do_div(test1, result);
	if (test1 != 6) {
		unit_return_fail(m,
			"do_div failure %d\n", test1);
	}

	test2 = 800;
	test5 = 200;
	result = div64_u64(test2, test5);
	if (result != (test2/test5)) {
		unit_return_fail(m,
			"div64_u64 failure %d\n", result);
	}

	return UNIT_SUCCESS;
}

int test_containerof(struct unit_module *m,
			struct gk20a *g, void *args)
{
	struct test_container *contptr;
	struct test_container *contptr1;
	struct test_container *contptr2;
	uint32_t *varptr1;
	uint32_t *varptr2;

	contptr = &cont;
	varptr1 = &cont.var1;
	varptr2 = &cont.var2;

	contptr1 = container_of(varptr1, struct test_container, var1);
	contptr2 = container_of(varptr2, struct test_container, var2);

	if ((contptr1 != contptr) || (contptr2 != contptr)) {
		unit_return_fail(m, "container_of failure\n");
	}

	return UNIT_SUCCESS;
}

int test_hertzconversion(struct unit_module *m,
			struct gk20a *g, void *args)
{
	uint32_t i, hz, khz, mhz;
	uint64_t long_hz;

	for (i = 1; i < 10; i++) {
		hz = i * 1000U;
		khz = HZ_TO_KHZ(hz);
		if (khz != i) {
			unit_return_fail(m, "HZ_TO_KHZ failure\n");
		}

		if (hz != KHZ_TO_HZ(i)) {
			unit_return_fail(m, "KHZ_TO_HZ failure\n");
		}

		hz = i * 1000000U;
		mhz = HZ_TO_MHZ(hz);
		if (mhz != i) {
			unit_return_fail(m, "HZ_TO_MHZ failure\n");
		}

		long_hz = i * 1000000U;
		mhz = HZ_TO_MHZ_ULL(long_hz);
		if (mhz != i) {
			unit_return_fail(m, "HZ_TO_MHZ_ULL failure\n");
		}
	}

	for (i = 0; i < 10; i++) {
		khz = i * 1000U;
		mhz = KHZ_TO_MHZ(khz);
		if (mhz != i) {
			unit_return_fail(m, "KHZ_TO_MHZ failure\n");
		}

		if (khz != MHZ_TO_KHZ(i)) {
			unit_return_fail(m, "MHZ_TO_KHZ failure\n");
		}
	}

	for (i = 0; i < 10; i++) {
		hz = i * 1000000;
		if (hz != MHZ_TO_HZ_ULL(i)) {
			unit_return_fail(m, "MHZ_TO_HZ_ULL failure\n");
		}
	}

	return UNIT_SUCCESS;
}
struct unit_module_test posix_utils_tests[] = {
	UNIT_TEST(hweight_test, test_hamming_weight, NULL, 0),
	UNIT_TEST(be32tocpu_test, test_be32tocpu, NULL, 0),
	UNIT_TEST(minmax_test, test_minmax, NULL, 0),
	UNIT_TEST(arraysize_test, test_arraysize, NULL, 0),
	UNIT_TEST(typecheck_test, test_typecheck, NULL, 0),
	UNIT_TEST(alignmacros_test, test_align_macros, NULL, 0),
	UNIT_TEST(roundmacros_test, test_round_macros, NULL, 0),
	UNIT_TEST(writeonce_test, test_write_once, NULL, 0),
	UNIT_TEST(divmacros_test, test_div_macros, NULL, 0),
	UNIT_TEST(containerof_test, test_containerof, NULL, 0),
	UNIT_TEST(conversion_test, test_hertzconversion, NULL, 0),
};

UNIT_MODULE(posix_utils, posix_utils_tests, UNIT_PRIO_POSIX_TEST);
