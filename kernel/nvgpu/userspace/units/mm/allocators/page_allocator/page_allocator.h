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

#ifndef UNIT_PAGE_ALLOCATOR_H
#define UNIT_PAGE_ALLOCATOR_H

struct gk20a;
struct unit_module;

/** @addtogroup SWUTS-mm-allocators-page-allocator
 *  @{
 *
 * Software Unit Test Specification for mm.allocators.page_allocator
 */

/**
 * Test specification for: test_nvgpu_page_allocator_init
 *
 * Description: Initialize page allocator.
 *
 * Test Type: Feature, Error injection
 *
 * Targets: nvgpu_allocator_init, nvgpu_page_allocator_init,
 *          nvgpu_allocator.ops.fini
 *
 * Input: None
 *
 * Steps:
 * - Initialize page allocator with following characteristics.
 *   - 4K memory base address.
 *   - 1M length of memory.
 *   - 4K block size.
 * - Check that page allocator initializations fails for scenarios such as
 *   odd value of block_size and fault injection for memory allocation.
 *
 * Output: Returns SUCCESS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_nvgpu_page_allocator_init(struct unit_module *m,
					struct gk20a *g, void *args);

/**
 * Test specification for: test_nvgpu_page_allocator_ops
 *
 * Description: Test page allocator ops
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_allocator.ops.alloc, nvgpu_allocator.ops.free_alloc,
 *          nvgpu_allocator.ops.reserve_carveout,
 *          nvgpu_allocator.ops.release_carveout, nvgpu_allocator.ops.base,
 *          nvgpu_allocator.ops.end, nvgpu_allocator.ops.length,
 *          nvgpu_allocator.ops.inited, nvgpu_allocator.ops.space
 *
 * Input: test_nvgpu_page_allocator_init
 *
 * Steps:
 * - Check page_allocator attributes using allocator ops.
 *   - Execute allocator ops to read attibute value.
 *   - Confirm that value is equal to the default values set during
 *     initialization.
 * - Allocate carveout and confirm that allocation is successful. Check that
 *   carveout cannot be reserved after normal page allocation.
 *
 * Output: Returns SUCCESS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_nvgpu_page_allocator_ops(struct unit_module *m,
					struct gk20a *g, void *args);

/**
 * Test specification for: test_page_allocator_sgt_ops
 *
 * Description: Test page alloc sgt ops
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_page_alloc.nvgpu_sgt.nvgpu_sgt_ops.sgl_next,
 *          nvgpu_page_alloc.nvgpu_sgt.nvgpu_sgt_ops.sgl_phys,
 *          nvgpu_page_alloc.nvgpu_sgt.nvgpu_sgt_ops.sgl_ipa,
 *          nvgpu_page_alloc.nvgpu_sgt.nvgpu_sgt_ops.sgl_ipa_to_pa,
 *          nvgpu_page_alloc.nvgpu_sgt.nvgpu_sgt_ops.sgl_dma,
 *          nvgpu_page_alloc.nvgpu_sgt.nvgpu_sgt_ops.sgl_length,
 *          nvgpu_page_alloc.nvgpu_sgt.nvgpu_sgt_ops.sgl_gpu_addr,
 *          nvgpu_page_alloc.nvgpu_sgt.nvgpu_sgt_ops.sgt_free
 *
 * Input: test_nvgpu_page_allocator_init
 *
 * Steps:
 * - Check allocated page attributes using sgt ops
 * - Confirm that allocation details are equal to values set during allocation.
 *
 * Output: Returns SUCCESS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_page_allocator_sgt_ops(struct unit_module *m,
					struct gk20a *g, void *args);

/**
 * Test specification for: test_page_alloc_fixed
 *
 * Description: Allocate memory at fixed address
 *
 * Test Type: Feature, Error injection
 *
 * Targets: nvgpu_allocator.ops.alloc_fixed
 *
 * Input: test_nvgpu_page_allocator_init or test_page_allocator_init_slabs,
 *        args (fault_at_alloc_cache, fault_at_sgl_alloc, simple_alloc_128K,
 *        alloc_no_scatter_gather, failing_alloc_8K)
 *
 * Steps:
 * - Allocate chunk of memory at fixed address as per test_parameters input.
 * - Check that result is equal to test_parameters expected output.
 *
 * Output: Returns SUCCESS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_page_alloc_fixed(struct unit_module *m, struct gk20a *g, void *args);

/**
 * Test specification for: test_page_alloc
 *
 * Description: Allocate memory using page allocator
 *
 * Test Type: Feature, Error injection
 *
 * Targets: nvgpu_allocator.ops.alloc
 *
 * Input: test_nvgpu_page_allocator_init or test_page_allocator_init_slabs,
 *        args (fault_at_alloc_cache, fault_at_nvgpu_alloc,
 *        first_simple_alloc_32K, fault_at_sgl_alloc, alloc_no_scatter_gather,
 *        alloc_contiguous, simple_alloc_512K, alloc_more_than_available,
 *        fault_at_page_cache, second_simple_alloc_32K, third_simple_alloc_32K,
 *        fourth_simple_alloc_32K, simple_alloc_8K, failing_alloc_16K)
 *
 * Steps:
 * - Allocate chunk of memory at any address as per test_parameters input.
 * - Check that result is equal to test_parameters expected output.
 *
 * Output: Returns SUCCESS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_page_alloc(struct unit_module *m, struct gk20a *g, void *args);

/**
 * Test specification for: test_page_free
 *
 * Description: Free allocated memory
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_allocator.ops.free_alloc
 *
 * Input: test_nvgpu_page_allocator_init or test_page_allocator_init_slabs,
 *        args (alloc_no_scatter_gather, first_simple_alloc_32K,
 *        simple_alloc_512K, fourth_simple_alloc_32K, second_simple_alloc_32K,
 *        first_simple_alloc_32K, third_simple_alloc_32K)
 *
 * Steps:
 * - Free allocated memory at given address as per test_parameters input.
 *
 * Output: Returns SUCCESS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_page_free(struct unit_module *m, struct gk20a *g, void *args);

/**
 * Test specification for: test_page_free_fixed
 *
 * Description: Free allocated page at given address
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_allocator.ops.free_fixed
 *
 * Input: test_nvgpu_page_allocator_init, args (alloc_no_scatter_gather,
 *        simple_alloc_128K)
 *
 * Steps:
 * - Free allocated memory at fixed address as per test_parameters input.
 *
 * Output: Returns SUCCESS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_page_free_fixed(struct unit_module *m, struct gk20a *g, void *args);

/**
 * Test specification for: test_page_allocator_init_slabs
 *
 * Description: Initialize page allocator with slabs.
 *
 * Test Type: Feature, Error injection
 *
 * Targets: nvgpu_allocator_init, nvgpu_page_allocator_init,
 *          nvgpu_page_alloc_init_slabs, nvgpu_allocator.ops.fini
 *
 * Input: None
 *
 * Steps:
 * - Initialize page allocator with following characteristics.
 *   - 64K memory base address.
 *   - 128K length of memory.
 *   - 64K block size.
 *   - Flags set to GPU_ALLOC_4K_VIDMEM_PAGES to enable slabs.
 * - Check that page allocator initializations fails for scenarios such as
 *   odd value of block_size and fault injection for memory allocation.
 *
 * Output: Returns SUCCESS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_page_allocator_init_slabs(struct unit_module *m,
					struct gk20a *g, void *args);

/**
 * Test specification for: test_nvgpu_page_allocator_destroy
 *
 * Description: Destroy page allocator structure
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_allocator.ops.fini
 *
 * Input: test_nvgpu_page_allocator_init or test_page_allocator_init_slabs
 *
 * Steps:
 * - De-initialize page allocator structure.
 *
 * Output: Returns SUCCESS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_nvgpu_page_allocator_destroy(struct unit_module *m,
					struct gk20a *g, void *args);

/**
 * @}
 */

#endif /* UNIT_PAGE_ALLOCATOR_H */
