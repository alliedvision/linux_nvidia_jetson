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

#ifndef UNIT_NVGPU_FB_H
#define UNIT_NVGPU_FB_H

struct gk20a;
struct unit_module;

/** @addtogroup SWUTS-fb
 *  @{
 *
 * Software Unit Test Specification for nvgpu.hal.fb
 */

/**
 * Test specification for: fb_gv11b_init_test
 *
 * Description: Tests the init HALs for GV11B.
 *
 * Targets: nvgpu_ecc_init_support, gv11b_fb_init_hw, gv11b_fb_init_fs_state,
 * gv11b_fb_ecc_init, gv11b_fb_ecc_free, gops_fb.fb_ecc_free,
 * gops_fb.fb_ecc_init, gops_ecc.ecc_init_support, gops_fb.init_hw,
 * gops_fb.init_fs_state, gm20b_fb_init_hw
 *
 * Test Type: Feature, Other (setup), Error injection
 *
 * Input: None
 *
 * Steps:
 * - Set up the ops function pointer for all the HALs under test.
 * - Initialize the g->mm structure with arbitrary addresses.
 * - Call the ecc_init_support HAL to initialize ECC support.
 * - Call the init_hw HAL and ensure the FB_NISO mask was set.
 * - Call the init_fs_state HAL and ensure atomic mode was set in the MMU
 *   control register.
 * - Perform dynamic memory error injection on the fb_ecc_init HAL to ensure
 *   it fails as expected.
 * - Call the fb_ecc_init HAL and ensure it succeeds.
 * - Call the fb_ecc_free HAL to free dynamic memory.
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int fb_gv11b_init_test(struct unit_module *m, struct gk20a *g, void *args);

/**
 * Test specification for: fb_gm20b_tlb_invalidate_test
 *
 * Description: .
 *
 * Targets: gm20b_fb_tlb_invalidate, gops_fb.tlb_invalidate
 *
 * Test Type: Feature, Error injection
 *
 * Input: None
 *
 * Steps:
 * - Initialize ops.fb.tlb_invalidate pointer to gm20b_fb_tlb_invalidate HAL.
 * - Create a test nvgpu_mem PDB with SYSMEM aperture.
 * - While the NVGPU is powered off, call gm20b_fb_tlb_invalidate and ensure
 *   it returned success.
 * - The power on state of NVGPU.
 * - Call gm20b_fb_tlb_invalidate again and check that it still failed (because
 *   the fb_mmu_ctrl_r register is not set properly)
 * - Set the fb_mmu_ctrl_pri_fifo_space_v bit in fb_mmu_ctrl_r register.
 * - Using an helper during register writes, intercept writes to fb_mmu_ctrl_r
 *   to cause a timeout after the MMU invalidate. Ensure that
 *   gm20b_fb_tlb_invalidate returns a failure.
 * - Set the fb_mmu_ctrl_pri_fifo_space_v bit again, and set the intercept
 *   helper to write the fb_mmu_ctrl_pri_fifo_empty_v bit upon a write to
 *   fb_mmu_ctrl_r. Ensure that gm20b_fb_tlb_invalidate succeeds.
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int fb_gm20b_tlb_invalidate_test(struct unit_module *m, struct gk20a *g,
	void *args);

/**
 * Test specification for: fb_gm20b_mmu_ctrl_test
 *
 * Description: Test GM20B HALs targeting MMU features.
 *
 * Targets: gm20b_fb_mmu_ctrl, gm20b_fb_mmu_debug_ctrl, gm20b_fb_mmu_debug_wr,
 * gm20b_fb_mmu_debug_rd, gm20b_fb_vpr_info_fetch, gm20b_fb_dump_vpr_info,
 * gm20b_fb_dump_wpr_info, gm20b_fb_read_wpr_info, gops_fb.mmu_ctrl,
 * gops_fb.mmu_debug_wr, gops_fb.mmu_debug_ctrl, gops_fb.mmu_debug_rd,
 * gops_fb.vpr_info_fetch, gops_fb.dump_wpr_info, gops_fb.dump_vpr_info,
 * gops_fb.read_wpr_info
 *
 * Test Type: Feature, Error injection
 *
 * Input: None
 *
 * Steps:
 * - Set up the ops function pointer for all the HALs under test.
 * - Program an arbitrary value in the fb_mmu_ctrl_r register and ensure the
 *   gm20b_fb_mmu_ctrl HAL returns the same value.
 * - Program an arbitrary value in the fb_mmu_debug_ctrl_r register and ensure
 *   the gm20b_fb_mmu_debug_ctrl HAL returns the same value.
 * - Program an arbitrary value in the fb_mmu_debug_wr_r register and ensure the
 *   gm20b_fb_mmu_debug_wr HAL returns the same value.
 * - Program an arbitrary value in the fb_mmu_debug_rd_r register and ensure the
 *   gm20b_fb_mmu_debug_rd HAL returns the same value.
 * - Call the VPR/WPR dump operations for code coverage. Ensure that none of
 *   those operations cause a crash.
 * - Write in the fb_mmu_vpr_info register so that calling
 *   gm20b_fb_vpr_info_fetch triggers timeout in the
 *   gm20b_fb_vpr_info_fetch_wait function. Ensure the return values reflects
 *   a timeout.
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int fb_gm20b_mmu_ctrl_test(struct unit_module *m, struct gk20a *g, void *args);

/**
 * Test specification for: fb_mmu_fault_gv11b_init_test
 *
 * Description: Init test to setup HAL pointers for FB_MMU fault testing.
 *
 * Targets: gv11b_fb_read_mmu_fault_buffer_size,
 * gv11b_fb_read_mmu_fault_buffer_put, gv11b_fb_write_mmu_fault_status,
 * gv11b_fb_read_mmu_fault_buffer_get
 *
 * Test Type: Init
 *
 * Input: None
 *
 * Steps:
 * - Set up the ops function pointer for all the HALs under test.
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int fb_mmu_fault_gv11b_init_test(struct unit_module *m, struct gk20a *g,
	void *args);

/**
 * Test specification for: fb_mmu_fault_gv11b_buffer_test
 *
 * Description: Ensure all HAL functions work without causing an ABORT.
 *
 * Targets: gv11b_fb_is_fault_buf_enabled, gv11b_fb_fault_buffer_get_ptr_update,
 * gv11b_fb_write_mmu_fault_buffer_size, gv11b_fb_fault_buf_set_state_hw,
 * gv11b_fb_read_mmu_fault_status, gv11b_fb_fault_buf_configure_hw,
 * gv11b_fb_is_fault_buffer_empty, gv11b_fb_read_mmu_fault_addr_lo_hi,
 * gops_fb.fault_buf_configure_hw, gops_fb.fault_buf_set_state_hw,
 * gv11b_fb_fault_buffer_size_val, gv11b_fb_read_mmu_fault_inst_lo_hi,
 * gv11b_fb_read_mmu_fault_info
 *
 * Test Type: Feature, Error injection
 *
 * Input: fb_mmu_fault_gv11b_init_test
 *
 * Steps:
 * - Call gv11b_fb_fault_buffer_get_ptr_update.
 * - Set the overflow bit in the fb_mmu_fault_buffer_get_r(0) register, and call
 *   gv11b_fb_fault_buffer_get_ptr_update.
 * - Call gv11b_fb_fault_buffer_size_val and check that the fault buffer is
 *   empty.
 * - Call the gv11b_fb_fault_buf_configure_hw HAL and enable fault buffer.
 * - Enable fault buffer again which shouldn't cause any crash.
 * - Disable the fault buffer.
 * - Enable fault buffer, set the busy bit in fb_mmu_fault_status_r register,
 *   disable the fault buffer which should cause an internal timeout. Ensure
 *   that the fault buffer is disabled anyway.
 * - Write test values in the fb_mmu_fault_addr_lo_r / fb_mmu_fault_addr_hi_r
 *   registers, call gv11b_fb_read_mmu_fault_addr_lo_hi and ensure the
 *   returned values match the test values.
 * - Write test values in the fb_mmu_fault_inst_lo_r / fb_mmu_fault_inst_hi_r
 *   registers, call gv11b_fb_read_mmu_fault_inst_lo_hi and ensure the
 *   returned values match the test values.
 * - Call the gv11b_fb_read_mmu_fault_info HAL and ensure it returns the same
 *   value as in the fb_mmu_fault_info_r register.
 * - Call the gv11b_fb_write_mmu_fault_status HAL to write a test value, then
 *   read the fb_mmu_fault_status_r register to ensure it is the same value.
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int fb_mmu_fault_gv11b_buffer_test(struct unit_module *m, struct gk20a *g,
	void *args);

/**
 * Test specification for: fb_mmu_fault_gv11b_snap_reg
 *
 * Description: Test that gv11b_mm_copy_from_fault_snap_reg behaves correctly
 * if the reported fault is valid/invalid.
 *
 * Targets: gv11b_mm_copy_from_fault_snap_reg
 *
 * Test Type: Feature
 *
 * Input: fb_mmu_fault_gv11b_init_test
 *
 * Steps:
 * - Create a test mmu_fault_info instance.
 * - Call gv11b_mm_copy_from_fault_snap_reg with an invalid fault bit and
 *   ensure the chid of the mmu_fault_info was just set to a default value of 0.
 * - Call gv11b_mm_copy_from_fault_snap_reg again with a valid fault bit and
 *   ensure the chid of the mmu_fault_info is now set to
 *   NVGPU_INVALID_CHANNEL_ID.
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int fb_mmu_fault_gv11b_snap_reg(struct unit_module *m, struct gk20a *g,
	void *args);

/**
 * Test specification for: fb_mmu_fault_gv11b_handle_fault
 *
 * Description: Test the gv11b_fb_handle_mmu_fault HAL for all supported
 * interrupt statuses.
 *
 * Targets: gv11b_fb_handle_mmu_fault, gv11b_fb_fault_buf_set_state_hw
 *
 * Test Type: Feature
 *
 * Input: fb_mmu_fault_gv11b_init_test
 *
 * Steps:
 * - Call gv11b_fb_handle_mmu_fault with an interrupt source set to "other"
 *   and ensure it was handled by checking the "valid_clear" bit of the
 *   fb_mmu_fault_status_r register.
 * - Enable the fault buffer.
 * - Set interrupt source as dropped and ensure it is handled by
 *   gv11b_fb_handle_mmu_fault.
 * - Repeat with a source as non-replayable.
 * - Repeat with a source as non-replayable and overflow.
 * - Repeat with a source as overflow and corrupted getptr.
 * - Disable the fault buffer.
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int fb_mmu_fault_gv11b_handle_fault(struct unit_module *m, struct gk20a *g,
	void *args);

/**
 * Test specification for: fb_mmu_fault_gv11b_handle_bar2_fault
 *
 * Description: Test the gv11b_fb_handle_bar2_fault HAL for all supported
 * interrupt statuses.
 *
 * Targets: gv11b_fb_handle_bar2_fault, gv11b_fb_mmu_fault_info_dump,
 * gv11b_fb_fault_buf_set_state_hw
 *
 * Test Type: Feature, Error injection
 *
 * Input: fb_mmu_fault_gv11b_init_test
 *
 * Steps:
 * - Create zero'ed test instances of mmu_fault_info and nvgpu_channel.
 * - Call gv11b_fb_handle_bar2_fault with a fault_status of 0.
 * - Ensure the gv11b_fb_mmu_fault_info_dump HAL does not cause a crash when
 *   called with a NULL pointer or a zero'ed out mmu_fault_info structure.
 * - Set the minimum set of properties in the mmu_fault_info structure (valid
 *   and a pointer to the channel)
 * - Call the gv11b_fb_mmu_fault_info_dump and ensure it doesn't cause a crash.
 * - Set the fault_status to non-replayable and call gv11b_fb_handle_bar2_fault.
 * - Set the g->ops.bus.bar2_bind HAL to report a failure and call
 *   gv11b_fb_handle_bar2_fault again.
 * - Repeat with the fault buffer disabled.
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int fb_mmu_fault_gv11b_handle_bar2_fault(struct unit_module *m, struct gk20a *g,
	void *args);

/**
 * Test specification for: fb_intr_gv11b_init_test
 *
 * Description: Init test to setup HAL pointers for FB_INTR testing.
 *
 * Targets: None
 *
 * Test Type: Init
 *
 * Input: None
 *
 * Steps:
 * - Set up the ops function pointer for all the HALs under test.
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int fb_intr_gv11b_init_test(struct unit_module *m, struct gk20a *g, void *args);

/**
 * Test specification for: fb_intr_gv11b_isr_test
 *
 * Description: Test ISR handling with all supported types of interrupts.
 *
 * Targets: gv11b_fb_intr_enable, gv11b_fb_intr_disable, gv11b_fb_intr_isr,
 * gv11b_fb_intr_is_mmu_fault_pending, gops_fb_intr.is_mmu_fault_pending,
 * gops_fb_intr.enable, gops_fb_intr.disable, gops_fb_intr.isr
 *
 * Test Type: Feature
 *
 * Input: fb_intr_gv11b_init_test
 *
 * Steps:
 * - Mask all interrupts in the fb_niso_intr_en_set_r register.
 * - Call the gv11b_fb_intr_enable HAL and ensure several interrupts are
 *   unmasked.
 * - Set the fb_niso_intr_r register to 0 (no interrupt), and ensure that
 *   gv11b_fb_intr_is_mmu_fault_pending indicates that no fault is pending.
 * - Call the gv11b_fb_intr_isr HAL.
 * - Set interrupt source as "access counter notify/error" and call the
 *   gv11b_fb_intr_isr HAL (this will only cause a nvgpu_info call)
 * - Set interrupt source as "MMU fault" and ensure that
 *   gv11b_fb_intr_is_mmu_fault_pending indicates that a fault is pending.
 * - Set interrupt source as "ECC fault" and call the gv11b_fb_intr_isr HAL
 *   (further ECC testing is done in other tests).
 * - Use the gv11b_fb_intr_disable HAL to disable interrupts.
 * - Ensure that what was written in the clear register matches the interrupts
 *   that were enabled at the beginning of this test.
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int fb_intr_gv11b_isr_test(struct unit_module *m, struct gk20a *g, void *args);

/**
 * Test specification for: fb_intr_gv11b_ecc_test
 *
 * Description: Tests handling of ECC errors.
 *
 * Targets: gv11b_fb_ecc_init, gv11b_fb_intr_isr, gv11b_fb_intr_handle_ecc,
 * gv11b_fb_ecc_free
 *
 * Test Type: Feature
 *
 * Input: fb_intr_gv11b_init_test, args as a subcase with one of these values:
 * - TEST_ECC_L2TLB
 * - TEST_ECC_HUBTLB
 * - TEST_ECC_FILLUNIT
 *
 * Steps:
 * - Based on the subcase passed as an argument to this test, select the
 *   appropriate values for each HW unit:
 *   - Address of the status register
 *   - Address of the corrected error count register
 *   - Address of the uncorrected error count register
 *   - Expected status mask for corrected errors
 *   - Expected status mask for uncorrected errors
 *   - Expected status mask for corrected errors overflow
 *   - Expected status mask for uncorrected errors overflow
 * - Call the gv11b_fb_ecc_init HAL.
 * - Test the hanlding of ISRs in the following cases:
 *   - Corrected error
 *   - Uncorrected error
 *   - Corrected error and overflow (with >0 number of errors)
 *   - Uncorrected error and overflow (with >0 number of errors)
 *   - Corrected and uncorrected with overflow and 0 errors.
 * - In the case of FILLUNIT, also test the case of corrected and uncorrected
 *   PDE0 errors.
 * - Clear the interrupt status register.
 * - Call the gv11b_fb_ecc_free HAL.
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int fb_intr_gv11b_ecc_test(struct unit_module *m, struct gk20a *g, void *args);

/* Values below are used by the fb_intr_gv11b_ecc_test test. */
#define TEST_ECC_L2TLB		1U
#define TEST_ECC_HUBTLB		2U
#define TEST_ECC_FILLUNIT	3U

/* Helper function to intercept writes to the MMU status register. */
void helper_intercept_mmu_write(u32 val);

/** @} */
#endif /* UNIT_NVGPU_FB_H */
