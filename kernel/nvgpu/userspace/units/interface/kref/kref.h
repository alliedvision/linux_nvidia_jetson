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

#ifndef UNIT_INTERFACE_KREF_H
#define UNIT_INTERFACE_KREF_H

struct gk20a;
struct unit_module;

/** @addtogroup SWUTS-interface-kref
 *  @{
 *
 * Software Unit Test Specification for interface-kref
 */

/**
 * Test specification for test_kref_init
 *
 * Description: Test the reference count initialization implementation.
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_ref_init
 *
 * Input: None
 *
 * Steps:
 * - Invoke the function nvgpu_ref_init to initialize nvgpu_ref structure.
 * - Read back the refcount value and confirm the value is initialized to 1.
 *   Otherwise, return FAIL.
 * - Return PASS.
 *
 * Output: Returns PASS if the refcount is initialized correctly, otherwise
 * returns FAIL.
 */
int test_kref_init(struct unit_module *m, struct gk20a *g, void *__args);

/**
 * Test specification for test_kref_get
 *
 * Description: Test the reference get implementation.
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_ref_get
 *
 * Input: None
 *
 * Steps:
 * - Invoke the function nvgpu_ref_init to initialize nvgpu_ref structure.
 * - Invoke the function nvgpu_ref_get in loop to increment the refcount value
 *   till LOOP_COUNT.
 * - Read back the refcount value and confirm that the value returned is in
 *   sync with the number of times nvgpu_ref_get is called. Otherwise return
 *   FAIL.
 * - Return PASS.
 *
 * Output: Returns PASS if the refcount is incremented correctly, otherwise
 * returns FAIL.
 */
int test_kref_get(struct unit_module *m, struct gk20a *g, void *__args);

/**
 * Test specification for test_kref_get_unless
 *
 * Description: Test the reference get unless implementation.
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_ref_get_unless_zero
 *
 * Input: None
 *
 * Steps:
 * - Initialize the refcount value as 0 for nvgpu_ref struct.
 * - Invoke function nvgpu_ref_get_unless_zero and confirm that the return
 *   value is 0. Otherwise return FAIL.
 * - Invoke the function nvgpu_ref_init to initialize nvgpu_ref structure.
 * - Invoke the function nvgpu_ref_get_unless_zero to increment the refcount
 *   value.
 * - Check and confirm that the return value is not zero. Otherwise, return
 *   FAIL.
 * - Return PASS.
 *
 * Output: Returns SUCCESS if the refcount is increased correctly according to
 * the current value in refcount, otherwise return FAIL.
 */
int test_kref_get_unless(struct unit_module *m, struct gk20a *g, void *__args);

/**
 * Test specification for test_kref_put
 *
 * Description: Test the reference put implementation.
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_ref_put
 *
 * Input: None
 *
 * Steps:
 * - Initialize the release_count as 0.
 * - Invoke the function nvgpu_ref_init to initialize nvgpu_ref structure.
 * - Invoke the function nvgpu_ref_get in loop to increment the refcount value
 *   till LOOP_COUNT.
 * - Read back the refcount value and confirm that the value returned is in
 *   sync with the number of times nvgpu_ref_get is called. Otherwise return
 *   FAIL.
 * - Invoke the function nvgpu_ref_put in loop for LOOP_COUNT times to
 *   decrement the refcount value to 0.
 * - Check the value of release_count value which is incremented in the
 *   release callback function to confirm that the release callback function
 *   is invoked and invoked only once. Otherwise return FAIL.
 * - Invoke the function nvgpu_ref_get to increment the refcount value.
 * - Invoke the function  nvgpu_ref_put with callback as NULL.
 * - Read back the refcount value and confirm that the value is reset to 0.
 *   Otherwise return FAIL.
 * - Return PASS.
 *
 * Output: Returns SUCCESS if the refcerence is released correctly, otherwise
 * return FAIL.
 */
int test_kref_put(struct unit_module *m, struct gk20a *g, void *__args);

/**
 * Test specification for test_kref_put_return
 *
 * Description: Test the reference put return implementation.
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_ref_put_return
 *
 * Input: None
 *
 * Steps:
 * - Initialize the release_count as 0.
 * - Invoke the function nvgpu_ref_init to initialize nvgpu_ref structure.
 * - Invoke the function nvgpu_ref_get in loop to increment the refcount value.
 * - Read back the refcount value and confirm that the value returned is in
 *   sync with the number of times nvgpu_ref_get is called. Otherwise return
 *   FAIL.
 * - Invoke the function nvgpu_ref_put in loop for (LOOP_COUNT - 1) to
 *   decrement the refcount value and confirm that the return value is always
 *   zero. Otherwise return FAIL.
 * - Invoke the function nvgpu_ref_put once more and confirm that the return
 *   value is equal to one. Otherwise return FAIL.
 * - Check the value of release_count value which is incremented in the
 *   release callback function to confirm that the release callback function
 *   is invoked and invoked only once. Otherwise return FAIL.
 * - Invoke the function nvgpu_ref_get to increment the refcount value.
 * - Invoke the function nvgpu_ref_put_return with callback as NULL.
 * - Check the return value and return FAIL if it is equal to zero.
 * - Return PASS.
 *
 * Output: Returns SUCCESS if the refcount is initialized correctly, otherwise
 * return FAIL.
 */
int test_kref_put_return(struct unit_module *m, struct gk20a *g, void *__args);

#endif /* UNIT_INTERFACE_KREF_H */
