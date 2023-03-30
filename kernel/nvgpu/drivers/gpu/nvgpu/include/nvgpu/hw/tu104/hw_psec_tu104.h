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
#ifndef NVGPU_HW_PSEC_TU104_H
#define NVGPU_HW_PSEC_TU104_H

#include <nvgpu/types.h>
#include <nvgpu/static_analysis.h>

#define psec_falcon_irqsset_r()                                    (0x00840000U)
#define psec_falcon_irqsset_swgen0_set_f()                               (0x40U)
#define psec_falcon_irqsclr_r()                                    (0x00840004U)
#define psec_falcon_irqstat_r()                                    (0x00840008U)
#define psec_falcon_irqstat_halt_true_f()                                (0x10U)
#define psec_falcon_irqstat_exterr_true_f()                              (0x20U)
#define psec_falcon_irqstat_swgen0_true_f()                              (0x40U)
#define psec_falcon_irqmode_r()                                    (0x0084000cU)
#define psec_falcon_irqmset_r()                                    (0x00840010U)
#define psec_falcon_irqmset_gptmr_f(v)                   ((U32(v) & 0x1U) << 0U)
#define psec_falcon_irqmset_wdtmr_f(v)                   ((U32(v) & 0x1U) << 1U)
#define psec_falcon_irqmset_mthd_f(v)                    ((U32(v) & 0x1U) << 2U)
#define psec_falcon_irqmset_ctxsw_f(v)                   ((U32(v) & 0x1U) << 3U)
#define psec_falcon_irqmset_halt_f(v)                    ((U32(v) & 0x1U) << 4U)
#define psec_falcon_irqmset_exterr_f(v)                  ((U32(v) & 0x1U) << 5U)
#define psec_falcon_irqmset_swgen0_f(v)                  ((U32(v) & 0x1U) << 6U)
#define psec_falcon_irqmset_swgen1_f(v)                  ((U32(v) & 0x1U) << 7U)
#define psec_falcon_irqmclr_r()                                    (0x00840014U)
#define psec_falcon_irqmclr_gptmr_f(v)                   ((U32(v) & 0x1U) << 0U)
#define psec_falcon_irqmclr_wdtmr_f(v)                   ((U32(v) & 0x1U) << 1U)
#define psec_falcon_irqmclr_mthd_f(v)                    ((U32(v) & 0x1U) << 2U)
#define psec_falcon_irqmclr_ctxsw_f(v)                   ((U32(v) & 0x1U) << 3U)
#define psec_falcon_irqmclr_halt_f(v)                    ((U32(v) & 0x1U) << 4U)
#define psec_falcon_irqmclr_exterr_f(v)                  ((U32(v) & 0x1U) << 5U)
#define psec_falcon_irqmclr_swgen0_f(v)                  ((U32(v) & 0x1U) << 6U)
#define psec_falcon_irqmclr_swgen1_f(v)                  ((U32(v) & 0x1U) << 7U)
#define psec_falcon_irqmclr_ext_f(v)                    ((U32(v) & 0xffU) << 8U)
#define psec_falcon_irqmask_r()                                    (0x00840018U)
#define psec_falcon_irqdest_r()                                    (0x0084001cU)
#define psec_falcon_irqdest_host_gptmr_f(v)              ((U32(v) & 0x1U) << 0U)
#define psec_falcon_irqdest_host_wdtmr_f(v)              ((U32(v) & 0x1U) << 1U)
#define psec_falcon_irqdest_host_mthd_f(v)               ((U32(v) & 0x1U) << 2U)
#define psec_falcon_irqdest_host_ctxsw_f(v)              ((U32(v) & 0x1U) << 3U)
#define psec_falcon_irqdest_host_halt_f(v)               ((U32(v) & 0x1U) << 4U)
#define psec_falcon_irqdest_host_exterr_f(v)             ((U32(v) & 0x1U) << 5U)
#define psec_falcon_irqdest_host_swgen0_f(v)             ((U32(v) & 0x1U) << 6U)
#define psec_falcon_irqdest_host_swgen1_f(v)             ((U32(v) & 0x1U) << 7U)
#define psec_falcon_irqdest_host_ext_f(v)               ((U32(v) & 0xffU) << 8U)
#define psec_falcon_irqdest_target_gptmr_f(v)           ((U32(v) & 0x1U) << 16U)
#define psec_falcon_irqdest_target_wdtmr_f(v)           ((U32(v) & 0x1U) << 17U)
#define psec_falcon_irqdest_target_mthd_f(v)            ((U32(v) & 0x1U) << 18U)
#define psec_falcon_irqdest_target_ctxsw_f(v)           ((U32(v) & 0x1U) << 19U)
#define psec_falcon_irqdest_target_halt_f(v)            ((U32(v) & 0x1U) << 20U)
#define psec_falcon_irqdest_target_exterr_f(v)          ((U32(v) & 0x1U) << 21U)
#define psec_falcon_irqdest_target_swgen0_f(v)          ((U32(v) & 0x1U) << 22U)
#define psec_falcon_irqdest_target_swgen1_f(v)          ((U32(v) & 0x1U) << 23U)
#define psec_falcon_irqdest_target_ext_f(v)            ((U32(v) & 0xffU) << 24U)
#define psec_falcon_curctx_r()                                     (0x00840050U)
#define psec_falcon_nxtctx_r()                                     (0x00840054U)
#define psec_falcon_mailbox0_r()                                   (0x00840040U)
#define psec_falcon_mailbox1_r()                                   (0x00840044U)
#define psec_falcon_itfen_r()                                      (0x00840048U)
#define psec_falcon_itfen_ctxen_enable_f()                                (0x1U)
#define psec_falcon_idlestate_r()                                  (0x0084004cU)
#define psec_falcon_idlestate_falcon_busy_v(r)              (((r) >> 0U) & 0x1U)
#define psec_falcon_idlestate_ext_busy_v(r)              (((r) >> 1U) & 0x7fffU)
#define psec_falcon_os_r()                                         (0x00840080U)
#define psec_falcon_engctl_r()                                     (0x008400a4U)
#define psec_falcon_cpuctl_r()                                     (0x00840100U)
#define psec_falcon_cpuctl_startcpu_f(v)                 ((U32(v) & 0x1U) << 1U)
#define psec_falcon_cpuctl_halt_intr_f(v)                ((U32(v) & 0x1U) << 4U)
#define psec_falcon_cpuctl_halt_intr_m()                       (U32(0x1U) << 4U)
#define psec_falcon_cpuctl_halt_intr_v(r)                   (((r) >> 4U) & 0x1U)
#define psec_falcon_cpuctl_cpuctl_alias_en_f(v)          ((U32(v) & 0x1U) << 6U)
#define psec_falcon_cpuctl_cpuctl_alias_en_m()                 (U32(0x1U) << 6U)
#define psec_falcon_cpuctl_cpuctl_alias_en_v(r)             (((r) >> 6U) & 0x1U)
#define psec_falcon_cpuctl_alias_r()                               (0x00840130U)
#define psec_falcon_cpuctl_alias_startcpu_f(v)           ((U32(v) & 0x1U) << 1U)
#define psec_falcon_imemc_r(i)\
		(nvgpu_safe_add_u32(0x00840180U, nvgpu_safe_mult_u32((i), 16U)))
