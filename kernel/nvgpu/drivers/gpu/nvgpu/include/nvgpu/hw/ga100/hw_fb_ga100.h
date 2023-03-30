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
#ifndef NVGPU_HW_FB_GA100_H
#define NVGPU_HW_FB_GA100_H

#include <nvgpu/types.h>
#include <nvgpu/static_analysis.h>

#define fb_fbhub_num_active_ltcs_r()                               (0x00100800U)
#define fb_fbhub_num_active_ltcs_hub_sys_atomic_mode_m()      (U32(0x1U) << 25U)
#define fb_fbhub_num_active_ltcs_hub_sys_atomic_mode_use_rmw_f()    (0x2000000U)
#define fb_fbhub_num_active_ltcs_hub_sys_ncoh_atomic_mode_m() (U32(0x1U) << 26U)
#define fb_fbhub_num_active_ltcs_hub_sys_ncoh_atomic_mode_use_read_f()    (0x0U)
#define fb_fbhub_num_active_ltcs_count_f(v)             ((U32(v) & 0x1fU) << 0U)
#define fb_fbhub_num_active_ltcs_count_m()                    (U32(0x1fU) << 0U)
#define fb_fbhub_num_active_ltcs_count_v(r)                (((r) >> 0U) & 0x1fU)
#define fb_mmu_hypervisor_ctl_r()                                  (0x00100ed0U)
#define fb_mmu_hypervisor_ctl_force_cbc_raw_mode_v(r)       (((r) >> 5U) & 0x1U)
#define fb_mmu_hypervisor_ctl_force_cbc_raw_mode_disable_v()       (0x00000000U)
#define fb_mmu_ctrl_r()                                            (0x00100c80U)
#define fb_mmu_ctrl_pri_fifo_empty_v(r)                    (((r) >> 15U) & 0x1U)
#define fb_mmu_ctrl_pri_fifo_empty_false_f()                              (0x0U)
#define fb_mmu_ctrl_pri_fifo_space_v(r)                   (((r) >> 16U) & 0xffU)
#define fb_mmu_ctrl_atomic_capability_mode_m()                (U32(0x3U) << 24U)
#define fb_mmu_ctrl_atomic_capability_mode_rmw_f()                  (0x2000000U)
#define fb_mmu_ctrl_atomic_capability_sys_ncoh_mode_m()       (U32(0x1U) << 27U)
#define fb_mmu_ctrl_atomic_capability_sys_ncoh_mode_l2_f()                (0x0U)
#define fb_mmu_bind_imb_r()                                        (0x00100cacU)
#define fb_mmu_bind_imb_aperture_f(v)                    ((U32(v) & 0x3U) << 0U)
#define fb_mmu_bind_imb_aperture_vid_mem_f()                              (0x0U)
#define fb_mmu_bind_imb_aperture_sys_mem_c_f()                            (0x2U)
#define fb_mmu_bind_imb_aperture_sys_mem_nc_f()                           (0x3U)
#define fb_mmu_bind_imb_addr_f(v)                  ((U32(v) & 0xfffffffU) << 4U)
#define fb_mmu_bind_imb_addr_alignment_v()                         (0x0000000cU)
#define fb_mmu_bind_r()                                            (0x00100cb0U)
#define fb_mmu_bind_engine_id_f(v)                      ((U32(v) & 0xffU) << 0U)
#define fb_mmu_bind_trigger_true_f()                               (0x80000000U)
#define fb_priv_mmu_phy_secure_r()                                 (0x00100ce4U)
#define fb_mmu_invalidate_pdb_r()                                  (0x00100cb8U)
#define fb_mmu_invalidate_pdb_aperture_vid_mem_f()                        (0x0U)
#define fb_mmu_invalidate_pdb_aperture_sys_mem_f()                        (0x2U)
#define fb_mmu_invalidate_pdb_addr_f(v)            ((U32(v) & 0xfffffffU) << 4U)
#define fb_mmu_invalidate_r()                                      (0x00100cbcU)
#define fb_mmu_invalidate_all_va_true_f()                                 (0x1U)
#define fb_mmu_invalidate_all_pdb_true_f()                                (0x2U)
#define fb_mmu_invalidate_replay_s()                                        (3U)
#define fb_mmu_invalidate_replay_start_ack_all_f()                       (0x10U)
#define fb_mmu_invalidate_replay_cancel_global_f()                       (0x20U)
#define fb_mmu_invalidate_trigger_v(r)                     (((r) >> 31U) & 0x1U)
#define fb_mmu_invalidate_trigger_true_v()                         (0x00000001U)
#define fb_mmu_invalidate_trigger_true_f()                         (0x80000000U)
#define fb_mmu_debug_wr_r()                                        (0x00100cc8U)
#define fb_mmu_debug_wr_aperture_s()                                        (2U)
#define fb_mmu_debug_wr_aperture_v(r)                       (((r) >> 0U) & 0x3U)
#define fb_mmu_debug_wr_aperture_vid_mem_f()                              (0x0U)
#define fb_mmu_debug_wr_aperture_sys_mem_coh_f()                          (0x2U)
#define fb_mmu_debug_wr_aperture_sys_mem_ncoh_f()                         (0x3U)
#define fb_mmu_debug_wr_vol_false_f()                                     (0x0U)
#define fb_mmu_debug_wr_addr_f(v)                  ((U32(v) & 0xfffffffU) << 4U)
#define fb_mmu_debug_wr_addr_alignment_v()                         (0x0000000cU)
#define fb_mmu_debug_rd_r()                                        (0x00100cccU)
#define fb_mmu_debug_rd_aperture_vid_mem_f()                              (0x0U)
#define fb_mmu_debug_rd_vol_false_f()                                     (0x0U)
#define fb_mmu_debug_rd_addr_f(v)                  ((U32(v) & 0xfffffffU) << 4U)
#define fb_mmu_debug_rd_addr_alignment_v()                         (0x0000000cU)
#define fb_mmu_debug_ctrl_r()                                      (0x00100cc4U)
#define fb_mmu_debug_ctrl_debug_v(r)                       (((r) >> 16U) & 0x1U)
#define fb_mmu_debug_ctrl_debug_m()                           (U32(0x1U) << 16U)
#define fb_mmu_debug_ctrl_debug_enabled_v()                        (0x00000001U)
#define fb_mmu_debug_ctrl_debug_enabled_f()                           (0x10000U)
#define fb_mmu_debug_ctrl_debug_disabled_f()                              (0x0U)
#define fb_mmu_l2tlb_ecc_status_r()                                (0x00100e70U)
#define fb_mmu_l2tlb_ecc_status_corrected_err_l2tlb_sa_data_m()\
				(U32(0x1U) << 0U)
