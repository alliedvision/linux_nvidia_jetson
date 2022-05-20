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
#ifndef UNIT_NVGPU_RUNLIST_GK20A_H
#define UNIT_NVGPU_RUNLIST_GK20A_H

#include <nvgpu/types.h>

struct unit_module;
struct gk20a;

/** @addtogroup SWUTS-fifo-runlist-gk20a
 *  @{
 *
 * Software Unit Test Specification for fifo/runlist/gk20a
 */

/**
 * Test specification for: test_gk20a_runlist_length_max
 *
 * Description: Branch coverage for gk20a_runlist_length_max
 *
 * Test Type: Feature
 *
 * Targets: gops_runlist.length_max, gk20a_runlist_length_max
 *
 * Input: test_fifo_init_support() run for this GPU
 *
 * Steps:
 * - Check that max length matches definition in H/W manuals.
 *
 * Output: Returns PASS if all branches gave expected results. FAIL otherwise.
 */
int test_gk20a_runlist_length_max(struct unit_module *m,
		struct gk20a *g, void *args);

/**
 * Test specification for: test_gk20a_runlist_hw_submit
 *
 * Description: Branch coverage for gk20a_runlist_hw_submit
 *
 * Test Type: Feature
 *
 * Targets: gops_runlist.hw_submit, gk20a_runlist_hw_submit
 *
 * Input: test_fifo_init_support() run for this GPU
 *
 * Steps:
 * - Check that fifo_runlist_base_r is not programmed when count is 0.
 * - Check that fifo_runlist_base_r is programmed with count > 0.
 * - Check that runlist_r register is programmed with runlist_id and count.
 *
 * Output: Returns PASS if all branches gave expected results. FAIL otherwise.
 */
int test_gk20a_runlist_hw_submit(struct unit_module *m,
		struct gk20a *g, void *args);

/**
 * Test specification for: test_gk20a_runlist_wait_pending
 *
 * Description: Branch coverage for gk20a_runlist_wait_pending
 *
 * Test Type: Feature
 *
 * Targets: gops_runlist.wait_pending, gk20a_runlist_wait_pending
 *
 * Input: test_fifo_init_support() run for this GPU
 *
 * Steps:
 * - Check case where runlist is not pending (not wait).
 *   - Set register to indicate that runlist is NOT pending.
 *   - Call gk20a_runlist_wait_pending.
 * - Check case where some polling is needed until runlist is not pending:
 *   - Install register IO callbacks in order to control
 *     value read from fifo_eng_runlist_r register.
 *   - Configure callback to clear pending bit after one nvgpu_readl.
 *   - Call gk20a_runlist_wait_pending.
 *   - Configure callback to clear pending bit after two nvgpu_readl.
 *   - Call gk20a_runlist_wait_pending.
 * - Check case where polling times out:
 *   - Set register to indicate that runlist is pending.
 *   - Call gk20a_runlist_wait_pending.
 *
 * Output: Returns PASS if all branches gave expected results. FAIL otherwise.
 */
int test_gk20a_runlist_wait_pending(struct unit_module *m,
		struct gk20a *g, void *args);

/**
 * Test specification for: test_gk20a_runlist_write_state
 *
 * Description: Branch coverage for gk20a_runlist_write_state
 *
 * Test Type: Feature
 *
 * Targets: gops_runlist.write_state, gk20a_runlist_write_state
 *
 * Input: test_fifo_init_support() run for this GPU
 *
 * Steps:
 * - Use nested loops to test combinations of:
 *  - Initial value fifo_sched_disable_r() is either 0 or U32_MAX.
 *  - runlists_mask varies from 0 to 3.
 *  - runlist_state is either RUNLIST_DISABLED or RUNLIST_ENABLED.
 * - Check that corresponding bits are set/cleared in fifo_sched_disabled_r.
 *
 * Output: Returns PASS if all branches gave expected results. FAIL otherwise.
 */
int test_gk20a_runlist_write_state(struct unit_module *m,
		struct gk20a *g, void *args);

/**
 * @}
 */

#endif /* UNIT_NVGPU_RUNLIST_GK20A_H */
