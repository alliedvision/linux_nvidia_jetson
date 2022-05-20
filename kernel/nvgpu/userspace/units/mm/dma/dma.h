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

#ifndef UNIT_DMA_H
#define UNIT_DMA_H

struct gk20a;
struct unit_module;

/** @addtogroup SWUTS-mm-dma
 *  @{
 *
 * Software Unit Test Specification for mm.dma
 */

/**
 * Test specification for: test_mm_dma_init
 *
 * Description: This test must be run once and be the first one as it
 * initializes the MM subsystem.
 *
 * Test Type: Feature, Other (setup)
 *
 * Targets: nvgpu_vm_init, nvgpu_iommuable
 *
 * Input: None
 *
 * Steps:
 * - Initialize the enabled flag NVGPU_MM_UNIFIED_MEMORY.
 * - Allocate a test buffer to be used as VIDMEM.
 * - Register test IO callbacks for PRAMIN.
 * - Set the ops.bus.set_bar0_window HAL.
 * - Set the ops.pramin.data032_r HAL.
 * - Register the BUS_BAR0 test IO space.
 * - Set all needed MM-related HALs.
 * - Ensure that MM HAL indicates that BAR1 is not supported.
 * - Create a test VM with big pages enabled.
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_mm_dma_init(struct unit_module *m, struct gk20a *g, void *__args);

/**
 * Test specification for: test_mm_dma_alloc
 *
 * Description: Test to target nvgpu_dma_alloc_* functions, testing automatic or
 * forced allocations in SYSMEM or VIDMEM.
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_dma_alloc_flags_sys, nvgpu_dma_free,
 * nvgpu_dma_alloc_flags_vid, nvgpu_dma_alloc, nvgpu_dma_alloc_sys
 *
 * Input: test_mm_dma_init
 *
 * Steps:
 * - Create a test nvgpu_mem instance (4 KB size with a static physical address)
 * - Set memory interface as non IOMMU-able, iGPU and SYSMEM.
 * - Create a DMA allocation on the nvgpu_mem instance and ensure it succeeds.
 * - Ensure the allocated DMA has a SYSMEM aperture.
 * - Free the allocation.
 * - Perform the same DMA allocation but explicitly request it to be performed
 *   in SYSMEM. Ensure it succeeded and has a SYSMEM aperture.
 * - Free the allocation.
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_mm_dma_alloc(struct unit_module *m, struct gk20a *g, void *args);

/**
 * Test specification for: test_mm_dma_alloc_flags
 *
 * Description: Test to target nvgpu_dma_alloc_flags_* functions, testing
 * several possible flags and SYSMEM/VIDMEM.
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_dma_alloc_flags_sys, nvgpu_dma_free,
 * nvgpu_dma_alloc_flags_vid, nvgpu_dma_free_sys, nvgpu_dma_alloc_flags
 *
 * Input: test_mm_dma_init
 *
 * Steps:
 * - Create a test nvgpu_mem instance (4 KB size with a static physical address)
 * - Set memory interface as non IOMMU-able, iGPU and SYSMEM.
 * - Create a DMA allocation on the nvgpu_mem instance with a READ_ONLY flag and
 *   ensure it succeeds.
 * - Ensure the allocated DMA has a SYSMEM aperture.
 * - Free the allocation.
 * - Perform the same DMA allocation with the NVGPU_DMA_PHYSICALLY_ADDRESSED
 *   flag. Ensure it succeeded and has a SYSMEM aperture.
 * - Free the allocation.
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_mm_dma_alloc_flags(struct unit_module *m, struct gk20a *g, void *args);

/**
 * Test specification for: test_mm_dma_alloc_map
 *
 * Description: Test to target nvgpu_dma_alloc_map_* functions, testing
 * allocations and GMMU mappings in SYSMEM or VIDMEM.
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_dma_alloc_map, nvgpu_dma_unmap_free, nvgpu_dma_alloc_map_sys,
 * nvgpu_dma_alloc_map_vid, nvgpu_dma_alloc_map_flags,
 * nvgpu_dma_alloc_map_flags_sys
 *
 * Input: test_mm_dma_init
 *
 * Steps:
 * - Create a test nvgpu_mem instance (4 KB size with a static physical address)
 * - Set memory interface as non IOMMU-able, iGPU and SYSMEM.
 * - Create a DMA allocation on the nvgpu_mem instance and map it, then ensure
 *   it succeeds.
 * - Ensure the allocated DMA has a SYSMEM aperture.
 * - Free and unmap the allocation.
 * - Perform the same DMA allocation/map but explicitly request it to be
 *   performed in SYSMEM. Ensure it succeeded and has a SYSMEM aperture.
 * - Free and unmap the allocation.
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_mm_dma_alloc_map(struct unit_module *m, struct gk20a *g, void *args);

/**
 * Test specification for: test_mm_dma_alloc_map_fault_injection
 *
 * Description: Test error handling branches in nvgpu_dma_alloc_map
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_dma_alloc_map, nvgpu_dma_unmap_free
 *
 * Input: test_mm_dma_init
 *
 * Steps:
 * - Create a test nvgpu_mem instance (4 KB size with a static physical address)
 * - Set memory interface as non IOMMU-able, iGPU and SYSMEM.
 * - Setup DMA fault injection to trigger at the next allocation.
 * - Try to perform an allocation and map and ensure it failed as expected.
 * - Reset fault injection to trigger at the 5th call in order to target the
 *   nvgpu_gmmu_map inside of the nvgpu_dma_alloc_flags_sys function.
 * - Try to perform an allocation and map and ensure it failed as expected.
 * - Disable fault injection.
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_mm_dma_alloc_map_fault_injection(struct unit_module *m,
						struct gk20a *g, void *args);
/** }@ */
#endif /* UNIT_DMA_H */
