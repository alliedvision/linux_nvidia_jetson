/*
 * Copyright (c) 2017-2019, NVIDIA CORPORATION.  All rights reserved.
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
/*
 * Function/Macro naming determines intended use:
 *
 *     <x>_r(void) : Returns the offset for register <x>.
 *
 *     <x>_o(void) : Returns the offset for element <x>.
 *
 *     <x>_w(void) : Returns the word offset for word (4 byte) element <x>.
 *
 *     <x>_<y>_s(void) : Returns size of field <y> of register <x> in bits.
 *
 *     <x>_<y>_f(u32 v) : Returns a value based on 'v' which has been shifted
 *         and masked to place it at field <y> of register <x>.  This value
 *         can be |'d with others to produce a full register value for
 *         register <x>.
 *
 *     <x>_<y>_m(void) : Returns a mask for field <y> of register <x>.  This
 *         value can be ~'d and then &'d to clear the value of field <y> for
 *         register <x>.
 *
 *     <x>_<y>_<z>_f(void) : Returns the constant value <z> after being shifted
 *         to place it at field <y> of register <x>.  This value can be |'d
 *         with others to produce a full register value for <x>.
 *
 *     <x>_<y>_v(u32 r) : Returns the value of field <y> from a full register
 *         <x> value 'r' after being shifted to place its LSB at bit 0.
 *         This value is suitable for direct comparison with other unshifted
 *         values appropriate for use in field <y> of register <x>.
 *
 *     <x>_<y>_<z>_v(void) : Returns the constant value for <z> defined for
 *         field <y> of register <x>.  This value is suitable for direct
 *         comparison with unshifted values appropriate for use in field <y>
 *         of register <x>.
 */
#ifndef NVGPU_HW_FIFO_GV100_H
#define NVGPU_HW_FIFO_GV100_H

#include <nvgpu/types.h>
#include <nvgpu/static_analysis.h>

#define fifo_userd_writeback_r()                                   (0x0000225cU)
#define fifo_userd_writeback_timer_f(v)                 ((U32(v) & 0xffU) << 0U)
#define fifo_userd_writeback_timer_disabled_v()                    (0x00000000U)
#define fifo_userd_writeback_timer_shorter_v()                     (0x00000003U)
#define fifo_userd_writeback_timer_100us_v()                       (0x00000064U)
#define fifo_userd_writeback_timescale_f(v)             ((U32(v) & 0xfU) << 12U)
#define fifo_userd_writeback_timescale_0_v()                       (0x00000000U)
#define fifo_runlist_base_r()                                      (0x00002270U)
#define fifo_runlist_base_ptr_f(v)                 ((U32(v) & 0xfffffffU) << 0U)
#define fifo_runlist_base_target_vid_mem_f()                              (0x0U)
#define fifo_runlist_base_target_sys_mem_coh_f()                   (0x20000000U)
#define fifo_runlist_base_target_sys_mem_ncoh_f()                  (0x30000000U)
#define fifo_runlist_r()                                           (0x00002274U)
#define fifo_runlist_engine_f(v)                        ((U32(v) & 0xfU) << 20U)
#define fifo_eng_runlist_base_r(i)\
		(nvgpu_safe_add_u32(0x00002280U, nvgpu_safe_mult_u32((i), 8U)))
#define fifo_eng_runlist_base__size_1_v()                          (0x0000000dU)
#define fifo_eng_runlist_r(i)\
		(nvgpu_safe_add_u32(0x00002284U, nvgpu_safe_mult_u32((i), 8U)))
#define fifo_eng_runlist__size_1_v()                               (0x0000000dU)
#define fifo_eng_runlist_length_f(v)                  ((U32(v) & 0xffffU) << 0U)
#define fifo_eng_runlist_length_max_v()                            (0x0000ffffU)
#define fifo_eng_runlist_pending_true_f()                            (0x100000U)
#define fifo_pb_timeslice_r(i)\
		(nvgpu_safe_add_u32(0x00002350U, nvgpu_safe_mult_u32((i), 4U)))
