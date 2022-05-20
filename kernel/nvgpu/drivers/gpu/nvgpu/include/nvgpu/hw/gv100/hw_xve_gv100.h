/*
 * Copyright (c) 2017-2019, NVIDIA CORPORATION.  All rights reserved.
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
#ifndef NVGPU_HW_XVE_GV100_H
#define NVGPU_HW_XVE_GV100_H

#include <nvgpu/types.h>
#include <nvgpu/static_analysis.h>

#define xve_rom_ctrl_r()                                           (0x00000050U)
#define xve_rom_ctrl_rom_shadow_f(v)                     ((U32(v) & 0x1U) << 0U)
#define xve_rom_ctrl_rom_shadow_disabled_f()                              (0x0U)
#define xve_rom_ctrl_rom_shadow_enabled_f()                               (0x1U)
#define xve_link_control_status_r()                                (0x00000088U)
#define xve_link_control_status_link_speed_m()                (U32(0xfU) << 16U)
#define xve_link_control_status_link_speed_v(r)            (((r) >> 16U) & 0xfU)
#define xve_link_control_status_link_speed_link_speed_2p5_v()      (0x00000001U)
#define xve_link_control_status_link_speed_link_speed_5p0_v()      (0x00000002U)
#define xve_link_control_status_link_speed_link_speed_8p0_v()      (0x00000003U)
#define xve_link_control_status_link_width_m()               (U32(0x3fU) << 20U)
#define xve_link_control_status_link_width_v(r)           (((r) >> 20U) & 0x3fU)
#define xve_link_control_status_link_width_x1_v()                  (0x00000001U)
#define xve_link_control_status_link_width_x2_v()                  (0x00000002U)
#define xve_link_control_status_link_width_x4_v()                  (0x00000004U)
#define xve_link_control_status_link_width_x8_v()                  (0x00000008U)
#define xve_link_control_status_link_width_x16_v()                 (0x00000010U)
#define xve_priv_xv_r()                                            (0x00000150U)
#define xve_priv_xv_cya_l0s_enable_f(v)                  ((U32(v) & 0x1U) << 7U)
#define xve_priv_xv_cya_l0s_enable_m()                         (U32(0x1U) << 7U)
#define xve_priv_xv_cya_l0s_enable_v(r)                     (((r) >> 7U) & 0x1U)
#define xve_priv_xv_cya_l1_enable_f(v)                   ((U32(v) & 0x1U) << 8U)
#define xve_priv_xv_cya_l1_enable_m()                          (U32(0x1U) << 8U)
#define xve_priv_xv_cya_l1_enable_v(r)                      (((r) >> 8U) & 0x1U)
#define xve_cya_2_r()                                              (0x00000704U)
#define xve_reset_r()                                              (0x00000718U)
#define xve_reset_reset_m()                                    (U32(0x1U) << 0U)
#define xve_reset_gpu_on_sw_reset_m()                          (U32(0x1U) << 1U)
#define xve_reset_counter_en_m()                               (U32(0x1U) << 2U)
#define xve_reset_counter_val_f(v)                     ((U32(v) & 0x7ffU) << 4U)
#define xve_reset_counter_val_m()                            (U32(0x7ffU) << 4U)
#define xve_reset_counter_val_v(r)                        (((r) >> 4U) & 0x7ffU)
#define xve_reset_clock_on_sw_reset_m()                       (U32(0x1U) << 15U)
#define xve_reset_clock_counter_en_m()                        (U32(0x1U) << 16U)
#define xve_reset_clock_counter_val_f(v)              ((U32(v) & 0x7ffU) << 17U)
#define xve_reset_clock_counter_val_m()                     (U32(0x7ffU) << 17U)
#define xve_reset_clock_counter_val_v(r)                 (((r) >> 17U) & 0x7ffU)
#endif
