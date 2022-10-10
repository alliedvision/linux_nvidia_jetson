/*
 * Copyright (c) 2019-2022, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef UNIT_NVGPU_THERM_H
#define UNIT_NVGPU_THERM_H

struct gk20a;
struct unit_module;

/** @addtogroup SWUTS-therm
 *  @{
 *
 * Software Unit Test Specification for therm
 */

/**
 * Test specification for: therm_test_setup_env
 *
 * Description: Do basic setup before starting other tests.
 *
 * Test Type: Other (setup)
 *
 * Input: None
 *
 * Steps:
 * - Initialize reg spaces used by tests.
 * - Setup HAL function pointers.
 *
 * Output:
 * - UNIT_FAIL if encounters an error creating reg space
 * - UNIT_SUCCESS otherwise
 */
int therm_test_setup_env(struct unit_module *m,
			  struct gk20a *g, void *args);

/**
 * Test specification for: therm_test_free_env
 *
 * Description: Cleanup resources allocated in therm_test_setup_env
 *
 * Test Type: Other (setup)
 *
 * Input: therm_test_setup_env has run.
 *
 * Steps:
 * - Free reg spaces.
 *
 * Output: UNIT_SUCCESS always.
 */
int therm_test_free_env(struct unit_module *m, struct gk20a *g, void *args);

/**
 * Test specification for: test_therm_init_support
 *
 * Description: Validate API nvgpu_init_therm_support.
 *
 * Test Type: Feature based, Error guessing.
 *
 * Targets: gops_therm.init_therm_support, gops_therm.init_therm_setup_hw,
 *          nvgpu_init_therm_support, gv11b_init_therm_setup_hw
 *
 * Input: therm_test_setup_env has run.
 *
 * Steps:
 * - Call API gops_therm.init_therm_support and verify it returns success.
 * - Set the HAL init_therm_setup_hw to NULL.
 * - Call API gops_therm.init_therm_support and verify it returns success.
 * - Set the HAL init_therm_setup_hw to a mock function that returns failure.
 * - Call API gops_therm.init_therm_support and verify it returns err.
 *
 * Output: Returns PASS if expected result is met, FAIL otherwise.
 */
int test_therm_init_support(struct unit_module *m, struct gk20a *g, void *args);

/**
 * Test specification for: test_therm_init_elcg_mode
 *
 * Description: Validate HAL init_elcg_mode.
 *
 * Test Type: Feature based, Error guessing.
 *
 * Targets: gops_therm.init_elcg_mode, gv11b_therm_init_elcg_mode
 *
 * Test Type: Feature based, Error guessing.
 *
 * Steps:
 * - Enable ELCG flag.
 * - Loop through 2 engines:
 *   - Loop through all Gate modes (RUN, AUTO, STOP), for each iteration:
 *     - Set the THERM_GATE_CTRL register to 0.
 *     - Call the HAL gops_therm.init_elcg_mode.
 *     - Read the THERM_GATE_CTRL register and verify register setting.
 *   - Repeat for an Invalid Gate mode for branch coverage.
 * - Disable ELCG flag.
 * - Set the THERM_GATE_CTRL register to 0.
 * - Call the HAL gops_therm.init_elcg_mode.
 * - Read the THERM_GATE_CTRL register and verify register setting were
 *   unchanged.
 * Output: Returns PASS if expected result is met, FAIL otherwise.
 */
int test_therm_init_elcg_mode(struct unit_module *m, struct gk20a *g,
					void *args);

/**
 * Test specification for: test_elcg_init_idle_filters
 *
 * Description: Validate HAL elcg_init_idle_filters.
 *
 * Test Type: Feature based, Error guessing.
 *
 * Targets: gops_therm.elcg_init_idle_filters, gv11b_elcg_init_idle_filters
 *
 * Input: therm_test_setup_env has run.
 *
 * Steps:
 * - Setup FIFO in gk20a struct for 2 active engines.
 * - Set the THERM_GATE_CTRL, THERM_FECS_IDLE_FILTER and
 *   THERM_HUBMMU_IDLE_FILTER registers to 0.
 * - Set the mock flag for simulation mode.
 * - Call the HAL gops_therm.elcg_init_idle_filters.
 * - Verify the API returns success and no register values were changed.
 * - Clear the mock flag for simulation mode.
 * - Call the HAL gops_therm.elcg_init_idle_filters.
 * - Verify the API returns success and the register values were correct.
 *
 * Output: Returns PASS if expected result is met, FAIL otherwise.
 */
int test_elcg_init_idle_filters(struct unit_module *m, struct gk20a *g,
					void *args);

/**
 * @}
 */

#endif /* UNIT_NVGPU_THERM_H */
