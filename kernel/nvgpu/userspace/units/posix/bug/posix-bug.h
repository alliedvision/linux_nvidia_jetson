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

/**
 * @addtogroup SWUTS-posix-bug
 * @{
 *
 * Software Unit Test Specification for posix-bug
 */

#ifndef __UNIT_POSIX_BUG_H__
#define __UNIT_POSIX_BUG_H__

/**
 * Test specification for test_expect_bug
 *
 * Description: Test the BUG implementation.
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_posix_bug, dump_stack,
 *          BUG, BUG_ON, nvgpu_assert
 *
 * Inputs: None
 *
 * Steps:
 * 1) Use the unit test framework specific EXPECT_BUG define to call BUG.
 * 2) BUG should be called as expected, but the portion of BUG implementation
 *    which cannot be run in unit test framework will not be executed.
 * 3) EXPECT_BUG is also tested to make sure that BUG is not called where it is
 *    not expected.
 * 4) Steps 1 and 2 are also executed to test the macro BUG_ON.
 *
 * Output:
 * The test returns PASS if BUG is called as expected based on the parameters
 * passed and EXPECT_BUG handles it accordingly.
 * The test returns FAIL if either BUG is not called as expected or if
 * EXPECT_BUG indicates that a BUG call was made which was not requested by
 * the test.
 *
 */
int test_expect_bug(struct unit_module *m, struct gk20a *g, void *args);

/**
 * Test specification for test_bug_cb
 *
 * Description: Test the bug callback functionality.
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_bug_register_cb, nvgpu_bug_unregister_cb,
 *          nvgpu_bug_cb_from_node
 *
 * Inputs: None
 *
 * Steps:
 * 1) Register two callbacks for BUG.
 * 2) Invoke BUG and check if both the callback functions are invoked as
 *    expected.
 * 3) Register two callbacks for BUG again.
 * 4) Remove one of the registered callbacks.
 * 5) Invoke BUG and confirm if the unregistered callback is not invoked.
 *
 * Output:
 * The test returns PASS if both the callbacks are invoked in the first
 * invocation of the BUG and in the second invocation, only the registered
 * callback is invoked and not the unregistered callback. Otherwise, the test
 * returns FAIL.
 *
 */
int test_bug_cb(struct unit_module *m, struct gk20a *g, void *args);

/**
 * Test specification for test_warn_msg
 *
 * Description: Test the warn message functionality.
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_posix_warn
 *
 * Inputs: None
 *
 * Steps:
 * 1) Call nvgpu_posix_warn function with cond as 0.
 * 2) Check the return value from function nvgpu_posix_warn. If it is not
 *    equal to 0, return FAIL.
 * 3) Call nvgpu_posix_warn function with cond as 1.
 * 4) Check the return value from function nvgpu_posix_warn. If it is not
 *    equal to 1, return FAIL.
 * 5) Return PASS.
 *
 * Output:
 * The test returns PASS if both the calls of nvgpu_posix_warn function returns
 * the expected return value. Otherwise, the test returns FAIL.
 *
 */
int test_warn_msg(struct unit_module *m, struct gk20a *g, void *args);

#endif /* __UNIT_POSIX_BUG_H__ */
