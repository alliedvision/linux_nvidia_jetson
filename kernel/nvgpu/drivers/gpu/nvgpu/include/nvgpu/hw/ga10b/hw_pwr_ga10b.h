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
#ifndef NVGPU_HW_PWR_GA10B_H
#define NVGPU_HW_PWR_GA10B_H

#include <nvgpu/types.h>
#include <nvgpu/static_analysis.h>

#define pwr_falcon2_pwr_base_r()                                   (0x0010b000U)
#define pwr_falcon_irqsset_r()                                     (0x0010a000U)
#define pwr_falcon_irqsset_swgen0_set_f()                                (0x40U)
#define pwr_falcon_irqsclr_r()                                     (0x0010a004U)
#define pwr_falcon_irqstat_r()                                     (0x0010a008U)
#define pwr_falcon_irqstat_wdt_true_f()                                   (0x2U)
#define pwr_falcon_irqstat_halt_true_f()                                 (0x10U)
#define pwr_falcon_irqstat_exterr_true_f()                               (0x20U)
#define pwr_falcon_irqstat_swgen0_true_f()                               (0x40U)
#define pwr_falcon_irqstat_ext_ecc_parity_true_f()                      (0x400U)
#define pwr_falcon_irqstat_swgen1_true_f()                               (0x80U)
#define pwr_falcon_irqstat_memerr_true_f()                            (0x40000U)
#define pwr_falcon_irqstat_iopmp_true_f()                            (0x800000U)
#define pwr_pmu_ecc_intr_status_r()                                (0x0010abfcU)
#define pwr_pmu_ecc_intr_status_corrected_m()                  (U32(0x1U) << 0U)
#define pwr_pmu_ecc_intr_status_uncorrected_m()                (U32(0x1U) << 1U)
#define pwr_falcon_irqmset_r()                                     (0x0010a010U)
#define pwr_falcon_irqmset_gptmr_f(v)                    ((U32(v) & 0x1U) << 0U)
#define pwr_falcon_irqmset_wdtmr_f(v)                    ((U32(v) & 0x1U) << 1U)
#define pwr_falcon_irqmset_mthd_f(v)                     ((U32(v) & 0x1U) << 2U)
#define pwr_falcon_irqmset_ctxsw_f(v)                    ((U32(v) & 0x1U) << 3U)
#define pwr_falcon_irqmset_halt_f(v)                     ((U32(v) & 0x1U) << 4U)
#define pwr_falcon_irqmset_exterr_f(v)                   ((U32(v) & 0x1U) << 5U)
#define pwr_falcon_irqmset_swgen0_f(v)                   ((U32(v) & 0x1U) << 6U)
#define pwr_falcon_irqmset_swgen1_f(v)                   ((U32(v) & 0x1U) << 7U)
#define pwr_falcon_irqmset_ext_ecc_parity_f(v)          ((U32(v) & 0x1U) << 10U)
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
#define pwr_falcon_irqdest_host_ext_ecc_parity_f(v)     ((U32(v) & 0x1U) << 10U)
#define pwr_falcon_irqdest_target_gptmr_f(v)            ((U32(v) & 0x1U) << 16U)
#define pwr_falcon_irqdest_target_wdtmr_f(v)            ((U32(v) & 0x1U) << 17U)
#define pwr_falcon_irqdest_target_mthd_f(v)             ((U32(v) & 0x1U) << 18U)
#define pwr_falcon_irqdest_target_ctxsw_f(v)            ((U32(v) & 0x1U) << 19U)
#define pwr_falcon_irqdest_target_halt_f(v)             ((U32(v) & 0x1U) << 20U)
#define pwr_falcon_irqdest_target_exterr_f(v)           ((U32(v) & 0x1U) << 21U)
#define pwr_falcon_irqdest_target_swgen0_f(v)           ((U32(v) & 0x1U) << 22U)
#define pwr_falcon_irqdest_target_swgen1_f(v)           ((U32(v) & 0x1U) << 23U)
#define pwr_falcon_irqdest_target_ext_ecc_parity_f(v)   ((U32(v) & 0x1U) << 26U)
#define pwr_falcon_mailbox1_r()                                    (0x0010a044U)
#define pwr_falcon_itfen_r()                                       (0x0010a048U)
#define pwr_falcon_itfen_ctxen_enable_f()                                 (0x1U)
#define pwr_falcon_os_r()                                          (0x0010a080U)
#define pwr_falcon_cpuctl_r()                                      (0x0010a100U)
#define pwr_falcon_cpuctl_startcpu_f(v)                  ((U32(v) & 0x1U) << 1U)
#define pwr_falcon_cpuctl_alias_r()                                (0x0010a130U)
#define pwr_falcon_hwcfg2_r()                                      (0x0010a0f4U)
#define pwr_falcon_hwcfg2_dbgmode_v(r)                      (((r) >> 3U) & 0x1U)
#define pwr_falcon_hwcfg2_dbgmode_enable_v()                       (0x00000001U)
#define pwr_falcon_dmatrfbase_r()                                  (0x0010a110U)
#define pwr_falcon_dmatrfbase1_r()                                 (0x0010a128U)
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
#define pwr_falcon_dmemc_r(i)\
		(nvgpu_safe_add_u32(0x0010a1c0U, nvgpu_safe_mult_u32((i), 8U)))
