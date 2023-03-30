/*
 * Copyright (c) 2018-2020, NVIDIA CORPORATION.  All rights reserved.
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
#ifndef NVGPU_HW_PERF_TU104_H
#define NVGPU_HW_PERF_TU104_H

#include <nvgpu/types.h>
#include <nvgpu/static_analysis.h>

#define perf_pmmgpc_perdomain_offset_v()                           (0x00000200U)
#define perf_pmmsys_perdomain_offset_v()                           (0x00000200U)
#define perf_pmmgpc_base_v()                                       (0x00180000U)
#define perf_pmmgpc_extent_v()                                     (0x00183fffU)
#define perf_pmmsys_base_v()                                       (0x00240000U)
#define perf_pmmsys_extent_v()                                     (0x00243fffU)
#define perf_pmmfbp_base_v()                                       (0x00200000U)
#define perf_pmmfbp_extent_v()                                     (0x00203fffU)
#define perf_pmasys_control_r()                                    (0x0024a000U)
#define perf_pmasys_control_membuf_status_v(r)              (((r) >> 4U) & 0x1U)
#define perf_pmasys_control_membuf_status_overflowed_v()           (0x00000001U)
#define perf_pmasys_control_membuf_status_overflowed_f()                 (0x10U)
#define perf_pmasys_control_membuf_clear_status_f(v)     ((U32(v) & 0x1U) << 5U)
#define perf_pmasys_control_membuf_clear_status_v(r)        (((r) >> 5U) & 0x1U)
#define perf_pmasys_control_membuf_clear_status_doit_v()           (0x00000001U)
#define perf_pmasys_control_membuf_clear_status_doit_f()                 (0x20U)
#define perf_pmasys_control_update_bytes_m()                   (U32(0x1U) << 3U)
#define perf_pmasys_control_update_bytes_doit_f()                         (0x8U)
#define perf_pmasys_control_stream_m()                         (U32(0x1U) << 0U)
#define perf_pmasys_control_stream_enable_f()                             (0x1U)
#define perf_pmasys_control_stream_disable_f()                            (0x0U)
#define perf_pmasys_mem_block_r()                                  (0x0024a070U)
#define perf_pmasys_mem_block_base_f(v)            ((U32(v) & 0xfffffffU) << 0U)
#define perf_pmasys_mem_block_target_f(v)               ((U32(v) & 0x3U) << 28U)
#define perf_pmasys_mem_block_target_v(r)                  (((r) >> 28U) & 0x3U)
#define perf_pmasys_mem_block_target_lfb_v()                       (0x00000000U)
#define perf_pmasys_mem_block_target_lfb_f()                              (0x0U)
#define perf_pmasys_mem_block_target_sys_coh_v()                   (0x00000002U)
#define perf_pmasys_mem_block_target_sys_coh_f()                   (0x20000000U)
#define perf_pmasys_mem_block_target_sys_ncoh_v()                  (0x00000003U)
#define perf_pmasys_mem_block_target_sys_ncoh_f()                  (0x30000000U)
#define perf_pmasys_mem_block_valid_f(v)                ((U32(v) & 0x1U) << 31U)
#define perf_pmasys_mem_block_valid_v(r)                   (((r) >> 31U) & 0x1U)
#define perf_pmasys_mem_block_valid_true_v()                       (0x00000001U)
#define perf_pmasys_mem_block_valid_true_f()                       (0x80000000U)
#define perf_pmasys_mem_block_valid_false_v()                      (0x00000000U)
#define perf_pmasys_mem_block_valid_false_f()                             (0x0U)
#define perf_pmasys_outbase_r()                                    (0x0024a074U)
#define perf_pmasys_outbase_ptr_f(v)               ((U32(v) & 0x7ffffffU) << 5U)
#define perf_pmasys_outbaseupper_r()                               (0x0024a078U)
#define perf_pmasys_outbaseupper_ptr_f(v)               ((U32(v) & 0xffU) << 0U)
#define perf_pmasys_outsize_r()                                    (0x0024a07cU)
#define perf_pmasys_outsize_numbytes_f(v)          ((U32(v) & 0x7ffffffU) << 5U)
#define perf_pmasys_mem_bytes_r()                                  (0x0024a084U)
#define perf_pmasys_mem_bytes_numbytes_f(v)        ((U32(v) & 0xfffffffU) << 4U)
#define perf_pmasys_mem_bump_r()                                   (0x0024a088U)
#define perf_pmasys_mem_bump_numbytes_f(v)         ((U32(v) & 0xfffffffU) << 4U)
#define perf_pmasys_mem_head_r()                                   (0x0024a080U)
#define perf_pmasys_mem_bytes_addr_r()                             (0x0024a08cU)
#define perf_pmasys_mem_bytes_addr_ptr_f(v)       ((U32(v) & 0x3fffffffU) << 2U)
#define perf_pmasys_mem_bytes_addr_ptr_b()                                  (2U)
#define perf_pmasys_enginestatus_r()                               (0x0024a0a4U)
#define perf_pmasys_enginestatus_rbufempty_f(v)          ((U32(v) & 0x1U) << 4U)
#define perf_pmasys_enginestatus_rbufempty_v(r)             (((r) >> 4U) & 0x1U)
#define perf_pmasys_enginestatus_rbufempty_empty_v()               (0x00000001U)
#define perf_pmasys_enginestatus_rbufempty_empty_f()                     (0x10U)
#define perf_pmasys_enginestatus_status_v(r)                (((r) >> 0U) & 0x7U)
#define perf_pmasys_enginestatus_status_empty_v()                  (0x00000000U)
#define perf_pmmsys_engine_sel_r(i)\
		(nvgpu_safe_add_u32(0x0024006cU, nvgpu_safe_mult_u32((i), 512U)))
#define perf_pmmsys_engine_sel__size_1_v()                         (0x00000020U)
#define perf_pmmfbp_engine_sel_r(i)\
		(nvgpu_safe_add_u32(0x0020006cU, nvgpu_safe_mult_u32((i), 512U)))
#define perf_pmmfbp_engine_sel__size_1_v()                         (0x00000020U)
#define perf_pmmgpc_engine_sel_r(i)\
		(nvgpu_safe_add_u32(0x0018006cU, nvgpu_safe_mult_u32((i), 512U)))
#define perf_pmmgpc_engine_sel__size_1_v()                         (0x00000020U)
#define perf_pmmsys_control_r(i)\
		(nvgpu_safe_add_u32(0x0024009cU, nvgpu_safe_mult_u32((i), 512U)))
#define perf_pmmfbp_fbps_control_r(i)\
		(nvgpu_safe_add_u32(0x0027c09cU, nvgpu_safe_mult_u32((i), 512U)))
#define perf_pmmgpc_gpcs_control_r(i)\
		(nvgpu_safe_add_u32(0x0027809cU, nvgpu_safe_mult_u32((i), 512U)))
#define perf_pmmsysrouter_perfmonstatus_r()                        (0x00248014U)
#define perf_pmmsysrouter_enginestatus_r()                         (0x00248010U)
#define perf_pmmsysrouter_enginestatus_status_v(r)          (((r) >> 0U) & 0x7U)
#define perf_pmmsysrouter_enginestatus_status_empty_v()            (0x00000000U)
#define perf_pmmsysrouter_enginestatus_status_quiescent_v()        (0x00000003U)
#define perf_pmmgpcrouter_perfmonstatus_r()                        (0x00244014U)
#define perf_pmmgpcrouter_enginestatus_r()                         (0x00244010U)
#define perf_pmmfbprouter_perfmonstatus_r()                        (0x00246014U)
#define perf_pmmfbprouter_enginestatus_r()                         (0x00246010U)
#endif
