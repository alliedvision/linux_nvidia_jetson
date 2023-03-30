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
#ifndef UNIT_NVGPU_FIFO_H
#define UNIT_NVGPU_FIFO_H

#include <nvgpu/types.h>

struct unit_module;
struct gk20a;

/** @addtogroup SWUTS-fifo-fifo
 *  @{
 *
 * Software Unit Test Specification for fifo/fifo */

/**
 * Test specification for: test_init_support
 *
 * Description: Test fifo software context init.
 *
 * Test Type: Feature
 *
 * Targets: gops_fifo.fifo_init_support, nvgpu_fifo_init_support,
 *          gops_fifo.setup_sw, nvgpu_fifo_setup_sw,
 *          nvgpu_fifo_setup_sw_common, nvgpu_fifo_cleanup_sw,
 *          nvgpu_fifo_cleanup_sw_common
 *
 * Input: None
 *
 * Steps:
 * - Initialize FIFO software with nvgpu_fifo_setup_sw_common(). If successful,
 *   initialize FIFO hardware setup.
 * - Test FIFO software and hardware setup with following cases:
 *   - FIFO software is already initialized.
 *   - Channel, TSG, PBDMA, engine or runlist setup fail.
 *   - PBDMA setup_sw and/or cleanup_sw is NULL.
 *   - FIFO hardware setup failure.
 *
 * Output: Returns PASS if all branches gave expected results. FAIL otherwise.
 */
int test_init_support(struct unit_module *m, struct gk20a *g, void *args);

/**
 * Test specification for: test_decode_pbdma_ch_eng_status
 *
 * Description: Test decoding of PBDMA channel/engine status.
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_fifo_decode_pbdma_ch_eng_status
 *
 * Input: None
 *
 * Steps:
 * - Test decode string returned for each possible index.
 *
 * Output: Returns PASS if all branches gave expected results. FAIL otherwise.
 */
int test_decode_pbdma_ch_eng_status(struct unit_module *m, struct gk20a *g,
								void *args);

/**
 * Test specification for: test_fifo_suspend
 *
 * Description: Test FIFO suspend
 *
 * Test Type: Feature
 *
 * Targets: gops_fifo.fifo_suspend, nvgpu_fifo_suspend
 *
 * Input: None
 *
 * Steps:
 * - Execute FIFO suspend and check if interrupt 0 and 1 are set to false.
 *
 * Output: Returns PASS if all branches gave expected results. FAIL otherwise.
 */
int test_fifo_suspend(struct unit_module *m, struct gk20a *g, void *args);

/**
 * Test specification for: test_fifo_sw_quiesce
 *
 * Description: Test FIFO quiescing
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_fifo_sw_quiesce
 *
 * Input: None
 *
 * Steps:
 * - Execute fifo sw quiesce and check runlist state.
 *
 * Output: Returns PASS if all branches gave expected results. FAIL otherwise.
 */
int test_fifo_sw_quiesce(struct unit_module *m, struct gk20a *g, void *args);


#endif /* UNIT_NVGPU_FIFO_H */
