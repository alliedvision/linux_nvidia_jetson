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

#ifndef UNIT_MM_HAL_GP10B_FUSA_H
#define UNIT_MM_HAL_GP10B_FUSA_H

struct gk20a;
struct unit_module;

/** @addtogroup SWUTS-mm-hal-gp10b_fusa
 *  @{
 *
 * Software Unit Test Specification for mm.hal.gp10b_fusa
 */

/**
 * Test specification for: test_env_init_mm_gp10b_fusa
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
int test_env_init_mm_gp10b_fusa(struct unit_module *m, struct gk20a *g,
								void *args);

/**
 * Test specification for: test_gp10b_mm_init_bar2_vm
 *
 * Description: Initialize bar2 VM
 *
 * Test Type: Feature, Error injection
 *
 * Targets: gops_mm.init_bar2_vm, gp10b_mm_init_bar2_vm, gops_mm.remove_bar2_vm,
 * gp10b_mm_remove_bar2_vm
 *
 * Input: test_env_init, args (value can be F_INIT_BAR2_VM_DEFAULT,
 *        F_INIT_BAR2_VM_INIT_VM_FAIL or F_INIT_BAR2_VM_ALLOC_INST_BLOCK_FAIL)
 *
 * Steps:
 * - Allocate and initialize bar2 VM.
 * - Check failure cases when allocation fails.
 *
 * Output: Returns SUCCESS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_gp10b_mm_init_bar2_vm(struct unit_module *m, struct gk20a *g,
								void *args);

/**
 * Test specification for: test_env_clean_mm_gp10b_fusa
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
int test_env_clean_mm_gp10b_fusa(struct unit_module *m, struct gk20a *g,
								void *args);

/** @} */
#endif /* UNIT_MM_HAL_GP10B_FUSA_H */
