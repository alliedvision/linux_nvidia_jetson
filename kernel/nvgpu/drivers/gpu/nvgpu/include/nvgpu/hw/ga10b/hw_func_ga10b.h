/*
 * Copyright (c) 2019-2021, NVIDIA CORPORATION.  All rights reserved.
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
#ifndef NVGPU_HW_FUNC_GA10B_H
#define NVGPU_HW_FUNC_GA10B_H

#include <nvgpu/types.h>
#include <nvgpu/static_analysis.h>

#define func_full_phys_offset_v()                                  (0x00b80000U)
#define func_doorbell_r()                                          (0x00030090U)
#define func_cfg0_r()                                              (0x00030000U)
#define func_priv_cpu_intr_top_en_set_r(i)\
		(nvgpu_safe_add_u32(0x00001608U, nvgpu_safe_mult_u32((i), 4U)))
#define func_priv_cpu_intr_top_en_clear_r(i)\
		(nvgpu_safe_add_u32(0x00001610U, nvgpu_safe_mult_u32((i), 4U)))
#define func_priv_cpu_intr_top_en_clear__size_1_v()                (0x00000001U)
#define func_priv_cpu_intr_top_en_set_r(i)\
		(nvgpu_safe_add_u32(0x00001608U, nvgpu_safe_mult_u32((i), 4U)))
#define func_priv_cpu_intr_leaf_en_set_r(i)\
		(nvgpu_safe_add_u32(0x00001200U, nvgpu_safe_mult_u32((i), 4U)))
#define func_priv_cpu_intr_leaf_en_clear_r(i)\
		(nvgpu_safe_add_u32(0x00001400U, nvgpu_safe_mult_u32((i), 4U)))
#define func_priv_cpu_intr_top_r(i)\
		(nvgpu_safe_add_u32(0x00001600U, nvgpu_safe_mult_u32((i), 4U)))
#define func_priv_cpu_intr_top__size_1_v()                         (0x00000001U)
#define func_priv_cpu_intr_leaf_r(i)\
		(nvgpu_safe_add_u32(0x00001000U, nvgpu_safe_mult_u32((i), 4U)))
#define func_priv_cpu_intr_leaf__size_1_v()                        (0x00000008U)
#define func_priv_cpu_intr_pfb_vector_v()                          (0x0000008dU)
#define func_priv_cpu_intr_pmu_vector_v()                          (0x00000098U)
#define func_priv_cpu_intr_ltc_all_vector_v()                      (0x00000099U)
#define func_priv_cpu_intr_gsp_vector_v()                          (0x0000009bU)
#define func_priv_cpu_intr_pbus_vector_v()                         (0x0000009cU)
#define func_priv_cpu_intr_priv_ring_vector_v()                    (0x0000009eU)
#endif