#define pwr_falcon_dmemc_offs_f(v)                      ((U32(v) & 0x3fU) << 2U)
#define pwr_falcon_dmemc_blk_f(v)                     ((U32(v) & 0xffffU) << 8U)
#define pwr_falcon_dmemc_aincw_f(v)                     ((U32(v) & 0x1U) << 24U)
#define pwr_falcon_dmemd_r(i)\
		(nvgpu_safe_add_u32(0x0010a1c4U, nvgpu_safe_mult_u32((i), 8U)))
#define pwr_pmu_new_instblk_r()                                    (0x0010a480U)
#define pwr_pmu_new_instblk_ptr_f(v)               ((U32(v) & 0xfffffffU) << 0U)
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
#define pwr_pmu_mutex_r(i)\
		(nvgpu_safe_add_u32(0x0010a580U, nvgpu_safe_mult_u32((i), 4U)))
#define pwr_pmu_mutex__size_1_v()                                  (0x00000010U)
#define pwr_pmu_mutex_value_f(v)                        ((U32(v) & 0xffU) << 0U)
#define pwr_pmu_mutex_value_v(r)                           (((r) >> 0U) & 0xffU)
#define pwr_pmu_mutex_value_initial_lock_f()                              (0x0U)
#define pwr_pmu_queue_head_r(i)\
		(nvgpu_safe_add_u32(0x0010a800U, nvgpu_safe_mult_u32((i), 4U)))
#define pwr_pmu_queue_head__size_1_v()                             (0x00000008U)
#define pwr_pmu_queue_head_address_f(v)           ((U32(v) & 0xffffffffU) << 0U)
#define pwr_pmu_queue_head_address_v(r)              (((r) >> 0U) & 0xffffffffU)
#define pwr_pmu_queue_tail_r(i)\
		(nvgpu_safe_add_u32(0x0010a820U, nvgpu_safe_mult_u32((i), 4U)))
#define pwr_pmu_queue_tail__size_1_v()                             (0x00000008U)
#define pwr_pmu_queue_tail_address_f(v)           ((U32(v) & 0xffffffffU) << 0U)
#define pwr_pmu_queue_tail_address_v(r)              (((r) >> 0U) & 0xffffffffU)
#define pwr_pmu_msgq_head_r()                                      (0x0010a4c8U)
#define pwr_pmu_msgq_head_val_f(v)                ((U32(v) & 0xffffffffU) << 0U)
#define pwr_pmu_msgq_head_val_v(r)                   (((r) >> 0U) & 0xffffffffU)
#define pwr_pmu_msgq_tail_r()                                      (0x0010a4ccU)
#define pwr_pmu_msgq_tail_val_f(v)                ((U32(v) & 0xffffffffU) << 0U)
#define pwr_pmu_msgq_tail_val_v(r)                   (((r) >> 0U) & 0xffffffffU)
#define pwr_pmu_idle_mask_r(i)\
		(nvgpu_safe_add_u32(0x0010be40U, nvgpu_safe_mult_u32((i), 4U)))
#define pwr_pmu_idle_mask_gr_enabled_f()                                  (0x1U)
#define pwr_pmu_idle_mask_ce_2_enabled_f()                           (0x200000U)
#define pwr_pmu_idle_count_r(i)\
		(nvgpu_safe_add_u32(0x0010bf80U, nvgpu_safe_mult_u32((i), 4U)))
#define pwr_pmu_idle_count_value_v(r)                (((r) >> 0U) & 0x7fffffffU)
#define pwr_pmu_idle_count_reset_f(v)                   ((U32(v) & 0x1U) << 31U)
#define pwr_pmu_idle_ctrl_r(i)\
		(nvgpu_safe_add_u32(0x0010bfc0U, nvgpu_safe_mult_u32((i), 4U)))
#define pwr_pmu_idle_ctrl_value_m()                            (U32(0x3U) << 0U)
#define pwr_pmu_idle_ctrl_value_busy_f()                                  (0x2U)
#define pwr_pmu_idle_ctrl_value_always_f()                                (0x3U)
#define pwr_pmu_idle_ctrl_filter_m()                           (U32(0x1U) << 2U)
#define pwr_pmu_idle_ctrl_filter_disabled_f()                             (0x0U)
#define pwr_pmu_idle_mask_supp_r(i)\
		(nvgpu_safe_add_u32(0x0010a9f0U, nvgpu_safe_mult_u32((i), 8U)))
