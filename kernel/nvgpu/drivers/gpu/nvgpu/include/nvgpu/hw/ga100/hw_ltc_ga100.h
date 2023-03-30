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
#ifndef NVGPU_HW_LTC_GA100_H
#define NVGPU_HW_LTC_GA100_H

#include <nvgpu/types.h>
#include <nvgpu/static_analysis.h>

#define ltc_pltcg_base_v()                                         (0x00140000U)
#define ltc_pltcg_extent_v()                                       (0x0017ffffU)
#define ltc_ltc0_ltss_v()                                          (0x00140200U)
#define ltc_ltc0_lts0_v()                                          (0x00140400U)
#define ltc_ltcs_ltss_v()                                          (0x0017e200U)
#define ltc_ltc0_lts0_dstg_cfg0_r()                                (0x00140518U)
#define ltc_ltcs_ltss_dstg_cfg0_r()                                (0x0017e318U)
#define ltc_ltc0_lts0_tstg_cfg1_r()                                (0x00140494U)
#define ltc_ltc0_lts0_tstg_cfg1_active_ways_v(r)         (((r) >> 0U) & 0xffffU)
#define ltc_ltc0_lts0_tstg_cfg1_active_sets_v(r)           (((r) >> 16U) & 0x3U)
#define ltc_ltc0_lts0_tstg_cfg1_active_sets_all_v()                (0x00000000U)
#define ltc_ltc0_lts0_tstg_cfg1_active_sets_half_v()               (0x00000001U)
#define ltc_ltc0_lts0_tstg_cfg1_active_sets_quarter_v()            (0x00000002U)
#define ltc_ltcs_ltss_cbc_ctrl1_r()                                (0x0017e26cU)
#define ltc_ltcs_ltss_cbc_ctrl1_clean_active_f()                          (0x1U)
#define ltc_ltcs_ltss_cbc_ctrl1_invalidate_active_f()                     (0x2U)
#define ltc_ltcs_ltss_cbc_ctrl1_clear_active_f()                          (0x4U)
#define ltc_ltcs_ltss_tstg_set_mgmt0_r()                           (0x0017e2acU)
#define ltc_ltcs_ltss_tstg_set_mgmt0_max_evict_last_f(v)\
				((U32(v) & 0x1fU) << 16U)
#define ltc_ltcs_ltss_tstg_set_mgmt0_max_evict_last_m()      (U32(0x1fU) << 16U)
#define ltc_ltcs_ltss_tstg_set_mgmt0_max_evict_last_v(r)  (((r) >> 16U) & 0x1fU)
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
#define ltc_ltcs_ltss_cbc_num_active_ltcs_r()                      (0x0017e27cU)
#define ltc_ltcs_misc_ltc_num_active_ltcs_r()                      (0x0017e000U)
#define ltc_ltcs_ltss_cbc_param_r()                                (0x0017e280U)
#define ltc_ltcs_ltss_cbc_param_bytes_per_comptagline_per_slice_v(r)\
				(((r) >> 0U) & 0x3ffU)
#define ltc_ltcs_ltss_cbc_param_amap_divide_rounding_v(r)  (((r) >> 10U) & 0x3U)
#define ltc_ltcs_ltss_cbc_param_amap_swizzle_rounding_v(r) (((r) >> 12U) & 0x3U)
#define ltc_ltcs_ltss_cbc_param2_r()                               (0x0017e3f4U)
#define ltc_ltcs_ltss_cbc_param2_gobs_per_comptagline_per_slice_v(r)\
				(((r) >> 0U) & 0xffffU)
#define ltc_ltcs_ltss_cbc_param2_cache_line_size_v(r)      (((r) >> 24U) & 0xfU)
#define ltc_ltcs_ltss_cbc_param2_slices_per_ltc_v(r)       (((r) >> 28U) & 0xfU)
#define ltc_ltcs_ltss_tstg_set_mgmt_3_r()                          (0x0017e2b4U)
#define ltc_ltcs_ltss_tstg_set_mgmt_3_disallow_clean_ce_imm_m()\
				(U32(0x1U) << 23U)
