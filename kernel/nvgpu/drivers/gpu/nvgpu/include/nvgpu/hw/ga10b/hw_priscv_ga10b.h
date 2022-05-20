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
#ifndef NVGPU_HW_PRISCV_GA10B_H
#define NVGPU_HW_PRISCV_GA10B_H

#include <nvgpu/types.h>
#include <nvgpu/static_analysis.h>

#define priscv_priscv_cpuctl_r()                                   (0x00000388U)
#define priscv_priscv_cpuctl_startcpu_true_f()                            (0x1U)
#define priscv_priscv_cpuctl_halted_v(r)                    (((r) >> 4U) & 0x1U)
#define priscv_priscv_br_retcode_r()                               (0x0000065cU)
#define priscv_priscv_br_retcode_result_v(r)                (((r) >> 0U) & 0x3U)
#define priscv_priscv_br_retcode_result_fail_f()                          (0x2U)
#define priscv_priscv_br_retcode_result_pass_f()                          (0x3U)
#define priscv_priscv_bcr_ctrl_r()                                 (0x00000668U)
#define priscv_priscv_bcr_dmaaddr_fmccode_lo_r()                   (0x00000678U)
#define priscv_priscv_bcr_dmaaddr_fmccode_hi_r()                   (0x0000067cU)
#define priscv_priscv_bcr_dmaaddr_fmcdata_lo_r()                   (0x00000680U)
#define priscv_priscv_bcr_dmaaddr_fmcdata_hi_r()                   (0x00000684U)
#define priscv_priscv_bcr_dmaaddr_pkcparam_lo_r()                  (0x00000670U)
#define priscv_priscv_bcr_dmaaddr_pkcparam_hi_r()                  (0x00000674U)
#define priscv_priscv_bcr_dmacfg_r()                               (0x0000066cU)
#define priscv_priscv_bcr_dmacfg_target_noncoherent_system_f()            (0x2U)
#define priscv_priscv_bcr_dmacfg_lock_locked_f()                   (0x80000000U)
#define priscv_riscv_irqmask_r()                                   (0x00000528U)
#define priscv_riscv_irqdest_r()                                   (0x0000052cU)
#endif
