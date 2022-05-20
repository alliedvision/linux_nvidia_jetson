/*
 * Copyright (c) 2019-2021, NVIDIA CORPORATION.  All rights reserved.
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
#ifndef UNIT_FIFO_RAMIN_GM20B_FUSA_H
#define UNIT_FIFO_RAMIN_GM20B_FUSA_H

#include <nvgpu/types.h>

struct unit_module;
struct gk20a;

/** @addtogroup SWUTS-fifo-ramin-gm20b
 *  @{
 *
 * Software Unit Test Specification for fifo/ramin/gm20b
 */

/**
 * Test specification for: test_gm20b_ramin_set_big_page_size
 *
 * Description: Test big page size set
 *
 * Test Type: Feature
 *
 * Targets: gops_ramin.set_big_page_size, gm20b_ramin_set_big_page_size
 *
 * Input: None
 *
 * Steps:
 * - Set big page size in given instance block.
 *
 * Output: Returns PASS if all branches gave expected results. FAIL otherwise.
 */
int test_gm20b_ramin_set_big_page_size(struct unit_module *m, struct gk20a *g,
								void *args);

/**
 * Test specification for: test_gm20b_ramin_set_big_page_size
 *
 * Description: Test big page size boundary values
 *
 * Test Type: Boundary Value
 *
 * Targets: gops_ramin.set_big_page_size, gm20b_ramin_set_big_page_size
 *
 * Input: None
 * Equivalence classes:
 * size
 * - Invalid : { 0 - (SZ_64K - 1), (SZ_64K + 1) - U32_MAX }
 * - Valid :   { SZ_64K }
 *
 * Steps:
 * - Set big page size in given instance block.
 * - Check that ramin region is updated if a valid big page size is provided.
 * - Check that ramin region is not updated if an invalid big page size is provided.
 *
 * Output: Returns PASS if all branches gave expected results. FAIL otherwise.
 */
int test_gm20b_ramin_set_big_page_size_bvec(struct unit_module *m, struct gk20a *g,
								void *args);
/**
 * @}
 */

#endif /* UNIT_FIFO_RAMIN_GM20B_FUSA_H */
