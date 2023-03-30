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
#ifndef UNIT_NVGPU_RAMFC_GV11B_H
#define UNIT_NVGPU_RAMFC_GV11B_H

#include <nvgpu/types.h>

struct unit_module;
struct gk20a;

/** @addtogroup SWUTS-fifo-ramfc-gv11b
 *  @{
 *
 * Software Unit Test Specification for fifo/ramfc/gv11b
 */

/**
 * Test specification for: test_gv11b_ramfc_setup
 *
 * Description: Test ramfc setup for channel
 *
 * Test Type: Feature
 *
 * Targets: gops_ramfc.setup, gv11b_ramfc_setup
 *
 * Input: None
 *
 * Steps:
 * - Save pbdma config values in channel instance block memory.
 * - Check that the stored value is correct.
 *
 * Output: Returns PASS if all branches gave expected results. FAIL otherwise.
 */
int test_gv11b_ramfc_setup(struct unit_module *m, struct gk20a *g, void *args);

/**
 * Test specification for: test_gv11b_ramfc_capture_ram_dump
 *
 * Description: Test channel status dump
 *
 * Test Type: Feature based
 *
 * Targets: gops_ramfc.capture_ram_dump, gv11b_ramfc_capture_ram_dump
 *
 * Input: None
 *
 * Steps:
 * - Read channel status from channel instance block.
 * - Check that channel dump info read is correct as expected.
 *
 * Output: Returns PASS if all branches gave expected results. FAIL otherwise.
 */
int test_gv11b_ramfc_capture_ram_dump(struct unit_module *m,
						struct gk20a *g, void *args);

/**
 * @}
 */

#endif /* UNIT_NVGPU_RAMFC_GV11B_H */
