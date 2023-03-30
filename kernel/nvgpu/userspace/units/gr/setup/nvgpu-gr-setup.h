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
#ifndef UNIT_NVGPU_GR_SETUP_H
#define UNIT_NVGPU_GR_SETUP_H

#include <nvgpu/types.h>

struct gk20a;
struct unit_module;

/** @addtogroup SWUTS-gr-setup
 *  @{
 *
 * Software Unit Test Specification for common.gr.setup
 */

/**
 * Test specification for: test_gr_setup_alloc_obj_ctx.
 *
 * Description: This test helps to verify common.gr object context creation.
 *
 * Test Type: Feature, Boundary Value
 *
 * Equivalence classes:
 * Variable: class_num
 * - Valid : {0 - U32_MAX}
 *   	Range of "class_num" variable for nvgpu-rm is
 *      0xC3C0U (VOLTA_COMPUTE_A), 0xC3B5U (VOLTA_DMA_COPY_A),
 *      0xC36FU (VOLTA_CHANNEL_GPFIFO_A).
 *      class_num range check is done in common.class unit.
 * Variable: flags
 * - Valid : {0 - U32_MAX}
 *
 * Targets: nvgpu_gr_setup_alloc_obj_ctx,
 *          nvgpu_gr_obj_ctx_alloc,
 *          nvgpu_gr_ctx_get_ctx_mem,
 *          nvgpu_gr_ctx_set_tsgid,
 *          nvgpu_gr_ctx_get_tsgid,
 *          nvgpu_gr_ctx_get_global_ctx_va,
 *          gops_gr_setup.alloc_obj_ctx,
 *          nvgpu_gr_ctx_load_golden_ctx_image,
 *          gm20b_ctxsw_prog_set_patch_addr,
 *          gv11b_gr_init_commit_global_attrib_cb,
 *          gm20b_gr_init_commit_global_attrib_cb,
 *          gv11b_gr_init_commit_global_timeslice,
 *          gv11b_gr_init_restore_stats_counter_bundle_data,
 *          gv11b_gr_init_commit_cbes_reserve,
 *          gv11b_gr_init_fe_go_idle_timeout,
 *          gm20b_gr_init_override_context_reset,
 *          gm20b_gr_init_pipe_mode_override,
 *          gp10b_gr_init_commit_global_bundle_cb,
 *          gm20b_gr_falcon_set_current_ctx_invalid,
 *          gm20b_gr_falcon_get_fecs_current_ctx_data
 *
 * Input: #test_gr_init_setup_ready must have been executed successfully.
 *
 * Steps:
 * -  Use stub functions for hals that use timeout and requires register update
 *    within timeout loop.
 *    - g->ops.mm.cache.l2_flush.
 *    - g->ops.gr.init.fe_pwr_mode_force_on.
 *    - g->ops.gr.init.wait_idle.
 *    - g->ops.gr.falcon.ctrl_ctxsw.
 * -  Set default golden image size.
 * -  Allocate and bind channel and tsg.
 * -  Start BVEC testing for variable class_num.
 *    class_num is tested for range in common.class. In common.gr, stub out
 *    the common.class HALs to perform independent range testing. Before
 *    stubbing, save the valid initialization values for common.class HALs.
 * -  Call g->ops.gr.setup.alloc_obj_ctx with input class_num at boundary
 *    values - min boundary(0), max boundary(U32_MAX) and once with value
 *    in valid range. g->ops.gr.setup.alloc_obj_ctx value should return
 *    0 as all class_num values are valid from common.gr perspective.
 *    End BVEC testing for variable class_num by restoring the stubbed
 *    common.class HALs.
 * -  Start BVEC testing for variable flags.
 * -  Call g->ops.gr.setup.alloc_obj_ctx with input variable flags at boundary
 *    values - min boundary(0), max boundary(U32_MAX) and once with value
 *    in valid range. g->ops.gr.setup.alloc_obj_ctx value should return
 *    0 as all flags values are valid from common.gr perspective.
 *    End BVEC testing for variable flags.
 * -  Call g->ops.gr.setup.alloc_obj_ctx with valid class_num -
 *    VOLTA_DMA_COPY_A and VOLTA_COMPUTE_A.
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_gr_setup_alloc_obj_ctx(struct unit_module *m,
				 struct gk20a *g, void *args);

/**
 * Test specification for: test_gr_setup_set_preemption_mode.
 *
 * Description: This test helps to verify set_preemption_mode.
 *
 * Test Type: Feature, Safety
 *
 * Targets: nvgpu_gr_setup_set_preemption_mode,
 *          nvgpu_gr_obj_ctx_set_ctxsw_preemption_mode,
 *          nvgpu_gr_ctx_check_valid_preemption_mode,
 *          nvgpu_gr_obj_ctx_update_ctxsw_preemption_mode,
 *          nvgpu_gr_ctx_get_compute_preemption_mode,
 *          nvgpu_gr_ctx_set_preemption_modes,
 *          nvgpu_gr_ctx_patch_write_begin,
 *          nvgpu_gr_ctx_patch_write_end,
 *          gp10b_gr_init_commit_global_cb_manager,
 *          nvgpu_gr_ctx_patch_write,
 *          gm20b_ctxsw_prog_get_patch_count,
 *          gm20b_ctxsw_prog_set_patch_count,
 *          gops_gr_init.get_default_preemption_modes,
 *          gp10b_gr_init_get_default_preemption_modes,
 *          gops_gr_setup.set_preemption_mode,
 *          gp10b_ctxsw_prog_set_compute_preemption_mode_cta,
 *          gops_gr_init.get_supported__preemption_modes,
 *          gp10b_gr_init_get_supported_preemption_modes
 *
 * Input: #test_gr_init_setup_ready and #test_gr_setup_alloc_obj_ctx
 *        must have been executed successfully.
 *
 * Steps:
 * -  Call g->ops.gr.setup.set_preemption_mode
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_gr_setup_set_preemption_mode(struct unit_module *m,
			       struct gk20a *g, void *args);

/**
 * Test specification for: test_gr_setup_free_obj_ctx.
 *
 * Description: Helps to verify common.gr object context cleanup.
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_gr_setup_free_subctx,
 *          nvgpu_gr_setup_free_gr_ctx,
 *          gops_gr_setup.free_gr_ctx,
 *          gops_gr_setup.free_subctx
 *
 * Input: #test_gr_init_setup_ready and #test_gr_setup_alloc_obj_ctx
 *        must have been executed successfully.
 *
 * Steps:
 * -  Call nvgpu_tsg_unbind_channel.
 * -  Call nvgpu_channel_close.
 * -  Call nvgpu_tsg_release.
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_gr_setup_free_obj_ctx(struct unit_module *m,
			       struct gk20a *g, void *args);

/**
 * Test specification for: test_gr_setup_preemption_mode_errors.
 *
 * Description: Helps to verify error paths in
 *              gops_gr_setup.set_preemption_mode call.
 *
 * Test Type: Error injection, Boundary value
 *
 * Equivalence classes:
 * Variable  : graphics_preempt_mode
 * - Valid   : {0}
 * - Invalid : {1 - U32_MAX}
 * Variable  : compute_preempt_mode
 * - Valid   : {0,2}
 * - Invalid : {3 - U32_MAX}
 *
 *
 * Targets: nvgpu_gr_setup_set_preemption_mode,
 *          nvgpu_gr_obj_ctx_set_ctxsw_preemption_mode
 *
 * Input: #test_gr_init_setup_ready and #test_gr_setup_alloc_obj_ctx
 *        must have been executed successfully.
 *
 * Steps:
 * - Verify various combinations of compute and graphics modes.
 * - Verify the error path by failing #nvgpu_preempt_channel.
 * - Verify the error path for NVGPU_INVALID_TSG_ID as ch->tsgid.
 * - Verify the error path for invalid ch->obj_class.
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_gr_setup_preemption_mode_errors(struct unit_module *m,
				      struct gk20a *g, void *args);

/**
 * Test specification for: test_gr_setup_alloc_obj_ctx_error_injections.
 *
 * Description: Helps to verify error paths in
 *              gops_gr_setup.alloc_obj_ctx call.
 *
 * Test Type: Error injection, Boundary values
 *
 * Targets: nvgpu_gr_setup_alloc_obj_ctx,
 *          nvgpu_gr_subctx_alloc, nvgpu_gr_obj_ctx_alloc,
 *          nvgpu_gr_obj_ctx_alloc_golden_ctx_image,
 *          nvgpu_gr_obj_ctx_get_golden_image_size,
 *          nvgpu_gr_obj_ctx_commit_global_ctx_buffers,
 *          nvgpu_gr_ctx_set_patch_ctx_data_count,
 *          nvgpu_gr_setup_free_subctx, nvgpu_gr_setup_free_gr_ctx,
 *          gm20b_ctxsw_prog_hw_get_fecs_header_size
 *
 * Input: #test_gr_init_setup_ready must have been executed successfully.
 *
 * Steps:
 * - Negative Tests for Setup alloc failures
 *   - Test-1 using invalid tsg, classobj and classnum.
 *   - Test-2 error injection in subctx allocation call.
 *   - Test-3 fail nvgpu_gr_obj_ctx_alloc by setting zero image size.
 *   - Test-4  and Test-8 fail nvgpu_gr_obj_ctx_alloc_golden_ctx_image
 *     by failing ctrl_ctsw.
 *   - Test-5 Fail L2 flush for branch coverage.
 *   - Test-6 Fake setup_free call for NULL checking.
 *
 * - Positive Tests
 *   - Test-7 nvgpu_gr_setup_alloc_obj_ctx pass without TSG subcontexts.
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_gr_setup_alloc_obj_ctx_error_injections(struct unit_module *m,
						 struct gk20a *g, void *args);
#endif /* UNIT_NVGPU_GR_SETUP_H */

/**
 * @}
 */

