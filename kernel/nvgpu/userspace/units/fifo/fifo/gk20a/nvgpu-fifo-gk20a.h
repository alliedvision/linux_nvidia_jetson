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
#ifndef UNIT_NVGPU_FIFO_GK20A_H
#define UNIT_NVGPU_FIFO_GK20A_H

#include <nvgpu/types.h>

struct unit_module;
struct gk20a;

/** @addtogroup SWUTS-fifo-fifo-gk20a
 *  @{
 *
 * Software Unit Test Specification for fifo/fifo/gk20a
 */

/**
 * Test specification for: test_gk20a_init_pbdma_map
 *
 * Description: Init PBDMA to runlists map
 *
 * Test Type: Feature
 *
 * Targets: gops_fifo.init_pbdma_map, gk20a_fifo_init_pbdma_map
 *
 * Input: test_fifo_init_support() run for this GPU
 *
 * Steps:
 * - Get number of PBDMA with nvgpu_get_litter_value.
 * - Call gk20a_fifo_init_pbdma_map using a pre-allocated pbdma_map.
 * - Check that pbdma_map[id] is non-zero for all PBDMAs.
 *
 * Output: Returns PASS if all branches gave expected results. FAIL otherwise.
 */
int test_gk20a_init_pbdma_map(struct unit_module *m,
		struct gk20a *g, void *args);


/**
 * Test specification for: test_gk20a_get_timeslices
 *
 * Description: Init PBDMA to runlists map
 *
 * Test Type: Feature
 *
 * Targets: gops_fifo.get_runlist_timeslice, gk20a_fifo_get_runlist_timeslice,
 *          gops_fifo.get_pb_timeslice, gk20a_fifo_get_pb_timeslice
 *
 * Input: test_fifo_init_support() run for this GPU
 *
 * Steps:
 * - Get runlist timeslice using gk20a_fifo_get_runlist_timeslice.
 * - Get PBDMA timeslice using gk20a_fifo_get_pb_timeslice.
 * - Check that timeslices are enabled, and non-zero.
 *
 * Output: Returns PASS if all branches gave expected results. FAIL otherwise.
 */
int test_gk20a_get_timeslices(struct unit_module *m,
		struct gk20a *g, void *args);

/**
 * @}
 */

#endif /* UNIT_NVGPU_FIFO_GK20A_H */
