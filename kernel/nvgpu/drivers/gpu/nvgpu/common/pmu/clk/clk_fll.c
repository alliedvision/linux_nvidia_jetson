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

#include <nvgpu/bios.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/boardobjgrp.h>
#include <nvgpu/boardobjgrp_e32.h>
#include <nvgpu/pmu/boardobjgrp_classes.h>
#include <nvgpu/boardobjgrpmask.h>
#include <nvgpu/string.h>
#include <nvgpu/pmu/clk/clk.h>

#include "ucode_clk_inf.h"
#include "clk_fll.h"
#include "clk_vin.h"
#include "clk.h"

#define NV_PERF_DOMAIN_4X_CLOCK_DOMAIN_SKIP	0x10U
#define NV_PERF_DOMAIN_4X_CLOCK_DOMAIN_MASK	0x1FU

static int devinit_get_fll_device_table(struct gk20a *g,
		struct clk_avfs_fll_objs *pfllobjs);
static struct fll_device *construct_fll_device(struct gk20a *g,
		void *pargs);
static int fll_device_init_pmudata_super(struct gk20a *g,
		struct pmu_board_obj *obj,
		struct nv_pmu_boardobj *pmu_obj);

static u32 clk_get_vbios_clk_domain(u32 vbios_domain);

u8 clk_get_fll_lut_vf_num_entries(struct nvgpu_clk_pmupstate *pclk)
{
	return ((pclk)->avfs_fllobjs->lut_num_entries);
}

u32 nvgpu_pmu_clk_fll_get_lut_min_volt(struct nvgpu_clk_pmupstate *pclk)
{
	return ((pclk)->avfs_fllobjs->lut_min_voltage_uv);
}

u32 nvgpu_pmu_clk_fll_get_lut_step_size(struct nvgpu_clk_pmupstate *pclk)
{
	return ((pclk)->avfs_fllobjs->lut_step_size_uv);
}

static int _clk_fll_devgrp_pmudatainit_super(struct gk20a *g,
		struct boardobjgrp *pboardobjgrp,
		struct nv_pmu_boardobjgrp_super *pboardobjgrppmu)
{
	struct nv_pmu_clk_clk_fll_device_boardobjgrp_set_header *pset =
		(struct nv_pmu_clk_clk_fll_device_boardobjgrp_set_header *)
		pboardobjgrppmu;
	struct clk_avfs_fll_objs *pfll_objs = (struct clk_avfs_fll_objs *)
		pboardobjgrp;
	int status = 0;

	nvgpu_log_info(g, " ");

	status = boardobjgrp_pmudatainit_e32(g, pboardobjgrp, pboardobjgrppmu);
	if (status != 0) {
		nvgpu_err(g, "failed to init fll pmuobjgrp");
		return status;
	}
	pset->lut_num_entries = pfll_objs->lut_num_entries;
	pset->lut_step_size_uv = pfll_objs->lut_step_size_uv;
	pset->lut_min_voltage_uv = pfll_objs->lut_min_voltage_uv;
	pset->max_min_freq_mhz = pfll_objs->max_min_freq_mhz;

	status = nvgpu_boardobjgrpmask_export(
		&pfll_objs->lut_prog_master_mask.super,
		pfll_objs->lut_prog_master_mask.super.bitcount,
		&pset->lut_prog_master_mask.super);

	nvgpu_log_info(g, " Done");
	return status;
}

static int _clk_fll_devgrp_pmudata_instget(struct gk20a *g,
		struct nv_pmu_boardobjgrp *pmuboardobjgrp,
		struct nv_pmu_boardobj **pmu_obj, u8 idx)
{
	struct nv_pmu_clk_clk_fll_device_boardobj_grp_set  *pgrp_set =
		(struct nv_pmu_clk_clk_fll_device_boardobj_grp_set *)
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

static int _clk_fll_devgrp_pmustatus_instget(struct gk20a *g,
		void *pboardobjgrppmu,
		struct nv_pmu_boardobj_query **obj_pmu_status, u8 idx)
{
	struct nv_pmu_clk_clk_fll_device_boardobj_grp_get_status
	*pgrp_get_status =
		(struct nv_pmu_clk_clk_fll_device_boardobj_grp_get_status *)
		pboardobjgrppmu;

