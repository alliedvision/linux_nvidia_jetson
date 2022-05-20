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
#ifndef UNIT_NVGPU_CTXSW_TIMEOUT_GV11B_H
#define UNIT_NVGPU_CTXSW_TIMEOUT_GV11B_H

#include <nvgpu/types.h>

struct unit_module;
struct gk20a;

/** @addtogroup SWUTS-fifo-ctxsw_timeout-gv11b
 *  @{
 *
 * Software Unit Test Specification for fifo/ctxsw_timeout/gv11b
 */

/**
 * Test specification for: test_gv11b_fifo_ctxsw_timeout_enable
 *
 * Description: Test ctxsw timeout enable/disable.
 *
 * Test Type: Feature
 *
 * Targets: gops_fifo.ctxsw_timeout_enable, gv11b_fifo_ctxsw_timeout_enable
 *
 * Input: test_fifo_init_support() run for this GPU
 *
 * Steps:
 * - Calculate ctxsw timout value and store.
 * - Set MSB bit to enable timeout and reset to disable it.
 * - Check if timeout value saved is correct.
 *
 * Output: Returns PASS if all branches gave expected results. FAIL otherwise.
 */
int test_gv11b_fifo_ctxsw_timeout_enable(struct unit_module *m,
		struct gk20a *g, void *args);

/**
 * Test specification for: test_gv11b_fifo_handle_ctxsw_timeout
 *
 * Description: Test ctxsw
 *
 * Test Type: Feature
 *
 * Targets: gops_fifo.handle_ctxsw_timeout, gv11b_fifo_handle_ctxsw_timeout,
 *          gops_fifo.ctxsw_timeout_info, gv11b_fifo_ctxsw_timeout_info
 *
 * Input: test_fifo_init_support() run for this GPU
 *
 * Steps:
 * - Go through engines list to find out which engines are pending.
 * - Figure out tsgid from ctx_status and info_status.
 * - Clear interrupts by writing 1 to corresponding engine id.
 * - Check that the timeout clear interrupts value written to memory is correct.
 *
 * Output: Returns PASS if all branches gave expected results. FAIL otherwise.
 */
int test_gv11b_fifo_handle_ctxsw_timeout(struct unit_module *m,
		struct gk20a *g, void *args);

/**
 * @}
 */

#endif /* UNIT_NVGPU_CTXSW_TIMEOUT_GV11B_H */
