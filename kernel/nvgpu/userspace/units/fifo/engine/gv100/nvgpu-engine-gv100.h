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
#ifndef UNIT_NVGPU_ENGINE_GV100_H
#define UNIT_NVGPU_ENGINE_GV100_H

#include <nvgpu/types.h>

struct unit_module;
struct gk20a;

/** @addtogroup SWUTS-fifo-engine-gv100
 *  @{
 *
 * Software Unit Test Specification for fifo/engine/gv100
 */

/**
 * Test specification for: test_gv100_read_engine_status_info
 *
 * Description: Branch coverage for gv100_read_engine_status_info
 *
 * Test Type: Feature
 *
 * Targets: gops_engine_status.read_engine_status_info,
 *          gv100_read_engine_status_info
 *
 * Input: test_fifo_init_support() run for this GPU
 *
 * Steps:
 * - Check that status.in_reload_status field is consistent with
 *   fifo_engine_status_eng_reload_f bit of fifo_engine_status_r H/W register.
 * - Other bits tested in a separate test for gm20b_read_engine_status_info.
 *
 * Output: Returns PASS if all branches gave expected results. FAIL otherwise.
 */
int test_gv100_read_engine_status_info(struct unit_module *m,
		struct gk20a *g, void *args);

/**
 * Test specification for: test_gv100_dump_engine_status
 *
 * Description: Branch coverage for gv100_dump_engine_status
 *
 * Test Type: Feature
 *
 * Targets: gops_engine_status.dump_engine_status, gv100_dump_engine_status
 *
 * Input: test_fifo_init_support() run for this GPU
 *
 * Steps:
 * - Check dumping of engine status, with combinations of:
 *   - ctx_id_type is TSG/channel.
 *   - ctx_next_id_tupe is TSG/channel.
 *   - in_reload_status is true/false.
 *   - is_faulted is true/false.
 *   - is_busy is true/false.
 *   Check number that read_engine_status_info was (num_engines - 1) times.
 *
 * Output: Returns PASS if all branches gave expected results. FAIL otherwise.
 */
int test_gv100_dump_engine_status(struct unit_module *m,
		struct gk20a *g, void *args);

/**
 * @}
 */

#endif /* UNIT_NVGPU_ENGINE_GV100_H */
