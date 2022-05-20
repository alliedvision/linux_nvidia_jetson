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
#ifndef UNIT_NVGPU_RAMFC_GP10B_H
#define UNIT_NVGPU_RAMFC_GP10B_H

#include <nvgpu/types.h>

struct unit_module;
struct gk20a;

/** @addtogroup SWUTS-fifo-ramfc-gp10b
 *  @{
 *
 * Software Unit Test Specification for fifo/ramfc/gp10b
 */

/**
 * Test specification for: test_gp10b_ramfc_commit_userd
 *
 * Description: Test userd commit
 *
 * Test Type: Feature
 *
 * Targets: gp10b_ramfc_commit_userd
 *
 * Input: None
 *
 * Steps:
 * - Save userd aperture and address in channel instance block.
 * - Check stored value is correct as expected.
 *
 * Output: Returns PASS if all branches gave expected results. FAIL otherwise.
 */
int test_gp10b_ramfc_commit_userd(struct unit_module *m, struct gk20a *g,
								void *args);

/**
 * @}
 */

#endif /* UNIT_NVGPU_RAMFC_GP10B_H */
