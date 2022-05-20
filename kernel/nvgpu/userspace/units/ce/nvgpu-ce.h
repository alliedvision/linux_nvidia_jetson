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

#ifndef UNIT_NVGPU_CE_H
#define UNIT_NVGPU_CE_H

struct gk20a;
struct unit_module;

/** @addtogroup SWUTS-ce
 *  @{
 *
 * Software Unit Test Specification for CE
 */

/**
 * Test specification for: test_ce_setup_env
 *
 * Description: Do basic setup before starting other tests.
 *
 * Test Type: Other (setup)
 *
 * Input: None
 *
 * Steps:
 * - Initialize reg spaces used by tests.
 * - Initialize required data for cg, mc modules.
 *
 * Output:
 * - UNIT_FAIL if encounters an error creating reg space
 * - UNIT_SUCCESS otherwise
 */
int test_ce_setup_env(struct unit_module *m,
			  struct gk20a *g, void *args);

/**
 * Test specification for: test_ce_free_env
 *
 * Description: Do basic setup before starting other tests.
 *
 * Test Type: Other (setup)
 *
 * Input: None
 *
 * Steps:
 * - Free reg spaces
 *
 * Output: UNIT_SUCCESS always.
 */
int test_ce_free_env(struct unit_module *m, struct gk20a *g, void *args);

/**
 * Test specification for: test_ce_init_support
 *
 * Description: Validate CE init functionality.
 *
 * Test Type: Feature
 *
 * Targets: gops_ce.ce_init_support, nvgpu_ce_init_support
 *
 * Input: test_ce_setup_env must have been run.
 *
 * Steps:
 * - Setup necessary mock HALs to do nothing and return success as appropriate.
 * - Call nvgpu_ce_init_support and verify success is returned.
 * - Set set_pce2lce_mapping and init_prod_values HAL function pointers to NULL
 *   for branch coverage.
 * - Call nvgpu_ce_init_support and verify success is returned.
 *
 * Output: Returns PASS if expected result is met, FAIL otherwise.
 */
int test_ce_init_support(struct unit_module *m, struct gk20a *g, void *args);

/**
 * Test specification for: test_ce_stall_isr
 *
 * Description: Validate stall interrupt handler functionality.
 *
 * Test Type: Feature
 *
 * Targets: gops_ce.isr_stall, gv11b_ce_stall_isr, gp10b_ce_stall_isr
 *
 * Input: test_ce_setup_env must have been run.
 *
 * Steps:
 * - Set all CE interrupt sources pending in the interrupt status reg for each
 *   instance.
 * - Call gops_ce.isr_stall.
 * - Verify all (and only) the stall interrupts are cleared.
 * - Set no CE interrupt sources pending in the interrupt status reg for each
 *   instance.
 * - Call gops_ce.isr_stall.
 * - Verify no interrupts are cleared.
 *
 * Output: Returns PASS if expected result is met, FAIL otherwise.
 */
int test_ce_stall_isr(struct unit_module *m, struct gk20a *g, void *args);

/**
 * Test specification for: test_ce_nonstall_isr
 *
 * Description: Validate nonstall interrupt handler functionality.
 *
 * Test Type: Feature
 *
 * Targets: gops_ce.isr_nonstall, gp10b_ce_nonstall_isr
 *
 * Input: test_ce_setup_env must have been run.
 *
 * Steps:
 * - Set all CE interrupt sources pending in the interrupt status reg for each
 *   instance.
 * - Call gops_ce.isr_nonstall.
 * - Verify only the nonstall interrupt is cleared and the expected ops are
 *   returned.
 * - Set no CE interrupt sources pending in the interrupt status reg for each
 *   instance.
 * - Call gops_ce.isr_nonstall.
 * - Verify no interrupts are cleared and no ops are returned.
 *
 * Output: Returns PASS if expected result is met, FAIL otherwise.
 */
int test_ce_nonstall_isr(struct unit_module *m, struct gk20a *g, void *args);

/**
 * Test specification for: test_mthd_buffer_fault_in_bar2_fault
 *
 * Description: Validate method buffer interrupt functionality.
 *
 * Test Type: Feature
 *
 * Targets: gops_ce.mthd_buffer_fault_in_bar2_fault,
 *          gv11b_ce_mthd_buffer_fault_in_bar2_fault
 *
 * Input: test_ce_setup_env must have been run.
 *
 * Steps:
 * - Set all CE interrupt sources pending in the interrupt status reg for each
 *   instance.
 * - Call gops_ce.mthd_buffer_fault_in_bar2_fault.
 * - Verify only the correct interrupt is cleared.
 * - Set no CE interrupt sources pending in the interrupt status reg for each
 *   instance.
 * - Call gops_ce.mthd_buffer_fault_in_bar2_fault.
 * - Verify no interrupts are cleared.
 *
 * Output: Returns PASS if expected result is met, FAIL otherwise.
 */
int test_mthd_buffer_fault_in_bar2_fault(struct unit_module *m, struct gk20a *g,
					void *args);

/**
 * Test specification for: test_get_num_pce
 *
 * Description: Validate function to get number of PCEs.
 *
 * Test Type: Feature
 *
 * Targets: gops_ce.get_num_pce, gv11b_ce_get_num_pce
 *
 * Input: test_ce_setup_env must have been run.
 *
 * Steps:
 * - Loop through all possible 16 bit values for the PCE Map register.
 *   - For each value, write to the PCE Map register.
 *   - Call gops_ce.get_num_pce and verify the correct number of PCEs is
 *     returned.
 *
 * Output: Returns PASS if expected result is met, FAIL otherwise.
 */
int test_get_num_pce(struct unit_module *m, struct gk20a *g, void *args);

/**
 * Test specification for: test_init_prod_values
 *
 * Description: Validate prod value init functionality.
 *
 * Test Type: Feature
 *
 * Targets: gops_ce.init_prod_values, gv11b_ce_init_prod_values
 *
 * Input: test_ce_setup_env must have been run.
 *
 * Steps:
 * - Clear the LCE Options register for all instances.
 * - Call gops_ce.init_prod_values.
 * - Verify all instances of the LCE Options register are set properly.
 *
 * Output: Returns PASS if expected result is met, FAIL otherwise.
 */
int test_init_prod_values(struct unit_module *m, struct gk20a *g, void *args);

#endif /* UNIT_NVGPU_CE_H */