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
#ifndef UNIT_NVGPU_FIFO_COMMON_H
#define UNIT_NVGPU_FIFO_COMMON_H

#include <nvgpu/types.h>

#ifdef UNIT_FIFO_DEBUG
#define unit_verbose	unit_info
#else
#define unit_verbose(unit, msg, ...) \
	do { \
		if (0) {\
			unit_info(unit, msg, ##__VA_ARGS__); \
		} \
	} while (0)
#endif

/** @addtogroup SWUTS-fifo-common
 *  @{
 *
 * Software Unit Test Specification for fifo
 */

/**
 * Test specification for: test_fifo_init_support
 *
 * Description: The FIFO unit shall initialize all sub-units.
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_fifo_init_support, nvgpu_fifo_cleanup_sw_common
 *
 * Input: None
 *
 * Steps:
 * - Setup gv11b register spaces for MASTER, TOP, FIFO, PBDMA, CCSR
 *   and USERMODE. This allows some HAL to read emulated values of gv11b
 *   registers.
 * - Init HAL for to use gv11b defaults
 * - Stub some HALs that would require reg access
 *   - g->ops.gr.init.get_no_of_sm
 * - Also stub the following HAL, since BAR1 is not initialized,
 *   and USERD not used in safety build
 *   - g->ops.userd.setup_sw
 * - Additionnaly the following HALs are set to NULL, as currenty
 *   not needed for subsequent tests.
 *   - g->ops.fifo.init_fifo_setup_hw = NULL;
 *   - g->ops.tsg.init_eng_method_buffers = NULL;
 * - Call nvgpu_fifo_init_support
 * - Cleanup gv11b register spaces.
 *
 * Output: Returns PASS if FIFO unit could be initialized. FAIL otherwise.
 */
int test_fifo_init_support(struct unit_module *m,
		struct gk20a *g, void *args);

/**
 * Test specification for: test_fifo_remove_support
 *
 * Description: The FIFO unit shall de-initialize all sub-units.
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_fifo_remove_support, nvgpu_fifo_cleanup_sw,
 *          nvgpu_fifo_cleanup_sw_common
 *
 * Input: test_fifo_init_support() called for this GPU.
 *
 * Steps:
 * - Call g->fifo.remove_support if defined
 * - Cleanup gv11b register spaces.
 *
 * Output: Returns PASS if FIFO unit could be initialized. FAIL otherwise.
 */
int test_fifo_remove_support(struct unit_module *m,
		struct gk20a *g, void *args);

/**
 * @}
 */

bool test_fifo_subtest_pruned(u32 branches, u32 final_branches);
char *test_fifo_flags_str(u32 flags, const char *labels[]);
u32 test_fifo_get_log2(u32 num);
#endif /* UNIT_NVGPU_FIFO_COMMON_H */
