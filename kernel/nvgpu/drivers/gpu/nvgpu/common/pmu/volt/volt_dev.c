/*
 * Copyright (c) 2016-2022, NVIDIA CORPORATION.  All rights reserved.
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

#include <nvgpu/types.h>
#include <nvgpu/pmu/pmuif/nvgpu_cmdif.h>
#include <nvgpu/bios.h>
#include <nvgpu/kmem.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/boardobjgrp.h>
#include <nvgpu/boardobjgrp_e32.h>
#include <nvgpu/pmu/boardobjgrp_classes.h>
#include <nvgpu/string.h>
#include <nvgpu/pmu/clk/clk.h>
#include <nvgpu/pmu/perf.h>
#include <nvgpu/pmu/volt.h>

#include "volt.h"
#include "ucode_volt_inf.h"
#include "volt_dev.h"
#include "volt_rail.h"

static int volt_device_pmu_data_init_super(struct gk20a *g,
	struct pmu_board_obj *obj, struct nv_pmu_boardobj *pmu_obj)
{
	int status;
	struct voltage_device *pdev;
	struct nv_pmu_volt_volt_device_boardobj_set *pset;

	status = pmu_board_obj_pmu_data_init_super(g, obj, pmu_obj);
	if (status != 0) {
		return status;
	}

	pdev = (struct voltage_device *)(void *)obj;
	pset = (struct nv_pmu_volt_volt_device_boardobj_set *)(void *)pmu_obj;

	pset->switch_delay_us = pdev->switch_delay_us;
	pset->voltage_min_uv = pdev->voltage_min_uv;
	pset->voltage_max_uv = pdev->voltage_max_uv;
	pset->volt_step_uv = pdev->volt_step_uv;

	return status;
}

static int volt_device_pmu_data_init_pwm(struct gk20a *g,
		struct pmu_board_obj *obj, struct nv_pmu_boardobj *pmu_obj)
{
	int status = 0;
	struct voltage_device_pwm *pdev;
	struct nv_pmu_volt_volt_device_pwm_boardobj_set *pset;

	status = volt_device_pmu_data_init_super(g, obj, pmu_obj);
	if (status != 0) {
		return  status;
	}

	pdev = (struct voltage_device_pwm *)(void *)obj;
	pset = (struct nv_pmu_volt_volt_device_pwm_boardobj_set *)(void *)pmu_obj;

	pset->raw_period = pdev->raw_period;
	pset->voltage_base_uv = pdev->voltage_base_uv;
	pset->voltage_offset_scale_uv = pdev->voltage_offset_scale_uv;
	pset->pwm_source = pdev->source;

	return status;
}

static int volt_construct_volt_device(struct gk20a *g,
	struct pmu_board_obj **obj, size_t size, void *pargs)
{
	struct voltage_device *ptmp_dev = (struct voltage_device *)pargs;
	struct voltage_device *pvolt_dev = NULL;
	int status = 0;

	pvolt_dev = nvgpu_kzalloc(g, size);
	if (pvolt_dev == NULL) {
		return -ENOMEM;
	}

	status = pmu_board_obj_construct_super(g,
			(struct pmu_board_obj *)(void *)pvolt_dev, pargs);
	if (status != 0) {
		return -EINVAL;
	}

	*obj = (struct pmu_board_obj *)(void *)pvolt_dev;

	pvolt_dev->volt_domain = ptmp_dev->volt_domain;
	pvolt_dev->i2c_dev_idx = ptmp_dev->i2c_dev_idx;
	pvolt_dev->switch_delay_us = ptmp_dev->switch_delay_us;
	pvolt_dev->rsvd_0 = VOLTAGE_DESCRIPTOR_TABLE_ENTRY_INVALID;
	pvolt_dev->rsvd_1 =
			VOLTAGE_DESCRIPTOR_TABLE_ENTRY_INVALID;
	pvolt_dev->operation_type = ptmp_dev->operation_type;
	pvolt_dev->voltage_min_uv = ptmp_dev->voltage_min_uv;
	pvolt_dev->voltage_max_uv = ptmp_dev->voltage_max_uv;

	pvolt_dev->super.pmudatainit = volt_device_pmu_data_init_super;

	return status;
}

static int volt_construct_pwm_volt_device(struct gk20a *g,
		struct pmu_board_obj **obj,
		size_t size, void *pargs)
{
	struct pmu_board_obj *obj_tmp = NULL;
	struct voltage_device_pwm *ptmp_dev =
			(struct voltage_device_pwm *)pargs;
	struct voltage_device_pwm *pdev = NULL;
	int status = 0;

	status = volt_construct_volt_device(g, obj, size, pargs);
	if (status != 0) {
		return status;
	}

	obj_tmp = (*obj);
	pdev  = (struct voltage_device_pwm *)(void *)*obj;

	obj_tmp->pmudatainit  = volt_device_pmu_data_init_pwm;

	/* Set VOLTAGE_DEVICE_PWM-specific parameters */
	pdev->voltage_base_uv = ptmp_dev->voltage_base_uv;
	pdev->voltage_offset_scale_uv = ptmp_dev->voltage_offset_scale_uv;
	pdev->source = ptmp_dev->source;
	pdev->raw_period = ptmp_dev->raw_period;

	return status;
}


