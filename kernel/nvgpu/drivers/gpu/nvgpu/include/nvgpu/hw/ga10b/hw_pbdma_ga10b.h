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
#ifndef NVGPU_HW_PBDMA_GA10B_H
#define NVGPU_HW_PBDMA_GA10B_H

#include <nvgpu/types.h>
#include <nvgpu/static_analysis.h>

#define pbdma_gp_entry1_r()                                        (0x10000004U)
#define pbdma_gp_entry1_length_f(v)                ((U32(v) & 0x1fffffU) << 10U)
#define pbdma_gp_base_r(i)\
		(nvgpu_safe_add_u32(0x00040048U, nvgpu_safe_mult_u32((i), 2048U)))
#define pbdma_gp_base_offset_f(v)                 ((U32(v) & 0x1fffffffU) << 3U)
#define pbdma_gp_base_rsvd_s()                                              (3U)
#define pbdma_gp_base_hi_r(i)\
		(nvgpu_safe_add_u32(0x0004004cU, nvgpu_safe_mult_u32((i), 2048U)))
#define pbdma_gp_base_hi_offset_f(v)                    ((U32(v) & 0xffU) << 0U)
#define pbdma_gp_base_hi_limit2_f(v)                   ((U32(v) & 0x1fU) << 16U)
#define pbdma_gp_fetch_r(i)\
		(nvgpu_safe_add_u32(0x00040050U, nvgpu_safe_mult_u32((i), 2048U)))
#define pbdma_gp_get_r(i)\
		(nvgpu_safe_add_u32(0x00040014U, nvgpu_safe_mult_u32((i), 2048U)))
#define pbdma_gp_put_r(i)\
		(nvgpu_safe_add_u32(0x00040000U, nvgpu_safe_mult_u32((i), 2048U)))
#define pbdma_get_r(i)\
		(nvgpu_safe_add_u32(0x00040018U, nvgpu_safe_mult_u32((i), 2048U)))
#define pbdma_get_hi_r(i)\
		(nvgpu_safe_add_u32(0x0004001cU, nvgpu_safe_mult_u32((i), 2048U)))
#define pbdma_put_r(i)\
		(nvgpu_safe_add_u32(0x0004005cU, nvgpu_safe_mult_u32((i), 2048U)))
#define pbdma_put_hi_r(i)\
		(nvgpu_safe_add_u32(0x00040060U, nvgpu_safe_mult_u32((i), 2048U)))
#define pbdma_pb_header_r(i)\
		(nvgpu_safe_add_u32(0x00040084U, nvgpu_safe_mult_u32((i), 2048U)))
#define pbdma_pb_header_method_zero_f()                                   (0x0U)
#define pbdma_pb_header_subchannel_zero_f()                               (0x0U)
#define pbdma_pb_header_level_main_f()                                    (0x0U)
#define pbdma_pb_header_first_true_f()                               (0x400000U)
#define pbdma_pb_header_type_inc_f()                               (0x20000000U)
#define pbdma_pb_header_type_non_inc_f()                           (0x60000000U)
#define pbdma_hdr_shadow_r(i)\
		(nvgpu_safe_add_u32(0x0004006cU, nvgpu_safe_mult_u32((i), 2048U)))
#define pbdma_gp_shadow_0_r(i)\
		(nvgpu_safe_add_u32(0x00040110U, nvgpu_safe_mult_u32((i), 2048U)))
#define pbdma_gp_shadow_1_r(i)\
		(nvgpu_safe_add_u32(0x00040114U, nvgpu_safe_mult_u32((i), 2048U)))
#define pbdma_subdevice_r(i)\
		(nvgpu_safe_add_u32(0x00040094U, nvgpu_safe_mult_u32((i), 2048U)))
#define pbdma_subdevice_id_f(v)                        ((U32(v) & 0xfffU) << 0U)
#define pbdma_subdevice_status_active_f()                          (0x10000000U)
#define pbdma_subdevice_channel_dma_enable_f()                     (0x20000000U)
#define pbdma_method0_r(i)\
		(nvgpu_safe_add_u32(0x000400c0U, nvgpu_safe_mult_u32((i), 2048U)))
