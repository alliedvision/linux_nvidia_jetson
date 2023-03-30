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

#ifndef UNIT_PAGE_TABLE_FAULTS_H
#define UNIT_PAGE_TABLE_FAULTS_H

struct gk20a;
struct unit_module;

/** @addtogroup SWUTS-mm-page_table_faults
 *  @{
 *
 * Software Unit Test Specification for mm.page_table_faults
 */

/**
 * Test specification for: test_page_faults_init
 *
 * Description: This test must be run once and be the first one as it
 * initializes the MM subsystem.
 *
 * Test Type: Feature, Other (setup)
 *
 * Targets: nvgpu_vm_init
 *
 * Input: None
 *
 * Steps:
 * - Initialize the enabled flag NVGPU_MM_UNIFIED_MEMORY.
 * - Allocate a test buffer to be used as VIDMEM.
 * - Set all needed MM-related HALs.
 * - Register the FB_MMU test IO space.
 * - Ensure that MM HAL indicates that BAR1 is not supported.
 * - Create a test VM with big pages enabled.
 * - Create a VM for BAR2 space
 * - Call the HAL to initialize fault reporting hardware and ensure it
 *   succeeded.
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_page_faults_init(struct unit_module *m, struct gk20a *g, void *__args);

/**
 * Test specification for: test_page_faults_pending
 *
 * Description: Check that no faults are already pending, then add one and check
 * that it is pending.
 *
 * Test Type: Feature
 *
 * Targets: gops_mc.is_mmu_fault_pending, gv11b_mc_is_mmu_fault_pending
 *
 * Input: test_page_faults_init
 *
 * Steps:
 * - Call the ops.mc.is_mmu_fault_pending HAL and ensure it returns that no
 *   faults are pending.
 * - Manually write an error in the status register.
 * - Call the ops.mc.is_mmu_fault_pending HAL again and ensure it returns that
 *   a fault is pending.
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_page_faults_pending(struct unit_module *m, struct gk20a *g,
	void *args);

/**
 * Test specification for: test_page_faults_disable_hw
 *
 * Description: Test the fault_disable_hw mechanism.
 *
 * Test Type: Feature
 *
 * Targets: gops_mm_mmu_fault.disable_hw, gv11b_mm_mmu_fault_disable_hw,
 * gops_fb.is_fault_buf_enabled, gv11b_fb_is_fault_buf_enabled
 *
 * Input: test_page_faults_init
 *
 * Steps:
 * - Call the ops.mm.mmu_fault.disable_hw HAL.
 * - Using the g->ops.fb.is_fault_buf_enabled HAL, ensure that both
 *   NVGPU_MMU_FAULT_NONREPLAY_REG_INDX and NVGPU_MMU_FAULT_REPLAY_REG_INDX
 *   fault buffers are disabled.
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_page_faults_disable_hw(struct unit_module *m, struct gk20a *g,
	void *args);

/**
 * Test specification for: test_page_faults_inst_block
 *
 * Description: This test supports 3 types of scenario to cover corner cases:
 * - 0 (default): regular nvgpu_alloc_inst_block with default values
 * - 1: nvgpu_alloc_inst_block with large page size
 * - 2: nvgpu_alloc_inst_block with large page size and set_big_page_size set to
 *      NULL to test a corner case in gv11b_init_inst_block (branch coverage)
 *
 * Test Type: Feature
 *
<<<<<<< HEAD
 * Targets: gops_mm.gops_mm_gmmu.get_default_big_page_size,
=======
 * Targets: gops_mm_gmmu.get_default_big_page_size,
>>>>>>> 2769ccf4e... gpu: nvgpu: userspace: update "Targets" field for mm
 * nvgpu_gmmu_default_big_page_size, nvgpu_alloc_inst_block,
 * gops_mm.init_inst_block, gv11b_mm_init_inst_block
 *
 * Input: test_page_faults_init
 *
 * Steps:
 * - Instantiate a nvgpu_mem instance.
 * - If running scenario 1 or 2, retrieve the default big page size.
 * - Use nvgpu_alloc_inst_block on the nvgpu_mem instance to allocate the
 *   inst_block.
 * - Call the ops.mm.init_inst_block HAL to initialize the inst_block with
 *   big page size if needed.
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_page_faults_inst_block(struct unit_module *m, struct gk20a *g,
					void *args);

/**
 * Test specification for: test_page_faults_clean
 *
 * Description: This test should be the last one to run as it de-initializes
 * components.
 *
 * Test Type: De-init
 *
 * Targets: gops_mm_mmu_fault.info_mem_destroy,
 * gv11b_mm_mmu_fault_info_mem_destroy, nvgpu_vm_put
 *
 * Input: test_page_faults_init
 *
 * Steps:
 * - Call the ops.mm.mmu_fault.info_mem_destroy HAL
 * - De-initialize the test system VM.
 * - De-initialize the BAR2 VM.
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_page_faults_clean(struct unit_module *m, struct gk20a *g, void *args);
/** }@ */
#endif /* UNIT_PAGE_TABLE_FAULTS_H */
