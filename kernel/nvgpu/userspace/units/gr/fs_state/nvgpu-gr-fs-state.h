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
#ifndef UNIT_NVGPU_GR_FS_STATE_H
#define UNIT_NVGPU_GR_FS_STATE_H

#include <nvgpu/types.h>

struct gk20a;
struct unit_module;

/** @addtogroup SWUTS-gr-fs-state
 *  @{
 *
 * Software Unit Test Specification for common.gr.fs_state
 */

/**
 * Test specification for: test_gr_fs_state_error_injection.
 *
 * Description: Verify error handling in #nvgpu_gr_fs_state_init()
 *
 * Test Type: Feature, Error guessing
 *
 * Targets: nvgpu_gr_fs_state_init,
 *          gv11b_gr_init_tpc_mask
 *
 * Input: gr_fs_state_setup must have been executed successfully.
 *
 * Steps:
 * - Negative tests.
 *   - Inject faults to trigger memory allocation failures in various
 *     functions called from #nvgpu_gr_fs_state_init.
 *   - Call #nvgpu_gr_fs_state_init and ensure that function returns
 *     error.
 *   - Set stub function for g->ops.gr.init.get_no_of_sm() which returns 0,
 *     meaning no SM was detected. nvgpu_gr_fs_state_init() should return
 *     error, and also a BUG is detected.
 *
 * - Positive test.
 *   - Disable all fault injections.
 *   - Call #nvgpu_gr_fs_state_init and ensure that function returns
 *     success.
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_gr_fs_state_error_injection(struct unit_module *m,
		struct gk20a *g, void *args);

#endif /* UNIT_NVGPU_GR_FS_STATE_H */

/**
 * @}
 */

