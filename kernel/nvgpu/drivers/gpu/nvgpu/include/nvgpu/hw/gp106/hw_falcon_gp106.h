/*
 * Copyright (c) 2017-2019, NVIDIA CORPORATION.  All rights reserved.
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
#ifndef NVGPU_HW_FALCON_GP106_H
#define NVGPU_HW_FALCON_GP106_H

#include <nvgpu/types.h>
#include <nvgpu/static_analysis.h>

#define falcon_falcon_irqsset_r()                                  (0x00000000U)
#define falcon_falcon_irqsset_swgen0_set_f()                             (0x40U)
#define falcon_falcon_irqsclr_r()                                  (0x00000004U)
#define falcon_falcon_irqstat_r()                                  (0x00000008U)
#define falcon_falcon_irqstat_halt_true_f()                              (0x10U)
#define falcon_falcon_irqstat_exterr_true_f()                            (0x20U)
#define falcon_falcon_irqstat_swgen0_true_f()                            (0x40U)
#define falcon_falcon_irqmode_r()                                  (0x0000000cU)
#define falcon_falcon_irqmset_r()                                  (0x00000010U)
#define falcon_falcon_irqmset_gptmr_f(v)                 ((U32(v) & 0x1U) << 0U)
#define falcon_falcon_irqmset_wdtmr_f(v)                 ((U32(v) & 0x1U) << 1U)
#define falcon_falcon_irqmset_mthd_f(v)                  ((U32(v) & 0x1U) << 2U)
#define falcon_falcon_irqmset_ctxsw_f(v)                 ((U32(v) & 0x1U) << 3U)
#define falcon_falcon_irqmset_halt_f(v)                  ((U32(v) & 0x1U) << 4U)
#define falcon_falcon_irqmset_exterr_f(v)                ((U32(v) & 0x1U) << 5U)
#define falcon_falcon_irqmset_swgen0_f(v)                ((U32(v) & 0x1U) << 6U)
#define falcon_falcon_irqmset_swgen1_f(v)                ((U32(v) & 0x1U) << 7U)
#define falcon_falcon_irqmclr_r()                                  (0x00000014U)
#define falcon_falcon_irqmclr_gptmr_f(v)                 ((U32(v) & 0x1U) << 0U)
#define falcon_falcon_irqmclr_wdtmr_f(v)                 ((U32(v) & 0x1U) << 1U)
#define falcon_falcon_irqmclr_mthd_f(v)                  ((U32(v) & 0x1U) << 2U)
#define falcon_falcon_irqmclr_ctxsw_f(v)                 ((U32(v) & 0x1U) << 3U)
#define falcon_falcon_irqmclr_halt_f(v)                  ((U32(v) & 0x1U) << 4U)
#define falcon_falcon_irqmclr_exterr_f(v)                ((U32(v) & 0x1U) << 5U)
#define falcon_falcon_irqmclr_swgen0_f(v)                ((U32(v) & 0x1U) << 6U)
#define falcon_falcon_irqmclr_swgen1_f(v)                ((U32(v) & 0x1U) << 7U)
#define falcon_falcon_irqmclr_ext_f(v)                  ((U32(v) & 0xffU) << 8U)
#define falcon_falcon_irqmask_r()                                  (0x00000018U)
#define falcon_falcon_irqdest_r()                                  (0x0000001cU)
#define falcon_falcon_irqdest_host_gptmr_f(v)            ((U32(v) & 0x1U) << 0U)
#define falcon_falcon_irqdest_host_wdtmr_f(v)            ((U32(v) & 0x1U) << 1U)
#define falcon_falcon_irqdest_host_mthd_f(v)             ((U32(v) & 0x1U) << 2U)
#define falcon_falcon_irqdest_host_ctxsw_f(v)            ((U32(v) & 0x1U) << 3U)
#define falcon_falcon_irqdest_host_halt_f(v)             ((U32(v) & 0x1U) << 4U)
#define falcon_falcon_irqdest_host_exterr_f(v)           ((U32(v) & 0x1U) << 5U)
#define falcon_falcon_irqdest_host_swgen0_f(v)           ((U32(v) & 0x1U) << 6U)
#define falcon_falcon_irqdest_host_swgen1_f(v)           ((U32(v) & 0x1U) << 7U)
#define falcon_falcon_irqdest_host_ext_f(v)             ((U32(v) & 0xffU) << 8U)
#define falcon_falcon_irqdest_target_gptmr_f(v)         ((U32(v) & 0x1U) << 16U)
#define falcon_falcon_irqdest_target_wdtmr_f(v)         ((U32(v) & 0x1U) << 17U)
#define falcon_falcon_irqdest_target_mthd_f(v)          ((U32(v) & 0x1U) << 18U)
#define falcon_falcon_irqdest_target_ctxsw_f(v)         ((U32(v) & 0x1U) << 19U)
#define falcon_falcon_irqdest_target_halt_f(v)          ((U32(v) & 0x1U) << 20U)
#define falcon_falcon_irqdest_target_exterr_f(v)        ((U32(v) & 0x1U) << 21U)
#define falcon_falcon_irqdest_target_swgen0_f(v)        ((U32(v) & 0x1U) << 22U)
#define falcon_falcon_irqdest_target_swgen1_f(v)        ((U32(v) & 0x1U) << 23U)
#define falcon_falcon_irqdest_target_ext_f(v)          ((U32(v) & 0xffU) << 24U)
#define falcon_falcon_curctx_r()                                   (0x00000050U)
#define falcon_falcon_nxtctx_r()                                   (0x00000054U)
#define falcon_falcon_mailbox0_r()                                 (0x00000040U)
#define falcon_falcon_mailbox1_r()                                 (0x00000044U)
#define falcon_falcon_itfen_r()                                    (0x00000048U)
#define falcon_falcon_itfen_ctxen_enable_f()                              (0x1U)
#define falcon_falcon_idlestate_r()                                (0x0000004cU)
#define falcon_falcon_idlestate_falcon_busy_v(r)            (((r) >> 0U) & 0x1U)
#define falcon_falcon_idlestate_ext_busy_v(r)            (((r) >> 1U) & 0x7fffU)
#define falcon_falcon_os_r()                                       (0x00000080U)
#define falcon_falcon_engctl_r()                                   (0x000000a4U)
#define falcon_falcon_cpuctl_r()                                   (0x00000100U)
#define falcon_falcon_cpuctl_startcpu_f(v)               ((U32(v) & 0x1U) << 1U)
#define falcon_falcon_cpuctl_sreset_f(v)                 ((U32(v) & 0x1U) << 2U)
#define falcon_falcon_cpuctl_hreset_f(v)                 ((U32(v) & 0x1U) << 3U)
#define falcon_falcon_cpuctl_halt_intr_f(v)              ((U32(v) & 0x1U) << 4U)
#define falcon_falcon_cpuctl_halt_intr_m()                     (U32(0x1U) << 4U)
#define falcon_falcon_cpuctl_halt_intr_v(r)                 (((r) >> 4U) & 0x1U)
#define falcon_falcon_cpuctl_stopped_m()                       (U32(0x1U) << 5U)
#define falcon_falcon_cpuctl_cpuctl_alias_en_f(v)        ((U32(v) & 0x1U) << 6U)
#define falcon_falcon_cpuctl_cpuctl_alias_en_m()               (U32(0x1U) << 6U)
#define falcon_falcon_cpuctl_cpuctl_alias_en_v(r)           (((r) >> 6U) & 0x1U)
#define falcon_falcon_cpuctl_alias_r()                             (0x00000130U)
#define falcon_falcon_cpuctl_alias_startcpu_f(v)         ((U32(v) & 0x1U) << 1U)
#define falcon_falcon_imemc_r(i)\
		(nvgpu_safe_add_u32(0x00000180U, nvgpu_safe_mult_u32((i), 16U)))
#define falcon_falcon_imemc_offs_f(v)                   ((U32(v) & 0x3fU) << 2U)
#define falcon_falcon_imemc_blk_f(v)                    ((U32(v) & 0xffU) << 8U)
#define falcon_falcon_imemc_aincw_f(v)                  ((U32(v) & 0x1U) << 24U)
#define falcon_falcon_imemc_secure_f(v)                 ((U32(v) & 0x1U) << 28U)
#define falcon_falcon_imemd_r(i)\
		(nvgpu_safe_add_u32(0x00000184U, nvgpu_safe_mult_u32((i), 16U)))
#define falcon_falcon_imemt_r(i)\
		(nvgpu_safe_add_u32(0x00000188U, nvgpu_safe_mult_u32((i), 16U)))
#define falcon_falcon_sctl_r()                                     (0x00000240U)
#define falcon_falcon_mmu_phys_sec_r()                             (0x00100ce4U)
#define falcon_falcon_bootvec_r()                                  (0x00000104U)
#define falcon_falcon_bootvec_vec_f(v)            ((U32(v) & 0xffffffffU) << 0U)
#define falcon_falcon_dmactl_r()                                   (0x0000010cU)
#define falcon_falcon_dmactl_dmem_scrubbing_m()                (U32(0x1U) << 1U)
#define falcon_falcon_dmactl_imem_scrubbing_m()                (U32(0x1U) << 2U)
#define falcon_falcon_dmactl_require_ctx_f(v)            ((U32(v) & 0x1U) << 0U)
#define falcon_falcon_hwcfg_r()                                    (0x00000108U)
#define falcon_falcon_hwcfg_imem_size_v(r)                (((r) >> 0U) & 0x1ffU)
#define falcon_falcon_hwcfg_dmem_size_v(r)                (((r) >> 9U) & 0x1ffU)
#define falcon_falcon_dmatrfbase_r()                               (0x00000110U)
#define falcon_falcon_dmatrfbase1_r()                              (0x00000128U)
#define falcon_falcon_dmatrfmoffs_r()                              (0x00000114U)
#define falcon_falcon_dmatrfcmd_r()                                (0x00000118U)
#define falcon_falcon_dmatrfcmd_imem_f(v)                ((U32(v) & 0x1U) << 4U)
#define falcon_falcon_dmatrfcmd_write_f(v)               ((U32(v) & 0x1U) << 5U)
#define falcon_falcon_dmatrfcmd_size_f(v)                ((U32(v) & 0x7U) << 8U)
#define falcon_falcon_dmatrfcmd_ctxdma_f(v)             ((U32(v) & 0x7U) << 12U)
#define falcon_falcon_dmatrffboffs_r()                             (0x0000011cU)
#define falcon_falcon_imctl_debug_r()                              (0x0000015cU)
#define falcon_falcon_imctl_debug_addr_blk_f(v)     ((U32(v) & 0xffffffU) << 0U)
#define falcon_falcon_imctl_debug_cmd_f(v)              ((U32(v) & 0x7U) << 24U)
#define falcon_falcon_imstat_r()                                   (0x00000144U)
#define falcon_falcon_traceidx_r()                                 (0x00000148U)
#define falcon_falcon_traceidx_maxidx_v(r)                (((r) >> 16U) & 0xffU)
#define falcon_falcon_traceidx_idx_f(v)                 ((U32(v) & 0xffU) << 0U)
#define falcon_falcon_tracepc_r()                                  (0x0000014cU)
#define falcon_falcon_tracepc_pc_v(r)                  (((r) >> 0U) & 0xffffffU)
#define falcon_falcon_exterraddr_r()                               (0x0010a168U)
#define falcon_falcon_exterrstat_r()                               (0x0010a16cU)
#define falcon_falcon_exterrstat_valid_m()                    (U32(0x1U) << 31U)
#define falcon_falcon_exterrstat_valid_v(r)                (((r) >> 31U) & 0x1U)
#define falcon_falcon_exterrstat_valid_true_v()                    (0x00000001U)
#define falcon_falcon_icd_cmd_r()                                  (0x00000200U)
#define falcon_falcon_icd_cmd_opc_s()                                       (4U)
#define falcon_falcon_icd_cmd_opc_f(v)                   ((U32(v) & 0xfU) << 0U)
#define falcon_falcon_icd_cmd_opc_m()                          (U32(0xfU) << 0U)
#define falcon_falcon_icd_cmd_opc_v(r)                      (((r) >> 0U) & 0xfU)
#define falcon_falcon_icd_cmd_opc_rreg_f()                                (0x8U)
#define falcon_falcon_icd_cmd_opc_rstat_f()                               (0xeU)
#define falcon_falcon_icd_cmd_idx_f(v)                  ((U32(v) & 0x1fU) << 8U)
#define falcon_falcon_icd_rdata_r()                                (0x0000020cU)
#define falcon_falcon_dmemc_r(i)\
		(nvgpu_safe_add_u32(0x000001c0U, nvgpu_safe_mult_u32((i), 8U)))
#define falcon_falcon_dmemc_offs_f(v)                   ((U32(v) & 0x3fU) << 2U)
#define falcon_falcon_dmemc_offs_m()                          (U32(0x3fU) << 2U)
#define falcon_falcon_dmemc_blk_f(v)                    ((U32(v) & 0xffU) << 8U)
#define falcon_falcon_dmemc_blk_m()                           (U32(0xffU) << 8U)
#define falcon_falcon_dmemc_aincw_f(v)                  ((U32(v) & 0x1U) << 24U)
#define falcon_falcon_dmemc_aincr_f(v)                  ((U32(v) & 0x1U) << 25U)
#define falcon_falcon_dmemd_r(i)\
		(nvgpu_safe_add_u32(0x000001c4U, nvgpu_safe_mult_u32((i), 8U)))
#define falcon_falcon_debug1_r()                                   (0x00000090U)
#define falcon_falcon_debug1_ctxsw_mode_s()                                 (1U)
#define falcon_falcon_debug1_ctxsw_mode_f(v)            ((U32(v) & 0x1U) << 16U)
#define falcon_falcon_debug1_ctxsw_mode_m()                   (U32(0x1U) << 16U)
#define falcon_falcon_debug1_ctxsw_mode_v(r)               (((r) >> 16U) & 0x1U)
#define falcon_falcon_debug1_ctxsw_mode_init_f()                          (0x0U)
#define falcon_falcon_debuginfo_r()                                (0x00000094U)
#endif
