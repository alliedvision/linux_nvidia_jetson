/*
 * Copyright (c) 2016-2021, NVIDIA CORPORATION.  All rights reserved.
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
#ifndef NVGPU_HW_PROJ_GV11B_H
#define NVGPU_HW_PROJ_GV11B_H

#include <nvgpu/types.h>
#include <nvgpu/static_analysis.h>

#define proj_gpc_base_v()                                          (0x00500000U)
#define proj_gpc_shared_base_v()                                   (0x00418000U)
#define proj_gpc_stride_v()                                        (0x00008000U)
#define proj_gpc_priv_stride_v()                                   (0x00000800U)
#define proj_ltc_stride_v()                                        (0x00002000U)
#define proj_lts_stride_v()                                        (0x00000200U)
#define proj_fbpa_stride_v()                                       (0x00004000U)
#define proj_ppc_in_gpc_base_v()                                   (0x00003000U)
#define proj_ppc_in_gpc_shared_base_v()                            (0x00003e00U)
#define proj_ppc_in_gpc_stride_v()                                 (0x00000200U)
#define proj_rop_base_v()                                          (0x00410000U)
#define proj_rop_shared_base_v()                                   (0x00408800U)
#define proj_rop_stride_v()                                        (0x00000400U)
#define proj_tpc_in_gpc_base_v()                                   (0x00004000U)
#define proj_tpc_in_gpc_stride_v()                                 (0x00000800U)
#define proj_tpc_in_gpc_shared_base_v()                            (0x00001800U)
#define proj_smpc_base_v()                                         (0x00000200U)
#define proj_smpc_shared_base_v()                                  (0x00000300U)
#define proj_smpc_unique_base_v()                                  (0x00000600U)
#define proj_smpc_stride_v()                                       (0x00000100U)
#define proj_host_num_engines_v()                                  (0x00000004U)
#define proj_host_num_pbdma_v()                                    (0x00000003U)
#define proj_scal_litter_num_tpc_per_gpc_v()                       (0x00000004U)
#define proj_scal_litter_num_fbps_v()                              (0x00000001U)
#define proj_scal_litter_num_fbpas_v()                             (0x00000001U)
#define proj_scal_litter_num_gpcs_v()                              (0x00000001U)
#define proj_scal_litter_num_pes_per_gpc_v()                       (0x00000002U)
#define proj_scal_litter_num_tpcs_per_pes_v()                      (0x00000002U)
#define proj_scal_litter_num_zcull_banks_v()                       (0x00000004U)
#define proj_scal_litter_num_sm_per_tpc_v()                        (0x00000002U)
#define proj_scal_litter_num_ltc_lts_sets_v()                      (0x00000040U)
#define proj_scal_litter_num_ltc_lts_ways_v()                      (0x00000010U)
#define proj_scal_max_gpcs_v()                                     (0x00000020U)
#define proj_scal_max_tpc_per_gpc_v()                              (0x00000008U)
#define proj_sm_unique_base_v()                                    (0x00000700U)
#define proj_sm_shared_base_v()                                    (0x00000680U)
#define proj_sm_stride_v()                                         (0x00000080U)
#endif
