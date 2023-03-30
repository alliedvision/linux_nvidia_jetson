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

#include <nvgpu/gk20a.h>
#include <nvgpu/static_analysis.h>

#include "static_analysis.h"

/**
 * unsigned addition tests
 *
 * parameters:
 *   type_name: type (u8, u32 etc.)
 *   type_max : Maximum value of the type.
 *   tmp_operand: random value in the set (1, Maximum value)
 *
 * Boundary values: (0, 1, max-1, max)
 *
 * Valid tests: Addition result within range for each boundary value and
 *              random value.
 * Invalid tests: Addition result out of range if possible for each
 *                boundary and random value.
 */
#define GENERATE_ARITHMETIC_ADD_TESTS(type_name, type_max, tmp_operand) do {\
	unit_assert(nvgpu_safe_add_##type_name(type_max, 0) == type_max, return UNIT_FAIL); \
	unit_assert(nvgpu_safe_add_##type_name(type_max - 1, 1) == type_max, return UNIT_FAIL); \
	unit_assert(nvgpu_safe_add_##type_name(type_max - tmp_operand, tmp_operand) == type_max, \
							return UNIT_FAIL); \
	err = EXPECT_BUG((void)nvgpu_safe_add_##type_name(1, type_max)); \
	unit_assert(err != 0, return UNIT_FAIL); \
	err = EXPECT_BUG((void)nvgpu_safe_add_##type_name(tmp_operand, type_max - tmp_operand + 1)); \
	unit_assert(err != 0, return UNIT_FAIL); \
	err = EXPECT_BUG((void)nvgpu_safe_add_##type_name(type_max - 1, tmp_operand > 1 ? tmp_operand : 2)); \
	unit_assert(err != 0, return UNIT_FAIL); \
} while (0)

/**
 * wrapping unsigned addition tests
 *
 * parameters:
 *   type_name: type (u8, u32 etc.)
 *   type_max : Maximum value of the type.
 *   tmp_operand: random value in the set (1, Maximum value)
 *
 * Boundary values: (0, 1, max-1, max)
 *
 * Valid tests: Addition result within range for each boundary value and
 *              random value. Addition result wrapping for each boundary
 *              and random value.
 */
#define GENERATE_ARITHMETIC_WRAPPING_ADD_TESTS(type_name, type_max, tmp_operand) do {\
	unit_assert(nvgpu_wrapping_add_##type_name(type_max, 0) == type_max, return UNIT_FAIL); \
	unit_assert(nvgpu_wrapping_add_##type_name(type_max - 1, 1) == type_max, return UNIT_FAIL); \
	unit_assert(nvgpu_wrapping_add_##type_name(type_max - tmp_operand, tmp_operand) == type_max, \
							return UNIT_FAIL); \
	unit_assert(nvgpu_wrapping_add_##type_name(1, type_max) == 0, return UNIT_FAIL); \
	unit_assert(nvgpu_wrapping_add_##type_name(tmp_operand, type_max - tmp_operand + 1) == 0, return UNIT_FAIL); \
	unit_assert(nvgpu_wrapping_add_##type_name(type_max - 1, 2) == 0, return UNIT_FAIL); \
	unit_assert(nvgpu_wrapping_add_##type_name(type_max, type_max) == (type_max - 1), return UNIT_FAIL); \
} while (0)

/**
 * signed addition tests
 *
 * parameters:
 *   type_name: type (s32, s64 etc.)
 *   type_min:  Minimum value of the type.
 *   type_max : Maximum value of the type.
 *   tmp_operand1: random positive value in the set (1, Maximum value / 2)
 *   tmp_operand2: random negative value in the set (-1, Minimum value / 2)
 *
 * Boundary values: (min, min+1, -1, 0, 1, max-1, max)
 *
 * Valid tests: Addition result within range for each boundary value and
 *              random value.
 * Invalid tests: Addition result out of range if possible for each
 *                boundary and random value.
 */
#define GENERATE_ARITHMETIC_SIGNED_ADD_TESTS(type_name, type_min, type_max, tmp_operand1, tmp_operand2) do {\
	unit_assert(nvgpu_safe_add_##type_name(type_min, type_max) == -1, \
							return UNIT_FAIL); \
	unit_assert(nvgpu_safe_add_##type_name(0, type_max) == type_max, \
							return UNIT_FAIL); \
	unit_assert(nvgpu_safe_add_##type_name(-1, -1) == -2, \
							return UNIT_FAIL); \
	unit_assert(nvgpu_safe_add_##type_name(-1, 1) == 0, \
							return UNIT_FAIL); \
	unit_assert(nvgpu_safe_add_##type_name(1, 1) == 2, \
							return UNIT_FAIL); \
	unit_assert(nvgpu_safe_add_##type_name(type_max - tmp_operand1, tmp_operand1) == type_max, \
							return UNIT_FAIL); \
	unit_assert(nvgpu_safe_add_##type_name(type_min - tmp_operand2, tmp_operand2) == type_min, \
							return UNIT_FAIL); \
	unit_assert(nvgpu_safe_add_##type_name(tmp_operand1, tmp_operand2) == tmp_operand1 + tmp_operand2, \
							return UNIT_FAIL); \
	unit_assert(nvgpu_safe_add_##type_name(tmp_operand1, type_min + 1) == tmp_operand1 + type_min + 1, \
							return UNIT_FAIL); \
	unit_assert(nvgpu_safe_add_##type_name(type_max - 1, tmp_operand2) == type_max - 1 + tmp_operand2, \
							return UNIT_FAIL); \
	err = EXPECT_BUG((void)nvgpu_safe_add_##type_name(type_max - tmp_operand1 + 1, tmp_operand1)); \
	unit_assert(err != 0, return UNIT_FAIL); \
	err = EXPECT_BUG((void)nvgpu_safe_add_##type_name(type_max, tmp_operand1)); \
	unit_assert(err != 0, return UNIT_FAIL); \
	err = EXPECT_BUG((void)nvgpu_safe_add_##type_name(type_max, type_max)); \
	unit_assert(err != 0, return UNIT_FAIL); \
	err = EXPECT_BUG((void)nvgpu_safe_add_##type_name(type_min - tmp_operand2 - 1, tmp_operand2)); \
	unit_assert(err != 0, return UNIT_FAIL); \
	err = EXPECT_BUG((void)nvgpu_safe_add_##type_name(type_min, tmp_operand2)); \
	unit_assert(err != 0, return UNIT_FAIL); \
	err = EXPECT_BUG((void)nvgpu_safe_add_##type_name(type_min, type_min)); \
	unit_assert(err != 0, return UNIT_FAIL); \
	err = EXPECT_BUG((void)nvgpu_safe_add_##type_name(type_min + 1, type_min + 1)); \
	unit_assert(err != 0, return UNIT_FAIL); \
	err = EXPECT_BUG((void)nvgpu_safe_add_##type_name(type_max - 1, type_max - 1)); \
	unit_assert(err != 0, return UNIT_FAIL); \
	err = EXPECT_BUG((void)nvgpu_safe_add_##type_name(type_max - 1, tmp_operand1 > 1 ? tmp_operand1 : 2)); \
	unit_assert(err != 0, return UNIT_FAIL); \
	err = EXPECT_BUG((void)nvgpu_safe_add_##type_name(type_min + 1, tmp_operand2 < -1 ? tmp_operand2 : -2)); \
	unit_assert(err != 0, return UNIT_FAIL); \
} while (0)

/**
 * unsigned subtraction tests
 *
 * parameters:
 *   type_name: type (u8, u32 etc.)
 *   type_max : Maximum value of the type.
 *   tmp_operand: random value in the set (1, Maximum value)
 *
 * Boundary values: (0, 1, max-1, max)
 *
 * Valid tests: Subtraction result within range for each boundary value and
 *              random value.
 * Invalid tests: Subtraction result out of range if possible for each
 *                boundary and random value.
 */
#define GENERATE_ARITHMETIC_SUBTRACT_TESTS(type_name, type_max, tmp_operand) do {\
	unit_assert(nvgpu_safe_sub_##type_name(0, 0) == 0, return UNIT_FAIL); \
	unit_assert(nvgpu_safe_sub_##type_name(1, 0) == 1, return UNIT_FAIL); \
	unit_assert(nvgpu_safe_sub_##type_name(type_max, tmp_operand) == type_max - tmp_operand, \
							return UNIT_FAIL); \
	unit_assert(nvgpu_safe_sub_##type_name(tmp_operand, 0) == tmp_operand, \
							return UNIT_FAIL); \
	unit_assert(nvgpu_safe_sub_##type_name(type_max, type_max - 1) == 1, \
							return UNIT_FAIL); \
	unit_assert(nvgpu_safe_sub_##type_name(type_max - 1, 1) == type_max - 2, \
							return UNIT_FAIL); \
	err = EXPECT_BUG((void)nvgpu_safe_sub_##type_name(0, 1)); \
	unit_assert(err != 0, return UNIT_FAIL); \
	err = EXPECT_BUG((void)nvgpu_safe_sub_##type_name(0, tmp_operand)); \
	unit_assert(err != 0, return UNIT_FAIL); \
	err = EXPECT_BUG((void)nvgpu_safe_sub_##type_name(0, type_max)); \
	unit_assert(err != 0, return UNIT_FAIL); \
	err = EXPECT_BUG((void)nvgpu_safe_sub_##type_name(tmp_operand, type_max)); \
	unit_assert(err != 0, return UNIT_FAIL); \
	err = EXPECT_BUG((void)nvgpu_safe_sub_##type_name(type_max - 1, type_max)); \
	unit_assert(err != 0, return UNIT_FAIL); \
} while (0)

/**
 * signed subtraction tests
 *
 * parameters:
 *   type_name: type (s32, s64 etc.)
 *   type_min:  Minimum value of the type.
 *   type_max : Maximum value of the type.
 *   tmp_operand1: random positive value in the set (1, Maximum value / 2)
 *   tmp_operand2: random negative value in the set (-1, Minimum value / 2)
 *
 * Boundary values: (min, min+1, -1, 0, 1, max-1, max)
 *
 * Valid tests: Subtraction result within range for each boundary value and
 *              random value.
 * Invalid tests: Subtraction result out of range if possible for each
 *                boundary and random value.
 */
#define GENERATE_ARITHMETIC_SIGNED_SUBTRACT_TESTS(type_name, type_min, type_max, tmp_operand1, tmp_operand2) do {\
	unit_assert(nvgpu_safe_sub_##type_name(tmp_operand2, tmp_operand2) == 0, return UNIT_FAIL); \
	unit_assert(nvgpu_safe_sub_##type_name(tmp_operand2, tmp_operand1) == tmp_operand2 - tmp_operand1, return UNIT_FAIL); \
	unit_assert(nvgpu_safe_sub_##type_name(tmp_operand1, tmp_operand2) == tmp_operand1 - tmp_operand2, return UNIT_FAIL); \
	unit_assert(nvgpu_safe_sub_##type_name(0, 0) == 0, return UNIT_FAIL); \
	unit_assert(nvgpu_safe_sub_##type_name(0, type_max) == 0 - type_max, return UNIT_FAIL); \
	unit_assert(nvgpu_safe_sub_##type_name(type_max, 0) == type_max, return UNIT_FAIL); \
	unit_assert(nvgpu_safe_sub_##type_name(-1, -1) == 0, return UNIT_FAIL); \
	unit_assert(nvgpu_safe_sub_##type_name(-1, 1) == -2, return UNIT_FAIL); \
	unit_assert(nvgpu_safe_sub_##type_name(1, -1) == 2, return UNIT_FAIL); \
	unit_assert(nvgpu_safe_sub_##type_name(1, 1) == 0, return UNIT_FAIL); \
	unit_assert(nvgpu_safe_sub_##type_name(type_min + 1, type_min + 1) == 0, return UNIT_FAIL); \
	unit_assert(nvgpu_safe_sub_##type_name(type_min, type_min) == 0, return UNIT_FAIL); \
	unit_assert(nvgpu_safe_sub_##type_name(type_max - 1, type_max - 1) == 0, return UNIT_FAIL); \
	unit_assert(nvgpu_safe_sub_##type_name(type_max, type_max) == 0, return UNIT_FAIL); \
	unit_assert(nvgpu_safe_sub_##type_name(type_min + 1, tmp_operand2) == type_min + 1 - tmp_operand2, return UNIT_FAIL); \
	unit_assert(nvgpu_safe_sub_##type_name(type_max - 1, tmp_operand1) == type_max - 1 - tmp_operand1, return UNIT_FAIL); \
	unit_assert(nvgpu_safe_sub_##type_name(tmp_operand2, tmp_operand2 - type_min) == type_min, return UNIT_FAIL); \
	unit_assert(nvgpu_safe_sub_##type_name(type_max, type_max - tmp_operand1) == tmp_operand1, return UNIT_FAIL); \
	err = EXPECT_BUG((void)nvgpu_safe_sub_##type_name(type_min, tmp_operand1)); \
	unit_assert(err != 0, return UNIT_FAIL); \
	err = EXPECT_BUG((void)nvgpu_safe_sub_##type_name(type_max, tmp_operand2)); \
	unit_assert(err != 0, return UNIT_FAIL); \
	err = EXPECT_BUG((void)nvgpu_safe_sub_##type_name(type_max - 1, type_min + 1)); \
	unit_assert(err != 0, return UNIT_FAIL); \
	err = EXPECT_BUG((void)nvgpu_safe_sub_##type_name(type_min + 1, type_max - 1)); \
	unit_assert(err != 0, return UNIT_FAIL); \
	err = EXPECT_BUG((void)nvgpu_safe_sub_##type_name(type_max - 1, tmp_operand2 < -1 ? tmp_operand2 : -2)); \
	unit_assert(err != 0, return UNIT_FAIL); \
	err = EXPECT_BUG((void)nvgpu_safe_sub_##type_name(type_min + 1, tmp_operand1 > 1 ? tmp_operand1 : 2)); \
	unit_assert(err != 0, return UNIT_FAIL); \
	err = EXPECT_BUG((void)nvgpu_safe_sub_##type_name(0, type_min)); \
	unit_assert(err != 0, return UNIT_FAIL); \
	err = EXPECT_BUG((void)nvgpu_safe_sub_##type_name(type_min, type_max)); \
	unit_assert(err != 0, return UNIT_FAIL); \
	err = EXPECT_BUG((void)nvgpu_safe_sub_##type_name(type_max, type_min)); \
	unit_assert(err != 0, return UNIT_FAIL); \
} while (0)

/**
 * unsigned multiplication tests
 *
 * parameters:
 *   type_name: type (u8, u32 etc.)
 *   type_max : Maximum value of the type.
 *   tmp_operand: random value in the set (1, Maximum value / 2)
 *
 * Boundary values: (0, 1, max-1, max)
 *
 * Valid tests: Multiplication result within range for each boundary value and
 *              random value.
 * Invalid tests: Multiplication result out of range if possible for each
 *                boundary and random value.
 */
#define GENERATE_ARITHMETIC_MULT_TESTS(type_name, type_max, tmp_operand) do {\
	unit_assert(nvgpu_safe_mult_##type_name(0, type_max) == 0, return UNIT_FAIL); \
	unit_assert(nvgpu_safe_mult_##type_name(type_max - 1, 1) == type_max - 1, return UNIT_FAIL); \
	unit_assert(nvgpu_safe_mult_##type_name(tmp_operand, 2) == tmp_operand * 2, return UNIT_FAIL); \
	err = EXPECT_BUG((void)nvgpu_safe_mult_##type_name(type_max - 1, 2)); \
	unit_assert(err != 0, return UNIT_FAIL); \
	err = EXPECT_BUG((void)nvgpu_safe_mult_##type_name(type_max - 1, tmp_operand > 1 ? tmp_operand : 2)); \
	unit_assert(err != 0, return UNIT_FAIL); \
	err = EXPECT_BUG((void)nvgpu_safe_mult_##type_name(type_max, type_max)); \
	unit_assert(err != 0, return UNIT_FAIL); \
} while (0)

/**
 * signed multiplication tests
 *
 * parameters:
 *   type_name: type (s32, s64 etc.)
 *   type_min:  Minimum value of the type.
 *   type_max : Maximum value of the type.
 *   tmp_operand1: random positive value in the set (1, Maximum value / 2)
 *   tmp_operand2: random negative value in the set (-1, Minimum value / 2)
 *
 * Boundary values: (min, min+1, -1, 0, 1, max-1, max)
 *
 * Valid tests: Multiplication result within range for each boundary value and
 *              random value.
 * Invalid tests: Multiplication result out of range if possible for each
 *                boundary and random value.
 */
#define GENERATE_ARITHMETIC_SIGNED_MULT_TESTS(type_name, type_min, type_max, tmp_operand1, tmp_operand2) do {\
	unit_assert(nvgpu_safe_mult_##type_name(0, type_max) == 0, return UNIT_FAIL); \
	unit_assert(nvgpu_safe_mult_##type_name(1, type_min) == type_min, return UNIT_FAIL); \
	unit_assert(nvgpu_safe_mult_##type_name(-1, -1) == 1, return UNIT_FAIL); \
	unit_assert(nvgpu_safe_mult_##type_name(-1, 1) == -1, return UNIT_FAIL); \
	unit_assert(nvgpu_safe_mult_##type_name(1, 1) == 1, return UNIT_FAIL); \
	unit_assert(nvgpu_safe_mult_##type_name(tmp_operand1, 2) == tmp_operand1 * 2, return UNIT_FAIL); \
	unit_assert(nvgpu_safe_mult_##type_name(tmp_operand2, 2) == tmp_operand2 * 2, return UNIT_FAIL); \
	unit_assert(nvgpu_safe_mult_##type_name(type_max, -1) == type_max * -1, return UNIT_FAIL); \
	unit_assert(nvgpu_safe_mult_##type_name(type_max - 1, -1) == (type_max - 1) * -1, return UNIT_FAIL); \
	unit_assert(nvgpu_safe_mult_##type_name(type_min + 1, -1) == (type_min + 1) * -1, return UNIT_FAIL); \
	err = EXPECT_BUG((void)nvgpu_safe_mult_##type_name(type_max, 2)); \
	unit_assert(err != 0, return UNIT_FAIL); \
	err = EXPECT_BUG((void)nvgpu_safe_mult_##type_name(type_max, -2)); \
	unit_assert(err != 0, return UNIT_FAIL); \
	err = EXPECT_BUG((void)nvgpu_safe_mult_##type_name(type_min, 2)); \
	unit_assert(err != 0, return UNIT_FAIL); \
	err = EXPECT_BUG((void)nvgpu_safe_mult_##type_name(type_min, -1)); \
	unit_assert(err != 0, return UNIT_FAIL); \
	err = EXPECT_BUG((void)nvgpu_safe_mult_##type_name(type_min, type_min)); \
	unit_assert(err != 0, return UNIT_FAIL); \
	err = EXPECT_BUG((void)nvgpu_safe_mult_##type_name(type_max, type_max)); \
	unit_assert(err != 0, return UNIT_FAIL); \
	err = EXPECT_BUG((void)nvgpu_safe_mult_##type_name(type_min, type_max)); \
	unit_assert(err != 0, return UNIT_FAIL); \
	err = EXPECT_BUG((void)nvgpu_safe_mult_##type_name(type_min + 1, type_min + 1)); \
	unit_assert(err != 0, return UNIT_FAIL); \
	err = EXPECT_BUG((void)nvgpu_safe_mult_##type_name(type_max - 1, type_max - 1)); \
	unit_assert(err != 0, return UNIT_FAIL); \
	err = EXPECT_BUG((void)nvgpu_safe_mult_##type_name(type_min + 1, type_max - 1)); \
	unit_assert(err != 0, return UNIT_FAIL); \
} while (0)

int test_arithmetic(struct unit_module *m, struct gk20a *g, void *args)
{
	s64 tmp_s64, tmp_s64_neg;
	s32 tmp_s32, tmp_s32_neg;
	u64 tmp_u64;
	u32 tmp_u32;
	u8 tmp_u8;
	int err;

	srand(time(0));

	/* random value in the set (1, U8_MAX - 1) */
	tmp_u8 = rand() % U8_MAX ?: 1;

	/* random value in the set ((u32)1, (u32)(U32_MAX - 1)) */
	tmp_u32 = (u32)((rand() % U32_MAX) ?: 1);

	/* random value in the set ((s32)1, (s32)(U32_MAX - 1) / 2) */
	tmp_s32 = (s32)((rand() % U32_MAX) / 2 ?: 1);

	/* random value in the set ((s32)-1, (s32)-((U32_MAX - 1) / 2)) */
	tmp_s32_neg = (s32)(0 - (rand() % U32_MAX) / 2) ?: -1;

	/* random value in the set ((u64)1, (u64)(U64_MAX - 1)) */
	tmp_u64 = (u64)((rand() % U64_MAX) ?: 1);

	/* random value in the set ((s64)1, (s64)(U64_MAX - 1) / 2) */
	tmp_s64 = (s64)((rand() % U64_MAX) / 2 ?: 1);

	/* random value in the set ((s64)-1, (s64)-((U64_MAX - 1) / 2)) */
	tmp_s64_neg = (s64)(0 - (rand() % U64_MAX) / 2) ?: -1;

	unit_info(m, "random operands\nu8: %u, u32: %u, s32: %d, s32_neg: %d\nu64: %llu, s64: %lld, s64_neg: %lld\n",
		  tmp_u8, tmp_u32, tmp_s32, tmp_s32_neg, tmp_u64, tmp_s64, tmp_s64_neg);

	/* U8 sub */
	GENERATE_ARITHMETIC_SUBTRACT_TESTS(u8, U8_MAX, tmp_u8);

	/* U32 add */
	GENERATE_ARITHMETIC_ADD_TESTS(u32, U32_MAX, tmp_u32);

	/* wrapping U32 add */
	GENERATE_ARITHMETIC_WRAPPING_ADD_TESTS(u32, U32_MAX, tmp_u32);

	/* S32 add */
	GENERATE_ARITHMETIC_SIGNED_ADD_TESTS(s32, INT_MIN, INT_MAX, tmp_s32, tmp_s32_neg);

	/* U32 sub */
	GENERATE_ARITHMETIC_SUBTRACT_TESTS(u32, U32_MAX, tmp_u32);

	/* S32 sub */
	GENERATE_ARITHMETIC_SIGNED_SUBTRACT_TESTS(s32, INT_MIN, INT_MAX, tmp_s32, tmp_s32_neg);

	/* U32 Mult */
	GENERATE_ARITHMETIC_MULT_TESTS(u32, U32_MAX, tmp_u32);

	/* U64 add */
	GENERATE_ARITHMETIC_ADD_TESTS(u64, U64_MAX, tmp_u64);

	/* S64 add */
	GENERATE_ARITHMETIC_SIGNED_ADD_TESTS(s64, LONG_MIN, LONG_MAX, tmp_s64, tmp_s64_neg);

	/* U64 sub */
	GENERATE_ARITHMETIC_SUBTRACT_TESTS(u64, U64_MAX, tmp_u64);

	/* S64 sub */
	GENERATE_ARITHMETIC_SIGNED_SUBTRACT_TESTS(s64, LONG_MIN, LONG_MAX, tmp_s64, tmp_s64_neg);

	/* U64 Mult */
	GENERATE_ARITHMETIC_MULT_TESTS(u64, U64_MAX, tmp_u64);

	/* S64 Mult */
	GENERATE_ARITHMETIC_SIGNED_MULT_TESTS(s64, LONG_MIN, LONG_MAX, tmp_s64, tmp_s64_neg);

	return UNIT_SUCCESS;
}

/**
 * unsigned to unsigned cast tests
 *
 * parameters:
 *   type1: type to cast from (u32, u64)
 *   type2: type to cast to (u8, u16, u32)
 *   type1_max:  Maximum value of the type1.
 *   type2_max : Maximum value of the type2.
 *
 * Boundary values: (0, type2 max, type1 max)
 *
 * Valid tests: Cast result within range for each valid boundary value and
 *              random value.
 * Invalid tests: Cast result out of range if possible for each invalid
 *                boundary and random value.
 */
#define GENERATE_UNSIGNED_CAST_TESTS(type1, type2, type1_max, type2_max) { \
	type1 temp; \
	type1 valid_##type2[] = {0, 1, (temp = rand() % type2_max) > 1 ? temp : 2, type2_max - 1, type2_max}; \
	type1 invalid_##type2[] = {(type1)type2_max + 1, (type1)type2_max + 2,\
				   (type1)type2_max + ((temp = rand() % (type1_max - type2_max)) > 2 ? temp : 3),\
				   type1_max - 1, type1_max}; \
	for (i = 0; i < ARRAY_SIZE(valid_##type2); i++) { \
		unit_assert(nvgpu_safe_cast_##type1##_to_##type2(valid_##type2[i]) == (type2)valid_##type2[i], return UNIT_FAIL); \
	} \
	for (i = 0; i < ARRAY_SIZE(invalid_##type2); i++) { \
		err = EXPECT_BUG((void)nvgpu_safe_cast_##type1##_to_##type2(invalid_##type2[i])); \
		unit_assert(err != 0, return UNIT_FAIL); \
	} \
}

/**
 * unsigned to signed cast tests
 *
 * parameters:
 *   type1: type to cast from (u32, u64)
 *   type2: type to cast to (s8, s32, s64)
 *   type1_max:  Maximum value of the type1.
 *   type2_min : Minimum value of the type2.
 *   type2_max : Maximum value of the type2.
 *
 * Boundary values: (type2 min, 0, type2 max, type1 max)
 *
 * Valid tests: Cast result within range for each valid boundary value and
 *              random value.
 * Invalid tests: Cast result out of range if possible for each invalid
 *                boundary and random value.
 */
#define GENERATE_SIGNED_CAST_TESTS(type1, type2, type1_max, type2_min, type2_max) { \
	type2 temp; \
	type1 valid_##type2[] = {0, 1, (temp = rand() % type2_max) > 1 ? temp : 2, type2_max - 1, type2_max}; \
	type1 invalid_##type2[] = {(type1)type2_min, (type1)type2_min + 1, \
				   (type1)type2_min + ((temp = rand() % type2_min) > 1 ? temp : 2), -1, \
				   (type1)type2_max + 1, (type1)type2_max + 2, \
				   (type1)type2_max + ((temp = rand() % (type1_max - type2_max)) > 2 ? temp : 3), \
				   type1_max - 1, type1_max}; \
	for (i = 0; i < ARRAY_SIZE(valid_##type2); i++) { \
		unit_assert(nvgpu_safe_cast_##type1##_to_##type2(valid_##type2[i]) == (type2)valid_##type2[i], return UNIT_FAIL); \
	} \
	for (i = 0; i < ARRAY_SIZE(invalid_##type2); i++) { \
		err = EXPECT_BUG((void)nvgpu_safe_cast_##type1##_to_##type2(invalid_##type2[i])); \
		unit_assert(err != 0, return UNIT_FAIL); \
	} \
}

/**
 * signed to unsigned cast tests
 *
 * parameters:
 *   type1: type to cast to (s8, s32, s64)
 *   type2: type to cast from (u8, u32, u64)
 *   type1_min : Minimum value of the type1.
 *   type1_max : Maximum value of the type1.
 *
 * Boundary values: (type1 min, 0, type1 max)
 *
 * Assumption: the range of non-negative values from type1 are subset of those
 *             from type2.
 *
 * Valid tests: Cast result within range for each valid boundary value and
 *              random value.
 * Invalid tests: Cast result out of range if possible for each invalid
 *                boundary and random value.
 */
#define GENERATE_SIGNED_TO_UNSIGNED_CAST_TESTS(type1, type2, type1_min, type1_max) { \
	type1 temp;\
	type1 valid_##type2[] = {0, 1, (temp = rand() % type1_max) > 1 ? temp : 2, type1_max - 1, type1_max}; \
	type1 invalid_##type2[] = {type1_min, type1_min + 1, type1_min + ((temp = rand() % type1_min) > 1 ? temp : 2), -1}; \
	for (i = 0; i < ARRAY_SIZE(valid_##type2); i++) { \
		unit_assert(nvgpu_safe_cast_##type1##_to_##type2(valid_##type2[i]) == (type2)valid_##type2[i], return UNIT_FAIL); \
	} \
	for (i = 0; i < ARRAY_SIZE(invalid_##type2); i++) { \
		err = EXPECT_BUG((void)nvgpu_safe_cast_##type1##_to_##type2(invalid_##type2[i])); \
		unit_assert(err != 0, return UNIT_FAIL); \
	} \
}

int test_cast(struct unit_module *m, struct gk20a *g, void *args)
{
	s64 random_s64;
	/**
	 * s64 to u32 cast tests
	 *
	 * Boundary values: (LONG_MIN, 0, U32_MAX, LONG_MAX)
	 *
	 * Valid tests: Cast result within range for each valid boundary value and
	 *              random value.
	 * Invalid tests: Cast result out of range if possible for each invalid
	 *                boundary and random value.
	 */
	s64 valid_s64_u32[] = {0, 1, (random_s64 = rand() % U32_MAX) > 1 ? random_s64 : 2, U32_MAX - 1, U32_MAX};
	s64 invalid_s64_u32[] = {LONG_MIN, LONG_MIN + 1,
				 LONG_MIN + ((random_s64 = rand() % LONG_MIN) > 1 ? random_s64 : 2), -1,
				 (s64)U32_MAX + 1, (s64)U32_MAX + 2,
				 (s64)U32_MAX + ((random_s64 = rand() % LONG_MAX) > 1 ? random_s64 : 3),
				 LONG_MAX - 1, LONG_MAX};

	/**
	 * s64 to s32 cast tests
	 *
	 * Boundary values: (LONG_MIN, INT_MIN, 0, INT_MAX, LONG_MAX)
	 *
	 * Valid tests: Cast result within range for each valid boundary value and
	 *              random value.
	 * Invalid tests: Cast result out of range if possible for each invalid
	 *                boundary and random value.
	 */
	s64 valid_s64_s32[] = {INT_MIN, INT_MIN + 1,
			       INT_MIN + ((random_s64 = rand() % INT_MIN) > 1 ? random_s64 : 2), -1, 0,
			       1, (random_s64 = rand() % INT_MAX) > 1 ? random_s64 : 2, INT_MAX - 1, INT_MAX};
	s64 invalid_s64_s32[] = {LONG_MIN, LONG_MIN + 1,
				 LONG_MIN + ((random_s64 = rand() % (INT_MIN - LONG_MIN)) > 1 ? random_s64 : 2),
				 (s64)INT_MIN - 2, (s64)INT_MIN - 1, (s64)INT_MAX + 1, (s64)INT_MAX + 2,
				 (s64)INT_MAX + ((random_s64 = rand() % (LONG_MAX - INT_MAX)) > 1 ? random_s64 : 3),
				 LONG_MAX - 1, LONG_MAX};
	int err;
	u32 i;

	/* U64 to U32 */
	GENERATE_UNSIGNED_CAST_TESTS(u64, u32, U64_MAX, U32_MAX);

	/* U64 to U16 */
	GENERATE_UNSIGNED_CAST_TESTS(u64, u16, U64_MAX, U16_MAX);

	/* U64 to U8 */
	GENERATE_UNSIGNED_CAST_TESTS(u64, u8, U64_MAX, U8_MAX);

	/* U32 to U16 */
	GENERATE_UNSIGNED_CAST_TESTS(u32, u16, U32_MAX, U16_MAX);

	/* U32 to U8 */
	GENERATE_UNSIGNED_CAST_TESTS(u32, u8, U32_MAX, U8_MAX);

	/* U64 to S64 */
	GENERATE_SIGNED_CAST_TESTS(u64, s64, U64_MAX, LONG_MIN, LONG_MAX);

	/* U64 to S32 */
	GENERATE_SIGNED_CAST_TESTS(u64, s32, U64_MAX, INT_MIN, INT_MAX);

	/* U32 to S32 */
	GENERATE_SIGNED_CAST_TESTS(u32, s32, U32_MAX, INT_MIN, INT_MAX);

	/* U32 to S8 */
	GENERATE_SIGNED_CAST_TESTS(u32, s8, U32_MAX, SCHAR_MIN, SCHAR_MAX);

	/* S64 to U64 */
	GENERATE_SIGNED_TO_UNSIGNED_CAST_TESTS(s64, u64, LONG_MIN, LONG_MAX);

	/* S32 to U64 */
	GENERATE_SIGNED_TO_UNSIGNED_CAST_TESTS(s32, u64, INT_MIN, INT_MAX);

	/* S32 to U32 */
	GENERATE_SIGNED_TO_UNSIGNED_CAST_TESTS(s32, u32, INT_MIN, INT_MAX);

	/* S8 to U8 */
	GENERATE_SIGNED_TO_UNSIGNED_CAST_TESTS(s8, u8, SCHAR_MIN, SCHAR_MAX);

	/* S64 to U32 */
	for (i = 0; i < ARRAY_SIZE(valid_s64_u32); i++) {
		unit_assert(nvgpu_safe_cast_s64_to_u32(valid_s64_u32[i]) == (u32)valid_s64_u32[i], return UNIT_FAIL);
	}

	for (i = 0; i < ARRAY_SIZE(invalid_s64_u32); i++) {
		err = EXPECT_BUG((void)nvgpu_safe_cast_s64_to_u32(invalid_s64_u32[i]));
		unit_assert(err != 0, return UNIT_FAIL);
	}

	/* S64 to S32 */
	for (i = 0; i < ARRAY_SIZE(valid_s64_s32); i++) {
		unit_assert(nvgpu_safe_cast_s64_to_s32(valid_s64_s32[i]) == (s32)valid_s64_s32[i], return UNIT_FAIL);
	}

	for (i = 0; i < ARRAY_SIZE(invalid_s64_s32); i++) {
		err = EXPECT_BUG((void)nvgpu_safe_cast_s64_to_s32(invalid_s64_s32[i]));
		unit_assert(err != 0, return UNIT_FAIL);
	}

	/* bool to U32 */
	unit_assert(nvgpu_safe_cast_bool_to_u32(false) == 0, return UNIT_FAIL);
	unit_assert(nvgpu_safe_cast_bool_to_u32(true) == 1, return UNIT_FAIL);

	return  UNIT_SUCCESS;
}

int test_safety_checks(struct unit_module *m, struct gk20a *g, void *args)
{
	nvgpu_safety_checks();

	return  UNIT_SUCCESS;
}

struct unit_module_test static_analysis_tests[] = {
	UNIT_TEST(arithmetic,		test_arithmetic,	NULL, 0),
	UNIT_TEST(cast,			test_cast,		NULL, 0),
	UNIT_TEST(safety_checks,	test_safety_checks,	NULL, 0),
};

UNIT_MODULE(static_analysis, static_analysis_tests, UNIT_PRIO_NVGPU_TEST);
