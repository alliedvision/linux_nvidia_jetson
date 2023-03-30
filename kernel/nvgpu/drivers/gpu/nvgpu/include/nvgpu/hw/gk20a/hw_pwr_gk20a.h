/*
 * Copyright (c) 2012-2020, NVIDIA CORPORATION.  All rights reserved.
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
#ifndef NVGPU_HW_PWR_GK20A_H
#define NVGPU_HW_PWR_GK20A_H

#include <nvgpu/types.h>
#include <nvgpu/static_analysis.h>

#define pwr_falcon_irqsset_r()                                     (0x0010a000U)
#define pwr_falcon_irqsset_swgen0_set_f()                                (0x40U)
#define pwr_falcon_irqsclr_r()                                     (0x0010a004U)
#define pwr_falcon_irqstat_r()                                     (0x0010a008U)
#define pwr_falcon_irqstat_halt_true_f()                                 (0x10U)
#define pwr_falcon_irqstat_exterr_true_f()                               (0x20U)
#define pwr_falcon_irqstat_swgen0_true_f()                               (0x40U)
#define pwr_falcon_irqmode_r()                                     (0x0010a00cU)
#define pwr_falcon_irqmset_r()                                     (0x0010a010U)
#define pwr_falcon_irqmset_gptmr_f(v)                    ((U32(v) & 0x1U) << 0U)
#define pwr_falcon_irqmset_wdtmr_f(v)                    ((U32(v) & 0x1U) << 1U)
#define pwr_falcon_irqmset_mthd_f(v)                     ((U32(v) & 0x1U) << 2U)
#define pwr_falcon_irqmset_ctxsw_f(v)                    ((U32(v) & 0x1U) << 3U)
#define pwr_falcon_irqmset_halt_f(v)                     ((U32(v) & 0x1U) << 4U)
#define pwr_falcon_irqmset_exterr_f(v)                   ((U32(v) & 0x1U) << 5U)
#define pwr_falcon_irqmset_swgen0_f(v)                   ((U32(v) & 0x1U) << 6U)
#define pwr_falcon_irqmset_swgen1_f(v)                   ((U32(v) & 0x1U) << 7U)
#define pwr_falcon_irqmclr_r()                                     (0x0010a014U)
#define pwr_falcon_irqmclr_gptmr_f(v)                    ((U32(v) & 0x1U) << 0U)
#define pwr_falcon_irqmclr_wdtmr_f(v)                    ((U32(v) & 0x1U) << 1U)
#define pwr_falcon_irqmclr_mthd_f(v)                     ((U32(v) & 0x1U) << 2U)
#define pwr_falcon_irqmclr_ctxsw_f(v)                    ((U32(v) & 0x1U) << 3U)
#define pwr_falcon_irqmclr_halt_f(v)                     ((U32(v) & 0x1U) << 4U)
#define pwr_falcon_irqmclr_exterr_f(v)                   ((U32(v) & 0x1U) << 5U)
#define pwr_falcon_irqmclr_swgen0_f(v)                   ((U32(v) & 0x1U) << 6U)
#define pwr_falcon_irqmclr_swgen1_f(v)                   ((U32(v) & 0x1U) << 7U)
#define pwr_falcon_irqmclr_ext_f(v)                     ((U32(v) & 0xffU) << 8U)
#define pwr_falcon_irqmask_r()                                     (0x0010a018U)
#define pwr_falcon_irqdest_r()                                     (0x0010a01cU)
#define pwr_falcon_irqdest_host_gptmr_f(v)               ((U32(v) & 0x1U) << 0U)
#define pwr_falcon_irqdest_host_wdtmr_f(v)               ((U32(v) & 0x1U) << 1U)
#define pwr_falcon_irqdest_host_mthd_f(v)                ((U32(v) & 0x1U) << 2U)
#define pwr_falcon_irqdest_host_ctxsw_f(v)               ((U32(v) & 0x1U) << 3U)
#define pwr_falcon_irqdest_host_halt_f(v)                ((U32(v) & 0x1U) << 4U)
#define pwr_falcon_irqdest_host_exterr_f(v)              ((U32(v) & 0x1U) << 5U)
#define pwr_falcon_irqdest_host_swgen0_f(v)              ((U32(v) & 0x1U) << 6U)
#define pwr_falcon_irqdest_host_swgen1_f(v)              ((U32(v) & 0x1U) << 7U)
#define pwr_falcon_irqdest_host_ext_f(v)                ((U32(v) & 0xffU) << 8U)
#define pwr_falcon_irqdest_target_gptmr_f(v)            ((U32(v) & 0x1U) << 16U)
#define pwr_falcon_irqdest_target_wdtmr_f(v)            ((U32(v) & 0x1U) << 17U)
#define pwr_falcon_irqdest_target_mthd_f(v)             ((U32(v) & 0x1U) << 18U)
#define pwr_falcon_irqdest_target_ctxsw_f(v)            ((U32(v) & 0x1U) << 19U)
#define pwr_falcon_irqdest_target_halt_f(v)             ((U32(v) & 0x1U) << 20U)
#define pwr_falcon_irqdest_target_exterr_f(v)           ((U32(v) & 0x1U) << 21U)
#define pwr_falcon_irqdest_target_swgen0_f(v)           ((U32(v) & 0x1U) << 22U)
#define pwr_falcon_irqdest_target_swgen1_f(v)           ((U32(v) & 0x1U) << 23U)
#define pwr_falcon_irqdest_target_ext_f(v)             ((U32(v) & 0xffU) << 24U)
#define pwr_falcon_curctx_r()                                      (0x0010a050U)
#define pwr_falcon_nxtctx_r()                                      (0x0010a054U)
#define pwr_falcon_mailbox0_r()                                    (0x0010a040U)
#define pwr_falcon_mailbox1_r()                                    (0x0010a044U)
#define pwr_falcon_itfen_r()                                       (0x0010a048U)
#define pwr_falcon_itfen_ctxen_enable_f()                                 (0x1U)
#define pwr_falcon_idlestate_r()                                   (0x0010a04cU)
#define pwr_falcon_idlestate_falcon_busy_v(r)               (((r) >> 0U) & 0x1U)
#define pwr_falcon_idlestate_ext_busy_v(r)               (((r) >> 1U) & 0x7fffU)
#define pwr_falcon_os_r()                                          (0x0010a080U)
#define pwr_falcon_engctl_r()                                      (0x0010a0a4U)
#define pwr_falcon_cpuctl_r()                                      (0x0010a100U)
#define pwr_falcon_cpuctl_startcpu_f(v)                  ((U32(v) & 0x1U) << 1U)
#define pwr_falcon_cpuctl_halt_intr_f(v)                 ((U32(v) & 0x1U) << 4U)
#define pwr_falcon_cpuctl_halt_intr_m()                        (U32(0x1U) << 4U)
#define pwr_falcon_cpuctl_halt_intr_v(r)                    (((r) >> 4U) & 0x1U)
#define pwr_falcon_imemc_r(i)\
		(nvgpu_safe_add_u32(0x0010a180U, nvgpu_safe_mult_u32((i), 16U)))
#define pwr_falcon_imemc_offs_f(v)                      ((U32(v) & 0x3fU) << 2U)
#define pwr_falcon_imemc_blk_f(v)                       ((U32(v) & 0xffU) << 8U)
#define pwr_falcon_imemc_aincw_f(v)                     ((U32(v) & 0x1U) << 24U)
#define pwr_falcon_imemd_r(i)\
		(nvgpu_safe_add_u32(0x0010a184U, nvgpu_safe_mult_u32((i), 16U)))
#define pwr_falcon_imemt_r(i)\
		(nvgpu_safe_add_u32(0x0010a188U, nvgpu_safe_mult_u32((i), 16U)))
#define pwr_falcon_bootvec_r()                                     (0x0010a104U)
#define pwr_falcon_bootvec_vec_f(v)               ((U32(v) & 0xffffffffU) << 0U)
#define pwr_falcon_dmactl_r()                                      (0x0010a10cU)
#define pwr_falcon_dmactl_dmem_scrubbing_m()                   (U32(0x1U) << 1U)
#define pwr_falcon_dmactl_imem_scrubbing_m()                   (U32(0x1U) << 2U)
#define pwr_falcon_hwcfg_r()                                       (0x0010a108U)
#define pwr_falcon_hwcfg_imem_size_v(r)                   (((r) >> 0U) & 0x1ffU)
#define pwr_falcon_hwcfg_dmem_size_v(r)                   (((r) >> 9U) & 0x1ffU)
#define pwr_falcon_dmatrfbase_r()                                  (0x0010a110U)
#define pwr_falcon_dmatrfmoffs_r()                                 (0x0010a114U)
#define pwr_falcon_dmatrfcmd_r()                                   (0x0010a118U)
#define pwr_falcon_dmatrfcmd_imem_f(v)                   ((U32(v) & 0x1U) << 4U)
#define pwr_falcon_dmatrfcmd_write_f(v)                  ((U32(v) & 0x1U) << 5U)
#define pwr_falcon_dmatrfcmd_size_f(v)                   ((U32(v) & 0x7U) << 8U)
#define pwr_falcon_dmatrfcmd_ctxdma_f(v)                ((U32(v) & 0x7U) << 12U)
#define pwr_falcon_dmatrffboffs_r()                                (0x0010a11cU)
#define pwr_falcon_exterraddr_r()                                  (0x0010a168U)
#define pwr_falcon_exterrstat_r()                                  (0x0010a16cU)
#define pwr_falcon_exterrstat_valid_m()                       (U32(0x1U) << 31U)
#define pwr_falcon_exterrstat_valid_v(r)                   (((r) >> 31U) & 0x1U)
#define pwr_falcon_exterrstat_valid_true_v()                       (0x00000001U)
#define pwr_pmu_falcon_icd_cmd_r()                                 (0x0010a200U)
#define pwr_pmu_falcon_icd_cmd_opc_s()                                      (4U)
#define pwr_pmu_falcon_icd_cmd_opc_f(v)                  ((U32(v) & 0xfU) << 0U)
#define pwr_pmu_falcon_icd_cmd_opc_m()                         (U32(0xfU) << 0U)
#define pwr_pmu_falcon_icd_cmd_opc_v(r)                     (((r) >> 0U) & 0xfU)
#define pwr_pmu_falcon_icd_cmd_opc_rreg_f()                               (0x8U)
#define pwr_pmu_falcon_icd_cmd_opc_rstat_f()                              (0xeU)
#define pwr_pmu_falcon_icd_cmd_idx_f(v)                 ((U32(v) & 0x1fU) << 8U)
#define pwr_pmu_falcon_icd_rdata_r()                               (0x0010a20cU)
#define pwr_falcon_dmemc_r(i)\
		(nvgpu_safe_add_u32(0x0010a1c0U, nvgpu_safe_mult_u32((i), 8U)))
#define pwr_falcon_dmemc_offs_f(v)                      ((U32(v) & 0x3fU) << 2U)
#define pwr_falcon_dmemc_offs_m()                             (U32(0x3fU) << 2U)
#define pwr_falcon_dmemc_blk_f(v)                       ((U32(v) & 0xffU) << 8U)
#define pwr_falcon_dmemc_blk_m()                              (U32(0xffU) << 8U)
#define pwr_falcon_dmemc_aincw_f(v)                     ((U32(v) & 0x1U) << 24U)
#define pwr_falcon_dmemc_aincr_f(v)                     ((U32(v) & 0x1U) << 25U)
#define pwr_falcon_dmemd_r(i)\
		(nvgpu_safe_add_u32(0x0010a1c4U, nvgpu_safe_mult_u32((i), 8U)))
#define pwr_pmu_new_instblk_r()                                    (0x0010a480U)
#define pwr_pmu_new_instblk_ptr_f(v)               ((U32(v) & 0xfffffffU) << 0U)
#define pwr_pmu_new_instblk_target_fb_f()                                 (0x0U)
#define pwr_pmu_new_instblk_target_sys_coh_f()                     (0x20000000U)
#define pwr_pmu_new_instblk_target_sys_ncoh_f()                    (0x30000000U)
#define pwr_pmu_new_instblk_valid_f(v)                  ((U32(v) & 0x1U) << 30U)
#define pwr_pmu_mutex_id_r()                                       (0x0010a488U)
#define pwr_pmu_mutex_id_value_v(r)                        (((r) >> 0U) & 0xffU)
#define pwr_pmu_mutex_id_value_init_v()                            (0x00000000U)
#define pwr_pmu_mutex_id_value_not_avail_v()                       (0x000000ffU)
#define pwr_pmu_mutex_id_release_r()                               (0x0010a48cU)
#define pwr_pmu_mutex_id_release_value_f(v)             ((U32(v) & 0xffU) << 0U)
#define pwr_pmu_mutex_id_release_value_m()                    (U32(0xffU) << 0U)
#define pwr_pmu_mutex_id_release_value_init_v()                    (0x00000000U)
#define pwr_pmu_mutex_id_release_value_init_f()                           (0x0U)
#define pwr_pmu_mutex_r(i)\
		(nvgpu_safe_add_u32(0x0010a580U, nvgpu_safe_mult_u32((i), 4U)))
#define pwr_pmu_mutex__size_1_v()                                  (0x00000010U)
#define pwr_pmu_mutex_value_f(v)                        ((U32(v) & 0xffU) << 0U)
#define pwr_pmu_mutex_value_v(r)                           (((r) >> 0U) & 0xffU)
#define pwr_pmu_mutex_value_initial_lock_f()                              (0x0U)
#define pwr_pmu_queue_head_r(i)\
		(nvgpu_safe_add_u32(0x0010a4a0U, nvgpu_safe_mult_u32((i), 4U)))
#define pwr_pmu_queue_head__size_1_v()                             (0x00000004U)
#define pwr_pmu_queue_head_address_f(v)           ((U32(v) & 0xffffffffU) << 0U)
#define pwr_pmu_queue_head_address_v(r)              (((r) >> 0U) & 0xffffffffU)
#define pwr_pmu_queue_tail_r(i)\
		(nvgpu_safe_add_u32(0x0010a4b0U, nvgpu_safe_mult_u32((i), 4U)))
#define pwr_pmu_queue_tail__size_1_v()                             (0x00000004U)
#define pwr_pmu_queue_tail_address_f(v)           ((U32(v) & 0xffffffffU) << 0U)
#define pwr_pmu_queue_tail_address_v(r)              (((r) >> 0U) & 0xffffffffU)
#define pwr_pmu_msgq_head_r()                                      (0x0010a4c8U)
#define pwr_pmu_msgq_head_val_f(v)                ((U32(v) & 0xffffffffU) << 0U)
#define pwr_pmu_msgq_head_val_v(r)                   (((r) >> 0U) & 0xffffffffU)
#define pwr_pmu_msgq_tail_r()                                      (0x0010a4ccU)
#define pwr_pmu_msgq_tail_val_f(v)                ((U32(v) & 0xffffffffU) << 0U)
#define pwr_pmu_msgq_tail_val_v(r)                   (((r) >> 0U) & 0xffffffffU)
#define pwr_pmu_idle_mask_r(i)\
		(nvgpu_safe_add_u32(0x0010a504U, nvgpu_safe_mult_u32((i), 16U)))
#define pwr_pmu_idle_mask_gr_enabled_f()                                  (0x1U)
#define pwr_pmu_idle_mask_ce_2_enabled_f()                           (0x200000U)
#define pwr_pmu_idle_mask_1_r(i)\
		(nvgpu_safe_add_u32(0x0010aa34U, nvgpu_safe_mult_u32((i), 8U)))
#define pwr_pmu_idle_count_r(i)\
		(nvgpu_safe_add_u32(0x0010a508U, nvgpu_safe_mult_u32((i), 16U)))
#define pwr_pmu_idle_count_value_f(v)             ((U32(v) & 0x7fffffffU) << 0U)
#define pwr_pmu_idle_count_value_v(r)                (((r) >> 0U) & 0x7fffffffU)
#define pwr_pmu_idle_count_reset_f(v)                   ((U32(v) & 0x1U) << 31U)
#define pwr_pmu_idle_ctrl_r(i)\
		(nvgpu_safe_add_u32(0x0010a50cU, nvgpu_safe_mult_u32((i), 16U)))
#define pwr_pmu_idle_ctrl_value_m()                            (U32(0x3U) << 0U)
#define pwr_pmu_idle_ctrl_value_busy_f()                                  (0x2U)
#define pwr_pmu_idle_ctrl_value_always_f()                                (0x3U)
#define pwr_pmu_idle_ctrl_filter_m()                           (U32(0x1U) << 2U)
#define pwr_pmu_idle_ctrl_filter_disabled_f()                             (0x0U)
#define pwr_pmu_idle_threshold_r(i)\
		(nvgpu_safe_add_u32(0x0010a8a0U, nvgpu_safe_mult_u32((i), 4U)))
#define pwr_pmu_idle_threshold_value_f(v)         ((U32(v) & 0x7fffffffU) << 0U)
#define pwr_pmu_idle_intr_r()                                      (0x0010a9e8U)
#define pwr_pmu_idle_intr_en_f(v)                        ((U32(v) & 0x1U) << 0U)
#define pwr_pmu_idle_intr_en_disabled_v()                          (0x00000000U)
#define pwr_pmu_idle_intr_en_enabled_v()                           (0x00000001U)
#define pwr_pmu_idle_intr_status_r()                               (0x0010a9ecU)
#define pwr_pmu_idle_intr_status_intr_f(v)               ((U32(v) & 0x1U) << 0U)
#define pwr_pmu_idle_intr_status_intr_m()                      (U32(0x1U) << 0U)
#define pwr_pmu_idle_intr_status_intr_v(r)                  (((r) >> 0U) & 0x1U)
#define pwr_pmu_idle_mask_supp_r(i)\
		(nvgpu_safe_add_u32(0x0010a9f0U, nvgpu_safe_mult_u32((i), 8U)))
#define pwr_pmu_idle_mask_1_supp_r(i)\
		(nvgpu_safe_add_u32(0x0010a9f4U, nvgpu_safe_mult_u32((i), 8U)))
#define pwr_pmu_idle_ctrl_supp_r(i)\
		(nvgpu_safe_add_u32(0x0010aa30U, nvgpu_safe_mult_u32((i), 8U)))
#define pwr_pmu_debug_r(i)\
		(nvgpu_safe_add_u32(0x0010a5c0U, nvgpu_safe_mult_u32((i), 4U)))
#define pwr_pmu_debug__size_1_v()                                  (0x00000004U)
#define pwr_pmu_mailbox_r(i)\
		(nvgpu_safe_add_u32(0x0010a450U, nvgpu_safe_mult_u32((i), 4U)))
#define pwr_pmu_mailbox__size_1_v()                                (0x0000000cU)
#define pwr_pmu_bar0_addr_r()                                      (0x0010a7a0U)
#define pwr_pmu_bar0_data_r()                                      (0x0010a7a4U)
#define pwr_pmu_bar0_ctl_r()                                       (0x0010a7acU)
#define pwr_pmu_bar0_timeout_r()                                   (0x0010a7a8U)
#define pwr_pmu_bar0_fecs_error_r()                                (0x0010a988U)
#define pwr_pmu_bar0_error_status_r()                              (0x0010a7b0U)
#define pwr_pmu_bar0_error_status_timeout_host_m()             (U32(0x1U) << 0U)
#define pwr_pmu_bar0_error_status_timeout_fecs_m()             (U32(0x1U) << 1U)
#define pwr_pmu_bar0_error_status_cmd_hwerr_m()                (U32(0x1U) << 2U)
#define pwr_pmu_bar0_error_status_err_cmd_m()                  (U32(0x1U) << 3U)
#define pwr_pmu_bar0_error_status_hosterr_m()                 (U32(0x1U) << 30U)
#define pwr_pmu_bar0_error_status_fecserr_m()                 (U32(0x1U) << 31U)
#define pwr_pmu_pg_idlefilth_r(i)\
		(nvgpu_safe_add_u32(0x0010a6c0U, nvgpu_safe_mult_u32((i), 4U)))
#define pwr_pmu_pg_ppuidlefilth_r(i)\
		(nvgpu_safe_add_u32(0x0010a6e8U, nvgpu_safe_mult_u32((i), 4U)))
#define pwr_pmu_pg_idle_cnt_r(i)\
		(nvgpu_safe_add_u32(0x0010a710U, nvgpu_safe_mult_u32((i), 4U)))
#define pwr_pmu_pg_intren_r(i)\
		(nvgpu_safe_add_u32(0x0010a760U, nvgpu_safe_mult_u32((i), 4U)))
#define pwr_fbif_transcfg_r(i)\
		(nvgpu_safe_add_u32(0x0010a600U, nvgpu_safe_mult_u32((i), 4U)))
#define pwr_fbif_transcfg_target_local_fb_f()                             (0x0U)
#define pwr_fbif_transcfg_target_coherent_sysmem_f()                      (0x1U)
#define pwr_fbif_transcfg_target_noncoherent_sysmem_f()                   (0x2U)
#define pwr_fbif_transcfg_mem_type_s()                                      (1U)
#define pwr_fbif_transcfg_mem_type_f(v)                  ((U32(v) & 0x1U) << 2U)
#define pwr_fbif_transcfg_mem_type_m()                         (U32(0x1U) << 2U)
#define pwr_fbif_transcfg_mem_type_v(r)                     (((r) >> 2U) & 0x1U)
#define pwr_fbif_transcfg_mem_type_virtual_f()                            (0x0U)
#define pwr_fbif_transcfg_mem_type_physical_f()                           (0x4U)
#endif