#define ltc_ltcs_ltss_tstg_set_mgmt_3_disallow_clean_ce_imm_enabled_f()\
				(0x800000U)
#define ltc_ltcs_ltss_tstg_set_mgmt_3_disallow_clean_fclr_imm_m()\
				(U32(0x1U) << 21U)
#define ltc_ltcs_ltss_tstg_set_mgmt_3_disallow_clean_fclr_imm_enabled_f()\
				(0x200000U)
#define ltc_ltcs_ltss_dstg_zbc_index_r()                           (0x0017e338U)
#define ltc_ltcs_ltss_dstg_zbc_index_address_f(v)        ((U32(v) & 0xfU) << 0U)
#define ltc_ltcs_ltss_dstg_zbc_color_clear_value_r(i)\
		(nvgpu_safe_add_u32(0x0017e33cU, nvgpu_safe_mult_u32((i), 4U)))
#define ltc_ltcs_ltss_dstg_zbc_color_clear_value__size_1_v()       (0x00000004U)
#define ltc_ltcs_ltss_dstg_zbc_depth_clear_value_r()               (0x0017e34cU)
#define ltc_ltcs_ltss_dstg_zbc_stencil_clear_value_r()             (0x0017e204U)
#define ltc_ltcs_ltss_tstg_set_mgmt_2_r()                          (0x0017e2b0U)
#define ltc_ltcs_ltss_tstg_set_mgmt_2_l2_bypass_mode_enabled_f()   (0x10000000U)
#define ltc_ltcs_ltss_intr_r()                                     (0x0017e20cU)
#define ltc_ltcs_ltss_intr_idle_error_cbc_m()                  (U32(0x1U) << 1U)
#define ltc_ltcs_ltss_intr_idle_error_cbc_reset_f()                       (0x2U)
#define ltc_ltcs_ltss_intr_en_idle_error_cbc_m()              (U32(0x1U) << 17U)
#define ltc_ltcs_ltss_intr_en_idle_error_cbc_enabled_f()              (0x20000U)
#define ltc_ltcs_ltss_intr_idle_error_tstg_m()                 (U32(0x1U) << 2U)
#define ltc_ltcs_ltss_intr_idle_error_tstg_reset_f()                      (0x4U)
#define ltc_ltcs_ltss_intr_en_idle_error_tstg_m()             (U32(0x1U) << 18U)
#define ltc_ltcs_ltss_intr_en_idle_error_tstg_enabled_f()             (0x40000U)
#define ltc_ltcs_ltss_intr_idle_error_dstg_m()                 (U32(0x1U) << 3U)
#define ltc_ltcs_ltss_intr_idle_error_dstg_reset_f()                      (0x8U)
#define ltc_ltcs_ltss_intr_en_idle_error_dstg_m()             (U32(0x1U) << 19U)
#define ltc_ltcs_ltss_intr_en_idle_error_dstg_enabled_f()             (0x80000U)
#define ltc_ltcs_ltss_intr_evicted_cb_m()                      (U32(0x1U) << 4U)
#define ltc_ltcs_ltss_intr_evicted_cb_reset_f()                          (0x10U)
#define ltc_ltcs_ltss_intr_en_evicted_cb_m()                  (U32(0x1U) << 20U)
#define ltc_ltcs_ltss_intr_en_evicted_cb_enabled_f()                 (0x100000U)
#define ltc_ltcs_ltss_intr_illegal_compstat_m()                (U32(0x1U) << 5U)
#define ltc_ltcs_ltss_intr_illegal_compstat_reset_f()                    (0x20U)
#define ltc_ltcs_ltss_intr_en_illegal_compstat_m()            (U32(0x1U) << 21U)
#define ltc_ltcs_ltss_intr_en_illegal_compstat_enabled_f()           (0x200000U)
#define ltc_ltcs_ltss_intr_en_illegal_compstat_disabled_f()               (0x0U)
#define ltc_ltcs_ltss_intr_illegal_atomic_m()                 (U32(0x1U) << 12U)
#define ltc_ltcs_ltss_intr_illegal_atomic_reset_f()                    (0x1000U)
#define ltc_ltcs_ltss_intr_en_illegal_atomic_m()              (U32(0x1U) << 28U)
#define ltc_ltcs_ltss_intr_en_illegal_atomic_enabled_f()           (0x10000000U)
#define ltc_ltcs_ltss_intr_blkactivity_err_m()                (U32(0x1U) << 13U)
#define ltc_ltcs_ltss_intr_blkactivity_err_reset_f()                   (0x2000U)
#define ltc_ltcs_ltss_intr_en_blkactivity_err_m()             (U32(0x1U) << 29U)
#define ltc_ltcs_ltss_intr_en_blkactivity_err_enabled_f()          (0x20000000U)
#define ltc_ltcs_ltss_intr_illegal_compstat_access_m()        (U32(0x1U) << 14U)
#define ltc_ltcs_ltss_intr_illegal_compstat_access_reset_f()           (0x4000U)
#define ltc_ltcs_ltss_intr_en_illegal_compstat_access_m()     (U32(0x1U) << 30U)
#define ltc_ltcs_ltss_intr_en_illegal_compstat_access_enabled_f()  (0x40000000U)
#define ltc_ltcs_ltss_intr2_r()                                    (0x0017e208U)
#define ltc_ltcs_ltss_intr2_trdone_invalid_tdtag_m()           (U32(0x1U) << 0U)
#define ltc_ltcs_ltss_intr2_trdone_invalid_tdtag_reset_f()                (0x1U)
#define ltc_ltcs_ltss_intr2_en_trdone_invalid_tdtag_m()       (U32(0x1U) << 16U)
#define ltc_ltcs_ltss_intr2_en_trdone_invalid_tdtag_enabled_f()       (0x10000U)
#define ltc_ltcs_ltss_intr2_unexpected_trdone_m()              (U32(0x1U) << 1U)
#define ltc_ltcs_ltss_intr2_unexpected_trdone_reset_f()                   (0x2U)
#define ltc_ltcs_ltss_intr2_en_unexpected_trdone_m()          (U32(0x1U) << 17U)
#define ltc_ltcs_ltss_intr2_en_unexpected_trdone_enabled_f()          (0x20000U)
#define ltc_ltcs_ltss_intr2_rwc_upg_unexpected_trdone_data_m() (U32(0x1U) << 2U)
#define ltc_ltcs_ltss_intr2_rwc_upg_unexpected_trdone_data_reset_f()      (0x4U)
#define ltc_ltcs_ltss_intr2_en_rwc_upg_unexpected_trdone_data_m()\
				(U32(0x1U) << 18U)
