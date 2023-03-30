/*
 * Copyright (c) 2021-2022, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
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
#ifndef T234_PMASYS_SOC_HWPM_H
#define T234_PMASYS_SOC_HWPM_H

#define pmasys_cg2_r()                                             (0x0f14a044U)
#define pmasys_cg2_slcg_f(v)                                (((v) & 0x1U) << 0U)
#define pmasys_cg2_slcg_m()                                         (0x1U << 0U)
#define pmasys_cg2_slcg_enabled_v()                                (0x00000000U)
#define pmasys_cg2_slcg_enabled_f()                                       (0x0U)
#define pmasys_cg2_slcg_disabled_v()                               (0x00000001U)
#define pmasys_cg2_slcg_disabled_f()                                      (0x1U)
#define pmasys_controlb_r()                                        (0x0f14a070U)
#define pmasys_controlb_coalesce_timeout_cycles_f(v)        (((v) & 0x7U) << 4U)
#define pmasys_controlb_coalesce_timeout_cycles_m()                 (0x7U << 4U)
#define pmasys_controlb_coalesce_timeout_cycles__prod_f()                (0x40U)
#define pmasys_channel_status_secure_r(i)\
		(0x0f14a610U + ((i)*384U))
#define pmasys_channel_status_secure_membuf_status_f(v)     (((v) & 0x1U) << 0U)
#define pmasys_channel_status_secure_membuf_status_m()              (0x1U << 0U)
#define pmasys_channel_status_secure_membuf_status_v(r)     (((r) >> 0U) & 0x1U)
#define pmasys_channel_status_secure_membuf_status_init_v()        (0x00000000U)
#define pmasys_channel_status_secure_membuf_status_overflowed_v()  (0x00000001U)
#define pmasys_channel_control_user_r(i)\
		(0x0f14a620U + ((i)*384U))
#define pmasys_channel_control_user_stream_f(v)             (((v) & 0x1U) << 0U)
#define pmasys_channel_control_user_stream_m()                      (0x1U << 0U)
#define pmasys_channel_control_user_stream_disable_v()             (0x00000000U)
#define pmasys_channel_control_user_stream_disable_f()                    (0x0U)
#define pmasys_channel_control_user_stream_enable_v()              (0x00000001U)
#define pmasys_channel_control_user_update_bytes_f(v)      (((v) & 0x1U) << 31U)
#define pmasys_channel_control_user_update_bytes_m()               (0x1U << 31U)
#define pmasys_channel_control_user_update_bytes_doit_v()          (0x00000001U)
#define pmasys_channel_control_user_update_bytes_doit_f()          (0x80000000U)
#define pmasys_channel_mem_bump_r(i)\
		(0x0f14a624U + ((i)*4U))
#define pmasys_channel_mem_block_r(i)\
		(0x0f14a638U + ((i)*4U))
#define pmasys_channel_mem_block__size_1_v()                       (0x00000001U)
#define pmasys_channel_mem_block_ptr_f(v)            (((v) & 0x3fffffffU) << 0U)
#define pmasys_channel_mem_block_ptr_m()                     (0x3fffffffU << 0U)
#define pmasys_channel_mem_block_base_f(v)            (((v) & 0xfffffffU) << 0U)
#define pmasys_channel_mem_block_base_m()                     (0xfffffffU << 0U)
#define pmasys_channel_mem_block_target_f(v)               (((v) & 0x3U) << 28U)
#define pmasys_channel_mem_block_target_m()                        (0x3U << 28U)
#define pmasys_channel_mem_block_target_lfb_v()                    (0x00000000U)
#define pmasys_channel_mem_block_target_sys_coh_v()                (0x00000002U)
#define pmasys_channel_mem_block_target_sys_ncoh_v()               (0x00000003U)
#define pmasys_channel_mem_block_valid_f(v)                (((v) & 0x1U) << 31U)
#define pmasys_channel_mem_block_valid_m()                         (0x1U << 31U)
#define pmasys_channel_mem_block_valid_false_v()                   (0x00000000U)
#define pmasys_channel_mem_block_valid_true_v()                    (0x00000001U)
#define pmasys_channel_config_user_r(i)\
		(0x0f14a640U + ((i)*384U))
#define pmasys_channel_config_user_coalesce_timeout_cycles_f(v)\
				(((v) & 0x7U) << 4U)
#define pmasys_channel_config_user_coalesce_timeout_cycles_m()      (0x7U << 4U)
#define pmasys_channel_config_user_coalesce_timeout_cycles__prod_v()\
				(0x00000004U)
#define pmasys_channel_config_user_coalesce_timeout_cycles__prod_f()     (0x40U)
#define pmasys_channel_outbase_r(i)\
		(0x0f14a644U + ((i)*4U))
#define pmasys_channel_outbase_ptr_f(v)               (((v) & 0x7ffffffU) << 5U)
#define pmasys_channel_outbase_ptr_m()                        (0x7ffffffU << 5U)
#define pmasys_channel_outbase_ptr_v(r)               (((r) >> 5U) & 0x7ffffffU)
#define pmasys_channel_outbaseupper_r(i)\
		(0x0f14a648U + ((i)*4U))
#define pmasys_channel_outbaseupper_ptr_f(v)               (((v) & 0xffU) << 0U)
#define pmasys_channel_outbaseupper_ptr_m()                        (0xffU << 0U)
#define pmasys_channel_outbaseupper_ptr_v(r)               (((r) >> 0U) & 0xffU)
#define pmasys_channel_outsize_r(i)\
		(0x0f14a64cU + ((i)*4U))
#define pmasys_channel_outsize_numbytes_f(v)          (((v) & 0x7ffffffU) << 5U)
#define pmasys_channel_outsize_numbytes_m()                   (0x7ffffffU << 5U)
#define pmasys_channel_mem_head_r(i)\
		(0x0f14a650U + ((i)*4U))
#define pmasys_channel_mem_bytes_addr_r(i)\
		(0x0f14a658U + ((i)*4U))
#define pmasys_channel_mem_bytes_addr_ptr_f(v)       (((v) & 0x3fffffffU) << 2U)
#define pmasys_channel_mem_bytes_addr_ptr_m()                (0x3fffffffU << 2U)
#define pmasys_trigger_config_user_r(i)\
		(0x0f14a694U + ((i)*384U))
#define pmasys_trigger_config_user_pma_pulse_f(v)           (((v) & 0x1U) << 0U)
#define pmasys_trigger_config_user_pma_pulse_m()                    (0x1U << 0U)
#define pmasys_trigger_config_user_pma_pulse_disable_v()           (0x00000000U)
#define pmasys_trigger_config_user_pma_pulse_disable_f()                  (0x0U)
#define pmasys_trigger_config_user_pma_pulse_enable_v()            (0x00000001U)
#define pmasys_trigger_config_user_record_stream_f(v)       (((v) & 0x1U) << 6U)
#define pmasys_trigger_config_user_record_stream_m()                (0x1U << 6U)
#define pmasys_trigger_config_user_record_stream_disable_v()       (0x00000000U)
#define pmasys_trigger_config_user_record_stream_disable_f()              (0x0U)
#define pmasys_trigger_config_user_record_stream_enable_v()        (0x00000001U)
#define pmasys_enginestatus_r()                                    (0x0f14a75cU)
#define pmasys_enginestatus_status_s()                                      (3U)
#define pmasys_enginestatus_status_f(v)                     (((v) & 0x7U) << 0U)
#define pmasys_enginestatus_status_m()                              (0x7U << 0U)
#define pmasys_enginestatus_status_v(r)                     (((r) >> 0U) & 0x7U)
#define pmasys_enginestatus_status_w()                                      (0U)
#define pmasys_enginestatus_status_empty_v()                       (0x00000000U)
#define pmasys_enginestatus_status_empty_f()                              (0x0U)
#define pmasys_enginestatus_status_active_v()                      (0x00000001U)
#define pmasys_enginestatus_status_paused_v()                      (0x00000002U)
#define pmasys_enginestatus_status_quiescent_v()                   (0x00000003U)
#define pmasys_enginestatus_status_stalled_v()                     (0x00000005U)
#define pmasys_enginestatus_status_faulted_v()                     (0x00000006U)
#define pmasys_enginestatus_status_halted_v()                      (0x00000007U)
#define pmasys_enginestatus_rbufempty_s()                                   (1U)
#define pmasys_enginestatus_rbufempty_f(v)                  (((v) & 0x1U) << 4U)
#define pmasys_enginestatus_rbufempty_m()                           (0x1U << 4U)
#define pmasys_enginestatus_rbufempty_v(r)                  (((r) >> 4U) & 0x1U)
#define pmasys_enginestatus_rbufempty_w()                                   (0U)
#define pmasys_enginestatus_rbufempty_empty_v()                    (0x00000001U)
#define pmasys_enginestatus_rbufempty_empty_f()                          (0x10U)
#define pmasys_enginestatus_mbu_status_f(v)                 (((v) & 0x3U) << 5U)
#define pmasys_enginestatus_mbu_status_m()                          (0x3U << 5U)
#define pmasys_enginestatus_mbu_status_idle_v()                    (0x00000000U)
#define pmasys_enginestatus_mbu_status_busy_v()                    (0x00000001U)
#define pmasys_enginestatus_mbu_status_pending_v()                 (0x00000002U)
#define pmasys_sys_trigger_start_mask_r()                          (0x0f14a66cU)
#define pmasys_sys_trigger_start_maskb_r()                         (0x0f14a670U)
#define pmasys_sys_trigger_stop_mask_r()                           (0x0f14a684U)
#define pmasys_sys_trigger_stop_maskb_r()                          (0x0f14a688U)
#endif
