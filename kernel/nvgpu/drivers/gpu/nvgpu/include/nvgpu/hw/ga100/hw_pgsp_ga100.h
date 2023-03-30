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
#ifndef NVGPU_HW_PGSP_GA100_H
#define NVGPU_HW_PGSP_GA100_H

#include <nvgpu/types.h>
#include <nvgpu/static_analysis.h>

#define pgsp_falcon_irqsset_r()                                    (0x00110000U)
#define pgsp_falcon_nxtctx_r()                                     (0x00110054U)
#define pgsp_falcon_nxtctx_ctxptr_f(v)             ((U32(v) & 0xfffffffU) << 0U)
#define pgsp_falcon_nxtctx_ctxtgt_fb_f()                                  (0x0U)
#define pgsp_falcon_nxtctx_ctxtgt_sys_coh_f()                      (0x20000000U)
#define pgsp_falcon_nxtctx_ctxtgt_sys_ncoh_f()                     (0x30000000U)
#define pgsp_falcon_nxtctx_ctxvalid_f(v)                ((U32(v) & 0x1U) << 30U)
#define pgsp_falcon_itfen_r()                                      (0x00110048U)
#define pgsp_falcon_itfen_ctxen_enable_f()                                (0x1U)
#define pgsp_falcon_engctl_r()                                     (0x001100a4U)
#define pgsp_falcon_engctl_switch_context_true_f()                        (0x8U)
#define pgsp_falcon_debug1_r()                                     (0x00110090U)
#define pgsp_falcon_debug1_ctxsw_mode_m()                     (U32(0x1U) << 16U)
#define pgsp_fbif_transcfg_r(i)\
		(nvgpu_safe_add_u32(0x00110600U, nvgpu_safe_mult_u32((i), 4U)))
#define pgsp_fbif_transcfg_target_local_fb_f()                            (0x0U)
#define pgsp_fbif_transcfg_target_coherent_sysmem_f()                     (0x1U)
#define pgsp_fbif_transcfg_target_noncoherent_sysmem_f()                  (0x2U)
#define pgsp_fbif_transcfg_mem_type_v(r)                    (((r) >> 2U) & 0x1U)
#define pgsp_fbif_transcfg_mem_type_virtual_f()                           (0x0U)
#define pgsp_fbif_transcfg_mem_type_physical_f()                          (0x4U)
#define pgsp_falcon_engine_r()                                     (0x001103c0U)
#define pgsp_falcon_engine_reset_true_f()                                 (0x1U)
#define pgsp_falcon_engine_reset_false_f()                                (0x0U)
#define pgsp_fbif_ctl_r()                                          (0x00110624U)
#define pgsp_fbif_ctl_allow_phys_no_ctx_allow_f()                        (0x80U)
#endif
