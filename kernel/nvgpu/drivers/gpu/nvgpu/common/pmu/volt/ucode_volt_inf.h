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
#ifndef NVGPU_PMUIF_VOLT_H
#define NVGPU_PMUIF_VOLT_H

#include <nvgpu/flcnif_cmn.h>

struct nv_pmu_volt_volt_rail_boardobjgrp_set_header {
	struct nv_pmu_boardobjgrp_e32 super;
};

struct nv_pmu_volt_volt_rail_boardobj_set {

	struct nv_pmu_boardobj super;
	u8 rel_limit_vfe_equ_idx;
	u8 alt_rel_limit_vfe_equ_idx;
	u8 ov_limit_vfe_equ_idx;
	u8 vmin_limit_vfe_equ_idx;
	u8 volt_margin_limit_vfe_equ_idx;
	u8 pwr_equ_idx;
	u8 volt_dev_idx_default;
	u8 volt_dev_idx_ipc_vmin;
	u8 volt_scale_exp_pwr_equ_idx;
	struct ctrl_boardobjgrp_mask_e32 vin_dev_mask;
	struct ctrl_boardobjgrp_mask_e32 volt_dev_mask;
	s32 volt_delta_uv[CTRL_VOLT_RAIL_VOLT_DELTA_MAX_ENTRIES];
};

union nv_pmu_volt_volt_rail_boardobj_set_union {
	struct nv_pmu_boardobj obj;
	struct nv_pmu_volt_volt_rail_boardobj_set super;
};

NV_PMU_BOARDOBJ_GRP_SET_MAKE_E32(volt, volt_rail);

/* ------------ VOLT_DEVICE's GRP_SET defines and structures ------------ */

struct nv_pmu_volt_volt_device_boardobjgrp_set_header {
	struct nv_pmu_boardobjgrp_e32 super;
};

struct nv_pmu_volt_volt_device_boardobj_set {
	struct nv_pmu_boardobj super;
	u32 switch_delay_us;
	u32 voltage_min_uv;
	u32 voltage_max_uv;
	u32 volt_step_uv;
};

struct nv_pmu_volt_volt_device_vid_boardobj_set {
	struct nv_pmu_volt_volt_device_boardobj_set super;
	s32 voltage_base_uv;
	s32 voltage_offset_scale_uv;
	u8 gpio_pin[CTRL_VOLT_VOLT_DEV_VID_VSEL_MAX_ENTRIES];
	u8 vsel_mask;
};

struct nv_pmu_volt_volt_device_pwm_boardobj_set {
	struct nv_pmu_volt_volt_device_boardobj_set super;
	u32 raw_period;
	s32 voltage_base_uv;
	s32 voltage_offset_scale_uv;
	enum nv_pmu_pmgr_pwm_source pwm_source;
};

union nv_pmu_volt_volt_device_boardobj_set_union {
	struct nv_pmu_boardobj obj;
	struct nv_pmu_volt_volt_device_boardobj_set super;
	struct nv_pmu_volt_volt_device_vid_boardobj_set vid;
	struct nv_pmu_volt_volt_device_pwm_boardobj_set pwm;
};

NV_PMU_BOARDOBJ_GRP_SET_MAKE_E32(volt, volt_device);

/* ------------ VOLT_POLICY's GRP_SET defines and structures ------------ */
struct nv_pmu_volt_volt_policy_boardobjgrp_set_header {
	struct nv_pmu_boardobjgrp_e32 super;
	u8 perf_core_vf_seq_policy_idx;
};

struct nv_pmu_volt_volt_policy_boardobj_set {
	struct nv_pmu_boardobj super;
};
struct nv_pmu_volt_volt_policy_sr_boardobj_set {
	struct nv_pmu_volt_volt_policy_boardobj_set super;
	u8 rail_idx;
};

struct nv_pmu_volt_volt_policy_sr_multi_step_boardobj_set {
	struct nv_pmu_volt_volt_policy_sr_boardobj_set super;
	u16 inter_switch_delay_us;
	u32 ramp_up_step_size_uv;
	u32 ramp_down_step_size_uv;
};

