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
#ifndef NVGPU_HW_PBDMA_GV100_H
#define NVGPU_HW_PBDMA_GV100_H

#include <nvgpu/types.h>
#include <nvgpu/static_analysis.h>

#define pbdma_gp_entry1_r()                                        (0x10000004U)
#define pbdma_gp_entry1_get_hi_v(r)                        (((r) >> 0U) & 0xffU)
#define pbdma_gp_entry1_length_f(v)                ((U32(v) & 0x1fffffU) << 10U)
#define pbdma_gp_entry1_length_v(r)                   (((r) >> 10U) & 0x1fffffU)
#define pbdma_gp_base_r(i)\
		(nvgpu_safe_add_u32(0x00040048U, nvgpu_safe_mult_u32((i), 8192U)))
#define pbdma_gp_base__size_1_v()                                  (0x0000000eU)
#define pbdma_gp_base_offset_f(v)                 ((U32(v) & 0x1fffffffU) << 3U)
#define pbdma_gp_base_rsvd_s()                                              (3U)
#define pbdma_gp_base_hi_r(i)\
		(nvgpu_safe_add_u32(0x0004004cU, nvgpu_safe_mult_u32((i), 8192U)))
#define pbdma_gp_base_hi_offset_f(v)                    ((U32(v) & 0xffU) << 0U)
#define pbdma_gp_base_hi_limit2_f(v)                   ((U32(v) & 0x1fU) << 16U)
#define pbdma_gp_fetch_r(i)\
		(nvgpu_safe_add_u32(0x00040050U, nvgpu_safe_mult_u32((i), 8192U)))
#define pbdma_gp_get_r(i)\
		(nvgpu_safe_add_u32(0x00040014U, nvgpu_safe_mult_u32((i), 8192U)))
#define pbdma_gp_put_r(i)\
		(nvgpu_safe_add_u32(0x00040000U, nvgpu_safe_mult_u32((i), 8192U)))
#define pbdma_pb_fetch_r(i)\
		(nvgpu_safe_add_u32(0x00040054U, nvgpu_safe_mult_u32((i), 8192U)))
#define pbdma_pb_fetch_hi_r(i)\
		(nvgpu_safe_add_u32(0x00040058U, nvgpu_safe_mult_u32((i), 8192U)))
#define pbdma_get_r(i)\
		(nvgpu_safe_add_u32(0x00040018U, nvgpu_safe_mult_u32((i), 8192U)))
#define pbdma_get_hi_r(i)\
		(nvgpu_safe_add_u32(0x0004001cU, nvgpu_safe_mult_u32((i), 8192U)))
#define pbdma_put_r(i)\
		(nvgpu_safe_add_u32(0x0004005cU, nvgpu_safe_mult_u32((i), 8192U)))
#define pbdma_put_hi_r(i)\
		(nvgpu_safe_add_u32(0x00040060U, nvgpu_safe_mult_u32((i), 8192U)))
#define pbdma_pb_header_r(i)\
		(nvgpu_safe_add_u32(0x00040084U, nvgpu_safe_mult_u32((i), 8192U)))
#define pbdma_pb_header_priv_user_f()                                     (0x0U)
#define pbdma_pb_header_method_zero_f()                                   (0x0U)
#define pbdma_pb_header_subchannel_zero_f()                               (0x0U)
#define pbdma_pb_header_level_main_f()                                    (0x0U)
#define pbdma_pb_header_first_true_f()                               (0x400000U)
#define pbdma_pb_header_type_inc_f()                               (0x20000000U)
#define pbdma_pb_header_type_non_inc_f()                           (0x60000000U)
#define pbdma_hdr_shadow_r(i)\
		(nvgpu_safe_add_u32(0x00040118U, nvgpu_safe_mult_u32((i), 8192U)))
#define pbdma_gp_shadow_0_r(i)\
		(nvgpu_safe_add_u32(0x00040110U, nvgpu_safe_mult_u32((i), 8192U)))
