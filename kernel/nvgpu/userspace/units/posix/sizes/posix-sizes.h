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

/**
 * @addtogroup SWUTS-posix-sizes
 * @{
 *
 * Software Unit Test Specification for posix-sizes
 */

#ifndef __UNIT_POSIX_SIZES_H__
#define __UNIT_POSIX_SIZES_H__

#include <stdlib.h>

#include <unit/io.h>
#include <unit/unit.h>

#include <nvgpu/sizes.h>

/**
 * Test specification for test_size_defines
 *
 * Description: Test the values of various size defines.
 *
 * Test Type: Feature
 *
 * Targets: SZ_256, SZ_1K, SZ_4K, SZ_64K, SZ_128K, SZ_1M, SZ_16M, SZ_32M,
 * 	    SZ_256M, SZ_512M, SZ_1G, SZ_4G.
 *
 * Inputs:
 * None
 *
 * Steps:
 * 1) Assign a local variable with the expected value of the define that
 *    has to compared.
 * 2) Compare the value of the local variable with the value of the
 *    define.
 * 3) Return fail if the value of the define does not match with the
 *    expected value stored in local variable.
 * 4) Repeat steps 1 - 3 for all the sizes defined in the unit.
 *
 * Output:
 * The test returns PASS if the values of #defines match with the respective
 * expected value.
 * The test returns FAIL if there is a mismatch between defined value and
 * the expected value.
 *
 */
int test_size_defines(struct unit_module *m, struct gk20a *g, void *args);

#endif /* __UNIT_POSIX_SIZES_H__ */
