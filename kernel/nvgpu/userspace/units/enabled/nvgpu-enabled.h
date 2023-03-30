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

#ifndef UNIT_NVGPU_ENABLED_H
#define UNIT_NVGPU_ENABLED_H

struct gk20a;
struct unit_module;

/** @addtogroup SWUTS-enabled
 *  @{
 *
 * Software Unit Test Specification for enabled
 */

/**
 * Test specification for: test_nvgpu_init_enabled_flags
 *
 * Description: Initialize GPU enabled_flags
 *
 * Test Type: Feature, Error guessing
 *
 * Targets: nvgpu_init_enabled_flags
 *
 * Input: None
 *
 * Steps:
 * - GPU structure contains enabled_flags initialized at boot
 *   - Store already created enabled_flags pointer in a global variable
 * - Initialize enabled_flags for this unit test
 *   - New created enabled_flags are set to false
 *   - Check if return value indicates success
 *
 * Output: Returns SUCCESS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_nvgpu_init_enabled_flags(struct unit_module *m, struct gk20a *g,
								void *args);

/**
 * Test specification for: test_nvgpu_enabled_flags_false_check
 *
 * Description: Check if enabled_flags are set to false.
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_is_enabled
 *
 * Input: test_nvgpu_init_enabled_flags
 *
 * Steps:
 * - Check flag value
 *   - As flags are allocated for unit test, flag value is expected to be false
 *   - Iterate over each flag and check if flag value is false
 *
 * Output: Returns SUCCESS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_nvgpu_enabled_flags_false_check(struct unit_module *m,
						struct gk20a *g, void *args);

/**
 * Test specification for: test_nvgpu_set_enabled
 *
 * Description: Set and reset enabled_flags
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_is_enabled, nvgpu_set_enabled
 *
 * Input: test_nvgpu_init_enabled_flags
 *
 * Steps:
 * - Set and reset each flag
 *   - Iterate over a flag 'i' and set it to true
 *   - Check if flag 'i' value is true
 *   - Reset value of flag 'i' to false
 *   - Check if flag 'i' value is false
 *
 * Output: Returns SUCCESS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_nvgpu_set_enabled(struct unit_module *m, struct gk20a *g, void *args);

/**
 * Test specification for: test_nvgpu_enabled_bvec
 *
 * Description: Boundary check for flag setting and checking.
 *
 * Test Type: Boundary value
 *
 * Targets: nvgpu_is_enabled, nvgpu_set_enabled
 *
 * Input: test_nvgpu_init_enabled_flags
 *
 * Equivalence classes:
 * Variable: flag
 * - Valid : { 0 - (NVGPU_MAX_ENABLED_BITS - 1) }
 * - Invalid : { NVGPU_MAX_ENABLED_BITS - UINT32_MAX }
 *
 * Steps:
 * - Set and check flag for bit position 0. The set API nvgpu_set_enabled
 *   should set the value and the check API nvgpu_is_enabled should return 1.
 * - Set and check flag for bit position NVGPU_MAX_ENABLED_BITS - 1. The set
 *   API nvgpu_set_enabled should set the value and the check API
 *   nvgpu_is_enabled should return 1.
 * - Set and check flag for bit position NVGPU_MAX_ENABLED_BITS/2. The set API
 *   nvgpu_set_enabled should set the value and the check API nvgpu_is_enabled
 *   should return 1.
 * - Try to set and check flag for bit position NVGPU_MAX_ENABLED_BITS. The
 *   set API nvgpu_set_enabled should return without doing any operation and
 *   the check API nvgpu_is_enabled should return 0 indicating an error for
 *   flag boundary value.
 * - Try to set and check flag for bit position NVGPU_MAX_ENABLED_BITS + 1. The
 *   set API nvgpu_set_enabled should return without doing any operation and
 *   the check API nvgpu_is_enabled should return 0 indicating an error for
 *   flag boundary value.
 * - Try to set and check flag for bit position UINT32_MAX. The set API
 *   nvgpu_set_enabled should return without doing any operation and
 *   the check API nvgpu_is_enabled should return 0 indicating an error for
 *   flag boundary value.
 *
 * Output: Returns SUCCESS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_nvgpu_enabled_bvec(struct unit_module *m, struct gk20a *g, void *args);

/**
 * Test specification for: test_nvgpu_free_enabled_flags
 *
 * Description: Free enabled_flags
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_free_enabled_flags
 *
 * Input: test_nvgpu_init_enabled_flags
 *
 * Steps:
 * - Free enabled_flag memory
 *   - Free enabled_flags allocated for this unit test
 *   - Restore originally created enabled_flags pointer
 *
 * Output: Returns SUCCESS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_nvgpu_free_enabled_flags(struct unit_module *m,
						struct gk20a *g, void *args);
#endif /* UNIT_NVGPU_ENABLED_H */
