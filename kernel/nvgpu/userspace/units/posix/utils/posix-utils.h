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
 * @addtogroup SWUTS-posix-utils
 * @{
 *
 * Software Unit Test Specification for posix-utils
 */

#ifndef __UNIT_POSIX_UTILS_H__
#define __UNIT_POSIX_UTILS_H__

/**
 * Test specification for test_hamming_weight
 *
 * Description: Test the hamming weight implementation.
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_posix_hweight8, nvgpu_posix_hweight16,
 *          nvgpu_posix_hweight32, nvgpu_posix_hweight64,
 *          hweight32, hweight_long
 *
 * Inputs: None
 *
 * Steps:
 * 1) Call nvgpu_posix_hweight8 in loop with only the loop index bit position
 *    set.
 * 2) Return FAIL if the return value from nvgpu_posix_hweight8 is not equal
 *    to 1 in any of the iterations.
 * 3) Repeat steps 1 and 2 for nvgpu_posix_hweight16, nvgpu_posix_hweight32
 *    nvgpu_posix_hweight64, hweight32 and hweight_long.
 * 4) Call nvgpu_posix_hweight8 with input parameter set as 0.
 * 5) Return FAIL if the return value from nvgpu_posix_hweight8 is not equal
 *    to 0.
 * 6) Call nvgpu_posix_hweight8 with input parameter set to maximum value.
 * 7) Return FAIL if the return value from nvgpu_posix_hweight8 is not equal
 *    to the number of bits in the input parameter.
 * 8) Repeat steps 4,5,6 and 7 for nvgpu_posix_hweight16, nvgpu_posix_hweight32
 *    nvgpu_posix_hweight64, hweight32 and hweight_long.
 *
 * Output:
 * The test returns PASS if all the hamming weight function invocations return
 * the expected value. Otherwise the test returns FAIL.
 *
 */
int test_hamming_weight(struct unit_module *m, struct gk20a *g, void *args);

/**
 * Test specification for test_be32tocpu
 *
 * Description: Test the endian conversion implementation.
 *
 * Test Type: Feature
 *
 * Targets: be32_to_cpu
 *
 * Inputs: None
 *
 * Steps:
 * 1) Invoke function be32_to_cpu with a fixed pattern as input.
 * 2) Check if the machine is little endian.
 * 3) If the machine is little endian, confirm that the return value from
 *    be32_to_cpu is equal to the little endian order of the pattern, else
 *    return FAIL.
 * 3) EXPECT_BUG is also tested to make sure that BUG is not called where it is
 *    not expected.
 *
 * Output:
 * The test returns PASS if BUG is called as expected based on the parameters
 * passed and EXPECT_BUG handles it accordingly.
 * The test returns FAIL if either BUG is not called as expected or if
 * EXPECT_BUG indicates that a BUG call was made which was not requested by
 * the test.
 *
 */
int test_be32tocpu(struct unit_module *m, struct gk20a *g, void *args);

/**
 * Test specification for test_minmax
 *
 * Description: Test the min and max implementations.
 *
 * Test Type: Feature
 *
 * Targets: min_t, min, min3, max
 *
 * Inputs: None
 *
 * Steps:
 * 1) Invoke function min in loop with different input parameter values.
 * 2) Check if the return value is the minimum value among the parameters
 *    passed. Else return FAIL.
 * 3) Invoke function min3 in loop with different input parameter values.
 * 4) Check if the return value is the minimum value among the parameters
 *    passed. Else return FAIL.
 * 5) Invoke function min_t in loop with type and values as input parameters.
 * 6) Check if the return value is the minimum value among the parameters
 *    passed for every iteration. Else return FAIL.
 * 7) Invoke function max in loop.
 * 8) Check if the return value is the maximum value among the parameters
 *    passed for every iteration. Else return FAIL.
 * 9) Return PASS.
 *
 * Output:
 * The test returns PASS if all the invocations of min and max implementations
 * returns the expected value. Otherwise, test returns FAIL.
 *
 */
int test_minmax(struct unit_module *m, struct gk20a *g, void *args);

/**
 * Test specification for test_arraysize
 *
 * Description: Test ARRAY_SIZE macro implementation.
 *
 * Test Type: Feature
 *
 * Targets: ARRAY_SIZE
 *
 * Inputs: None
 *
 * Steps:
 * 1) Invoke macro ARRAY_SIZE with multiple arrays and confirm
 *    that the results are as expected. Otherwise, return FAIL.
 * 4) Return PASS.
 *
 * Output:
 * The test returns PASS if all the invocations of ARRAY_SIZE macro returns
 * the results as expected. Otherwise, the test returns FAIL.
 *
 */
int test_arraysize(struct unit_module *m, struct gk20a *g, void *args);

/**
 * Test specification for test_typecheck
 *
 * Description: Test type checking macros.
 *
 * Test Type: Feature
 *
 * Targets: IS_UNSIGNED_TYPE, IS_UNSIGNED_LONG_TYPE,
 *          IS_SIGNED_LONG_TYPE
 *
 * Inputs: None
 *
 * Steps:
 * 1) Invoke macros IS_UNSIGNED_TYPE, IS_UNSIGNED_LONG_TYPE,
 *    IS_SIGNED_LONG_TYPE ARRAY_SIZE with multiple data types and confirm
 *    that the results are as expected. Otherwise, return FAIL.
 * 2) Return PASS.
 *
 * Output:
 * The test returns PASS if all the invocations of type checking macros returns
 * the results as expected. Otherwise, the test returns FAIL.
 *
 */
int test_typecheck(struct unit_module *m, struct gk20a *g, void *args);

/**
 * Test specification for test_align_macros
 *
 * Description: Test align macro implementations.
 *
 * Test Type: Feature
 *
 * Targets: ALIGN, ALIGN_MASK, PAGE_ALIGN
 *
 * Inputs: None
 *
 * Steps:
 * 1) Invoke macros ALIGN, ALIGN_MASK and PAGE_ALIGN and confirm that the
 *    results are masked as expected. Otherwise, return FAIL.
 * 2) Return PASS.
 *
 * Output:
 * The test returns PASS if all the invocations of various align macros
 * returns the results as expected. Otherwise, the test returns FAIL.
 *
 */
int test_align_macros(struct unit_module *m, struct gk20a *g, void *args);

/**
 * Test specification for test_round_macros
 *
 * Description: Test rounding macro implementation.
 *
 * Test Type: Feature
 *
 * Targets: round_mask, round_up, round_down
 *
 * Inputs: None
 *
 * Steps:
 * 1) Invoke macro round_mask in loop and confirm that the mask generated is
 *    as expected. Otherwise, return FAIL.
 * 2) Invoke macros round_up and round_up in loop for various input values and
 *    confirm that the values are rounded off as expected. Otherwise, return
 *    FAIL.
 * 3) Return PASS.
 *
 * Output:
 * The test returns PASS if all the invocations of round macros returns the
 * results as expected. Otherwise, the test returns FAIL.
 *
 */
int test_round_macros(struct unit_module *m, struct gk20a *g, void *args);

/**
 * Test specification for test_write_once
 *
 * Description: Test WRITE_ONCE macro implementation.
 *
 * Test Type: Feature
 *
 * Targets: WRITE_ONCE
 *
 * Inputs: None
 *
 * Steps:
 * 1) Invoke macro WRITE_ONCE in loop and confirm that the value is written
 *    into the variable as expected. Otherwise, return FAIL.
 * 2) Return PASS.
 *
 * Output:
 * The test returns PASS if all the invocations of WRITE_ONCE macro writes the
 * value into the variable. Otherwise, the test returns FAIL.
 *
 */
int test_write_once(struct unit_module *m, struct gk20a *g, void *args);

/**
 * Test specification for test_div_macros
 *
 * Description: Test various division macro implementations.
 *
 * Test Type: Feature
 *
 * Targets: DIV_ROUND_UP_U64, DIV_ROUND_UP, do_div, div64_u64
 *
 * Inputs: None
 *
 * Steps:
 * 1) Invoke macros DIV_ROUND_UP_U64, DIV_ROUND_UP, do_div and div64_u64 and
 *    confirm that the results are as expected. Otherwise, return FAIL.
 * 2) Return PASS.
 *
 * Output:
 * The test returns PASS if all the invocations of various division macros
 * returns the results as expected. Otherwise, the test returns FAIL.
 *
 */
int test_div_macros(struct unit_module *m, struct gk20a *g, void *args);

/**
 * Test specification for test_containerof
 *
 * Description: Test container_of implementation.
 *
 * Test Type: Feature
 *
 * Targets: container_of
 *
 * Inputs: Global struct instance cont.
 *
 * Steps:
 * 1) Invoke container_of with the first variable ptr in cont.
 * 2) Invoke container_of with the second variable ptr in cont.
 * 3) Confirm if both the invocation of container_of macro returns the address
 *    of the global struct instance cont. Otherwise, return FAIL.
 * 4) Return PASS.
 *
 * Output:
 * The test returns PASS if both the invocation of container_of returns the
 * same address as that of the global struct instance cont. Otherwise, the test
 * returns FAIL.
 *
 */
int test_containerof(struct unit_module *m, struct gk20a *g, void *args);

/**
 * Test specification for test_hertzconversion
 *
 * Description: Test hertz conversion macro implementation.
 *
 * Test Type: Feature
 *
 * Targets: HZ_TO_KHZ, HZ_TO_MHZ, HZ_TO_MHZ_ULL,
 *          KHZ_TO_HZ, MHZ_TO_KHZ, KHZ_TO_MHZ, MHZ_TO_HZ_ULL
 *
 * Inputs: None
 *
 * Steps:
 * 1) Invoke various hertz conversion macros with different input values.
 * 2) Check and confirm if the conversion macro results in expected value.
 *    Otherwise, return FAIL.
 * 4) Return PASS.
 *
 * Output:
 * The test returns PASS if all the invocations of various hertz conversion
 * functions returns the results as expected. Otherwise, the test returns FAIL.
 *
 */
int test_hertzconversion(struct unit_module *m, struct gk20a *g, void *args);

#endif /* __UNIT_POSIX_UTILS_H__ */
