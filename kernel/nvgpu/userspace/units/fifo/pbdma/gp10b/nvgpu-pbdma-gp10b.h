/*
 * Copyright (c) 2019, NVIDIA CORPORATION.  All rights reserved.
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
#ifndef UNIT_NVGPU_PBDMA_GP10B_H
#define UNIT_NVGPU_PBDMA_GP10B_H

#include <nvgpu/types.h>

struct unit_module;
struct gk20a;

/** @addtogroup SWUTS-fifo-pbdma-gp10b
 *  @{
 *
 * Software Unit Test Specification for fifo/pbdma/gp10b
 */

/**
 * Test specification for: test_gp10b_pbdma_get_signature
 *
 * Description: Get RAMFC setting for PBDMA signature
 *
 * Test Type: Feature
 *
 * Targets: gp10b_pbdma_get_signature
 *
 * Input: test_fifo_init_support() run for this GPU
 *
 * Steps:
 * - Check that PBDMA signature consists in the host class for current litter,
 *   combined with a SW signature set to 0.
 *
 * Output: Returns PASS if all branches gave expected results. FAIL otherwise.
 */
int test_gp10b_pbdma_get_signature(struct unit_module *m,
		struct gk20a *g, void *args);

/**
 * Test specification for: test_gp10b_pbdma_get_fc_runlist_timeslice
 *
 * Description: Get RAMFC setting for runlist timeslice
 *
 * Test Type: Feature
 *
 * Targets: gp10b_pbdma_get_fc_runlist_timeslice
 *
 * Input: test_fifo_init_support() run for this GPU
 *
 * Steps:
 * - Get runlist timeslice, and check that timeout and scale are within
 *   the range used for runlists.
 * - Also check that enable bit is set.
 *
 * Output: Returns PASS if all branches gave expected results. FAIL otherwise.
 */
int test_gp10b_pbdma_get_fc_runlist_timeslice(struct unit_module *m,
		struct gk20a *g, void *args);

/**
 * Test specification for: test_gp10b_pbdma_get_config_auth_level_privileged
 *
 * Description: Get RAMFC setting for privileged channel
 *
 * Test Type: Feature
 *
 * Targets: gp10b_pbdma_get_config_auth_level_privileged
 *
 * Input: test_fifo_init_support() run for this GPU
 *
 * Steps:
 * - Check that gp10b_pbdma_get_config_auth_level_privileged returns a value
 *   consistent with H/W manuals.
 *
 * Output: Returns PASS if all branches gave expected results. FAIL otherwise.
 */
int test_gp10b_pbdma_get_config_auth_level_privileged(struct unit_module *m,
		struct gk20a *g, void *args);

/**
 * @}
 */

#endif /* UNIT_NVGPU_PBDMA_GP10B_H */