#define fb_mmu_l2tlb_ecc_status_uncorrected_err_l2tlb_sa_data_m()\
				(U32(0x1U) << 1U)
#define fb_mmu_l2tlb_ecc_status_corrected_err_total_counter_overflow_m()\
				(U32(0x1U) << 16U)
#define fb_mmu_l2tlb_ecc_status_corrected_err_unique_counter_overflow_m()\
				(U32(0x1U) << 17U)
#define fb_mmu_l2tlb_ecc_status_uncorrected_err_total_counter_overflow_m()\
				(U32(0x1U) << 18U)
#define fb_mmu_l2tlb_ecc_status_uncorrected_err_unique_counter_overflow_m()\
				(U32(0x1U) << 19U)
#define fb_mmu_l2tlb_ecc_status_reset_clear_f()                    (0x40000000U)
#define fb_mmu_l2tlb_ecc_corrected_err_count_r()                   (0x00100e74U)
#define fb_mmu_l2tlb_ecc_corrected_err_count_total_s()                     (16U)
#define fb_mmu_l2tlb_ecc_corrected_err_count_total_v(r)  (((r) >> 0U) & 0xffffU)
#define fb_mmu_l2tlb_ecc_corrected_err_count_unique_s()                    (16U)
#define fb_mmu_l2tlb_ecc_corrected_err_count_unique_v(r)\
				(((r) >> 16U) & 0xffffU)