#define psec_falcon_imemc_offs_f(v)                     ((U32(v) & 0x3fU) << 2U)
#define psec_falcon_imemc_blk_f(v)                      ((U32(v) & 0xffU) << 8U)
#define psec_falcon_imemc_aincw_f(v)                    ((U32(v) & 0x1U) << 24U)
#define psec_falcon_imemd_r(i)\
		(nvgpu_safe_add_u32(0x00840184U, nvgpu_safe_mult_u32((i), 16U)))
#define psec_falcon_imemt_r(i)\
		(nvgpu_safe_add_u32(0x00840188U, nvgpu_safe_mult_u32((i), 16U)))
#define psec_falcon_sctl_r()                                       (0x00840240U)
#define psec_falcon_mmu_phys_sec_r()                               (0x00100ce4U)
#define psec_falcon_bootvec_r()                                    (0x00840104U)
#define psec_falcon_bootvec_vec_f(v)              ((U32(v) & 0xffffffffU) << 0U)
#define psec_falcon_dmactl_r()                                     (0x0084010cU)
#define psec_falcon_dmactl_dmem_scrubbing_m()                  (U32(0x1U) << 1U)
#define psec_falcon_dmactl_imem_scrubbing_m()                  (U32(0x1U) << 2U)
#define psec_falcon_dmactl_require_ctx_f(v)              ((U32(v) & 0x1U) << 0U)
#define psec_falcon_hwcfg_r()                                      (0x00840108U)
#define psec_falcon_hwcfg_imem_size_v(r)                  (((r) >> 0U) & 0x1ffU)
#define psec_falcon_hwcfg_dmem_size_v(r)                  (((r) >> 9U) & 0x1ffU)
#define psec_falcon_dmatrfbase_r()                                 (0x00840110U)
#define psec_falcon_dmatrfbase1_r()                                (0x00840128U)
#define psec_falcon_dmatrfmoffs_r()                                (0x00840114U)
#define psec_falcon_dmatrfcmd_r()                                  (0x00840118U)
#define psec_falcon_dmatrfcmd_imem_f(v)                  ((U32(v) & 0x1U) << 4U)
#define psec_falcon_dmatrfcmd_write_f(v)                 ((U32(v) & 0x1U) << 5U)
#define psec_falcon_dmatrfcmd_size_f(v)                  ((U32(v) & 0x7U) << 8U)
#define psec_falcon_dmatrfcmd_ctxdma_f(v)               ((U32(v) & 0x7U) << 12U)
#define psec_falcon_dmatrffboffs_r()                               (0x0084011cU)
#define psec_falcon_exterraddr_r()                                 (0x00840168U)
#define psec_falcon_exterrstat_r()                                 (0x0084016cU)
#define psec_falcon_exterrstat_valid_m()                      (U32(0x1U) << 31U)
#define psec_falcon_exterrstat_valid_v(r)                  (((r) >> 31U) & 0x1U)
#define psec_falcon_exterrstat_valid_true_v()                      (0x00000001U)
#define psec_sec2_falcon_icd_cmd_r()                               (0x00840200U)
#define psec_sec2_falcon_icd_cmd_opc_s()                                    (4U)
#define psec_sec2_falcon_icd_cmd_opc_f(v)                ((U32(v) & 0xfU) << 0U)
#define psec_sec2_falcon_icd_cmd_opc_m()                       (U32(0xfU) << 0U)
#define psec_sec2_falcon_icd_cmd_opc_v(r)                   (((r) >> 0U) & 0xfU)
#define psec_sec2_falcon_icd_cmd_opc_rreg_f()                             (0x8U)
#define psec_sec2_falcon_icd_cmd_opc_rstat_f()                            (0xeU)
#define psec_sec2_falcon_icd_cmd_idx_f(v)               ((U32(v) & 0x1fU) << 8U)
#define psec_sec2_falcon_icd_rdata_r()                             (0x0084020cU)
#define psec_falcon_dmemc_r(i)\
		(nvgpu_safe_add_u32(0x008401c0U, nvgpu_safe_mult_u32((i), 8U)))
