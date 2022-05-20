/*
 * Copyright (c) 2016-2020, NVIDIA CORPORATION.  All rights reserved.
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
#ifndef NVGPU_HW_CTXSW_PROG_GV11B_H
#define NVGPU_HW_CTXSW_PROG_GV11B_H

#include <nvgpu/types.h>
#include <nvgpu/static_analysis.h>

#define ctxsw_prog_fecs_header_v()                                 (0x00000100U)
#define ctxsw_prog_gpccs_header_stride_v()                         (0x00000100U)
#define ctxsw_prog_main_image_num_gpcs_o()                         (0x00000008U)
#define ctxsw_prog_main_image_ctl_o()                              (0x0000000cU)
#define ctxsw_prog_main_image_ctl_type_f(v)             ((U32(v) & 0x3fU) << 0U)
#define ctxsw_prog_main_image_ctl_type_undefined_v()               (0x00000000U)
#define ctxsw_prog_main_image_ctl_type_opengl_v()                  (0x00000008U)
#define ctxsw_prog_main_image_ctl_type_dx9_v()                     (0x00000010U)
#define ctxsw_prog_main_image_ctl_type_dx10_v()                    (0x00000011U)
#define ctxsw_prog_main_image_ctl_type_dx11_v()                    (0x00000012U)
#define ctxsw_prog_main_image_ctl_type_compute_v()                 (0x00000020U)
#define ctxsw_prog_main_image_ctl_type_per_veid_header_v()         (0x00000021U)
#define ctxsw_prog_main_image_patch_count_o()                      (0x00000010U)
#define ctxsw_prog_main_image_context_id_o()                       (0x000000f0U)
#define ctxsw_prog_main_image_patch_adr_lo_o()                     (0x00000014U)
#define ctxsw_prog_main_image_patch_adr_hi_o()                     (0x00000018U)
#define ctxsw_prog_main_image_zcull_o()                            (0x0000001cU)
#define ctxsw_prog_main_image_zcull_mode_no_ctxsw_v()              (0x00000001U)
#define ctxsw_prog_main_image_zcull_mode_separate_buffer_v()       (0x00000002U)
#define ctxsw_prog_main_image_zcull_ptr_o()                        (0x00000020U)
#define ctxsw_prog_main_image_pm_o()                               (0x00000028U)
#define ctxsw_prog_main_image_pm_mode_m()                      (U32(0x7U) << 0U)
#define ctxsw_prog_main_image_pm_mode_ctxsw_f()                           (0x1U)
#define ctxsw_prog_main_image_pm_mode_no_ctxsw_f()                        (0x0U)
#define ctxsw_prog_main_image_pm_mode_stream_out_ctxsw_f()                (0x2U)
#define ctxsw_prog_main_image_pm_smpc_mode_m()                 (U32(0x7U) << 3U)
#define ctxsw_prog_main_image_pm_smpc_mode_ctxsw_f()                      (0x8U)
#define ctxsw_prog_main_image_pm_smpc_mode_no_ctxsw_f()                   (0x0U)
#define ctxsw_prog_main_image_pm_ptr_o()                           (0x0000002cU)
#define ctxsw_prog_main_image_num_save_ops_o()                     (0x000000f4U)
#define ctxsw_prog_main_image_num_wfi_save_ops_o()                 (0x000000d0U)
#define ctxsw_prog_main_image_num_cta_save_ops_o()                 (0x000000d4U)
#define ctxsw_prog_main_image_num_gfxp_save_ops_o()                (0x000000d8U)
#define ctxsw_prog_main_image_num_cilp_save_ops_o()                (0x000000dcU)
#define ctxsw_prog_main_image_num_restore_ops_o()                  (0x000000f8U)
#define ctxsw_prog_main_image_zcull_ptr_hi_o()                     (0x00000060U)
#define ctxsw_prog_main_image_zcull_ptr_hi_v_f(v)    ((U32(v) & 0x1ffffU) << 0U)
#define ctxsw_prog_main_image_pm_ptr_hi_o()                        (0x00000094U)
#define ctxsw_prog_main_image_full_preemption_ptr_hi_o()           (0x00000064U)
#define ctxsw_prog_main_image_full_preemption_ptr_hi_v_f(v)\
				((U32(v) & 0x1ffffU) << 0U)
#define ctxsw_prog_main_image_full_preemption_ptr_o()              (0x00000068U)
#define ctxsw_prog_main_image_full_preemption_ptr_v_f(v)\
				((U32(v) & 0xffffffffU) << 0U)
#define ctxsw_prog_main_image_full_preemption_ptr_veid0_hi_o()     (0x00000070U)
#define ctxsw_prog_main_image_full_preemption_ptr_veid0_hi_v_f(v)\
				((U32(v) & 0x1ffffU) << 0U)
#define ctxsw_prog_main_image_full_preemption_ptr_veid0_o()        (0x00000074U)
#define ctxsw_prog_main_image_full_preemption_ptr_veid0_v_f(v)\
				((U32(v) & 0xffffffffU) << 0U)
#define ctxsw_prog_main_image_context_buffer_ptr_hi_o()            (0x00000078U)
#define ctxsw_prog_main_image_context_buffer_ptr_hi_v_f(v)\
				((U32(v) & 0x1ffffU) << 0U)
#define ctxsw_prog_main_image_context_buffer_ptr_o()               (0x0000007cU)
#define ctxsw_prog_main_image_context_buffer_ptr_v_f(v)\
				((U32(v) & 0xffffffffU) << 0U)
#define ctxsw_prog_main_image_magic_value_o()                      (0x000000fcU)
#define ctxsw_prog_main_image_magic_value_v_value_v()              (0x600dc0deU)
#define ctxsw_prog_local_priv_register_ctl_o()                     (0x0000000cU)
#define ctxsw_prog_local_priv_register_ctl_offset_v(r)   (((r) >> 0U) & 0xffffU)
#define ctxsw_prog_main_image_global_cb_ptr_o()                    (0x000000b8U)
#define ctxsw_prog_main_image_global_cb_ptr_v_f(v)\
				((U32(v) & 0xffffffffU) << 0U)
#define ctxsw_prog_main_image_global_cb_ptr_hi_o()                 (0x000000bcU)
#define ctxsw_prog_main_image_global_cb_ptr_hi_v_f(v)\
				((U32(v) & 0x1ffffU) << 0U)
#define ctxsw_prog_main_image_global_pagepool_ptr_o()              (0x000000c0U)
#define ctxsw_prog_main_image_global_pagepool_ptr_v_f(v)\
				((U32(v) & 0xffffffffU) << 0U)
#define ctxsw_prog_main_image_global_pagepool_ptr_hi_o()           (0x000000c4U)
#define ctxsw_prog_main_image_global_pagepool_ptr_hi_v_f(v)\
				((U32(v) & 0x1ffffU) << 0U)
#define ctxsw_prog_main_image_control_block_ptr_o()                (0x000000c8U)
#define ctxsw_prog_main_image_control_block_ptr_v_f(v)\
				((U32(v) & 0xffffffffU) << 0U)
#define ctxsw_prog_main_image_control_block_ptr_hi_o()             (0x000000ccU)
#define ctxsw_prog_main_image_control_block_ptr_hi_v_f(v)\
				((U32(v) & 0x1ffffU) << 0U)
#define ctxsw_prog_main_image_context_ramchain_buffer_addr_lo_o()  (0x000000e0U)
#define ctxsw_prog_main_image_context_ramchain_buffer_addr_lo_v_f(v)\
				((U32(v) & 0xffffffffU) << 0U)
#define ctxsw_prog_main_image_context_ramchain_buffer_addr_hi_o()  (0x000000e4U)
#define ctxsw_prog_main_image_context_ramchain_buffer_addr_hi_v_f(v)\
				((U32(v) & 0x1ffffU) << 0U)
#define ctxsw_prog_local_image_ppc_info_o()                        (0x000000f4U)
#define ctxsw_prog_local_image_ppc_info_num_ppcs_v(r)    (((r) >> 0U) & 0xffffU)
#define ctxsw_prog_local_image_ppc_info_ppc_mask_v(r)   (((r) >> 16U) & 0xffffU)
#define ctxsw_prog_local_image_num_tpcs_o()                        (0x000000f8U)
#define ctxsw_prog_local_magic_value_o()                           (0x000000fcU)
#define ctxsw_prog_local_magic_value_v_value_v()                   (0xad0becabU)
#define ctxsw_prog_main_extended_buffer_ctl_o()                    (0x000000ecU)
#define ctxsw_prog_main_extended_buffer_ctl_offset_v(r)  (((r) >> 0U) & 0xffffU)
#define ctxsw_prog_main_extended_buffer_ctl_size_v(r)     (((r) >> 16U) & 0xffU)
#define ctxsw_prog_extended_buffer_segments_size_in_bytes_v()      (0x00000100U)
#define ctxsw_prog_extended_marker_size_in_bytes_v()               (0x00000004U)
#define ctxsw_prog_extended_sm_dsm_perf_counter_register_stride_v()\
				(0x00000000U)
#define ctxsw_prog_extended_sm_dsm_perf_counter_control_register_stride_v()\
				(0x00000002U)
#define ctxsw_prog_main_image_priv_access_map_config_o()           (0x000000a0U)
#define ctxsw_prog_main_image_priv_access_map_config_mode_s()               (2U)
#define ctxsw_prog_main_image_priv_access_map_config_mode_f(v)\
				((U32(v) & 0x3U) << 0U)
#define ctxsw_prog_main_image_priv_access_map_config_mode_m()  (U32(0x3U) << 0U)
#define ctxsw_prog_main_image_priv_access_map_config_mode_v(r)\
				(((r) >> 0U) & 0x3U)
#define ctxsw_prog_main_image_priv_access_map_config_mode_allow_all_f()   (0x0U)
#define ctxsw_prog_main_image_priv_access_map_config_mode_use_map_f()     (0x2U)
#define ctxsw_prog_main_image_priv_access_map_addr_lo_o()          (0x000000a4U)
#define ctxsw_prog_main_image_priv_access_map_addr_hi_o()          (0x000000a8U)
#define ctxsw_prog_main_image_misc_options_o()                     (0x0000003cU)
#define ctxsw_prog_main_image_misc_options_verif_features_m()  (U32(0x1U) << 3U)
#define ctxsw_prog_main_image_misc_options_verif_features_disabled_f()    (0x0U)
#define ctxsw_prog_main_image_graphics_preemption_options_o()      (0x00000080U)
#define ctxsw_prog_main_image_graphics_preemption_options_control_f(v)\
				((U32(v) & 0x3U) << 0U)
#define ctxsw_prog_main_image_graphics_preemption_options_control_gfxp_f()\
				(0x1U)
#define ctxsw_prog_main_image_compute_preemption_options_o()       (0x00000084U)
#define ctxsw_prog_main_image_compute_preemption_options_control_f(v)\
				((U32(v) & 0x3U) << 0U)
#define ctxsw_prog_main_image_compute_preemption_options_control_cta_f()  (0x1U)
#define ctxsw_prog_main_image_compute_preemption_options_control_cilp_f() (0x2U)
#define ctxsw_prog_main_image_context_timestamp_buffer_control_o() (0x000000acU)
#define ctxsw_prog_main_image_context_timestamp_buffer_control_num_records_f(v)\
				((U32(v) & 0xffffU) << 0U)
#define ctxsw_prog_main_image_context_timestamp_buffer_ptr_hi_o()  (0x000000b0U)
#define ctxsw_prog_main_image_context_timestamp_buffer_ptr_hi_v_m()\
				(U32(0x1ffffU) << 0U)
#define ctxsw_prog_main_image_context_timestamp_buffer_ptr_o()     (0x000000b4U)
#define ctxsw_prog_main_image_context_timestamp_buffer_ptr_v_f(v)\
				((U32(v) & 0xffffffffU) << 0U)
#define ctxsw_prog_record_timestamp_record_size_in_bytes_v()       (0x00000080U)
#define ctxsw_prog_record_timestamp_record_size_in_words_v()       (0x00000020U)
#define ctxsw_prog_record_timestamp_magic_value_hi_o()             (0x00000004U)
#define ctxsw_prog_record_timestamp_magic_value_hi_v_value_v()     (0x600dbeefU)
#define ctxsw_prog_record_timestamp_timestamp_hi_o()               (0x0000001cU)
#define ctxsw_prog_record_timestamp_timestamp_hi_v_f(v)\
				((U32(v) & 0xffffffU) << 0U)
#define ctxsw_prog_record_timestamp_timestamp_hi_v_v(r)\
				(((r) >> 0U) & 0xffffffU)
#define ctxsw_prog_record_timestamp_timestamp_hi_tag_f(v)\
				((U32(v) & 0xffU) << 24U)
#define ctxsw_prog_record_timestamp_timestamp_hi_tag_m()     (U32(0xffU) << 24U)
#define ctxsw_prog_record_timestamp_timestamp_hi_tag_v(r) (((r) >> 24U) & 0xffU)
#define ctxsw_prog_record_timestamp_timestamp_hi_tag_invalid_timestamp_v()\
				(0x000000ffU)
#define ctxsw_prog_record_timestamp_timestamp_hi_tag_invalid_timestamp_f()\
				(0xff000000U)
#endif
