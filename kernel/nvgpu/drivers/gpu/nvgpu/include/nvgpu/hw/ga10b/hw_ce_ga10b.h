/*
 * Copyright (c) 2019-2022, NVIDIA CORPORATION.  All rights reserved.
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
#ifndef NVGPU_HW_CE_GA10B_H
#define NVGPU_HW_CE_GA10B_H

#include <nvgpu/types.h>
#include <nvgpu/static_analysis.h>

#define ce_lce_intr_en_r(i)\
		(nvgpu_safe_add_u32(0x0010440cU, nvgpu_safe_mult_u32((i), 128U)))
#define ce_lce_intr_en_blockpipe_m()                           (U32(0x1U) << 0U)
#define ce_lce_intr_en_nonblockpipe_m()                        (U32(0x1U) << 1U)
#define ce_lce_intr_en_launcherr_m()                           (U32(0x1U) << 2U)
#define ce_lce_intr_en_invalid_config_m()                      (U32(0x1U) << 3U)
#define ce_lce_intr_en_mthd_buffer_fault_m()                   (U32(0x1U) << 4U)
#define ce_lce_intr_en_fbuf_crc_fail_m()                       (U32(0x1U) << 5U)
#define ce_lce_intr_en_fbuf_magic_chk_fail_m()                 (U32(0x1U) << 6U)
#define ce_lce_intr_en_poison_error_m()                        (U32(0x1U) << 7U)
#define ce_lce_intr_en_nonstalling_debug_m()                  (U32(0x1U) << 30U)
#define ce_lce_intr_en_stalling_debug_m()                     (U32(0x1U) << 31U)
#define ce_intr_status_r(i)\
		(nvgpu_safe_add_u32(0x00104410U, nvgpu_safe_mult_u32((i), 128U)))
#define ce_intr_status_blockpipe_pending_f()                              (0x1U)
#define ce_intr_status_launcherr_pending_f()                              (0x4U)
#define ce_intr_status_invalid_config_pending_f()                         (0x8U)
#define ce_intr_status_invalid_config_reset_f()                           (0x8U)
#define ce_intr_status_mthd_buffer_fault_pending_f()                     (0x10U)
#define ce_intr_status_mthd_buffer_fault_reset_f()                       (0x10U)
#define ce_intr_status_fbuf_crc_fail_pending_f()                         (0x20U)
#define ce_intr_status_fbuf_crc_fail_reset_f()                           (0x20U)
#define ce_intr_status_fbuf_magic_chk_fail_pending_f()                   (0x40U)
#define ce_intr_status_fbuf_magic_chk_fail_reset_f()                     (0x40U)
#define ce_intr_status_poison_error_pending_f()                          (0x80U)
#define ce_intr_status_poison_error_reset_f()                            (0x80U)
#define ce_intr_status_stalling_debug_pending_f()                  (0x80000000U)
#define ce_intr_retrigger_r(i)\
		(nvgpu_safe_add_u32(0x00104434U, nvgpu_safe_mult_u32((i), 128U)))
#define ce_intr_retrigger_trigger_true_f()                                (0x1U)
#define ce_pce_map_r()                                             (0x00104028U)
#define ce_lce_opt_r(i)\
		(nvgpu_safe_add_u32(0x00104414U, nvgpu_safe_mult_u32((i), 128U)))
#define ce_lce_opt_force_barriers_npl__prod_f()                           (0x8U)
#define ce_lce_intr_notify_ctrl_r(i)\
		(nvgpu_safe_add_u32(0x00104424U, nvgpu_safe_mult_u32((i), 128U)))
#define ce_lce_intr_notify_ctrl_vector_v(r)               (((r) >> 0U) & 0xfffU)
#define ce_lce_intr_notify_ctrl_gsp_m()                       (U32(0x1U) << 30U)
#define ce_lce_intr_notify_ctrl_gsp_disable_f()                           (0x0U)
#define ce_lce_intr_notify_ctrl_cpu_m()                       (U32(0x1U) << 31U)
#define ce_lce_intr_notify_ctrl_cpu_enable_f()                     (0x80000000U)
#define ce_lce_intr_notify_ctrl_cpu_disable_f()                           (0x0U)
#define ce_lce_bind_status_r(i)\
		(nvgpu_safe_add_u32(0x00104404U, nvgpu_safe_mult_u32((i), 128U)))
#define ce_lce_bind_status_bound_v(r)                       (((r) >> 0U) & 0x1U)
#define ce_lce_bind_status_bound_false_v()                         (0x00000000U)
#define ce_lce_bind_status_ctx_ptr_v(r)               (((r) >> 1U) & 0xfffffffU)
#define ce_lce_launcherr_r(i)\
		(nvgpu_safe_add_u32(0x00104418U, nvgpu_safe_mult_u32((i), 128U)))
#define ce_lce_launcherr_report_v(r)                        (((r) >> 0U) & 0xfU)
#define ce_lce_launcherr_report_invalid_config_v()                 (0x0000000dU)
#define ce_lce_launcherr_report_method_buffer_access_fault_v()     (0x0000000eU)
#define ce_lce_intr_ctrl_r(i)\
		(nvgpu_safe_add_u32(0x0010442cU, nvgpu_safe_mult_u32((i), 128U)))
#define ce_lce_intr_ctrl_gsp_m()                              (U32(0x1U) << 30U)
#define ce_lce_intr_ctrl_gsp_disable_f()                                  (0x0U)
#define ce_lce_intr_ctrl_cpu_m()                              (U32(0x1U) << 31U)
#define ce_lce_intr_ctrl_cpu_enable_f()                            (0x80000000U)
#define ce_lce_intr_ctrl_cpu_disable_f()                                  (0x0U)
#define ce_lce_engctl_r(i)\
		(nvgpu_safe_add_u32(0x0010441cU, nvgpu_safe_mult_u32((i), 128U)))
#define ce_lce_engctl_stallreq_true_f()                                 (0x100U)
#define ce_lce_engctl_stallack_true_f()                                 (0x200U)
#endif