#define pwr_pmu_idle_threshold_r(i)\
		(nvgpu_safe_add_u32(0x0010be00U, nvgpu_safe_mult_u32((i), 4U)))
#define pwr_pmu_idle_threshold_value_f(v)         ((U32(v) & 0x7fffffffU) << 0U)
#define pwr_pmu_idle_intr_r()                                      (0x0010a9e8U)
#define pwr_pmu_idle_intr_en_f(v)                        ((U32(v) & 0x1U) << 0U)
#define pwr_pmu_idle_intr_status_r()                               (0x0010a9ecU)
#define pwr_pmu_idle_intr_status_intr_f(v)               ((U32(v) & 0x1U) << 0U)
#define pwr_pmu_idle_intr_status_intr_v(r)                  (((r) >> 0U) & 0x1U)
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
#define pwr_pmu_bar0_host_error_r()                                (0x0010a990U)
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
#define pwr_pmu_falcon_ecc_control_r()                             (0x0010a484U)
#define pwr_pmu_falcon_ecc_control_inject_corrected_err_f(v)\
				((U32(v) & 0x1U) << 0U)
#define pwr_pmu_falcon_ecc_control_inject_uncorrected_err_f(v)\
				((U32(v) & 0x1U) << 1U)
#define pwr_pmu_falcon_ecc_status_r()                              (0x0010a6b0U)
#define pwr_pmu_falcon_ecc_status_corrected_err_imem_m()       (U32(0x1U) << 0U)
#define pwr_pmu_falcon_ecc_status_corrected_err_dmem_m()       (U32(0x1U) << 1U)
#define pwr_pmu_falcon_ecc_status_uncorrected_err_imem_m()     (U32(0x1U) << 8U)
#define pwr_pmu_falcon_ecc_status_uncorrected_err_dmem_m()     (U32(0x1U) << 9U)
#define pwr_pmu_falcon_ecc_status_uncorrected_err_mpu_ram_m() (U32(0x1U) << 10U)
#define pwr_pmu_falcon_ecc_status_uncorrected_err_dcls_m()    (U32(0x1U) << 11U)
#define pwr_pmu_falcon_ecc_status_uncorrected_err_reg_m()     (U32(0x1U) << 12U)
#define pwr_pmu_falcon_ecc_status_corrected_err_total_counter_overflow_m()\
				(U32(0x1U) << 16U)
#define pwr_pmu_falcon_ecc_status_uncorrected_err_total_counter_overflow_m()\
				(U32(0x1U) << 18U)
#define pwr_pmu_falcon_ecc_status_reset_task_f()                   (0x80000000U)
#define pwr_pmu_falcon_ecc_address_r()                             (0x0010a6b4U)
#define pwr_pmu_falcon_ecc_address_row_address_v(r)      (((r) >> 0U) & 0xffffU)
#define pwr_pmu_falcon_ecc_corrected_err_count_r()                 (0x0010a6b8U)
#define pwr_pmu_falcon_ecc_corrected_err_count_total_s()                   (16U)
#define pwr_pmu_falcon_ecc_corrected_err_count_total_v(r)\
				(((r) >> 0U) & 0xffffU)
#define pwr_pmu_falcon_ecc_uncorrected_err_count_r()               (0x0010a6bcU)
#define pwr_pmu_falcon_ecc_uncorrected_err_count_total_s()                 (16U)
#define pwr_pmu_falcon_ecc_uncorrected_err_count_total_v(r)\
				(((r) >> 0U) & 0xffffU)
#define pwr_fbif_transcfg_r(i)\
		(nvgpu_safe_add_u32(0x0010ae00U, nvgpu_safe_mult_u32((i), 4U)))
#define pwr_fbif_transcfg_target_local_fb_f()                             (0x0U)
#define pwr_fbif_transcfg_target_coherent_sysmem_f()                      (0x1U)
#define pwr_fbif_transcfg_target_noncoherent_sysmem_f()                   (0x2U)
#define pwr_fbif_transcfg_mem_type_virtual_f()                            (0x0U)
#define pwr_fbif_transcfg_mem_type_physical_f()                           (0x4U)
#define pwr_falcon_engine_r()                                      (0x0010a3c0U)
#define pwr_falcon_engine_reset_true_f()                                  (0x1U)
#define pwr_falcon_engine_reset_false_f()                                 (0x0U)
#define pwr_riscv_irqmask_r()                                      (0x0010b528U)
#define pwr_riscv_irqdest_r()                                      (0x0010b52cU)
#endif
