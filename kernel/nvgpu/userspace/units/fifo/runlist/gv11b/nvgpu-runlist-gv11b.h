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
#ifndef UNIT_NVGPU_RUNLIST_GV11B_H
#define UNIT_NVGPU_RUNLIST_GV11B_H

#include <nvgpu/types.h>

struct unit_module;
struct gk20a;

/** @addtogroup SWUTS-fifo-runlist-gv11b
 *  @{
 *
 * Software Unit Test Specification for fifo/runlist/gv11b
 */

/**
 * Test specification for: test_gv11b_runlist_entry_size
 *
 * Description: Branch coverage for gv11b_runlist_entry_size
 *
 * Test Type: Feature
 *
 * Targets: gops_runlist.entry_size, gv11b_runlist_entry_size
 *
 * Input: test_fifo_init_support() run for this GPU
 *
 * Steps:
 * - Check that runlist entry size matches the H/W manuals
 *
 * Output: Returns PASS if all branches gave expected results. FAIL otherwise.
 */
int test_gv11b_runlist_entry_size(struct unit_module *m,
		struct gk20a *g, void *args);

/**
 * Test specification for: test_gv11b_runlist_get_tsg_entry
 *
 * Description: Branch coverage for gv11b_runlist_get_tsg_entry
 *
 * Test Type: Feature
 *
 * Targets: gops_runlist.get_tsg_entry, gv11b_runlist_get_tsg_entry
 *
 * Input: test_fifo_init_support() run for this GPU
 *
 * Steps:
 * - Allocate TSG.
 * - Get runlist entry with timeslice that does not need scaling
 *   - Check timeout and scale in returned runlist entry
 *   - Check length and tsgid as well
 * - Get runlist entry with a timeslice that needs scaling
 * - Get runlist entry with an oversize timeslice
 *
 * Output: Returns PASS if all branches gave expected results. FAIL otherwise.
 */
int test_gv11b_runlist_get_tsg_entry(struct unit_module *m,
		struct gk20a *g, void *args);

/**
 * Test specification for: test_gv11b_runlist_get_ch_entry
 *
 * Description: Branch coverage for gv11b_runlist_get_ch_entry
 *
 * Test Type: Feature
 *
 * Targets: gops_runlist.get_ch_entry, gv11b_runlist_get_ch_entry
 *
 * Input: test_fifo_init_support() run for this GPU
 *
 * Steps:
 * - Allocate channel.
 * - Get runlist entry for the channel.
 * - Check userd and inst block addr in returned runlist entry.
 *
 * Output: Returns PASS if all branches gave expected results. FAIL otherwise.
 */
int test_gv11b_runlist_get_ch_entry(struct unit_module *m,
		struct gk20a *g, void *args);

/**
 * Test specification for: test_gv11b_runlist_count_max
 *
 * Description: Branch coverage for gv11b_runlist_count_max
 *
 * Test Type: Feature
 *
 * Targets: gops_runlist.count_max, gv11b_runlist_count_max
 *
 * Input: test_fifo_init_support() run for this GPU
 *
 * Steps:
 * - Check engine runlist base size is equal to runlist base size defined by
 *   hw manual.
 *
 * Output: Returns PASS if all branches gave expected results. FAIL otherwise.
 */
int test_gv11b_runlist_count_max(struct unit_module *m,
		struct gk20a *g, void *args);

/**
 * @}
 */

#endif /* UNIT_NVGPU_RUNLIST_GV11B_H */
