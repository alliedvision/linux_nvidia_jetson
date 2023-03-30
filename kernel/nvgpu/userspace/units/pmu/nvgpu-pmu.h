/*
 * Copyright (c) 2020, NVIDIA CORPORATION.  All rights reserved.
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

struct gk20a;
struct unit_module;

/** @addtogroup SWUTS-pmu
 *  @{
 *
 * Software Unit Test Specification for pmu
 */

/**
 * Test specification for: test_pmu_early_init
 *
 * Description: The test_pmu_early_init shall test the
 * initialization of the PMU unit
 *
 * Test Type: Feature, Error guessing
 *
 * Targets: gops_pmu.pmu_early_init, nvgpu_pmu_early_init, gops_pmu.ecc_init,
 * 	gv11b_pmu_ecc_init, gops_pmu.ecc_free, gv11b_pmu_ecc_free
 *
 * Input: None
 *
 * Steps:
 * - Initialize the falcon test environment
 * - initialize the ECC init support, MM and LTC support
 * - Initialize the PMU
 * - Inject memory allocation fault to test the fail scenario 1
 * - Inject memory allocation fault to fail g->ops.pmu.ecc_init(g)
 * - Set correct parameters to test the pass scenario
 * - Set g->support_ls_pmu = false to test the fail scenario
 * - Set g->ops.pmu.is_pmu_supported = false to test the fail scenario
 * - Remove the PMU support
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_pmu_early_init(struct unit_module *m, struct gk20a *g, void *args);

/**
 * Test specification for: test_is_pmu_supported
 *
 * Description: The test_is_pmu_supported shall test the fail
 * scenario of the PMU unit
 *
 * Test Type: Error guessing
 *
 * Targets: gops_pmu.is_pmu_supported, gv11b_is_pmu_supported
 *
 * Input: None
 * Steps:
 * - Initialize the falcon test environment
 * - initialize the ECC init support
 * - Initialize the PMU unit
 * - Call g->ops.pmu.is_pmu_supported(g)
 * - Status for PMU support is returned as false
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */

int test_is_pmu_supported(struct unit_module *m, struct gk20a *g,
					void *__args);
/**
 * Test specification for: test_pmu_remove_support
 *
 * Description: The test_pmu_remove_support shall test the deinit of
 * PMU unit
 *
 * Test Type: Feature, Error guessing
 *
 * Targets: nvgpu_pmu_remove_support
 *
 * Input: None
 * Steps:
 * - Initialize the PMU unit
 * - Deinit the PMU unit
 * - Deinitilisation of PMU happens successfully
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */

int test_pmu_remove_support(struct unit_module *m, struct gk20a *g,
					void *__args);
/**
 * Test specification for: test_pmu_reset
 *
 * Description: The test_pmu_reset shall test the reset of the PMU unit
 *
 * Test Type: Feature, Error guessing
 *
 * Targets: nvgpu_pmu_enable_irq, nvgpu_pmu_reset, gops_pmu.pmu_enable_irq,
 *		gv11b_pmu_enable_irq,
 *		gops_pmu.get_irqdest, gv11b_pmu_get_irqdest,
 *		gops_pmu.reset_engine, gv11b_pmu_engine_reset,
 *		gops_pmu.is_engine_in_reset, gv11b_pmu_is_engine_in_reset
 *
 * Input: None
 *
 * Steps:
 * - Initialize the falcon environment
 * - initialize the ECC init support, MM and LTC support
 * - Initialize the PMU
 * - Reset the PMU to test the pass scenario
 * - Set the falcon_falcon_idlestate_r register to 0x1
 *   to make the falcon busy so that idle wait function fails
 *   This case covers failig branch of the reset function
 * - Set the falcon dmactl register to 0x2 (IMEM_SCRUBBING_PENDING)
 *    to test the fail scenario
 * - Set pwr_falcon_engine_r true to fail gv11b_pmu_is_engine_in_reset()
 * - Set g->is_fusa_sku = true to get branch coverage
 * - g->ops.pmu.pmu_enable_irq to NULL to achieve branch coverage
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */

int test_pmu_reset(struct unit_module *m, struct gk20a *g,
					void *__args);
/**
 * Test specification for: test_pmu_isr
 *
 * Description: The test_pmu_isr shall test the two main tasks of
 * the ISR routine of PMU.
 *
 * Test Type: Feature, Error guessing
 *
 * Targets: gops_pmu.pmu_isr, gk20a_pmu_isr,
		gops_pmu.handle_ext_irq, gv11b_pmu_handle_ext_irq
 *
 * Input: None
 *
 * Steps:
 * - Initialize the falcon environment
 * - Initialize the various registers needed for the test
 * - initialize the ECC init support
 * - Initialize the PMU
 * - Set the IRQ stat and mask registers
 * - Call the g->ops.pmu.pmu_isr(g) to test the pass scenario
 * - Test the fail scenario by setting pwr_pmu_falcon_ecc_status_r() and
 *   pwr_pmu_ecc_intr_status_r() register to create interrupts with
 *   different values
 * - Set pwr_falcon_irqstat_r(), pwr_falcon_irqmask_r() and
 *   pwr_falcon_irqdest_r() register to 0x1 to test branches in the function
 *   gv11b_pmu_handle_ext_irq()
 * - Set pwr_falcon_irqmask_r() and pwr_falcon_irqdest_r() to
 *   pwr_falcon_irqstat_ext_ecc_parity_true_f() i.e.0x400
 *   Set pwr_falcon_irqstat_r() to 0x0 to cover branch for intr = 0 in
 *   gk20a_pmu_isr()
 * - Set g->ops.pmu.handle_ext_irq = NULL to achieve branch coverage
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */

int test_pmu_isr(struct unit_module *m, struct gk20a *g, void *args);