	/*check whether pmuboardobjgrp has a valid boardobj in index*/
	if (((u32)BIT(idx) &
		pgrp_get_status->hdr.data.super.obj_mask.super.data[0]) == 0U) {
		return -EINVAL;
	}

	*obj_pmu_status = (struct nv_pmu_boardobj_query *)
			&pgrp_get_status->objects[idx].data.obj;
	return 0;
}

int clk_fll_sw_setup(struct gk20a *g)
{
	int status;
	struct boardobjgrp *pboardobjgrp = NULL;
	struct clk_avfs_fll_objs *pfllobjs;
	struct fll_device *pfll;
	struct fll_device *pfll_master;
	struct fll_device *pfll_local;
	u8 i;
	u8 j;

	nvgpu_log_info(g, " ");

	status = nvgpu_boardobjgrp_construct_e32(g,
			&g->pmu->clk_pmu->avfs_fllobjs->super);
	if (status != 0) {
		nvgpu_err(g,
		"error creating boardobjgrp for fll, status - 0x%x", status);
		goto done;
	}
	pfllobjs = g->pmu->clk_pmu->avfs_fllobjs;
	pboardobjgrp = &(g->pmu->clk_pmu->avfs_fllobjs->super.super);

	BOARDOBJGRP_PMU_CONSTRUCT(pboardobjgrp, CLK, FLL_DEVICE);

	status = BOARDOBJGRP_PMU_CMD_GRP_SET_CONSTRUCT(g, pboardobjgrp,
			clk, CLK, clk_fll_device, CLK_FLL_DEVICE);
	if (status != 0) {
		nvgpu_err(g,
			  "error constructing PMU_BOARDOBJ_CMD_GRP_SET interface - 0x%x",
			  status);
		goto done;
	}

	pboardobjgrp->pmudatainit  = _clk_fll_devgrp_pmudatainit_super;
	pboardobjgrp->pmudatainstget  = _clk_fll_devgrp_pmudata_instget;
	pboardobjgrp->pmustatusinstget  = _clk_fll_devgrp_pmustatus_instget;
	pfllobjs = (struct clk_avfs_fll_objs *)pboardobjgrp;
	pfllobjs->lut_num_entries = g->ops.clk.lut_num_entries;
	pfllobjs->lut_step_size_uv = CTRL_CLK_VIN_STEP_SIZE_UV;
	pfllobjs->lut_min_voltage_uv = CTRL_CLK_LUT_MIN_VOLTAGE_UV;

	/* Initialize lut prog master mask to zero.*/
	status = boardobjgrpmask_e32_init(&pfllobjs->lut_prog_master_mask,
			NULL);
	if (status != 0) {
		nvgpu_err(g, "boardobjgrpmask_e32_init failed err=%d", status);
		goto done;
	}

	status = devinit_get_fll_device_table(g, pfllobjs);
	if (status != 0) {
		goto done;
	}

	status = BOARDOBJGRP_PMU_CMD_GRP_GET_STATUS_CONSTRUCT(g,
				&g->pmu->clk_pmu->avfs_fllobjs->super.super,
				clk, CLK, clk_fll_device, CLK_FLL_DEVICE);
	if (status != 0) {
		nvgpu_err(g,
			  "error constructing PMU_BOARDOBJ_CMD_GRP_SET interface - 0x%x",
			  status);
		goto done;
	}

	BOARDOBJGRP_FOR_EACH(&(pfllobjs->super.super),
			     struct fll_device *, pfll, i) {
		pfll_master = NULL;
		j = 0;
		BOARDOBJGRP_ITERATOR(&(pfllobjs->super.super),
				struct fll_device *, pfll_local, j,
				&pfllobjs->lut_prog_master_mask.super) {
			if (pfll_local->clk_domain == pfll->clk_domain) {
				pfll_master = pfll_local;
				break;
			}
		}

		if (pfll_master == NULL) {
			status = nvgpu_boardobjgrpmask_bit_set(
				&pfllobjs->lut_prog_master_mask.super,
				pmu_board_obj_get_idx(pfll));
			if (status != 0) {
				nvgpu_err(g, "err setting lutprogmask");
				goto done;
			}
			pfll_master = pfll;
		}
		status = pfll_master->lut_broadcast_slave_register(
			g, pfllobjs, pfll_master, pfll);

		if (status != 0) {
			nvgpu_err(g, "err setting lutslavemask");
			goto done;
		}
	}
done:
	nvgpu_log_info(g, " done status %x", status);
	return status;
}

int clk_fll_pmu_setup(struct gk20a *g)
{
	int status;
	struct boardobjgrp *pboardobjgrp = NULL;

	nvgpu_log_info(g, " ");

	pboardobjgrp = &g->pmu->clk_pmu->avfs_fllobjs->super.super;

	if (!pboardobjgrp->bconstructed) {
		return -EINVAL;
	}

	status = pboardobjgrp->pmuinithandle(g, pboardobjgrp);

	nvgpu_log_info(g, "Done");
	return status;
}

static int devinit_get_fll_device_table(struct gk20a *g,
		struct clk_avfs_fll_objs *pfllobjs)
{
	int status = 0;
	u8 *fll_table_ptr = NULL;
	struct fll_descriptor_header fll_desc_table_header_sz = { 0 };
	struct fll_descriptor_header_10 fll_desc_table_header = { 0 };
	struct fll_descriptor_entry_10 fll_desc_table_entry = { 0 };
	u8 *fll_tbl_entry_ptr = NULL;
	u32 index = 0;
	struct fll_device fll_dev_data;
	struct fll_device *pfll_dev;
	struct clk_vin_device *pvin_dev;
	u32 desctablesize;
	u32 vbios_domain = NV_PERF_DOMAIN_4X_CLOCK_DOMAIN_SKIP;
	struct nvgpu_avfsvinobjs *pvinobjs = g->pmu->clk_pmu->avfs_vinobjs;

