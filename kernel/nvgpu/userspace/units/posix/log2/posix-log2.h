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

/**
 * @addtogroup SWUTS-posix-log2
 * @{
 *
 * Software Unit Test Specification for posix-log2
 */

#ifndef UNIT_POSIX_LOG2_H
#define UNIT_POSIX_LOG2_H

/**
 * Test specification for test_ilog2
 *
 * Description: Test integer logarithm of base 2 implementation.
 *
 * Test Type: Feature
 *
 * Targets: ilog2
 *
 * Inputs: None
 *
 * Steps:
 * 1) Invoke ilog2 in loop for all the power of two numbers that can be held in
 *    an unsigned long variable.
 * 2) Confirm if the return value from the macro is equal to the loop index.
 * 3) Return false otherwise.
 * 4) Invoke ilog2 in loop for numbers which are one greater than the power of
 *    two.
 * 5) Confirm if the return value from the macro is equal to the loop index.
 * 6) Return false otherwise.
 *
 * Output:
 * The test returns PASS if all the invocations of ilog2 returns the log to
 * the base 2 of the input number as expected. Otherwise, test returns FAIL.
 */
int test_ilog2(struct unit_module *m,
			struct gk20a *g, void *args);

/**
 * Test specification for test_roundup_powoftwo
 *
 * Description: Test round up power of two implementation.
 *
 * Test Type: Feature
 *
 * Targets: roundup_pow_of_two
 *
 * Inputs: None
 *
 * Steps:
 * 1) Invoke roundup_pow_of_two for input value 0UL using EXPECT_BUG.
 * 2) Check if EXPECT_BUG returns true, otherwise, return fail.
 * 3) Invoke roundup_pow_of_two in loop for all the power of two numbers
 *    that can be held in an unsigned long variable.
 * 4) Confirm if the return value from the macro is equal to the input argument
 *    passed. Return false otherwise.
 * 5) Invoke roundup_pow_of_two in loop for numbers which are one greater than
 *    the power of two.
 * 6) Confirm if the return value from the macro is equal to the rounded up
 *    power of two value of the input number. Return false otherwise.
 *
 * Output:
 * The test returns PASS if all the invocations of roundup_pow_of_two
 * returns the expected value as result. Otherwise, test returns FAIL.
 */
int test_roundup_powoftwo(struct unit_module *m,
			struct gk20a *g, void *args);

/**
 * Test specification for test_rounddown_powoftwo
 *
 * Description: Test round down power of two implementation.
 *
 * Test Type: Feature
 *
 * Targets: rounddown_pow_of_two
 *
 * Inputs: None
 *
 * Steps:
 * 1) Invoke rounddown_pow_of_two for input value 0UL using EXPECT_BUG.
 * 2) Check if EXPECT_BUG returns true, otherwise, return fail.
 * 3) Invoke rounddown_pow_of_two in loop for all the power of two numbers
 *    that can be held in an unsigned long variable.
 * 4) Confirm if the return value from the macro is equal to the input argument
 *    passed. Return false otherwise.
 * 5) Invoke rounddown_pow_of_two in loop for numbers which are one less than
 *    the power of two.
 * 6) Confirm if the return value from the macro is equal to the power of two
 *    rounded down value of the input number. Return false otherwise.
 *
 * Output:
 * The test returns PASS if all the invocations of rounddown_pow_of_two
 * returns the expected value as result. Otherwise, test returns FAIL.
 */
int test_rounddown_powoftwo(struct unit_module *m,
			struct gk20a *g, void *args);

/**
 * Test specification for test_ispow2
 *
 * Description: Test the power of two implementation.
 *
 * Test Type: Feature
 *
 * Targets: is_power_of_2
 *
 * Inputs: None
 *
 * Steps:
 * 1) Invoke is_power_of_2 in loop for all the power of two numbers that can
 *    be held in an unsigned long variable.
 * 2) Confirm if the return value from the macro is true, else, return fail.
 * 3) Invoke is_power_of_2 in loop for numbers which are one greater than the
 *    power of two.
 * 4) Confirm if the return value from the macro is false. Otherwise, return
 *    fail.
 *
 * Output:
 * The test returns PASS if all the invocations of is_power_of_2 returns the
 * result as expected. Otherwise, test returns FAIL.
 */
int test_ispow2(struct unit_module *m,
			struct gk20a *g, void *args);
#endif /* UNIT_POSIX_LOG2_H */
