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

#ifndef UNIT_BITMAP_ALLOCATOR_H
#define UNIT_BITMAP_ALLOCATOR_H

struct gk20a;
struct unit_module;

/** @addtogroup SWUTS-mm-allocators-bitmap-allocator
 *  @{
 *
 * Software Unit Test Specification for mm.allocators.bitmap_allocator
 */

/**
 * Test specification for: test_nvgpu_bitmap_allocator_init
 *
 * Description: Initialize bitmap allocator.
 *
 * Test Type: Feature, Error injection
 *
 * Targets: nvgpu_bitmap_allocator_init, nvgpu_bitmap_check_argument_limits,
 *          nvgpu_allocator.ops.fini, nvgpu_alloc_to_gpu
 *
 * Input: None
 *
 * Steps:
 * - Initialize bitmap allocator with following characteristics.
 *   - 1K memory base address.
 *   - 128K length of memory.
 *   - 1K block size.
 *   - Use this bitmap allocator for rest of the tests.
 *
 * Output: Returns SUCCESS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_nvgpu_bitmap_allocator_init(struct unit_module *m,
						struct gk20a *g, void *args);

/**
 * Test specification for: test_nvgpu_bitmap_allocator_ops
 *
 * Description: Check bitmap_allocator attribute values using allocator ops.
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_allocator.ops.base, nvgpu_allocator.ops.length,
 *          nvgpu_allocator.ops.end, nvgpu_allocator.ops.inited
 *
 * Input: test_nvgpu_bitmap_allocator_init
 *
 * Steps:
 * - Check bitmap_allocator attributes using allocator ops.
 *   - Execute allocator ops to read attibute value.
 *   - Confirm that value is equal to the default values set during
 *     initialization.
 *
 * Output: Returns SUCCESS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_nvgpu_bitmap_allocator_ops(struct unit_module *m,
						struct gk20a *g, void *args);

/**
 * Test specification for: test_nvgpu_bitmap_allocator_alloc
 *
 * Description: Allocate various sizes of memory to test different scenarios.
 *
 * Test Type: Feature, Error injection
 *
 * Targets: nvgpu_allocator.ops.alloc, nvgpu_allocator.ops.free_alloc,
 *          nvgpu_allocator.ops.alloc_fixed, nvgpu_allocator.ops.free_fixed,
 *          nvgpu_bitmap_alloc_from_rbtree_node, bitmap_allocator,
 *          alloc_loc, alloc_unlock
 *
 * Input: test_nvgpu_bitmap_allocator_init
 *
 * Steps:
 * - Allocate 3k memory using allocation functions.
 *   - Confirm that allocation is successful.
 * - Allocate 2M which is more than available memory.
 *   - Allocation is expected to fail.
 * - Allocate 4K, 8K, 16K and 32K memory segments.
 *   - Confirm all allocations are successful.
 * - Allocate various memory segments using fixed allocation functions.
 *   - Confirm aloocations are successful as expected.
 * - Free allocations.
 *   - Confirm allocations are freed.
 *
 * Output: Returns SUCCESS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_nvgpu_bitmap_allocator_alloc(struct unit_module *m,
						struct gk20a *g, void *args);

/**
 * Test specification for: test_nvgpu_bitmap_allocator_destroy
 *
 * Description: Free memory used for bitmap allocator.
 *
 * Test Type: Other (clean up)
 *
 * Targets: nvgpu_allocator.ops.fini
 *
 * Input: test_nvgpu_bitmap_allocator_init
 *
 * Steps:
 * - Free bitmap_allocator allocated for this unit test.
 *
 * Output: Returns SUCCESS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_nvgpu_bitmap_allocator_destroy(struct unit_module *m,
						struct gk20a *g, void *args);

/**
 * Test specification for: test_nvgpu_bitmap_allocator_critical
 *
 * Description: Test allocator functions for bitmap allocator in latency
 * critical path.
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_allocator_init, nvgpu_bitmap_allocator_init,
 *          nvgpu_bitmap_check_argument_limits, nvgpu_allocator.ops.alloc
 *          nvgpu_allocator.ops.free_alloc, nvgpu_allocator.ops.alloc_fixed,
 *          nvgpu_allocator.ops.free_fixed, nvgpu_allocator.ops.fini
 *
 * Input: None
 *
 * Steps:
 * - Initialize allocator with following characteristics.
 *   - 1K memory base address.
 *   - 128K memory length.
 *   - 1K block size.
 *   - GPU_ALLOC_NO_ALLOC_PAGE flag value.
 * - Allocate memory segments using allocation functions.
 *   - Confirm allocations are successful.
 * - Free allocated memory segments.
 * - Free bitmap allocator used for this test.
 *
 * Output: Returns SUCCESS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_nvgpu_bitmap_allocator_critical(struct unit_module *m,
						struct gk20a *g, void *args);

#endif /* UNIT_BITMAP_ALLOCATOR_H */
