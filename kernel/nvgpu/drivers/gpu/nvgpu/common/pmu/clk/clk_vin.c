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
#include <nvgpu/pmu/pmuif/nvgpu_cmdif.h>
#include <nvgpu/io.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/bug.h>
#include <nvgpu/boardobjgrp.h>
#include <nvgpu/boardobjgrp_e32.h>
#include <nvgpu/pmu/boardobjgrp_classes.h>
#include <nvgpu/string.h>
#include <nvgpu/pmu/clk/clk.h>
#include <nvgpu/pmu/cmd.h>
#include <nvgpu/pmu/volt.h>

#include "ucode_clk_inf.h"
#include "clk_vin.h"
#include "clk.h"

static int devinit_get_vin_device_table(struct gk20a *g,
		struct nvgpu_avfsvinobjs *pvinobjs);

static int vin_device_construct_v20(struct gk20a *g,
		struct pmu_board_obj **obj, size_t size, void *pargs);
static int vin_device_construct_super(struct gk20a *g,
		struct pmu_board_obj **obj, size_t size, void *pargs);
static struct clk_vin_device *construct_vin_device(
		struct gk20a *g, void *pargs);

static int vin_device_init_pmudata_v20(struct gk20a *g,
		struct pmu_board_obj *obj,
		struct nv_pmu_boardobj *pmu_obj);
static int vin_device_init_pmudata_super(struct gk20a *g,
		struct pmu_board_obj *obj,
		struct nv_pmu_boardobj *pmu_obj);

struct clk_vin_device *clk_get_vin_from_index(
		struct nvgpu_avfsvinobjs *pvinobjs, u8 idx)
{
	return ((struct clk_vin_device *)BOARDOBJGRP_OBJ_GET_BY_IDX(
		((struct boardobjgrp *)&(pvinobjs->super.super)), idx));
}

static int nvgpu_clk_avfs_get_vin_cal_fuse_v20(struct gk20a *g,
		struct nvgpu_avfsvinobjs *pvinobjs,
		struct vin_device_v20 *pvindev)
{
	int status = 0;
	s8 gain, offset;
	u8 i;

	if (pvinobjs->calibration_rev_vbios ==
			g->ops.fuse.read_vin_cal_fuse_rev(g)) {
		BOARDOBJGRP_FOR_EACH(&(pvinobjs->super.super),
				struct vin_device_v20 *, pvindev, i) {
			gain = 0;
			offset = 0;
			pvindev = (struct vin_device_v20 *)(void *)
					clk_get_vin_from_index(pvinobjs, i);
			status = g->ops.fuse.read_vin_cal_gain_offset_fuse(g,
					pvindev->super.id, &gain, &offset);
			if (status != 0) {
				nvgpu_err(g,
				"err reading vin cal for id %x", pvindev->super.id);
				return status;
			}
			pvindev->data.vin_cal.cal_v20.gain = gain;
			pvindev->data.vin_cal.cal_v20.offset = offset;
		}
	}
	return status;

}

static int _clk_vin_devgrp_pmudatainit_super(struct gk20a *g,
		struct boardobjgrp *pboardobjgrp,
		struct nv_pmu_boardobjgrp_super *pboardobjgrppmu)
{
	struct nv_pmu_clk_clk_vin_device_boardobjgrp_set_header *pset =
		(struct nv_pmu_clk_clk_vin_device_boardobjgrp_set_header *)
		pboardobjgrppmu;
	struct nvgpu_avfsvinobjs *pvin_obbj = (struct nvgpu_avfsvinobjs *)
			(void *)pboardobjgrp;
	int status = 0;

	nvgpu_log_info(g, " ");

	status = boardobjgrp_pmudatainit_e32(g, pboardobjgrp, pboardobjgrppmu);

	pset->b_vin_is_disable_allowed = pvin_obbj->vin_is_disable_allowed;
	pset->version = pvin_obbj->version;

	nvgpu_log_info(g, " Done");
	return status;
}

