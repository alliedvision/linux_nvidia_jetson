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

#ifndef UNIT_STRING_H
#define UNIT_STRING_H

struct gk20a;
struct unit_module;

/** @addtogroup SWUTS-interface-string
 *  @{
 *
 * Software Unit Test Specification for worker unit
 */


/**
 * Test specification for: test_memcpy_memcmp
 *
 * Description: Test functionality of the utility functions nvgpu_memcpy and
 *              nvgpu_memcmp.
 *
 * Test Type: Feature, Error guessing, Boundary values
 *
 * Targets: nvgpu_memcpy, nvgpu_memcmp
 *
 * Input: None.
 *
 * Steps:
 * - Initialize source array to the values 1-10.
 * - Initialize destination array to all 0's.
 * - Call nvgpu_memcpy with the source & destination arrays, passing the full
 *   length.
 * - Call nvgpu_memcmp with the source & destination arrays, passing the full
 *   length. Verify it returns a match.
 * - Re-init destination to 0.
 * - Call nvgpu_memcpy with the source & destination arrays, passing length-1.
 * - Call nvgpu_memcmp with the source & destination arrays, passing length-1.
 *   Verify it returns a match.
 * - Verify the final element of the destination array is still 0.
 * - Call nvgpu_memcmp with the source & destination arrays, passing length.
 *   Verify it returns a non-match.
 * - Call nvgpu_memcmp with a length 0. Verify a match is returned.
 *
 * Output: Returns PASS if expected result is met, FAIL otherwise.
 */
int test_memcpy_memcmp(struct unit_module *m, struct gk20a *g, void *args);

/**
 * Test specification for: test_strnadd_u32
 *
 * Description: Test functionality of the utility function test_strnadd_u32.
 *
 * Test Type: Feature, Error guessing, Boundary values
 *
 * Targets: nvgpu_strnadd_u32
 *
 * Input: None.
 *
 * Equivalence classes:
 * Variable: radix
 * - Valid: { 2 - 16 }
 * - Invalid: { 0, 1, 17 - UINT32_MAX }
 *
 * Steps:
 * - Call nvgpu_strnadd_u32 with invalid radix 0. Verify 0 is returned.
 * - Call nvgpu_strnadd_u32 with invalid radix 1. Verify 0 is returned.
 * - Call nvgpu_strnadd_u32 with invalid radix 17. Verify 0 is returned.
 * - Call nvgpu_strnadd_u32 with invalid radix 100. Verify 0 is returned.
 * - Call nvgpu_strnadd_u32 with invalid radix UINT32_MAX. Verify 0 is
 *   returned.
 * - Call nvgpu_strnadd_u32 with insufficient string sizes. Verify 0 is
 *   returned.
 * - Call nvgpu_strnadd_u32 with a binary value of 10. Verify returned size
 *   is 4 and the string contains "1010".
 * - Call nvgpu_strnadd_u32 with a decimal value of 1000. Verify returned size
 *   is 4 and the string contains "1000".
 * - Call nvgpu_strnadd_u32 with a hexadecimal value of 0xdeadbeef. Verify
 *   returned size is 8 and the string contains "deadbeef".
 *
 * Output: Returns PASS if expected result is met, FAIL otherwise.
 */
int test_strnadd_u32(struct unit_module *m, struct gk20a *g, void *args);

/**
 * Test specification for: test_mem_is_word_aligned
 *
 * Description: Test functionality of the utility function
 *              nvgpu_mem_is_word_aligned.
 *
 * Test Type: Feature, Error guessing, Boundary values
 *
 * Targets: nvgpu_mem_is_word_aligned
 *
 * Input: None.
 *
 * Steps:
 * - Call nvgpu_mem_is_word_aligned with various addresses and verify the
 *   correct value is returned.
 *
 * Output: Returns PASS if expected result is met, FAIL otherwise.
 */
int test_mem_is_word_aligned(struct unit_module *m, struct gk20a *g, void *args);

/**
 * @}
 */

#endif /* UNIT_STRING_H */
