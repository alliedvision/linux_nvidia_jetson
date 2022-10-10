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

#include <nvgpu/bios.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/pmu/pmuif/nvgpu_cmdif.h>
#include <nvgpu/boardobjgrp.h>
#include <nvgpu/boardobjgrp_e32.h>
#include <nvgpu/pmu/boardobjgrp_classes.h>
#include <nvgpu/string.h>
#include <nvgpu/pmu/therm.h>

#include "therm_dev.h"
#include "therm_channel.h"
#include "ucode_therm_inf.h"
#include "thrm.h"

static int _therm_channel_pmudatainit_device(struct gk20a *g,
			struct pmu_board_obj *obj,
			struct nv_pmu_boardobj *pmu_obj)
{
	int status = 0;
	struct therm_channel *pchannel;
	struct therm_channel_device *ptherm_channel;
	struct nv_pmu_therm_therm_channel_device_boardobj_set *pset;

	status = pmu_board_obj_pmu_data_init_super(g, obj, pmu_obj);
	if (status != 0) {
		nvgpu_err(g,
			"error updating pmu boardobjgrp for therm channel 0x%x",
			status);
		status = -ENOMEM;
		goto done;
	}

	pchannel = (struct therm_channel *)(void *)obj;
	pset = (struct nv_pmu_therm_therm_channel_device_boardobj_set *)
			(void *)pmu_obj;
	ptherm_channel = (struct therm_channel_device *)(void *)obj;

	pset->super.scaling = pchannel->scaling;
	pset->super.offset = pchannel->offset;
	pset->super.temp_min = pchannel->temp_min;
	pset->super.temp_max = pchannel->temp_max;

	pset->therm_dev_idx = ptherm_channel->therm_dev_idx;
	pset->therm_dev_prov_idx = ptherm_channel->therm_dev_prov_idx;

done:
	return status;
}
static struct pmu_board_obj *construct_channel_device(struct gk20a *g,
			void *pargs, size_t pargs_size, u8 type)
{
	struct pmu_board_obj *obj = NULL;
	struct therm_channel *pchannel;
	struct therm_channel_device *pchannel_device;
	int status;
	u16 scale_shift = BIT16(8);
	struct therm_channel_device *therm_device = (struct therm_channel_device*)pargs;

	(void)type;

	pchannel_device = nvgpu_kzalloc(g, pargs_size);
	if (pchannel_device == NULL) {
		return NULL;
	}
	obj = (struct pmu_board_obj *)(void *)pchannel_device;

	status = pmu_board_obj_construct_super(g, obj, pargs);
	if (status != 0) {
		return NULL;
	}

	/* Set Super class interfaces */
	obj->pmudatainit = _therm_channel_pmudatainit_device;

	pchannel = (struct therm_channel *)(void *)obj;
	pchannel_device = (struct therm_channel_device *)(void *)obj;

	g->ops.therm.get_internal_sensor_limits(&pchannel->temp_max,
		&pchannel->temp_min);
	pchannel->scaling = S16(scale_shift);
	pchannel->offset = 0;

	pchannel_device->therm_dev_idx = therm_device->therm_dev_idx;
	pchannel_device->therm_dev_prov_idx = therm_device->therm_dev_prov_idx;

	nvgpu_log_info(g, " Done");

	return obj;
}

static int _therm_channel_pmudata_instget(struct gk20a *g,
			struct nv_pmu_boardobjgrp *pmuboardobjgrp,
			struct nv_pmu_boardobj **pmu_obj,
			u8 idx)
{
	struct nv_pmu_therm_therm_channel_boardobj_grp_set *pgrp_set =
		(struct nv_pmu_therm_therm_channel_boardobj_grp_set *)
		pmuboardobjgrp;

	nvgpu_log_info(g, " ");

	/*check whether pmuboardobjgrp has a valid boardobj in index*/
	if (((u32)BIT(idx) &
			pgrp_set->hdr.data.super.obj_mask.super.data[0]) == 0U) {
		return -EINVAL;
	}

	*pmu_obj = (struct nv_pmu_boardobj *)
		&pgrp_set->objects[idx].data.obj;

	nvgpu_log_info(g, " Done");

	return 0;
}

static int therm_channel_pmustatus_instget(struct gk20a *g,
	void *pboardobjgrppmu, struct nv_pmu_boardobj_query
	**obj_pmu_status, u8 idx)
{
	struct nv_pmu_therm_therm_channel_boardobj_grp_get_status *pmu_status =
		(struct nv_pmu_therm_therm_channel_boardobj_grp_get_status *)
		(void *)pboardobjgrppmu;

	(void)g;

	/*check whether pmuboardobjgrp has a valid boardobj in index*/
	if (((u32)BIT(idx) &
		pmu_status->hdr.data.super.obj_mask.super.data[0]) == 0U) {
		return -EINVAL;
	}

	*obj_pmu_status = (struct nv_pmu_boardobj_query *)
			&pmu_status->objects[idx].data.obj;
	return 0;
}

