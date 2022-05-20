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
#ifndef UNIT_NVGPU_LTC_H
#define UNIT_NVGPU_LTC_H

#include <nvgpu/types.h>

/** @addtogroup SWUTS-ltc
 *  @{
 *
 * Software Unit Test Specification for ltc
 */

/**
 * Test specification for: test_ltc_init_support
 *
 * Description: The ltc unit gets initialized
 *
 * Test Type: Feature, Error guessing
 *
 * Targets: gops_ltc.init_ltc_support, nvgpu_init_ltc_support
 *
 * Input: None
 *
 * Steps:
 * - Initialize the test environment for ltc unit testing:
 *   - Setup gv11b register spaces for hals to read emulated values.
 *   - Register read/write IO callbacks.
 *   - Setup init parameters to setup gv11b arch.
 *   - Initialize hal to setup the hal functions.
 * - Call gops_ltc.init_ltc_support to initialize ltc unit.
 * - Call gops_ltc.init_ltc_support a second time to get branch coverage for
 *   already initialzed ltc. Call should not fail.
 * - Call gops_ltc.init_ltc_support with the init_fs_state HAL set to zero. Call
 *   should not fail.
 * - Call gops_ltc.init_ltc_support with fault injection enabled for
 *   nvgpu_kzalloc. Call should fail, but not crash.
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_ltc_init_support(struct unit_module *m,
		struct gk20a *g, void *args);

/**
 * Test specification for: test_ltc_ecc_init_free
 *
 * Description: Validate ltc unit initialization of ecc counters.
 *
 * Test Type: Feature, Error guessing
 *
 * Targets: nvgpu_ecc_counter_init_per_lts, nvgpu_ltc_ecc_free,
 *          gops_ltc.ecc_init, gv11b_lts_ecc_init
 *
 * Input: test_ltc_init_support must have completed successfully.
 *
 * Steps:
 * - Call nvgpu_gr_alloc() since parts of the gr structure are required for
 *   the failure paths.
 * - Save the current ecc count pointers from the gk20a struct and set the gk20a
 *   pointers to NULL.
 * - Setup kmem fault injection to trigger fault on allocation for first alloc.
 * - Call ltc ecc counter init and verify error is returned.
 * - Setup kmem fault injection to trigger fault on allocation for third alloc
 *   to validate failures to allocate on second dimension of array.
 * - Call ltc ecc counter init and verify error is returned.
 * - Re-init ecc support.
 * - Setup kmem fault injection to trigger fault on allocation for fifth alloc
 *   to validate failures to allocate for second ltc ecc stat.
 * - Call ltc ecc counter init and verify error is returned.
 * - Re-init ecc support.
 * - Disable kmem fault injection.
 * - Call ltc ecc counter init and verify no error is returned.
 * - Call ltc ecc counter free.
 * - Restore gk20a ltc ecc counter pointers to previous values.
 * - Free gr structures.
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_ltc_ecc_init_free(struct unit_module *m, struct gk20a *g, void *args);

/**
 * Test specification for: test_ltc_functionality_tests
 *
 * Description: This test test ltc sync enabled and queries data
 * related to different ltc data.
 * Checks whether valid data is returned or not.
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_ltc_sync_enabled, nvgpu_ltc_get_ltc_count,
 *          nvgpu_ltc_get_slices_per_ltc, nvgpu_ltc_get_cacheline_size
 *
 * Input: None
 *
 * Steps:
 * - Set ltc_enabled_current to false and then call
 *   nvgpu_ltc_sync_enabled.
 * - Call nvgpu_ltc_get_ltc_count
 * - Call nvgpu_ltc_get_slices_per_ltc
 * - Call nvgpu_ltc_get_cacheline_size
 * Checked called functions returns correct data.
 *
 * Output: Returns PASS if returned data is valid. FAIL otherwise.
 */
int test_ltc_functionality_tests(struct unit_module *m,
		struct gk20a *g, void *args);

/**
 * Test specification for: test_ltc_negative_tests
 *
 * Description: This test covers negative paths in ltc unit.
 *
 * Test Type: Feature, Error guessing
 *
 * Targets: nvgpu_ltc_sync_enabled, gops_ltc.ltc_remove_support,
 *          gops_ltc.init_ltc_support, nvgpu_init_ltc_support,
 *          nvgpu_ltc_remove_support
 *
 * Input: None
 *
 * Steps:
 * - Set ltc.set_enabled to NULL and then call nvgpu_ltc_sync_enabled
 * - Call gops_ltc.ltc_remove_support twice
 * - Call gops_ltc.init_ltc_support
 *
 * Output: Returns PASS if expected result is met, FAIL otherwise.
 */