#define fifo_pb_timeslice_timeout_16_f()                                 (0x10U)
#define fifo_pb_timeslice_timescale_0_f()                                 (0x0U)
#define fifo_pb_timeslice_enable_true_f()                          (0x10000000U)
#define fifo_pbdma_map_r(i)\
		(nvgpu_safe_add_u32(0x00002390U, nvgpu_safe_mult_u32((i), 4U)))
#define fifo_intr_0_r()                                            (0x00002100U)
#define fifo_intr_0_bind_error_pending_f()                                (0x1U)
#define fifo_intr_0_bind_error_reset_f()                                  (0x1U)
#define fifo_intr_0_sched_error_pending_f()                             (0x100U)
#define fifo_intr_0_sched_error_reset_f()                               (0x100U)
#define fifo_intr_0_chsw_error_pending_f()                            (0x10000U)
#define fifo_intr_0_chsw_error_reset_f()                              (0x10000U)
#define fifo_intr_0_memop_timeout_pending_f()                        (0x800000U)
#define fifo_intr_0_memop_timeout_reset_f()                          (0x800000U)
#define fifo_intr_0_lb_error_pending_f()                            (0x1000000U)
#define fifo_intr_0_lb_error_reset_f()                              (0x1000000U)
#define fifo_intr_0_pbdma_intr_pending_f()                         (0x20000000U)
#define fifo_intr_0_runlist_event_pending_f()                      (0x40000000U)
#define fifo_intr_0_channel_intr_pending_f()                       (0x80000000U)
#define fifo_intr_en_0_r()                                         (0x00002140U)
#define fifo_intr_en_0_sched_error_f(v)                  ((U32(v) & 0x1U) << 8U)
#define fifo_intr_en_0_sched_error_m()                         (U32(0x1U) << 8U)
#define fifo_intr_en_1_r()                                         (0x00002528U)
#define fifo_intr_bind_error_r()                                   (0x0000252cU)
#define fifo_intr_sched_error_r()                                  (0x0000254cU)
#define fifo_intr_sched_error_code_f(v)                 ((U32(v) & 0xffU) << 0U)
#define fifo_intr_chsw_error_r()                                   (0x0000256cU)
#define fifo_intr_pbdma_id_r()                                     (0x000025a0U)
#define fifo_intr_pbdma_id_status_f(v, i)\
		((U32(v) & 0x1U) << (0U + (i)*1U))
#define fifo_intr_pbdma_id_status_v(r, i)\
		(((r) >> (0U + i*1U)) & 0x1U)
#define fifo_intr_pbdma_id_status__size_1_v()                      (0x0000000eU)
#define fifo_intr_runlist_r()                                      (0x00002a00U)
#define fifo_fb_timeout_r()                                        (0x00002a04U)
#define fifo_fb_timeout_period_m()                      (U32(0x3fffffffU) << 0U)
#define fifo_fb_timeout_period_max_f()                             (0x3fffffffU)
#define fifo_fb_timeout_period_init_f()                                (0x3c00U)
#define fifo_sched_disable_r()                                     (0x00002630U)
#define fifo_sched_disable_runlist_f(v, i)\
		((U32(v) & 0x1U) << (0U + (i)*1U))
#define fifo_sched_disable_runlist_m(i)\
		(U32(0x1U) << (0U + (i)*1U))
#define fifo_sched_disable_true_v()                                (0x00000001U)
#define fifo_runlist_preempt_r()                                   (0x00002638U)
#define fifo_runlist_preempt_runlist_f(v, i)\
		((U32(v) & 0x1U) << (0U + (i)*1U))
#define fifo_runlist_preempt_runlist_m(i)\
		(U32(0x1U) << (0U + (i)*1U))
#define fifo_runlist_preempt_runlist_pending_v()                   (0x00000001U)
#define fifo_preempt_r()                                           (0x00002634U)
#define fifo_preempt_pending_true_f()                                (0x100000U)
#define fifo_preempt_type_channel_f()                                     (0x0U)
#define fifo_preempt_type_tsg_f()                                   (0x1000000U)
#define fifo_preempt_chid_f(v)                         ((U32(v) & 0xfffU) << 0U)
#define fifo_preempt_id_f(v)                           ((U32(v) & 0xfffU) << 0U)
#define fifo_engine_status_r(i)\
		(nvgpu_safe_add_u32(0x00002640U, nvgpu_safe_mult_u32((i), 8U)))
