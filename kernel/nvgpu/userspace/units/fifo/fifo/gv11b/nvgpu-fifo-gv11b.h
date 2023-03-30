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
#ifndef UNIT_NVGPU_FIFO_GV11B_H
#define UNIT_NVGPU_FIFO_GV11B_H

#include <nvgpu/types.h>

struct unit_module;
struct gk20a;

/** @addtogroup SWUTS-fifo-fifo-gv11b
 *  @{
 *
 * Software Unit Test Specification for fifo/fifo/gv11b
 */

/**
 * Test specification for: test_gv11b_fifo_init_hw
 *
 * Description: Reset and enable HW
 *
 * Test Type: Feature
 *
 * Targets: gops_fifo.reset_enable_hw, gv11b_init_fifo_reset_enable_hw,
 *          gops_fifo.init_fifo_setup_hw, gv11b_init_fifo_setup_hw
 *
 * Input: test_fifo_init_support() run for this GPU
 *
 * Steps:
 * - Check silicon platform case.
 *   - Call gv11b_init_fifo_reset_enable_hw and gv11b_init_fifo_setup_hw.
 *   - Check that userd writeback has been enabled.
 * - Check path for non-silicon platform
 *   - Check that fifof fb timeout has been programmed.
 *
 * Output: Returns PASS if all branches gave expected results. FAIL otherwise.
 */
int test_gv11b_fifo_init_hw(struct unit_module *m,
		struct gk20a *g, void *args);

/**
 * Test specification for: test_gv11b_fifo_mmu_fault_id_to_pbdma_id
 *
 * Description: Get PBDMA id from MMU fault
 *
 * Test Type: Feature
 *
 * Targets: gops_fifo.mmu_fault_id_to_pbdma_id,
 *          gv11b_fifo_mmu_fault_id_to_pbdma_id
 *
 * Input: test_fifo_init_support() run for this GPU
 *
 * Steps:
 * - Set fifo_cfg0_r with 3 PBDMAs, starting at MMU fault_id 15.
 * - Check that gv11b_fifo_mmu_fault_id_to_pbdma_id returns correct pbdma_id
 *   for all MMU fault_id in 15 <= mmu_fault_id < (15 + num_pbdma - 1)
 * - Check that gv11b_fifo_mmu_fault_id_to_pbdma_id returns INVALID_ID when
 *   mmu_fault_id < 15 or mmu_fault_id >= (15 + num_pbdma).
 *
 * Output: Returns PASS if all branches gave expected results. FAIL otherwise.
 */
int test_gv11b_fifo_mmu_fault_id_to_pbdma_id(struct unit_module *m,
		struct gk20a *g, void *args);

/**
 * @}
 */

#endif /* UNIT_NVGPU_FIFO_GV11B_H */
