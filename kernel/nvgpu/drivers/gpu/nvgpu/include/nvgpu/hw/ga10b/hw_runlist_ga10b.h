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
#ifndef NVGPU_HW_RUNLIST_GA10B_H
#define NVGPU_HW_RUNLIST_GA10B_H

#include <nvgpu/types.h>
#include <nvgpu/static_analysis.h>

#define runlist_channel_config_r()                                 (0x00000004U)
#define runlist_channel_config_num_channels_log2_2k_v()            (0x0000000bU)
#define runlist_channel_config_chram_bar0_offset_v(r) (((r) >> 4U) & 0xfffffffU)
#define runlist_channel_config_chram_bar0_offset_b()                        (4U)
#define runlist_doorbell_config_r()                                (0x00000008U)
#define runlist_doorbell_config_id_v(r)                 (((r) >> 16U) & 0xffffU)
#define runlist_fb_config_r()                                      (0x0000000cU)
#define runlist_fb_config_fb_thread_id_v(r)                (((r) >> 0U) & 0xffU)
#define runlist_pbdma_config_r(i)\
		(nvgpu_safe_add_u32(0x00000010U, nvgpu_safe_mult_u32((i), 4U)))
#define runlist_pbdma_config__size_1_v()                           (0x00000002U)
#define runlist_pbdma_config_id_v(r)                       (((r) >> 0U) & 0xffU)
#define runlist_pbdma_config_pbdma_bar0_offset_v(r)     (((r) >> 10U) & 0xffffU)
#define runlist_pbdma_config_valid_v(r)                    (((r) >> 31U) & 0x1U)
#define runlist_pbdma_config_valid_true_v()                        (0x00000001U)
#define runlist_userd_writeback_r()                                (0x00000028U)
#define runlist_userd_writeback_timer_100us_f()                          (0x64U)
#define runlist_userd_writeback_timescale_0_f()                           (0x0U)
#define runlist_engine_status0_r(i)\
		(nvgpu_safe_add_u32(0x00000200U, nvgpu_safe_mult_u32((i), 64U)))
#define runlist_engine_status0_tsgid_v(r)                 (((r) >> 0U) & 0xfffU)
#define runlist_engine_status0_ctx_status_v(r)             (((r) >> 13U) & 0x7U)
#define runlist_engine_status0_ctx_status_valid_v()                (0x00000001U)
#define runlist_engine_status0_ctx_status_save_v()                 (0x00000005U)
#define runlist_engine_status0_ctx_status_load_v()                 (0x00000006U)
#define runlist_engine_status0_ctx_status_switch_v()               (0x00000007U)
#define runlist_engine_status0_ctxsw_in_progress_f()                   (0x8000U)
#define runlist_engine_status0_next_tsgid_v(r)           (((r) >> 16U) & 0xfffU)
#define runlist_engine_status0_faulted_v(r)                (((r) >> 30U) & 0x1U)
#define runlist_engine_status0_faulted_true_v()                    (0x00000001U)
#define runlist_engine_status0_engine_v(r)                 (((r) >> 31U) & 0x1U)
#define runlist_engine_status0_engine_busy_v()                     (0x00000001U)
#define runlist_engine_status1_r(i)\
		(nvgpu_safe_add_u32(0x00000204U, nvgpu_safe_mult_u32((i), 64U)))
#define runlist_engine_status1_intr_id_v(r)               (((r) >> 16U) & 0x1fU)
#define runlist_engine_status_debug_r(i)\
		(nvgpu_safe_add_u32(0x00000228U, nvgpu_safe_mult_u32((i), 64U)))
#define runlist_engine_status_debug_engine_id_v(r)        (((r) >> 24U) & 0x3fU)
#define runlist_chram_channel_r(i)\
		(nvgpu_safe_add_u32(0x00000000U, nvgpu_safe_mult_u32((i), 4U)))