static int devinit_get_therm_channel_table(struct gk20a *g,
				struct therm_channels *pthermchannelobjs)
{
	int status = 0;
	u8 *therm_channel_table_ptr = NULL;
	u8 *curr_therm_channel_table_ptr = NULL;
	struct pmu_board_obj *obj_tmp;
	struct therm_channel_1x_header therm_channel_table_header = { 0 };
	struct therm_channel_1x_entry *therm_channel_table_entry = NULL;
	u32 index;
	u32 obj_index = 0;
	size_t therm_channel_size = 0;
	union {
		struct pmu_board_obj obj;
		struct therm_channel therm_channel;
		struct therm_channel_device device;
	} therm_channel_data;

	nvgpu_log_info(g, " ");

	therm_channel_table_ptr = (u8 *)nvgpu_bios_get_perf_table_ptrs(g,
			nvgpu_bios_get_bit_token(g, NVGPU_BIOS_PERF_TOKEN),
						THERMAL_CHANNEL_TABLE);
	if (therm_channel_table_ptr == NULL) {
		status = -EINVAL;
		goto done;
	}

	nvgpu_memcpy((u8 *)&therm_channel_table_header, therm_channel_table_ptr,
		VBIOS_THERM_CHANNEL_1X_HEADER_SIZE_09);

	if (therm_channel_table_header.version !=
			VBIOS_THERM_CHANNEL_VERSION_1X) {
		status = -EINVAL;
		goto done;
	}

	if (therm_channel_table_header.header_size <
			VBIOS_THERM_CHANNEL_1X_HEADER_SIZE_09) {
		status = -EINVAL;
		goto done;
	}

	curr_therm_channel_table_ptr = (therm_channel_table_ptr +
		VBIOS_THERM_CHANNEL_1X_HEADER_SIZE_09);

	for (index = 0; index < therm_channel_table_header.num_table_entries;
		index++) {
		therm_channel_table_entry = (struct therm_channel_1x_entry *)
			(curr_therm_channel_table_ptr +
				(therm_channel_table_header.table_entry_size * index));

		if (therm_channel_table_entry->class_id !=
				NV_VBIOS_THERM_CHANNEL_1X_ENTRY_CLASS_DEVICE) {
			continue;
		}

		therm_channel_data.device.therm_dev_idx = therm_channel_table_entry->param0;
		/* Check for valid therm device index */
		if (!therm_device_idx_is_valid(g->pmu->therm_pmu,
				therm_channel_data.device.therm_dev_idx)) {
			continue;
		}
		therm_channel_data.device.therm_dev_prov_idx = therm_channel_table_entry->param1;

		therm_channel_size = sizeof(struct therm_channel_device);
		therm_channel_data.obj.type = CTRL_THERMAL_THERM_CHANNEL_CLASS_DEVICE;

		obj_tmp = construct_channel_device(g, &therm_channel_data,
					therm_channel_size, therm_channel_data.obj.type);

		if (obj_tmp == NULL) {
			nvgpu_err(g,
				"unable to create thermal device for %d type %d",
				index, therm_channel_data.obj.type);
			status = -EINVAL;
			goto done;
		}

		status = boardobjgrp_objinsert(&pthermchannelobjs->super.super,
				obj_tmp, (u8)obj_index);

		if (status != 0) {
			nvgpu_err(g,
			"unable to insert thermal device boardobj for %d", index);
			status = -EINVAL;
			goto done;
		}

		++obj_index;
	}

done:
	nvgpu_log_info(g, " done status %x", status);
	return status;
}

int therm_channel_sw_setup(struct gk20a *g)
{
	int status;
	struct boardobjgrp *pboardobjgrp = NULL;
	struct therm_channels *pthermchannelobjs;

	/* Construct the Super Class and override the Interfaces */
	status = nvgpu_boardobjgrp_construct_e32(g,
			&g->pmu->therm_pmu->therm_channelobjs.super);
	if (status != 0) {
		nvgpu_err(g,
			  "error creating boardobjgrp for therm devices, "
			  "status - 0x%x", status);
		goto done;
	}

	pboardobjgrp = &g->pmu->therm_pmu->therm_channelobjs.super.super;
	pthermchannelobjs = &(g->pmu->therm_pmu->therm_channelobjs);

	/* Override the Interfaces */
	pboardobjgrp->pmudatainstget = _therm_channel_pmudata_instget;
	pboardobjgrp->pmustatusinstget = therm_channel_pmustatus_instget;

	status = devinit_get_therm_channel_table(g, pthermchannelobjs);
	if (status != 0) {
		goto done;
	}

	BOARDOBJGRP_PMU_CONSTRUCT(pboardobjgrp, THERM, THERM_CHANNEL);

	status = BOARDOBJGRP_PMU_CMD_GRP_SET_CONSTRUCT(g, pboardobjgrp,
			therm, THERM, therm_channel, THERM_CHANNEL);
	if (status != 0) {
		nvgpu_err(g,
			  "error constructing PMU_BOARDOBJ_CMD_GRP_SET interface - 0x%x",
			  status);
		goto done;
	}

	status = BOARDOBJGRP_PMU_CMD_GRP_GET_STATUS_CONSTRUCT(g, pboardobjgrp,
			therm, THERM, therm_channel, THERM_CHANNEL);
	if (status != 0) {
		nvgpu_err(g,
			"error constructing THERM_GET_STATUS interface - 0x%x",
			status);
		goto done;
	}

done:
	nvgpu_log_info(g, " done status %x", status);
	return status;
}

