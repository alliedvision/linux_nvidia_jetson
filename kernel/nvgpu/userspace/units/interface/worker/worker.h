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

#ifndef UNIT_WORKER_H
#define UNIT_WORKER_H

struct gk20a;
struct unit_module;

/** @addtogroup SWUTS-interface-worker
 *  @{
 *
 * Software Unit Test Specification for worker unit
 */

/**
 * Test specification for: test_init
 *
 * Description: Verify functionality of worker init APIs.
 *
 * Test Type: Feature, Error guessing, Boundary values
 *
 * Targets: nvgpu_worker_init_name, nvgpu_worker_init
 *
 * Input: None
 *
 * Steps:
 * - Case 1:
 *   - Call nvgpu_worker_init_name() with a long name to verify the API can
 *     handle strings longer than the worker struct supports.
 * - Case 2:
 *   - Call nvgpu_worker_init_name() with a short name to get full line/branch
 *     coverage.
 * - Case 3:
 *   - Enable fault injection for creating threads.
 *   - Call nvgpu_worker_init() and verify it returns an error.
 *   - Disable fault injection for creating threads.
 * - Case 4:
 *   - Call nvgpu_worker_init() and verify it returns success.
 * - Case 5:
 *   - Call nvgpu_worker_init() and verify it returns success to verify the API
 *     can handle being called after the worker is already initialized.
 *
 * Output: Returns PASS if expected result is met, FAIL otherwise.
 */
int test_init(struct unit_module *m, struct gk20a *g, void *args);

/**
 * Test specification for: test_enqueue
 *
 * Description: Verify functionality of worker enqueue API.
 *
 * Test Type: Feature, Error guessing
 *
 * Targets: nvgpu_worker_enqueue
 *
 * Input: test_init shall have run.
 *
 * Steps:
 * - Initialize work items.
 * - Case 1:
 *   - Enqueue work items, verify success.
 *   - Wait until all work items have been processed.
 * - Case 2:
 *   - Enqueue a work item.
 *   - Before the item is processed, enqueue it again and verify error is
 *     returned.
 *
 * Output: Returns PASS if expected result is met, FAIL otherwise.
 */
int test_enqueue(struct unit_module *m, struct gk20a *g, void *args);

/**
 * Test specification for: test_branches
 *
 * Description: Test a variety of special cases and error checking in the
 *              worker enqueue API and worker thread.
 *
 * Test Type: Feature, Error injection, Error guessing
 *
 * Targets: nvgpu_worker_enqueue, nvgpu_worker_should_stop
 *
 * Input: test_init shall have run.
 *
 * Steps:
 * - Case 1: Coverage for wait timeout.
 *   - Make timeout value for thread very short.
 *   - Enqueue a work item to trigger thread to break out of waiting state.
 *   - Wait until thread has executed processing at least 10 times.
 *   - Reset timeout value to maximum.
 * - Case 2: Coverage for worker_ops being NULL.
 *   - Set worker_op function pointers to NULL to verify these conditions
 *     are correctly handled by the worker thread.
 *   - Enqueue 3 work items to ensure all the conditions are checked in the
 *     thread loop.
 *   - Restore original worker_ops.
 * - Case 3: Coverage for wakeup_condition op returning true;
 *   - Setup wakeup_condition worker op to return true.
 *   - Enqueue a work item.
 *   - Wait until the the item has been processed.
 * - Case 4: Coverage for unexpected empty work item list.
 *   - Increment the worker put value to appear there is work pending.
 *   - Wake the thread by signalling the condition.
 *   - Wait for the thread to iterate the loop.
 * - Case 5: Coverage for the wakeup_early_exit op returning true.
 *   - Setup the wakeup_early_exit op to return true.
 *   - Enqueue a work item.
 *   - Wait for the thread to detect the early exit condition.
 *   - NOTE: This causes the worker thread to exit.
 * - Case 6: Coverage for failure to start thread.
 *   - Enable fault injection for creating threads.
 *   - Enqueue a work item (which will try to restart the thread).
 *   - Verify error is returned.
 *   - Disable fault injection for creating threads.
 * - Case 7: Coverage for starting a thread and state changes.
 *   - Enable fault injection for checking if thread is running to return true
 *     on second call.
 *   - Enqueue a work item (which will try to restart the thread).
 *   - Verify no error is returned.
 *   - Disable fault injection for checking if thread is running.
 * - Re-init the worker to restart the thread properly for next test.
 * - Case 8: Test thread stopping when thread_should_stop is set.
 *   - Enqueue a work item.
 *   - In, the wakeup_post_process callback, set the thread fault injection.
 *   - Wait until thread exits.
 *   - Disable thread fault injection.
 * - Re-init the worker to restart the thread properly for next test.
 *
 * Output: Returns PASS if expected result is met, FAIL otherwise.
 */
int test_branches(struct unit_module *m, struct gk20a *g, void *args);

/**
 * Test specification for: test_deinit
 *
 * Description: Test functionality of the deinit API.
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_worker_deinit
 *
 * Input: test_init shall have run.
 *
 * Steps:
 * - Call the nvgpu_worker_deinit() API.
 * - Wait 10us to ensure it has time to stop the running thread.
 *
 * Output: Returns PASS if expected result is met, FAIL otherwise.
 */
int test_deinit(struct unit_module *m, struct gk20a *g, void *args);

/**
 * @}
 */

#endif /* UNIT_WORKER_H */
