/*
 * Copyright (c) 2019-2021, NVIDIA CORPORATION.  All rights reserved.
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
#ifndef UNIT_NVGPU_GR_FALCON_GM20B_H
#define UNIT_NVGPU_GR_FALCON_GM20B_H

#include <nvgpu/types.h>

struct unit_module;
struct gk20a;

/** @addtogroup SWUTS-gr-falcon-gm20b
 *  @{
 *
 * Software Unit Test Specification for common.gr.falcon
 */

/**
 * Test specification for: test_gr_falcon_gm20b_ctrl_ctxsw
 *
 * Description: Helps to verify various failure and conditional checking
 *              in falcon gm20b hal functions.
 *
 * Test Type: Error injection
 *
 * Input: #test_fifo_init_support() run for this GPU
 *
 * Targets: gm20b_gr_falcon_wait_mem_scrubbing,
 *          gops_gr_falcon.wait_mem_scrubbing,
 *          gm20b_gr_falcon_wait_ctxsw_ready,
 *          gops_gr_falcon.wait_ctxsw_ready,
 *          gm20b_gr_falcon_init_ctx_state,
 *          gm20b_gr_falcon_submit_fecs_method_op,
 *          nvgpu_gr_get_falcon_ptr,
 *          gm20b_gr_falcon_ctrl_ctxsw
 *
 * Steps:
 * -  Call gm20b_gr_falcon_ctrl_ctxsw with watchdog timeout Method.
 * -  Call g->ops.gr.falcon.ctrl_ctxsw with Invalid Method.
 * -  Call gm20b_gr_falcon_submit_fecs_method_op with various
 *    method op codes.
 * -  Check that enable_set bit is set for ccsr_channel_r
 *
 * Output: Returns PASS if all branches gave expected results. FAIL otherwise.
 */
int test_gr_falcon_gm20b_ctrl_ctxsw(struct unit_module *m,
		struct gk20a *g, void *args);
/**
 * @}
 */

#endif /* UNIT_NVGPU_GR_FALCON_GK20A_H */
