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
#ifndef UNIT_NVGPU_CHANNEL_GV11B_H
#define UNIT_NVGPU_CHANNEL_GV11B_H

#include <nvgpu/types.h>

struct unit_module;
struct gk20a;

/** @addtogroup SWUTS-fifo-channel-gv11b
 *  @{
 *
 * Software Unit Test Specification for fifo/channel/gv11b
 */

/**
 * Test specification for: test_gv11b_channel_unbind
 *
 * Description: Branch coverage for gv11b_channel_unbind
 *
 * Test Type: Feature
 *
 * Targets: gops_channel.unbind, gv11b_channel_unbind
 *
 * Input: test_fifo_init_support() run for this GPU
 *
 * Steps:
 * - Allocate channel.
 * - Bind channel with g->ops.channel.bind().
 * - Check that channel is bound (ch->bound == 1).
 * - Clear ccsr_channel_inst_r and ccsr_channel_r registers.
 * - Unbind channel with gv11b_channel_unbind().
 * - Check that channel is not bound (ch->bound == 0).
 * - Check that ccsr registers were programmed to unbind channel.
 *
 * Output: Returns PASS if all branches gave expected results. FAIL otherwise.
 */
int test_gv11b_channel_unbind(struct unit_module *m,
		struct gk20a *g, void *args);

/**
 * Test specification for: test_gv11b_channel_count
 *
 * Description: Branch coverage for gv11b_channel_count
 *
 * Test Type: Feature
 *
 * Targets: gops_channel.count, gv11b_channel_count
 *
 * Input: test_fifo_init_support() run for this GPU
 *
 * Steps:
 * - Check that number of channel matches H/W manuals definition.
 *
 * Output: Returns PASS if all branches gave expected results. FAIL otherwise.
 */
int test_gv11b_channel_count(struct unit_module *m,
		struct gk20a *g, void *args);

/**
 * Test specification for: test_gv11b_channel_read_state
 *
 * Description: Branch coverage for gv11b_channel_read_state
 *
 * Test Type: Feature
 *
 * Targets: gops_channel.read_state, gv11b_channel_read_state
 *
 * Input: test_fifo_init_support() run for this GPU
 *
 * Steps:
 * - Allocate channel.
 * - Set ccsr_channel_r.
 * - Read state with gv11b_channel_read_state.
 * - Check case w/ and w/o eng_faulted.
 *
 * Note: other values are checked in gk20a_channel_read_state.
 *
 * Output: Returns PASS if all branches gave expected results. FAIL otherwise.
 */
int test_gv11b_channel_read_state(struct unit_module *m,
		struct gk20a *g, void *args);

/**
 * Test specification for: test_gv11b_channel_reset_faulted
 *
 * Description: Branch coverage for gv11b_channel_reset_faulted
 *
 * Test Type: Feature
 *
 * Targets: gops_channel.reset_faulted, gv11b_channel_reset_faulted
 *
 * Input: test_fifo_init_support() run for this GPU
 *
 * Steps:
 * - Allocate channel.
 * - Clear ccsr_channel_r register.
 * - Call gv11b_channel_reset_faulted.
 * - Check that eng_faulted_reset bit is set when eng is true.
 * - Check that pbdma_faulted_reset bit is set when pbdma is true.
 *
 * Output: Returns PASS if all branches gave expected results. FAIL otherwise.
 */
int test_gv11b_channel_reset_faulted(struct unit_module *m,
		struct gk20a *g, void *args);

/**
 * @}
 */

#endif /* UNIT_NVGPU_CHANNEL_GV11B_H */