#define fb_mmu_l2tlb_ecc_uncorrected_err_count_r()                 (0x00100e78U)
#define fb_mmu_l2tlb_ecc_uncorrected_err_count_total_s()                   (16U)
#define fb_mmu_l2tlb_ecc_uncorrected_err_count_total_v(r)\
				(((r) >> 0U) & 0xffffU)
#define fb_mmu_l2tlb_ecc_uncorrected_err_count_unique_s()                  (16U)
#define fb_mmu_l2tlb_ecc_uncorrected_err_count_unique_v(r)\
				(((r) >> 16U) & 0xffffU)
#define fb_mmu_l2tlb_ecc_address_r()                               (0x00100e7cU)
#define fb_mmu_hubtlb_ecc_status_r()                               (0x00100e84U)
#define fb_mmu_hubtlb_ecc_status_corrected_err_sa_data_m()     (U32(0x1U) << 0U)
#define fb_mmu_hubtlb_ecc_status_uncorrected_err_sa_data_m()   (U32(0x1U) << 1U)
#define fb_mmu_hubtlb_ecc_status_corrected_err_total_counter_overflow_m()\
				(U32(0x1U) << 16U)
#define fb_mmu_hubtlb_ecc_status_corrected_err_unique_counter_overflow_m()\
				(U32(0x1U) << 17U)
#define fb_mmu_hubtlb_ecc_status_uncorrected_err_total_counter_overflow_m()\
				(U32(0x1U) << 18U)
#define fb_mmu_hubtlb_ecc_status_uncorrected_err_unique_counter_overflow_m()\
				(U32(0x1U) << 19U)
#define fb_mmu_hubtlb_ecc_status_reset_clear_f()                   (0x40000000U)
#define fb_mmu_hubtlb_ecc_corrected_err_count_r()                  (0x00100e88U)
#define fb_mmu_hubtlb_ecc_corrected_err_count_total_s()                    (16U)
#define fb_mmu_hubtlb_ecc_corrected_err_count_total_v(r) (((r) >> 0U) & 0xffffU)
#define fb_mmu_hubtlb_ecc_corrected_err_count_unique_s()                   (16U)
#define fb_mmu_hubtlb_ecc_corrected_err_count_unique_v(r)\
				(((r) >> 16U) & 0xffffU)
#define fb_mmu_hubtlb_ecc_uncorrected_err_count_r()                (0x00100e8cU)
#define fb_mmu_hubtlb_ecc_uncorrected_err_count_total_s()                  (16U)
#define fb_mmu_hubtlb_ecc_uncorrected_err_count_total_v(r)\
				(((r) >> 0U) & 0xffffU)
#define fb_mmu_hubtlb_ecc_uncorrected_err_count_unique_s()                 (16U)
#define fb_mmu_hubtlb_ecc_uncorrected_err_count_unique_v(r)\
				(((r) >> 16U) & 0xffffU)
#define fb_mmu_hubtlb_ecc_address_r()                              (0x00100e90U)
#define fb_mmu_fillunit_ecc_status_r()                             (0x00100e98U)
#define fb_mmu_fillunit_ecc_status_corrected_err_pte_data_m()  (U32(0x1U) << 0U)
#define fb_mmu_fillunit_ecc_status_uncorrected_err_pte_data_m()\
				(U32(0x1U) << 1U)
#define fb_mmu_fillunit_ecc_status_corrected_err_pde0_data_m() (U32(0x1U) << 2U)
#define fb_mmu_fillunit_ecc_status_uncorrected_err_pde0_data_m()\
				(U32(0x1U) << 3U)
#define fb_mmu_fillunit_ecc_status_corrected_err_total_counter_overflow_m()\
				(U32(0x1U) << 16U)
#define fb_mmu_fillunit_ecc_status_corrected_err_unique_counter_overflow_m()\
				(U32(0x1U) << 17U)
#define fb_mmu_fillunit_ecc_status_uncorrected_err_total_counter_overflow_m()\
				(U32(0x1U) << 18U)
#define fb_mmu_fillunit_ecc_status_uncorrected_err_unique_counter_overflow_m()\
				(U32(0x1U) << 19U)
