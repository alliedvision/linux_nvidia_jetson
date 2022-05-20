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
#ifndef NVGPU_HW_SMCARB_GA100_H
#define NVGPU_HW_SMCARB_GA100_H

#include <nvgpu/types.h>
#include <nvgpu/static_analysis.h>

#define smcarb_smc_partition_gpc_map_r(i)\
		(nvgpu_safe_add_u32(0x0000cc00U, nvgpu_safe_mult_u32((i), 4U)))
#define smcarb_smc_partition_gpc_map_sys_pipe_local_gpc_id_f(v)\
				((U32(v) & 0x1fU) << 0U)
#define smcarb_smc_partition_gpc_map_sys_pipe_local_gpc_id_m()\
				(U32(0x1fU) << 0U)
#define smcarb_smc_partition_gpc_map_sys_pipe_id_f(v)   ((U32(v) & 0x1fU) << 8U)
#define smcarb_smc_partition_gpc_map_sys_pipe_id_m()          (U32(0x1fU) << 8U)
#define smcarb_smc_partition_gpc_map_physical_gpc_id_v(r) (((r) >> 16U) & 0x1fU)
#define smcarb_smc_partition_gpc_map_ugpu_id_v(r)          (((r) >> 24U) & 0x1U)
#define smcarb_smc_partition_gpc_map_valid_f(v)         ((U32(v) & 0x1U) << 31U)
#define smcarb_smc_partition_gpc_map_valid_m()                (U32(0x1U) << 31U)
#define smcarb_smc_partition_gpc_map_valid_true_v()                (0x00000001U)
#define smcarb_smc_partition_gpc_map_valid_false_v()               (0x00000000U)
#define smcarb_sys_pipe_info_r()                                   (0x0000cc80U)
#define smcarb_sys_pipe_info_mode_f(v)                   ((U32(v) & 0x1U) << 0U)
#define smcarb_sys_pipe_info_mode_m()                          (U32(0x1U) << 0U)
#define smcarb_sys_pipe_info_mode_v(r)                      (((r) >> 0U) & 0x1U)
#define smcarb_sys_pipe_info_mode_legacy_v()                       (0x00000000U)
#define smcarb_sys_pipe_info_mode_smc_v()                          (0x00000001U)
#define smcarb_ugpu_gpc_count_r()                                  (0x0000cc84U)
#define smcarb_ugpu_gpc_count_ugpu0_v(r)                   (((r) >> 0U) & 0x1fU)
#define smcarb_ugpu_gpc_count_ugpu1_v(r)                   (((r) >> 8U) & 0x1fU)
#define smcarb_timestamp_ctrl_r()                                  (0x0000cc8cU)
#define smcarb_timestamp_ctrl_disable_tick_m()                 (U32(0x1U) << 0U)
#define smcarb_timestamp_ctrl_disable_tick__prod_f()                      (0x0U)
#define smcarb_max_partitionable_sys_pipes_v()                     (0x00000008U)
#define smcarb_allowed_swizzid__size1_v()                          (0x0000000fU)
#endif
