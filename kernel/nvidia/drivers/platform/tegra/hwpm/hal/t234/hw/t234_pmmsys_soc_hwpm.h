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
#ifndef T234_PMMSYS_SOC_HWPM_H
#define T234_PMMSYS_SOC_HWPM_H

#define pmmsys_perdomain_offset_v()                                (0x00001000U)
#define pmmsys_control_r(i)\
		(0x0f10009cU + ((i)*4096U))
#define pmmsys_control_mode_f(v)                            (((v) & 0x7U) << 0U)
#define pmmsys_control_mode_m()                                     (0x7U << 0U)
#define pmmsys_control_mode_disable_v()                            (0x00000000U)
#define pmmsys_control_mode_disable_f()                                   (0x0U)
#define pmmsys_control_mode_a_v()                                  (0x00000001U)
#define pmmsys_control_mode_b_v()                                  (0x00000002U)
#define pmmsys_control_mode_c_v()                                  (0x00000003U)
#define pmmsys_control_mode_e_v()                                  (0x00000005U)
#define pmmsys_control_mode_null_v()                               (0x00000007U)
#define pmmsys_sys0_enginestatus_r(i)\
		(0x0f1000c8U + ((i)*4096U))
#define pmmsys_sys0_enginestatus_enable_f(v)                (((v) & 0x1U) << 8U)
#define pmmsys_sys0_enginestatus_enable_m()                         (0x1U << 8U)
#define pmmsys_sys0_enginestatus_enable_masked_v()                 (0x00000000U)
#define pmmsys_sys0_enginestatus_enable_out_v()                    (0x00000001U)
#define pmmsys_sys0_enginestatus_enable_out_f()                         (0x100U)
#define pmmsys_sys0router_enginestatus_r()                         (0x0f14d010U)
#define pmmsys_sys0router_enginestatus_status_f(v)          (((v) & 0x7U) << 0U)
#define pmmsys_sys0router_enginestatus_status_m()                   (0x7U << 0U)
#define pmmsys_sys0router_enginestatus_status_v(r)          (((r) >> 0U) & 0x7U)
#define pmmsys_sys0router_enginestatus_status_empty_v()            (0x00000000U)
#define pmmsys_sys0router_enginestatus_status_active_v()           (0x00000001U)
#define pmmsys_sys0router_enginestatus_status_paused_v()           (0x00000002U)
#define pmmsys_sys0router_enginestatus_status_quiescent_v()        (0x00000003U)
#define pmmsys_sys0router_enginestatus_status_stalled_v()          (0x00000005U)
#define pmmsys_sys0router_enginestatus_status_faulted_v()          (0x00000006U)
#define pmmsys_sys0router_enginestatus_status_halted_v()           (0x00000007U)
#define pmmsys_sys0router_enginestatus_enable_f(v)          (((v) & 0x1U) << 8U)
#define pmmsys_sys0router_enginestatus_enable_m()                   (0x1U << 8U)
#define pmmsys_sys0router_enginestatus_enable_masked_v()           (0x00000000U)
#define pmmsys_sys0router_enginestatus_enable_out_v()              (0x00000001U)
#define pmmsys_sys0router_perfmonstatus_r()                        (0x0f14d014U)
#define pmmsys_sys0router_perfmonstatus_merged_f(v)         (((v) & 0x7U) << 0U)
#define pmmsys_sys0router_perfmonstatus_merged_m()                  (0x7U << 0U)
#define pmmsys_sys0router_perfmonstatus_merged_v(r)         (((r) >> 0U) & 0x7U)
#define pmmsys_sys0router_cg2_r()                                  (0x0f14d018U)
#define pmmsys_sys0router_cg2_slcg_perfmon_m()                      (0x1U << 0U)
#define pmmsys_sys0router_cg2_slcg_perfmon_disabled_v()            (0x00000001U)
#define pmmsys_sys0router_cg2_slcg_perfmon_disabled_f()                   (0x1U)
#define pmmsys_sys0router_cg2_slcg_perfmon__prod_v()               (0x00000000U)
#define pmmsys_sys0router_cg2_slcg_perfmon__prod_f()                      (0x0U)
#define pmmsys_sys0router_cg2_slcg_router_m()                       (0x1U << 1U)
#define pmmsys_sys0router_cg2_slcg_router_disabled_v()             (0x00000001U)
#define pmmsys_sys0router_cg2_slcg_router_disabled_f()                    (0x2U)
#define pmmsys_sys0router_cg2_slcg_router__prod_v()                (0x00000000U)
#define pmmsys_sys0router_cg2_slcg_router__prod_f()                       (0x0U)
#define pmmsys_sys0router_cg2_slcg_m()                              (0x3U << 0U)
#define pmmsys_sys0router_cg2_slcg_disabled_v()                    (0x00000003U)
#define pmmsys_sys0router_cg2_slcg_disabled_f()                           (0x3U)
#define pmmsys_sys0router_cg2_slcg__prod_v()                       (0x00000000U)
#define pmmsys_sys0router_cg2_slcg__prod_f()                              (0x0U)
#endif
