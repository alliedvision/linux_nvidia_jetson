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

#ifndef UNIT_STATIC_ANALYSIS_H
#define UNIT_STATIC_ANALYSIS_H

struct gk20a;
struct unit_module;

/** @addtogroup SWUTS-interface-static_analysis
 *  @{
 *
 * Software Unit Test Specification for static analysis unit
 */

/**
 * Test specification for: test_arithmetic
 *
 * Description: Verify functionality of static analysis safe arithmetic APIs.
 *
 * Test Type: Feature, Error guessing, Boundary Value
 *
 * Targets: nvgpu_safe_sub_u8, nvgpu_safe_add_u32, nvgpu_safe_add_s32,
 *          nvgpu_safe_sub_u32, nvgpu_safe_sub_s32, nvgpu_safe_mult_u32,
 *          nvgpu_safe_add_u64, nvgpu_safe_add_s64, nvgpu_safe_sub_u64,
 *          nvgpu_safe_sub_s64, nvgpu_safe_mult_u64, nvgpu_safe_mult_s64,
 *          nvgpu_wrapping_add_u32
 *
 * Input: None
 *
 * -# Unsigned addition tests:
 *    Boundary values: {0, max}
 *
 *    Equivalence classes:
 *    Variable: Addition result of two unsigned operands.
 *    - Valid tests: Addition result within range for each boundary value and
 *                   random value in the set.
 *    - Invalid tests: Addition result out of range if possible for each
 *                     boundary and random value.
 *
 * -# Signed addition tests:
 *     Boundary values: {min, 0, max}
 *
 *     Equivalence classes:
 *     Variable: Addition result of two signed operands.
 *     - Valid tests: Addition result within range for each boundary value and
 *                    random value.
 *     - Invalid tests: Addition result out of range if possible for each
 *                      boundary and random value.
 *
 * -# Unsigned subtraction tests:
 *    Boundary values: {0, max}
 *
 *    Equivalence classes:
 *    Variable: Subtraction result of two unsigned operands.
 *    - Valid tests: Subtraction result within range for each boundary value and
 *                   random value.
 *    - Invalid tests: Subtraction result out of range if possible for each
 *                     boundary and random value.
 *
 * -# Signed subtraction tests:
 *    Boundary values: {min, 0, max}
 *
 *    Equivalence classes:
 *    Variable: Subtraction result of two signed operands.
 *    - Valid tests: Subtraction output within range for each boundary value and
 *                   random value.
 *    - Invalid tests: Subtraction output out of range if possible for each
 *                     boundary and random value.
 *
 * -# Unsigned multiplication tests:
 *    Boundary values: {0, max}
 *
 *    Equivalence classes:
 *    Variable: Multiplication result of two unsigned operands.
 *    - Valid tests: Multiplication result within range for each boundary value and
 *                   random value.
 *    - Invalid tests: Multiplication result out of range if possible for each
 *                     boundary and random value.
 *
 * -# Signed multiplication tests:
 *    Boundary values: {min, 0, max}
 *
 *    Equivalence classes:
 *    Variable: Multiplication result of two signed operands.
 *    - Valid tests: Multiplication result within range for each boundary value and
 *                   random value.
 *    - Invalid tests: Multiplication result out of range if possible for each
 *                     boundary and random value.
 *
 * Steps:
 * - Call the static analysis arithmetic APIs. Pass in valid values and verify
 *   correct return.
 * - Call the static analysis arithmetic APIs. Pass in values beyond type range
 *   and use EXPECT_BUG() to verify BUG() is called.
 *
 * Output: Returns PASS if expected result is met, FAIL otherwise.
 */
int test_arithmetic(struct unit_module *m, struct gk20a *g, void *args);

