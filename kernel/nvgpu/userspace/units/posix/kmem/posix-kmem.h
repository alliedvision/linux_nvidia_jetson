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

/**
 * @addtogroup SWUTS-posix-kmem
 * @{
 *
 * Software Unit Test Specification for posix-kmem
 */

#ifndef __UNIT_POSIX_KMEM_H__
#define __UNIT_POSIX_KMEM_H__

/**
 * Test specification for test_kmem_cache_create
 *
 * Description: Test the creation of kmem cache.
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_kmem_cache_create, nvgpu_kmem_cache_destroy
 *
 * Inputs:
 * 1) GPU driver struct g.
 * 2) Global define for cache size to alloc.
 *
 * Steps:
 * 1) Call nvgpu_kmem_cache_create with cache size as a parameter.
 * 2) Check the return value from cache create function. If the return value
 *    is NULL, return test FAIL.
 * 3) Check if the size value of the created cache is equal to the requested
 *    size, if not destroy the cache and return FAIL.
 * 4) Check if the g struct of the created cache is equal to the g struct
 *    instance passed to the cache creation API, if not destroy the cache and
 *    return FAIL.
 * 5) Destroy the created cache and return PASS.
 *
 * Output:
 * The test returns PASS if the cache is created successfully and the size and
 * g struct of the cache matches with the passed arguments. Otherwise the test
 * returns FAIL.
 *
 */
int test_kmem_cache_create(struct unit_module *m,
                                struct gk20a *g, void *args);

/**
 * Test specification for test_kmem_cache_alloc
 *
 * Description: Test the allocation of memory from kmem cache.
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_kmem_cache_create, nvgpu_kmem_cache_alloc,
 *          nvgpu_kmem_cache_free, nvgpu_kmem_cache_destroy
 *
 * Inputs:
 * 1) GPU driver struct g.
 * 2) Global define for cache size to alloc.
 *
 * Steps:
 * 1) Call nvgpu_kmem_cache_create with cache size as a parameter.
 * 2) Check the return value from cache create function. If the return value
 *    is NULL, return test FAIL.
 * 3) Check if the size value of the created cache is equal to the requested
 *    size, if not destroy the cache and return FAIL.
 * 4) Check if the g struct of the created cache is equal to the g struct
 *    instance passed to the cache creation API, if not destroy the cache and
 *    return FAIL.
 * 5) Call nvgpu_kmem_cache_alloc to alloc memory from the created cache.
 * 6) Check the return value from cache alloc function. If the return value
 *    is NULL, destroy the cache and return test FAIL.
 * 7) Free the allocated memory and destroy the cache, and return PASS.
 *
 * Output:
 * The test returns PASS if the cache creation and allocation of memory from
 * the cache is successful.  Otherwise, return FAIL.
 *
 */
int test_kmem_cache_alloc(struct unit_module *m,
                                struct gk20a *g, void *args);

/**
 * Test specification for test_kmem_kmalloc
 *
 * Description: Test the allocation of memory using kmalloc.
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_kmalloc_impl, nvgpu_kfree_impl
 *
 * Inputs:
 * 1) GPU driver struct g.
 * 2) Global define for size to alloc.
 *
 * Steps:
 * 1) Call nvgpu_kmalloc_impl with size as a parameter.
 * 2) Check the return value from nvgpu_kmalloc_impl function. If the return
 *    value is NULL, return test FAIL.
 * 3) Free the allocated memory.
 * 4) Return PASS.
 *
 * Output:
 * The test returns PASS if the memory is successfully allocated. Otherwise,
 * the test returns FAIL.
 */
int test_kmem_kmalloc(struct unit_module *m,
                                struct gk20a *g, void *args);

/**
 * Test specification for test_kmem_kzalloc
 *
 * Description: Test the allocation of memory using kzalloc.
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_kzalloc_impl, nvgpu_kfree_impl
 *
 * Inputs:
 * 1) GPU driver struct g.
 * 2) Global define for size to alloc.
 *
 * Steps:
 * 1) Call nvgpu_kzalloc_impl with size as a parameter.
 * 2) Check the return value from nvgpu_kmalloc_impl function. If the return
 *    value is NULL, return test FAIL.
 * 3) Check if the allocated chunk of memory is zero initialized. If it is not,
 *    free the memory and return FAIL.
 * 3) Free the allocated memory.
 * 4) Return PASS.
 *
 * Output:
 * The test returns PASS if the zero initialized memory is successfully
 * allocated. Otherwise, the test returns FAIL.
 */