#define pbdma_gp_shadow_1_r(i)\
		(nvgpu_safe_add_u32(0x00040114U, nvgpu_safe_mult_u32((i), 8192U)))
#define pbdma_subdevice_r(i)\
		(nvgpu_safe_add_u32(0x00040094U, nvgpu_safe_mult_u32((i), 8192U)))
#define pbdma_subdevice_id_f(v)                        ((U32(v) & 0xfffU) << 0U)
#define pbdma_subdevice_status_active_f()                          (0x10000000U)
#define pbdma_subdevice_channel_dma_enable_f()                     (0x20000000U)
#define pbdma_method0_r(i)\
		(nvgpu_safe_add_u32(0x000400c0U, nvgpu_safe_mult_u32((i), 8192U)))
#define pbdma_method0_fifo_size_v()                                (0x00000004U)
#define pbdma_method0_addr_f(v)                        ((U32(v) & 0xfffU) << 2U)
#define pbdma_method0_addr_v(r)                           (((r) >> 2U) & 0xfffU)
#define pbdma_method0_subch_v(r)                           (((r) >> 16U) & 0x7U)
#define pbdma_method0_first_true_f()                                 (0x400000U)
#define pbdma_method0_valid_true_f()                               (0x80000000U)
#define pbdma_method1_r(i)\
		(nvgpu_safe_add_u32(0x000400c8U, nvgpu_safe_mult_u32((i), 8192U)))
#define pbdma_method2_r(i)\
		(nvgpu_safe_add_u32(0x000400d0U, nvgpu_safe_mult_u32((i), 8192U)))
#define pbdma_method3_r(i)\
		(nvgpu_safe_add_u32(0x000400d8U, nvgpu_safe_mult_u32((i), 8192U)))
#define pbdma_data0_r(i)\
		(nvgpu_safe_add_u32(0x000400c4U, nvgpu_safe_mult_u32((i), 8192U)))
#define pbdma_acquire_r(i)\
		(nvgpu_safe_add_u32(0x00040030U, nvgpu_safe_mult_u32((i), 8192U)))
#define pbdma_acquire_retry_man_2_f()                                     (0x2U)
#define pbdma_acquire_retry_exp_2_f()                                   (0x100U)
#define pbdma_acquire_timeout_exp_f(v)                  ((U32(v) & 0xfU) << 11U)
#define pbdma_acquire_timeout_exp_max_v()                          (0x0000000fU)
#define pbdma_acquire_timeout_exp_max_f()                              (0x7800U)
#define pbdma_acquire_timeout_man_f(v)               ((U32(v) & 0xffffU) << 15U)
#define pbdma_acquire_timeout_man_max_v()                          (0x0000ffffU)
#define pbdma_acquire_timeout_man_max_f()                          (0x7fff8000U)
#define pbdma_acquire_timeout_en_enable_f()                        (0x80000000U)
#define pbdma_acquire_timeout_en_disable_f()                              (0x0U)
#define pbdma_status_r(i)\
		(nvgpu_safe_add_u32(0x00040100U, nvgpu_safe_mult_u32((i), 8192U)))
#define pbdma_channel_r(i)\
		(nvgpu_safe_add_u32(0x00040120U, nvgpu_safe_mult_u32((i), 8192U)))
#define pbdma_signature_r(i)\
		(nvgpu_safe_add_u32(0x00040010U, nvgpu_safe_mult_u32((i), 8192U)))
#define pbdma_signature_hw_valid_f()                                   (0xfaceU)
#define pbdma_signature_sw_zero_f()                                       (0x0U)
#define pbdma_userd_r(i)\
		(nvgpu_safe_add_u32(0x00040008U, nvgpu_safe_mult_u32((i), 8192U)))
#define pbdma_userd_target_vid_mem_f()                                    (0x0U)
#define pbdma_userd_target_sys_mem_coh_f()                                (0x2U)
#define pbdma_userd_target_sys_mem_ncoh_f()                               (0x3U)
#define pbdma_userd_addr_f(v)                       ((U32(v) & 0x7fffffU) << 9U)
#define pbdma_config_r(i)\
		(nvgpu_safe_add_u32(0x000400f4U, nvgpu_safe_mult_u32((i), 8192U)))