	nvgpu_log_info(g, " ");

	fll_table_ptr = (u8 *)nvgpu_bios_get_perf_table_ptrs(g,
			  nvgpu_bios_get_bit_token(g, NVGPU_BIOS_CLOCK_TOKEN),
							FLL_TABLE);
	if (fll_table_ptr == NULL) {
		status = -1;
		goto done;
	}

	nvgpu_memcpy((u8 *)&fll_desc_table_header_sz, fll_table_ptr,
			sizeof(struct fll_descriptor_header));
	if (fll_desc_table_header_sz.size >= FLL_DESCRIPTOR_HEADER_10_SIZE_7) {
		desctablesize = FLL_DESCRIPTOR_HEADER_10_SIZE_7;
	} else {
		if (fll_desc_table_header_sz.size ==
				FLL_DESCRIPTOR_HEADER_10_SIZE_6) {
			desctablesize = FLL_DESCRIPTOR_HEADER_10_SIZE_6;
		} else {
			nvgpu_err(g, "Invalid FLL_DESCRIPTOR_HEADER size");
			return -EINVAL;
		}
	}

	nvgpu_memcpy((u8 *)&fll_desc_table_header, fll_table_ptr,
		desctablesize);

	pfllobjs->max_min_freq_mhz =
			fll_desc_table_header.max_min_freq_mhz;
	pfllobjs->freq_margin_vfe_idx =
			fll_desc_table_header.freq_margin_vfe_idx;

