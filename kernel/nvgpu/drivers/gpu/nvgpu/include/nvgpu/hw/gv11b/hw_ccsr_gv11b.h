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
#ifndef NVGPU_HW_CCSR_GV11B_H
#define NVGPU_HW_CCSR_GV11B_H

#include <nvgpu/types.h>
#include <nvgpu/static_analysis.h>

#define ccsr_channel_inst_r(i)\
		(nvgpu_safe_add_u32(0x00800000U, nvgpu_safe_mult_u32((i), 8U)))
#define ccsr_channel_inst__size_1_v()                              (0x00000200U)
#define ccsr_channel_inst_ptr_f(v)                 ((U32(v) & 0xfffffffU) << 0U)
#define ccsr_channel_inst_target_vid_mem_f()                              (0x0U)
#define ccsr_channel_inst_target_sys_mem_coh_f()                   (0x20000000U)
#define ccsr_channel_inst_target_sys_mem_ncoh_f()                  (0x30000000U)
#define ccsr_channel_inst_bind_false_f()                                  (0x0U)
#define ccsr_channel_inst_bind_true_f()                            (0x80000000U)
#define ccsr_channel_r(i)\
		(nvgpu_safe_add_u32(0x00800004U, nvgpu_safe_mult_u32((i), 8U)))
#define ccsr_channel__size_1_v()                                   (0x00000200U)
#define ccsr_channel_enable_v(r)                            (((r) >> 0U) & 0x1U)
#define ccsr_channel_enable_in_use_v()                             (0x00000001U)
#define ccsr_channel_enable_set_f(v)                    ((U32(v) & 0x1U) << 10U)
#define ccsr_channel_enable_set_true_f()                                (0x400U)
#define ccsr_channel_enable_clr_true_f()                                (0x800U)
#define ccsr_channel_status_v(r)                           (((r) >> 24U) & 0xfU)
#define ccsr_channel_status_idle_v()                               (0x00000000U)
#define ccsr_channel_status_pending_v()                            (0x00000001U)
#define ccsr_channel_status_pending_ctx_reload_v()                 (0x00000002U)
#define ccsr_channel_status_pending_acquire_v()                    (0x00000003U)
#define ccsr_channel_status_pending_acq_ctx_reload_v()             (0x00000004U)
#define ccsr_channel_status_on_pbdma_v()                           (0x00000005U)
#define ccsr_channel_status_on_pbdma_and_eng_v()                   (0x00000006U)
#define ccsr_channel_status_on_eng_v()                             (0x00000007U)
#define ccsr_channel_status_on_eng_pending_acquire_v()             (0x00000008U)
#define ccsr_channel_status_on_eng_pending_v()                     (0x00000009U)
#define ccsr_channel_status_on_pbdma_ctx_reload_v()                (0x0000000aU)
#define ccsr_channel_status_on_pbdma_and_eng_ctx_reload_v()        (0x0000000bU)
#define ccsr_channel_status_on_eng_ctx_reload_v()                  (0x0000000cU)
#define ccsr_channel_status_on_eng_pending_ctx_reload_v()          (0x0000000dU)
#define ccsr_channel_status_on_eng_pending_acq_ctx_reload_v()      (0x0000000eU)
#define ccsr_channel_next_v(r)                              (((r) >> 1U) & 0x1U)
#define ccsr_channel_next_true_v()                                 (0x00000001U)
#define ccsr_channel_force_ctx_reload_true_f()                          (0x100U)
#define ccsr_channel_pbdma_faulted_f(v)                 ((U32(v) & 0x1U) << 22U)
#define ccsr_channel_pbdma_faulted_reset_f()                         (0x400000U)
#define ccsr_channel_eng_faulted_f(v)                   ((U32(v) & 0x1U) << 23U)
#define ccsr_channel_eng_faulted_v(r)                      (((r) >> 23U) & 0x1U)
#define ccsr_channel_eng_faulted_reset_f()                           (0x800000U)
#define ccsr_channel_eng_faulted_true_v()                          (0x00000001U)
#define ccsr_channel_busy_v(r)                             (((r) >> 28U) & 0x1U)
#define ccsr_channel_busy_true_v()                                 (0x00000001U)
#endif
