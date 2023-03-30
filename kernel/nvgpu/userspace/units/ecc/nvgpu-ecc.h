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

#ifndef UNIT_NVGPU_ECC_H
#define UNIT_NVGPU_ECC_H

struct gk20a;
struct unit_module;

/** @addtogroup SWUTS-ecc
 *  @{
 *
 * Software Unit Test Specification for nvgpu.common.ecc
 */


/**
 * Test specification for: test_ecc_init_support
 *
 * Description: Verify the "nvgpu_ecc_init_support" API.
 *
 * Test Type: Feature Based
 *
 * Targets: nvgpu_ecc_init_support, gops_ecc.ecc_init_support
 *
 * Input: None
 *
 * Steps:
 * - Test case #1
 *   - Fresh initalization, ecc.initialized = false.
 *   - "nvgpu_ecc_init_support" should succeed and return 0.
 * - Test case #2
 *   - Re-initialization, ecc.initailiziation = true.
 *   - "nvgpu_ecc_init_support" will succeed but skip init and return 0.
 *
 * Output:
 * - UNIT_FAIL if "nvgpu_ecc_init_support" fails with non-zero return value.
 * - UNIT_SUCCESS otherwise
 */
int test_ecc_init_support(struct unit_module *m,
			struct gk20a *g, void *args);

/**
 * Test specification for: test_ecc_finalize_support
 *
 * Description: Verify the "nvgpu_ecc_finalize_support" API.
 *
 * Test Type: Feature Based
 *
 * Targets: nvgpu_ecc_finalize_support, gops_ecc.ecc_finalize_support
 *
 * Input: None
 *
 * Steps:
 * - Test case #1
 *   - Fresh initalization, ecc.initialized = false.
 *   - "nvgpu_ecc_finalize_support" should succeed and return 0.
 * - Test case #2
 *   - Re-initialization, ecc.initailiziation = true.
 *   - "nvgpu_ecc_finalize_support" will succeed but skip init and return 0.
 *
 * Output:
 * - UNIT_FAIL if "nvgpu_ecc_finalize_support" fails with non-zero return value.

 * - UNIT_SUCCESS otherwise
 */
int test_ecc_finalize_support(struct unit_module *m,
			struct gk20a *g, void *args);

/**
 * Test specification for: test_ecc_counter_init
 *
 * Description: Verify "nvgpu_ecc_counter_init" API.
 *
 * Test Type: Feature Based
 *
 * Targets: nvgpu_ecc_counter_init, nvgpu_ecc_stat_add
 *
 * Input: nvgpu_ecc_init_support
 *
 * Steps:
 * - Invokes "nvgpu_ecc_init_support".
 * - Allocate memory for counter name string.
 * - Test case #1
 *   - Invokes "nvgpu_ecc_counter_init" with valid counter name("test_counter")
 *   - "nvgpu_ecc_counter_init" should succeed and return 0.
 * - Test case #2
 *   - Inject memory allocation fault
 *   - "nvgpu_ecc_counter_init" should return -ENOMEM
 * - Test Case #3
 *   - Set counter name to string with invalid length equal to
 *     NVGPU_ECC_STAT_NAME_MAX_SIZE.
 *   - "nvgpu_ecc_counter_init" will truncate the counter name and return 0.
 * - Test case #4
 *   - Verify that the g->ecc.stats_list is empty.
 *
 * Output:
 * - UNIT_FAIL under the following conditions:
 *   - Counter name memory allocation failed.
 *   - "nvgpu_ecc_init_support" failed.
 *   - "nvgpu_ecc_counter_init" failed.
 * - UNIT_SUCCESS otherwise
 */
int test_ecc_counter_init(struct unit_module *m,
			struct gk20a *g, void *args);

/**
 * Test specification for: test_ecc_free
 *
 * Description: Verify "nvgpu_ecc_free" API.
 *
 * Test Type: Feature Based
 *
 * Targets: nvgpu_ecc_free
 *
 * Input: nvgpu_ecc_init_support
 *
 * Steps:
 * - Do the following setup
 *   - "nvgpu_ecc_init_support".
 *   - assign respective HALs and allocate memory for g->ltc and g->gr.
 * - Test case #1
 *   - Invokes "nvgpu_ecc_free" with unassigned fb_ecc_free, fbpa_ecc_free
 *     and pmu.ecc_free HALs.
 *   - "nvgpu_ecc_free" should succeed without faulting.
 * - Test case #2
 *   - Invokes "nvgpu_ecc_free" with uassigned fb_ecc_free, fbpa_ecc_free
 *     and pmu.ecc_free HALs.
 *   - "nvgpu_ecc_free" should succeed without faulting.
 *
 * Output:
 * - UNIT_FAIL under the following conditions:
 *   - "nvgpu_ecc_init_support" failed.
 *   - Memory allocation failed for either g->gr or g->ltc.
 *   - Memory allocation failed for either ltc.ecc_sec/ded_count.
 *
 * - UNIT_SUCCESS otherwise
 */
int test_ecc_free(struct unit_module *m,
			struct gk20a *g, void *args);

#endif /* UNIT_NVGPU_ECC_H */