#define fifo_engine_status__size_1_v()                             (0x0000000fU)
#define fifo_engine_status_id_v(r)                        (((r) >> 0U) & 0xfffU)
#define fifo_engine_status_id_type_v(r)                    (((r) >> 12U) & 0x1U)
#define fifo_engine_status_id_type_chid_v()                        (0x00000000U)
#define fifo_engine_status_id_type_tsgid_v()                       (0x00000001U)
#define fifo_engine_status_ctx_status_v(r)                 (((r) >> 13U) & 0x7U)
#define fifo_engine_status_ctx_status_valid_v()                    (0x00000001U)
#define fifo_engine_status_ctx_status_ctxsw_load_v()               (0x00000005U)
#define fifo_engine_status_ctx_status_ctxsw_save_v()               (0x00000006U)
#define fifo_engine_status_ctx_status_ctxsw_switch_v()             (0x00000007U)
#define fifo_engine_status_next_id_v(r)                  (((r) >> 16U) & 0xfffU)
#define fifo_engine_status_next_id_type_v(r)               (((r) >> 28U) & 0x1U)
#define fifo_engine_status_next_id_type_chid_v()                   (0x00000000U)
#define fifo_engine_status_next_id_type_tsgid_v()                  (0x00000001U)
#define fifo_engine_status_eng_reload_v(r)                 (((r) >> 29U) & 0x1U)
#define fifo_engine_status_faulted_v(r)                    (((r) >> 30U) & 0x1U)
#define fifo_engine_status_faulted_true_v()                        (0x00000001U)
#define fifo_engine_status_engine_v(r)                     (((r) >> 31U) & 0x1U)
#define fifo_engine_status_engine_idle_v()                         (0x00000000U)
#define fifo_engine_status_engine_busy_v()                         (0x00000001U)
#define fifo_engine_status_ctxsw_v(r)                      (((r) >> 15U) & 0x1U)
#define fifo_engine_status_ctxsw_in_progress_v()                   (0x00000001U)
#define fifo_engine_status_ctxsw_in_progress_f()                       (0x8000U)
#define fifo_pbdma_status_r(i)\
		(nvgpu_safe_add_u32(0x00003080U, nvgpu_safe_mult_u32((i), 4U)))
#define fifo_pbdma_status__size_1_v()                              (0x0000000eU)
#define fifo_pbdma_status_id_v(r)                         (((r) >> 0U) & 0xfffU)
#define fifo_pbdma_status_id_type_v(r)                     (((r) >> 12U) & 0x1U)
#define fifo_pbdma_status_id_type_chid_v()                         (0x00000000U)
#define fifo_pbdma_status_id_type_tsgid_v()                        (0x00000001U)
#define fifo_pbdma_status_chan_status_v(r)                 (((r) >> 13U) & 0x7U)
#define fifo_pbdma_status_chan_status_valid_v()                    (0x00000001U)
#define fifo_pbdma_status_chan_status_chsw_load_v()                (0x00000005U)
#define fifo_pbdma_status_chan_status_chsw_save_v()                (0x00000006U)
#define fifo_pbdma_status_chan_status_chsw_switch_v()              (0x00000007U)
#define fifo_pbdma_status_next_id_v(r)                   (((r) >> 16U) & 0xfffU)
#define fifo_pbdma_status_next_id_type_v(r)                (((r) >> 28U) & 0x1U)
#define fifo_pbdma_status_next_id_type_chid_v()                    (0x00000000U)
#define fifo_pbdma_status_next_id_type_tsgid_v()                   (0x00000001U)
#define fifo_pbdma_status_chsw_v(r)                        (((r) >> 15U) & 0x1U)
#define fifo_pbdma_status_chsw_in_progress_v()                     (0x00000001U)
#define fifo_cfg0_r()                                              (0x00002004U)
#define fifo_cfg0_num_pbdma_v(r)                           (((r) >> 0U) & 0xffU)
#define fifo_cfg0_pbdma_fault_id_v(r)                     (((r) >> 16U) & 0xffU)
#endif
