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

#include "therm_dev.h"
#include "ucode_therm_inf.h"
#include "thrm.h"

bool therm_device_idx_is_valid(struct nvgpu_pmu_therm *therm_pmu, u8 idx)
{
	return boardobjgrp_idxisvalid(
			&(therm_pmu->therm_deviceobjs.super.super), idx);
}

static int _therm_device_pmudata_instget(struct gk20a *g,
			struct nv_pmu_boardobjgrp *pmuboardobjgrp,
			struct nv_pmu_boardobj **pmu_obj,
			u8 idx)
{
	struct nv_pmu_therm_therm_device_boardobj_grp_set *pgrp_set =
		(struct nv_pmu_therm_therm_device_boardobj_grp_set *)
		pmuboardobjgrp;

	nvgpu_log_info(g, " ");

	/*check whether pmuboardobjgrp has a valid boardobj in index*/
	if (((u32)BIT(idx) &
			pgrp_set->hdr.data.super.obj_mask.super.data[0]) == 0U) {
		return -EINVAL;
	}

	*pmu_obj = (struct nv_pmu_boardobj *)(void *)
		&pgrp_set->objects[idx].data;

	nvgpu_log_info(g, " Done");

	return 0;
}

static int construct_therm_device(struct gk20a *g,
	struct pmu_board_obj *obj, void *pargs)
{
	return pmu_board_obj_construct_super(g, obj, pargs);
}

static int construct_therm_device_gpu(struct gk20a *g,
	struct pmu_board_obj *obj, void *pargs)
{
	return construct_therm_device(g, obj, pargs);
}

static struct pmu_board_obj *therm_device_construct(struct gk20a *g,
	void *pargs)
{
	struct pmu_board_obj *obj = NULL;
	struct therm_device *ptherm_device = NULL;
	int status = 0;

	if (pmu_board_obj_get_type(pargs) !=
			NV_VBIOS_THERM_DEVICE_1X_ENTRY_CLASS_GPU) {
		nvgpu_err(g, "unsupported therm_device class - 0x%x",
			pmu_board_obj_get_type(pargs));
		return NULL;
	}

	ptherm_device = nvgpu_kzalloc(g, sizeof(struct therm_device));
	if (ptherm_device == NULL) {
		return NULL;
	}
	obj = (struct pmu_board_obj *)(void *)ptherm_device;

	status = construct_therm_device_gpu(g, obj, pargs);

	if (status != 0) {
		nvgpu_err(g,
			"could not allocate memory for therm_device");
		nvgpu_kfree(g, obj);
		obj = NULL;
	}

	return obj;
}

static int devinit_get_therm_device_table(struct gk20a *g,
				struct therm_devices *pthermdeviceobjs)
{
	int status = 0;
	u8 *therm_device_table_ptr = NULL;
	u8 *curr_therm_device_table_ptr = NULL;
	struct pmu_board_obj *obj_tmp;
	struct therm_device_1x_header therm_device_table_header = { 0 };
	struct therm_device_1x_entry *therm_device_table_entry = NULL;
	u32 index;
	u32 obj_index = 0;
	u8 class_id = 0;
	bool error_status = false;
	union {
		struct pmu_board_obj obj;
		struct therm_device therm_device;
	} therm_device_data;

	nvgpu_log_info(g, " ");

	therm_device_table_ptr = (u8 *)nvgpu_bios_get_perf_table_ptrs(g,
			nvgpu_bios_get_bit_token(g, NVGPU_BIOS_PERF_TOKEN),
			THERMAL_DEVICE_TABLE);
	if (therm_device_table_ptr == NULL) {
		status = -EINVAL;
		goto done;
	}

	nvgpu_memcpy((u8 *)&therm_device_table_header, therm_device_table_ptr,
		VBIOS_THERM_DEVICE_1X_HEADER_SIZE_04);

	if (therm_device_table_header.version !=
			VBIOS_THERM_DEVICE_VERSION_1X) {
		status = -EINVAL;
		goto done;
	}

