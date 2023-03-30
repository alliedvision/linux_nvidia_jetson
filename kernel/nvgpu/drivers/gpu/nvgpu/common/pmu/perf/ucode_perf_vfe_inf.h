/*
 * Copyright (c) 2016-2020, NVIDIA CORPORATION.  All rights reserved.
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
#ifndef NVGPU_PMUIF_PERFVFE_H
#define NVGPU_PMUIF_PERFVFE_H

#define NV_PMU_PERF_RPC_VFE_EQU_EVAL_VAR_COUNT_MAX                   2U
#define NV_PMU_VFE_VAR_SINGLE_SENSED_FUSE_SEGMENTS_MAX               1U

#define CTRL_PERF_VFE_VAR_TYPE_INVALID                               0x00U
#define CTRL_PERF_VFE_VAR_TYPE_DERIVED                               0x01U
#define CTRL_PERF_VFE_VAR_TYPE_DERIVED_PRODUCT                       0x02U
#define CTRL_PERF_VFE_VAR_TYPE_DERIVED_SUM                           0x03U
#define CTRL_PERF_VFE_VAR_TYPE_SINGLE                                0x04U
#define CTRL_PERF_VFE_VAR_TYPE_SINGLE_FREQUENCY                      0x05U
#define CTRL_PERF_VFE_VAR_TYPE_SINGLE_SENSED                         0x06U
#define CTRL_PERF_VFE_VAR_TYPE_SINGLE_SENSED_FUSE                    0x07U
#define CTRL_PERF_VFE_VAR_TYPE_SINGLE_SENSED_TEMP                    0x08U
#define CTRL_PERF_VFE_VAR_TYPE_SINGLE_VOLTAGE                        0x09U
#define CTRL_PERF_VFE_VAR_TYPE_SINGLE_CALLER_SPECIFIED               0x0AU

#define CTRL_PERF_VFE_VAR_SINGLE_OVERRIDE_TYPE_NONE                  0x00U
#define CTRL_PERF_VFE_VAR_SINGLE_OVERRIDE_TYPE_VALUE                 0x01U
#define CTRL_PERF_VFE_VAR_SINGLE_OVERRIDE_TYPE_OFFSET                0x02U
#define CTRL_PERF_VFE_VAR_SINGLE_OVERRIDE_TYPE_SCALE                 0x03U

#define CTRL_PERF_VFE_EQU_TYPE_INVALID                               0x00U
#define CTRL_PERF_VFE_EQU_TYPE_COMPARE                               0x01U
#define CTRL_PERF_VFE_EQU_TYPE_MINMAX                                0x02U
#define CTRL_PERF_VFE_EQU_TYPE_QUADRATIC                             0x03U
#define CTRL_PERF_VFE_EQU_TYPE_SCALAR                                0x04U

#define CTRL_PERF_VFE_EQU_OUTPUT_TYPE_UNITLESS                       0x00U
#define CTRL_PERF_VFE_EQU_OUTPUT_TYPE_FREQ_MHZ                       0x01U
#define CTRL_PERF_VFE_EQU_OUTPUT_TYPE_VOLT_UV                        0x02U
#define CTRL_PERF_VFE_EQU_OUTPUT_TYPE_VF_GAIN                        0x03U
#define CTRL_PERF_VFE_EQU_OUTPUT_TYPE_VOLT_DELTA_UV                  0x04U
#define CTRL_PERF_VFE_EQU_OUTPUT_TYPE_WORK_TYPE                      0x06U
#define CTRL_PERF_VFE_EQU_OUTPUT_TYPE_UTIL_RATIO                     0x07U
#define CTRL_PERF_VFE_EQU_OUTPUT_TYPE_WORK_FB_NORM                   0x08U
#define CTRL_PERF_VFE_EQU_OUTPUT_TYPE_POWER_MW                       0x09U
#define CTRL_PERF_VFE_EQU_OUTPUT_TYPE_PWR_OVER_UTIL_SLOPE            0x0AU
#define CTRL_PERF_VFE_EQU_OUTPUT_TYPE_VIN_CODE                       0x0BU

#define CTRL_PERF_VFE_EQU_QUADRATIC_COEFF_COUNT                      0x03U

#define CTRL_PERF_VFE_EQU_COMPARE_FUNCTION_EQUAL                     0x00U
#define CTRL_PERF_VFE_EQU_COMPARE_FUNCTION_GREATER_EQ                0x01U
#define CTRL_PERF_VFE_EQU_COMPARE_FUNCTION_GREATER                   0x02U

union nv_pmu_perf_vfe_var_type_data {
	u8 uid;
	u8 clk_domain_idx;
};

struct nv_pmu_perf_vfe_var_value {
	u8 var_type;
	union nv_pmu_perf_vfe_var_type_data var_type_data;
	u8 reserved[2];
	u32 var_value;
};

union nv_pmu_perf_vfe_equ_result {
	u32 freq_m_hz;
	u32 voltu_v;
	u32 vf_gain;
	int volt_deltau_v;
	u32 work_type;
	u32 util_ratio;
	u32 work_fb_norm;
	u32 power_mw;
	u32 pwr_over_util_slope;
	int vin_code;
};

struct nv_pmu_perf_rpc_vfe_equ_eval {
	u8 equ_idx;
	u8 var_count;
	u8 output_type;
	struct nv_pmu_perf_vfe_var_value var_values[
		NV_PMU_PERF_RPC_VFE_EQU_EVAL_VAR_COUNT_MAX];
	union nv_pmu_perf_vfe_equ_result result;
};

struct nv_pmu_rpc_struct_perf_vfe_eval {
	/*[IN/OUT] Must be first field in RPC structure */
	struct nv_pmu_rpc_header hdr;
	struct nv_pmu_perf_rpc_vfe_equ_eval data;
	u32  scratch[1];
};