	/* Read table entries*/
	fll_tbl_entry_ptr = fll_table_ptr + desctablesize;
	for (index = 0; index < fll_desc_table_header.entry_count; index++) {
		u32 fll_id;

		nvgpu_memcpy((u8 *)&fll_desc_table_entry, fll_tbl_entry_ptr,
				sizeof(struct fll_descriptor_entry_10));

		if (fll_desc_table_entry.fll_device_type ==
				CTRL_CLK_FLL_TYPE_DISABLED) {
			continue;
		}

		fll_id = fll_desc_table_entry.fll_device_id;

		if ((u8)fll_desc_table_entry.vin_idx_logic !=
				CTRL_CLK_VIN_ID_UNDEFINED) {
			pvin_dev = clk_get_vin_from_index(pvinobjs,
					(u8)fll_desc_table_entry.vin_idx_logic);
			if (pvin_dev == NULL) {
				return -EINVAL;
			} else {
				pvin_dev->flls_shared_mask |= BIT32(fll_id);
			}
		} else {
			nvgpu_err(g, "Invalid Logic ID");
			return -EINVAL;
		}

		fll_dev_data.lut_device.vselect_mode =
			BIOS_GET_FIELD(u8, fll_desc_table_entry.lut_params,
				NV_FLL_DESC_LUT_PARAMS_VSELECT);

		if ((u8)fll_desc_table_entry.vin_idx_sram !=
				CTRL_CLK_VIN_ID_UNDEFINED) {
			pvin_dev = clk_get_vin_from_index(pvinobjs,
					(u8)fll_desc_table_entry.vin_idx_sram);
			if (pvin_dev == NULL) {
				return -EINVAL;
			} else {
				pvin_dev->flls_shared_mask |= BIT32(fll_id);
			}
		} else {
			/* Make sure VSELECT mode is set correctly to _LOGIC*/
			if (fll_dev_data.lut_device.vselect_mode !=
					CTRL_CLK_FLL_LUT_VSELECT_LOGIC) {
				return -EINVAL;
			}
		}

		fll_dev_data.super.type =
			(u8)fll_desc_table_entry.fll_device_type;
		fll_dev_data.id = (u8)fll_desc_table_entry.fll_device_id;
		fll_dev_data.mdiv = BIOS_GET_FIELD(u8,
			fll_desc_table_entry.fll_params,
				NV_FLL_DESC_FLL_PARAMS_MDIV);
		fll_dev_data.input_freq_mhz =
			(u16)fll_desc_table_entry.ref_freq_mhz;
		fll_dev_data.min_freq_vfe_idx =
			(u8)fll_desc_table_entry.min_freq_vfe_idx;
		fll_dev_data.freq_ctrl_idx = CTRL_BOARDOBJ_IDX_INVALID;

		vbios_domain = U32(fll_desc_table_entry.clk_domain) &
				U32(NV_PERF_DOMAIN_4X_CLOCK_DOMAIN_MASK);
		fll_dev_data.clk_domain =
				clk_get_vbios_clk_domain(vbios_domain);

		fll_dev_data.rail_idx_for_lut = 0;
		fll_dev_data.vin_idx_logic =
			(u8)fll_desc_table_entry.vin_idx_logic;
		fll_dev_data.vin_idx_sram =
			(u8)fll_desc_table_entry.vin_idx_sram;
		fll_dev_data.b_skip_pldiv_below_dvco_min =
			BIOS_GET_FIELD(bool, fll_desc_table_entry.fll_params,
			NV_FLL_DESC_FLL_PARAMS_SKIP_PLDIV_BELOW_DVCO_MIN);
		fll_dev_data.lut_device.hysteresis_threshold =
			BIOS_GET_FIELD(u16, fll_desc_table_entry.lut_params,
			NV_FLL_DESC_LUT_PARAMS_HYSTERISIS_THRESHOLD);
		fll_dev_data.regime_desc.regime_id =
			CTRL_CLK_FLL_REGIME_ID_FFR;
		fll_dev_data.regime_desc.fixed_freq_regime_limit_mhz =
			(u16)fll_desc_table_entry.ffr_cutoff_freq_mhz;
		if (fll_desc_table_entry.fll_device_type == 0x1U) {
			fll_dev_data.regime_desc.target_regime_id_override = 0U;
			fll_dev_data.b_dvco_1x = false;
		} else {
			fll_dev_data.regime_desc.target_regime_id_override =
				CTRL_CLK_FLL_REGIME_ID_FFR;
			fll_dev_data.b_dvco_1x = true;
		}

		/*construct fll device*/
		pfll_dev = construct_fll_device(g, (void *)&fll_dev_data);

		status = boardobjgrp_objinsert(&pfllobjs->super.super,
				(struct pmu_board_obj *)pfll_dev, index);
		fll_tbl_entry_ptr += fll_desc_table_header.entry_size;
	}

done:
	nvgpu_log_info(g, " done status %x", status);
	return status;
}

static u32 clk_get_vbios_clk_domain(u32 vbios_domain)
{
	if (vbios_domain == 0U) {
		return CTRL_CLK_DOMAIN_GPCCLK;
	} else if (vbios_domain == 1U) {
		return CTRL_CLK_DOMAIN_XBARCLK;
	} else if (vbios_domain == 3U) {
		return CTRL_CLK_DOMAIN_SYSCLK;
	} else if (vbios_domain == 5U) {
		return CTRL_CLK_DOMAIN_NVDCLK;
	} else if (vbios_domain == 9U) {
		return CTRL_CLK_DOMAIN_HOSTCLK;
	} else {
		return 0;
	}
}

static int lutbroadcastslaveregister(struct gk20a *g,
		struct clk_avfs_fll_objs *pfllobjs, struct fll_device *pfll,
		struct fll_device *pfll_slave)
{
	if (pfll->clk_domain != pfll_slave->clk_domain) {
		return -EINVAL;
	}

