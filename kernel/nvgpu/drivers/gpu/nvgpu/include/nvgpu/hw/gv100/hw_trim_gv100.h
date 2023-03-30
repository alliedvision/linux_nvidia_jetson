/*
 * Copyright (c) 2018-2019, NVIDIA CORPORATION.  All rights reserved.
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
#ifndef NVGPU_HW_TRIM_GV100_H
#define NVGPU_HW_TRIM_GV100_H

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
#define trim_gpc_bcast_fr_clk_cntr_ncgpcclk_cfg_source_gpcclk_f()  (0x10000000U)
#define trim_gpc_bcast_fr_clk_cntr_ncgpcclk_cnt0_r()               (0x00132a74U)
#define trim_gpc_bcast_fr_clk_cntr_ncgpcclk_cnt1_r()               (0x00132a78U)
#define trim_sys_fll_fr_clk_cntr_xbarclk_cfg_r()                   (0x00136470U)
#define trim_sys_fll_fr_clk_cntr_xbarclk_cfg_source_xbarclk_f()    (0x10000000U)
#define trim_sys_fll_fr_clk_cntr_xbarclk_cntr0_r()                 (0x00136474U)
#define trim_sys_fll_fr_clk_cntr_xbarclk_cntr1_r()                 (0x00136478U)
#define trim_sys_fr_clk_cntr_sysclk_cfg_r()                        (0x0013762cU)
#define trim_sys_fr_clk_cntr_sysclk_cfg_source_sysclk_f()          (0x20000000U)
#define trim_sys_fr_clk_cntr_sysclk_cntr0_r()                      (0x00137630U)
#define trim_sys_fr_clk_cntr_sysclk_cntr1_r()                      (0x00137634U)
#endif
