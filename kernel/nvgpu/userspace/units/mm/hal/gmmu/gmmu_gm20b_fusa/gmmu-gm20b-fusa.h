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

#ifndef UNIT_MM_HAL_GMMU_GMMU_GM20B_FUSA_H
#define UNIT_MM_HAL_GMMU_GMMU_GM20B_FUSA_H

struct gk20a;
struct unit_module;

/** @addtogroup SWUTS-mm-hal-gmmu-gmmu_gm20b_fusa
 *  @{
 *
 * Software Unit Test Specification for mm.hal.gmmu.gmmu_gm20b_fusa
 */

/**
 * Test specification for: test_gm20b_mm_get_big_page_sizes
 *
 * Description: Test big page size
 *
 * Test Type: Feature
 *
 * Targets: gops_mm_gmmu.get_big_page_sizes, gm20b_mm_get_big_page_sizes
 *
 * Input: test_env_init
 *
 * Steps:
 * - Check big page size value
 *
 * Output: Returns SUCCESS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_gm20b_mm_get_big_page_sizes(struct unit_module *m, struct gk20a *g,
								void *args);

/** @} */
#endif /* UNIT_MM_HAL_GMMU_GMMU_GM20B_FUSA_H */
