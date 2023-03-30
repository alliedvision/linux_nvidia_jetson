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
#ifndef UNIT_NVGPU_GR_FALCON_H
#define UNIT_NVGPU_GR_FALCON_H

#include <nvgpu/types.h>

struct gk20a;
struct unit_module;

/** @addtogroup SWUTS-gr-falcon
 *  @{
 *
 * Software Unit Test Specification for common.gr.falcon
 */

/**
 * Test specification for: test_gr_falcon_init.
 *
 * Description: Helps to verify common.gr.falcon subunit initialization.
 *
 * Test Type: Feature, Error injection
 *
 * Targets: nvgpu_gr_falcon_init_support,
 *          nvgpu_gr_falcon_load_secure_ctxsw_ucode,
 *          gops_gr_falcon.load_ctxsw_ucode,
 *          gops_gr_falcon.get_fecs_ctx_state_store_major_rev_id,
 *          gm20b_gr_falcon_get_fecs_ctx_state_store_major_rev_id,
 *          gm20b_gr_falcon_get_gpccs_start_reg_offset,
 *          gm20b_gr_falcon_start_gpccs,
 *          gm20b_gr_falcon_fecs_base_addr,
 *          gm20b_gr_falcon_gpccs_base_addr
 *
 * Input: #test_gr_init_setup_ready must have been executed successfully.
 *
 * Steps:
 * -  Call #test_gr_init_setup_ready to setup the common.gr init.
 * -  Stub some falcon hals
 *    - g->ops.gr.falcon.load_ctxsw_ucode.
 *    - g->ops.gr.falcon.load_ctxsw_ucode_header.
 *    - g->ops.gr.falcon.bind_instblk.
 * -  Call #nvgpu_gr_falcon_init_support and fail memory allocation.
 * -  Call #nvgpu_gr_falcon_init_support and pass memory allocation.
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_gr_falcon_init(struct unit_module *m,
			struct gk20a *g, void *args);

/**
 * Test specification for: test_gr_falcon_deinit.
 *
 * Description: Helps to verify common.gr.falcon subunit deinitialization.
 *
 * Test Type: Feature, Error injection
 *
 * Targets: nvgpu_gr_falcon_remove_support
 *
 * Input: #test_gr_falcon_init must have been executed successfully.
 *
 * Steps:
 * -  Call #nvgpu_gr_falcon_remove_support.
 * -  Call #nvgpu_gr_falcon_remove_support will NULL pointer.
 * -  Call #test_gr_init_setup_cleanup to cleanup common.gr.
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_gr_falcon_deinit(struct unit_module *m,
				struct gk20a *g, void *args);

/**
 * Test specification for: test_gr_falcon_init_ctxsw.
 *
 * Description: This test helps to verify load and boot FECS and GPCCS ucodes.
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_gr_falcon_init_ctxsw,
 *          gm20b_gr_falcon_bind_instblk,
 *          nvgpu_gr_checksum_u32
 *
 * Input: #test_gr_falcon_init must have been executed successfully.
 *
 * Steps:
 * -  By default code use secure gpccs path.
 * -  Call #nvgpu_gr_falcon_init_ctxsw.
 * -  Call #nvgpu_gr_falcon_init_ctxsw to test recovery path failure.
 * -  Call #nvgpu_gr_falcon_init_ctxsw to test recovery path success.
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_gr_falcon_init_ctxsw(struct unit_module *m,
			       struct gk20a *g, void *args);

/**
 * Test specification for: test_gr_falcon_init_ctx_state.
 *
 * Description: Helps to verify context state initialization
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_gr_falcon_init_ctx_state,
 *          gm20b_gr_falcon_init_ctx_state,
 *          gp10b_gr_falcon_init_ctx_state
 *
 * Input: #test_gr_falcon_init must have been executed successfully.
 *
 * Steps:
 * -  Call #nvgpu_gr_falcon_init_ctx_state.
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_gr_falcon_init_ctx_state(struct unit_module *m,
			       struct gk20a *g, void *args);

/**
 * Test specification for: test_gr_falcon_query_test.
 *
 * Description: Helps to verify the common.gr.falcon query
 *              functions return valid values.
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_gr_falcon_get_fecs_ucode_segments,
 *          nvgpu_gr_falcon_get_gpccs_ucode_segments,
 *          nvgpu_gr_falcon_get_surface_desc_cpu_va
 *
 * Input: #test_gr_falcon_init must have been executed successfully.
 *
 * Steps:
 * -  Call #nvgpu_gr_falcon_get_fecs_ucode_segments.
 * -  Call #nvgpu_gr_falcon_get_gpccs_ucode_segments.
 * -  Call #nvgpu_gr_falcon_get_surface_desc_cpu_va.
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_gr_falcon_query_test(struct unit_module *m,
				      struct gk20a *g, void *args);

/**
 * Test specification for: test_gr_falcon_fail_ctxsw_ucode.
 *
 * Description: Helps to verify the allocation failures for
 *              nvgpu_gr_falcon_init_ctxsw_ucode function is handled properly.
 *
 * Test Type: Error injection
 *
 * Targets: nvgpu_gr_falcon_init_ctxsw_ucode,
 *          gops_gr_falcon.load_ctxsw_ucode
 *
 * Input: #test_gr_falcon_init must have been executed successfully.
 *
 * Steps:
 * -  Request Kmemory and dma allocation failures at various locations.
 * -  Call #nvgpu_gr_falcon_init_ctxsw_ucode.
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_gr_falcon_fail_ctxsw_ucode(struct unit_module *m,
				      struct gk20a *g, void *args);
#endif /* UNIT_NVGPU_GR_FALCON_H */

/**
 * @}
 */