	if (therm_device_table_header.header_size <
			VBIOS_THERM_DEVICE_1X_HEADER_SIZE_04) {
		status = -EINVAL;
		goto done;
	}

	curr_therm_device_table_ptr = (therm_device_table_ptr +
		VBIOS_THERM_DEVICE_1X_HEADER_SIZE_04);

	for (index = 0; index < therm_device_table_header.num_table_entries;
		index++) {
		therm_device_table_entry = (struct therm_device_1x_entry *)
			(curr_therm_device_table_ptr +
				(therm_device_table_header.table_entry_size * index));

		class_id = therm_device_table_entry->class_id;

		switch (class_id) {
		case NV_VBIOS_THERM_DEVICE_1X_ENTRY_CLASS_INVALID:
			continue;
			break;
		case NV_VBIOS_THERM_DEVICE_1X_ENTRY_CLASS_GPU:
			break;
		case NV_VBIOS_THERM_DEVICE_1X_ENTRY_CLASS_GPU_GPC_SCI:
			continue;
			break;
		case NV_VBIOS_THERM_DEVICE_1X_ENTRY_CLASS_GPU_GPC_TSOSC:
			continue;
			break;
		default:
			nvgpu_err(g,
				"Unknown thermal device class i - %x, class - %x",
				index, class_id);
			error_status = true;
			break;
		}

		if (error_status == true) {
			goto done;
		}

		therm_device_data.obj.type = class_id;
		obj_tmp = therm_device_construct(g, &therm_device_data);
		if (obj_tmp == NULL) {
			nvgpu_err(g,
				"unable to create thermal device for %d type %d",
				index, therm_device_data.obj.type);
			status = -EINVAL;
			goto done;
		}

		status = boardobjgrp_objinsert(&pthermdeviceobjs->super.super,
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

int therm_device_sw_setup(struct gk20a *g)
{
	int status;
	struct boardobjgrp *pboardobjgrp = NULL;
	struct therm_devices *pthermdeviceobjs;

	/* Construct the Super Class and override the Interfaces */
	status = nvgpu_boardobjgrp_construct_e32(g,
			&g->pmu->therm_pmu->therm_deviceobjs.super);
	if (status != 0) {
		nvgpu_err(g,
			  "error creating boardobjgrp for therm devices,"
			  "status - 0x%x", status);
		goto done;
	}

	pboardobjgrp = &g->pmu->therm_pmu->therm_deviceobjs.super.super;
	pthermdeviceobjs = &(g->pmu->therm_pmu->therm_deviceobjs);

	/* Override the Interfaces */
	pboardobjgrp->pmudatainstget = _therm_device_pmudata_instget;

	status = devinit_get_therm_device_table(g, pthermdeviceobjs);
	if (status != 0) {
		goto done;
	}

	BOARDOBJGRP_PMU_CONSTRUCT(pboardobjgrp, THERM, THERM_DEVICE);

	status = BOARDOBJGRP_PMU_CMD_GRP_SET_CONSTRUCT(g, pboardobjgrp,
			therm, THERM, therm_device, THERM_DEVICE);
	if (status != 0) {
		nvgpu_err(g,
			  "error constructing PMU_BOARDOBJ_CMD_GRP_SET interface - 0x%x",
			  status);
		goto done;
	}

done:
	nvgpu_log_info(g, " done status %x", status);
	return status;
}

int therm_device_pmu_setup(struct gk20a *g)
{
	int status = 0;
	struct boardobjgrp *pboardobjgrp = NULL;

	nvgpu_log_info(g, " ");

	if (!BOARDOBJGRP_IS_EMPTY(
			&g->pmu->therm_pmu->therm_deviceobjs.super.super)) {
		pboardobjgrp = &g->pmu->therm_pmu->therm_deviceobjs.super.super;
		status = pboardobjgrp->pmuinithandle(g, pboardobjgrp);
		if (status != 0) {
			goto exit;
		}
	}

exit:
	return status;
}