struct nv_pmu_perf_rpc_vfe_load {
	bool b_load;
};

struct nv_pmu_perf_vfe_var_boardobjgrp_get_status_header {
	struct nv_pmu_boardobjgrp_e32 super;
};

struct nv_pmu_perf_vfe_var_get_status_super {
	struct nv_pmu_boardobj_query obj;
};

union ctrl_perf_vfe_var_single_sensed_fuse_value_data {
	int signed_value;
	u32 unsigned_value;
};

struct ctrl_perf_vfe_var_single_sensed_fuse_value {
	bool b_signed;
	union ctrl_perf_vfe_var_single_sensed_fuse_value_data data;
};

struct nv_pmu_perf_vfe_var_single_sensed_fuse_get_status {
	struct nv_pmu_perf_vfe_var_get_status_super super;
	struct ctrl_perf_vfe_var_single_sensed_fuse_value fuse_value_integer;
	struct ctrl_perf_vfe_var_single_sensed_fuse_value fuse_value_hw_integer;
	u8 fuse_version;
	bool b_version_check_failed;
};

union nv_pmu_perf_vfe_var_boardobj_get_status_union {
	struct nv_pmu_boardobj_query obj;
	struct nv_pmu_perf_vfe_var_get_status_super super;
	struct nv_pmu_perf_vfe_var_single_sensed_fuse_get_status fuse_status;
};

NV_PMU_BOARDOBJ_GRP_GET_STATUS_MAKE_E32(perf, vfe_var);

struct nv_pmu_perf_vfe_var_boardobj_grp_get_status_pack {
	struct nv_pmu_perf_vfe_var_boardobj_grp_get_status pri;
	struct nv_pmu_perf_vfe_var_boardobj_grp_get_status rppm;
};

struct nv_pmu_vfe_var {
	struct nv_pmu_boardobj super;
	u32 out_range_min;
	u32 out_range_max;
	struct ctrl_boardobjgrp_mask_e32 mask_dependent_vars;
	struct ctrl_boardobjgrp_mask_e255 mask_dependent_equs;
};

struct nv_pmu_vfe_var_derived {
	struct nv_pmu_vfe_var super;
};

struct nv_pmu_vfe_var_derived_product {
	struct nv_pmu_vfe_var_derived super;
	u8 var_idx0;
	u8 var_idx1;
};

struct nv_pmu_vfe_var_derived_sum {
	struct nv_pmu_vfe_var_derived super;
	u8 var_idx0;
	u8 var_idx1;
};

struct nv_pmu_vfe_var_single {
	struct nv_pmu_vfe_var super;
	u8 override_type;
	u32 override_value;
};

struct nv_pmu_vfe_var_single_frequency {
	struct nv_pmu_vfe_var_single super;
	u8 clk_domain_idx;
};

struct nv_pmu_vfe_var_single_caller_specified {
	struct nv_pmu_vfe_var_single super;
	u8 uid;
};

struct nv_pmu_vfe_var_single_sensed {
	struct nv_pmu_vfe_var_single super;
};

struct ctrl_bios_vfield_register_segment_super {
	u8 low_bit;
	u8 high_bit;
};

struct ctrl_bios_vfield_register_segment_reg {
	struct ctrl_bios_vfield_register_segment_super super;
	u32 addr;
};

struct ctrl_bios_vfield_register_segment_index_reg {
	struct ctrl_bios_vfield_register_segment_super super;
	u32 addr;
	u32 reg_index;
	u32 index;
};

union ctrl_bios_vfield_register_segment_data {
	struct ctrl_bios_vfield_register_segment_reg reg;
	struct ctrl_bios_vfield_register_segment_index_reg index_reg;
};

struct ctrl_bios_vfield_register_segment {
	u8 type;
	union ctrl_bios_vfield_register_segment_data data;
};

struct ctrl_perf_vfe_var_single_sensed_fuse_info {
	u8 segment_count;
	struct ctrl_bios_vfield_register_segment
		segments[NV_PMU_VFE_VAR_SINGLE_SENSED_FUSE_SEGMENTS_MAX];
};

struct ctrl_perf_vfe_var_single_sensed_fuse_override_info {
	u32 fuse_val_override;
	u8 b_fuse_regkey_override;
};

