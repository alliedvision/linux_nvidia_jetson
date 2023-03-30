/*
 * Copyright (c) 2019-2022, NVIDIA CORPORATION.  All rights reserved.
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
#ifndef NVGPU_HW_PGSP_GA10B_H
#define NVGPU_HW_PGSP_GA10B_H

#include <nvgpu/types.h>
#include <nvgpu/static_analysis.h>

#define pgsp_falcon2_gsp_base_r()                                  (0x00111000U)
#define pgsp_falcon_irqsset_r()                                    (0x00110000U)
#define pgsp_falcon_engine_r()                                     (0x001103c0U)
#define pgsp_falcon_engine_reset_true_f()                                 (0x1U)
#define pgsp_falcon_engine_reset_false_f()                                (0x0U)
#define pgsp_falcon_irqstat_r()                                    (0x00110008U)
#define pgsp_falcon_irqstat_halt_true_f()                                (0x10U)
#define pgsp_falcon_irqstat_exterr_true_f()                              (0x20U)
#define pgsp_falcon_irqstat_swgen0_true_f()                              (0x40U)
#define pgsp_falcon_irqstat_swgen1_true_f()                              (0x80U)
#define pgsp_falcon_irqsclr_r()                                    (0x00110004U)
#define pgsp_falcon_irqmode_r()                                    (0x0011000cU)
#define pgsp_riscv_irqmset_r()                                     (0x00111520U)
#define pgsp_riscv_irqmset_gptmr_f(v)                    ((U32(v) & 0x1U) << 0U)
#define pgsp_riscv_irqmset_wdtmr_f(v)                    ((U32(v) & 0x1U) << 1U)
#define pgsp_riscv_irqmset_mthd_f(v)                     ((U32(v) & 0x1U) << 2U)
#define pgsp_riscv_irqmset_ctxsw_f(v)                    ((U32(v) & 0x1U) << 3U)
#define pgsp_riscv_irqmset_halt_f(v)                     ((U32(v) & 0x1U) << 4U)
#define pgsp_riscv_irqmset_exterr_f(v)                   ((U32(v) & 0x1U) << 5U)
#define pgsp_riscv_irqmset_swgen0_f(v)                   ((U32(v) & 0x1U) << 6U)
#define pgsp_riscv_irqmset_swgen1_f(v)                   ((U32(v) & 0x1U) << 7U)
#define pgsp_riscv_irqmclr_r()                                     (0x00111524U)
#define pgsp_riscv_irqmclr_gptmr_f(v)                    ((U32(v) & 0x1U) << 0U)
#define pgsp_riscv_irqmclr_wdtmr_f(v)                    ((U32(v) & 0x1U) << 1U)
#define pgsp_riscv_irqmclr_mthd_f(v)                     ((U32(v) & 0x1U) << 2U)
#define pgsp_riscv_irqmclr_ctxsw_f(v)                    ((U32(v) & 0x1U) << 3U)
#define pgsp_riscv_irqmclr_halt_f(v)                     ((U32(v) & 0x1U) << 4U)
#define pgsp_riscv_irqmclr_exterr_f(v)                   ((U32(v) & 0x1U) << 5U)
#define pgsp_riscv_irqmclr_swgen0_f(v)                   ((U32(v) & 0x1U) << 6U)
#define pgsp_riscv_irqmclr_swgen1_f(v)                   ((U32(v) & 0x1U) << 7U)
#define pgsp_riscv_irqmclr_ext_f(v)                     ((U32(v) & 0xffU) << 8U)
#define pgsp_riscv_irqmask_r()                                     (0x00111528U)
#define pgsp_riscv_irqdest_r()                                     (0x0011152cU)
#define pgsp_riscv_irqdest_gptmr_f(v)                    ((U32(v) & 0x1U) << 0U)
#define pgsp_riscv_irqdest_wdtmr_f(v)                    ((U32(v) & 0x1U) << 1U)
#define pgsp_riscv_irqdest_mthd_f(v)                     ((U32(v) & 0x1U) << 2U)
#define pgsp_riscv_irqdest_ctxsw_f(v)                    ((U32(v) & 0x1U) << 3U)
#define pgsp_riscv_irqdest_halt_f(v)                     ((U32(v) & 0x1U) << 4U)
#define pgsp_riscv_irqdest_exterr_f(v)                   ((U32(v) & 0x1U) << 5U)
#define pgsp_riscv_irqdest_swgen0_f(v)                   ((U32(v) & 0x1U) << 6U)
#define pgsp_riscv_irqdest_swgen1_f(v)                   ((U32(v) & 0x1U) << 7U)
#define pgsp_riscv_irqdest_ext_f(v)                     ((U32(v) & 0xffU) << 8U)
#define pgsp_fbif_transcfg_r(i)\
		(nvgpu_safe_add_u32(0x00110600U, nvgpu_safe_mult_u32((i), 4U)))
#define pgsp_fbif_transcfg_target_local_fb_f()                            (0x0U)
#define pgsp_fbif_transcfg_target_coherent_sysmem_f()                     (0x1U)
#define pgsp_fbif_transcfg_target_noncoherent_sysmem_f()                  (0x2U)
#define pgsp_fbif_transcfg_mem_type_s()                                     (1U)
#define pgsp_fbif_transcfg_mem_type_f(v)                 ((U32(v) & 0x1U) << 2U)
#define pgsp_fbif_transcfg_mem_type_m()                        (U32(0x1U) << 2U)
#define pgsp_fbif_transcfg_mem_type_v(r)                    (((r) >> 2U) & 0x1U)
#define pgsp_fbif_transcfg_mem_type_virtual_f()                           (0x0U)
#define pgsp_fbif_transcfg_mem_type_physical_f()                          (0x4U)
#define pgsp_hwcfg_r()                                             (0x00110abcU)
#define pgsp_hwcfg_emem_size_f(v)                      ((U32(v) & 0x1ffU) << 0U)
#define pgsp_hwcfg_emem_size_m()                             (U32(0x1ffU) << 0U)
#define pgsp_hwcfg_emem_size_v(r)                         (((r) >> 0U) & 0x1ffU)
#define pgsp_falcon_hwcfg1_r()                                     (0x0011012cU)
#define pgsp_falcon_hwcfg1_dmem_tag_width_f(v)         ((U32(v) & 0x1fU) << 21U)
#define pgsp_falcon_hwcfg1_dmem_tag_width_m()                (U32(0x1fU) << 21U)
#define pgsp_falcon_hwcfg1_dmem_tag_width_v(r)            (((r) >> 21U) & 0x1fU)
#define pgsp_ememc_r(i)\
		(nvgpu_safe_add_u32(0x00110ac0U, nvgpu_safe_mult_u32((i), 8U)))
#define pgsp_ememc__size_1_v()                                     (0x00000004U)
#define pgsp_ememc_blk_f(v)                             ((U32(v) & 0xffU) << 8U)
#define pgsp_ememc_blk_m()                                    (U32(0xffU) << 8U)
#define pgsp_ememc_blk_v(r)                                (((r) >> 8U) & 0xffU)
#define pgsp_ememc_offs_f(v)                            ((U32(v) & 0x3fU) << 2U)
#define pgsp_ememc_offs_m()                                   (U32(0x3fU) << 2U)
#define pgsp_ememc_offs_v(r)                               (((r) >> 2U) & 0x3fU)
#define pgsp_ememc_aincw_f(v)                           ((U32(v) & 0x1U) << 24U)
#define pgsp_ememc_aincw_m()                                  (U32(0x1U) << 24U)
#define pgsp_ememc_aincw_v(r)                              (((r) >> 24U) & 0x1U)
#define pgsp_ememc_aincr_f(v)                           ((U32(v) & 0x1U) << 25U)
#define pgsp_ememc_aincr_m()                                  (U32(0x1U) << 25U)
#define pgsp_ememc_aincr_v(r)                              (((r) >> 25U) & 0x1U)
#define pgsp_ememd_r(i)\
		(nvgpu_safe_add_u32(0x00110ac4U, nvgpu_safe_mult_u32((i), 8U)))
#define pgsp_ememd__size_1_v()                                     (0x00000004U)
#define pgsp_ememd_data_f(v)                      ((U32(v) & 0xffffffffU) << 0U)
#define pgsp_ememd_data_m()                             (U32(0xffffffffU) << 0U)
#define pgsp_ememd_data_v(r)                         (((r) >> 0U) & 0xffffffffU)
#define pgsp_queue_head_r(i)\
		(nvgpu_safe_add_u32(0x00110c00U, nvgpu_safe_mult_u32((i), 8U)))
#define pgsp_queue_head__size_1_v()                                (0x00000008U)
#define pgsp_queue_head_address_f(v)              ((U32(v) & 0xffffffffU) << 0U)
#define pgsp_queue_head_address_v(r)                 (((r) >> 0U) & 0xffffffffU)
#define pgsp_queue_tail_r(i)\
		(nvgpu_safe_add_u32(0x00110c04U, nvgpu_safe_mult_u32((i), 8U)))
#define pgsp_queue_tail__size_1_v()                                (0x00000008U)
#define pgsp_queue_tail_address_f(v)              ((U32(v) & 0xffffffffU) << 0U)
#define pgsp_queue_tail_address_v(r)                 (((r) >> 0U) & 0xffffffffU)
#define pgsp_msgq_head_r(i)\
		(nvgpu_safe_add_u32(0x00110c80U, nvgpu_safe_mult_u32((i), 8U)))
#define pgsp_msgq_head__size_1_v()                                 (0x00000008U)
#define pgsp_msgq_head_val_f(v)                   ((U32(v) & 0xffffffffU) << 0U)
#define pgsp_msgq_head_val_v(r)                      (((r) >> 0U) & 0xffffffffU)
#define pgsp_msgq_tail_r(i)\
		(nvgpu_safe_add_u32(0x00110c84U, nvgpu_safe_mult_u32((i), 8U)))
#define pgsp_msgq_tail__size_1_v()                                 (0x00000008U)
#define pgsp_msgq_tail_val_f(v)                   ((U32(v) & 0xffffffffU) << 0U)
#define pgsp_msgq_tail_val_v(r)                      (((r) >> 0U) & 0xffffffffU)
#define pgsp_falcon_exterraddr_r()                                 (0x00110168U)
#define pgsp_falcon_exterrstat_r()                                 (0x0011016cU)
#define pgsp_falcon_exterrstat_valid_m()                      (U32(0x1U) << 31U)
#define pgsp_falcon_exterrstat_valid_v(r)                  (((r) >> 31U) & 0x1U)
#define pgsp_falcon_exterrstat_valid_true_v()                      (0x00000001U)
#define pgsp_falcon_ecc_status_r()                                 (0x00110878U)
#define pgsp_falcon_ecc_status_uncorrected_err_imem_m()        (U32(0x1U) << 8U)
#define pgsp_falcon_ecc_status_uncorrected_err_dmem_m()        (U32(0x1U) << 9U)
#define pgsp_falcon_ecc_status_uncorrected_err_emem_m()       (U32(0x1U) << 13U)
#define pgsp_falcon_ecc_status_uncorrected_err_dcls_m()       (U32(0x1U) << 11U)
#define pgsp_falcon_ecc_status_uncorrected_err_reg_m()        (U32(0x1U) << 12U)
#endif