int test_ltc_negative_tests(struct unit_module *m,
		struct gk20a *g, void *args);

/**
 * Test specification for: test_ltc_remove_support
 *
 * Description: The ltc unit removes all populated ltc info.
 *
 * Test Type: Feature
 *
 * Targets: gops_ltc.ltc_remove_support, nvgpu_ltc_remove_support
 *
 * Input: None
 *
 * Steps:
 * - Call gops_ltc.ltc_remove_support
 *
 * Output: Returns PASS
 */
int test_ltc_remove_support(struct unit_module *m,
		struct gk20a *g, void *args);

/**
 * Test specification for: test_ltc_intr
 *
 * Description: Validate ltc interrupt handler (isr). The ltc isr is responsible
 *              for reporting errors determind from the ltc status registers.
 *
 * Test Type: Feature
 *
 * Targets: gops_ltc_intr.isr, gv11b_ltc_intr_isr,
 *          gp10b_ltc_intr_handle_lts_interrupts
 *
 * Input: test_ltc_init_support must have completed successfully.
 *
 * Steps:
 * - Allocate ECC stat counter objects used by handler (ecc_sec_count,
 *   ecc_ded_count).
 * - Test LTC isr with no interrupts pending.
 * - Test with corrected and uncorrected bits in the first LTC instances.
 *   - Set the corrected & uncorrected counter overflow bits in the first
 *     ecc_status register (NV_PLTCG_LTC0_LTS0_L2_CACHE_ECC_STATUS).
 *   - Set the interrupt pending bit in the first LTC interrupt register
 *     (NV_PLTCG_LTC0_LTS0_INTR).
 *   - Call the LTC isr.
 * - Test with corrected and uncorrected bits in the second LTC instance.
 *   - Set the corrected & uncorrected counter overflow bits in the second
 *     ecc_status register.
 *   - Set the interrupt pending bit in the second LTC interrupt register.
 *   - Call the LTC isr.
 * - Test with corrected bits only (for branch coverage).
 *   - Set the corrected counter overflow bit and not the uncorrected bit in
 *     the ecc_status register.
 *   - Set the interrupt pending bit in the LTC interrupt register.
 *   - Call the LTC isr.
 * - Test with uncorrected bits only (for branch coverage).
 *   - Set the uncorrected counter overflow bit and not the corrected bit in
 *     the ecc_status register.
 *   - Set the interrupt pending bit in the LTC interrupt register.
 *   - Call the LTC isr.
 * - Test with corrected and uncorrected error counts but without err bits (for
 *   branch coverage).
 *   - Clear the corrected & uncorrected counter overflow bits in the second
 *     ecc_status register.
 *   - Write values to the corrected & uncorrected count registers.
 *   - Set the interrupt pending bit in the second LTC interrupt register.
 *   - Call the LTC isr.
 * - Test handling of dstg error in data RAM.
 *   - Set the dstg corrected & uncorrected error bits in the ecc_status
 *     register.
 *   - Set the dstg RAM mask field of the dstg_ecc_address register
 *     (NV_PLTCG_LTC0_LTS0_DSTG_ECC_ADDRESS) to report data RAM.
 *   - Set the interrupt pending bit in the first LTC interrupt register.
 *   - Call the LTC isr.
 * - Test handling of dstg error in byte enable (BE) RAM.
 *   - Set the dstg corrected & uncorrected error bits in the ecc_status
 *     register.
 *   - Set the dstg RAM mask field of the dstg_ecc_address register to report
 *     BE RAM.
 *   - Set the interrupt pending bit in the first LTC interrupt register.
 *   - Call the LTC isr.
 * - Test handling of tstg and rstg errors.
 *   - Set the tstg and rstg, corrected & uncorrected counter error bits in the
 *     ecc_status register.
 *   - Set the interrupt pending bit in the first LTC interrupt register.
 *   - Call the LTC isr.
 * - Test handling of sec and ded errors.
 *   - Set the sec and ded pending error bits in the ecc_status register.
 *   - Set the interrupt pending bit in the first LTC interrupt register.
 *   - Call the LTC isr.
 * - Test handling of sec and ded errors when the l2 flush API succeeds (for
 *     branch coverage).
 *   - Override the MM l2_flush HAL to return success.
 *   - Set the sec and ded pending error bits in the ecc_status register.
 *   - Set the interrupt pending bit in the first LTC interrupt register.
 *   - Call the LTC isr.
 *
 * Output: Returns PASS unless counter initialization fails or an except occurs
 *         in interrupt handler.
 */
