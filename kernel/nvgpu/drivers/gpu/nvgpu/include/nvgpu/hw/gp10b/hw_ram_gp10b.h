/*
 * Copyright (c) 2014-2021, NVIDIA CORPORATION.  All rights reserved.
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
#ifndef NVGPU_HW_RAM_GP10B_H
#define NVGPU_HW_RAM_GP10B_H

#include <nvgpu/types.h>
#include <nvgpu/static_analysis.h>

#define ram_in_ramfc_s()                                                 (4096U)
#define ram_in_ramfc_w()                                                    (0U)
#define ram_in_page_dir_base_target_f(v)                 ((U32(v) & 0x3U) << 0U)
#define ram_in_page_dir_base_target_w()                                   (128U)
#define ram_in_page_dir_base_target_vid_mem_f()                           (0x0U)
#define ram_in_page_dir_base_target_sys_mem_coh_f()                       (0x2U)
#define ram_in_page_dir_base_target_sys_mem_ncoh_f()                      (0x3U)
#define ram_in_page_dir_base_vol_w()                                      (128U)
#define ram_in_page_dir_base_vol_true_f()                                 (0x4U)
#define ram_in_page_dir_base_fault_replay_tex_f(v)       ((U32(v) & 0x1U) << 4U)
#define ram_in_page_dir_base_fault_replay_tex_m()              (U32(0x1U) << 4U)
#define ram_in_page_dir_base_fault_replay_tex_w()                         (128U)
#define ram_in_page_dir_base_fault_replay_tex_true_f()                   (0x10U)
#define ram_in_page_dir_base_fault_replay_gcc_f(v)       ((U32(v) & 0x1U) << 5U)
#define ram_in_page_dir_base_fault_replay_gcc_m()              (U32(0x1U) << 5U)
#define ram_in_page_dir_base_fault_replay_gcc_w()                         (128U)
#define ram_in_page_dir_base_fault_replay_gcc_true_f()                   (0x20U)
#define ram_in_use_ver2_pt_format_f(v)                  ((U32(v) & 0x1U) << 10U)
#define ram_in_use_ver2_pt_format_m()                         (U32(0x1U) << 10U)
#define ram_in_use_ver2_pt_format_w()                                     (128U)
#define ram_in_use_ver2_pt_format_true_f()                              (0x400U)
#define ram_in_use_ver2_pt_format_false_f()                               (0x0U)
#define ram_in_big_page_size_f(v)                       ((U32(v) & 0x1U) << 11U)
#define ram_in_big_page_size_m()                              (U32(0x1U) << 11U)
#define ram_in_big_page_size_w()                                          (128U)
#define ram_in_big_page_size_128kb_f()                                    (0x0U)
#define ram_in_big_page_size_64kb_f()                                   (0x800U)
#define ram_in_page_dir_base_lo_f(v)                ((U32(v) & 0xfffffU) << 12U)
#define ram_in_page_dir_base_lo_w()                                       (128U)
#define ram_in_page_dir_base_hi_f(v)                    ((U32(v) & 0xffU) << 0U)
#define ram_in_page_dir_base_hi_w()                                       (129U)
#define ram_in_adr_limit_lo_f(v)                    ((U32(v) & 0xfffffU) << 12U)
#define ram_in_adr_limit_lo_w()                                           (130U)
#define ram_in_adr_limit_hi_f(v)                  ((U32(v) & 0xffffffffU) << 0U)
#define ram_in_adr_limit_hi_w()                                           (131U)
#define ram_in_engine_cs_w()                                              (132U)
#define ram_in_engine_cs_wfi_v()                                   (0x00000000U)
#define ram_in_engine_cs_wfi_f()                                          (0x0U)
#define ram_in_engine_cs_fg_v()                                    (0x00000001U)
#define ram_in_engine_cs_fg_f()                                           (0x8U)
#define ram_in_gr_cs_w()                                                  (132U)
#define ram_in_gr_cs_wfi_f()                                              (0x0U)
#define ram_in_gr_wfi_target_w()                                          (132U)
#define ram_in_gr_wfi_mode_w()                                            (132U)
#define ram_in_gr_wfi_mode_physical_v()                            (0x00000000U)
#define ram_in_gr_wfi_mode_physical_f()                                   (0x0U)
#define ram_in_gr_wfi_mode_virtual_v()                             (0x00000001U)
#define ram_in_gr_wfi_mode_virtual_f()                                    (0x4U)
#define ram_in_gr_wfi_ptr_lo_f(v)                   ((U32(v) & 0xfffffU) << 12U)
#define ram_in_gr_wfi_ptr_lo_w()                                          (132U)
#define ram_in_gr_wfi_ptr_hi_f(v)                       ((U32(v) & 0xffU) << 0U)
#define ram_in_gr_wfi_ptr_hi_w()                                          (133U)
#define ram_in_base_shift_v()                                      (0x0000000cU)
#define ram_in_alloc_size_v()                                      (0x00001000U)
#define ram_fc_size_val_v()                                        (0x00000200U)
#define ram_fc_gp_put_w()                                                   (0U)
#define ram_fc_userd_w()                                                    (2U)
#define ram_fc_userd_hi_w()                                                 (3U)
#define ram_fc_signature_w()                                                (4U)
#define ram_fc_gp_get_w()                                                   (5U)
#define ram_fc_pb_get_w()                                                   (6U)
#define ram_fc_pb_get_hi_w()                                                (7U)
#define ram_fc_pb_top_level_get_w()                                         (8U)
#define ram_fc_pb_top_level_get_hi_w()                                      (9U)
#define ram_fc_acquire_w()                                                 (12U)
#define ram_fc_semaphorea_w()                                              (14U)
#define ram_fc_semaphoreb_w()                                              (15U)
#define ram_fc_semaphorec_w()                                              (16U)
#define ram_fc_semaphored_w()                                              (17U)
#define ram_fc_gp_base_w()                                                 (18U)
#define ram_fc_gp_base_hi_w()                                              (19U)
#define ram_fc_gp_fetch_w()                                                (20U)
#define ram_fc_pb_fetch_w()                                                (21U)
#define ram_fc_pb_fetch_hi_w()                                             (22U)
#define ram_fc_pb_put_w()                                                  (23U)
#define ram_fc_pb_put_hi_w()                                               (24U)
#define ram_fc_pb_header_w()                                               (33U)
#define ram_fc_pb_count_w()                                                (34U)
#define ram_fc_subdevice_w()                                               (37U)
#define ram_fc_formats_w()                                                 (39U)
#define ram_fc_allowed_syncpoints_w()                                      (58U)
#define ram_fc_syncpointa_w()                                              (41U)
#define ram_fc_syncpointb_w()                                              (42U)
#define ram_fc_target_w()                                                  (43U)
#define ram_fc_hce_ctrl_w()                                                (57U)
#define ram_fc_chid_w()                                                    (58U)
#define ram_fc_chid_id_f(v)                            ((U32(v) & 0xfffU) << 0U)
#define ram_fc_chid_id_w()                                                  (0U)
#define ram_fc_config_w()                                                  (61U)
#define ram_fc_runlist_timeslice_w()                                       (62U)
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
#define ram_userd_gp_top_level_get_w()                                     (22U)
#define ram_userd_gp_top_level_get_hi_w()                                  (23U)
#define ram_rl_entry_size_v()                                      (0x00000008U)
#define ram_rl_entry_chid_f(v)                         ((U32(v) & 0xfffU) << 0U)
#define ram_rl_entry_id_f(v)                           ((U32(v) & 0xfffU) << 0U)
#define ram_rl_entry_type_f(v)                          ((U32(v) & 0x1U) << 13U)
#define ram_rl_entry_type_chid_f()                                        (0x0U)
#define ram_rl_entry_type_tsg_f()                                      (0x2000U)
#define ram_rl_entry_timeslice_scale_f(v)               ((U32(v) & 0xfU) << 14U)
#define ram_rl_entry_timeslice_scale_v(r)                  (((r) >> 14U) & 0xfU)
#define ram_rl_entry_timeslice_scale_3_f()                             (0xc000U)
#define ram_rl_entry_timeslice_timeout_f(v)            ((U32(v) & 0xffU) << 18U)
#define ram_rl_entry_timeslice_timeout_v(r)               (((r) >> 18U) & 0xffU)
#define ram_rl_entry_timeslice_timeout_128_f()                      (0x2000000U)
#define ram_rl_entry_tsg_length_f(v)                   ((U32(v) & 0x3fU) << 26U)
#define ram_rl_entry_tsg_length_max_v()                            (0x00000020U)
#endif
