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
#ifndef UNIT_NVGPU_ENGINE_GV11B_H
#define UNIT_NVGPU_ENGINE_GV11B_H

#include <nvgpu/types.h>

struct unit_module;
struct gk20a;

/** @addtogroup SWUTS-fifo-engine-gv11b
 *  @{
 *
 * Software Unit Test Specification for fifo/engine/gv11b
 */

/**
 * Test specification for: test_gv11b_is_fault_engine_subid_gpc
 *
 * Description: Branch coverage for gv11b_is_fault_engine_subid_gpc
 *
 * Test Type: Feature
 *
 * Targets: gops_engine.is_fault_engine_subid_gpc,
 *          gv11b_is_fault_engine_subid_gpc
 *
 * Input: test_fifo_init_support() run for this GPU
 *
 * Steps:
 * - Check that true is returned for GPC engine subid
 *   (i.e. gmmu_fault_client_type_gpc_v).
 * - Check that false is returned for non-GPC engine subid
 *   (i.e. gmmu_fault_client_type_hub_v).
 *
 * Output: Returns PASS if all branches gave expected results. FAIL otherwise.
 */
int test_gv11b_is_fault_engine_subid_gpc(struct unit_module *m,
		struct gk20a *g, void *args);

/**
 * @}
 */

#endif /* UNIT_NVGPU_ENGINE_GV11B_H */