struct ctrl_perf_vfe_var_single_sensed_fuse_vfield_info {
	struct ctrl_perf_vfe_var_single_sensed_fuse_info fuse;
	u32 fuse_val_default;
	u32 hw_correction_scale;
	int hw_correction_offset;
	u8 v_field_id;
};

struct ctrl_perf_vfe_var_single_sensed_fuse_ver_vfield_info {
	struct ctrl_perf_vfe_var_single_sensed_fuse_info fuse;
	u8 ver_expected;
	bool b_ver_expected_is_mask;
	bool b_ver_check;
	bool b_ver_check_ignore;
	bool b_use_default_on_ver_check_fail;
	u8 v_field_id_ver;
};

struct nv_pmu_vfe_var_single_sensed_fuse {
	struct nv_pmu_vfe_var_single_sensed super;
	struct ctrl_perf_vfe_var_single_sensed_fuse_override_info override_info;
	struct ctrl_perf_vfe_var_single_sensed_fuse_vfield_info vfield_info;
	struct ctrl_perf_vfe_var_single_sensed_fuse_ver_vfield_info
								vfield_ver_info;
	struct ctrl_perf_vfe_var_single_sensed_fuse_value fuse_val_default;
	bool b_fuse_value_signed;
};

struct nv_pmu_vfe_var_single_sensed_temp {
	struct nv_pmu_vfe_var_single_sensed super;
	u8 therm_channel_index;
	int temp_hysteresis_positive;
	int temp_hysteresis_negative;
	int temp_default;
};

struct nv_pmu_vfe_var_single_voltage {
	struct nv_pmu_vfe_var_single super;
};

struct nv_pmu_perf_vfe_var_boardobjgrp_set_header {
	struct nv_pmu_boardobjgrp_e32 super;
	u8 polling_periodms;
};

union nv_pmu_perf_vfe_var_boardobj_set_union {
	struct nv_pmu_boardobj obj;
	struct nv_pmu_vfe_var var;
	struct nv_pmu_vfe_var_derived var_derived;
	struct nv_pmu_vfe_var_derived_product var_derived_product;
	struct nv_pmu_vfe_var_derived_sum var_derived_sum;
	struct nv_pmu_vfe_var_single var_single;
	struct nv_pmu_vfe_var_single_frequency var_single_frequiency;
	struct nv_pmu_vfe_var_single_sensed var_single_sensed;
	struct nv_pmu_vfe_var_single_sensed_fuse var_single_sensed_fuse;
	struct nv_pmu_vfe_var_single_sensed_temp var_single_sensed_temp;
	struct nv_pmu_vfe_var_single_voltage var_single_voltage;
	struct nv_pmu_vfe_var_single_caller_specified
					var_single_caller_specified;
};

NV_PMU_BOARDOBJ_GRP_SET_MAKE_E32(perf, vfe_var);

struct nv_pmu_perf_vfe_var_boardobj_grp_set_pack {
	struct nv_pmu_perf_vfe_var_boardobj_grp_set pri;
	struct nv_pmu_perf_vfe_var_boardobj_grp_set rppm;
};

struct nv_pmu_vfe_equ {
	struct nv_pmu_boardobj super;
	u8 var_idx;
	u8 equ_idx_next;
	u8 output_type;
	u32 out_range_min;
	u32 out_range_max;
};

struct nv_pmu_vfe_equ_compare {
	struct nv_pmu_vfe_equ super;
	u8 func_id;
	u8 equ_idx_true;
	u8 equ_idx_false;
	u32 criteria;
};

struct nv_pmu_vfe_equ_minmax {
	struct nv_pmu_vfe_equ super;
	bool b_max;
	u8 equ_idx0;
	u8 equ_idx1;
};

struct nv_pmu_vfe_equ_quadratic {
	struct nv_pmu_vfe_equ super;
	u32 coeffs[CTRL_PERF_VFE_EQU_QUADRATIC_COEFF_COUNT];
};

struct nv_pmu_vfe_equ_scalar {
	struct nv_pmu_vfe_equ super;
	u8 equ_idx_to_scale;
};

struct nv_pmu_perf_vfe_equ_boardobjgrp_set_header {
	struct nv_pmu_boardobjgrp_e255 super;
};

union nv_pmu_perf_vfe_equ_boardobj_set_union {
	struct nv_pmu_boardobj obj;
	struct nv_pmu_vfe_equ equ;
	struct nv_pmu_vfe_equ_compare equ_comapre;
	struct nv_pmu_vfe_equ_minmax equ_minmax;
	struct nv_pmu_vfe_equ_quadratic equ_quadratic;
	struct nv_pmu_vfe_equ_scalar equ_scalar;
};

NV_PMU_BOARDOBJ_GRP_SET_MAKE_E255(perf, vfe_equ);

struct nv_pmu_perf_vfe_equ_boardobj_grp_set_pack {
	struct nv_pmu_perf_vfe_equ_boardobj_grp_set pri;
	struct nv_pmu_perf_vfe_var_boardobj_grp_set rppm;
};

#endif /* NVGPU_PMUIF_PERFVFE_H */
