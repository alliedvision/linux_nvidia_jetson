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
#ifndef NVGPU_HW_FUSE_GA10B_H
#define NVGPU_HW_FUSE_GA10B_H

#include <nvgpu/types.h>
#include <nvgpu/static_analysis.h>

#define fuse_status_opt_gpc_r()                                    (0x00820c1cU)
#define fuse_status_opt_tpc_gpc_r(i)\
		(nvgpu_safe_add_u32(0x00820c38U, nvgpu_safe_mult_u32((i), 4U)))
#define fuse_ctrl_opt_tpc_gpc_r(i)\
		(nvgpu_safe_add_u32(0x00820838U, nvgpu_safe_mult_u32((i), 4U)))
#define fuse_ctrl_opt_ltc_fbp_r(i)\
		(nvgpu_safe_add_u32(0x00820970U, nvgpu_safe_mult_u32((i), 4U)))
#define fuse_status_opt_pes_gpc_r(i)\
		(nvgpu_safe_add_u32(0x00820dbcU, nvgpu_safe_mult_u32((i), 4U)))
#define fuse_status_opt_rop_gpc_r(i)\
		(nvgpu_safe_add_u32(0x00822880U, nvgpu_safe_mult_u32((i), 4U)))
#define fuse_ctrl_opt_gpc_r()                                      (0x0082081cU)
#define fuse_ctrl_opt_fbp_r()                                      (0x00820938U)
#define fuse_status_opt_fbio_r()                                   (0x00820c14U)
#define fuse_status_opt_fbp_r()                                    (0x00820d38U)
#define fuse_opt_ecc_en_r()                                        (0x00820228U)
#define fuse_opt_feature_fuses_override_disable_r()                (0x008203f0U)
#define fuse_opt_vpr_enabled_r()                                   (0x0082067cU)
#define fuse_opt_vpr_enabled_data_v(r)                      (((r) >> 0U) & 0x1U)
#define fuse_opt_vpr_auto_fetch_disable_r()                        (0x00820684U)
#define fuse_opt_vpr_auto_fetch_disable_data_v(r)           (((r) >> 0U) & 0x1U)
#define fuse_opt_wpr_enabled_r()                                   (0x008205ecU)
#define fuse_opt_wpr_enabled_data_v(r)                      (((r) >> 0U) & 0x1U)
#define fuse_opt_sec_debug_en_r()                                  (0x00821040U)
#define fuse_opt_priv_sec_en_r()                                   (0x00820434U)
#define fuse_opt_sm_ttu_en_r()                                     (0x00820168U)
#define fuse_opt_secure_source_isolation_en_r()                    (0x00820410U)
#define fuse_feature_override_ecc_r()                              (0x0082380cU)
#define fuse_feature_override_ecc_sm_lrf_v(r)               (((r) >> 0U) & 0x1U)
#define fuse_feature_override_ecc_sm_lrf_enabled_v()               (0x00000001U)
#define fuse_feature_override_ecc_sm_lrf_override_v(r)      (((r) >> 3U) & 0x1U)
#define fuse_feature_override_ecc_sm_lrf_override_true_v()         (0x00000001U)
#define fuse_feature_override_ecc_sm_l1_data_v(r)           (((r) >> 4U) & 0x1U)
#define fuse_feature_override_ecc_sm_l1_data_enabled_v()           (0x00000001U)
#define fuse_feature_override_ecc_sm_l1_data_override_v(r)  (((r) >> 7U) & 0x1U)
#define fuse_feature_override_ecc_sm_l1_data_override_true_v()     (0x00000001U)
#define fuse_feature_override_ecc_sm_l1_tag_v(r)            (((r) >> 8U) & 0x1U)
#define fuse_feature_override_ecc_sm_l1_tag_enabled_v()            (0x00000001U)
#define fuse_feature_override_ecc_sm_l1_tag_override_v(r)  (((r) >> 11U) & 0x1U)
#define fuse_feature_override_ecc_sm_l1_tag_override_true_v()      (0x00000001U)
#define fuse_feature_override_ecc_ltc_v(r)                 (((r) >> 12U) & 0x1U)
#define fuse_feature_override_ecc_ltc_enabled_v()                  (0x00000001U)
#define fuse_feature_override_ecc_ltc_override_v(r)        (((r) >> 15U) & 0x1U)
#define fuse_feature_override_ecc_ltc_override_true_v()            (0x00000001U)
#define fuse_feature_override_ecc_dram_v(r)                (((r) >> 16U) & 0x1U)
#define fuse_feature_override_ecc_dram_enabled_v()                 (0x00000001U)
#define fuse_feature_override_ecc_dram_override_v(r)       (((r) >> 19U) & 0x1U)
#define fuse_feature_override_ecc_dram_override_true_v()           (0x00000001U)
#define fuse_feature_override_ecc_sm_cbu_v(r)              (((r) >> 20U) & 0x1U)
#define fuse_feature_override_ecc_sm_cbu_enabled_v()               (0x00000001U)
#define fuse_feature_override_ecc_sm_cbu_override_v(r)     (((r) >> 23U) & 0x1U)
#define fuse_feature_override_ecc_sm_cbu_override_true_v()         (0x00000001U)
#define fuse_feature_override_ecc_1_r()                            (0x00823810U)
#define fuse_feature_override_ecc_1_sm_l0_icache_v(r)       (((r) >> 0U) & 0x1U)
#define fuse_feature_override_ecc_1_sm_l0_icache_enabled_v()       (0x00000001U)
#define fuse_feature_override_ecc_1_sm_l0_icache_override_v(r)\
				(((r) >> 1U) & 0x1U)
