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
#ifndef T234_ADDR_MAP_SOC_HWPM_H
#define T234_ADDR_MAP_SOC_HWPM_H

#define addr_map_rpg_pm_base_r()                                   (0x0f100000U)
#define addr_map_rpg_pm_limit_r()                                  (0x0f149fffU)
#define addr_map_rpg_pm_pma_base_r()                               (0x0f10b000U)
#define addr_map_rpg_pm_pma_limit_r()                              (0x0f10bfffU)
#define addr_map_pma_base_r()                                      (0x0f14a000U)
#define addr_map_pma_limit_r()                                     (0x0f14bfffU)
#define addr_map_rtr_base_r()                                      (0x0f14d000U)
#define addr_map_rtr_limit_r()                                     (0x0f14dfffU)
#define addr_map_rpg_pm_disp_base_r()                              (0x0f10a000U)
#define addr_map_rpg_pm_disp_limit_r()                             (0x0f10afffU)
#define addr_map_disp_base_r()                                     (0x13800000U)
#define addr_map_disp_limit_r()                                    (0x138effffU)
#define addr_map_rpg_pm_vi0_base_r()                               (0x0f100000U)
#define addr_map_rpg_pm_vi0_limit_r()                              (0x0f100fffU)
#define addr_map_rpg_pm_vi1_base_r()                               (0x0f101000U)
#define addr_map_rpg_pm_vi1_limit_r()                              (0x0f101fffU)
#define addr_map_vi_thi_base_r()                                   (0x15f00000U)
#define addr_map_vi_thi_limit_r()                                  (0x15ffffffU)
#define addr_map_vi2_thi_base_r()                                  (0x14f00000U)
#define addr_map_vi2_thi_limit_r()                                 (0x14ffffffU)
#define addr_map_rpg_pm_vic_base_r()                               (0x0f103000U)
#define addr_map_rpg_pm_vic_limit_r()                              (0x0f103fffU)
#define addr_map_vic_base_r()                                      (0x15340000U)
#define addr_map_vic_limit_r()                                     (0x1537ffffU)
#define addr_map_rpg_pm_nvdec0_base_r()                            (0x0f111000U)
#define addr_map_rpg_pm_nvdec0_limit_r()                           (0x0f111fffU)
#define addr_map_nvdec_base_r()                                    (0x15480000U)
#define addr_map_nvdec_limit_r()                                   (0x154bffffU)
#define addr_map_rpg_pm_nvenc0_base_r()                            (0x0f112000U)
#define addr_map_rpg_pm_nvenc0_limit_r()                           (0x0f112fffU)
#define addr_map_nvenc_base_r()                                    (0x154c0000U)
#define addr_map_nvenc_limit_r()                                   (0x154fffffU)
#define addr_map_rpg_pm_ofa_base_r()                               (0x0f104000U)
#define addr_map_rpg_pm_ofa_limit_r()                              (0x0f104fffU)
#define addr_map_ofa_base_r()                                      (0x15a50000U)
#define addr_map_ofa_limit_r()                                     (0x15a5ffffU)
#define addr_map_rpg_pm_isp0_base_r()                              (0x0f102000U)
#define addr_map_rpg_pm_isp0_limit_r()                             (0x0f102fffU)
#define addr_map_isp_thi_base_r()                                  (0x14b00000U)
#define addr_map_isp_thi_limit_r()                                 (0x14bfffffU)
#define addr_map_rpg_pm_pcie_c0_base_r()                           (0x0f114000U)
#define addr_map_rpg_pm_pcie_c0_limit_r()                          (0x0f114fffU)
#define addr_map_rpg_pm_pcie_c1_base_r()                           (0x0f115000U)
#define addr_map_rpg_pm_pcie_c1_limit_r()                          (0x0f115fffU)
#define addr_map_rpg_pm_pcie_c2_base_r()                           (0x0f116000U)
#define addr_map_rpg_pm_pcie_c2_limit_r()                          (0x0f116fffU)
#define addr_map_rpg_pm_pcie_c3_base_r()                           (0x0f117000U)
#define addr_map_rpg_pm_pcie_c3_limit_r()                          (0x0f117fffU)
#define addr_map_rpg_pm_pcie_c4_base_r()                           (0x0f118000U)
#define addr_map_rpg_pm_pcie_c4_limit_r()                          (0x0f118fffU)
#define addr_map_rpg_pm_pcie_c5_base_r()                           (0x0f119000U)
#define addr_map_rpg_pm_pcie_c5_limit_r()                          (0x0f119fffU)
#define addr_map_rpg_pm_pcie_c6_base_r()                           (0x0f11a000U)
#define addr_map_rpg_pm_pcie_c6_limit_r()                          (0x0f11afffU)
#define addr_map_rpg_pm_pcie_c7_base_r()                           (0x0f11b000U)
#define addr_map_rpg_pm_pcie_c7_limit_r()                          (0x0f11bfffU)
#define addr_map_rpg_pm_pcie_c8_base_r()                           (0x0f11c000U)
#define addr_map_rpg_pm_pcie_c8_limit_r()                          (0x0f11cfffU)
#define addr_map_rpg_pm_pcie_c9_base_r()                           (0x0f11d000U)
#define addr_map_rpg_pm_pcie_c9_limit_r()                          (0x0f11dfffU)
#define addr_map_rpg_pm_pcie_c10_base_r()                          (0x0f11e000U)
#define addr_map_rpg_pm_pcie_c10_limit_r()                         (0x0f11efffU)
#define addr_map_pcie_c0_ctl_base_r()                              (0x14180000U)
#define addr_map_pcie_c0_ctl_limit_r()                             (0x1419ffffU)
#define addr_map_pcie_c1_ctl_base_r()                              (0x14100000U)
#define addr_map_pcie_c1_ctl_limit_r()                             (0x1411ffffU)
#define addr_map_pcie_c2_ctl_base_r()                              (0x14120000U)
#define addr_map_pcie_c2_ctl_limit_r()                             (0x1413ffffU)
#define addr_map_pcie_c3_ctl_base_r()                              (0x14140000U)
#define addr_map_pcie_c3_ctl_limit_r()                             (0x1415ffffU)
#define addr_map_pcie_c4_ctl_base_r()                              (0x14160000U)
#define addr_map_pcie_c4_ctl_limit_r()                             (0x1417ffffU)
#define addr_map_pcie_c5_ctl_base_r()                              (0x141a0000U)
#define addr_map_pcie_c5_ctl_limit_r()                             (0x141bffffU)
#define addr_map_pcie_c6_ctl_base_r()                              (0x141c0000U)
#define addr_map_pcie_c6_ctl_limit_r()                             (0x141dffffU)
#define addr_map_pcie_c7_ctl_base_r()                              (0x141e0000U)
#define addr_map_pcie_c7_ctl_limit_r()                             (0x141fffffU)
#define addr_map_pcie_c8_ctl_base_r()                              (0x140a0000U)
#define addr_map_pcie_c8_ctl_limit_r()                             (0x140bffffU)
#define addr_map_pcie_c9_ctl_base_r()                              (0x140c0000U)
#define addr_map_pcie_c9_ctl_limit_r()                             (0x140dffffU)
#define addr_map_pcie_c10_ctl_base_r()                             (0x140e0000U)
#define addr_map_pcie_c10_ctl_limit_r()                            (0x140fffffU)
#define addr_map_rpg_pm_pva0_0_base_r()                            (0x0f105000U)
#define addr_map_rpg_pm_pva0_0_limit_r()                           (0x0f105fffU)
#define addr_map_rpg_pm_pva0_1_base_r()                            (0x0f106000U)
#define addr_map_rpg_pm_pva0_1_limit_r()                           (0x0f106fffU)
#define addr_map_rpg_pm_pva0_2_base_r()                            (0x0f107000U)
#define addr_map_rpg_pm_pva0_2_limit_r()                           (0x0f107fffU)
#define addr_map_pva0_pm_base_r()                                  (0x16200000U)
#define addr_map_pva0_pm_limit_r()                                 (0x1620ffffU)
#define addr_map_rpg_pm_nvdla0_base_r()                            (0x0f108000U)
#define addr_map_rpg_pm_nvdla0_limit_r()                           (0x0f108fffU)
#define addr_map_rpg_pm_nvdla1_base_r()                            (0x0f109000U)
#define addr_map_rpg_pm_nvdla1_limit_r()                           (0x0f109fffU)
#define addr_map_nvdla0_base_r()                                   (0x15880000U)
#define addr_map_nvdla0_limit_r()                                  (0x158bffffU)
#define addr_map_nvdla1_base_r()                                   (0x158c0000U)
#define addr_map_nvdla1_limit_r()                                  (0x158fffffU)
#define addr_map_rpg_pm_mgbe0_base_r()                             (0x0f10c000U)
#define addr_map_rpg_pm_mgbe0_limit_r()                            (0x0f10cfffU)
#define addr_map_rpg_pm_mgbe1_base_r()                             (0x0f10d000U)
#define addr_map_rpg_pm_mgbe1_limit_r()                            (0x0f10dfffU)
#define addr_map_rpg_pm_mgbe2_base_r()                             (0x0f10e000U)
#define addr_map_rpg_pm_mgbe2_limit_r()                            (0x0f10efffU)
#define addr_map_rpg_pm_mgbe3_base_r()                             (0x0f10f000U)
#define addr_map_rpg_pm_mgbe3_limit_r()                            (0x0f10ffffU)
#define addr_map_mgbe0_mac_rm_base_r()                             (0x06810000U)
#define addr_map_mgbe0_mac_rm_limit_r()                            (0x0681ffffU)
#define addr_map_mgbe1_mac_rm_base_r()                             (0x06910000U)
#define addr_map_mgbe1_mac_rm_limit_r()                            (0x0691ffffU)
#define addr_map_mgbe2_mac_rm_base_r()                             (0x06a10000U)
#define addr_map_mgbe2_mac_rm_limit_r()                            (0x06a1ffffU)
#define addr_map_mgbe3_mac_rm_base_r()                             (0x06b10000U)
#define addr_map_mgbe3_mac_rm_limit_r()                            (0x06b1ffffU)
#define addr_map_rpg_pm_mss0_base_r()                              (0x0f11f000U)
#define addr_map_rpg_pm_mss0_limit_r()                             (0x0f11ffffU)
#define addr_map_rpg_pm_mss1_base_r()                              (0x0f120000U)
#define addr_map_rpg_pm_mss1_limit_r()                             (0x0f120fffU)
#define addr_map_rpg_pm_mss2_base_r()                              (0x0f121000U)
#define addr_map_rpg_pm_mss2_limit_r()                             (0x0f121fffU)
#define addr_map_rpg_pm_mss3_base_r()                              (0x0f122000U)
#define addr_map_rpg_pm_mss3_limit_r()                             (0x0f122fffU)
#define addr_map_rpg_pm_mss4_base_r()                              (0x0f123000U)
#define addr_map_rpg_pm_mss4_limit_r()                             (0x0f123fffU)
#define addr_map_rpg_pm_mss5_base_r()                              (0x0f124000U)
#define addr_map_rpg_pm_mss5_limit_r()                             (0x0f124fffU)
#define addr_map_rpg_pm_mss6_base_r()                              (0x0f125000U)
#define addr_map_rpg_pm_mss6_limit_r()                             (0x0f125fffU)
#define addr_map_rpg_pm_mss7_base_r()                              (0x0f126000U)
#define addr_map_rpg_pm_mss7_limit_r()                             (0x0f126fffU)
#define addr_map_rpg_pm_mss8_base_r()                              (0x0f127000U)
#define addr_map_rpg_pm_mss8_limit_r()                             (0x0f127fffU)
#define addr_map_rpg_pm_mss9_base_r()                              (0x0f128000U)
#define addr_map_rpg_pm_mss9_limit_r()                             (0x0f128fffU)
#define addr_map_rpg_pm_mss10_base_r()                             (0x0f129000U)
#define addr_map_rpg_pm_mss10_limit_r()                            (0x0f129fffU)
#define addr_map_rpg_pm_mss11_base_r()                             (0x0f12a000U)
#define addr_map_rpg_pm_mss11_limit_r()                            (0x0f12afffU)
#define addr_map_rpg_pm_mss12_base_r()                             (0x0f12b000U)
#define addr_map_rpg_pm_mss12_limit_r()                            (0x0f12bfffU)
#define addr_map_rpg_pm_mss13_base_r()                             (0x0f12c000U)
#define addr_map_rpg_pm_mss13_limit_r()                            (0x0f12cfffU)
#define addr_map_rpg_pm_mss14_base_r()                             (0x0f12d000U)
#define addr_map_rpg_pm_mss14_limit_r()                            (0x0f12dfffU)
#define addr_map_rpg_pm_mss15_base_r()                             (0x0f12e000U)
#define addr_map_rpg_pm_mss15_limit_r()                            (0x0f12efffU)
#define addr_map_mc0_base_r()                                      (0x02c20000U)
#define addr_map_mc0_limit_r()                                     (0x02c2ffffU)
#define addr_map_mc1_base_r()                                      (0x02c30000U)
#define addr_map_mc1_limit_r()                                     (0x02c3ffffU)
#define addr_map_mc2_base_r()                                      (0x02c40000U)
#define addr_map_mc2_limit_r()                                     (0x02c4ffffU)
#define addr_map_mc3_base_r()                                      (0x02c50000U)
#define addr_map_mc3_limit_r()                                     (0x02c5ffffU)
#define addr_map_mc4_base_r()                                      (0x02b80000U)
#define addr_map_mc4_limit_r()                                     (0x02b8ffffU)
#define addr_map_mc5_base_r()                                      (0x02b90000U)
#define addr_map_mc5_limit_r()                                     (0x02b9ffffU)
#define addr_map_mc6_base_r()                                      (0x02ba0000U)
#define addr_map_mc6_limit_r()                                     (0x02baffffU)
#define addr_map_mc7_base_r()                                      (0x02bb0000U)
#define addr_map_mc7_limit_r()                                     (0x02bbffffU)
#define addr_map_mc8_base_r()                                      (0x01700000U)
#define addr_map_mc8_limit_r()                                     (0x0170ffffU)
#define addr_map_mc9_base_r()                                      (0x01710000U)
#define addr_map_mc9_limit_r()                                     (0x0171ffffU)
#define addr_map_mc10_base_r()                                     (0x01720000U)
#define addr_map_mc10_limit_r()                                    (0x0172ffffU)
#define addr_map_mc11_base_r()                                     (0x01730000U)
#define addr_map_mc11_limit_r()                                    (0x0173ffffU)
#define addr_map_mc12_base_r()                                     (0x01740000U)
#define addr_map_mc12_limit_r()                                    (0x0174ffffU)
#define addr_map_mc13_base_r()                                     (0x01750000U)
#define addr_map_mc13_limit_r()                                    (0x0175ffffU)
#define addr_map_mc14_base_r()                                     (0x01760000U)
#define addr_map_mc14_limit_r()                                    (0x0176ffffU)
#define addr_map_mc15_base_r()                                     (0x01770000U)
#define addr_map_mc15_limit_r()                                    (0x0177ffffU)
#define addr_map_mcb_base_r()                                      (0x02c10000U)
#define addr_map_mcb_limit_r()                                     (0x02c1ffffU)
#define addr_map_rpg_pm_msshub0_base_r()                           (0x0f12f000U)
#define addr_map_rpg_pm_msshub0_limit_r()                          (0x0f12ffffU)
#define addr_map_rpg_pm_msshub1_base_r()                           (0x0f130000U)
#define addr_map_rpg_pm_msshub1_limit_r()                          (0x0f130fffU)
#define addr_map_rpg_pm_mcf0_base_r()                              (0x0f131000U)
#define addr_map_rpg_pm_mcf0_limit_r()                             (0x0f131fffU)
#define addr_map_rpg_pm_mcf1_base_r()                              (0x0f132000U)
#define addr_map_rpg_pm_mcf1_limit_r()                             (0x0f132fffU)
#define addr_map_rpg_pm_mcf2_base_r()                              (0x0f133000U)
#define addr_map_rpg_pm_mcf2_limit_r()                             (0x0f133fffU)
#define addr_map_rpg_pm_mssnvl_base_r()                            (0x0f113000U)
#define addr_map_rpg_pm_mssnvl_limit_r()                           (0x0f113fffU)
#define addr_map_mss_nvlink_1_base_r()                             (0x01f20000U)
#define addr_map_mss_nvlink_1_limit_r()                            (0x01f3ffffU)
#define addr_map_mss_nvlink_2_base_r()                             (0x01f40000U)
#define addr_map_mss_nvlink_2_limit_r()                            (0x01f5ffffU)
#define addr_map_mss_nvlink_3_base_r()                             (0x01f60000U)
#define addr_map_mss_nvlink_3_limit_r()                            (0x01f7ffffU)
#define addr_map_mss_nvlink_4_base_r()                             (0x01f80000U)
#define addr_map_mss_nvlink_4_limit_r()                            (0x01f9ffffU)
#define addr_map_mss_nvlink_5_base_r()                             (0x01fa0000U)
#define addr_map_mss_nvlink_5_limit_r()                            (0x01fbffffU)
#define addr_map_mss_nvlink_6_base_r()                             (0x01fc0000U)
#define addr_map_mss_nvlink_6_limit_r()                            (0x01fdffffU)
#define addr_map_mss_nvlink_7_base_r()                             (0x01fe0000U)
#define addr_map_mss_nvlink_7_limit_r()                            (0x01ffffffU)
#define addr_map_mss_nvlink_8_base_r()                             (0x01e00000U)
#define addr_map_mss_nvlink_8_limit_r()                            (0x01e1ffffU)
#define addr_map_rpg_pm_scf_base_r()                               (0x0f110000U)
#define addr_map_rpg_pm_scf_limit_r()                              (0x0f110fffU)
#define addr_map_pmc_misc_base_r()                                 (0x0c3a0000U)
#endif
