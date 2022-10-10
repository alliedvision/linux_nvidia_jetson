/*
 * Copyright (c) 2020-2022, NVIDIA CORPORATION.  All rights reserved.
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
#ifndef NVGPU_HW_TOP_GA100_H
#define NVGPU_HW_TOP_GA100_H

#include <nvgpu/types.h>
#include <nvgpu/static_analysis.h>

#define top_num_gpcs_r()                                           (0x00022430U)
#define top_num_gpcs_value_v(r)                            (((r) >> 0U) & 0x1fU)
#define top_tpc_per_gpc_r()                                        (0x00022434U)
#define top_tpc_per_gpc_value_v(r)                         (((r) >> 0U) & 0x1fU)
#define top_num_fbps_r()                                           (0x00022438U)
#define top_num_fbps_value_v(r)                            (((r) >> 0U) & 0x1fU)
#define top_num_fbpas_r()                                          (0x0002243cU)
#define top_num_fbpas_value_v(r)                           (((r) >> 0U) & 0x1fU)
#define top_ltc_per_fbp_r()                                        (0x00022450U)
#define top_ltc_per_fbp_value_v(r)                         (((r) >> 0U) & 0x1fU)
#define top_slices_per_ltc_r()                                     (0x0002245cU)
#define top_slices_per_ltc_value_v(r)                      (((r) >> 0U) & 0x1fU)
#define top_num_ltcs_r()                                           (0x00022454U)
#define top_num_ltcs_value_v(r)                            (((r) >> 0U) & 0x1fU)
#define top_num_ces_r()                                            (0x00022444U)
#define top_num_ces_value_v(r)                             (((r) >> 0U) & 0x1fU)
#define top_num_pes_r()                                            (0x00022460U)
#define top_num_pes_value_v(r)                             (((r) >> 0U) & 0x1fU)
#define top_device_info_cfg_r()                                    (0x000224fcU)
#define top_device_info_cfg_version_v(r)                    (((r) >> 0U) & 0xfU)
#define top_device_info_cfg_version_init_v()                       (0x00000002U)
#define top_device_info_cfg_num_rows_v(r)                (((r) >> 20U) & 0xfffU)
#define top_device_info2_r(i)\
		(nvgpu_safe_add_u32(0x00022800U, nvgpu_safe_mult_u32((i), 4U)))
#define top_device_info2_row_chain_v(r)                    (((r) >> 31U) & 0x1U)
#define top_device_info2_row_chain_more_v()                        (0x00000001U)
#define top_device_info2_row_chain_last_v()                        (0x00000000U)
#define top_device_info2_dev_fault_id_v(r)                (((r) >> 0U) & 0x7ffU)
#define top_device_info2_dev_instance_id_v(r)             (((r) >> 16U) & 0xffU)
#define top_device_info2_dev_type_enum_v(r)               (((r) >> 24U) & 0x7fU)
#define top_device_info2_dev_reset_id_v(r)                 (((r) >> 0U) & 0xffU)
#define top_device_info2_dev_device_pri_base_v(r)       (((r) >> 8U) & 0x3ffffU)
#define top_device_info2_dev_device_pri_base_b()                            (8U)
#define top_device_info2_dev_is_engine_v(r)                (((r) >> 30U) & 0x1U)
#define top_device_info2_dev_is_engine_true_v()                    (0x00000001U)
#define top_device_info2_dev_rleng_id_v(r)                  (((r) >> 0U) & 0x3U)
#define top_device_info2_dev_runlist_pri_base_v(r)      (((r) >> 10U) & 0xffffU)
#define top_device_info2_dev_runlist_pri_base_b()                          (10U)
#endif
