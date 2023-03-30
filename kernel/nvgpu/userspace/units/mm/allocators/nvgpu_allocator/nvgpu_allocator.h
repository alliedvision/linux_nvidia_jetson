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

#ifndef UNIT_NVGPU_ALLOCATOR_H
#define UNIT_NVGPU_ALLOCATOR_H

struct gk20a;
struct unit_module;

/** @addtogroup SWUTS-mm-allocators-nvgpu-allocator
 *  @{
 *
 * Software Unit Test Specification for mm.allocators.nvgpu_allocator
 */

/**
 * Test specification for: test_nvgpu_alloc_common_init
 *
 * Description: Test common_init() function
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_alloc_common_init
 *
 * Input: None
 *
 * Steps:
 * - Initialize nvgpu allocator with default ops values.
 *   - Confirm that the parameters passed to the function make their way into
 *     allocator struct.
 * - Initialize nvgpu allocator for various invalid input cases.
 *
 * Output: Returns SUCCESS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_nvgpu_alloc_common_init(struct unit_module *m,
						struct gk20a *g, void *args);

/**
 * Test specification for: test_nvgpu_alloc_destroy
 *
 * Description: Test allocator destroy function
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_alloc_common_init, nvgpu_alloc_destroy
 *
 * Input: None
 *
 * Steps:
 * - Trigger allocator destroy function which further invokes fini() op.
 * - Allocator struct should be completely zeroed after this function.
 *
 * Output: Returns SUCCESS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_nvgpu_alloc_destroy(struct unit_module *m,
						struct gk20a *g, void *args);

/**
 * Test specification for: test_nvgpu_alloc_ops_present
 *
 * Description: Test allocator destroy function
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_alloc, nvgpu_alloc_pte, nvgpu_alloc_fixed, nvgpu_free_fixed,
 *          nvgpu_alloc_reserve_carveout, nvgpu_alloc_release_carveout,
 *          nvgpu_alloc_base, nvgpu_alloc_length, nvgpu_alloc_end, nvgpu_free,
 *          nvgpu_alloc_initialized, nvgpu_alloc_space
 *
 * Input: None
 *
 * Steps:
 * - Test the logic for calling present ops.
 * - Actual functionality of the ops should be verified by the respective
 *   allocator unit tests.
 *
 * Output: Returns SUCCESS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_nvgpu_alloc_ops_present(struct unit_module *m,
						struct gk20a *g, void *args);

/**
 * Test specification for: test_nvgpu_allocator_init
 *
 * Description: Test allocator init function
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_allocator_init, nvgpu_alloc_destroy
 *
 * Input: None
 *
 * Steps:
 * - Initialize each allocator and check that the allocator is created
 *   successfully.
 *
 * Output: Returns SUCCESS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_nvgpu_allocator_init(struct unit_module *m,
						struct gk20a *g, void *args);

#endif /* UNIT_NVGPU_ALLOCATOR_H */
