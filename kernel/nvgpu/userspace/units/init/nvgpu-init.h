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

#ifndef UNIT_NVGPU_INIT_H
#define UNIT_NVGPU_INIT_H

struct gk20a;
struct unit_module;

/** @addtogroup SWUTS-init
 *  @{
 *
 * Software Unit Test Specification for nvgpu.common.init
 */

/**
 * Test specification for: init_test_setup_env
 *
 * Description: Do basic setup before starting other tests.
 *
 * Test Type: Other (setup)
 *
 * Input: None
 *
 * Steps:
 * - Initialize reg spaces used by init unit tests.
 *
 * Output:
 * - UNIT_FAIL if encounters an error creating reg space
 * - UNIT_SUCCESS otherwise
 */
int init_test_setup_env(struct unit_module *m,
			  struct gk20a *g, void *args);

/**
 * Test specification for: init_test_free_env
 *
 * Description: Cleanup resources allocated in init_test_setup_env()
 *
 * Test Type: Other (setup)
 *
 * Input: None
 *
 * Steps:
 * - Delete reg spaces
 *
 * Output:
 * - UNIT_SUCCESS always
 */
int init_test_free_env(struct unit_module *m,
			 struct gk20a *g, void *args);

/**
 * Test specification for: test_get_litter_value
 *
 * Description: Validate gv11b_get_litter_value()
 *
 * Test Type: Feature
 *
 * Targets: gv11b_get_litter_value
 *
 * Input: None
 *
 * Steps:
 *   - Call gv11b_get_litter_value() with all valid values and verify correct
 *     return value.
 *   - Call gv11b_get_litter_value() with invalid value and verify BUG().
 *
 * Output:
 * - UNIT_FAIL if nvgpu_can_busy() returns the incorrect value.
 * - UNIT_SUCCESS otherwise
 */
int test_get_litter_value(struct unit_module *m,
			  struct gk20a *g, void *args);

/**
 * Test specification for: test_can_busy
 *
 * Description: Validate nvgpu_can_busy()
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_can_busy
 *
 * Input: None
 *
 * Steps:
 * - Vary NVGPU_KERNEL_IS_DYING & NVGPU_DRIVER_IS_DYING enable values and
 *   verify the result from nvgpu_can_busy()
 *
 *
 * Output:
 * - UNIT_FAIL if nvgpu_can_busy() returns the incorrect value.
 * - UNIT_SUCCESS otherwise
 */
int test_can_busy(struct unit_module *m,
			 struct gk20a *g, void *args);

/**
 * Test specification for: test_get_put
 *
 * Description: Validate nvgpu_get() and nvgpu_put() and the refcount.
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_get, nvgpu_put
 *
 * Input:
 * - init_test_setup_env() must be called before.
 *
 * Steps:
 * - Initialize refcount.
 * - Get gpu and validate return and refcount.
 * - Put gpu and validate refcount.
 * - Put gpu again to initiate teardown and validate refcount.
 * - Get gpu again to verify failure return and validate refcount.
 * - Re-Initialize refcount.
 * - Set function pointers to NULL to test different paths/branches.
 * - Get gpu and validate return and refcount.
 * - Put gpu and validate refcount.
 * - Put gpu again to initiate teardown and validate refcount.
 *
 * Output:
 * - UNIT_FAIL if nvgpu_get() returns the incorrect value or refcount is
 *   incorrect
 * - UNIT_SUCCESS otherwise
 */
int test_get_put(struct unit_module *m,
			struct gk20a *g, void *args);

/**
 * Test specification for: test_check_gpu_state
 *
 * Description: Validate the nvgpu_check_gpu_state() API which will restart
 *
 * Test Type: Feature
 *
 * Input:
 * - init_test_setup_env() must be called before.
 *
 * Targets: nvgpu_check_gpu_state, is_nvgpu_gpu_state_valid,
 *          gops_mc.get_chip_details
 *
 * Steps:
 * - Test valid case.
 *   - Set the mc_boot_0 reg to a valid state.
 *   - Call nvgpu_check_gpu_state() and the call should return normally.
 * - Test invalid case.
 *   - Set the mc_boot_0 reg to the invalid state.
 *   - Call nvgpu_check_gpu_state() and trap the BUG() call.
 *
 * Output:
 * - UNIT_FAIL if nvgpu_check_gpu_state() does not cause a BUG() for the invalid
 *   case
 * - If the valid case fails, BUG() may occur and cause the framework to stop
 *   the test.
 * - UNIT_SUCCESS otherwise
 */
int test_check_gpu_state(struct unit_module *m,
				struct gk20a *g, void *args);

