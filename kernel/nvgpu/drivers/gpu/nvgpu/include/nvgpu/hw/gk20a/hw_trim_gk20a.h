/*
 * Copyright (c) 2012-2019, NVIDIA CORPORATION.  All rights reserved.
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
#ifndef NVGPU_HW_TRIM_GK20A_H
#define NVGPU_HW_TRIM_GK20A_H

#include <nvgpu/types.h>
#include <nvgpu/static_analysis.h>

#define trim_sys_gpcpll_cfg_r()                                    (0x00137000U)
#define trim_sys_gpcpll_cfg_enable_m()                         (U32(0x1U) << 0U)
#define trim_sys_gpcpll_cfg_enable_v(r)                     (((r) >> 0U) & 0x1U)
#define trim_sys_gpcpll_cfg_enable_no_f()                                 (0x0U)
#define trim_sys_gpcpll_cfg_enable_yes_f()                                (0x1U)
#define trim_sys_gpcpll_cfg_iddq_m()                           (U32(0x1U) << 1U)
#define trim_sys_gpcpll_cfg_iddq_v(r)                       (((r) >> 1U) & 0x1U)
#define trim_sys_gpcpll_cfg_iddq_power_on_v()                      (0x00000000U)
#define trim_sys_gpcpll_cfg_enb_lckdet_m()                     (U32(0x1U) << 4U)
#define trim_sys_gpcpll_cfg_enb_lckdet_power_on_f()                       (0x0U)
#define trim_sys_gpcpll_cfg_enb_lckdet_power_off_f()                     (0x10U)
#define trim_sys_gpcpll_cfg_pll_lock_v(r)                  (((r) >> 17U) & 0x1U)
#define trim_sys_gpcpll_cfg_pll_lock_true_f()                         (0x20000U)
#define trim_sys_gpcpll_coeff_r()                                  (0x00137004U)
#define trim_sys_gpcpll_coeff_mdiv_f(v)                 ((U32(v) & 0xffU) << 0U)
#define trim_sys_gpcpll_coeff_mdiv_m()                        (U32(0xffU) << 0U)
#define trim_sys_gpcpll_coeff_mdiv_v(r)                    (((r) >> 0U) & 0xffU)
#define trim_sys_gpcpll_coeff_ndiv_f(v)                 ((U32(v) & 0xffU) << 8U)
#define trim_sys_gpcpll_coeff_ndiv_m()                        (U32(0xffU) << 8U)
#define trim_sys_gpcpll_coeff_ndiv_v(r)                    (((r) >> 8U) & 0xffU)
#define trim_sys_gpcpll_coeff_pldiv_f(v)               ((U32(v) & 0x3fU) << 16U)
#define trim_sys_gpcpll_coeff_pldiv_m()                      (U32(0x3fU) << 16U)
#define trim_sys_gpcpll_coeff_pldiv_v(r)                  (((r) >> 16U) & 0x3fU)
#define trim_sys_sel_vco_r()                                       (0x00137100U)
#define trim_sys_sel_vco_gpc2clk_out_m()                       (U32(0x1U) << 0U)
#define trim_sys_sel_vco_gpc2clk_out_init_v()                      (0x00000000U)
#define trim_sys_sel_vco_gpc2clk_out_init_f()                             (0x0U)
#define trim_sys_sel_vco_gpc2clk_out_bypass_f()                           (0x0U)
#define trim_sys_sel_vco_gpc2clk_out_vco_f()                              (0x1U)
#define trim_sys_gpc2clk_out_r()                                   (0x00137250U)
#define trim_sys_gpc2clk_out_bypdiv_s()                                     (6U)
#define trim_sys_gpc2clk_out_bypdiv_f(v)                ((U32(v) & 0x3fU) << 0U)
#define trim_sys_gpc2clk_out_bypdiv_m()                       (U32(0x3fU) << 0U)
#define trim_sys_gpc2clk_out_bypdiv_v(r)                   (((r) >> 0U) & 0x3fU)
#define trim_sys_gpc2clk_out_bypdiv_by31_f()                             (0x3cU)
#define trim_sys_gpc2clk_out_vcodiv_s()                                     (6U)
#define trim_sys_gpc2clk_out_vcodiv_f(v)                ((U32(v) & 0x3fU) << 8U)
#define trim_sys_gpc2clk_out_vcodiv_m()                       (U32(0x3fU) << 8U)
#define trim_sys_gpc2clk_out_vcodiv_v(r)                   (((r) >> 8U) & 0x3fU)
#define trim_sys_gpc2clk_out_vcodiv_by1_f()                               (0x0U)
#define trim_sys_gpc2clk_out_sdiv14_m()                       (U32(0x1U) << 31U)
#define trim_sys_gpc2clk_out_sdiv14_indiv4_mode_f()                (0x80000000U)
#define trim_gpc_clk_cntr_ncgpcclk_cfg_r(i)\
		(nvgpu_safe_add_u32(0x00134124U, nvgpu_safe_mult_u32((i), 512U)))
#define trim_gpc_clk_cntr_ncgpcclk_cfg_noofipclks_f(v)\
				((U32(v) & 0x3fffU) << 0U)
#define trim_gpc_clk_cntr_ncgpcclk_cfg_write_en_asserted_f()          (0x10000U)
#define trim_gpc_clk_cntr_ncgpcclk_cfg_enable_asserted_f()           (0x100000U)
#define trim_gpc_clk_cntr_ncgpcclk_cfg_reset_asserted_f()           (0x1000000U)
#define trim_gpc_clk_cntr_ncgpcclk_cnt_r(i)\
		(nvgpu_safe_add_u32(0x00134128U, nvgpu_safe_mult_u32((i), 512U)))
#define trim_gpc_clk_cntr_ncgpcclk_cnt_value_v(r)       (((r) >> 0U) & 0xfffffU)
#define trim_sys_gpcpll_cfg2_r()                                   (0x0013700cU)
#define trim_sys_gpcpll_cfg2_pll_stepa_f(v)            ((U32(v) & 0xffU) << 24U)
#define trim_sys_gpcpll_cfg2_pll_stepa_m()                   (U32(0xffU) << 24U)
#define trim_sys_gpcpll_cfg3_r()                                   (0x00137018U)
#define trim_sys_gpcpll_cfg3_pll_stepb_f(v)            ((U32(v) & 0xffU) << 16U)
#define trim_sys_gpcpll_cfg3_pll_stepb_m()                   (U32(0xffU) << 16U)
#define trim_sys_gpcpll_ndiv_slowdown_r()                          (0x0013701cU)
#define trim_sys_gpcpll_ndiv_slowdown_slowdown_using_pll_m()  (U32(0x1U) << 22U)
#define trim_sys_gpcpll_ndiv_slowdown_slowdown_using_pll_yes_f()     (0x400000U)
#define trim_sys_gpcpll_ndiv_slowdown_slowdown_using_pll_no_f()           (0x0U)
#define trim_sys_gpcpll_ndiv_slowdown_en_dynramp_m()          (U32(0x1U) << 31U)
#define trim_sys_gpcpll_ndiv_slowdown_en_dynramp_yes_f()           (0x80000000U)
#define trim_sys_gpcpll_ndiv_slowdown_en_dynramp_no_f()                   (0x0U)
#define trim_gpc_bcast_gpcpll_ndiv_slowdown_debug_r()              (0x001328a0U)
#define trim_gpc_bcast_gpcpll_ndiv_slowdown_debug_pll_dynramp_done_synced_v(r)\
				(((r) >> 24U) & 0x1U)
#endif