struct nv_pmu_volt_volt_policy_splt_r_boardobj_set {
	struct nv_pmu_volt_volt_policy_boardobj_set super;
	u8 rail_idx_master;
	u8 rail_idx_slave;
	u8 delta_min_vfe_equ_idx;
	u8 delta_max_vfe_equ_idx;
	s32 offset_delta_min_uv;
	s32 offset_delta_max_uv;
};

struct nv_pmu_volt_volt_policy_srms_boardobj_set {
	struct nv_pmu_volt_volt_policy_splt_r_boardobj_set super;
	u16 inter_switch_delayus;
};

/* sr - > single_rail */
struct nv_pmu_volt_volt_policy_srss_boardobj_set {
	struct nv_pmu_volt_volt_policy_splt_r_boardobj_set super;
};

union nv_pmu_volt_volt_policy_boardobj_set_union {
	struct nv_pmu_boardobj obj;
	struct nv_pmu_volt_volt_policy_boardobj_set super;
	struct nv_pmu_volt_volt_policy_sr_boardobj_set single_rail;
	struct nv_pmu_volt_volt_policy_sr_multi_step_boardobj_set
		single_rail_ms;
	struct nv_pmu_volt_volt_policy_splt_r_boardobj_set split_rail;
	struct nv_pmu_volt_volt_policy_srms_boardobj_set
			split_rail_m_s;
	struct nv_pmu_volt_volt_policy_srss_boardobj_set
			split_rail_s_s;
};

NV_PMU_BOARDOBJ_GRP_SET_MAKE_E32(volt, volt_policy);

/* ----------- VOLT_RAIL's GRP_GET_STATUS defines and structures ----------- */
struct nv_pmu_volt_volt_rail_boardobjgrp_get_status_header {
	struct nv_pmu_boardobjgrp_e32 super;
};

struct nv_pmu_volt_volt_rail_boardobj_get_status {
	struct nv_pmu_boardobj_query super;
	u32 curr_volt_defaultu_v;
	u32 rel_limitu_v;
	u32 alt_rel_limitu_v;
	u32 ov_limitu_v;
	u32 max_limitu_v;
	u32 vmin_limitu_v;
	s32 volt_margin_limitu_v;
	u32 rsvd;
};

union nv_pmu_volt_volt_rail_boardobj_get_status_union {
	struct nv_pmu_boardobj_query obj;
	struct nv_pmu_volt_volt_rail_boardobj_get_status super;
};

NV_PMU_BOARDOBJ_GRP_GET_STATUS_MAKE_E32(volt, volt_rail);

#define NV_PMU_VOLT_CMD_ID_BOARDOBJ_GRP_SET			(0x00000000U)
#define NV_PMU_VOLT_CMD_ID_RPC					(0x00000001U)
#define NV_PMU_VOLT_CMD_ID_BOARDOBJ_GRP_GET_STATUS		(0x00000002U)

/*
 * VOLT MSG ID definitions
 */
#define NV_PMU_VOLT_MSG_ID_BOARDOBJ_GRP_SET			(0x00000000U)
#define NV_PMU_VOLT_MSG_ID_RPC					(0x00000001U)
#define NV_PMU_VOLT_MSG_ID_BOARDOBJ_GRP_GET_STATUS		(0x00000002U)

/* VOLT RPC */
#define NV_PMU_RPC_ID_VOLT_BOARD_OBJ_GRP_CMD			0x00U
#define NV_PMU_RPC_ID_VOLT_VOLT_SET_VOLTAGE			0x01U
#define NV_PMU_RPC_ID_VOLT_LOAD					0x02U
#define NV_PMU_RPC_ID_VOLT_VOLT_RAIL_GET_VOLTAGE		0x03U
#define NV_PMU_RPC_ID_VOLT_VOLT_POLICY_SANITY_CHECK		0x04U
#define NV_PMU_RPC_ID_VOLT_TEST_EXECUTE				0x05U
#define NV_PMU_RPC_ID_VOLT__COUNT				0x06U

/*
 * Defines the structure that holds data
 * used to execute LOAD RPC.
 */
struct nv_pmu_rpc_struct_volt_load {
	/*[IN/OUT] Must be first field in RPC structure */
	struct nv_pmu_rpc_header hdr;
	u32  scratch[1];
};

#endif /* NVGPU_PMUIF_VOLT_H */