#define fb_mmu_fillunit_ecc_status_reset_clear_f()                 (0x40000000U)
#define fb_mmu_fillunit_ecc_corrected_err_count_r()                (0x00100e9cU)
#define fb_mmu_fillunit_ecc_corrected_err_count_total_s()                  (16U)
#define fb_mmu_fillunit_ecc_corrected_err_count_total_v(r)\
				(((r) >> 0U) & 0xffffU)
#define fb_mmu_fillunit_ecc_corrected_err_count_unique_s()                 (16U)
#define fb_mmu_fillunit_ecc_corrected_err_count_unique_v(r)\
				(((r) >> 16U) & 0xffffU)
#define fb_mmu_fillunit_ecc_uncorrected_err_count_r()              (0x00100ea0U)
#define fb_mmu_fillunit_ecc_uncorrected_err_count_total_s()                (16U)
#define fb_mmu_fillunit_ecc_uncorrected_err_count_total_v(r)\
				(((r) >> 0U) & 0xffffU)
#define fb_mmu_fillunit_ecc_uncorrected_err_count_unique_s()               (16U)
#define fb_mmu_fillunit_ecc_uncorrected_err_count_unique_v(r)\
				(((r) >> 16U) & 0xffffU)
#define fb_mmu_fillunit_ecc_address_r()                            (0x00100ea4U)
#define fb_niso_cfg1_r()                                           (0x00100c14U)
#define fb_niso_cfg1_sysmem_nvlink_m()                        (U32(0x1U) << 17U)
#define fb_niso_cfg1_sysmem_nvlink_enabled_f()                        (0x20000U)
#define fb_niso_flush_sysmem_addr_r()                              (0x00100c10U)
#define fb_mmu_fault_buffer_lo_r(i)\
		(nvgpu_safe_add_u32(0x00100e24U, nvgpu_safe_mult_u32((i), 20U)))
#define fb_mmu_fault_buffer_lo_addr_f(v)            ((U32(v) & 0xfffffU) << 12U)
#define fb_mmu_fault_buffer_hi_r(i)\
		(nvgpu_safe_add_u32(0x00100e28U, nvgpu_safe_mult_u32((i), 20U)))
#define fb_mmu_fault_buffer_hi_addr_f(v)          ((U32(v) & 0xffffffffU) << 0U)
#define fb_mmu_fault_buffer_get_r(i)\
		(nvgpu_safe_add_u32(0x00100e2cU, nvgpu_safe_mult_u32((i), 20U)))
#define fb_mmu_fault_buffer_get_ptr_f(v)             ((U32(v) & 0xfffffU) << 0U)
#define fb_mmu_fault_buffer_get_ptr_m()                    (U32(0xfffffU) << 0U)
#define fb_mmu_fault_buffer_get_ptr_v(r)                (((r) >> 0U) & 0xfffffU)
#define fb_mmu_fault_buffer_get_getptr_corrupted_m()          (U32(0x1U) << 30U)
#define fb_mmu_fault_buffer_get_getptr_corrupted_clear_f()         (0x40000000U)
#define fb_mmu_fault_buffer_get_overflow_m()                  (U32(0x1U) << 31U)
#define fb_mmu_fault_buffer_get_overflow_clear_f()                 (0x80000000U)
#define fb_mmu_fault_buffer_put_r(i)\
		(nvgpu_safe_add_u32(0x00100e30U, nvgpu_safe_mult_u32((i), 20U)))
#define fb_mmu_fault_buffer_put_ptr_v(r)                (((r) >> 0U) & 0xfffffU)
#define fb_mmu_fault_buffer_size_r(i)\
		(nvgpu_safe_add_u32(0x00100e34U, nvgpu_safe_mult_u32((i), 20U)))
