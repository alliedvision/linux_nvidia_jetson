/*
 * Copyright (c) 2019-2020, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef UNIT_POSIX_BITOPS_H
#define UNIT_POSIX_BITOPS_H

struct gk20a;
struct unit_module;

/** @addtogroup SWUTS-posix-bitops
 *  @{
 *
 * Software Unit Test Specification for posix-bitops
 */

/**
 * Test specification for: test_bitmap_info
 *
 * Description: Print informational log information.
 *
 * Test Type: Other - informational
 *
 * Targets: None
 *
 * Input: None
 *
 * Steps:
 * - Print the number of bytes to store an "unsigned long."
 * - Print the number of bits to store an "unsigned long."
 *
 * Output: Returns SUCCESS always
 */
int test_bitmap_info(struct unit_module *m, struct gk20a *g, void *args);

/**
 * Test specification for: test_ffs
 *
 * Description: Test the API nvgpu_ffs() (Find First Set [bit]).
 *
 * Test Type: Feature, Boundary values
 *
 * Targets: nvgpu_ffs, nvgpu_posix_ffs
 *
 * Input: None
 *
 * Steps:
 * - Test passing set of pre-determined table values to nvgpu_ffs() and validate
 *   expected value (from table) is returned.
 * - Test each bit in an "unsigned long" individually:
 *   - Loop through each bit in a "unsigned long."
 *     - Pass the constructed "unsigned long" to nvgpu_ffs().
 *     - Verify the correct bit is returned.
 *
 * Output: Returns SUCCESS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_ffs(struct unit_module *m, struct gk20a *g, void *args);


/**
 * Test specification for: test_fls
 *
 * Description: Test the API nvgpu_fls() (Find Last Set [bit]).
 *
 * Test Type: Feature, Boundary values
 *
 * Targets: nvgpu_fls, nvgpu_posix_fls
 *
 * Input: None
 *
 * Steps:
 * - Test passing set of pre-determined table values to nvgpu_fls() and validate
 *   expected value (from table) is returned.
 * - Test each bit in an "unsigned long" individually:
 *   - Loop through each bit in a "unsigned long."
 *     - Pass the constructed "unsigned long" to nvgpu_fls().
 *     - Verify the correct bit is returned.
 *
 * Output: Returns SUCCESS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_fls(struct unit_module *m, struct gk20a *g, void *args);

/**
 * Test specification for: test_ffz
 *
 * Description: Test the API ffz() (Find First Zero [bit]).
 *
 * Test Type: Feature, Boundary values
 *
 * Targets: ffz
 *
 * Input: None
 *
 * Steps:
 * - Test each bit in an "unsigned long" individually:
 *   - Loop through each bit in a "unsigned long."
 *     - Pass the constructed "unsigned long" to ffz().
 *     - Verify the correct bit is returned.
 *
 * Output: Returns SUCCESS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_ffz(struct unit_module *m, struct gk20a *g, void *args);

/**
 * Test specification for: test_find_first_bit
 *
 * Description: Test the APIs find_first_zero_bit() and find_first_bit().
 *
 * Test Type: Feature, Boundary values
 *
 * Targets: find_first_zero_bit, find_first_bit
 *
 * Input: Pointer to struct test_find_bit_args as function parameter.
 * - The parameter test_find_bit_args is used to select between testing of:
 *   - find_first_zero_bit()
 *   - find_first_bit()
 *
 * Steps:
 * - Test the API honors the size parameter.
 *   - Call API with 16 bits set/zeroed and a size of 8. Verify returns 8.
 *   - Call API with 16 bits set/zeroed and a size of 20. Verify returns 16.
 *   - Call API with no bits set/zeroed and a size of 256. Verify returns 256.
 * - Test each bit in an array individually:
 *   - Set/zero all bits in entire array.
 *   - Loop through incrementally setting/zeroing bits in the array.
 *     - Pass the constructed array to API with size of entire array (in bits).
 *     - Verify the correct bit is returned.
 *
 * Output: Returns SUCCESS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_find_first_bit(struct unit_module *m, struct gk20a *g, void *__args);

/**
 * Test specification for: test_find_next_bit
 *
 * Description: Test the API find_next_bit().
 *
 * Test Type: Feature, Boundary values
 *
 * Targets: find_next_bit
 *
 * Input: None.
 *
 * Steps:
 * - Test the API honors the size and start parameters.
 *   - Clear array of "unsigned long."
 *   - Loop through each bit of array:
 *     - Call find_next_bit() with array, size of array, and starting bit as
 *       the loop index.
 *     - Verify array size is returned (since no bit is set).
 *   - Set all bits in array of "unsigned long."
 *   - Loop through each bit of array:
 *     - Call find_next_bit() with array, size of array, and starting bit as
 *       the loop index.
 *     - Verify bit number is returned.
 *   - Call find_next_bit() with a starting bit > size. Verify returns size.
 *
 * Output: Returns SUCCESS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_find_next_bit(struct unit_module *m, struct gk20a *g, void *__args);

/**
 * Test specification for: test_find_zero_area
 *
 * Description: Test the API bitmap_find_next_zero_area_off().
 *
 * Test Type: Feature, Boundary values
 *
 * Targets: bitmap_find_next_zero_area, bitmap_find_next_zero_area_off
 *
 * Input: None.
 *
 * Steps:
 * - Loop through array of all zeros:
 *   - Call bitmap_find_next_zero_area() with:
 *     - array of zeros
 *     - size of array (in bits)
 *     - start value of loop index
 *     - size of bitmap as size of array - 1 (bit)
 *     - 0 for alignment parameters
 *   - Verify returns loop index.
 *   - Call bitmap_find_next_zero_area() with:
 *     - array of zeros
 *     - size of array (in bits)
 *     - start value of loop index
 *     - size of bitmap as 1 (bit)
 *     - 0 for alignment parameters
 *   - Verify returns loop index.
 *   - Call bitmap_find_next_zero_area() with:
 *     - array of zeros
 *     - size of array (in bits)
 *     - start value of 0
 *     - size of bitmap as size of array - loop index
 *     - 0 for alignment parameters
 *   - Verify returns 0.
 * - Loop through array of all zeros:
 *   - Call bitmap_find_next_zero_area_off() with:
 *     - array of zeros
 *     - size of array (in bits)
 *     - start value of loop index
 *     - size of bitmap as size of array - 1 (bit)
 *     - 0 for alignment parameters
 *   - Verify returns loop index.
 *   - Call bitmap_find_next_zero_area_off() with:
 *     - array of zeros
 *     - size of array (in bits)
 *     - start value of loop index
 *     - size of bitmap as 1 (bit)
 *     - 0 for alignment parameters
 *   - Verify returns loop index.
 *   - Call bitmap_find_next_zero_area_off() with:
 *     - array of zeros
 *     - size of array (in bits)
 *     - start value of 0
 *     - size of bitmap as size of array - loop index
 *     - 0 for alignment parameters
 *   - Verify returns 0.
 * - Loop through array with all bits set:
 *   - Inner loop from 0 to outer loop index:
 *     - Call bitmap_find_next_zero_area_off() with:
 *       - array with all bits set
 *       - size of array (in bits)
 *       - start value of outer loop index
 *       - size of bitmap as Inner loop index
 *       - 0 for alignment parameters
 *     - Verify returns bitmap array size.
 * - Loop through array with alternating nibbles of 0's and 1's:
 *   - Inner loop from 0 to outer loop index:
 *     - Call bitmap_find_next_zero_area_off() with:
 *       - array of alternating nibbles
 *       - size of array (in bits)
 *       - start value of outer loop index
 *       - size of bitmap as Inner loop index
 *       - 0 for alignment parameters
 *     - Verify:
 *       - If inner loop is <= 4, returns result < size of array.
 *       - If inner loop > 4, returns size of array since bitmap would not fit.
 *     - Call bitmap_find_next_zero_area_off() with:
 *       - array of alternating nibbles
 *       - size of array (in bits)
 *       - start value of outer loop index
 *       - size of bitmap as Inner loop index modulo 4 + 1
 *       - 3 for alignment mask
 *       - 0 for alignment offset
 *     - Verify (result % 8 != 4)
 *     - Call bitmap_find_next_zero_area_off() with:
 *       - array of alternating nibbles
 *       - size of array (in bits)
 *       - start value of outer loop index
 *       - size of bitmap as Inner loop index modulo 2 + 1
 *       - 7 for alignment mask
 *       - 2 for alignment offset
 *     - Verify (result % 8 != 6)
 *
 * Output: Returns SUCCESS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_find_zero_area(struct unit_module *m, struct gk20a *g, void *unused);

/**
 * Test specification for: test_single_bitops
 *
 * Description: Test the APIs nvgpu_set_bit(), nvgpu_clear_bit(), and
 *              nvgpu_test_bit().
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_set_bit, nvgpu_clear_bit, nvgpu_test_bit
 *
 * Input: None.
 *
 * Steps:
 * - Loop through bits in an array, calling nvgpu_set_bit() for each iteration,
 *   which will set all the bits in the array.
 * - Loop back through array and verify all bits are set in the array.
 * - Loop through bits in an array, calling nvgpu_test_bit() for each iteration,
 *   and verify nvgpu_test_bit() returns true for each bit.
 * - Loop through bits in an array, calling nvgpu_clear_bit() for each
 *   iteration, which will clear all the bits in the array.
 * - Loop back through array and verify none of the bits are set in the array.
 * - Loop through bits in an array, calling nvgpu_test_bit() for each iteration,
 *   and verify nvgpu_test_bit() returns false for each bit.
 *
 * Output: Returns SUCCESS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_single_bitops(struct unit_module *m, struct gk20a *g, void *__args);

/**
 * Test specification for: test_bit_setclear
 *
 * Description: Test the APIs nvgpu_set_bit() and nvgpu_clear_bit().
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_set_bit, nvgpu_clear_bit
 *
 * Input: Pointer to struct test_find_bit_args as function parameter.
 * - The parameter test_find_bit_args is used to select between testing of:
 *   - nvgpu_clear_bit()
 *   - nvgpu_set_bit()
 *
 * Steps:
 * - If testing nvgpu_clear_bit(), initialize array with all bits set.
 *   Otherwise, clear the array.
 * - Loop through bits in an array, calling the API for each iteration,
 *   which will either clear or set all the bits in the array.
 * - Loop back through array and verify all bits are set properly in the array.
 *
 * Output: Returns SUCCESS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_bit_setclear(struct unit_module *m, struct gk20a *g, void *__args);

/**
 * Test specification for: test_test_and_setclear_bit
 *
 * Description: Test the APIs nvgpu_test_and_clear_bit() and
 *              nvgpu_test_and_set_bit().
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_test_and_clear_bit, nvgpu_test_and_set_bit
 *
 * Input: Pointer to struct test_find_bit_args as function parameter.
 * - The parameter test_find_bit_args is used to select between testing of:
 *   - nvgpu_test_and_clear_bit()
 *   - nvgpu_test_and_set_bit()
 *
 * Steps:
 * - If testing nvgpu_test_and_clear_bit(), initialize array with all bits set.
 *   Otherwise, clear the array.
 * - Loop through bits in an array:
 *   - Call the requested API for each iteration.
 *   - Verify if testing nvgpu_test_and_clear_bit(), it returns true since the
 *     bit was set above. Alternately, if testing nvgpu_test_and_set_bit(),
 *     verify, the call returns false.
 * - Loop through bits in an array:
 *   - Call the 'inverse' API for each iteration. I.e. if the original parameter
 *     to the test was to test nvgpu_test_and_clear_bit(),
 *     call nvgpu_test_and_set_bit(), and vice versa.
 *   - Verify the API call returns the expected value based on if array should
 *     be all set or cleared.
 * - Loop back through array and verify all bits are set properly in the array,
 *   which should be the original initialization value.
 *
 * Output: Returns SUCCESS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_test_and_setclear_bit(struct unit_module *m, struct gk20a *g,
				void *__args);

/**
 * Test specification for: test_bitmap_setclear
 *
 * Description: Test the APIs nvgpu_bitmap_clear() and nvgpu_bitmap_set().
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_bitmap_clear, nvgpu_bitmap_set
 *
 * Input: Pointer to struct test_find_bit_args as function parameter.
 * - The parameter test_find_bit_args is used to select between testing of:
 *   - nvgpu_bitmap_clear()
 *   - nvgpu_bitmap_set()
 *
 * Steps:
 * - Loop through the bits of an array:
 *   - Inner loop from 0 to possible bitmap size
 *     (i.e. array size - outer loop bit):
 *     - Initialize the array to 0's if testing nvgpu_bitmap_set() or 1's if
 *       testing nvgpu_bitmap_clear().
 *     - Call the API with the array, starting at the bit in the outer loop,
 *       and the bitmap size of the inner loop.
 *     - Verify the bitmap is set/cleared properly in the array.
 *
 * Output: Returns SUCCESS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_bitmap_setclear(struct unit_module *m, struct gk20a *g, void *__args);

/**
 * Test specification for: test_bitops_misc
 *
 * Description: Test the various macros implemented in bitops unit.
 *
 * Test Type: Feature
 *
 * Targets: BITS_TO_LONGS, BIT, GENMASK, for_each_set_bit
 *
 * Input: None
 *
 * Steps:
 * - Invoke BITS_TO_LONGS for various bit values and confirm the return value
 *   is as expected.
 * - Invoke GENMASK for various mask value settings. Confirm if the return
 *   value has only the requested bits set.
 * - Invoke for_each_set_bit for a particular bit pattern. Confirm if the loop
 *   is executed only for set bit positions.
 * - Invoke BIT macro for various bit positions in loop. Confirm if the bit
 *   positions are as requested.
 *
 * Output: Returns SUCCESS if all the macro invocations returns result as
 * expected. Otherwise, the test returns FAIL.
 *
 */
int test_bitops_misc(struct unit_module *m, struct gk20a *g, void *__args);

#endif /* UNIT_POSIX_BITOPS_H */
