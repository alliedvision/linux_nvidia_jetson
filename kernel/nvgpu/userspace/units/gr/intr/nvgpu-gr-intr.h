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
#ifndef UNIT_NVGPU_GR_INTR_H
#define UNIT_NVGPU_GR_INTR_H

#include <nvgpu/types.h>

struct gk20a;
struct unit_module;

/** @addtogroup SWUTS-gr-intr
 *  @{
 *
 * Software Unit Test Specification for common.gr.intr
 */

/**
 * Test specification for: test_gr_intr_without_channel.
 *
 * Description: This test helps to verify the stall interrupts for some
 *              common.gr subunits without any channel allocation.
 *              Also helps to verify the nonstall interrupts.
 *
 * Test Type: Feature, Error guessing
 *
 * Targets: gops_gr_intr.stall_isr, nvgpu_gr_intr_stall_isr,
 *          gops_gr_intr.nonstall_isr, gm20b_gr_intr_nonstall_isr,
 *          nvgpu_gr_intr_init_support,
 *          nvgpu_gr_intr_handle_fecs_error,
 *          gops_gr_falcon.dump_stats,
 *          gm20b_gr_falcon_fecs_dump_stats,
 *          gm20b_gr_falcon_read_status1_fecs_ctxsw,
 *          gm20b_gr_falcon_get_fecs_ctxsw_mailbox_size,
 *          gm20b_gr_falcon_fecs_host_clear_intr,
 *          nvgpu_gr_intr_remove_support
 *
 * Input: #test_gr_init_setup_ready must have been executed successfully.
 *
 * Steps:
 * -  Set exception for FE, MEMFMT, PD, SCC, DS, SSYNC, MME, SKED
 *    in gr interrupt register.
 * -  Set Error injection for allocation, fecs and exception interrupts
 *    without properly setting the valid needed sub interrupts.
 * -  Call g->ops.gr.intr.stall_isr.
 * -  Call g->ops.gr.intr.nonstall_isr without nonstall pending interrupt set.
 * -  Set nonstall trap pending interrupt bit.
 * -  Call g->ops.gr.intr.nonstall_isr.
 * -  g->ops.gr.intr.handle_ssync_hww to NULL.
 * -  Call g->ops.gr.intr.nonstall_isr.
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */

int test_gr_intr_without_channel(struct unit_module *m,
				 struct gk20a *g, void *args);

/**
 * Test specification for: test_gr_intr_setup_channel.
 *
 * Description: This test helps to verify the stall interrupts for some
 *              common.gr subunits with channel and tsg allocation.
 *              Helps to figure out the current context on interrupt
 *              pending with subunit error.
 *
 * Test Type: Feature, Error guessing
 *
 * Targets: nvgpu_gr_intr_stall_isr,
 *          gops_gr_intr.handle_notify_pending,
 *          nvgpu_gr_intr_handle_notify_pending,
 *          gops_gr_intr.handle_semaphore_pending,
 *          nvgpu_gr_intr_handle_semaphore_pending,
 *          gops_gr_intr.handle_class_error,
 *          gm20b_gr_intr_handle_class_error,
 *          gm20b_gr_falcon_get_current_ctx,
 *          gm20b_gr_falcon_get_ctx_ptr,
 *          nvgpu_gr_intr_get_channel_from_ctx,
 *          nvgpu_gr_get_intr_ptr,
 *          nvgpu_gr_intr_remove_support
 *
 * Input: #test_gr_init_setup_ready must have been executed successfully.
 *
 * Steps:
 * -  Setup channel and tsg and bing tsg & channel.
 *    -  nvgpu_channel_setup_sw & nvgpu_tsg_setup_sw.
 *    -  nvgpu_tsg_open_new & nvgpu_tsg_channel_open_new.
 *    -  nvgpu_tsg_bind_channel.
 * -  Negative test - without setting the current context.
 *    -  Call g->ops.gr.intr.stall_isr.
 *    -  Set Notify and Semaphore wait_queue uninitialized.
 *    -  Call g->ops.gr.intr.stall_isr.
 * -  Positive test.
 *    -  Set tsgid as the current context.
 *    -  Set local cache.
 *    -  Call g->ops.gr.intr.stall_isr.
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */

int test_gr_intr_setup_channel(struct unit_module *m,
			       struct gk20a *g, void *args);

