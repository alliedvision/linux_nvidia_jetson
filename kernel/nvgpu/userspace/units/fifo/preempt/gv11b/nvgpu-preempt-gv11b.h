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
#ifndef UNIT_NVGPU_PREEMPT_GV11B_H
#define UNIT_NVGPU_PREEMPT_GV11B_H

#include <nvgpu/types.h>

struct unit_module;
struct gk20a;

/** @addtogroup SWUTS-fifo-preempt-gv11b
 *  @{
 *
 * Software Unit Test Specification for fifo/preempt/gv11b
 */

/**
 * Test specification for: test_gv11b_fifo_preempt_trigger
 *
 * Description: Test fifo preempt trigger
 *
 * Test Type: Feature
 *
 * Targets: gv11b_fifo_preempt_trigger
 *
 * Input: test_fifo_init_support
 *
 * Steps:
 * - Preempt trigger writes given id to the preempt register if id type is TSG.
 * - Read preempt register to check if preempt register value is equal to given
 *   id for TSG type id or original value otherwise.
 *
 * Output: Returns PASS if all branches gave expected results. FAIL otherwise.
 */
int test_gv11b_fifo_preempt_trigger(struct unit_module *m, struct gk20a *g,
								void *args);

/**
 * Test specification for: test_gv11b_fifo_preempt_runlists_for_rc
 *
 * Description: Test runlist preempt
 *
 * Test Type: Feature
 *
 * Targets: gv11b_fifo_preempt_runlists_for_rc,
 *          gops_fifo.preempt_runlists_for_rc,
 *          gv11b_fifo_issue_runlist_preempt
 *
 * Input: test_fifo_init_support
 *
 * Steps:
 * - Bits corresponding to active runlists are set to issue runlist preempt.
 * - Check that value stored in memory is as expected.
 *
 * Output: Returns PASS if all branches gave expected results. FAIL otherwise.
 */
int test_gv11b_fifo_preempt_runlists_for_rc(struct unit_module *m,
				struct gk20a *g, void *args);

/**
 * Test specification for: test_gv11b_fifo_preempt_poll_pbdma
 *
 * Description: Test preempt pbdma with tsg/ch poll
 *
 * Test Type: Feature, Error injection
 *
 * Targets: gv11b_fifo_preempt_poll_pbdma, fifo_preempt_check_tsg_on_pbdma
 *
 * Input: test_fifo_init_support
 *
 * Steps:
 * - Introduce different cases of ch/tsg status on PBDMA.
 * - Check that pbdma preempt returns success for valid cases.
 *
 * Output: Returns PASS if all branches gave expected results. FAIL otherwise.
 */
int test_gv11b_fifo_preempt_poll_pbdma(struct unit_module *m, struct gk20a *g,
								void *args);

/**
 * Test specification for: test_gv11b_fifo_preempt_channel
 *
 * Description: Test channel preempt
 *
 * Test Type: Feature
 *
 * Targets: gv11b_fifo_preempt_channel
 *
 * Input: test_fifo_init_support
 *
 * Steps:
 * - Check that preemption of channel with valid tsgid triggers tsg preempt.
 *
 * Output: Returns PASS if all branches gave expected results. FAIL otherwise.
 */
int test_gv11b_fifo_preempt_channel(struct unit_module *m, struct gk20a *g,
								void *args);

/**
 * Test specification for: test_gv11b_fifo_preempt_tsg
 *
 * Description: Test TSG preempt
 *
 * Test Type: Feature
 *
 * Targets: gv11b_fifo_preempt_tsg, gops_fifo.preempt_tsg,
 *          gv11b_fifo_preempt_locked
 *
 * Input: test_fifo_init_support
 *
 * Steps:
 * - Write h/w register to trigger TSG preempt.
 * - Check if written value is as expected.
 *
 * Output: Returns PASS if all branches gave expected results. FAIL otherwise.
 */
int test_gv11b_fifo_preempt_tsg(struct unit_module *m, struct gk20a *g,
								void *args);

/**
 * Test specification for: test_gv11b_fifo_is_preempt_pending
 *
 * Description: Test if preempt is pending
 *
 * Test Type: Feature, Error injection
 *
 * Targets: gv11b_fifo_is_preempt_pending, gv11b_fifo_preempt_poll_eng,
 *          fifo_check_eng_intr_pending
 *
 * Input: test_fifo_init_support
 *
 * Steps:
 * - Check pbdma and engine preempt status; determine if preempt is completed.
 * - Vary engine preempt status for various ctx status scenarios.
 * - Check that the return value corresponds to input cases.
 *
 * Output: Returns PASS if all branches gave expected results. FAIL otherwise.
 */
int test_gv11b_fifo_is_preempt_pending(struct unit_module *m, struct gk20a *g,
								void *args);
/**
 * @}
 */

#endif /* UNIT_NVGPU_PREEMPT_GV11B_H */
