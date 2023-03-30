/*
 * Copyright (c) 2020, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef NVGPU_VOLT_H
#define NVGPU_VOLT_H

#include <nvgpu/boardobjgrp_e32.h>
#include <common/pmu/boardobj/ucode_boardobj_inf.h>

#define CTRL_VOLT_RAIL_VOLT_DELTA_MAX_ENTRIES	0x04U


#define CTRL_VOLT_VOLT_RAIL_MAX_RAILS	\
	CTRL_BOARDOBJGRP_E32_MAX_OBJECTS

#define CTRL_VOLT_RAIL_VOLT_DELTA_MAX_ENTRIES	0x04U
#define CTRL_VOLT_VOLT_DEV_VID_VSEL_MAX_ENTRIES	0x8U
#define CTRL_VOLT_DOMAIN_INVALID				0x00U
#define CLK_PROG_VFE_ENTRY_SRAM					0x01U
#define NV_PMU_VOLT_VALUE_0V_IN_UV				0U

/*
 * Macros for Voltage Domain HAL.
 */
#define CTRL_VOLT_DOMAIN_HAL_GP10X_SINGLE_RAIL 0x00U

/*!
 * Special value corresponding to an invalid Voltage Rail Index.
 */
#define CTRL_VOLT_RAIL_INDEX_INVALID	\
			CTRL_BOARDOBJ_IDX_INVALID

/*!
 * Special value corresponding to an invalid Voltage Device Index.
 */
#define CTRL_VOLT_DEVICE_INDEX_INVALID	\
			CTRL_BOARDOBJ_IDX_INVALID

/*!
 * Special value corresponding to an invalid Voltage Policy Index.
 */
#define CTRL_VOLT_POLICY_INDEX_INVALID	\
			CTRL_BOARDOBJ_IDX_INVALID

enum nv_pmu_pmgr_pwm_source {
	NV_PMU_PMGR_PWM_SOURCE_INVALID = 0,
	NV_PMU_PMGR_PWM_SOURCE_THERM_VID_PWM_0 = 4,
	NV_PMU_PMGR_PWM_SOURCE_THERM_IPC_VMIN_VID_PWM_0 = 13U,
};

/*!
 * Macros for Voltage Device Types.
 */
#define CTRL_VOLT_DEVICE_TYPE_INVALID		0x00U
#define CTRL_VOLT_DEVICE_TYPE_PWM		0x03U

/*
 * Macros for Volt Device Operation types.
 */
#define CTRL_VOLT_DEVICE_OPERATION_TYPE_INVALID			0x00U
#define CTRL_VOLT_DEVICE_OPERATION_TYPE_DEFAULT			0x01U
#define CTRL_VOLT_VOLT_DEVICE_OPERATION_TYPE_IPC_VMIN		0x04U

/*!
 * Macros for Voltage Domains.
 */
#define CTRL_VOLT_DOMAIN_INVALID	0x00U
#define CTRL_VOLT_DOMAIN_LOGIC		0x01U
#define CTRL_VOLT_DOMAIN_SRAM		0x02U

/*!
 * Macros for Volt Policy types.
 *
 * Virtual VOLT_POLICY types are indexed starting from 0xFF.
 */
#define CTRL_VOLT_POLICY_TYPE_INVALID			0x00U
#define CTRL_VOLT_POLICY_TYPE_SINGLE_RAIL		0x01U
#define CTRL_VOLT_POLICY_TYPE_SR_MULTI_STEP		0x02U
#define CTRL_VOLT_POLICY_TYPE_SR_SINGLE_STEP		0x03U
#define CTRL_VOLT_POLICY_TYPE_SINGLE_RAIL_MULTI_STEP	0x04U
#define CTRL_VOLT_POLICY_TYPE_SPLIT_RAIL		0xFEU
#define CTRL_VOLT_POLICY_TYPE_UNKNOWN			0xFFU

/*!
 * Macros for Volt Policy Client types.
 */
#define CTRL_VOLT_POLICY_CLIENT_INVALID			0x00U
#define CTRL_VOLT_POLICY_CLIENT_PERF_CORE_VF_SEQ	0x01U

/*!
 * metadata of voltage rail functionality.
 */
struct voltage_rail_metadata {
	u8 volt_domain_hal;
	u8 pct_delta;
	u32 ext_rel_delta_uv[CTRL_VOLT_RAIL_VOLT_DELTA_MAX_ENTRIES];
	u8 logic_rail_idx;
	u8 sram_rail_idx;
	struct boardobjgrp_e32 volt_rails;
};

struct voltage_device_metadata {
	struct boardobjgrp_e32 volt_devices;
};

struct voltage_policy_metadata {
	struct boardobjgrp_e32 volt_policies;
	u8 perf_core_vf_seq_policy_idx;
};

struct nvgpu_pmu_volt_metadata {
	struct voltage_rail_metadata volt_rail_metadata;
	struct voltage_device_metadata volt_dev_metadata;
	struct voltage_policy_metadata volt_policy_metadata;
};

u8 volt_rail_vbios_volt_domain_convert_to_internal
	(struct gk20a *g, u8 vbios_volt_domain);

#endif /* NVGPU_VOLT_H */
