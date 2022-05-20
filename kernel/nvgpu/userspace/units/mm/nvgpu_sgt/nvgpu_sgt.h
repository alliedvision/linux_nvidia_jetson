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

#ifndef UNIT_NVGPU_SGT_H
#define UNIT_NVGPU_SGT_H

struct gk20a;
struct unit_module;

/** @addtogroup SWUTS-mm-nvgpu-sgt
 *  @{
 *
 * Software Unit Test Specification for mm-nvgpu-sgt
 */

/**
 * Test specification for: test_nvgpu_sgt_basic_apis
 *
 * Description: Tests for the simple APIs provided by nvgpu_sgt unit.
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_mem_posix_create_from_list, nvgpu_sgt_create_from_mem,
 * nvgpu_sgt_get_phys, nvgpu_sgt_get_dma, nvgpu_sgt_iommuable,
 * nvgpu_sgt_get_gpu_addr, nvgpu_sgt_get_ipa, nvgpu_sgt_get_length,
 * nvgpu_sgt_ipa_to_pa
 *
 * Input: None
 *
 * Steps:
 * - Test nvgpu_sgt_create_from_mem.
 *   - Create an nvgpu_mem object from an sgl.
 *   - Set VA for nvgpu_mem object.
 *   - Pass nvgpu_mem object to nvgpu_sgt_create_from_mem(), and verify pointer
 *     returned.
 * - Test nvgpu_sgt_get_phys by checking the physical address in sgt above is
 *   set to the VA set in the nvgpu_mem.
 * - Test nvgpu_sgt_get_dma by passing in the sgl from sgt above, and verify
 *   correct DMA address is returned.
 * - Test nvgpu_sgt_get_length by passing in the sgl from sgt above, and verify
 *   correct sgl length is returned.
 * - Test nvgpu_sgt_iommuable.
 *   - Call nvgpu_sgt_iommuable() and verify returned value maches what is set
 *     posix struct member mm_sgt_is_iommuable.
 *   - Override the nvgpu_sgt_ops HAL with the sgt_iommuable op set to NULL.
 *   - Call nvgpu_sgt_iommuable() and verify false is returned.
 * - Using an overridden HAL op to return an expected value, call
 *   nvgpu_sgt_get_gpu_addr() and verify the returned value is correct.
 * - Using an overridden HAL op to return an expected value, call
 *   nvgpu_sgt_get_ipa() and verify the returned value is correct.
 * - Using an overridden HAL op to return an expected value, call
 *   nvgpu_sgt_ipa_to_pa() and verify the returned value is correct.
 * - Call nvgpu_sgt_free(), passing a NULL pointer for the sgt to test the error
 *   checking path.
 * - Call nvgpu_sgt_free(), passing the sgt used previously in this test.
 * - Restore default nvgpu_sgt_ops HALs.
 *
 * Output: Returns SUCCESS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_nvgpu_sgt_basic_apis(struct unit_module *m, struct gk20a *g,
				     void *args);

/**
 * Test specification for: test_nvgpu_sgt_get_next
 *
 * Description: Tests test_nvgpu_sgt_get_next API by building sgl's and
 *              verifying correct pointers returned by calling the API.
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_mem_sgt_posix_create_from_list, nvgpu_sgt_get_next
 *
 * Input: None
 *
 * Steps:
 * - Create a table of sgl's.
 * - Create an sgt from the list of sgl's.
 * - Call nvgpu_sgt_get_next() in a loop and verify it returns the list above
 *   in the correct order and the final element returned is NULL.
 *
 * Output: Returns SUCCESS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_nvgpu_sgt_get_next(struct unit_module *m, struct gk20a *g,
				   void *args);

/**
 * Test specification for: test_nvgpu_sgt_alignment_non_iommu
 *
 * Description: Test the alignment API for the case where there is no IOMMU.
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_mem_sgt_posix_create_from_list, nvgpu_sgt_alignment,
 * nvgpu_sgt_free
 *
 * Input: Static sgt_align_test_array table of alignment combinations.
 *
 * Steps:
 * - Loop through the table of test alignment combinations. For each config:
 *   - Create an sgt.
 *   - Call nvgpu_sgt_alignment() and verify the expected alignment is returned.
 *   - Free the sgt.
 *
 * Output: Returns SUCCESS if the steps above were executed successfully. FAIL
 * otherwise.
 */
/*
 * Test: test_nvgpu_sgt_alignment_non_iommu
 * Walks table above to test a variety of different sgl's to test alignment
 * API calculates correctly when there is no IOMMU.
 */
int test_nvgpu_sgt_alignment_non_iommu(struct unit_module *m,
					      struct gk20a *g, void *args);

/**
 * Test specification for: test_nvgpu_sgt_alignment_with_iommu
 *
 * Description: Test the alignment API for the case where there is an IOMMU.
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_sgt_create_from_mem, nvgpu_sgt_alignment, nvgpu_sgt_free
 *
 * Input: None
 *
 * Steps:
 * - Create an sgt.
 * - Test the code paths for not using the IOMMU for alignment.
 *   - Cycle through all combinations of the following conditions that gate
 *     using the IOMMU address:
 *     - IOMMU enabled.
 *     - The sgt being iommuable.
 *     - The sgl's DMA address not equal 0.
 *   - For each case, verify nvgpu_set_alignment() does not return the DMA
 *     address of the sgl (the IOMMU address).
 * - Test when the IOMMU is enabled, the sgt is iommuable, and the sgl's DMA
 *   address is not 0, that nvgpu_set_alignment() does return the sgl's DMA
 *   address for the alignment.
 * - Free the sgt.
 *
 * Output: Returns SUCCESS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_nvgpu_sgt_alignment_with_iommu(struct unit_module *m,
					       struct gk20a *g, void *args);

#endif /* UNIT_NVGPU_SGT_H */
