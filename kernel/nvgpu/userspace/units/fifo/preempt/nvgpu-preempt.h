/*
 * Copyright (c) 2019, NVIDIA CORPORATION.  All rights reserved.
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
#ifndef UNIT_NVGPU_PREEMPT_H
#define UNIT_NVGPU_PREEMPT_H

#include <nvgpu/types.h>

struct unit_module;
struct gk20a;

/** @addtogroup SWUTS-fifo-preempt
 *  @{
 *
 * Software Unit Test Specification for fifo/preempt */

/**
 * Test specification for: test_preempt
 *
 * Description: Test TSG preempt.
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_preempt_channel
 *
 * Input: test_fifo_init_support
 *
 * Steps:
 * - Test channel preempt with below cases:
 *   - Channel bound to TSG: TSG is preempted.
 *   - Independent channel, not bound to TSG.
 *
 * Output: Returns PASS if all branches gave expected results. FAIL otherwise.
 */
int test_preempt(struct unit_module *m, struct gk20a *g, void *args);

/**
 * Test specification for: test_preempt_poll_tsg_on_pbdma
 *
 * Description: Poll and preempt all TSGs serving PBDMA.
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_preempt_poll_tsg_on_pbdma
 *
 * Input: test_fifo_init_support
 *
 * Steps:
 * - Go through list of TSGs serving PBDMAs and preempt the TSGs.
 *
 * Output: Returns PASS if all branches gave expected results. FAIL otherwise.
 */
int test_preempt_poll_tsg_on_pbdma(struct unit_module *m, struct gk20a *g,
								void *args);

/**
 * Test specification for: test_preempt_get_timeout
 *
 * Description: Check GPU timeout value
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_preempt_get_timeout
 *
 * Input: test_fifo_init_support
 *
 * Steps:
 * - GPU timeout value is not set in init. So, check if timeout value is 0.
 *
 * Output: Returns PASS if all branches gave expected results. FAIL otherwise.
 */
int test_preempt_get_timeout(struct unit_module *m, struct gk20a *g,
								void *args);

#endif /* UNIT_NVGPU_PREEMPT_H */