#define ltc_ltcs_ltss_intr2_en_rwc_upg_unexpected_trdone_data_enabled_f()\
				(0x40000U)
#define ltc_ltcs_ltss_intr2_rwc_upg_unexpected_trdone_cancel_m()\
				(U32(0x1U) << 3U)
#define ltc_ltcs_ltss_intr2_rwc_upg_unexpected_trdone_cancel_reset_f()    (0x8U)
#define ltc_ltcs_ltss_intr2_en_rwc_upg_unexpected_trdone_cancel_m()\
				(U32(0x1U) << 19U)
#define ltc_ltcs_ltss_intr2_en_rwc_upg_unexpected_trdone_cancel_enabled_f()\
				(0x80000U)
#define ltc_ltcs_ltss_intr2_prbrs_invalid_subid_m()            (U32(0x1U) << 4U)
#define ltc_ltcs_ltss_intr2_prbrs_invalid_subid_reset_f()                (0x10U)
#define ltc_ltcs_ltss_intr2_en_prbrs_invalid_subid_m()        (U32(0x1U) << 20U)
#define ltc_ltcs_ltss_intr2_en_prbrs_invalid_subid_enabled_f()       (0x100000U)
#define ltc_ltcs_ltss_intr2_unexpected_prbrs_m()               (U32(0x1U) << 5U)
#define ltc_ltcs_ltss_intr2_unexpected_prbrs_reset_f()                   (0x20U)
#define ltc_ltcs_ltss_intr2_en_unexpected_prbrs_m()           (U32(0x1U) << 21U)
#define ltc_ltcs_ltss_intr2_en_unexpected_prbrs_enabled_f()          (0x200000U)
#define ltc_ltcs_ltss_intr2_prbin_unexpected_prbrs_m()         (U32(0x1U) << 6U)
#define ltc_ltcs_ltss_intr2_prbin_unexpected_prbrs_reset_f()             (0x40U)
#define ltc_ltcs_ltss_intr2_en_prbin_unexpected_prbrs_m()     (U32(0x1U) << 22U)
#define ltc_ltcs_ltss_intr2_en_prbin_unexpected_prbrs_enabled_f()    (0x400000U)
#define ltc_ltcs_ltss_intr2_prbimo_unexpected_prbrs_m()        (U32(0x1U) << 7U)
#define ltc_ltcs_ltss_intr2_prbimo_unexpected_prbrs_reset_f()            (0x80U)
#define ltc_ltcs_ltss_intr2_en_prbimo_unexpected_prbrs_m()    (U32(0x1U) << 23U)
#define ltc_ltcs_ltss_intr2_en_prbimo_unexpected_prbrs_enabled_f()   (0x800000U)
#define ltc_ltcs_ltss_intr2_prbx_missing_data_m()              (U32(0x1U) << 8U)
#define ltc_ltcs_ltss_intr2_prbx_missing_data_reset_f()                 (0x100U)
#define ltc_ltcs_ltss_intr2_en_prbx_missing_data_m()          (U32(0x1U) << 24U)
#define ltc_ltcs_ltss_intr2_en_prbx_missing_data_enabled_f()        (0x1000000U)
#define ltc_ltcs_ltss_intr2_prbx_unexpected_data_m()           (U32(0x1U) << 9U)
#define ltc_ltcs_ltss_intr2_prbx_unexpected_data_reset_f()              (0x200U)
#define ltc_ltcs_ltss_intr2_en_prbx_unexpected_data_m()       (U32(0x1U) << 25U)
#define ltc_ltcs_ltss_intr2_en_prbx_unexpected_data_enabled_f()     (0x2000000U)
#define ltc_ltcs_ltss_intr2_prbrs_unexpected_pa7_m()          (U32(0x1U) << 10U)
#define ltc_ltcs_ltss_intr2_prbrs_unexpected_pa7_reset_f()              (0x400U)
#define ltc_ltcs_ltss_intr2_en_prbrs_unexpected_pa7_m()       (U32(0x1U) << 26U)
#define ltc_ltcs_ltss_intr2_en_prbrs_unexpected_pa7_enabled_f()     (0x4000000U)
#define ltc_ltcs_ltss_intr2_trdone_unexpected_pa7_m()         (U32(0x1U) << 11U)
#define ltc_ltcs_ltss_intr2_trdone_unexpected_pa7_reset_f()             (0x800U)
#define ltc_ltcs_ltss_intr2_en_trdone_unexpected_pa7_m()      (U32(0x1U) << 27U)
#define ltc_ltcs_ltss_intr2_en_trdone_unexpected_pa7_enabled_f()    (0x8000000U)
#define ltc_ltcs_ltss_intr2_sysfill_bypass_invalid_subid_m()  (U32(0x1U) << 12U)
#define ltc_ltcs_ltss_intr2_sysfill_bypass_invalid_subid_reset_f()     (0x1000U)
#define ltc_ltcs_ltss_intr2_en_sysfill_bypass_invalid_subid_m()\
				(U32(0x1U) << 28U)