static struct voltage_device_entry *volt_dev_construct_dev_entry_pwm(
		struct gk20a *g,
		u32 voltage_uv, void *pargs)
{
	struct voltage_device_pwm_entry *pentry = NULL;
	struct voltage_device_pwm_entry *ptmp_entry =
			(struct voltage_device_pwm_entry *)pargs;

	pentry = nvgpu_kzalloc(g, sizeof(struct voltage_device_pwm_entry));
	if (pentry == NULL) {
		return NULL;
	}

	(void) memset(pentry, 0, sizeof(struct voltage_device_pwm_entry));

	pentry->super.voltage_uv = voltage_uv;
	pentry->duty_cycle = ptmp_entry->duty_cycle;

	return (struct voltage_device_entry *)pentry;
}

static u8 volt_dev_operation_type_convert(u8 vbios_type)
{
	switch (vbios_type) {
	case NV_VBIOS_VDT_1X_ENTRY_PARAM1_PSV_OPERATION_TYPE_DEFAULT:
		return CTRL_VOLT_DEVICE_OPERATION_TYPE_DEFAULT;
	case NV_VBIOS_VDT_1X_ENTRY_PARAM1_PSV_OPERATION_TYPE_IPC_VMIN:
		return CTRL_VOLT_VOLT_DEVICE_OPERATION_TYPE_IPC_VMIN;
	}

	return CTRL_VOLT_DEVICE_OPERATION_TYPE_INVALID;
}

static struct voltage_device *volt_volt_device_construct(struct gk20a *g,
		void *pargs)
{
	struct pmu_board_obj *obj = NULL;

	if (pmu_board_obj_get_type(pargs) == CTRL_VOLT_DEVICE_TYPE_PWM) {
		int status = volt_construct_pwm_volt_device(g, &obj,
				sizeof(struct voltage_device_pwm), pargs);
		if (status != 0) {
			nvgpu_err(g,
				" Could not allocate memory for VOLTAGE_DEVICE type (%x).",
				pmu_board_obj_get_type(pargs));
			obj = NULL;
		}
	}

	return (struct voltage_device *)(void *)obj;
}

