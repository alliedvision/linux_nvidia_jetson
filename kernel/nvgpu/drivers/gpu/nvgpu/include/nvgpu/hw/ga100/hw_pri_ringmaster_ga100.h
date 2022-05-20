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
#ifndef NVGPU_HW_PRI_RINGMASTER_GA100_H
#define NVGPU_HW_PRI_RINGMASTER_GA100_H

#include <nvgpu/types.h>
#include <nvgpu/static_analysis.h>

#define pri_ringmaster_command_r()                                 (0x0012004cU)
#define pri_ringmaster_command_cmd_m()                        (U32(0x3fU) << 0U)
#define pri_ringmaster_command_cmd_v(r)                    (((r) >> 0U) & 0x3fU)
#define pri_ringmaster_command_cmd_no_cmd_v()                      (0x00000000U)
#define pri_ringmaster_command_cmd_ack_interrupt_f()                      (0x2U)
#define pri_ringmaster_command_cmd_enumerate_and_start_ring_f()           (0x4U)
#define pri_ringmaster_master_ring_start_results_r()               (0x00120050U)
#define pri_ringmaster_master_ring_start_results_connectivity_pass_f()    (0x1U)
#define pri_ringmaster_intr_status0_r()                            (0x00120058U)
#define pri_ringmaster_intr_status0_ring_start_conn_fault_v(r)\
				(((r) >> 0U) & 0x1U)
#define pri_ringmaster_intr_status0_disconnect_fault_v(r)   (((r) >> 1U) & 0x1U)
#define pri_ringmaster_intr_status0_overflow_fault_v(r)     (((r) >> 2U) & 0x1U)
#define pri_ringmaster_intr_status0_ring_enum_fault_v(r)    (((r) >> 3U) & 0x1U)
#define pri_ringmaster_intr_status0_gpc_rs_map_config_fault_v(r)\
				(((r) >> 4U) & 0x1U)
#define pri_ringmaster_intr_status0_gbl_write_error_fbp_v(r)\
				(((r) >> 16U) & 0xffffU)
#define pri_ringmaster_intr_status0_gbl_write_error_sys_v(r)\
				(((r) >> 8U) & 0x1U)
#define pri_ringmaster_intr_status1_r()                            (0x0012005cU)
#define pri_ringmaster_enum_fbp_r()                                (0x00120074U)
#define pri_ringmaster_enum_fbp_count_v(r)                 (((r) >> 0U) & 0x1fU)
#define pri_ringmaster_enum_gpc_r()                                (0x00120078U)
#define pri_ringmaster_enum_gpc_count_v(r)                 (((r) >> 0U) & 0x1fU)
#define pri_ringmaster_enum_ltc_r()                                (0x0012006cU)
#define pri_ringmaster_gpc_rs_map_r(i)\
		(nvgpu_safe_add_u32(0x001200c0U, nvgpu_safe_mult_u32((i), 4U)))
#define pri_ringmaster_gpc_rs_map_smc_valid_f(v)        ((U32(v) & 0x1U) << 31U)
#define pri_ringmaster_gpc_rs_map_smc_valid_m()               (U32(0x1U) << 31U)
#define pri_ringmaster_gpc_rs_map_smc_valid_true_v()               (0x00000001U)
#define pri_ringmaster_gpc_rs_map_smc_valid_false_v()              (0x00000000U)
#define pri_ringmaster_gpc_rs_map_smc_engine_id_f(v)     ((U32(v) & 0x7U) << 8U)
#define pri_ringmaster_gpc_rs_map_smc_engine_id_m()            (U32(0x7U) << 8U)
#define pri_ringmaster_gpc_rs_map_smc_engine_local_cluster_id_f(v)\
				((U32(v) & 0x7U) << 0U)
#define pri_ringmaster_gpc_rs_map_smc_engine_local_cluster_id_m()\
				(U32(0x7U) << 0U)
#endif
