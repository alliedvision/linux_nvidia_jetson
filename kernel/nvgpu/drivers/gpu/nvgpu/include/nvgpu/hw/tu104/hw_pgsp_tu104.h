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
#ifndef NVGPU_HW_PGSP_TU104_H
#define NVGPU_HW_PGSP_TU104_H

#include <nvgpu/types.h>
#include <nvgpu/static_analysis.h>

#define pgsp_falcon_irqsset_r()                                    (0x00110000U)
#define pgsp_falcon_irqsset_swgen0_set_f()                               (0x40U)
#define pgsp_falcon_irqsclr_r()                                    (0x00110004U)
#define pgsp_falcon_irqstat_r()                                    (0x00110008U)
#define pgsp_falcon_irqstat_halt_true_f()                                (0x10U)
#define pgsp_falcon_irqstat_exterr_true_f()                              (0x20U)
#define pgsp_falcon_irqstat_swgen0_true_f()                              (0x40U)
#define pgsp_falcon_irqmode_r()                                    (0x0011000cU)
#define pgsp_falcon_irqmset_r()                                    (0x00110010U)
#define pgsp_falcon_irqmset_gptmr_f(v)                   ((U32(v) & 0x1U) << 0U)
#define pgsp_falcon_irqmset_wdtmr_f(v)                   ((U32(v) & 0x1U) << 1U)
#define pgsp_falcon_irqmset_mthd_f(v)                    ((U32(v) & 0x1U) << 2U)
#define pgsp_falcon_irqmset_ctxsw_f(v)                   ((U32(v) & 0x1U) << 3U)
#define pgsp_falcon_irqmset_halt_f(v)                    ((U32(v) & 0x1U) << 4U)
#define pgsp_falcon_irqmset_exterr_f(v)                  ((U32(v) & 0x1U) << 5U)
#define pgsp_falcon_irqmset_swgen0_f(v)                  ((U32(v) & 0x1U) << 6U)
#define pgsp_falcon_irqmset_swgen1_f(v)                  ((U32(v) & 0x1U) << 7U)
#define pgsp_falcon_irqmclr_r()                                    (0x00110014U)
#define pgsp_falcon_irqmclr_gptmr_f(v)                   ((U32(v) & 0x1U) << 0U)
#define pgsp_falcon_irqmclr_wdtmr_f(v)                   ((U32(v) & 0x1U) << 1U)
#define pgsp_falcon_irqmclr_mthd_f(v)                    ((U32(v) & 0x1U) << 2U)
#define pgsp_falcon_irqmclr_ctxsw_f(v)                   ((U32(v) & 0x1U) << 3U)
#define pgsp_falcon_irqmclr_halt_f(v)                    ((U32(v) & 0x1U) << 4U)
#define pgsp_falcon_irqmclr_exterr_f(v)                  ((U32(v) & 0x1U) << 5U)
#define pgsp_falcon_irqmclr_swgen0_f(v)                  ((U32(v) & 0x1U) << 6U)
#define pgsp_falcon_irqmclr_swgen1_f(v)                  ((U32(v) & 0x1U) << 7U)
#define pgsp_falcon_irqmclr_ext_f(v)                    ((U32(v) & 0xffU) << 8U)
#define pgsp_falcon_irqmask_r()                                    (0x00110018U)
#define pgsp_falcon_irqdest_r()                                    (0x0011001cU)
#define pgsp_falcon_irqdest_host_gptmr_f(v)              ((U32(v) & 0x1U) << 0U)
#define pgsp_falcon_irqdest_host_wdtmr_f(v)              ((U32(v) & 0x1U) << 1U)
#define pgsp_falcon_irqdest_host_mthd_f(v)               ((U32(v) & 0x1U) << 2U)
#define pgsp_falcon_irqdest_host_ctxsw_f(v)              ((U32(v) & 0x1U) << 3U)
#define pgsp_falcon_irqdest_host_halt_f(v)               ((U32(v) & 0x1U) << 4U)
#define pgsp_falcon_irqdest_host_exterr_f(v)             ((U32(v) & 0x1U) << 5U)
#define pgsp_falcon_irqdest_host_swgen0_f(v)             ((U32(v) & 0x1U) << 6U)
#define pgsp_falcon_irqdest_host_swgen1_f(v)             ((U32(v) & 0x1U) << 7U)
#define pgsp_falcon_irqdest_host_ext_f(v)               ((U32(v) & 0xffU) << 8U)
#define pgsp_falcon_irqdest_target_gptmr_f(v)           ((U32(v) & 0x1U) << 16U)
#define pgsp_falcon_irqdest_target_wdtmr_f(v)           ((U32(v) & 0x1U) << 17U)
#define pgsp_falcon_irqdest_target_mthd_f(v)            ((U32(v) & 0x1U) << 18U)
#define pgsp_falcon_irqdest_target_ctxsw_f(v)           ((U32(v) & 0x1U) << 19U)
#define pgsp_falcon_irqdest_target_halt_f(v)            ((U32(v) & 0x1U) << 20U)
#define pgsp_falcon_irqdest_target_exterr_f(v)          ((U32(v) & 0x1U) << 21U)
#define pgsp_falcon_irqdest_target_swgen0_f(v)          ((U32(v) & 0x1U) << 22U)
#define pgsp_falcon_irqdest_target_swgen1_f(v)          ((U32(v) & 0x1U) << 23U)
#define pgsp_falcon_irqdest_target_ext_f(v)            ((U32(v) & 0xffU) << 24U)
#define pgsp_falcon_curctx_r()                                     (0x00110050U)
#define pgsp_falcon_nxtctx_r()                                     (0x00110054U)
#define pgsp_falcon_nxtctx_ctxptr_f(v)             ((U32(v) & 0xfffffffU) << 0U)
#define pgsp_falcon_nxtctx_ctxtgt_fb_f()                                  (0x0U)
#define pgsp_falcon_nxtctx_ctxtgt_sys_coh_f()                      (0x20000000U)
#define pgsp_falcon_nxtctx_ctxtgt_sys_ncoh_f()                     (0x30000000U)
#define pgsp_falcon_nxtctx_ctxvalid_f(v)                ((U32(v) & 0x1U) << 30U)
#define pgsp_falcon_mailbox0_r()                                   (0x00110040U)
#define pgsp_falcon_mailbox1_r()                                   (0x00110044U)
#define pgsp_falcon_itfen_r()                                      (0x00110048U)
#define pgsp_falcon_itfen_ctxen_enable_f()                                (0x1U)
#define pgsp_falcon_idlestate_r()                                  (0x0011004cU)
#define pgsp_falcon_idlestate_falcon_busy_v(r)              (((r) >> 0U) & 0x1U)
#define pgsp_falcon_idlestate_ext_busy_v(r)              (((r) >> 1U) & 0x7fffU)
#define pgsp_falcon_os_r()                                         (0x00110080U)
#define pgsp_falcon_engctl_r()                                     (0x001100a4U)
#define pgsp_falcon_engctl_switch_context_true_f()                        (0x8U)
#define pgsp_falcon_engctl_switch_context_false_f()                       (0x0U)
#define pgsp_falcon_cpuctl_r()                                     (0x00110100U)
#define pgsp_falcon_cpuctl_startcpu_f(v)                 ((U32(v) & 0x1U) << 1U)
#define pgsp_falcon_cpuctl_halt_intr_f(v)                ((U32(v) & 0x1U) << 4U)
#define pgsp_falcon_cpuctl_halt_intr_m()                       (U32(0x1U) << 4U)
#define pgsp_falcon_cpuctl_halt_intr_v(r)                   (((r) >> 4U) & 0x1U)
#define pgsp_falcon_cpuctl_cpuctl_alias_en_f(v)          ((U32(v) & 0x1U) << 6U)
#define pgsp_falcon_cpuctl_cpuctl_alias_en_m()                 (U32(0x1U) << 6U)
#define pgsp_falcon_cpuctl_cpuctl_alias_en_v(r)             (((r) >> 6U) & 0x1U)
#define pgsp_falcon_cpuctl_alias_r()                               (0x00110130U)
#define pgsp_falcon_cpuctl_alias_startcpu_f(v)           ((U32(v) & 0x1U) << 1U)
#define pgsp_falcon_imemc_r(i)\
		(nvgpu_safe_add_u32(0x00110180U, nvgpu_safe_mult_u32((i), 16U)))
