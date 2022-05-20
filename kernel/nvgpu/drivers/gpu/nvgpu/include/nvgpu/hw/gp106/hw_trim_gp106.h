/*
 * Copyright (c) 2016-2019, NVIDIA CORPORATION.  All rights reserved.
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
#ifndef NVGPU_HW_TRIM_GP106_H
#define NVGPU_HW_TRIM_GP106_H

#include <nvgpu/types.h>
#include <nvgpu/static_analysis.h>

#define trim_gpc_bcast_clk_cntr_ncgpcclk_cfg_r()                   (0x00132924U)
#define trim_gpc_bcast_clk_cntr_ncgpcclk_cfg_noofipclks_s()                (16U)
#define trim_gpc_bcast_clk_cntr_ncgpcclk_cfg_noofipclks_f(v)\
				((U32(v) & 0xffffU) << 0U)
#define trim_gpc_bcast_clk_cntr_ncgpcclk_cfg_noofipclks_m() (U32(0xffffU) << 0U)
#define trim_gpc_bcast_clk_cntr_ncgpcclk_cfg_noofipclks_v(r)\
				(((r) >> 0U) & 0xffffU)
#define trim_gpc_bcast_clk_cntr_ncgpcclk_cfg_write_en_s()                   (1U)
#define trim_gpc_bcast_clk_cntr_ncgpcclk_cfg_write_en_f(v)\
				((U32(v) & 0x1U) << 16U)
#define trim_gpc_bcast_clk_cntr_ncgpcclk_cfg_write_en_m()     (U32(0x1U) << 16U)
#define trim_gpc_bcast_clk_cntr_ncgpcclk_cfg_write_en_v(r) (((r) >> 16U) & 0x1U)
#define trim_gpc_bcast_clk_cntr_ncgpcclk_cfg_write_en_deasserted_f()      (0x0U)
#define trim_gpc_bcast_clk_cntr_ncgpcclk_cfg_write_en_asserted_f()    (0x10000U)
#define trim_gpc_bcast_clk_cntr_ncgpcclk_cfg_enable_s()                     (1U)
#define trim_gpc_bcast_clk_cntr_ncgpcclk_cfg_enable_f(v)\
				((U32(v) & 0x1U) << 20U)
#define trim_gpc_bcast_clk_cntr_ncgpcclk_cfg_enable_m()       (U32(0x1U) << 20U)
#define trim_gpc_bcast_clk_cntr_ncgpcclk_cfg_enable_v(r)   (((r) >> 20U) & 0x1U)
#define trim_gpc_bcast_clk_cntr_ncgpcclk_cfg_enable_deasserted_f()        (0x0U)
#define trim_gpc_bcast_clk_cntr_ncgpcclk_cfg_enable_asserted_f()     (0x100000U)
#define trim_gpc_bcast_clk_cntr_ncgpcclk_cfg_reset_s()                      (1U)
#define trim_gpc_bcast_clk_cntr_ncgpcclk_cfg_reset_f(v) ((U32(v) & 0x1U) << 24U)
#define trim_gpc_bcast_clk_cntr_ncgpcclk_cfg_reset_m()        (U32(0x1U) << 24U)
#define trim_gpc_bcast_clk_cntr_ncgpcclk_cfg_reset_v(r)    (((r) >> 24U) & 0x1U)
#define trim_gpc_bcast_clk_cntr_ncgpcclk_cfg_reset_deasserted_f()         (0x0U)
#define trim_gpc_bcast_clk_cntr_ncgpcclk_cfg_reset_asserted_f()     (0x1000000U)
#define trim_gpc_bcast_clk_cntr_ncgpcclk_cfg_source_gpc2clk_f()    (0x70000000U)
#define trim_gpc_bcast_clk_cntr_ncgpcclk_cnt_r()                   (0x00132928U)
#define trim_fbpa_bcast_clk_cntr_ncltcclk_cfg_r()                  (0x00132128U)
#define trim_fbpa_bcast_clk_cntr_ncltcclk_cfg_source_dramdiv4_rec_clk1_f()\
				(0x30000000U)
#define trim_fbpa_bcast_clk_cntr_ncltcclk_cnt_r()                  (0x0013212cU)
#define trim_sys_clk_cntr_ncltcpll_cfg_r()                         (0x001373c0U)
#define trim_sys_clk_cntr_ncltcpll_cfg_source_xbar2clk_f()         (0x20000000U)
#define trim_sys_clk_cntr_ncltcpll_cnt_r()                         (0x001373c4U)
#define trim_sys_clk_cntr_ncsyspll_cfg_r()                         (0x001373b0U)
#define trim_sys_clk_cntr_ncsyspll_cfg_source_sys2clk_f()                 (0x0U)
#define trim_sys_clk_cntr_ncsyspll_cnt_r()                         (0x001373b4U)
#endif
