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
#ifndef NVGPU_HW_NVLIPT_TU104_H
#define NVGPU_HW_NVLIPT_TU104_H

#include <nvgpu/types.h>
#include <nvgpu/static_analysis.h>

#define nvlipt_intr_control_link0_r()                              (0x000004b4U)
#define nvlipt_intr_control_link0_stallenable_f(v)       ((U32(v) & 0x1U) << 0U)
#define nvlipt_intr_control_link0_stallenable_m()              (U32(0x1U) << 0U)
#define nvlipt_intr_control_link0_stallenable_v(r)          (((r) >> 0U) & 0x1U)
#define nvlipt_intr_control_link0_nostallenable_f(v)     ((U32(v) & 0x1U) << 1U)
#define nvlipt_intr_control_link0_nostallenable_m()            (U32(0x1U) << 1U)
#define nvlipt_intr_control_link0_nostallenable_v(r)        (((r) >> 1U) & 0x1U)
#define nvlipt_err_uc_status_link0_r()                             (0x00000524U)
#define nvlipt_err_uc_status_link0_dlprotocol_f(v)       ((U32(v) & 0x1U) << 4U)
#define nvlipt_err_uc_status_link0_dlprotocol_v(r)          (((r) >> 4U) & 0x1U)
#define nvlipt_err_uc_status_link0_datapoisoned_f(v)    ((U32(v) & 0x1U) << 12U)
#define nvlipt_err_uc_status_link0_datapoisoned_v(r)       (((r) >> 12U) & 0x1U)
#define nvlipt_err_uc_status_link0_flowcontrol_f(v)     ((U32(v) & 0x1U) << 13U)
#define nvlipt_err_uc_status_link0_flowcontrol_v(r)        (((r) >> 13U) & 0x1U)
#define nvlipt_err_uc_status_link0_responsetimeout_f(v) ((U32(v) & 0x1U) << 14U)
#define nvlipt_err_uc_status_link0_responsetimeout_v(r)    (((r) >> 14U) & 0x1U)
#define nvlipt_err_uc_status_link0_targeterror_f(v)     ((U32(v) & 0x1U) << 15U)
#define nvlipt_err_uc_status_link0_targeterror_v(r)        (((r) >> 15U) & 0x1U)
#define nvlipt_err_uc_status_link0_unexpectedresponse_f(v)\
				((U32(v) & 0x1U) << 16U)
#define nvlipt_err_uc_status_link0_unexpectedresponse_v(r) (((r) >> 16U) & 0x1U)
#define nvlipt_err_uc_status_link0_receiveroverflow_f(v)\
				((U32(v) & 0x1U) << 17U)
#define nvlipt_err_uc_status_link0_receiveroverflow_v(r)   (((r) >> 17U) & 0x1U)
#define nvlipt_err_uc_status_link0_malformedpacket_f(v) ((U32(v) & 0x1U) << 18U)
#define nvlipt_err_uc_status_link0_malformedpacket_v(r)    (((r) >> 18U) & 0x1U)
#define nvlipt_err_uc_status_link0_stompedpacketreceived_f(v)\
				((U32(v) & 0x1U) << 19U)
#define nvlipt_err_uc_status_link0_stompedpacketreceived_v(r)\
				(((r) >> 19U) & 0x1U)
#define nvlipt_err_uc_status_link0_unsupportedrequest_f(v)\
				((U32(v) & 0x1U) << 20U)
#define nvlipt_err_uc_status_link0_unsupportedrequest_v(r) (((r) >> 20U) & 0x1U)
#define nvlipt_err_uc_status_link0_ucinternal_f(v)      ((U32(v) & 0x1U) << 22U)
#define nvlipt_err_uc_status_link0_ucinternal_v(r)         (((r) >> 22U) & 0x1U)
#define nvlipt_err_uc_mask_link0_r()                               (0x00000528U)
#define nvlipt_err_uc_severity_link0_r()                           (0x0000052cU)
#define nvlipt_err_uc_first_link0_r()                              (0x00000530U)
#define nvlipt_err_uc_advisory_link0_r()                           (0x00000534U)
#define nvlipt_err_c_status_link0_r()                              (0x00000538U)
#define nvlipt_err_c_mask_link0_r()                                (0x0000053cU)
#define nvlipt_err_c_first_link0_r()                               (0x00000540U)
#define nvlipt_err_control_link0_r()                               (0x00000544U)
#define nvlipt_err_control_link0_fatalenable_f(v)        ((U32(v) & 0x1U) << 1U)
#define nvlipt_err_control_link0_fatalenable_m()               (U32(0x1U) << 1U)
#define nvlipt_err_control_link0_fatalenable_v(r)           (((r) >> 1U) & 0x1U)
#define nvlipt_err_control_link0_nonfatalenable_f(v)     ((U32(v) & 0x1U) << 2U)
#define nvlipt_err_control_link0_nonfatalenable_m()            (U32(0x1U) << 2U)
#define nvlipt_err_control_link0_nonfatalenable_v(r)        (((r) >> 2U) & 0x1U)
#define nvlipt_intr_control_common_r()                             (0x000004b0U)
#define nvlipt_intr_control_common_stallenable_f(v)      ((U32(v) & 0x1U) << 0U)
#define nvlipt_intr_control_common_stallenable_m()             (U32(0x1U) << 0U)
#define nvlipt_intr_control_common_stallenable_v(r)         (((r) >> 0U) & 0x1U)
#define nvlipt_intr_control_common_nonstallenable_f(v)   ((U32(v) & 0x1U) << 1U)
#define nvlipt_intr_control_common_nonstallenable_m()          (U32(0x1U) << 1U)
#define nvlipt_intr_control_common_nonstallenable_v(r)      (((r) >> 1U) & 0x1U)
#define nvlipt_scratch_cold_r()                                    (0x000007d4U)
#define nvlipt_scratch_cold_data_f(v)             ((U32(v) & 0xffffffffU) << 0U)
#define nvlipt_scratch_cold_data_v(r)                (((r) >> 0U) & 0xffffffffU)
#define nvlipt_scratch_cold_data_init_v()                          (0xdeadbaadU)
#endif
