/*
 * Copyright (c) 2018-2019, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef __UNIT_FAULT_INJECTION_KMEM_H__
#define __UNIT_FAULT_INJECTION_KMEM_H__

struct gk20a;
struct unit_module;

/** @addtogroup SWUTS-posix-fault-injection
 *  @{
 */

/**
 * Test specification for: test_kmem_init
 *
 * Description: Initialization required for kmem fault injection tests.
 *
 * Test Type: Other (Setup)
 *
 * Input: test_fault_injection_init() must have been called prior to this test.
 *
 * Steps:
 * - Get the pointer to the kmem fault injection object.
 *
 * Output: Returns SUCCESS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_kmem_init(struct unit_module *m,
		   struct gk20a *g, void *__args);


/**
 * Test specification for: test_kmem_cache_fi_default
 *
 * Description: This test simply tests the default case of fault injection
 *              disabled for calling kmem cache routines.
 *
 * Test Type: Feature Based
 *
 * Input: test_fault_injection_init() & test_kmem_init() must have been called
 *        prior to this test.
 *
 * Steps:
 * - Verify the kmem fault injection is disabled.
 * - Create a kmem cache object and verify the return is non-NULL.
 * - Destroy the kmem cache object.
 *
 * Output: Returns SUCCESS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_kmem_cache_fi_default(struct unit_module *m,
			       struct gk20a *g, void *__args);

/**
 * Test specification for: test_kmem_cache_fi_enabled
 *
 * Description: This test validates immediate fault injection for kmem cache
 *              create.
 *
 * Test Type: Feature Based
 *
 * Input: test_fault_injection_init() & test_kmem_init() must have been called
 *        prior to this test.
 *
 * Steps:
 * - Enable kmem fault injection immediately.
 * - Create a kmem cache object and verify the return is NULL, indicating
 *   failure.
 *
 * Output: Returns SUCCESS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_kmem_cache_fi_enabled(struct unit_module *m,
			       struct gk20a *g, void *__args);

/**
 * Test specification for: test_kmem_cache_fi_delayed_enable
 *
 * Description: This test validates delayed enable of fault injection for kmem
 *              cache APIs.
 *
 * Test Type: Feature Based
 *
 * Input: test_fault_injection_init() & test_kmem_init() must have been called
 *        prior to this test.
 *
 * Steps:
 * - Enable kmem fault injection for after 2 calls.
 * - Create a kmem cache object and verify the return is non-NULL, indicating
 *   pass.
 * - Allocate from the kmem cache object and verify the 1st call passes.
 * - Allocate from the kmem cache object and verify the 2nd call fails.
 * - Cleanup the allocated cache.
 *
 * Output: Returns SUCCESS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_kmem_cache_fi_delayed_enable(struct unit_module *m,
			       struct gk20a *g, void *__args);

/**
 * Test specification for: test_kmem_cache_fi_delayed_disable
 *
 * Description: This test validates delayed disable of fault injection for kmem
 *              cache APIs.
 *
 * Test Type: Feature Based
 *
 * Input: test_fault_injection_init() & test_kmem_init() must have been called
 *        prior to this test.
 *
 * Steps:
 * - Enable kmem fault injection immediately.
 * - Disable fault injection for after 1 call.
 * - Create a kmem cache object and verify the return is NULL, indicating fail.
 * - Create a kmem cache object and verify the return is non-NULL for the 2nd
 *   call, indicating pass and the fault injection was disabled.
 * - Cleanup the allocated cache.
 *
 * Output: Returns SUCCESS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_kmem_cache_fi_delayed_disable(struct unit_module *m,
			       struct gk20a *g, void *__args);

/**
 * Test specification for: test_kmem_kmalloc_fi_default
 *
 * Description: This test simply tests the default case of fault injection
 *              disabled for calling kmem kmalloc routines.
 *
 * Test Type: Feature Based
 *
 * Input: test_fault_injection_init() & test_kmem_init() must have been called
 *        prior to this test.
 *
 * Steps:
 * - Verify the kmem fault injection is disabled.
 * - Allocate memory with nvgpu_kmalloc() and verify the call passed.
 * - Free the kmem kmalloc memory.
 *
 * Output: Returns SUCCESS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_kmem_kmalloc_fi_default(struct unit_module *m,
			       struct gk20a *g, void *__args);

/**
 * Test specification for: test_kmem_kmalloc_fi_enabled
 *
 * Description: This test validates immediate fault injection for kmem kmalloc.
 *
 * Test Type: Feature Based
 *
 * Input: test_fault_injection_init() & test_kmem_init() must have been called
 *        prior to this test.
 *
 * Steps:
 * - Enable kmem fault injection immediately.
 * - Allocate memory with nvgpu_kmalloc() and verify the result is NULL,
 *   indicating fail.
 *
 * Output: Returns SUCCESS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_kmem_kmalloc_fi_enabled(struct unit_module *m,
			       struct gk20a *g, void *__args);

/**
 * Test specification for: test_kmem_kmalloc_fi_delayed_enable
 *
 * Description: This test validates delayed enable of fault injection for kmem
 *              kmalloc APIs.
 *
 * Test Type: Feature Based
 *
 * Input: test_fault_injection_init() & test_kmem_init() must have been called
 *        prior to this test.
 *
 * Steps:
 * - Enable kmem fault injection for after 2 calls.
 * - Call nvgpu_kmalloc() 3 times and verify it fails only on the 3rd call.
 * - Cleanup the allocated memory.
 *
 * Output: Returns SUCCESS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_kmem_kmalloc_fi_delayed_enable(struct unit_module *m,
			       struct gk20a *g, void *__args);

/**
 * Test specification for: test_kmem_kmalloc_fi_delayed_disable
 *
 * Description: This test validates delayed disable of fault injection for kmem
 *              kalloc APIs.
 *
 * Test Type: Feature Based
 *
 * Input: test_fault_injection_init() & test_kmem_init() must have been called
 *        prior to this test.
 *
 * Steps:
 * - Enable kmem fault injection immediately.
 * - Disable fault injection for after 2 calls.
 * - Call nvgpu_kmalloc() in a loop and verify it fails until the 3rd call.
 * - Cleanup the allocated cache.
 *
 * Output: Returns SUCCESS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_kmem_kmalloc_fi_delayed_disable(struct unit_module *m,
			       struct gk20a *g, void *__args);

#endif /* __UNIT_FAULT_INJECTION_KMEM_H__ */