#define pbdma_config_l2_evict_first_f()                                   (0x0U)
#define pbdma_config_l2_evict_normal_f()                                  (0x1U)
#define pbdma_config_ce_split_enable_f()                                  (0x0U)
#define pbdma_config_ce_split_disable_f()                                (0x10U)
#define pbdma_config_auth_level_non_privileged_f()                        (0x0U)
#define pbdma_config_auth_level_privileged_f()                          (0x100U)
#define pbdma_config_userd_writeback_disable_f()                          (0x0U)
#define pbdma_config_userd_writeback_enable_f()                        (0x1000U)
#define pbdma_userd_hi_r(i)\
		(nvgpu_safe_add_u32(0x0004000cU, nvgpu_safe_mult_u32((i), 8192U)))
#define pbdma_userd_hi_addr_f(v)                        ((U32(v) & 0xffU) << 0U)
#define pbdma_hce_ctrl_r(i)\
		(nvgpu_safe_add_u32(0x000400e4U, nvgpu_safe_mult_u32((i), 8192U)))
#define pbdma_hce_ctrl_hce_priv_mode_yes_f()                             (0x20U)
#define pbdma_intr_0_r(i)\
		(nvgpu_safe_add_u32(0x00040108U, nvgpu_safe_mult_u32((i), 8192U)))
#define pbdma_intr_0_memreq_v(r)                            (((r) >> 0U) & 0x1U)
#define pbdma_intr_0_memreq_pending_f()                                   (0x1U)
#define pbdma_intr_0_memack_timeout_pending_f()                           (0x2U)
#define pbdma_intr_0_memack_extra_pending_f()                             (0x4U)
#define pbdma_intr_0_memdat_timeout_pending_f()                           (0x8U)
#define pbdma_intr_0_memdat_extra_pending_f()                            (0x10U)
#define pbdma_intr_0_memflush_pending_f()                                (0x20U)
#define pbdma_intr_0_memop_pending_f()                                   (0x40U)
#define pbdma_intr_0_lbconnect_pending_f()                               (0x80U)
#define pbdma_intr_0_lbreq_pending_f()                                  (0x100U)
#define pbdma_intr_0_lback_timeout_pending_f()                          (0x200U)
#define pbdma_intr_0_lback_extra_pending_f()                            (0x400U)
#define pbdma_intr_0_lbdat_timeout_pending_f()                          (0x800U)
#define pbdma_intr_0_lbdat_extra_pending_f()                           (0x1000U)
#define pbdma_intr_0_gpfifo_pending_f()                                (0x2000U)
#define pbdma_intr_0_gpptr_pending_f()                                 (0x4000U)
#define pbdma_intr_0_gpentry_pending_f()                               (0x8000U)
#define pbdma_intr_0_gpcrc_pending_f()                                (0x10000U)
#define pbdma_intr_0_pbptr_pending_f()                                (0x20000U)
#define pbdma_intr_0_pbentry_pending_f()                              (0x40000U)
#define pbdma_intr_0_pbcrc_pending_f()                                (0x80000U)
#define pbdma_intr_0_clear_faulted_error_pending_f()                 (0x100000U)
#define pbdma_intr_0_method_pending_f()                              (0x200000U)
#define pbdma_intr_0_methodcrc_pending_f()                           (0x400000U)
#define pbdma_intr_0_device_pending_f()                              (0x800000U)
#define pbdma_intr_0_eng_reset_pending_f()                          (0x1000000U)
#define pbdma_intr_0_semaphore_pending_f()                          (0x2000000U)
#define pbdma_intr_0_acquire_pending_f()                            (0x4000000U)
#define pbdma_intr_0_pri_pending_f()                                (0x8000000U)
#define pbdma_intr_0_no_ctxsw_seg_pending_f()                      (0x20000000U)
#define pbdma_intr_0_pbseg_pending_f()                             (0x40000000U)
#define pbdma_intr_0_signature_pending_f()                         (0x80000000U)
#define pbdma_intr_1_r(i)\
		(nvgpu_safe_add_u32(0x00040148U, nvgpu_safe_mult_u32((i), 8192U)))
