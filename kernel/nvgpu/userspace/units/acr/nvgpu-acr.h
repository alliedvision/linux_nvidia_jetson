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

struct gk20a;
struct unit_module;

/** @addtogroup SWUTS-acr
 *  @{
 *
 * Software Unit Test Specification for acr
 */

/**
 * Test specification for: test_acr_init
 *
 * Description: The test_acr_init shall test the initialization of
 * the ACR unit
 *
 * Test Type: Feature, Error guessing
 *
 * Targets: nvgpu_acr_init
 *
 * Input: None
 *
 * Steps:
 * - Initialize the falcon test environment
 * - Initialize the ECC support
 * - Initialize the PMU
 * - Inject memory allocation fault to test the fail scenario 1
 * - Give incorrect chip version to test the fail scenario 2
 * - Give correct chip id and set the register to enable debug mode
 *   to have branch coverage
 * - Give correct chip id and test the pass scenario
 * - Uninitialize the PMU support
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_acr_init(struct unit_module *m, struct gk20a *g, void *args);

/**
 * Test specification for: test_acr_prepare_ucode_blob
 *
 * Description: The test_acr_prepare_ucode_blob shall test the blob creation of
 * the ACR unit
 *
 * Test Type: Feature, Error guessing
 *
 * Targets: g->acr->prepare_ucode_blob
 *
 * Input: None
 * Steps:
 * - Initialize the test env and register space needed for the test
 * - Prepare HW and SW setup needed for the test
 * - Inject memmory allocation failure to test fai scneario for
 *   g->acr->prepare_ucode_blob(g)
 * - Give incorrect chip version number to test second fail scenario
 * - NVGPU_SEC_SECUREGPCCS flag is set to false to get the branch coverage
 * - NVGPU_SEC_SECUREGPCCS flag is set to true to test the pass scenario
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */

int test_acr_prepare_ucode_blob(struct unit_module *m, struct gk20a *g,
					void *__args);
/**
 * Test specification for: test_acr_is_lsf_lazy_bootstrap
 *
 * Description: The test_acr_is_lsf_lazy_bootstrap shall test the
 * lazy bootstrap of the ACR unit
 *
 * Test Type: Feature, Error guessing
 *
 * Targets: nvgpu_acr_is_lsf_lazy_bootstrap
 *
 * Input: None
 *
 * Steps:
 * - Initialize the test env and register space needed for the test
 * - Prepare HW and SW setup needed for the test
 * - Pass scenario: lsf lazy bootstrap the ACR for following falcon ids:
 *   FALCON_ID_FECS, FALCON_ID_PMU and FALCON_ID_GPCCS
 * - Pass invalid falcon id to fail the function
 * - Pass acr as NULL to fail nvgpu_acr_is_lsf_lazy_bootstrap()
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */

int test_acr_is_lsf_lazy_bootstrap(struct unit_module *m, struct gk20a *g,
					void *__args);
/**
 * Test specification for: test_acr_construct_execute
 *
 * Description: The test_acr_construct_execute shall test the two main tasks of
 * the ACR unit:
 * 1. Blob construct of LS ucode in non-wpr memory
 * 2. ACR HS ucode load & bootstrap
 *
 * Test Type: Feature, Error guessing
 *
 * Targets: nvgpu_acr_construct_execute
 *
 * Input: None
 *
 * Steps:
 * - Initialize the test env and register space needed for the test
 * - Prepare HW and SW setup needed for the test
 * - Set the falcon_falcon_cpuctl_halt_intr_m bit for the
 *   register falcon_falcon_cpuctl_r
 * - Inject memory allocation failure in g->acr->prepare_ucode_blob so that
 *   acr_construct_execute() fails
 * - Cover fail scenario when "is_falcon_supported"
 *    is set to false. This fails nvgpu_acr_bootstrap_hs_acr()
 * - Set is_falcon_supported to true to test the pass scenario
 * - Pass g->acr as NULL to create fail scenario.
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */

int test_acr_construct_execute(struct unit_module *m,
					struct gk20a *g, void *args);
/**
 * Test specification for: test_acr_bootstrap_hs_acr
 *
 * Description: The test_acr_bootstrap_hs_acr shall test the ACR HS ucode load
 * & bootstrap functionality of the ACR unit
 *
 * Test Type: Feature, Error guessing
 *
 * Targets: nvgpu_acr_bootstrap_hs_acr, nvgpu_pmu_report_bar0_pri_err_status,
 *	gops_pmu.validate_mem_integrity, gv11b_pmu_validate_mem_integrity,
 *	gops_pmu.is_debug_mode_enabled, gv11b_pmu_is_debug_mode_en,
 *	gops_acr.pmu_clear_bar0_host_err_status,
 *	gv11b_clear_pmu_bar0_host_err_status, gops_pmu.bar0_error_status,
 *	gv11b_pmu_bar0_error_status
 *
 * Input: None
 *
 * Steps:
 * - Initialize the test env and register space needed for the test
 * - Prepare HW and SW setup needed for the test
 * - Call prepare_ucode_blob without setting halt bit so that
 *     timeout error occurs in acr bootstrap
 * - Set the falcon_falcon_cpuctl_halt_intr_m bit for the
 *    register falcon_falcon_cpuctl_r
 * - Prepare the ucode blob
 * - Set  mailbox_error = true to create read failure for mailbox 0 register
 * - Inject memory allocation failure to fail nvgpu_acr_bootstrap_hs_acr()
 * - Call nvgpu_acr_bootstrap_hs_acr() twice to cover recovery branch.
 * - Cover branch for fail scenario when "is_falcon_supported" is set to false
 * - Cover branch by setting g->acr->acr.acr_engine_bus_err_status = NULL
 * - Cover branch when "acr_engine_bus_err_status" ops fails
 * - Cover all scenarios to test gv11b_pmu_bar0_error_status() by wriring
 *    different values to pwr_pmu_bar0_error_status_r() register
 * - Set g->acr->acr.acr_validate_mem_integrity = NULL to cover branch
 * - Set g->acr->acr.report_acr_engine_bus_err_status = NULL to cover branch
 * - Set ->ops.pmu.is_debug_mode_enabled = true to get branch coverage
 * - Cover branch by setting p->is_silicon = true
 * - Pass g->acr = NULL to fail nvgpu_acr_bootstrap_hs_acr()
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */

int test_acr_bootstrap_hs_acr(struct unit_module *m,
					struct gk20a *g, void *args);
