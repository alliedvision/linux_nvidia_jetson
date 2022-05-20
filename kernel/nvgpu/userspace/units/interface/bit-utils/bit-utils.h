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

#ifndef UNIT_BIT_UTILS_H
#define UNIT_BIT_UTILS_H

struct gk20a;
struct unit_module;

/** @addtogroup SWUTS-interface-bit-utils
 *  @{
 *
 * Software Unit Test Specification for common.utils.bit-utils
 */

/**
 * Test specification for: test_hi_lo
 *
 * Description: Verify functionality of hi/lo bit-utils APIs.
 *
 * Test Type: Feature
 *
 * Targets: u64_hi32, u64_lo32, hi32_lo32_to_u64
 *
 * Input: None
 *
 * Steps:
 * - Call u64_hi32 with a u64 value and verify the correct value is returned.
 * - Call u64_lo32 with a u64 value and verify the correct value is returned.
 * - Call hi32_lo32_to_u64 with two u32 values and verify the correct u64 value
 *   is returned.
 *
 * Output: Returns PASS if expected result is met, FAIL otherwise.
 */
int test_hi_lo(struct unit_module *m, struct gk20a *g, void *args);

/**
 * Test specification for: test_fields
 *
 * Description: Verify functionality of bit field bit-util APIs.
 *
 * Test Type: Feature
 *
 * Targets: set_field, get_field
 *
 * Input: None
 *
 * Steps:
 * - Call set_field() with a variety of inputs and verify the correct value is
 *   returned.
 * - Call get_field() with a variety of inputs and verify the correct value is
 *   returned.
 *
 * Output: Returns PASS if expected result is met, FAIL otherwise.
 */
int test_fields(struct unit_module *m, struct gk20a *g, void *args);

/**
 * @}
 */

#endif /* UNIT_BIT_UTILS_H */
