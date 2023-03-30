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

#ifndef UNIT_MM_MM_H
#define UNIT_MM_MM_H

struct gk20a;
struct unit_module;

/** @addtogroup SWUTS-mm-mm
 *  @{
 *
 * Software Unit Test Specification for mm.mm
 */

/**
 * Test specification for: test_mm_init_hal
 *
 * Description: The Enabled flags, HAL and register spaces must be initialized
 * properly before running any other tests.
 *
 * Test Type: Other (Init)
 *
 * Targets: gops_mm.init_bar2_vm, gops_mm.is_bar1_supported
 *
 * Input: None
 *
 * Steps:
 * - Set verbosity based on unit testing arguments.
 * - Initialize the platform:
 *   - Set the UNIFIED_MEMORY flag if iGPU configuration, disabled otherwise
 *   - Enable the following flags to enable various MM-related features:
 *     - NVGPU_SUPPORT_SEC2_VM
 *     - NVGPU_SUPPORT_GSP_VM
 *     - NVGPU_MM_FORCE_128K_PMU_VM
 * - Set all the minimum HAL needed for the mm.mm module.
 * - Register IO reg space for FB_MMU and HW_FLUSH.
 * - Ensure BAR1 support is disabled.
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_mm_init_hal(struct unit_module *m, struct gk20a *g, void *args);

/**
 * Test specification for: test_nvgpu_init_mm
 *
 * Description: The nvgpu_init_mm_support function must initialize all the
 * necessary components on the mm unit. It must also properly handle error
 * cases.
 *
 * Test Type: Feature, Error guessing
 *
 * Targets: gops_mm.init_mm_support, nvgpu_init_mm_support
 *
 * Input: test_mm_init_hal must have been executed successfully.
 *
 * Steps:
 * - Rely on error injection mechanisms to target all the possible error
 *   cases within the nvgpu_init_mm_support function. In particular, this step
 *   will use KMEM (malloc), DMA and HAL error injection mechanisms to
 *   selectively cause errors, and then check the error code to ensure the
 *   expected failure occurred.
 * - nvgpu_init_mm_support is then called and expected to succeed.
 * - Call nvgpu_init_mm_support again to test the case where initialization
 *   already succeeded.
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_nvgpu_init_mm(struct unit_module *m, struct gk20a *g, void *args);

/**
 * Test specification for: test_nvgpu_mm_setup_hw
 *
 * Description: The nvgpu_mm_setup_hw function must initialize all HW related
 * components on the mm unit. It must also properly handle error cases.
 *
 * Test Type: Feature, Error guessing
 *
 * Targets: gops_mm.setup_hw, nvgpu_mm_setup_hw
 *
 * Input: test_mm_init_hal and test_nvgpu_init_mm must have been executed
 * successfully.
 *
 * Steps:
 * - Rely on HAL error injection mechanisms to target all the possible error
 *   cases within the test_nvgpu_mm_setup_hw function.
 * - test_nvgpu_mm_setup_hw is then called and expected to succeed.
 * - Call nvgpu_init_mm_support again to test the case where initialization
 *   already succeeded and test a branch on set_mmu_page_size HAL.
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_nvgpu_mm_setup_hw(struct unit_module *m, struct gk20a *g, void *args);

/**
 * Test specification for: test_mm_suspend
 *
 * Description: The nvgpu_mm_suspend shall suspend the hardware-related
 * components by calling the relevant HALs to flush L2, disable FB interrupts
 * and disable MMU fault handling.
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_set_power_state, gops_mm.mm_suspend, nvgpu_mm_suspend
 *
 * Input: test_mm_init_hal, test_nvgpu_init_mm and test_nvgpu_mm_setup_hw must
 * have been executed successfully.
 *
 * Steps:
 * - Simulate that the GPU power is off.
 * - Run nvgpu_mm_suspend and check that it failed with -ETIMEDOUT.
 * - Simulate that power is on.
 * - Run nvgpu_mm_suspend and check that it succeeded.
 * - Define extra HALs. (intr disable, MMU fault disable)
 * - Simulate that power is on.
 * - Run nvgpu_mm_suspend and check that it succeeded.
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_mm_suspend(struct unit_module *m, struct gk20a *g, void *args);

/**
 * Test specification for: test_mm_remove_mm_support
 *
 * Description: The mm.remove_support operation (nvgpu_remove_mm_support
 * function) shall de-allocate all resources related to mm. In particular, it
 * is expected that nvgpu_remove_mm_support will call the nvgpu_pd_cache_fini
 * as its last step.
 *
 * Test Type: Feature
 *
 * Targets: gops_mm.pd_cache_init, nvgpu_pd_cache_init, gops_mm.remove_support
 *
 * Input: test_mm_init_hal, test_nvgpu_init_mm and test_nvgpu_mm_setup_hw must
 * have been executed successfully
 *
 * Steps:
 * - Allocate pd_cache by calling nvgpu_pd_cache_init.
 * - Call mm.remove_support.
 * - Verify that g->mm.pd_cache is NULL.
 * - Setup additional HALs for line/branch coverage: mmu_fault.info_mem_destroy
 *   and mm.remove_bar2_vm.
 * - Call mm.remove_support again.
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_mm_remove_mm_support(struct unit_module *m, struct gk20a *g,
				void *args);

/**
 * Test specification for: test_mm_page_sizes
 *
 * Description: The mm page size related operations shall provide information
 * about big page sizes available.
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_mm_get_default_big_page_size,
 * nvgpu_mm_get_available_big_page_sizes
 *
 * Input: test_mm_init_hal, test_nvgpu_init_mm and test_nvgpu_mm_setup_hw must
 * have been executed successfully.
 *
 * Steps:
 * - Call nvgpu_mm_get_default_big_page_size and check that it returns 64KB.
 * - Call nvgpu_mm_get_available_big_page_sizes and check that it returns 64KB.
 * - Disable big page support.
 * - Call nvgpu_mm_get_default_big_page_size and check that it returns 0.
 * - Call nvgpu_mm_get_available_big_page_sizes and check that it returns 0.
 * - Enable big page support.
 * - Setup the mm.gmmu.get_big_page_sizes HAL.
 * - Call nvgpu_mm_get_available_big_page_sizes and check that it returns a
 *   bitwise OR of SZ_64K and SZ_128K.
 * - Restore the mm.gmmu.get_big_page_sizes HAL to NULL.
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_mm_page_sizes(struct unit_module *m, struct gk20a *g, void *args);

/**
 * Test specification for: test_mm_inst_block
 *
 * Description: The nvgpu_inst_block_ptr shall return the base address of the
 * provided memory block, taking into account necessary RAMIN offset.
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_inst_block_ptr, gops_ramin.base_shift
 *
 * Input: test_mm_init_hal, test_nvgpu_init_mm and test_nvgpu_mm_setup_hw must
 * have been executed successfully.
 *
 * Steps:
 * - Create an arbitrary nvgpu_mem block with SYSMEM aperture and a well
 *   defined CPU VA.
 * - Setup the ramin.base_shift HAL.
 * - Call nvgpu_inst_block_ptr.
 * - Check that the returned address has been shifted by the same number of bits
 *   than provided by the ramin.base_shift HAL.
 * - For code coverage, enable NVGPU_SUPPORT_NVLINK, call nvgpu_inst_block_ptr
 *   again and check for the same bit shift as earlier.
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_mm_inst_block(struct unit_module *m, struct gk20a *g, void *args);

/**
 * Test specification for: test_mm_alloc_inst_block
 *
 * Description: The nvgpu_alloc_inst_block shall allocate DMA resources for a
 * given block.
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_alloc_inst_block
 *
 * Input: test_mm_init_hal, test_nvgpu_init_mm and test_nvgpu_mm_setup_hw must
 * have been executed successfully.
 *
 * Steps:
 * - Create an arbitrary nvgpu_mem block.
 * - Call nvgpu_alloc_inst_block and ensure it succeeded.
 * - Enable DMA fault injection.
 * - Call nvgpu_alloc_inst_block and ensure it did not succeed.
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_mm_alloc_inst_block(struct unit_module *m, struct gk20a *g,
				void *args);

/**
 * Test specification for: test_gk20a_from_mm
 *
 * Description: Simple test to check gk20a_from_mm.
 *
 * Test Type: Feature
 *
 * Targets: gk20a_from_mm
 *
 * Input: None
 *
 * Steps:
 * - Call gk20a_from_mm with the g->mm pointer and ensure it returns a
 *   pointer on g.
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_gk20a_from_mm(struct unit_module *m, struct gk20a *g, void *args);

/**
 * Test specification for: test_bar1_aperture_size_mb_gk20a
 *
 * Description: Simple test to check bar1_aperture_size_mb_gk20a.
 *
 * Test Type: Feature
 *
 * Targets: bar1_aperture_size_mb_gk20a
 *
 * Input: None
 *
 * Steps:
 * - Ensure that g->mm.bar1.aperture_size matches the expected value from
 *   bar1_aperture_size_mb_gk20a
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_bar1_aperture_size_mb_gk20a(struct unit_module *m, struct gk20a *g,
	void *args);
#endif /* UNIT_MM_MM_H */