#define pbdma_intr_1_ctxnotvalid_m()                          (U32(0x1U) << 31U)
#define pbdma_intr_1_ctxnotvalid_pending_f()                       (0x80000000U)
#define pbdma_intr_en_0_r(i)\
		(nvgpu_safe_add_u32(0x0004010cU, nvgpu_safe_mult_u32((i), 8192U)))
#define pbdma_intr_en_0_lbreq_enabled_f()                               (0x100U)
#define pbdma_intr_en_1_r(i)\
		(nvgpu_safe_add_u32(0x0004014cU, nvgpu_safe_mult_u32((i), 8192U)))
#define pbdma_intr_stall_r(i)\
		(nvgpu_safe_add_u32(0x0004013cU, nvgpu_safe_mult_u32((i), 8192U)))
#define pbdma_intr_stall_lbreq_enabled_f()                              (0x100U)
#define pbdma_intr_stall_1_r(i)\
		(nvgpu_safe_add_u32(0x00040140U, nvgpu_safe_mult_u32((i), 8192U)))
#define pbdma_intr_stall_1_hce_illegal_op_enabled_f()                     (0x1U)
#define pbdma_udma_nop_r()                                         (0x00000008U)
#define pbdma_runlist_timeslice_r(i)\
		(nvgpu_safe_add_u32(0x000400f8U, nvgpu_safe_mult_u32((i), 8192U)))
#define pbdma_runlist_timeslice_timeout_128_f()                          (0x80U)
#define pbdma_runlist_timeslice_timescale_3_f()                        (0x3000U)
#define pbdma_runlist_timeslice_enable_true_f()                    (0x10000000U)
#define pbdma_target_r(i)\
		(nvgpu_safe_add_u32(0x000400acU, nvgpu_safe_mult_u32((i), 8192U)))
#define pbdma_target_engine_sw_f()                                       (0x1fU)
#define pbdma_target_eng_ctx_valid_true_f()                           (0x10000U)
#define pbdma_target_eng_ctx_valid_false_f()                              (0x0U)
#define pbdma_target_ce_ctx_valid_true_f()                            (0x20000U)
#define pbdma_target_ce_ctx_valid_false_f()                               (0x0U)
#define pbdma_target_host_tsg_event_reason_pbdma_idle_f()                 (0x0U)
#define pbdma_target_host_tsg_event_reason_semaphore_acquire_failure_f()\
				(0x1000000U)
#define pbdma_target_host_tsg_event_reason_tsg_yield_f()            (0x2000000U)
#define pbdma_target_host_tsg_event_reason_host_subchannel_switch_f()\
				(0x3000000U)
#define pbdma_target_should_send_tsg_event_true_f()                (0x20000000U)
#define pbdma_target_should_send_tsg_event_false_f()                      (0x0U)
#define pbdma_target_needs_host_tsg_event_true_f()                 (0x80000000U)
#define pbdma_target_needs_host_tsg_event_false_f()                       (0x0U)
#define pbdma_set_channel_info_r(i)\
		(nvgpu_safe_add_u32(0x000400fcU, nvgpu_safe_mult_u32((i), 8192U)))
#define pbdma_set_channel_info_veid_f(v)                ((U32(v) & 0x3fU) << 8U)
#define pbdma_timeout_r(i)\
		(nvgpu_safe_add_u32(0x0004012cU, nvgpu_safe_mult_u32((i), 8192U)))
#define pbdma_timeout_period_m()                        (U32(0xffffffffU) << 0U)
#define pbdma_timeout_period_max_f()                               (0xffffffffU)
#define pbdma_timeout_period_init_f()                                 (0x10000U)
#endif