int test_kmem_kzalloc(struct unit_module *m,
                                struct gk20a *g, void *args);

/**
 * Test specification for test_kmem_kcalloc
 *
 * Description: Test the allocation of memory using kcalloc.
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_kcalloc_impl, nvgpu_kfree_impl
 *
 * Inputs:
 * 1) GPU driver struct g.
 * 2) Global define for size to alloc.
 * 3) Global define for count to alloc.
 *
 * Steps:
 * 1) Call nvgpu_kcalloc_impl with size and count as parameters.
 * 2) Check the return value from nvgpu_kmalloc_impl function. If the return
 *    value is NULL, return test FAIL.
 * 3) Check if the allocated chunk of memory is zero initialized. If it is not,
 *    free the memory and return FAIL.
 * 3) Free the allocated memory.
 * 4) Return PASS.
 *
 * Output:
 * The test returns PASS if the zero initialized memory is successfully
 * allocated. Otherwise, the test returns FAIL.
 */
int test_kmem_kcalloc(struct unit_module *m,
                                struct gk20a *g, void *args);

/**
 * Test specification for test_kmem_virtual_alloc
 *
 * Description: Test the allocation of memory using virtual alloc APIs.
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_vmalloc_impl, nvgpu_vfree_impl,
 *          nvgpu_vzalloc_impl
 *
 * Inputs:
 * 1) GPU driver struct g.
 * 2) Global define for cache size to alloc.
 *
 * Steps:
 * 1) Call nvgpu_vmalloc_impl with size as a parameter.
 * 2) Check the return value from nvgpu_vmalloc_impl function. If the return
 *    value is NULL, return test FAIL.
 * 3) Free the memory using nvgpu_vfree_impl.
 * 4) Call nvgpu_vzalloc_impl with size as a parameter.
 * 5) Check the return value from nvgpu_vzalloc_impl function. If the return
 *    value is NULL, return test FAIL.
 * 6) Check if the allocated chunk of memory is zero initialized. If it is not,
 *    free the memory and return FAIL.
 * 7) Free the memory using nvgpu_vfree_impl.
 * 8) Return PASS.
 *
 * Output:
 * The test returns PASS if,
 * - The virtual allocation API nvgpu_vmalloc_impl successfully allocates memory.
 * - The virtual allocation API nvgpu_vzalloc_impl successfully allocates zero
 *   initialised memory.
 * If any of the above points fail, the test returns FAIL.
 */
int test_kmem_virtual_alloc(struct unit_module *m,
                                struct gk20a *g, void *args);

/**
 * Test specification for test_kmem_big_alloc
 *
 * Description: Test the allocation of memory using big alloc APIs.
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_big_alloc_impl, nvgpu_big_free
 *
 * Inputs:
 * 1) GPU driver struct g.
 * 2) Global define for cache size to alloc.
 *
 * Steps:
 * 1) Call nvgpu_big_alloc_impl with size as a parameter and clear flag set to
 *    false.
 * 2) Check the return value from nvgpu_big_alloc_impl function. If the return
 *    value is NULL, return test FAIL.
 * 3) Free the memory using nvgpu_big_free.
 * 4) Call nvgpu_big_alloc_impl with size as a parameter and clear flag set to
 *    true.
 * 5) Check the return value from nvgpu_big_alloc_impl function. If the return
 *    value is NULL, return test FAIL.
 * 6) Check if the allocated chunk of memory is zero initialized. If it is not,
 *    free the memory and return FAIL.
 * 7) Free the memory using nvgpu_big_free..
 * 8) Return PASS.
 *
 * Output:
 *
 */
int test_kmem_big_alloc(struct unit_module *m,
                                struct gk20a *g, void *args);
#endif /* __UNIT_POSIX_KMEM_H__ */
