/*
 * Copyright (c) 2018-2021, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef __UNIT_NVGPU_FUSE_GM20B_H__
#define __UNIT_NVGPU_FUSE_GM20B_H__

struct gk20a;
struct unit_module;

/** @addtogroup SWUTS-fuse
 *  @{
 */

extern struct fuse_test_args gm20b_init_args;

/**
 * Test specification for: test_fuse_gm20b_check_sec
 *
 * Description: Verify fuse API check_priv_security() when security fuse is
 *              enabled.
 *
 * Test Type: Feature
 *
 * Targets: gops_fuse.check_priv_security, gm20b_fuse_check_priv_security
 *
 * Input: test_fuse_device_common_init() must be called for this GPU.
 *
 * Steps:
 * - Setup the security regs appropriately.
 * - Call the fuse API check_priv_security().
 * - Verify Security flags are enabled/disabled correctly.
 * - Repeat above steps for ACR enabled and disabled.
 *
 * Output: Returns SUCCESS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_fuse_gm20b_check_sec(struct unit_module *m,
			      struct gk20a *g, void *__args);

/**
 * Test specification for: test_fuse_gm20b_check_gcplex_fail
 *
 * Description: Verify fuse API check_priv_security() handles an error from
 *              reading gcplex.
 *
 * Test Type: Feature
 *
 * Targets: gops_fuse.check_priv_security, gm20b_fuse_check_priv_security
 *
 * Input: test_fuse_device_common_init() must be called for this GPU.
 *
 * Steps:
 * - Override HAL for reading gcplex so it returns an error.
 * - Call the fuse API check_priv_security(), which will read gcplex, and verify
 *   an error is returned.
 *
 * Output: Returns SUCCESS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_fuse_gm20b_check_gcplex_fail(struct unit_module *m,
			 struct gk20a *g, void *__args);

/**
 * Test specification for: test_fuse_gm20b_check_sec_invalid_gcplex
 *
 * Description: Verify fuse API check_priv_security() handles invalid gcplex
 *              configurations of WPR and VPR bits.
 *
 * Test Type: Feature
 *
 * Targets: gops_fuse.check_priv_security, gm20b_fuse_check_priv_security
 *
 * Input: test_fuse_device_common_init() must be called for this GPU.
 *
 * Steps:
 * - Override HAL for reading gcplex so the WPR/VPR configuration can be
 *   overwritten.
 * - Enable Security fuse.
 * - Write an invalid WPR/VPR configuration into the gcplex override by using
 *   the overridden HAL.
 * - Call the fuse API check_priv_security() and verify an error is returned.
 * - Repeat the previous 2 steps for all invalid combinations of WPR/VPR
 *   configurations.
 *
 * Output: Returns SUCCESS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_fuse_gm20b_check_sec_invalid_gcplex(struct unit_module *m,
					     struct gk20a *g, void *__args);

/**
 * Test specification for: test_fuse_gm20b_check_non_sec
 *
 * Description:  Verify fuse API check_priv_security() when security fuse is
 *               disabled.
 *
 * Test Type: Feature
 *
 * Targets: gops_fuse.check_priv_security, gm20b_fuse_check_priv_security
 *
 * Input: test_fuse_device_common_init() must be called for this GPU.
 *
 * Steps:
 * - Disable Security fuse.
 * - Call the fuse API check_priv_security().
 * - Verify correct security flags are disabled.
 *
 * Output: Returns SUCCESS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_fuse_gm20b_check_non_sec(struct unit_module *m,
				  struct gk20a *g, void *__args);

/**
 * Test specification for: test_fuse_gm20b_basic_fuses
 *
 * Description:  Verify fuse reads for basic value-return APIs.
 *
 * Test Type: Feature
 *
 * Targets: gops_fuse.fuse_status_opt_fbio, gops_fuse.fuse_status_opt_fbp,
 *          gops_fuse.fuse_status_opt_l2_fbp, gops_fuse.fuse_status_opt_tpc_gpc,
 *          gops_fuse.fuse_opt_sec_debug_en, gops_fuse.fuse_opt_priv_sec_en,
 *          gops_fuse.fuse_ctrl_opt_tpc_gpc, gm20b_fuse_status_opt_fbio,
 *          gm20b_fuse_status_opt_fbp, gm20b_fuse_status_opt_l2_fbp,
 *          gm20b_fuse_status_opt_tpc_gpc, gm20b_fuse_opt_sec_debug_en,
 *          gm20b_fuse_opt_priv_sec_en, gm20b_fuse_ctrl_opt_tpc_gpc
 *
 * Input: test_fuse_device_common_init() must be called for this GPU.
 *
 * Steps:
 * - For each fuse API that returns the value of the fuse, do the following:
 *   - Write valid values to the fuse register in the mock IO.
 *   - Call the API to read fuse.
 *   - Verify the correct value is returned.
 *
 * Output: Returns SUCCESS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_fuse_gm20b_basic_fuses(struct unit_module *m,
				struct gk20a *g, void *__args);

/**
 * Test specification for: test_fuse_gm20b_basic_fuses_bvec
 *
 * Description:  Verify fuse reads for basic value-return APIs.
 *
 * Test Type: BVEC
 *
 * Targets: gops_fuse.fuse_status_opt_tpc_gpc,
 *
 * Equivalence classes:
 * - Valid : {0, gr->config->max_gpc_count - 1}
 * - Invalid : {gr->config->max_gpc_count, U32_MAX}
 *
 * Input: test_fuse_device_common_init() must be called for this GPU.
 *
 * Steps:
 * - For each fuse API that returns the value of the fuse, do the following:
 *   - Read values for valid/invalid GPCs.
 *   - Verify the correct value/error is returned.
 *
 * Output: Returns SUCCESS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_fuse_gm20b_basic_fuses_bvec(struct unit_module *m,
				struct gk20a *g, void *__args);
#ifdef CONFIG_NVGPU_SIM
int test_fuse_gm20b_check_fmodel(struct unit_module *m,
				 struct gk20a *g, void *__args);
#endif
#endif /* __UNIT_NVGPU_FUSE_GM20B_H__ */
