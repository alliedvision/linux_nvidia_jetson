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

#ifndef UNIT_MM_AS_H
#define UNIT_MM_AS_H

struct gk20a;
struct unit_module;

/** @addtogroup SWUTS-mm-as
 *  @{
 *
 * Software Unit Test Specification for mm.as
 */

/** Special case to cause the gk20a_as_share malloc to fail */
#define SPECIAL_CASE_AS_MALLOC_FAIL	1
/** Special case to cause the VM init to fail */
#define SPECIAL_CASE_VM_INIT_FAIL	2
/**
 * Special case to cause the call to gk20a_busy to fail in gk20a_as_alloc_share
 */
#define SPECIAL_CASE_GK20A_BUSY_ALLOC	3
/**
 * Special case to cause the call to gk20a_busy to fail in
 * gk20a_as_release_share
 */
#define SPECIAL_CASE_GK20A_BUSY_RELEASE	4

/**
 * Structure to hold various parameters for the test_as_alloc_share function.
 */
struct test_parameters {
	/**
	 * Size of big pages
	 */
	int big_page_size;

	/**
	 * Address for small big page vma split
	 */
	unsigned long long small_big_split;

	/**
	 * Flags to use when calling gk20a_as_alloc_share. Should be one of the
	 * NVGPU_AS_ALLOC_* flag defined in as.h.
	 */
	int flags;

	/**
	 * The expected error code from gk20a_as_alloc_share. Typically 0 if
	 * the test is expecting success, or a specific error value if it is
	 * expecting failure.
	 */
	int expected_error;

	/**
	 * If true, enable NVGPU_MM_UNIFY_ADDRESS_SPACES before running
	 * gk20a_as_alloc_share (and disable it afterwards).
	 */
	bool unify_address_spaces_flag;

	/**
	 * One of the SPECIAL_CASE_* values defined above, to trigger special
	 * corner cases. No special case if set to 0.
	 */
	int special_case;
};

/**
 * Test specification for: test_init_mm
 *
 * Description: Test to initialize the mm.as environment.
 *
 * Test Type: Other (Init)
 *
 * Targets: nvgpu_init_mm_support, gk20a_as_alloc_share, nvgpu_ref_init
 *
 * Input: None
 *
 * Steps:
 * - Set all the minimum HAL needed for the mm.as unit.
 * - Call nvgpu_init_mm_support to initialize the mm subsystem and check the
 *   return code to ensure success.
 * - Call gk20a_as_alloc_share with zeroed parameters and ensure that it
 *   returns -ENODEV as expected since g->refcount has not been initialized.
 * - Initialize g->refcount.
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_init_mm(struct unit_module *m, struct gk20a *g, void *args);

/**
 * Test specification for: test_as_alloc_share
 *
 * Description: The AS unit shall be able to allocate address spaces based on
 * required flags, or report appropriate error codes in case of failures.
 *
 * Test Type: Feature
 *
 * Targets: gk20a_as_alloc_share, gk20a_as_release_share,
 * gk20a_vm_release_share, gk20a_from_as
 *
 * Input:
 * - The test_init_mm must have been executed
 * - The args argument points to an instance of struct test_parameters that
 *   contains the flags to be used, any special cases if needed and the expected
 *   return value from gk20a_as_alloc_share.
 *
 * Steps:
 * - Increment a global id counter used to track the allocations made later by
 *   calls to gk20a_as_alloc_share.
 * - Test if a special case is requested in the arguments and act accordingly
 *   by either:
 *   - enabling the NVGPU_MM_UNIFY_ADDRESS_SPACES flag
 *   - enabling KMEM fault injection
 *   - enabling nvgpu fault injection (for gk20a_busy)
 * - Call the gk20a_as_alloc_share with the flags and page size set in the
 *   arguments.
 * - If needed disable the NVGPU_MM_UNIFY_ADDRESS_SPACES.
 * - Disable all fault injection mechanisms.
 * - Compare the return code of gk20a_as_alloc_share with the one expected from
 *   the test arguments.
 * - If the call to gk20a_as_alloc_share was expected to succeed, compare the
 *   id of the allocated as with the global id counter to ensure they match.
 * - Enable nvgpu fault injection if a special case is enabled.
 * - Call the gk20a_as_release_share on the allocated as and collect its
 *   return value. Check the return value either for success or for an expected
 *   failure if fault injection was enabled.
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_as_alloc_share(struct unit_module *m, struct gk20a *g, void *args);

/**
 * Test specification for: test_gk20a_from_as
 *
 * Description: Simple test to check gk20a_from_as.
 *
 * Test Type: Feature
 *
 * Targets: gk20a_from_as
 *
 * Input: None
 *
 * Steps:
 * - Call gk20a_from_as with an 'as' pointer and ensure it returns a
 *   pointer on g.
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_gk20a_from_as(struct unit_module *m, struct gk20a *g, void *args);

#endif /* UNIT_MM_AS_H */
