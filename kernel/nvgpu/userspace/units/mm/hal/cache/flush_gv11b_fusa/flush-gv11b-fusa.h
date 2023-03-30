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

#ifndef UNIT_MM_HAL_CACHE_FLUSH_GV11B_FUSA_H
#define UNIT_MM_HAL_CACHE_FLUSH_GV11B_FUSA_H

struct gk20a;
struct unit_module;

/** @addtogroup SWUTS-mm-hal-cache-flush-gv11b-fusa
 *  @{
 *
 * Software Unit Test Specification for mm.hal.cache.flush_gv11b_fusa
 */

/**
 * Test specification for: test_env_init_flush_gv11b_fusa
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
int test_env_init_flush_gv11b_fusa(struct unit_module *m, struct gk20a *g,
	void *args);

/**
 * Test specification for: test_gv11b_mm_l2_flush
 *
 * Description: Test L2 flush
 *
 * Test Type: Feature
 *
 * Targets: gops_mm_cache.l2_flush, gv11b_mm_l2_flush
 *
 * Input: test_env_init, args (value can be
 *        F_GV11B_L2_FLUSH_PASS_BAR1_BIND_NOT_NULL,
 *        F_GV11B_L2_FLUSH_PASS_BAR1_BIND_NULL, F_GV11B_L2_FLUSH_FB_FLUSH_FAIL,
 *        F_GV11B_L2_FLUSH_L2_FLUSH_FAIL, F_GV11B_L2_FLUSH_TLB_INVALIDATE_FAIL,
 *        F_GV11B_L2_FLUSH_FB_FLUSH2_FAIL)
 *
 * Steps:
 * - Invoke L2 flush command
 * - Test L2 flush with various scenarios as below:
 *   - fb_flush is successful or fails
 *   - l2_flush passes or fails
 *   - bar1_bind is populated or not populated
 *   - tlb_invalidate passes or fails
 *
 * Output: Returns SUCCESS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_gv11b_mm_l2_flush(struct unit_module *m, struct gk20a *g, void *args);

/**
 * Test specification for: test_env_clean_flush_gv11b_fusa
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
int test_env_clean_flush_gv11b_fusa(struct unit_module *m, struct gk20a *g,
	void *args);

/** @} */
#endif /* UNIT_MM_HAL_CACHE_FLUSH_GV11B_FUSA_H */