#define fb_mmu_fault_buffer_size_val_f(v)            ((U32(v) & 0xfffffU) << 0U)
#define fb_mmu_fault_buffer_size_val_v(r)               (((r) >> 0U) & 0xfffffU)
#define fb_mmu_fault_buffer_size_overflow_intr_enable_f()          (0x20000000U)
#define fb_mmu_fault_buffer_size_enable_m()                   (U32(0x1U) << 31U)
#define fb_mmu_fault_buffer_size_enable_v(r)               (((r) >> 31U) & 0x1U)
#define fb_mmu_fault_buffer_size_enable_true_f()                   (0x80000000U)
#define fb_mmu_fault_addr_lo_r()                                   (0x00100e4cU)
#define fb_mmu_fault_addr_lo_phys_aperture_v(r)             (((r) >> 0U) & 0x3U)
#define fb_mmu_fault_addr_lo_addr_v(r)                 (((r) >> 12U) & 0xfffffU)
#define fb_mmu_fault_addr_hi_r()                                   (0x00100e50U)
#define fb_mmu_fault_addr_hi_addr_v(r)               (((r) >> 0U) & 0xffffffffU)
#define fb_mmu_fault_inst_lo_r()                                   (0x00100e54U)
#define fb_mmu_fault_inst_lo_engine_id_v(r)               (((r) >> 0U) & 0x1ffU)
#define fb_mmu_fault_inst_lo_aperture_v(r)                 (((r) >> 10U) & 0x3U)
#define fb_mmu_fault_inst_lo_addr_v(r)                 (((r) >> 12U) & 0xfffffU)
#define fb_mmu_fault_inst_hi_r()                                   (0x00100e58U)
#define fb_mmu_fault_inst_hi_addr_v(r)               (((r) >> 0U) & 0xffffffffU)
#define fb_mmu_fault_info_r()                                      (0x00100e5cU)
#define fb_mmu_fault_info_fault_type_v(r)                  (((r) >> 0U) & 0x1fU)
#define fb_mmu_fault_info_replayable_fault_v(r)             (((r) >> 7U) & 0x1U)
#define fb_mmu_fault_info_client_v(r)                      (((r) >> 8U) & 0x7fU)
#define fb_mmu_fault_info_access_type_v(r)                 (((r) >> 16U) & 0xfU)
#define fb_mmu_fault_info_client_type_v(r)                 (((r) >> 20U) & 0x1U)
#define fb_mmu_fault_info_gpc_id_v(r)                     (((r) >> 24U) & 0x1fU)
#define fb_mmu_fault_info_protected_mode_v(r)              (((r) >> 29U) & 0x1U)
#define fb_mmu_fault_info_replayable_fault_en_v(r)         (((r) >> 30U) & 0x1U)
#define fb_mmu_fault_info_valid_v(r)                       (((r) >> 31U) & 0x1U)
#define fb_mmu_fault_status_r()                                    (0x00100e60U)
#define fb_mmu_fault_status_dropped_bar1_phys_set_f()                     (0x1U)
#define fb_mmu_fault_status_dropped_bar1_virt_set_f()                     (0x2U)
#define fb_mmu_fault_status_dropped_bar2_phys_set_f()                     (0x4U)
#define fb_mmu_fault_status_dropped_bar2_virt_set_f()                     (0x8U)
#define fb_mmu_fault_status_dropped_ifb_phys_set_f()                     (0x10U)
#define fb_mmu_fault_status_dropped_ifb_virt_set_f()                     (0x20U)
#define fb_mmu_fault_status_dropped_other_phys_set_f()                   (0x40U)
#define fb_mmu_fault_status_dropped_other_virt_set_f()                   (0x80U)
#define fb_mmu_fault_status_replayable_m()                     (U32(0x1U) << 8U)
#define fb_mmu_fault_status_replayable_error_m()              (U32(0x1U) << 10U)
#define fb_mmu_fault_status_non_replayable_error_m()          (U32(0x1U) << 11U)
#define fb_mmu_fault_status_replayable_overflow_m()           (U32(0x1U) << 12U)
#define fb_mmu_fault_status_non_replayable_overflow_m()       (U32(0x1U) << 13U)
#define fb_mmu_fault_status_replayable_getptr_corrupted_m()   (U32(0x1U) << 14U)
#define fb_mmu_fault_status_non_replayable_getptr_corrupted_m()\
				(U32(0x1U) << 15U)