/**
 * Test specification for: test_gr_intr_sw_exceptions.
 *
 * Description: Helps to verify pending interrupts for illegal method.
 *
 * Test Type: Feature, Error guessing
 *
 * Targets: gops_gr_intr.stall_isr, nvgpu_gr_intr_stall_isr,
 *          gops_gr_intr.flush_channel_tlb, nvgpu_gr_intr_flush_channel_tlb,
 *          gops_gr_intr.handle_sw_method,
 *          gv11b_gr_intr_handle_sw_method,
 *          gops_gr_intr.trapped_method_info,
 *          gm20b_gr_intr_get_trapped_method_info,
 *          nvgpu_gr_intr_set_error_notifier,
 *          nvgpu_gr_intr_report_exception
 *
 * Input: #test_gr_init_setup_ready must have been executed successfully.
 *
 * Steps:
 * -  Setup illegal method pending interrupt bit.
 * -  Set the illegal method trapped addresses and datas.
 * -  Set invalid class and data.
 * -  Call g->ops.gr.intr.stall_isr.
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */

int test_gr_intr_sw_exceptions(struct unit_module *m,
			       struct gk20a *g, void *args);

/**
 * Test specification for: test_gr_intr_fecs_exceptions.
 *
 * Description: Helps to verify pending interrupts for fecs exceptions.
 *              Helps to verify exceptions for ctxsw_interrupts,
 *              fault_during_ctxsw, unimp_firmware_method,
 *              unimpl_illegal_method, watchdog, ecc_corrected and
 *              ecc_uncorrected interrupts.
 *
 * Test Type: Feature, Error guessing
 *
 * Targets: gops_gr_intr.stall_isr, nvgpu_gr_intr_stall_isr,
 *          gops_gr_intr.handle_fecs_error, gv11b_gr_intr_handle_fecs_error,
 *          gp10b_gr_intr_handle_fecs_error,
 *          gops_gr_intr.get_ctxsw_checksum_mismatch_mailbox_val,
 *          gv11b_gr_intr_ctxsw_checksum_mismatch_mailbox_val,
 *          gops_gr_falcon.read_fecs_ctxsw_mailbox,
 *          gm20b_gr_falcon_read_mailbox_fecs_ctxsw,
 *          gops_gr_falcon.dump_stats,
 *          gm20b_gr_falcon_fecs_dump_stats,
 *          gm20b_gr_falcon_read_status1_fecs_ctxsw,
 *          gm20b_gr_falcon_read_status0_fecs_ctxsw,
 *          gm20b_gr_falcon_get_fecs_ctxsw_mailbox_size,
 *          gm20b_gr_falcon_fecs_host_clear_intr,
 *          gm20b_gr_falcon_fecs_host_intr_status,
 *          gv11b_gr_falcon_handle_fecs_ecc_error,
 *          nvgpu_gr_intr_set_error_notifier,
 *          nvgpu_gr_intr_report_exception
 *
 * Input: #test_gr_init_setup_ready must have been executed successfully.
 *
 * Steps:
 * -  Set fecs exception interrupt bits.
 * -  Set fecs pending interrupt bit.
 * -  Set various ecc error register combinations.
 * -  Call g->ops.gr.intr.stall_isr.
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_gr_intr_fecs_exceptions(struct unit_module *m,
				 struct gk20a *g, void *args);

/**
 * Test specification for: test_gr_intr_gpc_exceptions.
 *
 * Description: Helps to verify pending interrupts for gpc_exceptions.
 *
 * Test Type: Feature, Error guessing
 *
 * Targets: nvgpu_gr_intr_handle_gpc_exception,
 *          gops_gr_intr.read_gpc_exception,
 *          gm20b_gr_intr_read_gpc_exception,
 *          gops_gr_intr.read_exception1, gm20b_gr_intr_read_exception1,
 *          gops_gr_intr.handle_exceptions,
 *          gm20b_gr_intr_handle_exceptions,
 *          gops_gr_intr.read_gpc_tpc_exception,
 *          gm20b_gr_intr_read_gpc_tpc_exception,
 *          gops_gr_intr.handle_gpc_gpccs_exception,
 *          gv11b_gr_intr_handle_gpc_gpccs_exception,
 *          gops_gr_intr.handle_gpc_gpcmmu_exception,
 *          gv11b_gr_intr_handle_gpc_gpcmmu_exception,
 *          gops_gr_intr.handle_gcc_exception,
 *          gv11b_gr_intr_handle_gcc_exception,
 *          gops_gr_intr.handle_sm_exception,
 *          nvgpu_gr_intr_handle_sm_exception,
 *          gops_gr_intr.get_tpc_exception, gm20b_gr_intr_get_tpc_exception,
 *          gops_gr_intr.handle_gpc_setup_exception,
 *          gv11b_gr_intr_handle_gpc_setup_exception,
 *          gops_gr_intr.handle_gpc_prop_exception,
 *          gv11b_gr_intr_handle_gpc_prop_exception,
 *          gops_gr_intr.handle_gpc_pes_exception,
 *          gv11b_gr_intr_handle_gpc_pes_exception,
 *          gops_gr_intr.handle_gpc_zcull_exception,
 *          gv11b_gr_intr_handle_gpc_zcull_exception,
 *          gops_gr_intr.handle_tpc_sm_ecc_exception,
 *          gv11b_gr_intr_handle_tpc_sm_ecc_exception,
 *          gops_gr_intr.handle_tpc_mpc_exception,
 *          gv11b_gr_intr_handle_tpc_mpc_exception,
 *          gops_gr_intr.handle_tpc_pe_exception,
 *          gv11b_gr_intr_handle_tpc_pe_exception,
 *          gops_gr_intr.set_hww_esr_report_mask,
 *          gv11b_gr_intr_set_hww_esr_report_mask,
 *          gops_gr_intr.get_esr_sm_sel, gv11b_gr_intr_get_esr_sm_sel,
 *          gops_gr_intr.clear_sm_hww, gv11b_gr_intr_clear_sm_hww,
 *          gops_gr_intr.handle_ssync_hww, gv11b_gr_intr_handle_ssync_hww,
 *          gops_gr_intr.record_sm_error_state,
 *          gv11b_gr_intr_record_sm_error_state,
 *          gops_gr_intr.get_sm_hww_warp_esr,
 *          gv11b_gr_intr_get_warp_esr_sm_hww,
 *          gops_gr_intr.get_sm_hww_warp_esr_pc,
 *          gv11b_gr_intr_get_warp_esr_sm_hww_pc,
 *          gops_gr_intr.get_sm_hww_global_esr,
 *          gv11b_gr_intr_get_sm_hww_global_esr,
 *          gops_gr_intr.get_sm_no_lock_down_hww_global_esr_mask,
 *          gv11b_gr_intr_get_sm_no_lock_down_hww_global_esr_mask,
 *          nvgpu_gr_intr_set_error_notifier,
 *          nvgpu_gr_intr_stall_isr,
 *          gops_gr_intr.read_pending_interrupts,
 *          gm20b_gr_intr_read_pending_interrupts,
 *          gops_gr_intr.clear_pending_interrupts,
 *          gm20b_gr_intr_clear_pending_interrupts,
 *          nvgpu_gr_gpc_offset,
 *          nvgpu_gr_tpc_offset,
 *          nvgpu_gr_sm_offset
 *
 * Input: #test_gr_init_setup_ready must have been executed successfully.
 *
 * Steps:
 * -  Making use of functions for following tests
 *    -  g->ops.gr.intr.handle_tpc_sm_ecc_exception.
 *    -  g->ops.gr.intr.handle_gpc_gpcmmu_exception.
 *    -  g->ops.gr.intr.handle_gpc_gpccs_exception.
 *    -  g->ops.gr.intr.handle_gcc_exception.
 *    -  g->ops.gr.intr.stall_isr.
 * -  Negative tests.
 *    -  Verify gpc exceptions with setting any gpc exception bits but
 *       enable the pending gpc exception interrupt bit.
 *    -  Verify gpc exceptions by setting gpc exception bits and
 *       enable the pending gpc exception interrupt bit.Without setting
 *       ecc status registers.
 *    -  Verify tpc_exception interrupt with NULL gpc and tpc hals
 *       and enabling gpc_tpc_exceptions.
 *    -  Verify gpc exceptions by setting gpc exception bits,
 *       ecc status registers and the pending gpc exception interrupt bit.
 *       Without setting SM ESR registers.
 *    -  Verify gpc and tpc_exceptions with various ecc registers value
 *       combinations for overflow and corrected and uncorrected errors.
 * -  Positive test.
 *    -  Verify gpc exceptions by setting gpc exception bits,
 *       tpc exception bits, ecc status registers, setting SM ESR registers
 *       and the pending gpc exception interrupt bit.
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_gr_intr_gpc_exceptions(struct unit_module *m,
				struct gk20a *g, void *args);

#endif /* UNIT_NVGPU_GR_INTR_H */

/**
 * @}
 */