static int volt_get_voltage_device_table_1x_psv(struct gk20a *g,
		struct vbios_voltage_device_table_1x_entry *p_bios_entry,
		struct voltage_device_metadata *p_Volt_Device_Meta_Data,
		u8 entry_Idx)
{
	int status = 0;
	u32 entry_cnt = 0;
	struct voltage_device *pvolt_dev = NULL;
	struct voltage_device_pwm *pvolt_dev_pwm = NULL;
	struct voltage_device_pwm *ptmp_dev = NULL;
	u32 duty_cycle;
	u32 frequency_hz;
	u32 voltage_uv;
	u8 ext_dev_idx;
	u8 steps;
	u8 volt_domain = 0;
	struct voltage_device_pwm_entry pwm_entry = { };

	ptmp_dev = nvgpu_kzalloc(g, sizeof(struct voltage_device_pwm));
	if (ptmp_dev == NULL) {
		return -ENOMEM;
	}

	frequency_hz = BIOS_GET_FIELD(u32, p_bios_entry->param0,
			NV_VBIOS_VDT_1X_ENTRY_PARAM0_PSV_INPUT_FREQUENCY);

	ext_dev_idx = BIOS_GET_FIELD(u8, p_bios_entry->param0,
			NV_VBIOS_VDT_1X_ENTRY_PARAM0_PSV_EXT_DEVICE_INDEX);

	ptmp_dev->super.operation_type = volt_dev_operation_type_convert(
			BIOS_GET_FIELD(u8, p_bios_entry->param1,
			NV_VBIOS_VDT_1X_ENTRY_PARAM1_PSV_OPERATION_TYPE));

	if (ptmp_dev->super.operation_type ==
			CTRL_VOLT_DEVICE_OPERATION_TYPE_INVALID) {
		nvgpu_err(g, " Invalid Voltage Device Operation Type.");

		status = -EINVAL;
		goto done;
	}

	/* Skip and return success as ucode doesn't support IPC VMIN type */
	if (ptmp_dev->super.operation_type ==
			CTRL_VOLT_VOLT_DEVICE_OPERATION_TYPE_IPC_VMIN) {
		status = 0;
		goto done;
	}

	ptmp_dev->super.voltage_min_uv = BIOS_GET_FIELD(u32,
		p_bios_entry->param1,
		NV_VBIOS_VDT_1X_ENTRY_PARAM1_PSV_VOLTAGE_MINIMUM);

	ptmp_dev->super.voltage_max_uv = BIOS_GET_FIELD(u32,
		p_bios_entry->param2,
		NV_VBIOS_VDT_1X_ENTRY_PARAM2_PSV_VOLTAGE_MAXIMUM);

	ptmp_dev->voltage_base_uv = BIOS_GET_FIELD(s32, p_bios_entry->param3,
			NV_VBIOS_VDT_1X_ENTRY_PARAM3_PSV_VOLTAGE_BASE);

	steps = BIOS_GET_FIELD(u8, p_bios_entry->param3,
			NV_VBIOS_VDT_1X_ENTRY_PARAM3_PSV_VOLTAGE_STEPS);
	if (steps == VOLT_DEV_PWM_VOLTAGE_STEPS_INVALID) {
		steps = VOLT_DEV_PWM_VOLTAGE_STEPS_DEFAULT;
	}

	ptmp_dev->voltage_offset_scale_uv =
			BIOS_GET_FIELD(s32, p_bios_entry->param4,
				NV_VBIOS_VDT_1X_ENTRY_PARAM4_PSV_OFFSET_SCALE);

	volt_domain = volt_rail_vbios_volt_domain_convert_to_internal(g,
		(u8)p_bios_entry->volt_domain);
	if (volt_domain == CTRL_VOLT_DOMAIN_INVALID) {
		nvgpu_err(g, "invalid voltage domain = %d",
			(u8)p_bios_entry->volt_domain);
		status = -EINVAL;
		goto done;
	}

	if (ptmp_dev->super.operation_type ==
		CTRL_VOLT_DEVICE_OPERATION_TYPE_DEFAULT ||
		ptmp_dev->super.operation_type ==
		CTRL_VOLT_VOLT_DEVICE_OPERATION_TYPE_IPC_VMIN) {
		if (volt_domain == CTRL_VOLT_DOMAIN_LOGIC) {
			ptmp_dev->source =
				NV_PMU_PMGR_PWM_SOURCE_THERM_VID_PWM_0;
		}

		if (ptmp_dev->super.operation_type ==
			CTRL_VOLT_VOLT_DEVICE_OPERATION_TYPE_IPC_VMIN) {
			if (ptmp_dev->source ==
				NV_PMU_PMGR_PWM_SOURCE_THERM_VID_PWM_0) {
				ptmp_dev->source =
					NV_PMU_PMGR_PWM_SOURCE_THERM_IPC_VMIN_VID_PWM_0;
			}
		}
		ptmp_dev->raw_period =
			g->ops.clk.get_crystal_clk_hz(g) / frequency_hz;
	}

	/* Initialize data for parent class. */
	ptmp_dev->super.super.type = CTRL_VOLT_DEVICE_TYPE_PWM;
	ptmp_dev->super.volt_domain = volt_domain;
	ptmp_dev->super.i2c_dev_idx = ext_dev_idx;
	ptmp_dev->super.switch_delay_us = (u16)p_bios_entry->settle_time_us;

	pvolt_dev = volt_volt_device_construct(g, ptmp_dev);
	if (pvolt_dev == NULL) {
		nvgpu_err(g, " Failure to construct VOLTAGE_DEVICE object.");

		status = -EINVAL;
		goto done;
	}

	status = boardobjgrp_objinsert(
				&p_Volt_Device_Meta_Data->volt_devices.super,
				(struct pmu_board_obj *)pvolt_dev, entry_Idx);
	if (status != 0) {
		nvgpu_err(g,
			"could not add VOLTAGE_DEVICE for entry %d into boardobjgrp ",
			entry_Idx);
		goto done;
	}

	pvolt_dev_pwm = (struct voltage_device_pwm *)pvolt_dev;

	duty_cycle = 0;
	do {
		voltage_uv = (u32)(pvolt_dev_pwm->voltage_base_uv +
			(s32)((((s64)((s32)duty_cycle)) *
			pvolt_dev_pwm->voltage_offset_scale_uv)
			/ ((s64)((s32) pvolt_dev_pwm->raw_period))));

		/* Skip creating entry for invalid voltage. */
		if ((voltage_uv >= pvolt_dev_pwm->super.voltage_min_uv) &&
			(voltage_uv <= pvolt_dev_pwm->super.voltage_max_uv)) {
			if (pvolt_dev_pwm->voltage_offset_scale_uv < 0) {
				pwm_entry.duty_cycle =
					pvolt_dev_pwm->raw_period - duty_cycle;
			} else {
				pwm_entry.duty_cycle = duty_cycle;
			}

			/* Check if there is room left in the voltage table. */
			if (entry_cnt == VOLTAGE_TABLE_MAX_ENTRIES) {
				nvgpu_err(g, "Voltage table is full");
				status = -EINVAL;
				goto done;
			}

			pvolt_dev->pentry[entry_cnt] =
				volt_dev_construct_dev_entry_pwm(g,
					voltage_uv, &pwm_entry);
			if (pvolt_dev->pentry[entry_cnt] == NULL) {
				nvgpu_err(g,
					" Error creating voltage_device_pwm_entry!");
				status = -EINVAL;
				goto done;
			}

			entry_cnt++;
		}

		/* Obtain next value after the specified steps. */
		duty_cycle = duty_cycle + (u32)steps;

		/* Cap duty cycle to PWM period. */
		if (duty_cycle > pvolt_dev_pwm->raw_period) {
			duty_cycle = pvolt_dev_pwm->raw_period;
		}

	} while (duty_cycle < pvolt_dev_pwm->raw_period);

done:
	if (pvolt_dev != NULL) {
		pvolt_dev->num_entries = entry_cnt;
	}

	nvgpu_kfree(g, ptmp_dev);
	return status;
}