#define pgsp_falcon_imemc_offs_f(v)                     ((U32(v) & 0x3fU) << 2U)
#define pgsp_falcon_imemc_blk_f(v)                      ((U32(v) & 0xffU) << 8U)
#define pgsp_falcon_imemc_aincw_f(v)                    ((U32(v) & 0x1U) << 24U)
#define pgsp_falcon_imemd_r(i)\
		(nvgpu_safe_add_u32(0x00110184U, nvgpu_safe_mult_u32((i), 16U)))
#define pgsp_falcon_imemt_r(i)\
		(nvgpu_safe_add_u32(0x00110188U, nvgpu_safe_mult_u32((i), 16U)))
#define pgsp_falcon_sctl_r()                                       (0x00110240U)
#define pgsp_falcon_mmu_phys_sec_r()                               (0x00100ce4U)
#define pgsp_falcon_bootvec_r()                                    (0x00110104U)
#define pgsp_falcon_bootvec_vec_f(v)              ((U32(v) & 0xffffffffU) << 0U)
#define pgsp_falcon_dmactl_r()                                     (0x0011010cU)
#define pgsp_falcon_dmactl_dmem_scrubbing_m()                  (U32(0x1U) << 1U)
#define pgsp_falcon_dmactl_imem_scrubbing_m()                  (U32(0x1U) << 2U)
#define pgsp_falcon_dmactl_require_ctx_f(v)              ((U32(v) & 0x1U) << 0U)
#define pgsp_falcon_hwcfg_r()                                      (0x00110108U)
#define pgsp_falcon_hwcfg_imem_size_v(r)                  (((r) >> 0U) & 0x1ffU)
#define pgsp_falcon_hwcfg_dmem_size_v(r)                  (((r) >> 9U) & 0x1ffU)
#define pgsp_falcon_dmatrfbase_r()                                 (0x00110110U)
#define pgsp_falcon_dmatrfbase1_r()                                (0x00110128U)
#define pgsp_falcon_dmatrfmoffs_r()                                (0x00110114U)
#define pgsp_falcon_dmatrfcmd_r()                                  (0x00110118U)
#define pgsp_falcon_dmatrfcmd_imem_f(v)                  ((U32(v) & 0x1U) << 4U)
#define pgsp_falcon_dmatrfcmd_write_f(v)                 ((U32(v) & 0x1U) << 5U)
#define pgsp_falcon_dmatrfcmd_size_f(v)                  ((U32(v) & 0x7U) << 8U)
#define pgsp_falcon_dmatrfcmd_ctxdma_f(v)               ((U32(v) & 0x7U) << 12U)
#define pgsp_falcon_dmatrffboffs_r()                               (0x0011011cU)
#define pgsp_falcon_exterraddr_r()                                 (0x00110168U)
#define pgsp_falcon_exterrstat_r()                                 (0x0011016cU)
#define pgsp_falcon_exterrstat_valid_m()                      (U32(0x1U) << 31U)
#define pgsp_falcon_exterrstat_valid_v(r)                  (((r) >> 31U) & 0x1U)
#define pgsp_falcon_exterrstat_valid_true_v()                      (0x00000001U)
#define pgsp_sec2_falcon_icd_cmd_r()                               (0x00110200U)
#define pgsp_sec2_falcon_icd_cmd_opc_s()                                    (4U)
#define pgsp_sec2_falcon_icd_cmd_opc_f(v)                ((U32(v) & 0xfU) << 0U)
#define pgsp_sec2_falcon_icd_cmd_opc_m()                       (U32(0xfU) << 0U)
#define pgsp_sec2_falcon_icd_cmd_opc_v(r)                   (((r) >> 0U) & 0xfU)
#define pgsp_sec2_falcon_icd_cmd_opc_rreg_f()                             (0x8U)
#define pgsp_sec2_falcon_icd_cmd_opc_rstat_f()                            (0xeU)
#define pgsp_sec2_falcon_icd_cmd_idx_f(v)               ((U32(v) & 0x1fU) << 8U)
#define pgsp_sec2_falcon_icd_rdata_r()                             (0x0011020cU)
#define pgsp_falcon_dmemc_r(i)\
		(nvgpu_safe_add_u32(0x001101c0U, nvgpu_safe_mult_u32((i), 8U)))