#define fb_mmu_fault_status_busy_true_f()                          (0x40000000U)
#define fb_mmu_fault_status_valid_m()                         (U32(0x1U) << 31U)
#define fb_mmu_fault_status_valid_set_f()                          (0x80000000U)
#define fb_mmu_fault_status_valid_clear_f()                        (0x80000000U)
#define fb_mmu_local_memory_range_r()                              (0x00100ce0U)
#define fb_mmu_local_memory_range_lower_scale_v(r)          (((r) >> 0U) & 0xfU)
#define fb_mmu_local_memory_range_lower_mag_v(r)           (((r) >> 4U) & 0x3fU)
#define fb_mmu_local_memory_range_ecc_mode_v(r)            (((r) >> 30U) & 0x1U)
#define fb_niso_scrub_status_r()                                   (0x00100b20U)
#define fb_niso_scrub_status_flag_v(r)                      (((r) >> 0U) & 0x1U)
#define fb_mmu_int_vector_info_fault_r()                           (0x00100ee0U)
#define fb_mmu_int_vector_info_fault_vector_v(r)         (((r) >> 0U) & 0xffffU)
#define fb_mmu_int_vector_ecc_error_r()                            (0x00100edcU)
#define fb_mmu_int_vector_ecc_error_vector_v(r)          (((r) >> 0U) & 0xffffU)
#define fb_mmu_int_vector_fault_r(i)\
		(nvgpu_safe_add_u32(0x00100ee4U, nvgpu_safe_mult_u32((i), 4U)))
#define fb_mmu_int_vector_fault_error_v(r)               (((r) >> 0U) & 0xffffU)
#define fb_mmu_int_vector_fault_notify_v(r)             (((r) >> 16U) & 0xffffU)
#define fb_mmu_num_active_ltcs_r()                                 (0x00100ec0U)
#define fb_mmu_num_active_ltcs_count_v(r)                  (((r) >> 0U) & 0x1fU)
#define fb_mmu_cbc_base_r()                                        (0x00100ec4U)
#define fb_mmu_cbc_base_address_f(v)               ((U32(v) & 0x3ffffffU) << 0U)
#define fb_mmu_cbc_base_address_alignment_shift_v()                (0x0000000bU)
#define fb_mmu_cbc_top_r()                                         (0x00100ec8U)
#define fb_mmu_cbc_top_size_f(v)                      ((U32(v) & 0x7fffU) << 0U)
#define fb_mmu_cbc_max_r()                                         (0x00100eccU)
#define fb_mmu_cbc_max_comptagline_f(v)             ((U32(v) & 0xffffffU) << 0U)
#define fb_mmu_cbc_max_comptagline_m()                    (U32(0xffffffU) << 0U)
#define fb_mmu_vpr_mode_r()                                        (0x001fa800U)
#define fb_mmu_vpr_mode_fetch_f(v)                       ((U32(v) & 0x1U) << 2U)
#define fb_mmu_vpr_mode_fetch_v(r)                          (((r) >> 2U) & 0x1U)
#define fb_mmu_vpr_mode_fetch_false_v()                            (0x00000000U)
#define fb_mmu_vpr_mode_fetch_true_f()                                    (0x4U)
#define fb_mmu_vpr_addr_lo_r()                                     (0x001fa804U)
#define fb_mmu_vpr_addr_lo_val_v(r)                   (((r) >> 4U) & 0xfffffffU)
#define fb_mmu_vpr_addr_lo_val_alignment_v()                       (0x0000000cU)
#define fb_mmu_vpr_addr_hi_r()                                     (0x001fa808U)
#define fb_mmu_vpr_addr_hi_val_v(r)                   (((r) >> 4U) & 0xfffffffU)
#define fb_mmu_vpr_addr_hi_val_alignment_v()                       (0x0000000cU)
#define fb_mmu_vpr_cya_lo_r()                                      (0x001fa80cU)
#define fb_mmu_vpr_cya_hi_r()                                      (0x001fa810U)
#define fb_mmu_wpr1_addr_lo_r()                                    (0x001fa81cU)
#define fb_mmu_wpr1_addr_lo_val_v(r)                  (((r) >> 4U) & 0xfffffffU)
#define fb_mmu_wpr1_addr_lo_val_alignment_v()                      (0x0000000cU)
#define fb_mmu_wpr1_addr_hi_r()                                    (0x001fa820U)
#define fb_mmu_wpr1_addr_hi_val_v(r)                  (((r) >> 4U) & 0xfffffffU)
#define fb_mmu_wpr1_addr_hi_val_alignment_v()                      (0x0000000cU)
#define fb_mmu_wpr2_addr_lo_r()                                    (0x001fa824U)
#define fb_mmu_wpr2_addr_lo_val_v(r)                  (((r) >> 4U) & 0xfffffffU)
#define fb_mmu_wpr2_addr_lo_val_alignment_v()                      (0x0000000cU)
#define fb_mmu_wpr2_addr_hi_r()                                    (0x001fa828U)
#define fb_mmu_wpr2_addr_hi_val_v(r)                  (((r) >> 4U) & 0xfffffffU)
#define fb_mmu_wpr2_addr_hi_val_alignment_v()                      (0x0000000cU)
#define fb_mmu_wpr_allow_read_r()                                  (0x001fa814U)
#define fb_mmu_wpr_allow_write_r()                                 (0x001fa818U)
#define fb_mmu_smc_eng_cfg_0_r(i)\
		(nvgpu_safe_add_u32(0x001f94c0U, nvgpu_safe_mult_u32((i), 4U)))
