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
#ifndef UNIT_NVGPU_PBDMA_GV11B_H
#define UNIT_NVGPU_PBDMA_GV11B_H

#include <nvgpu/types.h>

struct unit_module;
struct gk20a;

/** @addtogroup SWUTS-fifo-pbdma-gv11b
 *  @{
 *
 * Software Unit Test Specification for fifo/pbdma/gv11b
 */

/**
 * Test specification for: test_gv11b_pbdma_setup_hw
 *
 * Description: PBDMA H/W initialization.
 *
 * Test Type: Feature
 *
 * Targets: gops_pbdma.setup_hw, gv11b_pbdma_setup_hw
 *
 * Input: test_fifo_init_support() run for this GPU
 *
 * Steps:
 * - Get number of PBDMA.
 * - Call gv11b_pbdma_setup_hw.
 * - For each HW PBDMA id, check that PBDMA timeout is set to max.
 *
 * Output: Returns PASS if all branches gave expected results. FAIL otherwise.
 */
int test_gv11b_pbdma_setup_hw(struct unit_module *m,
		struct gk20a *g, void *args);

/**
 * Test specification for: test_gv11b_pbdma_intr_enable
 *
 * Description: PBDMA interrupt enabling/disabling.
 *
 * Test Type: Feature
 *
 * Targets: gops_pbdma.intr_enable, gv11b_pbdma_intr_enable,
 *          gm20b_pbdma_disable_and_clear_all_intr,
 *          gm20b_pbdma_clear_all_intr
 *
 * Input: test_fifo_init_support() run for this GPU
 *
 * Steps:
 * - Get number of PBDMAs
 * - Check interrupt enable case:
 *   - Call gv11b_pbdma_intr_enable with enable = true.
 *   - Check that interrupts were cleared for all HW PDBMA (i.e. non-zero value
 *     written to pbdma_intr_0 and pbdma_intr_1).
 *   - Check that all intr_0 interrupts are enabled (i.e. pbdma_intr_en_0
 *     written with content of pbdma_intr_stall_r).
 *   - Check that all intr_1 interrupts are enabled (i.e. pbdma_intr_en_1
 *     written with content of pbdma_intr_stall_1, with
       pbdma_intr_stall_1_hce_illegal_op_enabled_f cleared).
 * - Check interrupt disable case:
 *   - Call gv11b_pbdma_intr_enable with enable = false.
 *   - Check that interrupts were disabled for all HW PDBMA (i.e. zero written
 *     to pbdma_intr_0 and pbdma_intr_1).
 *   - Check that interrupts were cleared for all HW PDBMA (i.e. non-zero value
 *     written to pbdma_intr_0 and pbdma_intr_1).
 *
 *
 * Output: Returns PASS if all branches gave expected results. FAIL otherwise.
 */
int test_gv11b_pbdma_intr_enable(struct unit_module *m,
		struct gk20a *g, void *args);

/**
 * Test specification for: test_gv11b_pbdma_handle_intr_0
 *
 * Description: Interrupt handling for pbdma_intr_0
 *
 * Test Type: Feature
 *
 * Targets: gops_pbdma.handle_intr_0, gv11b_pbdma_handle_intr_0
 *
 * Input: test_fifo_init_support() run for this GPU
 *
 * Steps:
 * - Set pbdma_intr_0 with a combination of the following interrupts:
 *   - clear_faulted_error: Check that recover is true and that method0 has
 *     been reset.
 *   - eng_reset: Check that recover is true.
 * - Other interrupts are tested explicitly for gm20b_pbdma_handle_intr_0.
 * - Call gv11b_pbdma_handle_intr_0 with additional error codes to exercise
 *   all branches in report_pbdma_error.
 *
 * Output: Returns PASS if all branches gave expected results. FAIL otherwise.
 */
int test_gv11b_pbdma_handle_intr_0(struct unit_module *m,
		struct gk20a *g, void *args);