#define ltc_ltcs_ltss_intr2_en_sysfill_bypass_invalid_subid_enabled_f()\
				(0x10000000U)
#define ltc_ltcs_ltss_intr2_unexpected_sysfill_bypass_m()     (U32(0x1U) << 13U)
#define ltc_ltcs_ltss_intr2_unexpected_sysfill_bypass_reset_f()        (0x2000U)
#define ltc_ltcs_ltss_intr2_en_unexpected_sysfill_bypass_m()  (U32(0x1U) << 29U)
#define ltc_ltcs_ltss_intr2_en_unexpected_sysfill_bypass_enabled_f()\
				(0x20000000U)
#define ltc_ltcs_ltss_intr2_checkedin_unexpected_prbrs_m()    (U32(0x1U) << 14U)
#define ltc_ltcs_ltss_intr2_checkedin_unexpected_prbrs_reset_f()       (0x4000U)
#define ltc_ltcs_ltss_intr2_en_checkedin_unexpected_prbrs_m() (U32(0x1U) << 30U)
#define ltc_ltcs_ltss_intr2_en_checkedin_unexpected_prbrs_enabled_f()\
				(0x40000000U)
#define ltc_ltcs_ltss_intr2_checkedin_unexpected_trdone_m()   (U32(0x1U) << 15U)
#define ltc_ltcs_ltss_intr2_checkedin_unexpected_trdone_reset_f()      (0x8000U)
#define ltc_ltcs_ltss_intr2_en_checkedin_unexpected_trdone_m()\
				(U32(0x1U) << 31U)
