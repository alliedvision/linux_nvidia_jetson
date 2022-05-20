/*
 * Copyright (c) 2018-2021, NVIDIA CORPORATION.  All rights reserved.
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
#ifndef NVGPU_HW_LTC_TU104_H
#define NVGPU_HW_LTC_TU104_H

#include <nvgpu/types.h>
#include <nvgpu/static_analysis.h>

#define ltc_pltcg_base_v()                                         (0x00140000U)
#define ltc_pltcg_extent_v()                                       (0x0017ffffU)
#define ltc_ltc0_ltss_v()                                          (0x00140200U)
#define ltc_ltc0_lts0_v()                                          (0x00140400U)
#define ltc_ltcs_ltss_v()                                          (0x0017e200U)
#define ltc_ltcs_lts0_cbc_ctrl1_r()                                (0x0014046cU)
#define ltc_ltc0_lts0_dstg_cfg0_r()                                (0x00140518U)
#define ltc_ltcs_ltss_dstg_cfg0_r()                                (0x0017e318U)
#define ltc_ltcs_ltss_dstg_cfg0_vdc_4to2_disable_m()          (U32(0x1U) << 15U)
#define ltc_ltc0_lts0_tstg_cfg1_r()                                (0x00140494U)
#define ltc_ltc0_lts0_tstg_cfg1_active_ways_v(r)         (((r) >> 0U) & 0xffffU)
#define ltc_ltc0_lts0_tstg_cfg1_active_sets_v(r)           (((r) >> 16U) & 0x3U)
#define ltc_ltc0_lts0_tstg_cfg1_active_sets_all_v()                (0x00000000U)
#define ltc_ltc0_lts0_tstg_cfg1_active_sets_half_v()               (0x00000001U)
#define ltc_ltc0_lts0_tstg_cfg1_active_sets_quarter_v()            (0x00000002U)
#define ltc_ltcs_ltss_cbc_ctrl1_r()                                (0x0017e26cU)
#define ltc_ltcs_ltss_cbc_ctrl1_clean_active_f()                          (0x1U)
#define ltc_ltcs_ltss_cbc_ctrl1_invalidate_active_f()                     (0x2U)
#define ltc_ltcs_ltss_cbc_ctrl1_clear_v(r)                  (((r) >> 2U) & 0x1U)
#define ltc_ltcs_ltss_cbc_ctrl1_clear_active_v()                   (0x00000001U)
#define ltc_ltcs_ltss_cbc_ctrl1_clear_active_f()                          (0x4U)
#define ltc_ltc0_lts0_cbc_ctrl1_r()                                (0x0014046cU)
#define ltc_ltcs_ltss_cbc_ctrl2_r()                                (0x0017e270U)
#define ltc_ltcs_ltss_cbc_ctrl2_clear_lower_bound_f(v)\
				((U32(v) & 0xfffffU) << 0U)
#define ltc_ltcs_ltss_cbc_ctrl3_r()                                (0x0017e274U)
#define ltc_ltcs_ltss_cbc_ctrl3_clear_upper_bound_f(v)\
				((U32(v) & 0xfffffU) << 0U)
#define ltc_ltcs_ltss_cbc_ctrl3_clear_upper_bound_init_v()         (0x000fffffU)
#define ltc_ltcs_ltss_cbc_base_r()                                 (0x0017e278U)
#define ltc_ltcs_ltss_cbc_base_alignment_shift_v()                 (0x0000000bU)
#define ltc_ltcs_ltss_cbc_base_address_v(r)           (((r) >> 0U) & 0x3ffffffU)
#define ltc_ltcs_ltss_cbc_num_active_ltcs_r()                      (0x0017e27cU)
#define ltc_ltcs_ltss_cbc_num_active_ltcs__v(r)            (((r) >> 0U) & 0x1fU)
#define ltc_ltcs_ltss_cbc_num_active_ltcs_nvlink_peer_through_l2_f(v)\
				((U32(v) & 0x1U) << 24U)
#define ltc_ltcs_ltss_cbc_num_active_ltcs_nvlink_peer_through_l2_v(r)\
				(((r) >> 24U) & 0x1U)
#define ltc_ltcs_ltss_cbc_num_active_ltcs_serialize_f(v)\
				((U32(v) & 0x1U) << 25U)
#define ltc_ltcs_ltss_cbc_num_active_ltcs_serialize_v(r)   (((r) >> 25U) & 0x1U)
#define ltc_ltcs_misc_ltc_num_active_ltcs_r()                      (0x0017e000U)
#define ltc_ltcs_ltss_cbc_param_r()                                (0x0017e280U)
#define ltc_ltcs_ltss_cbc_param_bytes_per_comptagline_per_slice_v(r)\
				(((r) >> 0U) & 0x3ffU)
#define ltc_ltcs_ltss_cbc_param_amap_divide_rounding_v(r)  (((r) >> 10U) & 0x3U)
#define ltc_ltcs_ltss_cbc_param_amap_swizzle_rounding_v(r) (((r) >> 12U) & 0x3U)
#define ltc_ltcs_ltss_cbc_param2_r()                               (0x0017e3f4U)
#define ltc_ltcs_ltss_cbc_param2_gobs_per_comptagline_per_slice_v(r)\
				(((r) >> 0U) & 0xffffU)
#define ltc_ltcs_ltss_cbc_param2_num_cache_lines_v(r)     (((r) >> 16U) & 0xffU)
#define ltc_ltcs_ltss_cbc_param2_cache_line_size_v(r)      (((r) >> 24U) & 0xfU)
#define ltc_ltcs_ltss_cbc_param2_slices_per_ltc_v(r)       (((r) >> 28U) & 0xfU)
#define ltc_ltcs_ltss_tstg_set_mgmt_r()                            (0x0017e2acU)
#define ltc_ltcs_ltss_tstg_set_mgmt_max_ways_evict_last_f(v)\
				((U32(v) & 0x1fU) << 16U)
#define ltc_ltcs_ltss_dstg_zbc_index_r()                           (0x0017e338U)
#define ltc_ltcs_ltss_dstg_zbc_index_address_f(v)        ((U32(v) & 0xfU) << 0U)
#define ltc_ltcs_ltss_dstg_zbc_color_clear_value_r(i)\
		(nvgpu_safe_add_u32(0x0017e33cU, nvgpu_safe_mult_u32((i), 4U)))
#define ltc_ltcs_ltss_dstg_zbc_color_clear_value__size_1_v()       (0x00000004U)
#define ltc_ltcs_ltss_dstg_zbc_depth_clear_value_r()               (0x0017e34cU)
#define ltc_ltcs_ltss_dstg_zbc_depth_clear_value_field_s()                 (32U)
#define ltc_ltcs_ltss_dstg_zbc_depth_clear_value_field_f(v)\
				((U32(v) & 0xffffffffU) << 0U)
#define ltc_ltcs_ltss_dstg_zbc_depth_clear_value_field_m()\
				(U32(0xffffffffU) << 0U)
#define ltc_ltcs_ltss_dstg_zbc_depth_clear_value_field_v(r)\
				(((r) >> 0U) & 0xffffffffU)
#define ltc_ltcs_ltss_dstg_zbc_stencil_clear_value_r()             (0x0017e204U)
#define ltc_ltcs_ltss_dstg_zbc_stencil_clear_value_field_s()                (8U)
#define ltc_ltcs_ltss_dstg_zbc_stencil_clear_value_field_f(v)\
				((U32(v) & 0xffU) << 0U)
#define ltc_ltcs_ltss_dstg_zbc_stencil_clear_value_field_m()  (U32(0xffU) << 0U)
#define ltc_ltcs_ltss_dstg_zbc_stencil_clear_value_field_v(r)\
				(((r) >> 0U) & 0xffU)
#define ltc_ltcs_ltss_tstg_set_mgmt_2_r()                          (0x0017e2b0U)
#define ltc_ltcs_ltss_tstg_set_mgmt_2_l2_bypass_mode_enabled_f()   (0x10000000U)
#define ltc_ltcs_ltss_g_elpg_r()                                   (0x0017e214U)
#define ltc_ltcs_ltss_g_elpg_flush_v(r)                     (((r) >> 0U) & 0x1U)
#define ltc_ltcs_ltss_g_elpg_flush_pending_v()                     (0x00000001U)
#define ltc_ltcs_ltss_g_elpg_flush_pending_f()                            (0x1U)
#define ltc_ltc0_ltss_g_elpg_r()                                   (0x00140214U)
#define ltc_ltc0_ltss_g_elpg_flush_v(r)                     (((r) >> 0U) & 0x1U)
#define ltc_ltc0_ltss_g_elpg_flush_pending_v()                     (0x00000001U)
#define ltc_ltc0_ltss_g_elpg_flush_pending_f()                            (0x1U)
#define ltc_ltcs_ltss_intr_r()                                     (0x0017e20cU)
#define ltc_ltcs_ltss_intr_ecc_sec_error_pending_f()                    (0x100U)
#define ltc_ltcs_ltss_intr_ecc_ded_error_pending_f()                    (0x200U)
#define ltc_ltcs_ltss_intr_en_evicted_cb_m()                  (U32(0x1U) << 20U)
#define ltc_ltcs_ltss_intr_en_illegal_compstat_m()            (U32(0x1U) << 21U)
#define ltc_ltcs_ltss_intr_en_illegal_compstat_enabled_f()           (0x200000U)
#define ltc_ltcs_ltss_intr_en_illegal_compstat_disabled_f()               (0x0U)
#define ltc_ltcs_ltss_intr_en_illegal_compstat_access_m()     (U32(0x1U) << 30U)
#define ltc_ltcs_ltss_intr_en_ecc_sec_error_enabled_f()             (0x1000000U)
#define ltc_ltcs_ltss_intr_en_ecc_ded_error_enabled_f()             (0x2000000U)
#define ltc_ltc0_lts0_intr_r()                                     (0x0014040cU)
#define ltc_ltc0_lts0_dstg_ecc_report_r()                          (0x0014051cU)
#define ltc_ltc0_lts0_dstg_ecc_report_sec_count_m()           (U32(0xffU) << 0U)
#define ltc_ltc0_lts0_dstg_ecc_report_sec_count_v(r)       (((r) >> 0U) & 0xffU)
#define ltc_ltc0_lts0_dstg_ecc_report_ded_count_m()          (U32(0xffU) << 16U)
#define ltc_ltc0_lts0_dstg_ecc_report_ded_count_v(r)      (((r) >> 16U) & 0xffU)
#define ltc_ltcs_ltss_tstg_cmgmt0_r()                              (0x0017e2a0U)
#define ltc_ltcs_ltss_tstg_cmgmt0_invalidate_v(r)           (((r) >> 0U) & 0x1U)
#define ltc_ltcs_ltss_tstg_cmgmt0_invalidate_pending_v()           (0x00000001U)
#define ltc_ltcs_ltss_tstg_cmgmt0_invalidate_pending_f()                  (0x1U)
#define ltc_ltcs_ltss_tstg_cmgmt0_max_cycles_between_invalidates_v(r)\
				(((r) >> 8U) & 0xfU)
#define ltc_ltcs_ltss_tstg_cmgmt0_max_cycles_between_invalidates_3_v()\
				(0x00000003U)
#define ltc_ltcs_ltss_tstg_cmgmt0_max_cycles_between_invalidates_3_f()  (0x300U)
#define ltc_ltcs_ltss_tstg_cmgmt0_invalidate_evict_last_class_v(r)\
				(((r) >> 28U) & 0x1U)
#define ltc_ltcs_ltss_tstg_cmgmt0_invalidate_evict_last_class_true_v()\
				(0x00000001U)
#define ltc_ltcs_ltss_tstg_cmgmt0_invalidate_evict_last_class_true_f()\
				(0x10000000U)
#define ltc_ltcs_ltss_tstg_cmgmt0_invalidate_evict_normal_class_v(r)\
				(((r) >> 29U) & 0x1U)
#define ltc_ltcs_ltss_tstg_cmgmt0_invalidate_evict_normal_class_true_v()\
				(0x00000001U)
#define ltc_ltcs_ltss_tstg_cmgmt0_invalidate_evict_normal_class_true_f()\
				(0x20000000U)
#define ltc_ltcs_ltss_tstg_cmgmt0_invalidate_evict_first_class_v(r)\
				(((r) >> 30U) & 0x1U)
#define ltc_ltcs_ltss_tstg_cmgmt0_invalidate_evict_first_class_true_v()\
				(0x00000001U)
#define ltc_ltcs_ltss_tstg_cmgmt0_invalidate_evict_first_class_true_f()\
				(0x40000000U)
#define ltc_ltcs_ltss_tstg_cmgmt1_r()                              (0x0017e2a4U)
#define ltc_ltcs_ltss_tstg_cmgmt1_clean_v(r)                (((r) >> 0U) & 0x1U)
#define ltc_ltcs_ltss_tstg_cmgmt1_clean_pending_v()                (0x00000001U)
#define ltc_ltcs_ltss_tstg_cmgmt1_clean_pending_f()                       (0x1U)
#define ltc_ltcs_ltss_tstg_cmgmt1_max_cycles_between_cleans_v(r)\
				(((r) >> 8U) & 0xfU)
#define ltc_ltcs_ltss_tstg_cmgmt1_max_cycles_between_cleans_3_v()  (0x00000003U)
#define ltc_ltcs_ltss_tstg_cmgmt1_max_cycles_between_cleans_3_f()       (0x300U)
#define ltc_ltcs_ltss_tstg_cmgmt1_clean_wait_for_fb_to_pull_v(r)\
				(((r) >> 16U) & 0x1U)
#define ltc_ltcs_ltss_tstg_cmgmt1_clean_wait_for_fb_to_pull_true_v()\
				(0x00000001U)
#define ltc_ltcs_ltss_tstg_cmgmt1_clean_wait_for_fb_to_pull_true_f()  (0x10000U)
#define ltc_ltcs_ltss_tstg_cmgmt1_clean_evict_last_class_v(r)\
				(((r) >> 28U) & 0x1U)
#define ltc_ltcs_ltss_tstg_cmgmt1_clean_evict_last_class_true_v()  (0x00000001U)
#define ltc_ltcs_ltss_tstg_cmgmt1_clean_evict_last_class_true_f()  (0x10000000U)
#define ltc_ltcs_ltss_tstg_cmgmt1_clean_evict_normal_class_v(r)\
				(((r) >> 29U) & 0x1U)
#define ltc_ltcs_ltss_tstg_cmgmt1_clean_evict_normal_class_true_v()\
				(0x00000001U)
#define ltc_ltcs_ltss_tstg_cmgmt1_clean_evict_normal_class_true_f()\
				(0x20000000U)
#define ltc_ltcs_ltss_tstg_cmgmt1_clean_evict_first_class_v(r)\
				(((r) >> 30U) & 0x1U)
#define ltc_ltcs_ltss_tstg_cmgmt1_clean_evict_first_class_true_v() (0x00000001U)
#define ltc_ltcs_ltss_tstg_cmgmt1_clean_evict_first_class_true_f() (0x40000000U)
#define ltc_ltc0_ltss_tstg_cmgmt0_r()                              (0x001402a0U)
#define ltc_ltc0_ltss_tstg_cmgmt0_invalidate_v(r)           (((r) >> 0U) & 0x1U)
#define ltc_ltc0_ltss_tstg_cmgmt0_invalidate_pending_v()           (0x00000001U)
#define ltc_ltc0_ltss_tstg_cmgmt0_invalidate_pending_f()                  (0x1U)
#define ltc_ltc0_ltss_tstg_cmgmt1_r()                              (0x001402a4U)
#define ltc_ltc0_ltss_tstg_cmgmt1_clean_v(r)                (((r) >> 0U) & 0x1U)
#define ltc_ltc0_ltss_tstg_cmgmt1_clean_pending_v()                (0x00000001U)
#define ltc_ltc0_ltss_tstg_cmgmt1_clean_pending_f()                       (0x1U)
#define ltc_ltc0_lts0_tstg_info_1_r()                              (0x0014058cU)
#define ltc_ltc0_lts0_tstg_info_1_slice_size_in_kb_v(r)  (((r) >> 0U) & 0xffffU)
#define ltc_ltc0_lts0_tstg_info_1_slices_per_l2_v(r)      (((r) >> 16U) & 0x1fU)
#define ltc_ltcs_ltss_tstg_set_mgmt_1_r()                          (0x0017e39cU)
#define ltc_ltcs_ltss_tstg_set_mgmt_1_plc_recompress_plc_m()   (U32(0x1U) << 7U)
#define ltc_ltcs_ltss_tstg_set_mgmt_1_plc_recompress_plc_disabled_f()     (0x0U)
#define ltc_ltcs_ltss_tstg_set_mgmt_1_plc_recompress_rmw_m()  (U32(0x1U) << 29U)
#define ltc_ltcs_ltss_tstg_set_mgmt_1_plc_recompress_rmw_disabled_f()     (0x0U)
#define ltc_ltcs_ltss_tstg_cfg2_r()                                (0x0017e298U)
#define ltc_ltcs_ltss_tstg_cfg2_vidmem_l1_promote_f(v)  ((U32(v) & 0x3U) << 16U)
#define ltc_ltcs_ltss_tstg_cfg2_vidmem_l1_promote_m()         (U32(0x3U) << 16U)
#define ltc_ltcs_ltss_tstg_cfg2_vidmem_l1_promote_v(r)     (((r) >> 16U) & 0x3U)
#define ltc_ltcs_ltss_tstg_cfg2_vidmem_l1_promote_none_v()         (0x00000000U)
#define ltc_ltcs_ltss_tstg_cfg2_vidmem_l1_promote_64b_v()          (0x00000001U)
#define ltc_ltcs_ltss_tstg_cfg2_vidmem_l1_promote_128b_v()         (0x00000002U)
#define ltc_ltcs_ltss_tstg_cfg2_vidmem_t1_promote_f(v)  ((U32(v) & 0x3U) << 18U)
#define ltc_ltcs_ltss_tstg_cfg2_vidmem_t1_promote_m()         (U32(0x3U) << 18U)
#define ltc_ltcs_ltss_tstg_cfg2_vidmem_t1_promote_v(r)     (((r) >> 18U) & 0x3U)
#define ltc_ltcs_ltss_tstg_cfg2_vidmem_t1_promote_none_v()         (0x00000000U)
#define ltc_ltcs_ltss_tstg_cfg2_vidmem_t1_promote_64b_v()          (0x00000001U)
#define ltc_ltcs_ltss_tstg_cfg2_vidmem_t1_promote_128b_v()         (0x00000002U)
#define ltc_ltcs_ltss_tstg_cfg3_r()                                (0x0017e29cU)
#define ltc_ltcs_ltss_tstg_cfg3_sysmem_l1_promote_f(v)  ((U32(v) & 0x3U) << 16U)
#define ltc_ltcs_ltss_tstg_cfg3_sysmem_l1_promote_m()         (U32(0x3U) << 16U)
#define ltc_ltcs_ltss_tstg_cfg3_sysmem_l1_promote_v(r)     (((r) >> 16U) & 0x3U)
#define ltc_ltcs_ltss_tstg_cfg3_sysmem_l1_promote_none_v()         (0x00000000U)
#define ltc_ltcs_ltss_tstg_cfg3_sysmem_l1_promote_64b_v()          (0x00000001U)
#define ltc_ltcs_ltss_tstg_cfg3_sysmem_l1_promote_128b_v()         (0x00000002U)
#define ltc_ltcs_ltss_tstg_cfg3_sysmem_t1_promote_f(v)  ((U32(v) & 0x3U) << 18U)
#define ltc_ltcs_ltss_tstg_cfg3_sysmem_t1_promote_m()         (U32(0x3U) << 18U)
#define ltc_ltcs_ltss_tstg_cfg3_sysmem_t1_promote_v(r)     (((r) >> 18U) & 0x3U)
#define ltc_ltcs_ltss_tstg_cfg3_sysmem_t1_promote_none_v()         (0x00000000U)
#define ltc_ltcs_ltss_tstg_cfg3_sysmem_t1_promote_64b_v()          (0x00000001U)
#define ltc_ltcs_ltss_tstg_cfg3_sysmem_t1_promote_128b_v()         (0x00000002U)
#endif