	return nvgpu_boardobjgrpmask_bit_set(&pfll->
		lut_prog_broadcast_slave_mask.super,
		pmu_board_obj_get_idx(pfll_slave));
}

static struct fll_device *construct_fll_device(struct gk20a *g,
		void *pargs)
{
	struct pmu_board_obj *obj = NULL;
	struct fll_device *pfll_dev;
	struct fll_device *board_obj_fll_ptr = NULL;
	int status;

	nvgpu_log_info(g, " ");

	board_obj_fll_ptr = nvgpu_kzalloc(g, sizeof(struct fll_device));
	if (board_obj_fll_ptr == NULL) {
		return NULL;
	}
	obj = (struct pmu_board_obj *)(void *)board_obj_fll_ptr;

	status = pmu_board_obj_construct_super(g, obj, pargs);
	if (status != 0) {
		return NULL;
	}

	pfll_dev = (struct fll_device *)pargs;
	obj->pmudatainit  = fll_device_init_pmudata_super;
	board_obj_fll_ptr->lut_broadcast_slave_register =
		lutbroadcastslaveregister;
	board_obj_fll_ptr->id = pfll_dev->id;
	board_obj_fll_ptr->mdiv = pfll_dev->mdiv;
	board_obj_fll_ptr->rail_idx_for_lut = pfll_dev->rail_idx_for_lut;
	board_obj_fll_ptr->input_freq_mhz = pfll_dev->input_freq_mhz;
	board_obj_fll_ptr->clk_domain = pfll_dev->clk_domain;
	board_obj_fll_ptr->vin_idx_logic = pfll_dev->vin_idx_logic;
	board_obj_fll_ptr->vin_idx_sram = pfll_dev->vin_idx_sram;
	board_obj_fll_ptr->min_freq_vfe_idx =
		pfll_dev->min_freq_vfe_idx;
	board_obj_fll_ptr->freq_ctrl_idx = pfll_dev->freq_ctrl_idx;
	board_obj_fll_ptr->b_skip_pldiv_below_dvco_min =
		pfll_dev->b_skip_pldiv_below_dvco_min;
	nvgpu_memcpy((u8 *)&board_obj_fll_ptr->lut_device,
		(u8 *)&pfll_dev->lut_device,
		sizeof(struct nv_pmu_clk_lut_device_desc));
	nvgpu_memcpy((u8 *)&board_obj_fll_ptr->regime_desc,
		(u8 *)&pfll_dev->regime_desc,
		sizeof(struct nv_pmu_clk_regime_desc));
	board_obj_fll_ptr->b_dvco_1x=pfll_dev->b_dvco_1x;

	status = boardobjgrpmask_e32_init(
		&board_obj_fll_ptr->lut_prog_broadcast_slave_mask, NULL);
	if (status != 0) {
		nvgpu_err(g, "boardobjgrpmask_e32_init failed err=%d", status);
		status = obj->destruct(obj);
		if (status != 0) {
			nvgpu_err(g, "destruct failed err=%d", status);
		}
		return NULL;
	}

	nvgpu_log_info(g, " Done");

	return (struct fll_device *)(void *)obj;
}

static int fll_device_init_pmudata_super(struct gk20a *g,
		struct pmu_board_obj *obj,
		struct nv_pmu_boardobj *pmu_obj)
{
	int status = 0;
	struct fll_device *pfll_dev;
	struct nv_pmu_clk_clk_fll_device_boardobj_set *perf_pmu_data;