/**
 * Test specification for: test_cast
 *
 * Description: Verify functionality of static analysis safe cast APIs.
 *
 * Test Type: Feature, Error guessing, Boundary Value
 *
 * Targets: nvgpu_safe_cast_u64_to_u32, nvgpu_safe_cast_u64_to_u16,
 *          nvgpu_safe_cast_u64_to_u8, nvgpu_safe_cast_u64_to_s64,
 *          nvgpu_safe_cast_u64_to_s32, nvgpu_safe_cast_s64_to_u64,
 *          nvgpu_safe_cast_s64_to_u32, nvgpu_safe_cast_s64_to_s32,
 *          nvgpu_safe_cast_u32_to_u16, nvgpu_safe_cast_u32_to_u8,
 *          nvgpu_safe_cast_u32_to_s32, nvgpu_safe_cast_u32_to_s8,
 *          nvgpu_safe_cast_s32_to_u64, nvgpu_safe_cast_s32_to_u32,
 *          nvgpu_safe_cast_s8_to_u8, nvgpu_safe_cast_bool_to_u32
 *
 * Input: None
 *
 * -# unsigned to unsigned cast tests:
 *    Boundary values: {0, type2 max, type1 max}
 *
 *    Equivalence classes:
 *    Variable: Cast input value.
 *    - Valid tests: Cast result within range for each valid boundary value and
 *                   random value [0, type2 max].
 *    - Invalid tests: Cast result out of range if possible for each invalid
 *                     boundary and random value [type2 max + 1, type1 max].
 *
 * -# unsigned to signed cast tests:
 *    Boundary values: {type2 min, 0, type2 max, type1 max}
 *
 *    Equivalence classes:
 *    Variable: Cast input value.
 *    - Valid tests: Cast result within range for each valid boundary value and
 *                   random value. [0, type2 max]
 *    - Invalid tests: Cast result out of range if possible for each invalid
 *                     boundary and random value. {[type2 min, -1], [type2 max + 1, type1 max]}
 *
 * -# signed to unsigned cast tests:
 *    Boundary values: {type1 min, 0, type1 max}
 *
 *    Equivalence classes:
 *    Variable: Cast input value.
 *    - Valid tests: Cast result within range for each valid boundary value and
 *                   random value. [0, type1 max]
 *    - Invalid tests: Cast result out of range if possible for each invalid
 *                     boundary and random value. [type1 min, -1]
 *
 * -# s64 to u32 cast tests:
 *    Boundary values: {LONG_MIN, 0, U32_MAX, LONG_MAX}
 *
 *    Equivalence classes:
 *    Variable: Cast input value.
 *    - Valid tests: Cast result within range for each valid boundary value and
 *                   random value. [0, U32_MAX]
 *    - Invalid tests: Cast result out of range if possible for each invalid
 *                     boundary and random value. {[LONG_MIN, -1], [U32_MAX + 1, LONG_MAX]}
 *
 * -# s64 to s32 cast tests:
 *    Boundary values: {LONG_MIN, INT_MIN, 0, INT_MAX, LONG_MAX}
 *
 *    Equivalence classes:
 *    Variable: Cast input value.
 *    - Valid tests: Cast result within range for each valid boundary value and
 *                   random value. [INT_MIN, INT_MAX]
 *    - Invalid tests: Cast result out of range if possible for each invalid
 *                     boundary and random value. {[LONG_MIN, INT_MIN - 1], [INT_MAX + 1, LONG_MAX]}
 *
 * Steps:
 * - Call the static analysis cast APIs. Pass in valid values and verify
 *   correct return.
 * - Call the static analysis cast APIs. Pass in values beyond type range
 *   and use EXPECT_BUG() to verify BUG() is called.
 *
 * Output: Returns PASS if expected result is met, FAIL otherwise.
 */
int test_cast(struct unit_module *m, struct gk20a *g, void *args);

/**
 * Test specification for: test_safety_checks
 *
 * Description: Verify functionality of static analysis safety_check() API.
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_safety_checks
 *
 * Input: None
 *
 * Steps:
 * - Call the API nvgpu_safety_checks(). No error should occur.
 *
 * Output: Returns PASS if expected result is met, FAIL otherwise.
 */
int test_safety_checks(struct unit_module *m, struct gk20a *g, void *args);

/**
 * @}
 */

#endif /* UNIT_STATIC_ANALYSIS_H */
