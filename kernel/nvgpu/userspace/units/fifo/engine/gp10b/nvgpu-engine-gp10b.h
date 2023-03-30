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
#ifndef UNIT_NVGPU_ENGINE_GP10B_H
#define UNIT_NVGPU_ENGINE_GP10B_H

#include <nvgpu/types.h>

struct unit_module;
struct gk20a;

/** @addtogroup SWUTS-fifo-engine-gp10b
 *  @{
 *
 * Software Unit Test Specification for fifo/engine/gp10b
 */

/**
 * Test specification for: test_gp10b_engine_init_ce_info
 *
 * Description: Branch coverage for gp10b_engine_init_ce_info
 *
 * Test Type: Feature
 *
 * Targets: gp10b_engine_init_ce_info
 *
 * Input: test_fifo_init_support has run..
 *
 * Steps:
 * - Check valid cases for gp10b_engine_init_ce_info:
 *   - Check GRCE case (runlist shared with GR engine).
 *   - Check fault_id adjustment for GRCE (0 -> 0x1b).
 *   - Check ASYCNC CE case (runlist NOT shared with GR engine).
 *   In valid cases, check that function returns 0 and that expected number
 *   of CE engines has been added.
 *
 * - Use stubs to check failure cases for gp10b_engine_init_ce_info:
 *   - g->ops.top.get_num_engine_type_entries is NULL.
 *   - g->ops.top.get_num_engine_type_entries returns 0.
 *   - Failure to get device info with g->ops.top.get_device_info.
 *   - Failure to find PBDMA servicing engine runlist (i.e. failure of
 *     g->ops.pbdma.find_for_runlist).
 *   In all failure cases, check that error code is returned.
 *
 * Output: Returns PASS if all branches gave expected results. FAIL otherwise.
 */
int test_gp10b_engine_init_ce_info(struct unit_module *m,
		struct gk20a *g, void *args);

/**
 * @}
 */

#endif /* UNIT_NVGPU_ENGINE_GP10B_H */
