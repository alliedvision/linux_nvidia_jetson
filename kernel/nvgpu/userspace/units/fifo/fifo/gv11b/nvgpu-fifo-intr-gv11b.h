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
#ifndef UNIT_NVGPU_FIFO_INTR_GV11B_H
#define UNIT_NVGPU_FIFO_INTR_GV11B_H

#include <nvgpu/types.h>

struct unit_module;
struct gk20a;

/** @addtogroup SWUTS-fifo-fifo-gv11b
 *  @{
 *
 * Software Unit Test Specification for fifo/fifo/gv11b
 */

/**
 * Test specification for: test_gv11b_fifo_intr_0_enable
 *
 * Description: Enable stalling interrupts
 *
 * Test Type: Feature
 *
 * Targets: gops_fifo.intr_0_enable, gv11b_fifo_intr_0_enable
 *
 * Input: test_fifo_init_support() run for this GPU
 *
 * Steps:
 * - Check enabling of interrupts:
 *   - Check that fifo ctxsw_timeout is enabled.
 *   - Check that pbdma interrupts are enabled.
 *   - Check that runlist interrupts are cleared (~0 written to
 *     fifo_intr_runlist_r).
 *   - Check that fifo interrupts are cleared (~0 written to fifo_intr_0_r).
 *   - Check that fifo interrupt enable mask is non-zero.
 * - Check disabling of interrupts:
 *   - Check that fifo ctxsw_timeout is disabled.
 *   - Check that pbdma interrupts are disabled.
 *   - Check that fifo interrupt enable mask is zero.
 *
 * Output: Returns PASS if all branches gave expected results. FAIL otherwise.
 */
int test_gv11b_fifo_intr_0_enable(struct unit_module *m,
		struct gk20a *g, void *args);

/**
 * Test specification for: test_gv11b_fifo_handle_sched_error
 *
 * Description: Handle scheduling error
 *
 * Test Type: Feature
 *
 * Targets: gops_fifo.handle_sched_error, gv11b_fifo_handle_sched_error
 *
 * Input: test_fifo_init_support() run for this GPU
 *
 * Steps:
 * - Set fifo_intr_sched_error_r with sched error code.
 * - Call gv11b_fifo_handle_sched_error.
 * - Check for valid sched error codes SCHED_ERROR_CODE_RL_REQ_TIMEOUT and
 *   SCHED_ERROR_CODE_BAD_TSG.
 * - Check for invalid sched error code (outside expected range).
 *
 * Output: Returns PASS if all branches gave expected results. FAIL otherwise.
 */
int test_gv11b_fifo_handle_sched_error(struct unit_module *m,
		struct gk20a *g, void *args);

/**
 * Test specification for: test_gv11b_fifo_intr_0_isr
 *
 * Description: Stalling interrupt handler
 *
 * Test Type: Feature
 *
 * Targets: gops_fifo.intr_0_isr, gv11b_fifo_intr_0_isr
 *
 * Input: test_fifo_init_support() run for this GPU
 *
 * Steps:
 * - Install register io callbacks to emulate clearing of interrupts
 *   (write to fifo_intr_0 clears interrupts).
 * - Set fifo_intr_0 with all combinations of handled interrupts, as
 *   well as one unhandled interrupt.
 * - Check that gv11b_fifo_intr_0_isr clears interrupts for all handled
 *   interrupts.
 * - Check that, when g->fifo.sw_ready is false, gv11b_fifo_intr_0_isr
 *   clears any pending interrupt.
 *
 * Output: Returns PASS if all branches gave expected results. FAIL otherwise.
 */
int test_gv11b_fifo_intr_0_isr(struct unit_module *m,
		struct gk20a *g, void *args);

/**
 * Test specification for: test_gv11b_fifo_intr_recover_mask
 *
 * Description: Set/unset recovery mask
 *
 * Test Type: Feature
 *
 * Targets: gops_fifo.intr_set_recover_mask,
 *          gv11b_fifo_intr_set_recover_mask,
 *          gops_fifo.intr_unset_recover_mask,
 *          gv11b_fifo_intr_unset_recover_mask
 *
 * Input: test_fifo_init_support() run for this GPU
 *
 * Steps:
 * - Install register io callbacks to emulate clearing of the mask of
 *   engines that timed out (write to fifo_intr_ctxsw_timeout_r clears mask).
 * - Enable interrupts with gv11b_fifo_intr_0_enable, and make sure that
 *   ctxsw_timeout interrupt is enabled.
 * - Call gv11b_fifo_intr_set_recover_mask, and check that:
 *   - ctxsw_timeout interrupt is disabled in fifo_intr_en_0_r.
 *   - fifo_intr_ctxsw_timeout_r has been cleared.
 * - Call gv11b_fifo_intr_unset_recover_mask, and check that:
 *   - ctxsw_timeout interrupt is enabled in fifo_intr_en_0_r.
 *
 * Output: Returns PASS if all branches gave expected results. FAIL otherwise.
 */
int test_gv11b_fifo_intr_recover_mask(struct unit_module *m,
		struct gk20a *g, void *args);

/**
 * @}
 */

#endif /* UNIT_NVGPU_FIFO_INTR_GV11B_H */