/**
 * Test specification for: test_gv11b_pbdma_handle_intr_1
 *
 * Description: Interrupt handling for pbdma_intr_1
 *
 * Test Type: Feature
 *
 * Targets: gops_pbdma.handle_intr_1, gv11b_pbdma_handle_intr_1
 *
 * Input: test_fifo_init_support() run for this GPU
 *
 * Steps:
 * - Set pbdma_intr_1 variable (passed to the interrupt handling function) and
 *   pbdma_intr_1_r() register (using nvgpu_writel).
 * - Call gv11b_pbdma_handle_intr_1 with pbdma_intr_1 variable.
 * - Check that recover is true only when both pbdma_intr_1 variable and
 *   register are true.
 * - Check that recover is false otherwise.
 *
 * Output: Returns PASS if all branches gave expected results. FAIL otherwise.
 */
int test_gv11b_pbdma_handle_intr_1(struct unit_module *m,
		struct gk20a *g, void *args);

/**
 * Test specification for: test_gv11b_pbdma_intr_descs
 *
 * Description: Fatal channel interrupt mask
 *
 * Test Type: Feature
 *
 * Targets: gops_pbdma.channel_fatal_0_intr_descs,
 *          gv11b_pbdma_channel_fatal_0_intr_descs
 *
 * Input: test_fifo_init_support() run for this GPU
 *
 * Steps:
 * - Get mask of fatal channel interrupts with gv11b_pbdma_channel_fatal_0_intr_descs.
 * - Check that g->fifo is configured to process those interrupts.
 *
 * Output: Returns PASS if all branches gave expected results. FAIL otherwise.
 */
int test_gv11b_pbdma_intr_descs(struct unit_module *m,
		struct gk20a *g, void *args);

/**
 * Test specification for: test_gv11b_pbdma_get_fc
 *
 * Description: Get settings to program RAMFC.
 *
 * Test Type: Feature
 *
 * Targets: gops_pbdma.get_fc_pb_header, gv11b_pbdma_get_fc_pb_header,
 *          gops_pbdma.get_fc_target, gv11b_pbdma_get_fc_target,
 *          gm20b_pbdma_get_fc_target
 *
 * Input: test_fifo_init_support() run for this GPU
 *
 * Steps:
 * - Check that gv11b_pbdma_get_fc_pb_header() returns default for
 *   PB header (no method, no subch).
 * - Check that gv11b_pbdma_get_fc_target() indicates that contexts
 *   are valid (CE and non-CE).
 *
 * Output: Returns PASS if all branches gave expected results. FAIL otherwise.
 */
int test_gv11b_pbdma_get_fc(struct unit_module *m,
		struct gk20a *g, void *args);

/**
 * Test specification for: test_gv11b_pbdma_set_channel_info_veid
 *
 * Description: PBDMA sub-context id (aka veid)
 *
 * Test Type: Feature
 *
 * Targets: gops_pbdma.set_channel_info_veid,
 *          gv11b_pbdma_set_channel_info_veid
 *
 * Input: test_fifo_init_support() run for this GPU
 *
 * Steps:
 * - For each subctx_id (0..63), check that gv11b_pbdma_set_channel_info_veid
 *   returns veid as per HW manuals.
 *
 * Output: Returns PASS if all branches gave expected results. FAIL otherwise.
 */
int test_gv11b_pbdma_set_channel_info_veid(struct unit_module *m,
		struct gk20a *g, void *args);

/**
 * Test specification for: test_gv11b_pbdma_config_userd_writeback_enable
 *
 * Description: USERD writeback enable
 *
 * Test Type: Feature
 *
 * Targets: gops_pbdma.config_userd_writeback_enable,
 *          gv11b_pbdma_config_userd_writeback_enable
 *
 * Input: test_fifo_init_support() run for this GPU
 *
 * Steps:
 * - Check that gv11b_pbdma_config_userd_writeback_enable() returns
 *   USER writeback enable as per HW manuals.
 *
 * Output: Returns PASS if all branches gave expected results. FAIL otherwise.
 */
int test_gv11b_pbdma_config_userd_writeback_enable(struct unit_module *m,
		struct gk20a *g, void *args);

/**
 * @}
 */

#endif /* UNIT_NVGPU_PBDMA_GV11B_H */