#define fb_mmu_smc_eng_cfg_0_remote_swizid_f(v)          ((U32(v) & 0xfU) << 0U)
#define fb_mmu_smc_eng_cfg_0_remote_swizid_m()                 (U32(0xfU) << 0U)
#define fb_mmu_smc_eng_cfg_0_mmu_eng_veid_offset_f(v)  ((U32(v) & 0x3fU) << 16U)
#define fb_mmu_smc_eng_cfg_0_mmu_eng_veid_offset_m()         (U32(0x3fU) << 16U)
#define fb_mmu_smc_eng_cfg_0_veid_max_f(v)             ((U32(v) & 0x3fU) << 24U)
#define fb_mmu_smc_eng_cfg_0_veid_max_m()                    (U32(0x3fU) << 24U)
#define fb_mmu_smc_eng_cfg_1_r(i)\
		(nvgpu_safe_add_u32(0x001f94e0U, nvgpu_safe_mult_u32((i), 4U)))
#define fb_mmu_smc_eng_cfg_1_gpc_mask_f(v)            ((U32(v) & 0xffffU) << 0U)
#define fb_mmu_smc_eng_cfg_1_gpc_mask_m()                   (U32(0xffffU) << 0U)
#define fb_mmu_mmu_eng_id_cfg_r(i)\
		(nvgpu_safe_add_u32(0x001f9600U, nvgpu_safe_mult_u32((i), 4U)))
#define fb_mmu_mmu_eng_id_cfg_remote_swizid_f(v)         ((U32(v) & 0xfU) << 0U)
#define fb_mmu_mmu_eng_id_cfg_remote_swizid_m()                (U32(0xfU) << 0U)
#define fb_mmu_hypervisor_ctl_r()                                  (0x00100ed0U)
#define fb_mmu_hypervisor_ctl_use_smc_veid_tables_f(v)   ((U32(v) & 0x1U) << 3U)
#define fb_mmu_hypervisor_ctl_use_smc_veid_tables_m()          (U32(0x1U) << 3U)
#define fb_mmu_hypervisor_ctl_use_smc_veid_tables_enable_v()       (0x00000001U)
#define fb_mmu_hypervisor_ctl_use_smc_veid_tables_disable_v()      (0x00000000U)
#define fb_hshub_prg_config_r(i)\
		(nvgpu_safe_add_u32(0x00004c7cU, nvgpu_safe_mult_u32((i), 8192U)))
#define fb_hshub_prg_config_num_hshubs_v(r)                 (((r) >> 1U) & 0x3U)
#define fb_hshub_num_active_ltcs_r(i)\
		(nvgpu_safe_add_u32(0x00004c20U, nvgpu_safe_mult_u32((i), 8192U)))
#endif