#define psec_falcon_dmemc_offs_f(v)                     ((U32(v) & 0x3fU) << 2U)
#define psec_falcon_dmemc_offs_m()                            (U32(0x3fU) << 2U)
#define psec_falcon_dmemc_blk_f(v)                      ((U32(v) & 0xffU) << 8U)
#define psec_falcon_dmemc_blk_m()                             (U32(0xffU) << 8U)
#define psec_falcon_dmemc_aincw_f(v)                    ((U32(v) & 0x1U) << 24U)
#define psec_falcon_dmemc_aincr_f(v)                    ((U32(v) & 0x1U) << 25U)
#define psec_falcon_dmemd_r(i)\
		(nvgpu_safe_add_u32(0x008401c4U, nvgpu_safe_mult_u32((i), 8U)))
#define psec_falcon_debug1_r()                                     (0x00840090U)
#define psec_falcon_debug1_ctxsw_mode_s()                                   (1U)
#define psec_falcon_debug1_ctxsw_mode_f(v)              ((U32(v) & 0x1U) << 16U)
#define psec_falcon_debug1_ctxsw_mode_m()                     (U32(0x1U) << 16U)
#define psec_falcon_debug1_ctxsw_mode_v(r)                 (((r) >> 16U) & 0x1U)
#define psec_falcon_debug1_ctxsw_mode_init_f()                            (0x0U)
#define psec_fbif_transcfg_r(i)\
		(nvgpu_safe_add_u32(0x00840600U, nvgpu_safe_mult_u32((i), 4U)))