static int _clk_vin_devgrp_pmudata_instget(struct gk20a *g,
		struct nv_pmu_boardobjgrp *pmuboardobjgrp,
		struct nv_pmu_boardobj **pmu_obj, u8 idx)
{
	struct nv_pmu_clk_clk_vin_device_boardobj_grp_set *pgrp_set =
		(struct nv_pmu_clk_clk_vin_device_boardobj_grp_set *)
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

static int _clk_vin_devgrp_pmustatus_instget(struct gk20a *g,
		void *pboardobjgrppmu,
		struct nv_pmu_boardobj_query **obj_pmu_status, u8 idx)
{
	struct nv_pmu_clk_clk_vin_device_boardobj_grp_get_status
	*pgrp_get_status =
		(struct nv_pmu_clk_clk_vin_device_boardobj_grp_get_status *)
		pboardobjgrppmu;

	(void)g;

	/*check whether pmuboardobjgrp has a valid boardobj in index*/
	if (((u32)BIT(idx) &
		pgrp_get_status->hdr.data.super.obj_mask.super.data[0]) == 0U) {
		return -EINVAL;
	}

	*obj_pmu_status = (struct nv_pmu_boardobj_query *)
			&pgrp_get_status->objects[idx].data.obj;
	return 0;
}

int clk_vin_sw_setup(struct gk20a *g)
{
	int status;
	struct boardobjgrp *pboardobjgrp = NULL;
	struct vin_device_v20 *pvindev = NULL;
	struct nvgpu_avfsvinobjs *pvinobjs;

	nvgpu_log_info(g, " ");

	status = nvgpu_boardobjgrp_construct_e32(g,
			&g->pmu->clk_pmu->avfs_vinobjs->super);
	if (status != 0) {
		nvgpu_err(g,
			"error creating boardobjgrp for clk vin, statu - 0x%x",
			status);
		goto done;
	}

	pboardobjgrp = &g->pmu->clk_pmu->avfs_vinobjs->super.super;
	pvinobjs = g->pmu->clk_pmu->avfs_vinobjs;

	BOARDOBJGRP_PMU_CONSTRUCT(pboardobjgrp, CLK, VIN_DEVICE);

	status = BOARDOBJGRP_PMU_CMD_GRP_SET_CONSTRUCT(g, pboardobjgrp,
			clk, CLK, clk_vin_device, CLK_VIN_DEVICE);
	if (status != 0) {
		nvgpu_err(g,
			"error constructing PMU_BOARDOBJ_CMD_GRP_SET interface - 0x%x",
			status);
		goto done;
	}

	pboardobjgrp->pmudatainit  = _clk_vin_devgrp_pmudatainit_super;
	pboardobjgrp->pmudatainstget  = _clk_vin_devgrp_pmudata_instget;
	pboardobjgrp->pmustatusinstget  = _clk_vin_devgrp_pmustatus_instget;

	status = devinit_get_vin_device_table(g, g->pmu->clk_pmu->avfs_vinobjs);
	if (status != 0) {
		goto done;
	}

	/*update vin calibration to fuse */
	status = nvgpu_clk_avfs_get_vin_cal_fuse_v20(g, pvinobjs, pvindev);
	if (status != 0) {
		nvgpu_err(g, "clk_avfs_get_vin_cal_fuse_v20 failed err=%d",
			status);
		goto done;
	}

	status = BOARDOBJGRP_PMU_CMD_GRP_GET_STATUS_CONSTRUCT(g,
				&g->pmu->clk_pmu->avfs_vinobjs->super.super,
				clk, CLK, clk_vin_device, CLK_VIN_DEVICE);
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

int clk_vin_pmu_setup(struct gk20a *g)
{
	int status;
	struct boardobjgrp *pboardobjgrp = NULL;

	nvgpu_log_info(g, " ");

	pboardobjgrp = &g->pmu->clk_pmu->avfs_vinobjs->super.super;

	if (!pboardobjgrp->bconstructed) {
		return -EINVAL;
	}

	status = pboardobjgrp->pmuinithandle(g, pboardobjgrp);

	nvgpu_log_info(g, "Done");
	return status;
}

static int devinit_get_vin_device_table(struct gk20a *g,
		struct nvgpu_avfsvinobjs *pvinobjs)
{
	int status = 0;
	u8 *vin_table_ptr = NULL;
	struct vin_descriptor_header_10 vin_desc_table_header = { 0 };
	struct vin_descriptor_entry_10 vin_desc_table_entry = { 0 };
	u8 *vin_tbl_entry_ptr = NULL;
	u32 index = 0;
	s8 offset = 0, gain = 0;
	struct clk_vin_device *pvin_dev;
	u32 cal_type;

	union {
		struct pmu_board_obj obj;
		struct clk_vin_device vin_device;
		struct vin_device_v20 vin_device_v20;
	} vin_device_data;

	nvgpu_log_info(g, " ");

	vin_table_ptr = (u8 *)nvgpu_bios_get_perf_table_ptrs(g,
			nvgpu_bios_get_bit_token(g, NVGPU_BIOS_CLOCK_TOKEN),
						VIN_TABLE);
	if (vin_table_ptr == NULL) {
		status = -1;
		goto done;
	}

	nvgpu_memcpy((u8 *)&vin_desc_table_header, vin_table_ptr,
			sizeof(struct vin_descriptor_header_10));
	/* Right now we support 0x10 version only */
	pvinobjs->version = (vin_desc_table_header.version == 0x10U) ?
			NV2080_CTRL_CLK_VIN_DEVICES_V10 :
			NV2080_CTRL_CLK_VIN_DEVICES_DISABLED;
	pvinobjs->calibration_rev_vbios =
			BIOS_GET_FIELD(u8, vin_desc_table_header.flags0,
				NV_VIN_DESC_FLAGS0_VIN_CAL_REVISION);
	pvinobjs->vin_is_disable_allowed =
			BIOS_GET_FIELD(bool, vin_desc_table_header.flags0,
				NV_VIN_DESC_FLAGS0_DISABLE_CONTROL);
	cal_type = BIOS_GET_FIELD(u32, vin_desc_table_header.flags0,
				NV_VIN_DESC_FLAGS0_VIN_CAL_TYPE);
	if (cal_type != CTRL_CLK_VIN_CAL_TYPE_V20) {
		nvgpu_err(g, "Unsupported Vin calibration type");
		status = -1;
		goto done;
	}

	offset = BIOS_GET_FIELD(s8, vin_desc_table_header.vin_cal,
			NV_VIN_DESC_VIN_CAL_OFFSET);
	gain = BIOS_GET_FIELD(s8, vin_desc_table_header.vin_cal,
			NV_VIN_DESC_VIN_CAL_GAIN);

	/* Read table entries*/
	vin_tbl_entry_ptr = vin_table_ptr + vin_desc_table_header.header_sizee;
	for (index = 0; index < vin_desc_table_header.entry_count; index++) {
		nvgpu_memcpy((u8 *)&vin_desc_table_entry, vin_tbl_entry_ptr,
				sizeof(struct vin_descriptor_entry_10));

		if (vin_desc_table_entry.vin_device_type ==
				CTRL_CLK_VIN_TYPE_DISABLED) {
			continue;
		}

		vin_device_data.obj.type =
			(u8)vin_desc_table_entry.vin_device_type;
		vin_device_data.vin_device.id =
				(u8)vin_desc_table_entry.vin_device_id;
		vin_device_data.vin_device.volt_domain_vbios =
			(u8)vin_desc_table_entry.volt_domain_vbios;
		vin_device_data.vin_device.flls_shared_mask = 0;
		vin_device_data.vin_device.por_override_mode =
				CTRL_CLK_VIN_SW_OVERRIDE_VIN_USE_HW_REQ;
		vin_device_data.vin_device.override_mode =
				CTRL_CLK_VIN_SW_OVERRIDE_VIN_USE_HW_REQ;
		vin_device_data.vin_device_v20.data.cal_type = (u8) cal_type;
		vin_device_data.vin_device_v20.data.vin_cal.cal_v20.offset =
				offset;
		vin_device_data.vin_device_v20.data.vin_cal.cal_v20.gain =
				gain;
		vin_device_data.vin_device_v20.data.vin_cal.cal_v20.offset_vfe_idx =
					CTRL_CLK_VIN_VFE_IDX_INVALID;

		pvin_dev = construct_vin_device(g, (void *)&vin_device_data);

		status = boardobjgrp_objinsert(&pvinobjs->super.super,
				(struct pmu_board_obj *)pvin_dev, (u8)index);

		vin_tbl_entry_ptr += vin_desc_table_header.entry_size;
	}

done:
	nvgpu_log_info(g, " done status %x", status);
	return status;
}

static int vin_device_construct_v20(struct gk20a *g,
		struct pmu_board_obj **obj, size_t size, void *pargs)
{
	struct pmu_board_obj *obj_tmp = (struct pmu_board_obj *)pargs;
	struct vin_device_v20 *pvin_device_v20;
	struct vin_device_v20 *ptmpvin_device_v20 = (struct vin_device_v20 *)pargs;
	int status = 0;

	if (pmu_board_obj_get_type(pargs) != CTRL_CLK_VIN_TYPE_V20) {
		return -EINVAL;
	}

	obj_tmp->type_mask |= BIT32(CTRL_CLK_VIN_TYPE_V20);
	status = vin_device_construct_super(g, obj, size, pargs);
	if (status != 0) {
		return -EINVAL;
	}

	pvin_device_v20 = (struct vin_device_v20 *)(void *)*obj;

	pvin_device_v20->super.super.pmudatainit =
			vin_device_init_pmudata_v20;

	pvin_device_v20->data.cal_type = ptmpvin_device_v20->data.cal_type;
	pvin_device_v20->data.vin_cal.cal_v20.offset =
			ptmpvin_device_v20->data.vin_cal.cal_v20.offset;
	pvin_device_v20->data.vin_cal.cal_v20.gain =
			ptmpvin_device_v20->data.vin_cal.cal_v20.gain;
	pvin_device_v20->data.vin_cal.cal_v20.offset_vfe_idx =
			ptmpvin_device_v20->data.vin_cal.cal_v20.offset_vfe_idx;

	return status;
}
static int vin_device_construct_super(struct gk20a *g,
		struct pmu_board_obj **obj, size_t size, void *pargs)
{
	struct clk_vin_device *pvin_device;
	struct clk_vin_device *ptmpvin_device =
		(struct clk_vin_device *)pargs;
	int status = 0;

	pvin_device = nvgpu_kzalloc(g, size);
	if (pvin_device == NULL) {
		return -ENOMEM;
	}

	status = pmu_board_obj_construct_super(g,
			(struct pmu_board_obj *)(void *)pvin_device, pargs);
	if (status != 0) {
		return -EINVAL;
	}

	*obj = (struct pmu_board_obj *)(void *)pvin_device;

	pvin_device->super.pmudatainit =
			vin_device_init_pmudata_super;

	pvin_device->id = ptmpvin_device->id;
	pvin_device->volt_domain_vbios = ptmpvin_device->volt_domain_vbios;
	pvin_device->flls_shared_mask = ptmpvin_device->flls_shared_mask;
	pvin_device->volt_domain = CTRL_VOLT_DOMAIN_LOGIC;
	pvin_device->por_override_mode = ptmpvin_device->por_override_mode;
	pvin_device->override_mode = ptmpvin_device->override_mode;

	return status;
}
static struct clk_vin_device *construct_vin_device(
		struct gk20a *g, void *pargs)
{
	struct pmu_board_obj *obj = NULL;
	int status;

	nvgpu_log_info(g, " %d", pmu_board_obj_get_type(pargs));

	status = vin_device_construct_v20(g, &obj,
			sizeof(struct vin_device_v20), pargs);

	if (status != 0) {
		return NULL;
	}

	nvgpu_log_info(g, " Done");

	return (struct clk_vin_device *)(void *)obj;
}

static int vin_device_init_pmudata_v20(struct gk20a *g,
		struct pmu_board_obj *obj,
		struct nv_pmu_boardobj *pmu_obj)
{
	int status = 0;
	struct vin_device_v20 *pvin_dev_v20;
	struct nv_pmu_clk_clk_vin_device_v20_boardobj_set *perf_pmu_data;

	nvgpu_log_info(g, " ");

	status = vin_device_init_pmudata_super(g, obj, pmu_obj);
	if (status != 0) {
		return status;
	}

	pvin_dev_v20 = (struct vin_device_v20 *)(void *)obj;
	perf_pmu_data = (struct nv_pmu_clk_clk_vin_device_v20_boardobj_set *)
		pmu_obj;

	perf_pmu_data->data.cal_type = pvin_dev_v20->data.cal_type;
	perf_pmu_data->data.vin_cal.cal_v20.offset =
			pvin_dev_v20->data.vin_cal.cal_v20.offset;
	perf_pmu_data->data.vin_cal.cal_v20.gain =
			pvin_dev_v20->data.vin_cal.cal_v20.gain;
	perf_pmu_data->data.vin_cal.cal_v20.offset_vfe_idx =
			pvin_dev_v20->data.vin_cal.cal_v20.offset_vfe_idx;

	nvgpu_log_info(g, " Done");

	return status;
}

static int vin_device_init_pmudata_super(struct gk20a *g,
		struct pmu_board_obj *obj,
		struct nv_pmu_boardobj *pmu_obj)
{
	int status = 0;
	struct clk_vin_device *pvin_dev;
	struct nv_pmu_clk_clk_vin_device_boardobj_set *perf_pmu_data;

	nvgpu_log_info(g, " ");

	status = pmu_board_obj_pmu_data_init_super(g, obj, pmu_obj);
	if (status != 0) {
		return status;
	}

	pvin_dev = (struct clk_vin_device *)(void *)obj;
	perf_pmu_data = (struct nv_pmu_clk_clk_vin_device_boardobj_set *)
		pmu_obj;

	perf_pmu_data->id = pvin_dev->id;
	perf_pmu_data->volt_rail_idx =
			nvgpu_pmu_volt_rail_volt_domain_convert_to_idx(
					g, pvin_dev->volt_domain);
	perf_pmu_data->flls_shared_mask = pvin_dev->flls_shared_mask;
	perf_pmu_data->por_override_mode = pvin_dev->por_override_mode;
	perf_pmu_data->override_mode = pvin_dev->override_mode;

	nvgpu_log_info(g, " Done");

	return status;
}

int clk_pmu_vin_load(struct gk20a *g)
{
	int status;
	struct nvgpu_pmu *pmu = g->pmu;
	struct nv_pmu_rpc_struct_clk_load clk_load_rpc;

	(void) memset(&clk_load_rpc, 0,
			sizeof(struct nv_pmu_rpc_struct_clk_load));

	clk_load_rpc.clk_load.feature = NV_NV_PMU_CLK_LOAD_FEATURE_VIN;
	clk_load_rpc.clk_load.action_mask =
		NV_NV_PMU_CLK_LOAD_ACTION_MASK_VIN_HW_CAL_PROGRAM_YES << 4;

	/* Continue with PMU setup, assume FB map is done  */
	PMU_RPC_EXECUTE_CPB(status, pmu, CLK, LOAD, &clk_load_rpc, 0);
	if (status != 0) {
		nvgpu_err(g,
			"Failed to execute Clock Load RPC status=0x%x",
			status);
	}

	return status;
}

int clk_vin_init_pmupstate(struct gk20a *g)
{
	/* If already allocated, do not re-allocate */
	if (g->pmu->clk_pmu->avfs_vinobjs != NULL) {
		return 0;
	}

	g->pmu->clk_pmu->avfs_vinobjs = nvgpu_kzalloc(g,
			sizeof(*g->pmu->clk_pmu->avfs_vinobjs));
	if (g->pmu->clk_pmu->avfs_vinobjs == NULL) {
		return -ENOMEM;
	}

	return 0;
}

void clk_vin_free_pmupstate(struct gk20a *g)
{
	nvgpu_kfree(g, g->pmu->clk_pmu->avfs_vinobjs);
	g->pmu->clk_pmu->avfs_vinobjs = NULL;
}