#define pbdma_method0_fifo_size_v()                                (0x00000004U)
#define pbdma_method0_addr_f(v)                        ((U32(v) & 0xfffU) << 2U)
#define pbdma_method0_addr_v(r)                           (((r) >> 2U) & 0xfffU)
#define pbdma_method0_subch_v(r)                           (((r) >> 16U) & 0x7U)
#define pbdma_method0_first_true_f()                                 (0x400000U)
#define pbdma_method0_valid_true_f()                               (0x80000000U)
#define pbdma_method1_r(i)\
		(nvgpu_safe_add_u32(0x000400c8U, nvgpu_safe_mult_u32((i), 2048U)))
#define pbdma_method2_r(i)\
		(nvgpu_safe_add_u32(0x000400d0U, nvgpu_safe_mult_u32((i), 2048U)))
#define pbdma_method3_r(i)\
		(nvgpu_safe_add_u32(0x000400d8U, nvgpu_safe_mult_u32((i), 2048U)))
#define pbdma_acquire_r(i)\
		(nvgpu_safe_add_u32(0x00040030U, nvgpu_safe_mult_u32((i), 2048U)))
#define pbdma_acquire_retry_man_2_f()                                     (0x2U)
#define pbdma_acquire_retry_exp_2_f()                                   (0x100U)
#define pbdma_acquire_timeout_exp_f(v)                  ((U32(v) & 0xfU) << 11U)
#define pbdma_acquire_timeout_exp_max_v()                          (0x0000000fU)
#define pbdma_acquire_timeout_man_f(v)               ((U32(v) & 0xffffU) << 15U)
#define pbdma_acquire_timeout_man_max_v()                          (0x0000ffffU)
#define pbdma_acquire_timeout_en_enable_f()                        (0x80000000U)
#define pbdma_signature_r(i)\
		(nvgpu_safe_add_u32(0x00040010U, nvgpu_safe_mult_u32((i), 2048U)))
#define pbdma_signature_sw_zero_f()                                       (0x0U)
#define pbdma_config_r(i)\
		(nvgpu_safe_add_u32(0x000400f4U, nvgpu_safe_mult_u32((i), 2048U)))
#define pbdma_config_auth_level_privileged_f()                          (0x100U)
#define pbdma_config_userd_writeback_m()                      (U32(0x1U) << 12U)
#define pbdma_config_userd_writeback_enable_f()                        (0x1000U)
#define pbdma_hce_ctrl_r(i)\
		(nvgpu_safe_add_u32(0x000400e4U, nvgpu_safe_mult_u32((i), 2048U)))
#define pbdma_hce_ctrl_hce_priv_mode_yes_f()                             (0x20U)
#define pbdma_intr_0_r(i)\
		(nvgpu_safe_add_u32(0x00040108U, nvgpu_safe_mult_u32((i), 2048U)))
#define pbdma_intr_0__size_1_v()                                   (0x00000006U)
#define pbdma_intr_0_gpfifo_pending_f()                                (0x2000U)
#define pbdma_intr_0_gpptr_pending_f()                                 (0x4000U)
#define pbdma_intr_0_gpentry_pending_f()                               (0x8000U)
#define pbdma_intr_0_gpcrc_pending_f()                                (0x10000U)
#define pbdma_intr_0_pbptr_pending_f()                                (0x20000U)
#define pbdma_intr_0_pbentry_pending_f()                              (0x40000U)
#define pbdma_intr_0_pbcrc_pending_f()                                (0x80000U)
#define pbdma_intr_0_method_pending_f()                              (0x200000U)
#define pbdma_intr_0_device_pending_f()                              (0x800000U)
#define pbdma_intr_0_eng_reset_pending_f()                          (0x1000000U)
#define pbdma_intr_0_semaphore_pending_f()                          (0x2000000U)
#define pbdma_intr_0_acquire_pending_f()                            (0x4000000U)
#define pbdma_intr_0_pri_pending_f()                                (0x8000000U)
#define pbdma_intr_0_pbseg_pending_f()                             (0x40000000U)
#define pbdma_intr_0_signature_pending_f()                         (0x80000000U)
#define pbdma_intr_1_r(i)\
		(nvgpu_safe_add_u32(0x00040148U, nvgpu_safe_mult_u32((i), 2048U)))