#define ltc_ltcs_ltss_intr2_en_checkedin_unexpected_trdone_enabled_f()\
				(0x80000000U)
#define ltc_ltcs_ltss_intr3_r()                                    (0x0017e388U)
#define ltc_ltcs_ltss_intr3_checkedout_rwc_upg_unexpected_nvport_m()\
				(U32(0x1U) << 0U)
#define ltc_ltcs_ltss_intr3_checkedout_rwc_upg_unexpected_nvport_reset_f()\
				(0x1U)
#define ltc_ltcs_ltss_intr3_en_checkedout_rwc_upg_unexpected_nvport_m()\
				(U32(0x1U) << 16U)
#define ltc_ltcs_ltss_intr3_en_checkedout_rwc_upg_unexpected_nvport_enabled_f()\
				(0x10000U)
#define ltc_ltcs_ltss_intr3_checkedout_trdone_unexpected_nvport_m()\
				(U32(0x1U) << 1U)
#define ltc_ltcs_ltss_intr3_checkedout_trdone_unexpected_nvport_reset_f() (0x2U)
#define ltc_ltcs_ltss_intr3_en_checkedout_trdone_unexpected_nvport_m()\
				(U32(0x1U) << 17U)
#define ltc_ltcs_ltss_intr3_en_checkedout_trdone_unexpected_nvport_enabled_f()\
				(0x20000U)
#define ltc_ltcs_ltss_intr3_checkedout_prbrs_unexpected_nvport_m()\
				(U32(0x1U) << 2U)
#define ltc_ltcs_ltss_intr3_checkedout_prbrs_unexpected_nvport_reset_f()  (0x4U)
#define ltc_ltcs_ltss_intr3_en_checkedout_prbrs_unexpected_nvport_m()\
				(U32(0x1U) << 18U)
#define ltc_ltcs_ltss_intr3_en_checkedout_prbrs_unexpected_nvport_enabled_f()\
				(0x40000U)
