/*
 * Copyright (c) 2018-2020, NVIDIA CORPORATION.  All rights reserved.
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
#ifndef NVGPU_HW_TRIM_TU104_H
#define NVGPU_HW_TRIM_TU104_H

#include <nvgpu/types.h>
#include <nvgpu/static_analysis.h>

#define trim_sys_nvlink_uphy_cfg_r()                               (0x00132410U)
#define trim_sys_nvlink_uphy_cfg_lockdect_wait_dly_length_f(v)\
				((U32(v) & 0x3ffU) << 0U)
#define trim_sys_nvlink_uphy_cfg_lockdect_wait_dly_length_m()\
				(U32(0x3ffU) << 0U)
#define trim_sys_nvlink_uphy_cfg_lockdect_wait_dly_length_v(r)\
				(((r) >> 0U) & 0x3ffU)
#define trim_sys_nvlink_uphy_cfg_phy2clks_use_lockdet_f(v)\
				((U32(v) & 0x1U) << 12U)
#define trim_sys_nvlink_uphy_cfg_phy2clks_use_lockdet_m()     (U32(0x1U) << 12U)
#define trim_sys_nvlink_uphy_cfg_phy2clks_use_lockdet_v(r) (((r) >> 12U) & 0x1U)
#define trim_sys_nvlink_uphy_cfg_nvlink_wait_dly_f(v)  ((U32(v) & 0xffU) << 16U)
#define trim_sys_nvlink_uphy_cfg_nvlink_wait_dly_m()         (U32(0xffU) << 16U)
#define trim_sys_nvlink_uphy_cfg_nvlink_wait_dly_v(r)     (((r) >> 16U) & 0xffU)
#define trim_sys_nvlink0_ctrl_r()                                  (0x00132420U)
#define trim_sys_nvlink0_ctrl_unit2clks_pll_turn_off_f(v)\
				((U32(v) & 0x1U) << 0U)
#define trim_sys_nvlink0_ctrl_unit2clks_pll_turn_off_m()       (U32(0x1U) << 0U)
#define trim_sys_nvlink0_ctrl_unit2clks_pll_turn_off_v(r)   (((r) >> 0U) & 0x1U)
#define trim_sys_nvlink0_status_r()                                (0x00132424U)
#define trim_sys_nvlink0_status_pll_off_f(v)             ((U32(v) & 0x1U) << 5U)
#define trim_sys_nvlink0_status_pll_off_m()                    (U32(0x1U) << 5U)
#define trim_sys_nvlink0_status_pll_off_v(r)                (((r) >> 5U) & 0x1U)
#define trim_sys_nvl_common_clk_alt_switch_r()                     (0x001371c4U)
#define trim_sys_nvl_common_clk_alt_switch_slowclk_f(v) ((U32(v) & 0x3U) << 16U)
#define trim_sys_nvl_common_clk_alt_switch_slowclk_m()        (U32(0x3U) << 16U)
#define trim_sys_nvl_common_clk_alt_switch_slowclk_v(r)    (((r) >> 16U) & 0x3U)
#define trim_sys_nvl_common_clk_alt_switch_slowclk_xtal4x_v()      (0x00000003U)
#define trim_sys_nvl_common_clk_alt_switch_slowclk_xtal4x_f()         (0x30000U)
#define trim_sys_nvl_common_clk_alt_switch_slowclk_xtal_in_v()     (0x00000000U)
#define trim_sys_nvl_common_clk_alt_switch_slowclk_xtal_in_f()            (0x0U)
#define trim_sys_nvl_common_clk_alt_switch_finalsel_f(v) ((U32(v) & 0x3U) << 0U)
#define trim_sys_nvl_common_clk_alt_switch_finalsel_m()        (U32(0x3U) << 0U)
#define trim_sys_nvl_common_clk_alt_switch_finalsel_v(r)    (((r) >> 0U) & 0x3U)
#define trim_sys_nvl_common_clk_alt_switch_finalsel_slowclk_v()    (0x00000000U)
#define trim_sys_nvl_common_clk_alt_switch_finalsel_slowclk_f()           (0x0U)
#define trim_sys_nvl_common_clk_alt_switch_finalsel_miscclk_v()    (0x00000002U)
#define trim_sys_nvl_common_clk_alt_switch_finalsel_miscclk_f()           (0x2U)
#define trim_sys_nvl_common_clk_alt_switch_finalsel_onesrcclk_v()  (0x00000003U)
#define trim_sys_nvl_common_clk_alt_switch_finalsel_onesrcclk_f()         (0x3U)
#define trim_gpc_bcast_fr_clk_cntr_ncgpcclk_cfg_r()                (0x00132a70U)
#define trim_gpc_bcast_fr_clk_cntr_ncgpcclk_cfg_update_cycle_init_f()     (0x0U)
#define trim_gpc_bcast_fr_clk_cntr_ncgpcclk_cfg_cont_update_enabled_f()\
				(0x8000000U)
#define trim_gpc_bcast_fr_clk_cntr_ncgpcclk_cfg_start_count_disabled_f()  (0x0U)
#define trim_gpc_bcast_fr_clk_cntr_ncgpcclk_cfg_start_count_enabled_f()\
				(0x2000000U)
#define trim_gpc_bcast_fr_clk_cntr_ncgpcclk_cfg_reset_m()     (U32(0x1U) << 24U)
#define trim_gpc_bcast_fr_clk_cntr_ncgpcclk_cfg_reset_asserted_f()  (0x1000000U)
#define trim_gpc_bcast_fr_clk_cntr_ncgpcclk_cfg_reset_deasserted_f()      (0x0U)
#define trim_gpc_bcast_fr_clk_cntr_ncgpcclk_cfg_source_gpcclk_noeg_f()\
				(0x20000000U)
#define trim_gpc_bcast_fr_clk_cntr_ncgpcclk_cnt0_r()               (0x00132a74U)
#define trim_gpc_bcast_fr_clk_cntr_ncgpcclk_cnt1_r()               (0x00132a78U)
#define trim_sys_fll_fr_clk_cntr_xbarclk_cfg_r()                   (0x00136470U)
#define trim_sys_fll_fr_clk_cntr_xbarclk_cfg_update_cycle_init_f()        (0x0U)
#define trim_sys_fll_fr_clk_cntr_xbarclk_cfg_cont_update_enabled_f()\
				(0x8000000U)
#define trim_sys_fll_fr_clk_cntr_xbarclk_cfg_start_count_disabled_f()     (0x0U)
#define trim_sys_fll_fr_clk_cntr_xbarclk_cfg_start_count_enabled_f()\
				(0x2000000U)
#define trim_sys_fll_fr_clk_cntr_xbarclk_cfg_reset_m()        (U32(0x1U) << 24U)
#define trim_sys_fll_fr_clk_cntr_xbarclk_cfg_reset_asserted_f()     (0x1000000U)
#define trim_sys_fll_fr_clk_cntr_xbarclk_cfg_reset_deasserted_f()         (0x0U)
#define trim_sys_fll_fr_clk_cntr_xbarclk_cfg_source_xbar_nobg_f()         (0x0U)
#define trim_sys_fll_fr_clk_cntr_xbarclk_cntr0_r()                 (0x00136474U)
#define trim_sys_fll_fr_clk_cntr_xbarclk_cntr1_r()                 (0x00136478U)
#define trim_sys_fr_clk_cntr_sysclk_cfg_r()                        (0x0013762cU)
#define trim_sys_fr_clk_cntr_sysclk_cfg_update_cycle_init_f()             (0x0U)
#define trim_sys_fr_clk_cntr_sysclk_cfg_cont_update_enabled_f()     (0x8000000U)
#define trim_sys_fr_clk_cntr_sysclk_cfg_start_count_disabled_f()          (0x0U)
#define trim_sys_fr_clk_cntr_sysclk_cfg_start_count_enabled_f()     (0x2000000U)
#define trim_sys_fr_clk_cntr_sysclk_cfg_reset_m()             (U32(0x1U) << 24U)
#define trim_sys_fr_clk_cntr_sysclk_cfg_reset_asserted_f()          (0x1000000U)
#define trim_sys_fr_clk_cntr_sysclk_cfg_reset_deasserted_f()              (0x0U)
#define trim_sys_fr_clk_cntr_sysclk_cfg_source_sys_noeg_f()               (0x0U)
#define trim_sys_fr_clk_cntr_sysclk_cntr0_r()                      (0x00137630U)
#define trim_sys_fr_clk_cntr_sysclk_cntr1_r()                      (0x00137634U)
#define trim_sys_ind_clk_sys_core_clksrc_r()                       (0x00137c00U)
#define trim_sys_ind_clk_sys_core_clksrc_hostclk_fll_f()                (0x180U)
#define trim_fault_threshold_high_r()                              (0x00132af0U)
#define trim_fault_threshold_high_count_v(r)         (((r) >> 0U) & 0xffffffffU)
#define trim_fault_threshold_low_r()                               (0x00132af4U)
#define trim_fault_threshold_low_count_v(r)          (((r) >> 0U) & 0xffffffffU)
#define trim_fault_status_r()                                      (0x00132b0cU)
#define trim_fault_status_dc_m()                               (U32(0x1U) << 0U)
#define trim_fault_status_dc_v(r)                           (((r) >> 0U) & 0x1U)
#define trim_fault_status_dc_true_v()                              (0x00000001U)
#define trim_fault_status_lower_threshold_m()                  (U32(0x1U) << 1U)
#define trim_fault_status_lower_threshold_v(r)              (((r) >> 1U) & 0x1U)
#define trim_fault_status_lower_threshold_true_v()                 (0x00000001U)
#define trim_fault_status_higher_threshold_m()                 (U32(0x1U) << 2U)
#define trim_fault_status_higher_threshold_v(r)             (((r) >> 2U) & 0x1U)
#define trim_fault_status_higher_threshold_true_v()                (0x00000001U)
#define trim_fault_status_overflow_m()                         (U32(0x1U) << 3U)
#define trim_fault_status_overflow_v(r)                     (((r) >> 3U) & 0x1U)
#define trim_fault_status_overflow_true_v()                        (0x00000001U)
#define trim_fault_status_fault_out_v(r)                    (((r) >> 4U) & 0x1U)
#define trim_fault_status_fault_out_true_v()                       (0x00000001U)
#define trim_gpcclk_fault_priv_level_mask_r()                      (0x00132bb0U)
#define trim_gpcclk_fault_threshold_high_r()                       (0x00132af0U)
#define trim_gpcclk_fault_threshold_low_r()                        (0x00132af4U)
#define trim_gpcclk_fault_status_r()                               (0x00132b0cU)
#define trim_sysclk_fault_priv_level_mask_r()                      (0x00137b80U)
#define trim_sysclk_fault_threshold_high_r()                       (0x00137674U)
#define trim_sysclk_fault_threshold_low_r()                        (0x00137678U)
#define trim_sysclk_fault_status_r()                               (0x00137690U)
#define trim_hubclk_fault_priv_level_mask_r()                      (0x00137b80U)
#define trim_hubclk_fault_threshold_high_r()                       (0x001376b4U)
#define trim_hubclk_fault_threshold_low_r()                        (0x001376b8U)
#define trim_hubclk_fault_status_r()                               (0x001376d0U)
#define trim_hostclk_fault_priv_level_mask_r()                     (0x00137b80U)
#define trim_hostclk_fault_threshold_high_r()                      (0x00137774U)
#define trim_hostclk_fault_threshold_low_r()                       (0x00137778U)
#define trim_hostclk_fault_status_r()                              (0x00137790U)
#define trim_xbarclk_fault_priv_level_mask_r()                     (0x00137b80U)
#define trim_xbarclk_fault_threshold_high_r()                      (0x00137980U)
#define trim_xbarclk_fault_threshold_low_r()                       (0x00137984U)
#define trim_xbarclk_fault_status_r()                              (0x0013799cU)
#define trim_nvdclk_fault_priv_level_mask_r()                      (0x00137b80U)
#define trim_nvdclk_fault_threshold_high_r()                       (0x001379c0U)
#define trim_nvdclk_fault_threshold_low_r()                        (0x001379c4U)
#define trim_nvdclk_fault_status_r()                               (0x001379dcU)
#define trim_dramclk_fault_priv_level_mask_r()                     (0x001321e4U)
#define trim_dramclk_fault_threshold_high_r()                      (0x001321a0U)
#define trim_dramclk_fault_threshold_low_r()                       (0x001321a4U)
#define trim_dramclk_fault_status_r()                              (0x001321bcU)
#define trim_pwrclk_fault_priv_level_mask_r()                      (0x00137b80U)
#define trim_pwrclk_fault_threshold_high_r()                       (0x001376f4U)
#define trim_pwrclk_fault_threshold_low_r()                        (0x001376f8U)
#define trim_pwrclk_fault_status_r()                               (0x00137710U)
#define trim_utilsclk_fault_priv_level_mask_r()                    (0x00137b80U)
#define trim_utilsclk_fault_threshold_high_r()                     (0x00137734U)
#define trim_utilsclk_fault_threshold_low_r()                      (0x00137738U)
#define trim_utilsclk_fault_status_r()                             (0x00137750U)
#define trim_pex_refclk_fault_priv_level_mask_r()                  (0x00137b80U)
#define trim_pex_refclk_fault_threshold_high_r()                   (0x001377b4U)
#define trim_pex_refclk_fault_threshold_low_r()                    (0x001377b8U)
#define trim_pex_refclk_fault_status_r()                           (0x001377d0U)
#define trim_nvl_commonclk_fault_priv_level_mask_r()               (0x00137b80U)
#define trim_nvl_commonclk_fault_threshold_high_r()                (0x00137940U)
#define trim_nvl_commonclk_fault_threshold_low_r()                 (0x00137944U)
#define trim_nvl_commonclk_fault_status_r()                        (0x0013795cU)
#define trim_xclk_fault_priv_level_mask_r()                        (0x00137b00U)
#define trim_xclk_fault_threshold_high_r()                         (0x00137900U)
#define trim_xclk_fault_threshold_low_r()                          (0x00137904U)
#define trim_xclk_fault_status_r()                                 (0x0013791cU)
#define trim_fmon_master_status_priv_mask_r()                      (0x00137b40U)
#define trim_fmon_master_status_r()                                (0x00137a00U)
#define trim_fmon_master_status_fault_out_v(r)              (((r) >> 0U) & 0x1U)
#define trim_fmon_master_status_fault_out_true_v()                 (0x00000001U)
#define trim_xtal4x_cfg5_r()                                       (0x001370c0U)
#define trim_xtal4x_cfg5_curr_state_v(r)                   (((r) >> 16U) & 0xfU)
#define trim_xtal4x_cfg5_curr_state_good_v()                       (0x00000006U)
#define trim_xtal4x_cfg_r()                                        (0x001370a0U)
#define trim_xtal4x_cfg_pll_lock_v(r)                      (((r) >> 17U) & 0x1U)
#define trim_xtal4x_cfg_pll_lock_true_v()                          (0x00000001U)
#define trim_mem_pll_status_r()                                    (0x00137390U)
#define trim_mem_pll_status_dram_curr_state_v(r)            (((r) >> 1U) & 0x1U)
#define trim_mem_pll_status_dram_curr_state_good_v()               (0x00000001U)
#define trim_mem_pll_status_refm_curr_state_v(r)           (((r) >> 17U) & 0x1U)
#define trim_mem_pll_status_refm_curr_state_good_v()               (0x00000001U)
#define trim_sppll0_cfg_r()                                        (0x0000e800U)
#define trim_sppll0_cfg_curr_state_v(r)                    (((r) >> 17U) & 0x1U)
#define trim_sppll0_cfg_curr_state_good_v()                        (0x00000001U)
#define trim_sppll1_cfg_r()                                        (0x0000e820U)
#define trim_sppll1_cfg_curr_state_v(r)                    (((r) >> 17U) & 0x1U)
#define trim_sppll1_cfg_curr_state_good_v()                        (0x00000001U)
#endif