#define pbdma_intr_1_ctxnotvalid_pending_f()                       (0x80000000U)
#define pbdma_intr_0_en_set_tree_r(i,j)\
		(nvgpu_safe_add_u32(nvgpu_safe_add_u32(0x00040170U, \
		nvgpu_safe_mult_u32((i), 2048U)), nvgpu_safe_mult_u32((j), 4U)))
#define pbdma_intr_0_en_set_tree__size_1_v()                       (0x00000006U)
#define pbdma_intr_0_en_set_tree_gpfifo_enabled_f()                    (0x2000U)
#define pbdma_intr_0_en_set_tree_gpptr_enabled_f()                     (0x4000U)
#define pbdma_intr_0_en_set_tree_gpentry_enabled_f()                   (0x8000U)
#define pbdma_intr_0_en_set_tree_gpcrc_enabled_f()                    (0x10000U)
#define pbdma_intr_0_en_set_tree_pbptr_enabled_f()                    (0x20000U)
#define pbdma_intr_0_en_set_tree_pbentry_enabled_f()                  (0x40000U)
#define pbdma_intr_0_en_set_tree_pbcrc_enabled_f()                    (0x80000U)
#define pbdma_intr_0_en_set_tree_method_enabled_f()                  (0x200000U)
#define pbdma_intr_0_en_set_tree_device_enabled_f()                  (0x800000U)
#define pbdma_intr_0_en_set_tree_eng_reset_enabled_f()              (0x1000000U)
#define pbdma_intr_0_en_set_tree_semaphore_enabled_f()              (0x2000000U)
#define pbdma_intr_0_en_set_tree_acquire_enabled_f()                (0x4000000U)
#define pbdma_intr_0_en_set_tree_pri_enabled_f()                    (0x8000000U)
#define pbdma_intr_0_en_set_tree_pbseg_enabled_f()                 (0x40000000U)
#define pbdma_intr_0_en_set_tree_signature_enabled_f()             (0x80000000U)
#define pbdma_intr_0_en_clear_tree_r(i,j)\
		(nvgpu_safe_add_u32(nvgpu_safe_add_u32(0x00040190U, \
		nvgpu_safe_mult_u32((i), 2048U)), nvgpu_safe_mult_u32((j), 4U)))
#define pbdma_intr_0_en_clear_tree__size_1_v()                     (0x00000006U)
#define pbdma_intr_0_en_clear_tree__size_2_v()                     (0x00000002U)
#define pbdma_intr_0_en_clear_tree_gpfifo_enabled_f()                  (0x2000U)
#define pbdma_intr_0_en_clear_tree_gpptr_enabled_f()                   (0x4000U)
#define pbdma_intr_0_en_clear_tree_gpentry_enabled_f()                 (0x8000U)
#define pbdma_intr_0_en_clear_tree_gpcrc_enabled_f()                  (0x10000U)
#define pbdma_intr_0_en_clear_tree_pbptr_enabled_f()                  (0x20000U)
#define pbdma_intr_0_en_clear_tree_pbentry_enabled_f()                (0x40000U)
#define pbdma_intr_0_en_clear_tree_pbcrc_enabled_f()                  (0x80000U)
#define pbdma_intr_0_en_clear_tree_method_enabled_f()                (0x200000U)
#define pbdma_intr_0_en_clear_tree_device_enabled_f()                (0x800000U)
#define pbdma_intr_0_en_clear_tree_eng_reset_enabled_f()            (0x1000000U)
#define pbdma_intr_0_en_clear_tree_semaphore_enabled_f()            (0x2000000U)
#define pbdma_intr_0_en_clear_tree_acquire_enabled_f()              (0x4000000U)
#define pbdma_intr_0_en_clear_tree_pri_enabled_f()                  (0x8000000U)
#define pbdma_intr_0_en_clear_tree_pbseg_enabled_f()               (0x40000000U)
#define pbdma_intr_0_en_clear_tree_signature_enabled_f()           (0x80000000U)
#define pbdma_intr_1_en_set_tree_r(i,j)\
		(nvgpu_safe_add_u32(nvgpu_safe_add_u32(0x00040180U, \
		nvgpu_safe_mult_u32((i), 2048U)), nvgpu_safe_mult_u32((j), 4U)))