#define ltc_ltcs_ltss_intr3_checkedout_ninb_ncnp_req_m()       (U32(0x1U) << 3U)
#define ltc_ltcs_ltss_intr3_checkedout_ninb_ncnp_req_reset_f()            (0x8U)
#define ltc_ltcs_ltss_intr3_en_checkedout_ninb_ncnp_req_m()   (U32(0x1U) << 19U)
#define ltc_ltcs_ltss_intr3_en_checkedout_ninb_ncnp_req_enabled_f()   (0x80000U)
#define ltc_ltcs_ltss_intr3_checkedout_creq_ncnp_req_m()       (U32(0x1U) << 4U)
#define ltc_ltcs_ltss_intr3_checkedout_creq_ncnp_req_reset_f()           (0x10U)
#define ltc_ltcs_ltss_intr3_en_checkedout_creq_ncnp_req_m()   (U32(0x1U) << 20U)
#define ltc_ltcs_ltss_intr3_en_checkedout_creq_ncnp_req_enabled_f()  (0x100000U)
#define ltc_ltcs_ltss_intr3_rmwrs_invalid_subid_m()            (U32(0x1U) << 5U)
#define ltc_ltcs_ltss_intr3_rmwrs_invalid_subid_reset_f()                (0x20U)
#define ltc_ltcs_ltss_intr3_en_rmwrs_invalid_subid_m()        (U32(0x1U) << 21U)
#define ltc_ltcs_ltss_intr3_en_rmwrs_invalid_subid_enabled_f()       (0x200000U)
#define ltc_ltcs_ltss_intr3_unexpected_rmwrs_m()               (U32(0x1U) << 6U)
#define ltc_ltcs_ltss_intr3_unexpected_rmwrs_reset_f()                   (0x40U)
#define ltc_ltcs_ltss_intr3_en_unexpected_rmwrs_m()           (U32(0x1U) << 22U)
#define ltc_ltcs_ltss_intr3_en_unexpected_rmwrs_enabled_f()          (0x400000U)
#define ltc_ltcs_ltss_intr3_ecc_corrected_m()                  (U32(0x1U) << 7U)
#define ltc_ltcs_ltss_intr3_ecc_uncorrected_m()                (U32(0x1U) << 8U)
#define ltc_ltcs_ltss_intr3_illegal_access_kind_type1_m()     (U32(0x1U) << 10U)
#define ltc_ltcs_ltss_intr3_illegal_access_kind_type1_reset_f()         (0x400U)
#define ltc_ltcs_ltss_intr3_en_illegal_access_kind_type1_m()  (U32(0x1U) << 26U)
#define ltc_ltcs_ltss_intr3_en_illegal_access_kind_type1_enabled_f()\
				(0x4000000U)
#define ltc_ltcs_ltss_intr3_illegal_access_kind_type2_m()     (U32(0x1U) << 11U)
#define ltc_ltcs_ltss_intr3_illegal_access_kind_type2_reset_f()         (0x800U)
#define ltc_ltcs_ltss_intr3_en_illegal_access_kind_type2_m()  (U32(0x1U) << 27U)
#define ltc_ltcs_ltss_intr3_en_illegal_access_kind_type2_enabled_f()\
				(0x8000000U)
#define ltc_ltc0_lts0_intr_r()                                     (0x0014040cU)
#define ltc_ltc0_lts0_intr2_r()                                    (0x00140408U)
#define ltc_ltc0_lts0_intr3_r()                                    (0x00140588U)
#define ltc_ltcs_ltss_tstg_cmgmt0_r()                              (0x0017e2a0U)
#define ltc_ltcs_ltss_tstg_cmgmt0_invalidate_pending_f()                  (0x1U)
#define ltc_ltcs_ltss_tstg_cmgmt0_max_cycles_between_invalidates_3_f()  (0x300U)
#define ltc_ltcs_ltss_tstg_cmgmt0_invalidate_evict_last_class_true_f()\
				(0x10000000U)
#define ltc_ltcs_ltss_tstg_cmgmt0_invalidate_evict_normal_class_true_f()\
				(0x20000000U)
#define ltc_ltcs_ltss_tstg_cmgmt0_invalidate_evict_first_class_true_f()\
				(0x40000000U)
#define ltc_ltcs_ltss_tstg_cmgmt1_r()                              (0x0017e2a4U)
#define ltc_ltcs_ltss_tstg_cmgmt1_clean_pending_f()                       (0x1U)
#define ltc_ltcs_ltss_tstg_cmgmt1_max_cycles_between_cleans_3_f()       (0x300U)
#define ltc_ltcs_ltss_tstg_cmgmt1_clean_wait_for_fb_to_pull_true_f()  (0x10000U)
#define ltc_ltcs_ltss_tstg_cmgmt1_clean_evict_last_class_true_f()  (0x10000000U)
#define ltc_ltcs_ltss_tstg_cmgmt1_clean_evict_normal_class_true_f()\
				(0x20000000U)
