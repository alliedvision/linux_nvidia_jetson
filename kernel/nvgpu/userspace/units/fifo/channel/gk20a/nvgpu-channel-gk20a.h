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
#ifndef UNIT_NVGPU_CHANNEL_GK20A_H
#define UNIT_NVGPU_CHANNEL_GK20A_H

#include <nvgpu/types.h>

struct unit_module;
struct gk20a;

/** @addtogroup SWUTS-fifo-channel-gk20a
 *  @{
 *
 * Software Unit Test Specification for fifo/channel/gk20a
 */

/**
 * Test specification for: test_gk20a_channel_enable
 *
 * Description: Branch coverage for gk20a_channel_enable
 *
 * Test Type: Feature
 *
 * Targets: gops_channel.enable, gk20a_channel_enable
 *
 * Input: test_fifo_init_support() run for this GPU
 *
 * Steps:
 * - Allocate channel
 * - Call gk20a_channel_enable
 * - Check that enable_set bit is set for ccsr_channel_r
 *
 * Output: Returns PASS if all branches gave expected results. FAIL otherwise.
 */
int test_gk20a_channel_enable(struct unit_module *m,
		struct gk20a *g, void *args);

/**
 * Test specification for: test_gk20a_channel_disable
 *
 * Description: Branch coverage for gk20a_channel_disable
 *
 * Test Type: Feature
 *
 * Targets: gops_channel.disable, gk20a_channel_disable
 *
 * Input: test_fifo_init_support() run for this GPU
 *
 * Steps:
 * - Allocate channel
 * - Call gk20a_channel_disable
 * - Check that enable_clr bit is set for ccsr_channel_r
 *
 * Output: Returns PASS if all branches gave expected results. FAIL otherwise.
 */
int test_gk20a_channel_disable(struct unit_module *m,
		struct gk20a *g, void *args);

/**
 * Test specification for: test_gk20a_channel_read_state
 *
 * Description: Branch coverage for gk20a_channel_read_state
 *
 * Test Type: Feature
 *
 * Targets: gops_channel.read_state, gk20a_channel_read_state
 *
 * Input: test_fifo_init_support() run for this GPU
 *
 * Steps:
 * - Allocate channel
 * - Build ccsr_channel_r with all combinations of next, enable,
 *   status and busy fields.
 * - Check that interpreted status for next, enabled, busy, ctx_reload
 *   and pending_acquire are in accordance with fields read from H/W.
 *
 * Output: Returns PASS if all branches gave expected results. FAIL otherwise.
 */
int test_gk20a_channel_read_state(struct unit_module *m,
		struct gk20a *g, void *args);

/**
 * @}
 */

#endif /* UNIT_NVGPU_CHANNEL_GK20A_H */