#define pbdma_intr_1_en_set_tree_hce_re_illegal_op_enabled_f()            (0x1U)
#define pbdma_intr_1_en_set_tree_hce_re_alignb_enabled_f()                (0x2U)
#define pbdma_intr_1_en_set_tree_hce_priv_enabled_f()                     (0x4U)
#define pbdma_intr_1_en_set_tree_hce_illegal_mthd_enabled_f()             (0x8U)
#define pbdma_intr_1_en_set_tree_hce_illegal_class_enabled_f()           (0x10U)
#define pbdma_intr_1_en_set_tree_ctxnotvalid_enabled_f()           (0x80000000U)
#define pbdma_intr_1_en_clear_tree_r(i,j)\
		(nvgpu_safe_add_u32(nvgpu_safe_add_u32(0x000401a0U, \
		nvgpu_safe_mult_u32((i), 2048U)), nvgpu_safe_mult_u32((j), 4U)))
#define pbdma_intr_1_en_clear_tree_hce_re_illegal_op_enabled_f()          (0x1U)
#define pbdma_intr_1_en_clear_tree_hce_re_alignb_enabled_f()              (0x2U)
#define pbdma_intr_1_en_clear_tree_hce_priv_enabled_f()                   (0x4U)
#define pbdma_intr_1_en_clear_tree_hce_illegal_mthd_enabled_f()           (0x8U)
#define pbdma_intr_1_en_clear_tree_hce_illegal_class_enabled_f()         (0x10U)
#define pbdma_intr_1_en_clear_tree_ctxnotvalid_enabled_f()         (0x80000000U)
#define pbdma_udma_nop_r()                                         (0x00000008U)
#define pbdma_target_r(i)\
		(nvgpu_safe_add_u32(0x000400acU, nvgpu_safe_mult_u32((i), 2048U)))
#define pbdma_target_engine_f(v)                         ((U32(v) & 0x3U) << 0U)
#define pbdma_target_eng_ctx_valid_true_f()                           (0x10000U)
#define pbdma_target_ce_ctx_valid_true_f()                            (0x20000U)
#define pbdma_set_channel_info_r(i)\
		(nvgpu_safe_add_u32(0x000400fcU, nvgpu_safe_mult_u32((i), 2048U)))
#define pbdma_set_channel_info_veid_f(v)                ((U32(v) & 0x3fU) << 8U)
#define pbdma_set_channel_info_chid_f(v)              ((U32(v) & 0xfffU) << 16U)
#define pbdma_status_sched_r(i)\
		(nvgpu_safe_add_u32(0x0004015cU, nvgpu_safe_mult_u32((i), 2048U)))
#define pbdma_status_sched_tsgid_v(r)                     (((r) >> 0U) & 0xfffU)
#define pbdma_status_sched_chan_status_v(r)                (((r) >> 13U) & 0x7U)
#define pbdma_status_sched_chan_status_valid_v()                   (0x00000001U)
#define pbdma_status_sched_chan_status_chsw_save_v()               (0x00000005U)
#define pbdma_status_sched_chan_status_chsw_load_v()               (0x00000006U)
#define pbdma_status_sched_chan_status_chsw_switch_v()             (0x00000007U)
#define pbdma_status_sched_next_tsgid_v(r)               (((r) >> 16U) & 0xfffU)
#define pbdma_intr_notify_r(i)\
		(nvgpu_safe_add_u32(0x000400f8U, nvgpu_safe_mult_u32((i), 2048U)))
#define pbdma_intr_notify_vector_f(v)                  ((U32(v) & 0xfffU) << 0U)
#define pbdma_intr_notify_ctrl_gsp_disable_f()                            (0x0U)
#define pbdma_intr_notify_ctrl_cpu_enable_f()                      (0x80000000U)
#define pbdma_cfg0_r(i)\
		(nvgpu_safe_add_u32(0x00040104U, nvgpu_safe_mult_u32((i), 2048U)))
#define pbdma_cfg0__size_1_v()                                     (0x00000006U)
#define pbdma_cfg0_pbdma_fault_id_v(r)                    (((r) >> 0U) & 0x3ffU)
#endif