static int therm_channel_currtemp_update(struct gk20a *g,
		struct pmu_board_obj *obj,
		struct nv_pmu_boardobj *pmu_obj)
{
	struct therm_channel_get_status *therm_channel_obj;
	struct nv_pmu_therm_therm_channel_boardobj_get_status *pstatus;

	nvgpu_log_info(g, " ");

	therm_channel_obj = (struct therm_channel_get_status *)
		(void *)obj;
	pstatus = (struct nv_pmu_therm_therm_channel_boardobj_get_status *)
		(void *)pmu_obj;

	if (pstatus->super.type != therm_channel_obj->super.type) {
		nvgpu_err(g, "pmu data and boardobj type not matching");
		return -EINVAL;
	}

	therm_channel_obj->curr_temp = pstatus->current_temp;
	return 0;
}

static int therm_channel_boardobj_grp_get_status(struct gk20a *g)
{
	struct boardobjgrp *pboardobjgrp = NULL;
	struct boardobjgrpmask *pboardobjgrpmask;
	struct nv_pmu_boardobjgrp_super *pboardobjgrppmu;
	struct pmu_board_obj *obj = NULL;
	struct nv_pmu_boardobj_query *pboardobjpmustatus = NULL;
	int status;
	u8 index;

	nvgpu_log_info(g, " ");

	if (g->pmu->therm_pmu == NULL) {
		return -EINVAL;
	}

	pboardobjgrp = &g->pmu->therm_pmu->therm_channelobjs.super.super;
	pboardobjgrpmask = &g->pmu->therm_pmu->therm_channelobjs.super.mask.super;
	status = pboardobjgrp->pmugetstatus(g, pboardobjgrp, pboardobjgrpmask);
	if (status != 0) {
		nvgpu_err(g, "err getting boardobjs from pmu");
		return status;
	}
	pboardobjgrppmu = pboardobjgrp->pmu.getstatus.buf;

	BOARDOBJGRP_FOR_EACH(pboardobjgrp, struct pmu_board_obj*, obj, index) {
		status = pboardobjgrp->pmustatusinstget(g,
				(struct nv_pmu_boardobjgrp *)(void *)pboardobjgrppmu,
				&pboardobjpmustatus, index);
		if (status != 0) {
			nvgpu_err(g, "could not get status object instance");
			return status;
		}
		status = therm_channel_currtemp_update(g, obj,
				(struct nv_pmu_boardobj *)(void *)pboardobjpmustatus);
		if (status != 0) {
			nvgpu_err(g, "could not update therm_channel status");
			return status;
		}
	}
	return 0;

}

int nvgpu_pmu_therm_channel_get_curr_temp(struct gk20a *g, u32 *temp)
{
	struct boardobjgrp *pboardobjgrp;
	struct pmu_board_obj *obj = NULL;
	struct therm_channel_get_status *therm_channel_status = NULL;
	int status;
	u8 index;

	status = therm_channel_boardobj_grp_get_status(g);
	if (status != 0) {
		nvgpu_err(g, "therm_channel get status failed");
		return status;
	}

	pboardobjgrp = &g->pmu->therm_pmu->therm_channelobjs.super.super;

	BOARDOBJGRP_FOR_EACH(pboardobjgrp, struct pmu_board_obj*, obj, index) {
		therm_channel_status = (struct therm_channel_get_status *)
				(void *)obj;
		if (therm_channel_status->curr_temp != 0U) {
			*temp = therm_channel_status->curr_temp;
			return status;
		}
	}
	return status;
}

int therm_channel_pmu_setup(struct gk20a *g)
{
	int status = 0;
	struct boardobjgrp *pboardobjgrp = NULL;

	nvgpu_log_info(g, " ");

	if (!BOARDOBJGRP_IS_EMPTY(
			&g->pmu->therm_pmu->therm_channelobjs.super.super)) {
		pboardobjgrp =
			&g->pmu->therm_pmu->therm_channelobjs.super.super;
		status = pboardobjgrp->pmuinithandle(g, pboardobjgrp);
		if (status != 0) {
			goto exit;
		}
	}

exit:
	return status;
}
