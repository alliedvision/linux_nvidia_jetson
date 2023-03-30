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

#ifndef UNIT_NVGPU_PTIMER_H
#define UNIT_NVGPU_PTIMER_H

struct gk20a;
struct unit_module;

/** @addtogroup SWUTS-ptimer
 *  @{
 *
 * Software Unit Test Specification for nvgpu.common.ptimer
 */

/**
 * Test specification for: ptimer_test_setup_env
 *
 * Description: Setup prerequisites for tests.
 *
 * Test Type: Other (setup)
 *
 * Input: None
 *
 * Steps:
 * - Setup ptimer HAL function pointers.
 * - Setup timer reg space in mockio.
 *
 * Output:
 * - UNIT_FAIL if encounters an error creating reg space
 * - UNIT_SUCCESS otherwise
 */
int ptimer_test_setup_env(struct unit_module *m,
			struct gk20a *g, void *args);

/**
 * Test specification for: ptimer_test_free_env
 *
 * Description: Release resources from ptimer_test_setup_env()
 *
 * Test Type: Other (setup)
 *
 * Input: ptimer_test_setup_env() has been executed.
 *
 * Steps:
 * - Delete ptimer register space from mockio.
 *
 * Output:
 * - UNIT_FAIL if encounters an error creating reg space
 * - UNIT_SUCCESS otherwise
 */
int ptimer_test_free_env(struct unit_module *m,
			struct gk20a *g, void *args);

/**
 * Test specification for: test_ptimer_isr
 *
 * Description: Verify the ptimer isr API. The ISR only logs the errors and
 *              clears the ISR regs. This test verifies the code paths do not
 *              cause errors.
 *
 * Test Type: Feature Based
 *
 * Targets: gops_ptimer.isr, gk20a_ptimer_isr
 *
 * Input: None
 *
 * Steps:
 * - Test isr with 0 register values.
 *   - Initialize registers to 0: pri_timeout_save_0, pri_timeout_save_1,
 *     pri_timeout_fecs_errcode.
 *   - Call isr API.
 *   - Verify the save_* regs were all set to 0.
 * - Test with FECS bits set.
 *   - Set the fecs bit in the pri_timeout_save_0 reg and an error code in the
 *     pri_timeout_fecs_errcode reg.
 *   - Call isr API.
 *   - Verify the save_* regs were all set to 0.
 * - Test with FECS bits set and verify priv_ring decode error HAL is invoked.
 *   - Set the fecs bit in the pri_timeout_save_0 reg and an error code in the
 *     pri_timeout_fecs_errcode reg.
 *   - Set the HAL priv_ring.decode_error_code to a mock function.
 *   - Call isr API.
 *   - Verify the fecs error code was passed to the decode_error_code mock
 *     function.
 *   - Verify the save_* regs were all set to 0.
 * - Test branch for save0 timeout bit being set.
 *   - Set the timeout bit in the pri_timeout_save_0 reg.
 *   - Call isr API.
 *   - Verify the save_* regs were all set to 0.
 *
 * Output:
 * - UNIT_FAIL if encounters an error creating reg space
 * - UNIT_SUCCESS otherwise
 */
int test_ptimer_isr(struct unit_module *m,
			struct gk20a *g, void *args);

/**
 * Test specification for: test_ptimer_scaling
 *
 * Description: Verify the nvgpu_ptimer_scale() API.
 *
 * Test Type: Feature Based, Boundary Values
 *
 * Targets: nvgpu_ptimer_scale
 *
 * Equivalence classes:
 * Variable: timeout
 * - Valid : 0 to U32_MAX/10
 *
 * Input: None
 *
 * Steps:
 * - Initialize ptimer source freq as per gv11b platform freq (i.e. 31250000U).
 * - Call the nvgpu_ptimer_scale() API with below BVEC test values and verify the
 *   returned value and error code.
 *   Valid test values : 0, 1000, U32_MAX/10
 *   Invalid test values : U32_MAX/10 + 1, U32_MAX/5, U32_MAX
 *
 * Output:
 * - UNIT_FAIL if encounters an error creating reg space
 * - UNIT_SUCCESS otherwise
 */
int test_ptimer_scaling(struct unit_module *m,
			struct gk20a *g, void *args);

#endif /* UNIT_NVGPU_PTIMER_H */