static int volt_get_volt_devices_table(struct gk20a *g,
		struct voltage_device_metadata *pvolt_device_metadata)
{
	int status = 0;
	u8 *volt_device_table_ptr = NULL;
	struct vbios_voltage_device_table_1x_header header = { 0 };
	struct vbios_voltage_device_table_1x_entry entry  = { 0 };
	u8 entry_idx;
	u8 *entry_offset;

	volt_device_table_ptr = (u8 *)nvgpu_bios_get_perf_table_ptrs(g,
			nvgpu_bios_get_bit_token(g, NVGPU_BIOS_PERF_TOKEN),
			VOLTAGE_DEVICE_TABLE);
	if (volt_device_table_ptr == NULL) {
		status = -EINVAL;
		goto done;
	}

	nvgpu_memcpy((u8 *)&header, volt_device_table_ptr,
			sizeof(struct vbios_voltage_device_table_1x_header));

	/* Read in the entries. */
	for (entry_idx = 0; entry_idx < header.num_table_entries; entry_idx++) {
		entry_offset = (volt_device_table_ptr + header.header_size +
					(entry_idx * header.table_entry_size));

		nvgpu_memcpy((u8 *)&entry, entry_offset,
			sizeof(struct vbios_voltage_device_table_1x_entry));

		if (entry.type == NV_VBIOS_VOLTAGE_DEVICE_1X_ENTRY_TYPE_PSV) {
			status = volt_get_voltage_device_table_1x_psv(g,
					&entry, pvolt_device_metadata,
					entry_idx);
		}
	}

done:
	return status;
}

static int volt_device_devgrp_pmudata_instget(struct gk20a *g,
	struct nv_pmu_boardobjgrp *pmuboardobjgrp,
	struct nv_pmu_boardobj **pmu_obj, u8 idx)
{
	struct nv_pmu_volt_volt_device_boardobj_grp_set *pgrp_set =
		(struct nv_pmu_volt_volt_device_boardobj_grp_set *)
		pmuboardobjgrp;

	nvgpu_log_info(g, " ");

	/*check whether pmuboardobjgrp has a valid boardobj in index*/
	if (((u32)BIT(idx) &
		pgrp_set->hdr.data.super.obj_mask.super.data[0]) == 0U) {
		return -EINVAL;
	}

	*pmu_obj = (struct nv_pmu_boardobj *)
		&pgrp_set->objects[idx].data.obj;
	nvgpu_log_info(g, "Done");
	return 0;
}

static int volt_device_state_init(struct gk20a *g,
			struct voltage_device *pvolt_dev)
{
	int status = 0;
	struct voltage_rail *pRail = NULL;
	u8 rail_idx = 0;

