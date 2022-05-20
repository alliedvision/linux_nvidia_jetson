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
#ifndef NVGPU_HW_PRI_SYS_GA10B_H
#define NVGPU_HW_PRI_SYS_GA10B_H

#include <nvgpu/types.h>
#include <nvgpu/static_analysis.h>

#define pri_sys_priv_error_adr_r()                                 (0x00122120U)
#define pri_sys_priv_error_wrdat_r()                               (0x00122124U)
#define pri_sys_priv_error_info_r()                                (0x00122128U)
#define pri_sys_priv_error_info_subid_v(r)                (((r) >> 24U) & 0xffU)
#define pri_sys_priv_error_info_local_ordering_v(r)        (((r) >> 22U) & 0x1U)
#define pri_sys_priv_error_info_priv_level_v(r)            (((r) >> 20U) & 0x3U)
#define pri_sys_priv_error_info_priv_master_v(r)           (((r) >> 0U) & 0xffU)
#define pri_sys_priv_error_code_r()                                (0x0012212cU)
#define pri_sys_pri_fence_r()                                      (0x001221fcU)
#define pri_sys_pri_error_r()                                      (0x00000000U)
#define pri_sys_pri_error_code_v(r)                    (((r) >> 8U) & 0xffffffU)
#define pri_sys_pri_error_code_host_fecs_err_v()                   (0x00bad00fU)
#define pri_sys_pri_error_code_host_pri_timeout_v()                (0x00bad001U)
#define pri_sys_pri_error_code_host_fb_ack_timeout_v()             (0x00bad0b0U)
#define pri_sys_pri_error_code_fecs_pri_timeout_v()                (0x00badf10U)
#define pri_sys_pri_error_code_fecs_pri_orphan_v()                 (0x00badf20U)
#define pri_sys_pri_error_code_fecs_dead_ring_v()                  (0x00badf30U)
#define pri_sys_pri_error_code_fecs_trap_v()                       (0x00badf40U)
#define pri_sys_pri_error_code_fecs_pri_client_err_v()             (0x00badf50U)
#define pri_sys_pri_error_code_fecs_pri_lock_from_security_sensor_v()\
				(0x00badf60U)
#define pri_sys_pri_error_extra_v(r)                       (((r) >> 0U) & 0xffU)
#define pri_sys_pri_error_extra_async_idle_v()                     (0x00000001U)
#define pri_sys_pri_error_extra_extra_sync_req_v()                 (0x00000020U)
#define pri_sys_pri_error_extra_no_such_address_v()                (0x00000040U)
#define pri_sys_pri_error_fecs_pri_route_err_extra_write_only_v()  (0x00000045U)
#define pri_sys_pri_error_local_priv_ring_extra_no_such_target_v() (0x00000080U)
#endif