#define runlist_chram_channel_enable_v(r)                   (((r) >> 1U) & 0x1U)
#define runlist_chram_channel_enable_in_use_v()                    (0x00000001U)
#define runlist_chram_channel_next_v(r)                     (((r) >> 2U) & 0x1U)
#define runlist_chram_channel_next_true_v()                        (0x00000001U)
#define runlist_chram_channel_busy_v(r)                     (((r) >> 3U) & 0x1U)
#define runlist_chram_channel_busy_true_v()                        (0x00000001U)
#define runlist_chram_channel_eng_faulted_v(r)              (((r) >> 5U) & 0x1U)
#define runlist_chram_channel_eng_faulted_true_v()                 (0x00000001U)
#define runlist_chram_channel_on_pbdma_m()                     (U32(0x1U) << 6U)
#define runlist_chram_channel_on_pbdma_v(r)                 (((r) >> 6U) & 0x1U)
#define runlist_chram_channel_on_pbdma_true_v()                    (0x00000001U)
#define runlist_chram_channel_on_eng_m()                       (U32(0x1U) << 7U)
#define runlist_chram_channel_on_eng_v(r)                   (((r) >> 7U) & 0x1U)
#define runlist_chram_channel_on_eng_true_v()                      (0x00000001U)
#define runlist_chram_channel_pending_m()                      (U32(0x1U) << 8U)
#define runlist_chram_channel_pending_v(r)                  (((r) >> 8U) & 0x1U)
#define runlist_chram_channel_pending_true_v()                     (0x00000001U)
#define runlist_chram_channel_ctx_reload_m()                   (U32(0x1U) << 9U)
#define runlist_chram_channel_ctx_reload_v(r)               (((r) >> 9U) & 0x1U)
#define runlist_chram_channel_ctx_reload_true_v()                  (0x00000001U)
#define runlist_chram_channel_pbdma_busy_m()                  (U32(0x1U) << 10U)
#define runlist_chram_channel_pbdma_busy_v(r)              (((r) >> 10U) & 0x1U)
#define runlist_chram_channel_pbdma_busy_true_v()                  (0x00000001U)
#define runlist_chram_channel_eng_busy_m()                    (U32(0x1U) << 11U)
#define runlist_chram_channel_eng_busy_v(r)                (((r) >> 11U) & 0x1U)
#define runlist_chram_channel_eng_busy_true_v()                    (0x00000001U)
#define runlist_chram_channel_acquire_fail_m()                (U32(0x1U) << 12U)
#define runlist_chram_channel_acquire_fail_v(r)            (((r) >> 12U) & 0x1U)
#define runlist_chram_channel_acquire_fail_true_v()                (0x00000001U)
#define runlist_chram_channel_update_f(v)         ((U32(v) & 0xffffffffU) << 0U)
#define runlist_chram_channel_update_enable_channel_v()            (0x00000002U)
#define runlist_chram_channel_update_disable_channel_v()           (0x00000003U)
#define runlist_chram_channel_update_force_ctx_reload_v()          (0x00000200U)
#define runlist_chram_channel_update_reset_pbdma_faulted_v()       (0x00000011U)
#define runlist_chram_channel_update_reset_eng_faulted_v()         (0x00000021U)
#define runlist_chram_channel_update_clear_channel_v()             (0xffffffffU)
#define runlist_submit_base_lo_r()                                 (0x00000080U)
#define runlist_submit_base_lo_ptr_align_shift_v()                 (0x0000000aU)
#define runlist_submit_base_lo_ptr_lo_f(v)         ((U32(v) & 0x3fffffU) << 10U)
#define runlist_submit_base_lo_target_vid_mem_f()                         (0x0U)
#define runlist_submit_base_lo_target_sys_mem_coherent_f()                (0x2U)
#define runlist_submit_base_lo_target_sys_mem_noncoherent_f()             (0x3U)
#define runlist_submit_base_hi_r()                                 (0x00000084U)
#define runlist_submit_base_hi_ptr_hi_f(v)              ((U32(v) & 0xffU) << 0U)
#define runlist_submit_r()                                         (0x00000088U)
#define runlist_submit_length_f(v)                    ((U32(v) & 0xffffU) << 0U)
#define runlist_submit_length_max_v()                              (0x0000ffffU)
#define runlist_submit_offset_f(v)                   ((U32(v) & 0xffffU) << 16U)
#define runlist_submit_info_r()                                    (0x0000008cU)
#define runlist_submit_info_pending_true_f()                           (0x8000U)
#define runlist_sched_disable_r()                                  (0x00000094U)
#define runlist_sched_disable_runlist_enabled_v()                  (0x00000000U)
#define runlist_sched_disable_runlist_disabled_v()                 (0x00000001U)
#define runlist_preempt_r()                                        (0x00000098U)
#define runlist_preempt_id_f(v)                        ((U32(v) & 0xfffU) << 0U)
#define runlist_preempt_tsg_preempt_pending_true_f()                 (0x100000U)
#define runlist_preempt_runlist_preempt_pending_true_f()             (0x200000U)
#define runlist_preempt_type_runlist_f()                                  (0x0U)
#define runlist_preempt_type_tsg_f()                                (0x1000000U)
#define runlist_virtual_channel_cfg_r(i)\
		(nvgpu_safe_add_u32(0x00000300U, nvgpu_safe_mult_u32((i), 4U)))
#define runlist_virtual_channel_cfg_mask_hw_mask_hw_init_f()            (0x7ffU)
#define runlist_virtual_channel_cfg_pending_enable_true_f()        (0x80000000U)
#define runlist_intr_vectorid_r(i)\
		(nvgpu_safe_add_u32(0x00000160U, nvgpu_safe_mult_u32((i), 4U)))