	nvgpu_log_info(g, " ");

	status = pmu_board_obj_pmu_data_init_super(g, obj, pmu_obj);
	if (status != 0) {
		return status;
	}

	pfll_dev = (struct fll_device *)(void *)obj;
	perf_pmu_data = (struct nv_pmu_clk_clk_fll_device_boardobj_set *)
		pmu_obj;

	perf_pmu_data->id = pfll_dev->id;
	perf_pmu_data->mdiv = pfll_dev->mdiv;
	perf_pmu_data->rail_idx_for_lut = pfll_dev->rail_idx_for_lut;
	perf_pmu_data->input_freq_mhz = pfll_dev->input_freq_mhz;
	perf_pmu_data->vin_idx_logic = pfll_dev->vin_idx_logic;
	perf_pmu_data->vin_idx_sram = pfll_dev->vin_idx_sram;
	perf_pmu_data->clk_domain = pfll_dev->clk_domain;
	perf_pmu_data->min_freq_vfe_idx =
		pfll_dev->min_freq_vfe_idx;
	perf_pmu_data->freq_ctrl_idx = pfll_dev->freq_ctrl_idx;
	perf_pmu_data->b_skip_pldiv_below_dvco_min =
			pfll_dev->b_skip_pldiv_below_dvco_min;
	perf_pmu_data->b_dvco_1x = pfll_dev->b_dvco_1x;
	nvgpu_memcpy((u8 *)&perf_pmu_data->lut_device,
		(u8 *)&pfll_dev->lut_device,
		sizeof(struct nv_pmu_clk_lut_device_desc));
	nvgpu_memcpy((u8 *)&perf_pmu_data->regime_desc,
		(u8 *)&pfll_dev->regime_desc,
		sizeof(struct nv_pmu_clk_regime_desc));

	status = nvgpu_boardobjgrpmask_export(
		&pfll_dev->lut_prog_broadcast_slave_mask.super,
		pfll_dev->lut_prog_broadcast_slave_mask.super.bitcount,
		&perf_pmu_data->lut_prog_broadcast_slave_mask.super);

	nvgpu_log_info(g, " Done");

	return status;
}


u8 nvgpu_pmu_clk_fll_get_fmargin_idx(struct gk20a *g)
{
	struct clk_avfs_fll_objs *pfllobjs =  g->pmu->clk_pmu->avfs_fllobjs;
	u8 fmargin_idx;

	fmargin_idx = pfllobjs->freq_margin_vfe_idx;
	if (fmargin_idx == 255U) {
		return 0;
	}
	return fmargin_idx;
}

u16 nvgpu_pmu_clk_fll_get_min_max_freq(struct gk20a *g)
{
	if ((g->pmu->clk_pmu != NULL) &&
			(g->pmu->clk_pmu->avfs_fllobjs != NULL)) {
		return (g->pmu->clk_pmu->avfs_fllobjs->max_min_freq_mhz);
	}
	return 0;
}

int clk_fll_init_pmupstate(struct gk20a *g)
{
	/* If already allocated, do not re-allocate */
	if (g->pmu->clk_pmu->avfs_fllobjs != NULL) {
		return 0;
	}

	g->pmu->clk_pmu->avfs_fllobjs = nvgpu_kzalloc(g,
			sizeof(*g->pmu->clk_pmu->avfs_fllobjs));
	if (g->pmu->clk_pmu->avfs_fllobjs == NULL) {
		return -ENOMEM;
	}

	return 0;
}

void clk_fll_free_pmupstate(struct gk20a *g)
{
	nvgpu_kfree(g, g->pmu->clk_pmu->avfs_fllobjs);
	g->pmu->clk_pmu->avfs_fllobjs = NULL;
}