int test_ltc_intr(struct unit_module *m, struct gk20a *g, void *args);

/**
 * Test specification for: test_ltc_intr_en_illegal_compstat
 *
 * Description: Validate the inter_en_illegal_compstat API.
 *
 * Test Type: Feature
 *
 * Targets: gops_ltc_intr.en_illegal_compstat, gv11b_ltc_intr_en_illegal_compstat
 *
 * Input: None
 *
 * Steps:
 * - Clear the LTC intr register (NV_PLTCG_LTCS_LTSS_INTR).
 * - Call the gv11b_ltc_intr_en_illegal_compstat HAL requesting enable.
 * - Verify correct setting in LTC intr register.
 * - Call the gv11b_ltc_intr_en_illegal_compstat HAL requesting disable.
 * - Verify correct setting in LTC intr register.
 *
 * Output: Returns PASS if register is configured correctly. FAIL otherwise.
 */
int test_ltc_intr_en_illegal_compstat(struct unit_module *m,
				struct gk20a *g, void *args);

/**
 * Test specification for: test_ltc_intr_configure
 *
 * Description: Validate the ltc interrupt configure API.
 *
 * Test Type: Feature
 *
 * Targets: gops_ltc_intr.configure, gv11b_ltc_intr_configure
 *
 * Input: None
 *
 * Steps:
 * - Call the gops_ltc_intr.configure HAL.
 * - Verify correct setting in LTC intr register (NV_PLTCG_LTCS_LTSS_INTR).
 * - For branch coverage, verify handling when en_illegal_compstat HAL is NULL.
 *   - Set en_illegal_compstat HAL to NULL.
 *   - Call the gv11b_ltc_intr_configure HAL.
 *   - Verify correct setting in LTC intr register.
 *
 * Output: Returns PASS if register is configured correctly. FAIL otherwise.
 */
int test_ltc_intr_configure(struct unit_module *m,
				struct gk20a *g, void *args);

/**
 * Test specification for: test_determine_L2_size_bytes
 *
 * Description: Validate the ltc API to determine L2 size.
 *
 * Test Type: Feature
 *
 * Targets: gops_ltc.determine_L2_size_bytes, gp10b_determine_L2_size_bytes
 *
 * Input: test_ltc_init_support must have completed successfully.
 *
 * Steps:
 * - Set the L2 configuration in the ltc NV_PLTCG_LTC0_LTSS_TSTG_INFO_ register.
 * - Call the gops_ltc.determine_L2_size_bytes HAL.
 * - Verify the correct L2 size is returned.
 *
 * Output: Returns PASS if correct size returned. FAIL otherwise.
 */
int test_determine_L2_size_bytes(struct unit_module *m,
				struct gk20a *g, void *args);

/**
 * Test specification for: test_ltc_set_enabled
 *
 * Description: Validate the ltc API to enable level 2 cache.
 *
 * Test Type: Feature
 *
 * Targets: gops_ltc.set_enabled, gp10b_ltc_set_enabled
 *
 * Input: None
 *
 * Steps:
 * - Clear the NV_PLTCG_LTCS_LTSS_TSTG_SET_MGMT_2 register
 * - Call the gops_ltc.set_enabled HAL requesting enable.
 * - Verify the L2 bypass mode is disabled in NV_PLTCG_LTCS_LTSS_TSTG_SET_MGMT_2.
 * - Clear the NV_PLTCG_LTCS_LTSS_TSTG_SET_MGMT_2 register
 * - Call the gops_ltc.set_enabled HAL requesting disable.
 * - Verify the L2 bypass mode is enabled in NV_PLTCG_LTCS_LTSS_TSTG_SET_MGMT_2.
 *
 * Output: Returns PASS if register is configured correctly. FAIL otherwise.
 */
int test_ltc_set_enabled(struct unit_module *m,	struct gk20a *g, void *args);

/**
 * Test specification for: test_flush_ltc
 *
 * Description: Validate the ltc API to flush the cache.
 *
 * Test Type: Feature, Error guessing
 *
 * Targets: gops_ltc.flush, gm20b_flush_ltc
 *
 * Input: None
 *
 * Steps:
 * - Configure the registers to reflect the clean and invalidate has completed
 *   for each ltc.
 * - Call the flush API.
 * - Configure the registers to reflect the clean and invalidate are pending
 *   for each ltc.
 * - Call the flush API to get branch coverage of the timeout handling.
 *
 * Output: Returns PASS if register is configured correctly. FAIL otherwise.
 */
int test_flush_ltc(struct unit_module *m, struct gk20a *g, void *args);

/**
 * @}
 */

#endif /* UNIT_NVGPU_LTC_H */