#define pgsp_falcon_dmemc_offs_f(v)                     ((U32(v) & 0x3fU) << 2U)
#define pgsp_falcon_dmemc_offs_m()                            (U32(0x3fU) << 2U)
#define pgsp_falcon_dmemc_blk_f(v)                      ((U32(v) & 0xffU) << 8U)
#define pgsp_falcon_dmemc_blk_m()                             (U32(0xffU) << 8U)
#define pgsp_falcon_dmemc_aincw_f(v)                    ((U32(v) & 0x1U) << 24U)
#define pgsp_falcon_dmemc_aincr_f(v)                    ((U32(v) & 0x1U) << 25U)
#define pgsp_falcon_dmemd_r(i)\
		(nvgpu_safe_add_u32(0x001101c4U, nvgpu_safe_mult_u32((i), 8U)))
#define pgsp_falcon_debug1_r()                                     (0x00110090U)
#define pgsp_falcon_debug1_ctxsw_mode_s()                                   (1U)
#define pgsp_falcon_debug1_ctxsw_mode_f(v)              ((U32(v) & 0x1U) << 16U)
#define pgsp_falcon_debug1_ctxsw_mode_m()                     (U32(0x1U) << 16U)
#define pgsp_falcon_debug1_ctxsw_mode_v(r)                 (((r) >> 16U) & 0x1U)
#define pgsp_falcon_debug1_ctxsw_mode_init_f()                            (0x0U)
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
#define pgsp_falcon_engine_r()                                     (0x001103c0U)
#define pgsp_falcon_engine_reset_true_f()                                 (0x1U)
#define pgsp_falcon_engine_reset_false_f()                                (0x0U)
#define pgsp_fbif_ctl_r()                                          (0x00110624U)
#define pgsp_fbif_ctl_allow_phys_no_ctx_init_f()                          (0x0U)
#define pgsp_fbif_ctl_allow_phys_no_ctx_disallow_f()                      (0x0U)
#define pgsp_fbif_ctl_allow_phys_no_ctx_allow_f()                        (0x80U)
#endif
