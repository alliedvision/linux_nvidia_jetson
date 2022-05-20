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
#ifndef NVGPU_HW_IOCTRL_GV100_H
#define NVGPU_HW_IOCTRL_GV100_H

#include <nvgpu/types.h>
#include <nvgpu/static_analysis.h>

#define ioctrl_reset_r()                                           (0x00000140U)
#define ioctrl_reset_sw_post_reset_delay_microseconds_v()          (0x00000008U)
#define ioctrl_reset_linkreset_f(v)                     ((U32(v) & 0x3fU) << 8U)
#define ioctrl_reset_linkreset_m()                            (U32(0x3fU) << 8U)
#define ioctrl_reset_linkreset_v(r)                        (((r) >> 8U) & 0x3fU)
#define ioctrl_debug_reset_r()                                     (0x00000144U)
#define ioctrl_debug_reset_link_f(v)                    ((U32(v) & 0x3fU) << 0U)
#define ioctrl_debug_reset_link_m()                           (U32(0x3fU) << 0U)
#define ioctrl_debug_reset_link_v(r)                       (((r) >> 0U) & 0x3fU)
#define ioctrl_debug_reset_common_f(v)                  ((U32(v) & 0x1U) << 31U)
#define ioctrl_debug_reset_common_m()                         (U32(0x1U) << 31U)
#define ioctrl_debug_reset_common_v(r)                     (((r) >> 31U) & 0x1U)
#define ioctrl_clock_control_r(i)\
		(nvgpu_safe_add_u32(0x00000180U, nvgpu_safe_mult_u32((i), 4U)))
#define ioctrl_clock_control__size_1_v()                           (0x00000006U)
#define ioctrl_clock_control_clkdis_f(v)                 ((U32(v) & 0x1U) << 0U)
#define ioctrl_clock_control_clkdis_m()                        (U32(0x1U) << 0U)
#define ioctrl_clock_control_clkdis_v(r)                    (((r) >> 0U) & 0x1U)
#define ioctrl_top_intr_0_status_r()                               (0x00000200U)
#define ioctrl_top_intr_0_status_link_f(v)              ((U32(v) & 0x3fU) << 0U)
#define ioctrl_top_intr_0_status_link_m()                     (U32(0x3fU) << 0U)
#define ioctrl_top_intr_0_status_link_v(r)                 (((r) >> 0U) & 0x3fU)
#define ioctrl_top_intr_0_status_common_f(v)            ((U32(v) & 0x1U) << 31U)
#define ioctrl_top_intr_0_status_common_m()                   (U32(0x1U) << 31U)
#define ioctrl_top_intr_0_status_common_v(r)               (((r) >> 31U) & 0x1U)
#define ioctrl_common_intr_0_mask_r()                              (0x00000220U)
#define ioctrl_common_intr_0_mask_fatal_f(v)             ((U32(v) & 0x1U) << 0U)
#define ioctrl_common_intr_0_mask_fatal_v(r)                (((r) >> 0U) & 0x1U)
#define ioctrl_common_intr_0_mask_nonfatal_f(v)          ((U32(v) & 0x1U) << 1U)
#define ioctrl_common_intr_0_mask_nonfatal_v(r)             (((r) >> 1U) & 0x1U)
#define ioctrl_common_intr_0_mask_correctable_f(v)       ((U32(v) & 0x1U) << 2U)
#define ioctrl_common_intr_0_mask_correctable_v(r)          (((r) >> 2U) & 0x1U)
#define ioctrl_common_intr_0_mask_intra_f(v)             ((U32(v) & 0x1U) << 3U)
#define ioctrl_common_intr_0_mask_intra_v(r)                (((r) >> 3U) & 0x1U)
#define ioctrl_common_intr_0_mask_intrb_f(v)             ((U32(v) & 0x1U) << 4U)
#define ioctrl_common_intr_0_mask_intrb_v(r)                (((r) >> 4U) & 0x1U)
#define ioctrl_common_intr_0_status_r()                            (0x00000224U)
#define ioctrl_common_intr_0_status_fatal_f(v)           ((U32(v) & 0x1U) << 0U)
#define ioctrl_common_intr_0_status_fatal_v(r)              (((r) >> 0U) & 0x1U)
#define ioctrl_common_intr_0_status_nonfatal_f(v)        ((U32(v) & 0x1U) << 1U)
#define ioctrl_common_intr_0_status_nonfatal_v(r)           (((r) >> 1U) & 0x1U)
#define ioctrl_common_intr_0_status_correctable_f(v)     ((U32(v) & 0x1U) << 2U)
#define ioctrl_common_intr_0_status_correctable_v(r)        (((r) >> 2U) & 0x1U)
#define ioctrl_common_intr_0_status_intra_f(v)           ((U32(v) & 0x1U) << 3U)
#define ioctrl_common_intr_0_status_intra_v(r)              (((r) >> 3U) & 0x1U)
#define ioctrl_common_intr_0_status_intrb_f(v)           ((U32(v) & 0x1U) << 4U)
#define ioctrl_common_intr_0_status_intrb_v(r)              (((r) >> 4U) & 0x1U)
#define ioctrl_link_intr_0_mask_r(i)\
		(nvgpu_safe_add_u32(0x00000240U, nvgpu_safe_mult_u32((i), 20U)))