/**
 * Test specification for: test_hal_init
 *
 * Description: Test HAL initialization for GV11B
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_detect_chip
 *
 * Input:
 * - init_test_setup_env() must be called before.
 *
 * Steps:
 * - Nominal test
 *   - Setup the mc_boot_0 reg for GV11B.
 *   - Initialize the fuse regs.
 *   - Init the HAL and verify successful return.
 * - Branch test (re-init HAL)
 *   - Init the HAL again and verify successful return.
 * - Branch test (GPU version a01 check)
 *   - Clear HAL inited flag in gk20a.
 *   - Set Posix flag to make device version a01.
 *   - Init the HAL and verify successful return.
 *   - Clear Posix a01 version flag.
 * - Negative test (security fuse)
 *   - Clear HAL inited flag in gk20a.
 *   - Initialize the fuse regs for secure mode.
 *   - Init the HAL and verify failure return.
 *   - Reset the fuse regs for non-secure mode.
 * - Negative test (invalid GPU versions)
 *   - Loop setting invalid GPU versions.
 *     - Init the HAL and verify failure return.
 *
 * Output:
 * - UNIT_FAIL if HAL initialization fails
 * - UNIT_SUCCESS otherwise
 */
int test_hal_init(struct unit_module *m,
			 struct gk20a *g, void *args);

/**
 * Test specification for: test_poweron
 *
 * Description: Test nvgpu_finalize_poweron
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_finalize_poweron, nvgpu_init_gpu_characteristics
 *
 * Input:
 * - init_test_setup_env() must be called before.
 *
 * Steps:
 * 1) Setup poweron init function pointers.
 * 2) Call nvgpu_finalize_poweron().
 * 3) Check return status.
 * - These 3 basic steps are repeated:
 *   a) For the case where all units return success.
 *   b) Once each for individual unit returning failure.
 * - Lastly, it verifies the case where the the deviceis already powered on.
 *
 * Output:
 * - UNIT_FAIL if nvgpu_finalize_poweron() ever returns the unexpected value.
 * - UNIT_SUCCESS otherwise
 */
int test_poweron(struct unit_module *m, struct gk20a *g, void *args);

/**
 * Test specification for: test_poweron_branches
 *
 * Description: Test branches in nvgpu_finalize_poweron not covered by the
 * basic path already covered in test_poweron.
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_finalize_poweron
 *
 * Input:
 * - init_test_setup_env() must be called before.
 *
 * Steps:
 * 1) Setup poweron init function pointers to NULL and enable flags.
 * 2) Call nvgpu_finalize_poweron().
 * 3) Check return status.
 * 4) Test syncpt handling by enabling syncpts, altering syncpt flags, and
 *    manipluatin mem calls to cover other paths in the syncpt init.
 *
 * Output:
 * - UNIT_FAIL if nvgpu_finalize_poweron() ever returns the unexpected value.
 * - UNIT_SUCCESS otherwise
 */
int test_poweron_branches(struct unit_module *m, struct gk20a *g, void *args);

/**
 * Test specification for: test_poweroff
 *
 * Description: Test nvgpu_prepare_poweroff
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_prepare_poweroff
 *
 * Input:
 * - init_test_setup_env() must be called before.
 *
 * Steps:
 * 1) Setup poweroff init function pointers.
 * 2) Call nvgpu_finalize_poweron().
 * 3) Check return status.
 * - These 3 basic steps are repeated:
 *   a) For the case where all units return success.
 *   b) Once each for individual unit returning failure.
 *   b) To complete branch coverage, with appropriate function poiners set to
 *      NULL.
 *
 * Output:
 * - UNIT_FAIL if nvgpu_finalize_poweron() ever returns the unexpected value.
 * - UNIT_SUCCESS otherwise
 */
int test_poweroff(struct unit_module *m, struct gk20a *g, void *args);

/**
 * Test specification for: test_quiesce
 *
 * Description: Test putting device in quiesce
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_sw_quiesce_init_support, nvgpu_sw_quiesce_remove_support,
 *          nvgpu_sw_quiesce_thread, nvgpu_sw_quiesce, nvgpu_sw_quiesce_bug_cb,
 *          nvgpu_bug_exit
 *
 * Input:
 * - init_test_setup_env() must be called before.
 *
 * Steps:
 * - Use stub for g->ops.mc.intr_mask, g->ops.runlist.write_state and
 *   g->ops.fifo.preempt_runlists_for_rc.
 * - Call nvgpu_sw_quiesce, wait for SW quiesce threads to complete,
 *   and check that interrupts have been disabled.
 * - Check SW quiesce invoked from BUG().
 * - Check cases where nvgpu_sw_quiesce does not wake up threads:
 *   - NVGPU_DISABLE_SW_QUIESCE is set.
 *   - g->sw_quiesce_pending is already true.
 *   - g->sw_quiesce_init_done is false.
 * - Check cases where nvgpu_sw_quiesce_thread skips quiescing:
 *   - nvgpu_thread_should_stop is true (using fault injection).
 *   - g->is_virtual is true.
 *   - g->powered_on is false.
 * - Check failure cases in nvgpu_sw_quiesce_init_support:
 *   - sw_quiesce_cond initialization failure (using cond fault injection).
 *   - sw_quiesce already initialized.
 *   - sw_quiesce_thread creation failure (using thread fault injection).
 *   - sw_quiesce_wdog creation failure (using thread fault injection).
 *
 * Output:
 * - UNIT_FAIL if SW quiesce did not behave as expected.
 * - UNIT_SUCCESS otherwise
 */
int test_quiesce(struct unit_module *m, struct gk20a *g, void *args);

#endif /* UNIT_NVGPU_INIT_H */