#define ltc_ltcs_ltss_tstg_cmgmt1_clean_evict_first_class_true_f() (0x40000000U)
#define ltc_ltc0_ltss_tstg_cmgmt0_r()                              (0x001402a0U)
#define ltc_ltc0_ltss_tstg_cmgmt0_invalidate_pending_f()                  (0x1U)
#define ltc_ltc0_ltss_tstg_cmgmt1_r()                              (0x001402a4U)
#define ltc_ltc0_ltss_tstg_cmgmt1_clean_pending_f()                       (0x1U)
#define ltc_ltc0_lts0_tstg_info_1_r()                              (0x0014058cU)
#define ltc_ltc0_lts0_tstg_info_1_slice_size_in_kb_v(r)  (((r) >> 0U) & 0xffffU)
#define ltc_ltc0_lts0_tstg_info_1_slices_per_l2_v(r)      (((r) >> 16U) & 0x1fU)
#define ltc_ltcs_ltss_tstg_set_mgmt_1_r()                          (0x0017e39cU)
#define ltc_ltcs_ltss_tstg_set_mgmt_1_plc_recompress_plc_m()   (U32(0x1U) << 7U)
#define ltc_ltcs_ltss_tstg_set_mgmt_1_plc_recompress_plc_disabled_f()     (0x0U)
#define ltc_ltcs_ltss_tstg_set_mgmt_1_plc_recompress_plc_enabled_f()     (0x80U)
#define ltc_ltcs_ltss_tstg_set_mgmt_1_plc_recompress_rmw_m()  (U32(0x1U) << 29U)
#define ltc_ltcs_ltss_tstg_set_mgmt_1_plc_recompress_rmw_disabled_f()     (0x0U)
#define ltc_ltcs_ltss_tstg_set_mgmt_1_plc_recompress_rmw_enabled_f()\
				(0x20000000U)
#define ltc_ltc0_lts0_l2_cache_ecc_address_r()                     (0x001404fcU)
#define ltc_ltc0_lts0_l2_cache_ecc_address_ram_v(r)       (((r) >> 22U) & 0xffU)
#define ltc_ltc0_lts0_l2_cache_ecc_address_ram_dstg_db_clrbe_trlram0_v()\
				(0x00000000U)
#define ltc_ltc0_lts0_l2_cache_ecc_address_ram_dstg_db_clrbe_trlram1_v()\
				(0x00000001U)
#define ltc_ltc0_lts0_l2_cache_ecc_address_ram_dstg_db_clrbe_trlram2_v()\
				(0x00000002U)
#define ltc_ltc0_lts0_l2_cache_ecc_address_ram_dstg_db_clrbe_trlram3_v()\
				(0x00000003U)
#define ltc_ltc0_lts0_l2_cache_ecc_address_ram_dstg_db_clrbe_trlram4_v()\
				(0x00000004U)
#define ltc_ltc0_lts0_l2_cache_ecc_address_ram_dstg_db_clrbe_trlram5_v()\
				(0x00000005U)
#define ltc_ltc0_lts0_l2_cache_ecc_address_ram_dstg_db_clrbe_trlram6_v()\
				(0x00000006U)
#define ltc_ltc0_lts0_l2_cache_ecc_address_ram_dstg_db_clrbe_trlram7_v()\
				(0x00000007U)
#define ltc_ltc0_lts0_l2_cache_ecc_address_ram_dstg_db_bank0_v()   (0x00000008U)
#define ltc_ltc0_lts0_l2_cache_ecc_address_ram_dstg_db_bank1_v()   (0x00000009U)
#define ltc_ltc0_lts0_l2_cache_ecc_address_ram_dstg_db_bank2_v()   (0x0000000aU)
#define ltc_ltc0_lts0_l2_cache_ecc_address_ram_dstg_db_bank3_v()   (0x0000000bU)
#define ltc_ltc0_lts0_l2_cache_ecc_address_subunit_v(r)    (((r) >> 30U) & 0x3U)
#define ltc_ltc0_lts0_l2_cache_ecc_address_subunit_rstg_v()        (0x00000000U)
#define ltc_ltc0_lts0_l2_cache_ecc_address_subunit_tstg_v()        (0x00000001U)
#define ltc_ltc0_lts0_l2_cache_ecc_address_subunit_dstg_v()        (0x00000002U)
#define ltc_ltc0_lts0_l2_cache_ecc_corrected_err_count_r()         (0x001404f4U)
#define ltc_ltc0_lts0_l2_cache_ecc_corrected_err_count_total_s()           (16U)
#define ltc_ltc0_lts0_l2_cache_ecc_corrected_err_count_total_m()\
				(U32(0xffffU) << 0U)