#define fuse_feature_override_ecc_1_sm_l0_icache_override_true_v() (0x00000001U)
#define fuse_feature_override_ecc_1_sm_l1_icache_v(r)       (((r) >> 2U) & 0x1U)
#define fuse_feature_override_ecc_1_sm_l1_icache_enabled_v()       (0x00000001U)
#define fuse_feature_override_ecc_1_sm_l1_icache_override_v(r)\
				(((r) >> 3U) & 0x1U)
#define fuse_feature_override_ecc_1_sm_l1_icache_override_true_v() (0x00000001U)
#define fuse_p2prx_pdi_r()                                         (0x00823000U)
#define fuse_p2prx_pdi_loaded_v(r)                          (((r) >> 1U) & 0x1U)
#define fuse_p2prx_pdi_loaded_true_v()                             (0x00000001U)
#define fuse_opt_pdi_0_r()                                         (0x00820344U)
#define fuse_opt_pdi_1_r()                                         (0x00820348U)
#define fuse_pmu_fcd_r()                                           (0x00820b18U)
#define fuse_gsp_fcd_r()                                           (0x00820b1cU)
#define fuse_pmu_dcs_r()                                           (0x00820af8U)
#define fuse_gsp_dcs_r()                                           (0x00820afcU)
#define fuse_pmu_enen_r()                                          (0x00820a24U)
#define fuse_gsp_enen_r()                                          (0x00820a28U)
#define fuse_pmu_nvriscv_bre_en_r()                                (0x00820b00U)
#define fuse_gsp_nvriscv_bre_en_r()                                (0x00820b14U)
#define fuse_pmu_nvriscv_devd_r()                                  (0x00820b70U)
#define fuse_gsp_nvriscv_devd_r()                                  (0x00820b10U)
#define fuse_pmu_nvriscv_pld_r()                                   (0x00820b08U)
#define fuse_gsp_nvriscv_pld_r()                                   (0x00820b0cU)
#define fuse_pmu_nvriscv_sen_r()                                   (0x00820b04U)
#define fuse_gsp_nvriscv_sen_r()                                   (0x00820a0cU)
#define fuse_pmu_nvriscv_sa_r()                                    (0x00820a04U)
#define fuse_gsp_nvriscv_sa_r()                                    (0x00820a14U)
#define fuse_pmu_nvriscv_sh_r()                                    (0x00820a00U)
#define fuse_gsp_nvriscv_sh_r()                                    (0x00820a10U)
#define fuse_pmu_nvriscv_si_r()                                    (0x00820a08U)
#define fuse_gsp_nvriscv_si_r()                                    (0x00820a18U)
#define fuse_secure_pmu_dbgd_r()                                   (0x00820640U)
#define fuse_secure_gsp_dbgd_r()                                   (0x0082074cU)
#define fuse_pkc_pmu_algo_dis_r()                                  (0x0082073cU)
#define fuse_pkc_gsp_algo_dis_r()                                  (0x00820754U)
#endif