	/* Initialize VOLT_DEVICE step size. */
	if (pvolt_dev->num_entries <= VOLTAGE_TABLE_MAX_ENTRIES_ONE) {
		pvolt_dev->volt_step_uv = NV_PMU_VOLT_VALUE_0V_IN_UV;
	} else {
		pvolt_dev->volt_step_uv = (pvolt_dev->pentry[1]->voltage_uv -
				pvolt_dev->pentry[0]->voltage_uv);
	}

	/* Build VOLT_RAIL SW state from VOLT_DEVICE SW state. */
	/* If VOLT_RAIL isn't supported, exit. */
	if (!BOARDOBJGRP_IS_EMPTY(&g->pmu->volt->volt_metadata->
		volt_rail_metadata.volt_rails.super)) {
		rail_idx = nvgpu_pmu_volt_rail_volt_domain_convert_to_idx(g,
				pvolt_dev->volt_domain);
		if (rail_idx == CTRL_BOARDOBJ_IDX_INVALID) {
			nvgpu_err(g,
				" could not convert voltage domain to rail index.");
			status = -EINVAL;
			goto done;
		}

		pRail = (struct voltage_rail *)BOARDOBJGRP_OBJ_GET_BY_IDX(
			&g->pmu->volt->volt_metadata->volt_rail_metadata.volt_rails.super,
			rail_idx);
		if (pRail == NULL) {
			nvgpu_err(g,
				"could not obtain ptr to rail object from rail index");
			status = -EINVAL;
			goto done;
		}

		status = volt_rail_volt_dev_register(g, pRail,
			pmu_board_obj_get_idx(pvolt_dev), pvolt_dev->operation_type);
		if (status != 0) {
			nvgpu_err(g,
				"Failed to register the device with rail obj");
			goto done;
		}
	}

done:
	if (status != 0) {
		nvgpu_err(g, "Error in building rail sw state device sw");
	}

	return status;
}

int volt_dev_pmu_setup(struct gk20a *g)
{
	int status;
	struct boardobjgrp *pboardobjgrp = NULL;

	nvgpu_log_info(g, " ");

	pboardobjgrp = &g->pmu->volt->volt_metadata->volt_dev_metadata.volt_devices.super;

	if (!pboardobjgrp->bconstructed) {
		return -EINVAL;
	}

	status = pboardobjgrp->pmuinithandle(g, pboardobjgrp);

	nvgpu_log_info(g, "Done");
	return status;
}

int volt_dev_sw_setup(struct gk20a *g)
{
	int status = 0;
	struct boardobjgrp *pboardobjgrp = NULL;
	struct voltage_device *pvolt_device;
	u8 i;

	nvgpu_log_info(g, " ");

	status = nvgpu_boardobjgrp_construct_e32(g,
			&g->pmu->volt->volt_metadata->volt_dev_metadata.volt_devices);
	if (status != 0) {
		nvgpu_err(g,
			"error creating boardobjgrp for volt rail, "
			"status - 0x%x", status);
		goto done;
	}

	pboardobjgrp = &g->pmu->volt->volt_metadata->volt_dev_metadata.volt_devices.super;

	pboardobjgrp->pmudatainstget  = volt_device_devgrp_pmudata_instget;

	/* Obtain Voltage Rail Table from VBIOS */
	status = volt_get_volt_devices_table(g, &g->pmu->volt->volt_metadata->
			volt_dev_metadata);
	if (status != 0) {
		goto done;
	}

	/* Populate data for the VOLT_RAIL PMU interface */
	BOARDOBJGRP_PMU_CONSTRUCT(pboardobjgrp, VOLT, VOLT_DEVICE);

	status = BOARDOBJGRP_PMU_CMD_GRP_SET_CONSTRUCT(g, pboardobjgrp,
			volt, VOLT, volt_device, VOLT_DEVICE);
	if (status != 0) {
		nvgpu_err(g,
			"error constructing PMU_BOARDOBJ_CMD_GRP_SET interface - 0x%x",
			status);
		goto done;
	}

	/* update calibration to fuse */
	BOARDOBJGRP_FOR_EACH(&(g->pmu->volt->volt_metadata->volt_dev_metadata.volt_devices.
			       super),
			     struct voltage_device *, pvolt_device, i) {
		status = volt_device_state_init(g, pvolt_device);
		if (status != 0) {
			nvgpu_err(g,
				"failure while executing devices's state init interface");
			nvgpu_err(g,
				" railIdx = %d, status = 0x%x", i, status);
			goto done;
		}
	}

done:
	nvgpu_log_info(g, " done status %x", status);
	return status;
}