#define ltc_ltc0_lts0_l2_cache_ecc_corrected_err_count_total_v(r)\
				(((r) >> 0U) & 0xffffU)
#define ltc_ltc0_lts0_l2_cache_ecc_uncorrected_err_count_r()       (0x001404f8U)
#define ltc_ltc0_lts0_l2_cache_ecc_uncorrected_err_count_total_s()         (16U)
#define ltc_ltc0_lts0_l2_cache_ecc_uncorrected_err_count_total_m()\
				(U32(0xffffU) << 0U)
#define ltc_ltc0_lts0_l2_cache_ecc_uncorrected_err_count_total_v(r)\
				(((r) >> 0U) & 0xffffU)
#define ltc_ltc0_lts0_l2_cache_ecc_control_r()                     (0x001404ecU)
#define ltc_ltc0_lts0_l2_cache_ecc_control_inject_corrected_err_f(v)\
				((U32(v) & 0x1U) << 4U)
#define ltc_ltc0_lts0_l2_cache_ecc_control_inject_uncorrected_err_f(v)\
				((U32(v) & 0x1U) << 5U)
#define ltc_ltc0_lts0_l2_cache_ecc_control_inject_corrected_err_rstg_f(v)\
				((U32(v) & 0x1U) << 6U)
#define ltc_ltc0_lts0_l2_cache_ecc_control_inject_uncorrected_err_rstg_f(v)\
				((U32(v) & 0x1U) << 7U)
#define ltc_ltc0_lts0_l2_cache_ecc_control_inject_corrected_err_tstg_f(v)\
				((U32(v) & 0x1U) << 8U)
#define ltc_ltc0_lts0_l2_cache_ecc_control_inject_uncorrected_err_tstg_f(v)\
				((U32(v) & 0x1U) << 9U)
#define ltc_ltc0_lts0_l2_cache_ecc_control_inject_corrected_err_dstg_f(v)\
				((U32(v) & 0x1U) << 10U)
#define ltc_ltc0_lts0_l2_cache_ecc_control_inject_uncorrected_err_dstg_f(v)\
				((U32(v) & 0x1U) << 11U)
#define ltc_ltc0_lts0_l2_cache_ecc_status_r()                      (0x001404f0U)
#define ltc_ltc0_lts0_l2_cache_ecc_status_uncorrected_err_rstg_m()\
				(U32(0x1U) << 0U)
#define ltc_ltc0_lts0_l2_cache_ecc_status_corrected_err_rstg_m()\
				(U32(0x1U) << 1U)
#define ltc_ltc0_lts0_l2_cache_ecc_status_uncorrected_err_tstg_m()\
				(U32(0x1U) << 2U)
#define ltc_ltc0_lts0_l2_cache_ecc_status_corrected_err_tstg_m()\
				(U32(0x1U) << 3U)
#define ltc_ltc0_lts0_l2_cache_ecc_status_uncorrected_err_dstg_m()\
				(U32(0x1U) << 4U)
#define ltc_ltc0_lts0_l2_cache_ecc_status_corrected_err_dstg_m()\
				(U32(0x1U) << 5U)
#define ltc_ltc0_lts0_l2_cache_ecc_status_corrected_err_total_counter_overflow_m()\
				(U32(0x1U) << 16U)
#define ltc_ltc0_lts0_l2_cache_ecc_status_uncorrected_err_total_counter_overflow_m()\
				(U32(0x1U) << 18U)
#define ltc_ltc0_lts0_l2_cache_ecc_status_reset_task_f()           (0x40000000U)
#define ltc_ltc0_lts0_intr3_r()                                    (0x00140588U)
#endif
