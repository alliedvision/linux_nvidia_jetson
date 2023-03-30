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

#ifndef UNIT_PD_CACHE_H
#define UNIT_PD_CACHE_H

struct gk20a;
struct unit_module;

/** @addtogroup SWUTS-mm-gmmu-pd_cache
 *  @{
 *
 * Software Unit Test Specification for mm.gmmu.pd_cache
 */

/**
 * Test specification for: test_pd_cache_init
 *
 * Description: Test to cover the initialization routines of pd_cache.
 *
 * Test Type: Feature, Error Injection
 *
 * Targets: gops_mm.pd_cache_init, nvgpu_pd_cache_init
 *
 * Input: None
 *
 * Steps:
 * - Check that init with a memory failure returns -ENOMEM and that the pd_cache
 *   is not initialized.
 * - Perform a normal initialization and ensure that all the expected data
 *   structures were initialized.
 * - Perform the initialization again and  make sure that any re-init call
 *   doesn't blow away a previously inited pd_cache.
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_pd_cache_init(struct unit_module *m, struct gk20a *g, void *__args);

/**
 * Test specification for: test_pd_cache_fini
 *
 * Description: Test to cover the de-initialization routines of pd_cache.
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_pd_cache_fini
 *
 * Input: test_pd_cache_init
 *
 * Steps:
 * - Check that de-initializing the pd_cache results in a NULL pointer.
 * - Call the de-initialization again and ensure it doesn't cause a crash.
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_pd_cache_fini(struct unit_module *m, struct gk20a *g, void *__args);

/**
 * Test specification for: test_pd_cache_valid_alloc
 *
 * Description: Checks that pd_cache allocates suitable DMA'able buffer of
 * memory, that it is sufficiently aligned for use by the GMMU and it can
 * allocate valid PDs.
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_pd_alloc, nvgpu_pd_write, nvgpu_pd_free, nvgpu_pd_cache_fini
 *
 * Input: None
 *
 * Steps:
 * - Initialize a pd_cache.
 * - Allocate a PD of each valid PD size and ensure they are properly
 *   populated with nvgpu_mem data. This tests read/write and alignment.
 *   - Do a write to the zeroth word and then verify this made it to
 *     the nvgpu_mem. Using the zeroth word makes it easy to read back.
 *   - Check alignment is at least as much as the size.
 *   - Free the PD.
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_pd_cache_valid_alloc(struct unit_module *m, struct gk20a *g,
	void *__args);

/**
 * Test specification for: test_per_pd_size
 *
 * Description: Checks that pd_cache allocations are successful in a number of
 * supported sizes.
 *
 * Test Type: Feature
 *
 * Targets: gops_mm.pd_cache_init, nvgpu_pd_cache_init, nvgpu_pd_alloc,
 * nvgpu_pd_free, nvgpu_pd_cache_fini
 *
 * Input: None
 *
 * Steps:
 * - Initialize a pd_cache.
 * - Set PD size to 256 bytes (i.e. minimum PD size)
 * - While the PD size is smaller than the page size:
 *   - Call one of 2 scenario:
 *     - Ensure that 16 256B, 8 512B, etc, PDs can fit into a single page sized
 *       DMA allocation.
 *     - Ensure that previously allocated PD entries are re-usable.
 *   - Double the PD size.
 * - De-allocate the pd_cache.
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_per_pd_size(struct unit_module *m, struct gk20a *g, void *__args);

/**
 * Test specification for: test_pd_write
 *
 * Description: Ensure that the pd_cache writes a word of memory in a
 * passed PD with 2 word or 4 word PDE/PTE.
 *
 * Test Type: Feature
 *
 * Targets: gp10b_mm_get_mmu_levels, gops_mm.pd_cache_init, nvgpu_pd_cache_init,
 * nvgpu_pd_alloc, nvgpu_pd_offset_from_index, nvgpu_pd_write, nvgpu_pd_free,
 * nvgpu_pd_cache_fini
 *
 * Input: None
 *
 * Steps:
 * - Initialize a pd_cache.
 * - Allocate 2 test PD with page size 4KB.
 * - Iterate over the 3 supported index sizes: 0, 16, 255:
 *   - Get the PD offset from the current index at the 3rd level and 4th level
 *     (respectively for 2 word and 4 word PDE/PTE.)
 *   - Write a known 32-bit pattern as a PD.
 *   - Read back the pattern and ensure it matches the written value.
 * - De-allocate the 2 test PD.
 * - De-allocate the pd_cache.
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_pd_write(struct unit_module *m, struct gk20a *g, void *__args);

/**
 * Test specification for: test_gpu_address
 *
 * Description: Ensure the pd_cache does provide a valid GPU physical address
 * for a given PD.
 *
 * Test Type: Feature
 *
 * Targets: gops_mm.pd_cache_init, nvgpu_pd_cache_init, nvgpu_pd_alloc,
 * nvgpu_pd_gpu_addr, nvgpu_pd_free, nvgpu_pd_cache_fini
 *
 * Input: None
 *
 * Steps:
 * - Initialize a pd_cache.
 * - Allocate a test PD with page size 4KB.
 * - Get the GPU address of the allocated PD and ensure it is not NULL.
 * - De-allocate the test PD.
 * - De-allocate the pd_cache.
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_gpu_address(struct unit_module *m, struct gk20a *g, void *__args);

/**
 * Test specification for: test_offset_computation
 *
 * Description: Ensure that the pd_cache unit returns a valid word offset for
 * 2 and 4 word PDE/PTE.
 *
 * Test Type: Feature
 *
 * Targets: gp10b_mm_get_mmu_levels, nvgpu_pd_offset_from_index
 *
 * Input: None
 *
 * Steps:
 * - Get all supported MMU levels.
 * - Iterate over 4 index sizes: 0, 4, 16, 255.
 *   - Get the offset for a 2 word PDE/PTE and ensure it matches the expected
 *     value.
 *   - Get the offset for a 4 word PDE/PTE and ensure it matches the expected
 *     value.
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_offset_computation(struct unit_module *m, struct gk20a *g,
	void *__args);

/**
 * Test specification for: test_init_deinit
 *
 * Description: Ensure that the initialization routines of pd_cache handle all
 * corner cases appropriately.
 *
 * Test Type: Feature, Error injection
 *
 * Targets: gops_mm.pd_cache_init, nvgpu_pd_cache_init, nvgpu_pd_alloc,
 * nvgpu_pd_cache_fini, nvgpu_pd_free
 *
 * Input: None
 *
 * Steps:
 * - Initialize a pd_cache.
 * - Allocate a test PD with page size 4KB.
 * - Enable memory and DMA fault injection.
 * - Call the pd_cache initialization again.
 * - Since the pd_cache was already initialized, ensure the previous call
 *   still reported success, confirming that no further allocations were made.
 * - Disable fault injection.
 * - De-allocate the test PD.
 * - De-allocate the pd_cache.
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_init_deinit(struct unit_module *m, struct gk20a *g, void *__args);

/**
 * Test specification for: test_pd_cache_alloc_gen
 *
 * Description: Simple test that will perform allocations. It allocates
 * nr allocs of the passed size either all at once or in an interleaved
 * pattern.
 * If nr_allocs_before_free is set then this value will determine how many
 * allocs to do before trying frees. If unset it will be simply be nr.
 * If nr_free_before_alloc is set this will determine the number of frees to
 * do before swapping back to allocs. This way you can control the interleaving
 * pattern to some degree. If not set it defaults to nr_allocs_before_free.
 * Anything left over after the last free loop will be freed in one big loop.
 *
 * Test Type: Feature
 *
 * Targets: gops_mm.pd_cache_init, nvgpu_pd_cache_init, nvgpu_pd_alloc,
 * nvgpu_pd_cache_fini, nvgpu_pd_free
 *
 * Input: None
 *
 * Steps:
 * - Initialize a pd_cache.
 * - If there is no requested "allocs before free" value, set it to the
 *   requested total number of allocations. Also set the number of "frees before
 *   alloc" to 0.
 * - Loop over the requested number of allocations with index 'i':
 *   - Loop from 0 to the requested number of "allocs before free":
 *     - Perform a PD allocation of the requested size.
 *   - Loop from 0 to the requested number of "frees before alloc":
 *     - Perform a PD free of allocation at index 'i'.
 * - Loop backwards to free all the allocations.
 * - Loop over all the PD allocation handles and ensure they have been zero'ed
 *   out as expected.
 * - De-allocate the pd_cache.
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_pd_cache_alloc_gen(struct unit_module *m, struct gk20a *g,
	void *__args);

/**
 * Test specification for: test_pd_free_empty_pd
 *
 * Description: Test free on empty PD cache and extra corner cases.
 *
 * Test Type: Feature, Error injection
 *
 * Targets: gops_mm.pd_cache_init, nvgpu_pd_cache_init, nvgpu_pd_alloc,
 * nvgpu_pd_cache_fini, nvgpu_pd_free
 *
 * Input: None
 *
 * Steps:
 * - Initialize a pd_cache.
 * - Allocate a test PD with a 2KB page size (cached).
 * - Free the test PD.
 * - Attempt to free the test PD again and ensure it causes a call to BUG().
 * - Attempt another free with pd.mem set to NULL and ensure it causes a call to
 *   BUG().
 * - Allocate a test PD with a 4KB page size (direct).
 * - Free the test PD.
 * - Call the free again which should not cause a BUG().
 * - Call the free again with pd.mem set to NULL which should not cause a BUG().
 * - De-allocate the pd_cache.
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_pd_free_empty_pd(struct unit_module *m, struct gk20a *g,
	void *__args);

/**
 * Test specification for: test_pd_alloc_invalid_input
 *
 * Description: Test invalid nvgpu_pd_alloc() calls. Invalid bytes,
 * invalid pd_cache, etc.
 *
 * Test Type: Error injection
 *
 * Targets: gops_mm.pd_cache_init, nvgpu_pd_cache_init, nvgpu_pd_alloc,
 * nvgpu_pd_cache_fini
 *
 * Input: None
 *
 * Steps:
 * - Ensure that no pd_cache is initialized in the system.
 * - Attempt to perform an allocation and ensure it causes a call to BUG().
 * - Initialize a pd_cache.
 * - Perform several allocation attempts with invalid sizes and ensure all
 *   calls report a failure.
 * - De-allocate the pd_cache.
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_pd_alloc_invalid_input(struct unit_module *m, struct gk20a *g,
	void *__args);

/**
 * Test specification for: test_pd_alloc_direct_fi
 *
 * Description: Test invalid nvgpu_pd_alloc() when out of memory conditions
 * occur for direct allocations.
 *
 * Test Type: Error injection
 *
 * Targets: nvgpu_pd_cache_init, nvgpu_pd_alloc, gops_mm.pd_cache_init,
 * nvgpu_pd_cache_fini
 *
 * Input: None
 *
 * Steps:
 * - Initialize a pd_cache.
 * - Enable kernel memory error injection.
 * - Try to perform a PD allocation and ensure it failed.
 * - Disable kernel memory error injection.
 * - Enable DMA memory error injection.
 * - Try to perform a PD allocation and ensure it failed.
 * - Disable DMA memory error injection.
 * - De-allocate the pd_cache.
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_pd_alloc_direct_fi(struct unit_module *m, struct gk20a *g, void *args);

/**
 * Test specification for: test_pd_alloc_fi
 *
 * Description: Test invalid nvgpu_pd_alloc() when out of memory conditions
 * occur for nvgpu_pd_alloc_new allocations.
 *
 * Test Type: Error injection
 *
 * Targets: gops_mm.pd_cache_init, nvgpu_pd_cache_init, nvgpu_pd_alloc,
 * nvgpu_pd_cache_fini
 *
 * Input: None
 *
 * Steps:
 * - Initialize a pd_cache.
 * - Enable kernel memory error injection.
 * - Try to perform a PD allocation and ensure it failed.
 * - Disable kernel memory error injection.
 * - Enable DMA memory error injection.
 * - Try to perform a PD allocation and ensure it failed.
 * - Disable DMA memory error injection.
 * - De-allocate the pd_cache.
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_pd_alloc_fi(struct unit_module *m, struct gk20a *g, void *args);

/** }@ */
#endif /* UNIT_PD_CACHE_H */
