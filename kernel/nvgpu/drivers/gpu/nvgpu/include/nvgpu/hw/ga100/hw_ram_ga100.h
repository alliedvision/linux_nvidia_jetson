/*
 * Copyright (c) 2020-2021, NVIDIA CORPORATION.  All rights reserved.
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
#ifndef NVGPU_HW_RAM_GA100_H
#define NVGPU_HW_RAM_GA100_H

#include <nvgpu/types.h>
#include <nvgpu/static_analysis.h>

#define ram_in_ramfc_s()                                                 (4096U)
#define ram_in_ramfc_w()                                                    (0U)
#define ram_in_page_dir_base_target_vid_mem_f()                           (0x0U)
#define ram_in_page_dir_base_target_sys_mem_coh_f()                       (0x2U)
#define ram_in_page_dir_base_target_sys_mem_ncoh_f()                      (0x3U)
#define ram_in_page_dir_base_vol_true_f()                                 (0x4U)
#define ram_in_use_ver2_pt_format_true_f()                              (0x400U)
#define ram_in_big_page_size_m()                              (U32(0x1U) << 11U)
#define ram_in_big_page_size_w()                                          (128U)
#define ram_in_big_page_size_128kb_f()                                    (0x0U)
#define ram_in_big_page_size_64kb_f()                                   (0x800U)
#define ram_in_page_dir_base_lo_f(v)                ((U32(v) & 0xfffffU) << 12U)
#define ram_in_page_dir_base_lo_w()                                       (128U)
#define ram_in_page_dir_base_hi_f(v)              ((U32(v) & 0xffffffffU) << 0U)
#define ram_in_page_dir_base_hi_w()                                       (129U)
#define ram_in_engine_cs_w()                                              (132U)
#define ram_in_engine_cs_wfi_v()                                   (0x00000000U)
#define ram_in_engine_wfi_mode_f(v)                      ((U32(v) & 0x1U) << 2U)
#define ram_in_engine_wfi_mode_virtual_v()                         (0x00000001U)
#define ram_in_engine_wfi_target_w()                                      (132U)
#define ram_in_engine_wfi_ptr_lo_f(v)               ((U32(v) & 0xfffffU) << 12U)
#define ram_in_engine_wfi_ptr_hi_f(v)                   ((U32(v) & 0xffU) << 0U)
#define ram_in_engine_wfi_ptr_hi_w()                                      (133U)
#define ram_in_engine_wfi_veid_f(v)                     ((U32(v) & 0x3fU) << 0U)
#define ram_in_engine_wfi_veid_w()                                        (134U)
#define ram_in_eng_method_buffer_addr_lo_w()                              (136U)
#define ram_in_eng_method_buffer_addr_hi_w()                              (137U)
#define ram_in_sc_pdb_valid_long_w(i)\
		(166ULL + (((i)*1ULL)/32ULL))
#define ram_in_sc_pdb_valid__size_1_v()                            (0x00000040U)
#define ram_in_sc_page_dir_base_target_f(v, i)\
		((U32(v) & 0x3U) << (0U + (i)*0U))
#define ram_in_sc_page_dir_base_target__size_1_v()                 (0x00000040U)
#define ram_in_sc_page_dir_base_target_vid_mem_v()                 (0x00000000U)
#define ram_in_sc_page_dir_base_target_sys_mem_coh_v()             (0x00000002U)
#define ram_in_sc_page_dir_base_target_sys_mem_ncoh_v()            (0x00000003U)
#define ram_in_sc_page_dir_base_vol_f(v, i)\
		((U32(v) & 0x1U) << (2U + (i)*0U))
#define ram_in_sc_page_dir_base_vol_w(i)\
		(168U + (((i)*128U)/32U))
#define ram_in_sc_page_dir_base_vol_true_v()                       (0x00000001U)
#define ram_in_sc_page_dir_base_fault_replay_tex_f(v, i)\
		((U32(v) & 0x1U) << (4U + (i)*0U))
#define ram_in_sc_page_dir_base_fault_replay_gcc_f(v, i)\
		((U32(v) & 0x1U) << (5U + (i)*0U))
#define ram_in_sc_use_ver2_pt_format_f(v, i)\
		((U32(v) & 0x1U) << (10U + (i)*0U))
#define ram_in_sc_big_page_size_f(v, i)\
		((U32(v) & 0x1U) << (11U + (i)*0U))
#define ram_in_sc_page_dir_base_hi_w(i)\
		(169U + (((i)*128U)/32U))
#define ram_in_sc_page_dir_base_lo_0_f(v)           ((U32(v) & 0xfffffU) << 12U)
#define ram_in_base_shift_v()                                      (0x0000000cU)
#define ram_in_alloc_size_v()                                      (0x00001000U)
#define ram_fc_size_val_v()                                        (0x00000200U)
#define ram_fc_signature_w()                                                (4U)
#define ram_fc_pb_get_w()                                                   (6U)
#define ram_fc_pb_get_hi_w()                                                (7U)
#define ram_fc_pb_top_level_get_w()                                         (8U)
#define ram_fc_pb_top_level_get_hi_w()                                      (9U)
#define ram_fc_acquire_w()                                                 (12U)
#define ram_fc_sem_addr_hi_w()                                             (14U)
#define ram_fc_sem_addr_lo_w()                                             (15U)
#define ram_fc_sem_payload_lo_w()                                          (16U)
#define ram_fc_sem_payload_hi_w()                                          (39U)
#define ram_fc_sem_execute_w()                                             (17U)
#define ram_fc_gp_base_w()                                                 (18U)
#define ram_fc_gp_base_hi_w()                                              (19U)
#define ram_fc_pb_put_w()                                                  (23U)
#define ram_fc_pb_put_hi_w()                                               (24U)
#define ram_fc_pb_header_w()                                               (33U)
#define ram_fc_pb_count_w()                                                (34U)
#define ram_fc_subdevice_w()                                               (37U)
#define ram_fc_target_w()                                                  (43U)
#define ram_fc_hce_ctrl_w()                                                (57U)
#define ram_fc_config_w()                                                  (61U)
#define ram_fc_set_channel_info_w()                                        (63U)
#define ram_fc_intr_notify_w()                                             (62U)
#define ram_userd_base_shift_v()                                   (0x00000009U)
#define ram_userd_chan_size_v()                                    (0x00000200U)
#define ram_userd_put_w()                                                  (16U)
#define ram_userd_get_w()                                                  (17U)
#define ram_userd_ref_w()                                                  (18U)
#define ram_userd_put_hi_w()                                               (19U)
#define ram_userd_top_level_get_w()                                        (22U)
#define ram_userd_top_level_get_hi_w()                                     (23U)
#define ram_userd_get_hi_w()                                               (24U)
#define ram_userd_gp_get_w()                                               (34U)
#define ram_userd_gp_put_w()                                               (35U)
#define ram_rl_entry_size_v()                                      (0x00000010U)
#define ram_rl_entry_type_channel_v()                              (0x00000000U)
#define ram_rl_entry_type_tsg_v()                                  (0x00000001U)
#define ram_rl_entry_id_f(v)                           ((U32(v) & 0xfffU) << 0U)
#define ram_rl_entry_chan_runqueue_selector_f(v)         ((U32(v) & 0x1U) << 1U)
#define ram_rl_entry_chan_inst_target_f(v)               ((U32(v) & 0x3U) << 4U)
#define ram_rl_entry_chan_inst_target_sys_mem_ncoh_v()             (0x00000003U)
#define ram_rl_entry_chan_inst_target_sys_mem_coh_v()              (0x00000002U)
#define ram_rl_entry_chan_inst_target_vid_mem_v()                  (0x00000000U)
#define ram_rl_entry_chan_userd_target_f(v)              ((U32(v) & 0x3U) << 6U)
#define ram_rl_entry_chan_userd_target_vid_mem_v()                 (0x00000000U)
#define ram_rl_entry_chan_userd_target_sys_mem_coh_v()             (0x00000002U)
#define ram_rl_entry_chan_userd_target_sys_mem_ncoh_v()            (0x00000003U)
#define ram_rl_entry_chan_userd_ptr_lo_f(v)         ((U32(v) & 0xffffffU) << 8U)
#define ram_rl_entry_chan_userd_ptr_hi_f(v)       ((U32(v) & 0xffffffffU) << 0U)
#define ram_rl_entry_chid_f(v)                         ((U32(v) & 0xfffU) << 0U)
#define ram_rl_entry_chan_inst_ptr_lo_f(v)          ((U32(v) & 0xfffffU) << 12U)
#define ram_rl_entry_chan_inst_ptr_hi_f(v)        ((U32(v) & 0xffffffffU) << 0U)
#define ram_rl_entry_tsg_timeslice_scale_f(v)           ((U32(v) & 0xfU) << 16U)
#define ram_rl_entry_tsg_timeslice_timeout_f(v)        ((U32(v) & 0xffU) << 24U)
#define ram_rl_entry_tsg_length_f(v)                    ((U32(v) & 0xffU) << 0U)
#define ram_rl_entry_tsg_length_max_v()                            (0x00000080U)
#define ram_rl_entry_tsg_tsgid_f(v)                    ((U32(v) & 0xfffU) << 0U)
#define ram_rl_entry_chan_userd_ptr_align_shift_v()                (0x00000008U)
#define ram_rl_entry_chan_inst_ptr_align_shift_v()                 (0x0000000cU)
#endif
