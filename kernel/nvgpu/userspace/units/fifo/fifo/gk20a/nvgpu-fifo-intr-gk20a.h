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
#ifndef UNIT_NVGPU_FIFO_INTR_GK20A_H
#define UNIT_NVGPU_FIFO_INTR_GK20A_H

#include <nvgpu/types.h>

struct unit_module;
struct gk20a;

/** @addtogroup SWUTS-fifo-fifo-gk20a
 *  @{
 *
 * Software Unit Test Specification for fifo/fifo/gk20a
 */

/**
 * Test specification for: test_gk20a_fifo_intr_1_enable
 *
 * Description: Enable/disable non-stalling interrupts
 *
 * Test Type: Feature
 *
 * Targets: gops_fifo.intr_1_enable, gk20a_fifo_intr_1_enable
 *
 * Input: test_fifo_init_support() run for this GPU
 *
 * Steps:
 * - Clear fifo_intr_en_1_r().
 * - Call gk20a_fifo_intr_1_enable with enable = true, then check that
 *   interrupts have been enabled in fifo_intr_en_1_r().
 * - Call gk20a_fifo_intr_1_enable with enable = false, then check that
 *   interrupts have been disabled in fifo_intr_en_1_r().
 *
 * Output: Returns PASS if all branches gave expected results. FAIL otherwise.
 */
int test_gk20a_fifo_intr_1_enable(struct unit_module *m,
		struct gk20a *g, void *args);

/**
 * Test specification for: test_gk20a_fifo_intr_1_isr
 *
 * Description: Non-stalling interrupt service routine
 *
 * Test Type: Feature
 *
 * Targets: gops_fifo.intr_1_isr, gk20a_fifo_intr_1_isr
 *
 * Input: test_fifo_init_support() run for this GPU
 *
 * Steps:
 * - Check that gk20a_fifo_intr_1_isr only clears channel interrupt when
 *   multiple interrupts are pending.
 * - Check that gk20a_fifo_intr_1_isr does not clear any interrupt when
 *   channel interrupt is not pending.
 *
 * Output: Returns PASS if all branches gave expected results. FAIL otherwise.
 */
int test_gk20a_fifo_intr_1_isr(struct unit_module *m,
		struct gk20a *g, void *args);

/**
 * Test specification for: test_gk20a_fifo_intr_handle_chsw_error
 *
 * Description: Non-stalling interrupt service routine
 *
 * Test Type: Feature
 *
 * Targets: gops_fifo.intr_handle_chsw_error,
 *          gk20a_fifo_intr_handle_chsw_error
 *
 * Input: test_fifo_init_support() run for this GPU
 *
 * Steps:
 * - Write fifo_intr_chsw_error_r to fake some pending interrupts.
 * - Call gk20a_fifo_intr_handle_chsw_error.
 * - Use stub for gr.falcon.dump to clear fifo_intr_chsw_error_r
 *   (before the handling function writes back to it, in order to
 *   clear interrupts).
 * - Check that gk20a_fifo_intr_handle_chsw_error clears interrupts
 *   by writing to fifo_intr_chsw_error_r.
 *
 * Output: Returns PASS if all branches gave expected results. FAIL otherwise.
 */
int test_gk20a_fifo_intr_handle_chsw_error(struct unit_module *m,
		struct gk20a *g, void *args);

/**
 * Test specification for: test_gk20a_fifo_intr_handle_runlist_event
 *
 * Description: Non-stalling interrupt service routine
 *
 * Test Type: Feature
 *
 * Targets: gops_fifo.intr_handle_runlist_event,
 *          gk20a_fifo_intr_handle_runlist_event
 *
 * Input: test_fifo_init_support() run for this GPU
 *
 * Steps:
 * - Install read/write register io callbacks.
 * - Call gk20a_fifo_intr_handle_runlist_event.
 * - In the read callback, return fake interrupt pending mask.
 * - In the write callback, check that the same interrupt mask
 *   is used to clear interrupts.
 *
 * Output: Returns PASS if all branches gave expected results. FAIL otherwise.
 */
int test_gk20a_fifo_intr_handle_runlist_event(struct unit_module *m,
		struct gk20a *g, void *args);

/**
 * Test specification for: test_gk20a_fifo_pbdma_isr
 *
 * Description: PBDMA interrupt service routine
 *
 * Test Type: Feature
 *
 * Targets: gk20a_fifo_pbdma_isr, gops_fifo.pbdma_isr
 *
 * Input: test_fifo_init_support() run for this GPU
 *
 * Steps:
 * - Get number of PBDMAs with nvgpu_get_litter_value, and check that
 *   it is non-zero.
 * - For each pbdma_id:
 *   - Set bit in fifo_intr_pbdma_id_r to indicate that one
 *     interrupt is pending for this PBDMA.
 *   - Call gk20a_fifo_pbdma_isr.
 *   - Check that ops.pbdma.handle_intr is called exactly once.
 *   - In the ops.pbdma.handle_intr stub, check that pbdma_id matches
 *     the interrupt mask.
 *
 * Output: Returns PASS if all branches gave expected results. FAIL otherwise.
 */
int test_gk20a_fifo_pbdma_isr(struct unit_module *m,
		struct gk20a *g, void *args);

/**
 * @}
 */

#endif /* UNIT_NVGPU_FIFO_INTR_GK20A_H */
