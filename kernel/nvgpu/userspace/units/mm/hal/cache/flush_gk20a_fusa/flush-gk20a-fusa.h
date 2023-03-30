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

#ifndef UNIT_MM_HAL_CACHE_FLUSH_GK20A_FUSA_H
#define UNIT_MM_HAL_CACHE_FLUSH_GK20A_FUSA_H

struct gk20a;
struct unit_module;

/** @addtogroup SWUTS-mm-hal-cache-flush-gk20a-fusa
 *  @{
 *
 * Software Unit Test Specification for mm.hal.cache.flush_gk20a_fusa
 */

/**
 * Test specification for: test_env_init_flush_gk20a_fusa
 *
 * Description: Initialize environment for MM tests
 *
 * Test Type: Feature
 *
 * Targets: None
 *
 * Input: None
 *
 * Steps:
 * - Init HALs and initialize VMs similar to nvgpu_init_system_vm().
 *
 * Output: Returns SUCCESS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_env_init_flush_gk20a_fusa(struct unit_module *m, struct gk20a *g,
								void *args);

/**
 * Test specification for: test_gk20a_mm_fb_flush
 *
 * Description: Test FB flush
 *
 * Test Type: Feature
 *
 * Targets: gops_mm_cache.fb_flush, gk20a_mm_fb_flush,
 * gops_mm.get_flush_retries
 *
 * Input: test_env_init, args (value can be F_GK20A_FB_FLUSH_DEFAULT_INPUT,
 *        F_GK20A_FB_FLUSH_GET_RETRIES, F_GK20A_FB_FLUSH_PENDING_TRUE,
 *        F_GK20A_FB_FLUSH_OUTSTANDING_TRUE,
 *        F_GK20A_FB_FLUSH_OUTSTANDING_PENDING_TRUE,
 *        F_GK20A_FB_FLUSH_DUMP_VPR_WPR_INFO or
 *        F_GK20A_FB_FLUSH_NVGPU_POWERED_OFF)
 *
 * Steps:
 * - Invoke FB flush command
 * - Test FB flush with various scenarios as below:
 *   - flush outstanding, flush pending, GPU powered off
 *
 * Output: Returns SUCCESS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_gk20a_mm_fb_flush(struct unit_module *m, struct gk20a *g, void *args);

/**
 * Test specification for: test_gk20a_mm_l2_flush
 *
 * Description: Test L2 flush
 *
 * Test Type: Feature
 *
 * Targets: gops_mm_cache.l2_flush, gk20a_mm_l2_flush,
 * gk20a_mm_l2_invalidate_locked
 *
 * Input: test_env_init, args (value can be F_GK20A_L2_FLUSH_DEFAULT_INPUT,
 *        F_GK20A_L2_FLUSH_GET_RETRIES, F_GK20A_L2_FLUSH_PENDING_TRUE,
 *        F_GK20A_L2_FLUSH_OUTSTANDING_TRUE, F_GK20A_L2_FLUSH_INVALIDATE or
 *        F_GK20A_L2_FLUSH_NVGPU_POWERED_OFF)
 *
 * Steps:
 * - Invoke L2 flush command
 * - Test L2 flush with various scenarios as below:
 *   - flush dirty outstanding, flush dirty pending, GPU powered off,
 *     flush with invalidate
 *
 * Output: Returns SUCCESS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_gk20a_mm_l2_flush(struct unit_module *m, struct gk20a *g, void *args);

/**
 * Test specification for: test_gk20a_mm_l2_invalidate
 *
 * Description: Test L2 invalidate
 *
 * Test Type: Feature
 *
 * Targets: gops_mm_cache.l2_invalidate, gk20a_mm_l2_invalidate,
 * gk20a_mm_l2_invalidate_locked
 *
 * Input: test_env_init, args (value can be F_GK20A_L2_INVALIDATE_DEFAULT_INPUT,
 *        F_GK20A_L2_INVALIDATE_PENDING_TRUE,
 *        F_GK20A_L2_INVALIDATE_OUTSTANDING_TRUE,
 *        F_GK20A_L2_INVALIDATE_GET_RETRIES_NULL or
 *        F_GK20A_L2_INVALIDATE_NVGPU_POWERED_OFF)
 *
 * Steps:
 * - Invoke L2 invalidate
 * - Test when invalidate is outstanding and/or pending
 *
 * Output: Returns SUCCESS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_gk20a_mm_l2_invalidate(struct unit_module *m, struct gk20a *g,
								void *args);

/**
 * Test specification for: test_env_clean_flush_gk20a_fusa
 *
 * Description: Cleanup test environment
 *
 * Test Type: Feature
 *
 * Targets: None
 *
 * Input: test_env_init
 *
 * Steps:
 * - Destroy memory and VMs initialized for the test.
 *
 * Output: Returns SUCCESS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_env_clean_flush_gk20a_fusa(struct unit_module *m, struct gk20a *g,
								void *args);

/** @} */
#endif /* UNIT_MM_HAL_CACHE_FLUSH_GK20A_FUSA_H */