#define psec_fbif_transcfg_target_local_fb_f()                            (0x0U)
#define psec_fbif_transcfg_target_coherent_sysmem_f()                     (0x1U)
#define psec_fbif_transcfg_target_noncoherent_sysmem_f()                  (0x2U)
#define psec_fbif_transcfg_mem_type_s()                                     (1U)
#define psec_fbif_transcfg_mem_type_f(v)                 ((U32(v) & 0x1U) << 2U)
#define psec_fbif_transcfg_mem_type_m()                        (U32(0x1U) << 2U)
#define psec_fbif_transcfg_mem_type_v(r)                    (((r) >> 2U) & 0x1U)
#define psec_fbif_transcfg_mem_type_virtual_f()                           (0x0U)
#define psec_fbif_transcfg_mem_type_physical_f()                          (0x4U)
#define psec_falcon_engine_r()                                     (0x008403c0U)
#define psec_falcon_engine_reset_true_f()                                 (0x1U)
#define psec_falcon_engine_reset_false_f()                                (0x0U)
#define psec_fbif_ctl_r()                                          (0x00840624U)
#define psec_fbif_ctl_allow_phys_no_ctx_init_f()                          (0x0U)
#define psec_fbif_ctl_allow_phys_no_ctx_disallow_f()                      (0x0U)
#define psec_fbif_ctl_allow_phys_no_ctx_allow_f()                        (0x80U)
#define psec_hwcfg_r()                                             (0x00840abcU)
#define psec_hwcfg_emem_size_f(v)                      ((U32(v) & 0x1ffU) << 0U)
#define psec_hwcfg_emem_size_m()                             (U32(0x1ffU) << 0U)
#define psec_hwcfg_emem_size_v(r)                         (((r) >> 0U) & 0x1ffU)
#define psec_falcon_hwcfg1_r()                                     (0x0084012cU)
#define psec_falcon_hwcfg1_dmem_tag_width_f(v)         ((U32(v) & 0x1fU) << 21U)
#define psec_falcon_hwcfg1_dmem_tag_width_m()                (U32(0x1fU) << 21U)
#define psec_falcon_hwcfg1_dmem_tag_width_v(r)            (((r) >> 21U) & 0x1fU)
#define psec_ememc_r(i)\
		(nvgpu_safe_add_u32(0x00840ac0U, nvgpu_safe_mult_u32((i), 8U)))
#define psec_ememc__size_1_v()                                     (0x00000004U)
#define psec_ememc_blk_f(v)                             ((U32(v) & 0xffU) << 8U)
#define psec_ememc_blk_m()                                    (U32(0xffU) << 8U)
#define psec_ememc_blk_v(r)                                (((r) >> 8U) & 0xffU)
#define psec_ememc_offs_f(v)                            ((U32(v) & 0x3fU) << 2U)
#define psec_ememc_offs_m()                                   (U32(0x3fU) << 2U)
#define psec_ememc_offs_v(r)                               (((r) >> 2U) & 0x3fU)
#define psec_ememc_aincw_f(v)                           ((U32(v) & 0x1U) << 24U)
#define psec_ememc_aincw_m()                                  (U32(0x1U) << 24U)
#define psec_ememc_aincw_v(r)                              (((r) >> 24U) & 0x1U)
#define psec_ememc_aincr_f(v)                           ((U32(v) & 0x1U) << 25U)
#define psec_ememc_aincr_m()                                  (U32(0x1U) << 25U)
#define psec_ememc_aincr_v(r)                              (((r) >> 25U) & 0x1U)
#define psec_ememd_r(i)\
		(nvgpu_safe_add_u32(0x00840ac4U, nvgpu_safe_mult_u32((i), 8U)))
#define psec_ememd__size_1_v()                                     (0x00000004U)
#define psec_ememd_data_f(v)                      ((U32(v) & 0xffffffffU) << 0U)
#define psec_ememd_data_m()                             (U32(0xffffffffU) << 0U)
#define psec_ememd_data_v(r)                         (((r) >> 0U) & 0xffffffffU)
#define psec_msgq_head_r(i)\
		(nvgpu_safe_add_u32(0x00840c80U, nvgpu_safe_mult_u32((i), 8U)))
#define psec_msgq_head_val_f(v)                   ((U32(v) & 0xffffffffU) << 0U)
#define psec_msgq_head_val_m()                          (U32(0xffffffffU) << 0U)
#define psec_msgq_head_val_v(r)                      (((r) >> 0U) & 0xffffffffU)
#define psec_msgq_tail_r(i)\
		(nvgpu_safe_add_u32(0x00840c84U, nvgpu_safe_mult_u32((i), 8U)))
#define psec_msgq_tail_val_f(v)                   ((U32(v) & 0xffffffffU) << 0U)
#define psec_msgq_tail_val_m()                          (U32(0xffffffffU) << 0U)
#define psec_msgq_tail_val_v(r)                      (((r) >> 0U) & 0xffffffffU)
#define psec_queue_head_r(i)\
		(nvgpu_safe_add_u32(0x00840c00U, nvgpu_safe_mult_u32((i), 8U)))
#define psec_queue_head_address_f(v)              ((U32(v) & 0xffffffffU) << 0U)
#define psec_queue_head_address_m()                     (U32(0xffffffffU) << 0U)
#define psec_queue_head_address_v(r)                 (((r) >> 0U) & 0xffffffffU)
#define psec_queue_tail_r(i)\
		(nvgpu_safe_add_u32(0x00840c04U, nvgpu_safe_mult_u32((i), 8U)))
#define psec_queue_tail_address_f(v)              ((U32(v) & 0xffffffffU) << 0U)
#define psec_queue_tail_address_m()                     (U32(0xffffffffU) << 0U)
#define psec_queue_tail_address_v(r)                 (((r) >> 0U) & 0xffffffffU)
#endif