#define ioctrl_link_intr_0_mask_fatal_f(v)               ((U32(v) & 0x1U) << 0U)
#define ioctrl_link_intr_0_mask_fatal_v(r)                  (((r) >> 0U) & 0x1U)
#define ioctrl_link_intr_0_mask_nonfatal_f(v)            ((U32(v) & 0x1U) << 1U)
#define ioctrl_link_intr_0_mask_nonfatal_v(r)               (((r) >> 1U) & 0x1U)
#define ioctrl_link_intr_0_mask_correctable_f(v)         ((U32(v) & 0x1U) << 2U)
#define ioctrl_link_intr_0_mask_correctable_v(r)            (((r) >> 2U) & 0x1U)
#define ioctrl_link_intr_0_mask_intra_f(v)               ((U32(v) & 0x1U) << 3U)
#define ioctrl_link_intr_0_mask_intra_v(r)                  (((r) >> 3U) & 0x1U)
#define ioctrl_link_intr_0_mask_intrb_f(v)               ((U32(v) & 0x1U) << 4U)
#define ioctrl_link_intr_0_mask_intrb_v(r)                  (((r) >> 4U) & 0x1U)
#define ioctrl_link_intr_0_status_r(i)\
		(nvgpu_safe_add_u32(0x00000244U, nvgpu_safe_mult_u32((i), 20U)))
#define ioctrl_link_intr_0_status_fatal_f(v)             ((U32(v) & 0x1U) << 0U)
#define ioctrl_link_intr_0_status_fatal_v(r)                (((r) >> 0U) & 0x1U)
#define ioctrl_link_intr_0_status_nonfatal_f(v)          ((U32(v) & 0x1U) << 1U)
#define ioctrl_link_intr_0_status_nonfatal_v(r)             (((r) >> 1U) & 0x1U)
#define ioctrl_link_intr_0_status_correctable_f(v)       ((U32(v) & 0x1U) << 2U)
#define ioctrl_link_intr_0_status_correctable_v(r)          (((r) >> 2U) & 0x1U)
#define ioctrl_link_intr_0_status_intra_f(v)             ((U32(v) & 0x1U) << 3U)
#define ioctrl_link_intr_0_status_intra_v(r)                (((r) >> 3U) & 0x1U)
#define ioctrl_link_intr_0_status_intrb_f(v)             ((U32(v) & 0x1U) << 4U)
#define ioctrl_link_intr_0_status_intrb_v(r)                (((r) >> 4U) & 0x1U)
#endif
