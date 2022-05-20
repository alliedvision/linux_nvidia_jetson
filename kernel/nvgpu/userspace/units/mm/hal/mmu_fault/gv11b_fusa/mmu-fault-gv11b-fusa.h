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

#ifndef UNIT_MM_HAL_MMU_FAULT_GV11B_FUSA_H
#define UNIT_MM_HAL_MMU_FAULT_GV11B_FUSA_H

struct gk20a;
struct unit_module;

/** @addtogroup SWUTS-mm-hal-mmu_fault-gv11b_fusa
 *  @{
 *
 * Software Unit Test Specification for mm.hal.mmu_fault.mmu_fault_gv11b_fusa
 */

/**
 * Test specification for: test_env_init_mm_mmu_fault_gv11b_fusa
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
int test_env_init_mm_mmu_fault_gv11b_fusa(struct unit_module *m,
					struct gk20a *g, void *args);

/**
 * Test specification for: test_gv11b_mm_mmu_fault_setup_sw
 *
 * Description: Test mmu fault setup sw function
 *
 * Test Type: Feature, Error injection
 *
 * Targets: gops_mm_mmu_fault.setup_sw, gv11b_mm_mmu_fault_setup_sw,
 * gops_mm_mmu_fault.info_mem_destroy,
 * gv11b_mm_mmu_fault_info_mem_destroy
 *
 * Input: test_env_init
 *
 * Steps:
 * - Check that mmu hw fault buffer is allocated and mapped.
 * - Check that gv11b_mm_mmu_fault_info_mem_destroy() deallocates fault buffer
 *   memory.
 *
 * Output: Returns SUCCESS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_gv11b_mm_mmu_fault_setup_sw(struct unit_module *m, struct gk20a *g,
					void *args);

/**
 * Test specification for:
 *
 * Description: Test mmu fault setup hw function
 *
 * Test Type: Feature
 *
 * Targets: gops_mm_mmu_fault.setup_hw, gv11b_mm_mmu_fault_setup_hw
 *
 * Input: test_env_init
 *
 * Steps:
 * - Check that gv11b_mm_mmu_fault_setup_hw() configures fault buffer. Here,
 *   buffer addr is written to memory to be used by h/w for fault notification.
 *
 * Output: Returns SUCCESS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_gv11b_mm_mmu_fault_setup_hw(struct unit_module *m, struct gk20a *g,
					void *args);

/**
 * Test specification for: test_gv11b_mm_mmu_fault_disable_hw
 *
 * Description: Test mmu fault disable hw function
 *
 * Test Type: Feature
 *
 * Targets: gops_mm_mmu_fault.disable_hw, gv11b_mm_mmu_fault_disable_hw
 *
 * Input: test_env_init
 *
 * Steps:
 * - Check that gv11b_mm_mmu_fault_disable_hw() sets disabled state if fault
 *   buf is enabled.
 *
 * Output: Returns SUCCESS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_gv11b_mm_mmu_fault_disable_hw(struct unit_module *m, struct gk20a *g,
					void *args);

/**
 * Test specification for: test_gv11b_mm_mmu_fault_handle_other_fault_notify
 *
 * Description: Test other fault notify
 *
 * Test Type: Feature
 *
 * Targets: gv11b_mm_mmu_fault_handle_other_fault_notify
 *
 * Input: test_env_init
 *
 * Steps:
 * - Check that BAR2 / physical faults are recognized and notified.
 *
 * Output: Returns SUCCESS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_gv11b_mm_mmu_fault_handle_other_fault_notify(struct unit_module *m,
					struct gk20a *g, void *args);

/**
 * Test specification for: test_gv11b_mm_mmu_fault_parse_mmu_fault_info
 *
 * Description: Test mmu fault parse function
 *
 * Test Type: Feature
 *
 * Targets: gv11b_mm_mmu_fault_parse_mmu_fault_info
 *
 * Input: test_env_init
 *
 * Steps:
 * - Parse mmu fault info such as fault type, client type and client id.
 *
 * Output: Returns SUCCESS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_gv11b_mm_mmu_fault_parse_mmu_fault_info(struct unit_module *m,
					struct gk20a *g, void *args);

/**
 * Test specification for: test_handle_mmu_fault_common
 *
 * Description: Test mmu fault handler
 *
 * Test Type: Feature
 *
 * Targets: gv11b_mm_mmu_fault_handle_mmu_fault_common,
 *          gv11b_mm_mmu_fault_handle_mmu_fault_ce,
 *          gv11b_mm_mmu_fault_handle_non_replayable,
 *          gv11b_mm_mmu_fault_handle_mmu_fault_refch
 *
 * Input: test_env_init
 *
 * Steps:
 * - Check that fault handler processes valid and invalid cases of mmu fault.
 *
 * Output: Returns SUCCESS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_handle_mmu_fault_common(struct unit_module *m,
					struct gk20a *g, void *args);

/**
 * Test specification for:
 *
 * Description: Test non-replayable replayable fault handler
 *
 * Test Type: Feature
 *
 * Targets: gv11b_mm_mmu_fault_handle_nonreplay_replay_fault,
 *          gv11b_mm_mmu_fault_handle_buf_valid_entry,
 *          gv11b_fb_copy_from_hw_fault_buf
 *
 * Input: test_env_init
 *
 * Steps:
 * - Test non-replayable fault handler with valid and invalid cases.
 *
 * Output: Returns SUCCESS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_handle_nonreplay_replay_fault(struct unit_module *m, struct gk20a *g,
					void *args);

/**
 * Test specification for: test_env_clean_mm_mmu_fault_gv11b_fusa
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
int test_env_clean_mm_mmu_fault_gv11b_fusa(struct unit_module *m,
					struct gk20a *g, void *args);

/** @} */
#endif /* UNIT_MM_HAL_MMU_FAULT_GV11B_FUSA_H */
