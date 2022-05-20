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

#ifndef UNIT_MM_HAL_GV11B_FUSA_H
#define UNIT_MM_HAL_GV11B_FUSA_H

struct gk20a;
struct unit_module;

/** @addtogroup SWUTS-mm-hal-gv11b-fusa
 *  @{
 *
 * Software Unit Test Specification for mm.hal.gv11b_fusa
 */

/**
 * Test specification for: test_env_init_mm_gv11b_fusa
 *
 * Description: Initialize environment for MM tests
 *
 * Test Type: Feature
 *
 * Targets: None
 *
 * Input: None
 *
 * Steps:
 * - Init HALs and initialize VMs similar to nvgpu_init_system_vm().
 *
 * Output: Returns SUCCESS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_env_init_mm_gv11b_fusa(struct unit_module *m, struct gk20a *g,
					void *args);

/**
 * Test specification for: test_gv11b_mm_init_inst_block
 *
 * Description: Initialize instance block
 *
 * Test Type: Feature
 *
 * Targets: gops_mm.init_inst_block, gv11b_mm_init_inst_block
 *
 * Input: test_env_init, args (value can be F_INIT_INST_BLOCK_SET_BIG_PAGE_ZERO,
 *        F_INIT_INST_BLOCK_SET_BIG_PAGE_SIZE_NULL or
 *        F_INIT_INST_BLOCK_INIT_SUBCTX_PDB_NULL)
 *
 * Steps:
 * - Allocate memory for instance block.
 * - Initialize GPU accessible instance block memory.
 *
 * Output: Returns SUCCESS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_gv11b_mm_init_inst_block(struct unit_module *m, struct gk20a *g,
					void *args);

/**
 * Test specification for: test_gv11b_mm_is_bar1_supported
 *
 * Description: Test if bar1_is_supported
 *
 * Test Type: Feature
 *
 * Targets: gops_mm.is_bar1_supported, gv11b_mm_is_bar1_supported
 *
 * Input: test_env_init
 *
 * Steps:
 * - Execute gv11b_mm_is_bar1_supported() to check if bar1 is supported.
 *
 * Output: Returns SUCCESS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_gv11b_mm_is_bar1_supported(struct unit_module *m, struct gk20a *g,
					void *args);
/**
 * Test specification for: test_env_clean_mm_gv11b_fusa
 *
 * Description: Cleanup test environment
 *
 * Test Type: Feature
 *
 * Targets: None
 *
 * Input: test_env_init
 *
 * Steps:
 * - Destroy memory and VMs initialized for the test.
 *
 * Output: Returns SUCCESS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_env_clean_mm_gv11b_fusa(struct unit_module *m, struct gk20a *g,
					void *args);

/** @} */
#endif /* UNIT_MM_HAL_GV11B_FUSA_H */
