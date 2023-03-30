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

#ifndef UNIT_BUDDY_ALLOCATOR_H
#define UNIT_BUDDY_ALLOCATOR_H

struct gk20a;
struct unit_module;

/** @addtogroup SWUTS-mm-allocators-buddy-allocator
 *  @{
 *
 * Software Unit Test Specification for mm.allocators.buddy_allocator
 */

/**
 * Test specification for: test_nvgpu_buddy_allocator_init
 *
 * Description: Initialize buddy allocator.
 *
 * Test Type: Feature, Error injection
 *
 * Targets: nvgpu_allocator_init, nvgpu_buddy_allocator_init,
 *          nvgpu_buddy_check_argument_limits, nvgpu_buddy_set_attributes,
 *          balloc_allocator_align, balloc_compute_max_order, balloc_init_lists,
 *          balloc_max_order_in, balloc_get_order, balloc_get_order_list,
 *          nvgpu_allocator.ops.fini
 *
 * Input: None
 *
 * Steps:
 * - Allocate memory for nvgpu allocator
 * - Initialize buddy allocator with different parameters to test init function.
 *   - zero block size.
 *   - block size which is not power of 2.
 *   - max order set greater than GPU_BALLOC_MAX_ORDER.
 *   - zero memory size.
 *   - base address zero.
 *   - unaligned base address.
 *   - Confirm the output of the above test cases is as expected.
 * - Initialize buddy allocator with following attributes.
 *   - 4K base address.
 *   - 1M memory size.
 *   - 4K block size.
 *   - max order equal to GPU_BALLOC_MAX_ORDER.
 *   - flags set to NULL.
 *   - NULL vm.
 *   - GVA space disabled.
 *   - This initialized allocator will be used for this unit test.
 *
 * Output: Returns SUCCESS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_nvgpu_buddy_allocator_init(struct unit_module *m,
						struct gk20a *g, void *args);

/**
 * Test specification for: test_nvgpu_buddy_allocator_carveout
 *
 * Description: Test allocation of carveouts.
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_allocator.ops.alloc, nvgpu_allocator.ops.reserve_carveout,
 *          nvgpu_allocator.ops.release_carveout,
 *          nvgpu_alloc_carveout_from_co_entry
 *
 * Input: test_nvgpu_buddy_allocator_init
 *
 * Steps:
 * - Allocate segment of memory as carveout.
 *   - Use reserved_carveout operation of buddy allocator to portion out segment
 *     of memory.
 *   - Confirm that the carveout is successful.
 * - Test carveout allocation with below variations.
 *   - Carveout base address less than buddy allocator base address.
 *   - Carveout length more than buddy allocator size.
 *   - Unaligned base address.
 *   - Reserve carveout after normal memory allocation.
 *
 * Output: Returns SUCCESS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_nvgpu_buddy_allocator_carveout(struct unit_module *m,
						struct gk20a *g, void *args);

/**
 * Test specification for: test_nvgpu_buddy_allocator_basic_ops
 *
 * Description: Test buddy allocator attribute and allocation functions.
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_allocator.ops.base, nvgpu_allocator.ops.length,
 *          nvgpu_allocator.ops.end, nvgpu_allocator.ops.inited,
 *          nvgpu_allocator.ops.space, nvgpu_allocator.ops.alloc,
 *          nvgpu_allocator.ops.alloc_pte, nvgpu_allocator.ops.free_alloc,
 *          nvgpu_allocator.ops.alloc_fixed, nvgpu_buddy_allocator_flag_ops,
 *          nvgpu_buddy_from_buddy_entry, balloc_base_shift, buddy_allocator,
 *          balloc_base_unshift, balloc_owner, balloc_order_to_len, alloc_lock,
 *          alloc_unlock, nvgpu_alloc_to_gpu, nvgpu_buddy_from_rbtree_node,
 *          nvgpu_fixed_alloc_from_rbtree_node
 *
 * Input: test_nvgpu_buddy_allocator_init
 *
 * Steps:
 * - Check buddy allocator attribute values.
 *   - Use buddy allocator ops to check base, length, end and space values.
 *   - Confirm that values match init values.
 * - Test memory allocation functions.
 *   - Use alloc and alloc_fixed ops to allocate chunk of memory
 *   - Confirm allocation is successful.
 * - Test allocation ops with below vaiations.
 *   - Zero length memory segment.
 *   - Allocate more than available memory.
 *   - Unaligned base.
 *   - Base address equal to previously assigned carveout base.
 *   - Zero PTE size.
 *
 * Output: Returns SUCCESS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_nvgpu_buddy_allocator_basic_ops(struct unit_module *m,
						struct gk20a *g, void *args);

/**
 * Test specification for: test_nvgpu_buddy_allocator_destroy
 *
 * Description: Free buddy allocator used for previous tests.
 *
 * Test Type: Other (cleanup)
 *
 * Targets: nvgpu_allocator.ops.fini
 *
 * Input: test_nvgpu_buddy_allocator_init
 *
 * Steps:
 * - Free using buddy allocator fini ops
 * - Free nvgpu allocator
 *
 * Output: Returns SUCCESS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_nvgpu_buddy_allocator_destroy(struct unit_module *m,
						struct gk20a *g, void *args);

/**
 * Test specification for: test_nvgpu_buddy_allocator_alloc
 *
 * Description: Test cleanup branch of memory allocations.
 *
 * Test Type: Feature, Error injection
 *
 * Targets: nvgpu_allocator_init, nvgpu_allocator.ops.alloc,
 *          nvgpu_allocator.ops.alloc_fixed, nvgpu_buddy_allocator_flag_ops,
 *          nvgpu_allocator.ops.fini
 *
 * Input: None
 *
 * Steps:
 * - Allocate nvgpu and buddy allocator for this test
 *   - 4K base address.
 *   - 1M memory size.
 *   - 1K block size.
 *   - Zero max order.
 *   - flags set to 0.
 *   - NULL vm.
 *   - GVA space disabled.
 * - Inject fault at specific step to test alloc function cleanup.
 * - Allocate fixed memory segment, when part of memory is already allocated.
 *   - Use alloc_fixed to test if such allocation is successful
 * - Test buddy allocator destroy function
 *   - Increase count of buddy, split buddy and alloced buddy list lengths one
 *     by one and check if destroy function triggers BUG()
 * - Free nvgpu and buddy allocator used in this test
 *
 * Output: Returns SUCCESS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_nvgpu_buddy_allocator_alloc(struct unit_module *m,
						struct gk20a *g, void *args);

/**
 * Test specification for: test_buddy_allocator_with_small_pages
 *
 * Description: Test buddy allocator functions with big pages disabled.
 *
 * Test Type: Feature, Error injection
 *
 * Targets: nvgpu_allocator_init, nvgpu_buddy_allocator_init,
 *          nvgpu_buddy_check_argument_limits, nvgpu_allocator.ops.inited
 *          nvgpu_buddy_set_attributes, nvgpu_allocator.ops.alloc_pte,
 *          nvgpu_allocator.ops.alloc_fixed, nvgpu_allocator.ops.fini
 *
 * Input: None
 *
 * Steps:
 * - Initialize vm environment with below characteristics.
 *   - low_hole = 64K.
 *   - aperture_size = GK20A_PMU_VA_SIZE.
 *   - kernel_reserved = aperture_size - low_hole.
 *   - flags = GPU_ALLOC_GVA_SPACE, GVA space enabled.
 *   - userspace_managed = false, unified_va = false.
 *   - big_pages = false.
 * - Initialize buddy allocator for this test.
 *   - Base address = 1K.
 *   - Allocator size = 1M.
 *   - Block size = 1K.
 *   - max order = 10.
 *   - GVA space enabled.
 *   - vm initialized in the previous step.
 * - Test sincere allocations with buddy allocator ops.
 * - Test alloc ops with below variations.
 *   - Request more than available memory.
 *   - Alloc with size unaligned with respect to PTE size.
 *   - unusual page size.
 *   - Length zero memory segment.
 *   - Inject faults to test cleanup code.
 * - Free buddy allocator.
 * - Free vm environment.
 *
 * Output: Returns SUCCESS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_buddy_allocator_with_small_pages(struct unit_module *m,
						struct gk20a *g, void *args);

/**
 * Test specification for: test_buddy_allocator_with_big_pages
 *
 * Description: Test buddy allocator functions with big pages enabled.
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_allocator_init, nvgpu_buddy_allocator_init,
 *          nvgpu_buddy_check_argument_limits, nvgpu_buddy_set_attributes,
 *          nvgpu_allocator.ops.alloc_pte, nvgpu_allocator.ops.alloc_fixed
 *          nvgpu_allocator.ops.alloc, nvgpu_allocator.ops.free_alloc,
 *          nvgpu_allocator.ops.fini
 *
 * Input: None
 *
 * Steps:
 * - Initialize vm environment with below characteristics.
 *   - low_hole = 64K.
 *   - aperture_size = GK20A_PMU_VA_SIZE.
 *   - kernel_reserved = aperture_size - low_hole.
 *   - flags = GPU_ALLOC_GVA_SPACE, GVA space enabled.
 *   - userspace_managed = false, unified_va = false.
 *   - big_pages = true.
 * - Initialize buddy allocator for this test.
 *   - Base address = 64M, PDE aligned.
 *   - Allocator size = 256M.
 *   - Block size = 4K.
 *   - max order = GPU_BALLOC_MAX_ORDER.
 *   - GVA space enabled.
 *   - vm initialized in the previous step.
 * - Test sincere allocations with buddy allocator ops
 * - Test alloc ops with below variations
 *   - base address less than buddy allocator base address
 *   - unusual page size
 * - Free buddy allocator
 * - Free vm environment
 *
 * Output: Returns SUCCESS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_buddy_allocator_with_big_pages(struct unit_module *m,
						struct gk20a *g, void *args);

#endif /* UNIT_BUDDY_ALLOCATOR_H */
