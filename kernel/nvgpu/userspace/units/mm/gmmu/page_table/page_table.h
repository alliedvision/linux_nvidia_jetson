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

#ifndef UNIT_PAGE_TABLE_H
#define UNIT_PAGE_TABLE_H

struct gk20a;
struct unit_module;

/** @addtogroup SWUTS-mm-gmmu-page_table
 *  @{
 *
 * Software Unit Test Specification for mm.gmmu.page_table
 */

/**
 * Test specification for: test_nvgpu_gmmu_map_unmap_map_fail
 *
 * Description: Test special corner cases causing map to fail. Mostly to cover
 * error handling and some branches.
 *
 * Test Type: Feature, Error injection
 *
 * Targets: nvgpu_gmmu_map
 *
 * Input: args as an int to choose a supported scenario.
 *
 * Steps:
 * - Instantiate a nvgpu_mem instance, with a known size and PA.
 * - Depending on one of the supported scenario (passed as argument):
 *   - Enable fault injection to trigger a NULL SGT.
 *   - Enable fault injection to trigger a failure in pd_allocate().
 *   - Enable fault injection to trigger a failure in pd_allocate_children().
 *   - Set the VM PMU as guest managed to make __nvgpu_vm_alloc_va() fail.
 * - Call the nvgpu_gmmu_map() function and ensure it failed as expected.
 * - Disable error injection.
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_nvgpu_gmmu_map_unmap_map_fail(struct unit_module *m, struct gk20a *g,
	void *__args);

/**
 * Test specification for: test_nvgpu_gmmu_map_unmap
 *
 * Description: This test does a simple map and unmap of a buffer. Several
 * parameters can be changed and provided in the args. This test will also
 * attempt to compare the data in PTEs to the parameters provided.
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_gmmu_map_fixed, gops_mm_gmmu.map, nvgpu_gmmu_map,
 * nvgpu_get_pte, gops_mm_gmmu.unmap, nvgpu_gmmu_unmap
 *
 * Input: args as a struct test_parameters to hold scenario and test parameters.
 *
 * Steps:
 * - Instantiate a nvgpu_mem instance, with a known size and PA.
 * - If scenario requires a fixed mapping, call nvgpu_gmmu_map_fixed() with a
 *   known fixed PA. Otherwise call nvgpu_gmmu_map().
 * - Check that the mapping succeeded.
 * - Check that the mapping is 4KB aligned.
 * - Get the PTE from the mapping and ensure it actually exists.
 * - Make sure the PTE is marked as valid.
 * - Make sure the PTE matches the PA that was mapped.
 * - Depending on the scenario's mapping flags, check RO and RW bits in PTE.
 * - Depending on the scenario's privileged flag, check that the PTE is correct.
 * - Depending on the scenario's cacheable flag, check that the PTE is correct.
 * - Unmap the buffer.
 * - Ensure that the PTE is now invalid.
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_nvgpu_gmmu_map_unmap(struct unit_module *m, struct gk20a *g,
	void *args);

/**
 * Test specification for: test_nvgpu_gmmu_map_unmap_adv
 *
 * Description: Similar to test_nvgpu_gmmu_map_unmap but supports more advanced
 * parameters and creates a test SGT.
 *
 * Test Type: Feature
 *
 * Targets: gops_mm_gmmu.map, nvgpu_gmmu_map_locked,
 * gops_mm_gmmu.unmap, nvgpu_gmmu_unmap, gk20a_from_vm
 *
 * Input: args as a struct test_parameters to hold scenario and test parameters.
 *
 * Steps:
 * - Instantiate a nvgpu_mem instance, with a known size and PA.
 * - Create an SGT with a custom SGL.
 * - Perform a mapping using the SGT and the parameters in argument.
 * - Ensure the mapping succeeded and is 4KB-aligned.
 * - Unmap the buffer.
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_nvgpu_gmmu_map_unmap_adv(struct unit_module *m, struct gk20a *g,
	void *args);

/**
 * Test specification for: test_nvgpu_gmmu_map_unmap_batched
 *
 * Description: This tests uses the batch mode and maps 2 buffers. Then it
 * checks that the flags in the batch structure were set correctly.
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_gmmu_map_locked, nvgpu_gmmu_unmap, gops_mm_gmmu.unmap,
 * nvgpu_gmmu_unmap_locked
 *
 * Input: args as a struct test_parameters to hold scenario and test parameters.
 *
 * Steps:
 * - Instantiate 2 nvgpu_mem instances, with a known size and PA.
 * - Create an SGT with a custom SGL.
 * - Perform a mapping using the SGT, the parameters in argument and an instance
 *   of struct vm_gk20a_mapping_batch.
 * - Repeat for the 2nd nvgpu_mem instance.
 * - Ensure the need_tlb_invalidate of the batch is set as expected.
 * - Reset the need_tlb_invalidate flag.
 * - Unmap both buffers.
 * - Ensure the need_tlb_invalidate of the batch is set as expected.
 * - Ensure the gpu_l2_flushed of the batch is set as expected.
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_nvgpu_gmmu_map_unmap_batched(struct unit_module *m, struct gk20a *g,
	void *args);

/**
 * Test specification for: test_nvgpu_page_table_c1_full
 *
 * Description: Test case to cover NVGPU-RQCD-45 C1.
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_vm_init, nvgpu_gmmu_map, gops_mm_gmmu.map,
 * nvgpu_gmmu_map_locked, gops_mm_gmmu.unmap, nvgpu_gmmu_unmap,
 * nvgpu_vm_put
 *
 * Input: None
 *
 * Steps:
 * - Create a test VM.
 * - Create a 64KB-aligned and a 4KB-aligned nvgpu_mem objects.
 * - Create an nvgpu_mem object with a custom SGL composed of blocks of length
 *   4KB or 64KB.
 * - For each of the nvgpu_mem objects:
 *   - Map the buffer.
 *   - Ensure mapping succeeded.
 *   - Ensure alignment is correct.
 *   - Ensure that the page table attributes are correct.
 *   - Unmap the buffer.
 *   - Ensure the corresponding PTE is not valid anymore.
 * - Free the VM.
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_nvgpu_page_table_c1_full(struct unit_module *m, struct gk20a *g,
	void *args);

/**
 * Test specification for: test_nvgpu_page_table_c2_full
 *
 * Description: Test case to cover NVGPU-RQCD-45 C2.
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_vm_init, gops_mm_gmmu.map, nvgpu_gmmu_map_fixed,
 * gops_mm_gmmu.unmap, nvgpu_gmmu_unmap, nvgpu_vm_put
 *
 * Input: None
 *
 * Steps:
 * - Create a test VM.
 * - Create a 64KB-aligned nvgpu_mem object.
 * - Perform a fixed allocation, check properties and unmap.
 * - Repeat the same operation to ensure it still succeeds (thereby ensuring
 *   the first unmap was done properly).
 * - Change the nvgpu_mem object to be 4KB aligned.
 * - Repeat the mapping/check/unmapping operation and check for success to
 *   ensure page markers were cleared properly during previous allocations.
 * - Free the VM.
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_nvgpu_page_table_c2_full(struct unit_module *m, struct gk20a *g,
	void *args);

/**
 * Test specification for: test_nvgpu_gmmu_init_page_table_fail
 *
 * Description: Test special corner cases causing nvgpu_gmmu_init_page_table
 * to fail, mostly to cover error handling and some branches.
 *
 * Test Type: Error injection
 *
 * Targets: nvgpu_gmmu_init_page_table
 *
 * Input: None
 *
 * Steps:
 * - Enable KMEM fault injection.
 * - Call nvgpu_gmmu_init_page_table.
 * - Disable KMEM fault injection.
 * - Ensure that nvgpu_gmmu_init_page_table failed as expected.
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_nvgpu_gmmu_init_page_table_fail(struct unit_module *m,
					struct gk20a *g, void *args);

/**
 * Test specification for: test_nvgpu_gmmu_set_pte
 *
 * Description: This test targets the nvgpu_set_pte() function by mapping a
 * buffer, and then trying to alter the validity bit of the corresponding PTE.
 *
 * Test Type: Feature, Error injection
 *
 * Targets: nvgpu_get_pte, nvgpu_set_pte, nvgpu_pte_words
 *
 * Input: None
 *
 * Steps:
 * - Map a test buffer (dynamic) and get the assigned GPU VA.
 * - Ensure the mapping succeeded.
 * - Check that nvgpu_pte_words returns the expected value (2).
 * - Use nvgpu_get_pte to retrieve the PTE from the assigned GPU VA, ensure
 *   it is valid.
 * - Call nvgpu_set_pte with an invalid address and ensure it failed.
 * - Using nvgpu_set_pte, rewrite the PTE with the validity bit flipped and
 *   ensure it reports success.
 * - Retrieve the PTE again, ensure it succeeds and then check that the PTE
 *   is invalid.
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
static int test_nvgpu_gmmu_set_pte(struct unit_module *m,
					struct gk20a *g, void *args);

/**
 * Test specification for: test_nvgpu_gmmu_init
 *
 * Description: This test must be run once and be the first one as it
 * initializes the MM subsystem.
 *
 * Test Type: Other (setup), Feature
 *
 * Targets: nvgpu_gmmu_init_page_table, nvgpu_vm_init
 *
 * Input: None
 *
 * Steps:
 * - Set debug log masks if needed.
 * - For iGPU, enable the following flags: NVGPU_MM_UNIFIED_MEMORY,
 *   NVGPU_USE_COHERENT_SYSMEM, NVGPU_SUPPORT_NVLINK
 * - Setup all the needed HALs.
 * - Create a test PMU VM to be used by other tests which will cause the
 *   nvgpu_gmmu_init_page_table function to be called.
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_nvgpu_gmmu_init(struct unit_module *m, struct gk20a *g, void *args);

/**
 * Test specification for: test_nvgpu_gmmu_clean
 *
 * Description: This test should be the last one to run as it de-initializes
 * components.
 *
 * Test Type: Other (cleanup)
 *
 * Targets: None
 *
 * Input: None
 *
 * Steps:
 * - Set log mask to 0.
 * - Call nvgpu_vm_put to remove the test VM.
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_nvgpu_gmmu_clean(struct unit_module *m, struct gk20a *g, void *args);

/**
 * Test specification for: test_nvgpu_gmmu_perm_str
 *
 * Description: Tests all supported combinations of permissions on the
 * nvgpu_gmmu_perm_str function.
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_gmmu_perm_str
 *
 * Input: None
 *
 * Steps:
 * - Call nvgpu_gmmu_perm_str with flag gk20a_mem_flag_none and ensure it
 *   returns "RW"
 * - Call nvgpu_gmmu_perm_str with flag gk20a_mem_flag_write_only and ensure it
 *   returns "WO"
 * - Call nvgpu_gmmu_perm_str with flag gk20a_mem_flag_read_only and ensure it
 *   returns "RO"
 * - Call nvgpu_gmmu_perm_str with an invalid flag and ensure it
 *   returns "??"
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_nvgpu_gmmu_perm_str(struct unit_module *m, struct gk20a *g,
	void *args);
/** }@ */
#endif /* UNIT_PAGE_TABLE_H */