#define runlist_intr_vectorid__size_1_v()                          (0x00000002U)
#define runlist_intr_vectorid_vector_v(r)                 (((r) >> 0U) & 0xfffU)
#define runlist_intr_vectorid_gsp_enable_f()                       (0x40000000U)
#define runlist_intr_vectorid_cpu_enable_f()                       (0x80000000U)
#define runlist_intr_retrigger_r(i)\
		(nvgpu_safe_add_u32(0x00000180U, nvgpu_safe_mult_u32((i), 4U)))
#define runlist_intr_retrigger_trigger_true_f()                           (0x1U)
#define runlist_intr_0_r()                                         (0x00000100U)
#define runlist_intr_0_ctxsw_timeout_eng_pending_f(i)\
		((U32(0x1U) << (0U + ((i)*1U))))
#define runlist_intr_0_ctxsw_timeout_eng_reset_f(i)\
		((U32(0x1U) << (0U + ((i)*1U))))
#define runlist_intr_0_bad_tsg_pending_f()                             (0x1000U)
#define runlist_intr_0_pbdmai_intr_tree_j_pending_f(i, j)\
		((U32(0x1U) << (16U + ((i)*1U) + ((j)*2U))))
#define runlist_intr_0_pbdmai_intr_tree_j__size_1_v()              (0x00000002U)
#define runlist_intr_bad_tsg_r()                                   (0x00000174U)
#define runlist_intr_bad_tsg_code_v(r)                      (((r) >> 0U) & 0xfU)
#define runlist_intr_0_en_set_tree_r(i)\
		(nvgpu_safe_add_u32(0x00000120U, nvgpu_safe_mult_u32((i), 8U)))
#define runlist_intr_0_en_set_tree_ctxsw_timeout_eng0_enabled_f()         (0x1U)
#define runlist_intr_0_en_set_tree_ctxsw_timeout_eng1_enabled_f()         (0x2U)
#define runlist_intr_0_en_set_tree_ctxsw_timeout_eng2_enabled_f()         (0x4U)
#define runlist_intr_0_en_set_tree_bad_tsg_enabled_f()                 (0x1000U)
#define runlist_intr_0_en_set_tree_pbdma0_intr_tree_0_enabled_f()     (0x10000U)
#define runlist_intr_0_en_set_tree_pbdma1_intr_tree_0_enabled_f()     (0x20000U)
#define runlist_intr_0_en_clear_tree_r(i)\
		(nvgpu_safe_add_u32(0x00000140U, nvgpu_safe_mult_u32((i), 8U)))
#define runlist_intr_0_en_clear_tree_ctxsw_timeout_eng0_enabled_f()       (0x1U)
#define runlist_intr_0_en_clear_tree_ctxsw_timeout_eng1_enabled_f()       (0x2U)
#define runlist_intr_0_en_clear_tree_ctxsw_timeout_eng2_enabled_f()       (0x4U)
#define runlist_engine_ctxsw_timeout_info_r(i)\
		(nvgpu_safe_add_u32(0x00000224U, nvgpu_safe_mult_u32((i), 64U)))
#define runlist_engine_ctxsw_timeout_info__size_1_v()              (0x00000003U)
#define runlist_engine_ctxsw_timeout_info_prev_tsgid_v(r)\
				(((r) >> 0U) & 0x3fffU)
#define runlist_engine_ctxsw_timeout_info_ctxsw_state_v(r) (((r) >> 14U) & 0x3U)
#define runlist_engine_ctxsw_timeout_info_ctxsw_state_load_v()     (0x00000001U)
#define runlist_engine_ctxsw_timeout_info_ctxsw_state_save_v()     (0x00000002U)
#define runlist_engine_ctxsw_timeout_info_ctxsw_state_switch_v()   (0x00000003U)
#define runlist_engine_ctxsw_timeout_info_next_tsgid_v(r)\
				(((r) >> 16U) & 0x3fffU)
#define runlist_engine_ctxsw_timeout_info_status_v(r)      (((r) >> 30U) & 0x3U)
#define runlist_engine_ctxsw_timeout_info_status_ack_received_v()  (0x00000002U)
#define runlist_engine_ctxsw_timeout_info_status_dropped_timeout_v()\
				(0x00000003U)
#define runlist_engine_ctxsw_timeout_config_r(i)\
		(nvgpu_safe_add_u32(0x00000220U, nvgpu_safe_mult_u32((i), 64U)))
#define runlist_engine_ctxsw_timeout_config__size_1_v()            (0x00000003U)
#define runlist_engine_ctxsw_timeout_config_period_f(v)\
				((U32(v) & 0x7fffffffU) << 0U)
#define runlist_engine_ctxsw_timeout_config_period_v(r)\
				(((r) >> 0U) & 0x7fffffffU)
#define runlist_engine_ctxsw_timeout_config_period_max_f()         (0x7fffffffU)
#define runlist_engine_ctxsw_timeout_config_detection_disabled_f()        (0x0U)
#define runlist_engine_ctxsw_timeout_config_detection_enabled_f()  (0x80000000U)
#endif
