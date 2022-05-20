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

#include <nvgpu/gk20a.h>
#include <nvgpu/boardobjgrp.h>
#include <nvgpu/boardobjgrp_e32.h>
#include <nvgpu/boardobjgrp_e255.h>
#include <nvgpu/pmu/boardobjgrp_classes.h>
#include <nvgpu/string.h>
#include <nvgpu/timers.h>
#include <nvgpu/pmu/clk/clk.h>
#include <nvgpu/clk_arb.h>
#include <nvgpu/pmu/volt.h>
#include <nvgpu/pmu/perf.h>
#include <nvgpu/pmu/cmd.h>

#include "ucode_clk_inf.h"
#include "clk_vf_point.h"
#include "clk.h"

int nvgpu_clk_domain_volt_to_freq(struct gk20a *g, u8 clkdomain_idx,
	u32 *pclkmhz, u32 *pvoltuv, u8 railidx)
{
	struct nv_pmu_rpc_clk_domain_35_prog_freq_to_volt  rpc;
	struct nvgpu_pmu *pmu = g->pmu;
	int status = -EINVAL;

	(void)memset(&rpc, 0,
		sizeof(struct nv_pmu_rpc_clk_domain_35_prog_freq_to_volt));
	rpc.volt_rail_idx =
		nvgpu_pmu_volt_rail_volt_domain_convert_to_idx(g, railidx);
	rpc.clk_domain_idx = clkdomain_idx;
	rpc.voltage_type = CTRL_VOLT_DOMAIN_LOGIC;
	rpc.input.value = *pvoltuv;
	PMU_RPC_EXECUTE_CPB(status, pmu, CLK,
			CLK_DOMAIN_35_PROG_VOLT_TO_FREQ, &rpc, 0);
	if (status != 0) {
		nvgpu_err(g, "Failed to execute Freq to Volt RPC status=0x%x",
			status);
	}
	*pclkmhz = rpc.output.value;
	return status;
}

static int _clk_vf_point_pmudatainit_super(struct gk20a *g, struct pmu_board_obj
	*obj,	struct nv_pmu_boardobj *pmu_obj);

static int _clk_vf_points_pmudatainit(struct gk20a *g,
		struct boardobjgrp *pboardobjgrp,
		struct nv_pmu_boardobjgrp_super *pboardobjgrppmu)
{
	int status = 0;

	status = boardobjgrp_pmudatainit_e32(g, pboardobjgrp, pboardobjgrppmu);
	if (status != 0) {
		nvgpu_err(g,
			  "error updating pmu boardobjgrp for clk vfpoint 0x%x",
			  status);
		goto done;
	}

done:
	return status;
}

static int _clk_vf_points_pmudata_instget(struct gk20a *g,
		struct nv_pmu_boardobjgrp *pmuboardobjgrp,
		struct nv_pmu_boardobj **pmu_obj, u8 idx)
{
	struct nv_pmu_clk_clk_vf_point_boardobj_grp_set  *pgrp_set =
		(struct nv_pmu_clk_clk_vf_point_boardobj_grp_set *)
		pmuboardobjgrp;

	nvgpu_log_info(g, " ");

	/*check whether pmuboardobjgrp has a valid boardobj in index*/
	if (idx >= CTRL_BOARDOBJGRP_E255_MAX_OBJECTS) {
		return -EINVAL;
	}

	*pmu_obj = (struct nv_pmu_boardobj *)
		&pgrp_set->objects[idx].data.obj;
	nvgpu_log_info(g, " Done");
	return 0;
}

static int _clk_vf_points_pmustatus_instget(struct gk20a *g,
		void *pboardobjgrppmu,
		struct nv_pmu_boardobj_query **obj_pmu_status, u8 idx)
{
	struct nv_pmu_clk_clk_vf_point_boardobj_grp_get_status
	*pgrp_get_status =
		(struct nv_pmu_clk_clk_vf_point_boardobj_grp_get_status *)
		pboardobjgrppmu;

	/*check whether pmuboardobjgrp has a valid boardobj in index*/
	if (idx >= CTRL_BOARDOBJGRP_E255_MAX_OBJECTS) {
		return -EINVAL;
	}

	*obj_pmu_status = (struct nv_pmu_boardobj_query *)(void *)
			&pgrp_get_status->objects[idx].data.obj;
	return 0;
}

int clk_vf_point_sw_setup(struct gk20a *g)
{
	int status;
	struct boardobjgrp *pboardobjgrp = NULL;

	nvgpu_log_info(g, " ");

	status = nvgpu_boardobjgrp_construct_e255(g,
			&g->pmu->clk_pmu->clk_vf_pointobjs->super);
	if (status != 0) {
		nvgpu_err(g,
		"error creating boardobjgrp for clk vfpoint, status - 0x%x",
		status);
		goto done;
	}

	pboardobjgrp = &g->pmu->clk_pmu->clk_vf_pointobjs->super.super;

	BOARDOBJGRP_PMU_CONSTRUCT(pboardobjgrp, CLK, CLK_VF_POINT);

	status = BOARDOBJGRP_PMU_CMD_GRP_SET_CONSTRUCT(g, pboardobjgrp,
			clk, CLK, clk_vf_point, CLK_VF_POINT);
	if (status != 0) {
		nvgpu_err(g,
			"error constructing PMU_BOARDOBJ_CMD_GRP_SET - 0x%x",
			status);
		goto done;
	}

	status = BOARDOBJGRP_PMU_CMD_GRP_GET_STATUS_CONSTRUCT(g,
			&g->pmu->clk_pmu->clk_vf_pointobjs->super.super,
			clk, CLK, clk_vf_point, CLK_VF_POINT);
	if (status != 0) {
		nvgpu_err(g,
			"error constructing PMU_BOARDOBJ_CMD_GRP_SET - 0x%x",
			status);
		goto done;
	}

	pboardobjgrp->pmudatainit = _clk_vf_points_pmudatainit;
	pboardobjgrp->pmudatainstget  = _clk_vf_points_pmudata_instget;
	pboardobjgrp->pmustatusinstget  = _clk_vf_points_pmustatus_instget;

done:
	nvgpu_log_info(g, " done status %x", status);
	return status;
}

int clk_vf_point_pmu_setup(struct gk20a *g)
{
	int status;
	struct boardobjgrp *pboardobjgrp = NULL;

	nvgpu_log_info(g, " ");

	pboardobjgrp = &g->pmu->clk_pmu->clk_vf_pointobjs->super.super;

	if (!pboardobjgrp->bconstructed) {
		return -EINVAL;
	}

	status = pboardobjgrp->pmuinithandle(g, pboardobjgrp);

	nvgpu_log_info(g, "Done");
	return status;
}

static int clk_vf_point_construct_super(struct gk20a *g,
		struct pmu_board_obj **obj, size_t size, void *pargs)
{
	struct clk_vf_point *pclkvfpoint;
	struct clk_vf_point *ptmpvfpoint =
			(struct clk_vf_point *)pargs;
	int status = 0;

	pclkvfpoint = nvgpu_kzalloc(g, size);
	if (pclkvfpoint == NULL) {
		return -ENOMEM;
	}

	status = pmu_board_obj_construct_super(g,
			(struct pmu_board_obj *)(void *)pclkvfpoint, pargs);
	if (status != 0) {
		return -EINVAL;
	}

	*obj = (struct pmu_board_obj *)(void *)pclkvfpoint;

	pclkvfpoint->super.pmudatainit =
			_clk_vf_point_pmudatainit_super;

	pclkvfpoint->vfe_equ_idx = ptmpvfpoint->vfe_equ_idx;
	pclkvfpoint->volt_rail_idx = ptmpvfpoint->volt_rail_idx;

	return status;
}

static int _clk_vf_point_pmudatainit_volt(struct gk20a *g,
		struct pmu_board_obj *obj,
		struct nv_pmu_boardobj *pmu_obj)
{
	int status = 0;
	struct clk_vf_point_volt *pclk_vf_point_volt;
	struct nv_pmu_clk_clk_vf_point_volt_boardobj_set *pset;

	nvgpu_log_info(g, " ");

	status = _clk_vf_point_pmudatainit_super(g, obj, pmu_obj);
	if (status != 0) {
		return status;
	}

	pclk_vf_point_volt =
		(struct clk_vf_point_volt *)(void *)obj;

	pset = (struct nv_pmu_clk_clk_vf_point_volt_boardobj_set *)
		pmu_obj;

	pset->source_voltage_uv = pclk_vf_point_volt->source_voltage_uv;
	pset->freq_delta.data = pclk_vf_point_volt->freq_delta.data;
	pset->freq_delta.type = pclk_vf_point_volt->freq_delta.type;

	return status;
}

static int _clk_vf_point_pmudatainit_freq(struct gk20a *g,
		struct pmu_board_obj *obj,
		struct nv_pmu_boardobj *pmu_obj)
{
	int status = 0;
	struct clk_vf_point_freq *pclk_vf_point_freq;
	struct nv_pmu_clk_clk_vf_point_freq_boardobj_set *pset;

	nvgpu_log_info(g, " ");

	status = _clk_vf_point_pmudatainit_super(g, obj, pmu_obj);
	if (status != 0) {
		return status;
	}

	pclk_vf_point_freq =
		(struct clk_vf_point_freq *)(void *)obj;

	pset = (struct nv_pmu_clk_clk_vf_point_freq_boardobj_set *)
		pmu_obj;

	pset->freq_mhz = pclk_vf_point_freq->super.pair.freq_mhz;

	pset->volt_delta_uv = pclk_vf_point_freq->volt_delta_uv;

	return status;
}

static int clk_vf_point_construct_volt_35(struct gk20a *g,
		struct pmu_board_obj **obj, size_t size, void *pargs)
{
	struct pmu_board_obj *obj_tmp = (struct pmu_board_obj *)pargs;
	struct clk_vf_point_volt *pclkvfpoint;
	struct clk_vf_point_volt *ptmpvfpoint =
			(struct clk_vf_point_volt *)pargs;
	int status = 0;

	if (pmu_board_obj_get_type(pargs) !=
			CTRL_CLK_CLK_VF_POINT_TYPE_35_VOLT_PRI) {
		return -EINVAL;
	}

	obj_tmp->type_mask = (u32) BIT(CTRL_CLK_CLK_VF_POINT_TYPE_35_VOLT_PRI);
	status = clk_vf_point_construct_super(g, obj, size, pargs);
	if (status != 0) {
		return -EINVAL;
	}

	pclkvfpoint = (struct clk_vf_point_volt *) (void *) *obj;

	pclkvfpoint->super.super.pmudatainit =
			_clk_vf_point_pmudatainit_volt;

	pclkvfpoint->source_voltage_uv = ptmpvfpoint->source_voltage_uv;
	pclkvfpoint->freq_delta = ptmpvfpoint->freq_delta;

	return status;
}

static int clk_vf_point_construct_freq_35(struct gk20a *g,
		struct pmu_board_obj **obj, size_t size, void *pargs)
{
	struct pmu_board_obj *obj_tmp = (struct pmu_board_obj *)pargs;
	struct clk_vf_point_freq *pclkvfpoint;
	struct clk_vf_point_freq *ptmpvfpoint =
			(struct clk_vf_point_freq *)pargs;
	int status = 0;

	if (pmu_board_obj_get_type(pargs) != CTRL_CLK_CLK_VF_POINT_TYPE_35_FREQ) {
		return -EINVAL;
	}

	obj_tmp->type_mask = (u32) BIT(CTRL_CLK_CLK_VF_POINT_TYPE_35_FREQ);
	status = clk_vf_point_construct_super(g, obj, size, pargs);
	if (status != 0) {
		return -EINVAL;
	}

	pclkvfpoint = (struct clk_vf_point_freq *)(void *) *obj;

	pclkvfpoint->super.super.pmudatainit =
			_clk_vf_point_pmudatainit_freq;

	pclkvfpoint->super.pair.freq_mhz = ptmpvfpoint->super.pair.freq_mhz;

	return status;
}

struct clk_vf_point *nvgpu_construct_clk_vf_point(struct gk20a *g, void *pargs)
{
	struct pmu_board_obj *obj = NULL;
	int status;

	nvgpu_log_info(g, " ");
	switch (pmu_board_obj_get_type(pargs)) {

	case CTRL_CLK_CLK_VF_POINT_TYPE_35_FREQ:
		status = clk_vf_point_construct_freq_35(g, &obj,
			sizeof(struct clk_vf_point_freq), pargs);
		break;

	case CTRL_CLK_CLK_VF_POINT_TYPE_35_VOLT_PRI:
		status = clk_vf_point_construct_volt_35(g, &obj,
			sizeof(struct clk_vf_point_volt), pargs);
		break;

	default:
		status = -EINVAL;
		break;
	}

	if (status != 0) {
		return NULL;
	}

	nvgpu_log_info(g, " Done");

	return (struct clk_vf_point *)(void *)obj;
}

static int _clk_vf_point_pmudatainit_super(struct gk20a *g,
		struct pmu_board_obj *obj,
		struct nv_pmu_boardobj *pmu_obj)
{
	int status = 0;
	struct clk_vf_point *pclk_vf_point;
	struct nv_pmu_clk_clk_vf_point_boardobj_set *pset;

	nvgpu_log_info(g, " ");

	status = pmu_board_obj_pmu_data_init_super(g, obj, pmu_obj);
	if (status != 0) {
		return status;
	}

	pclk_vf_point =
		(struct clk_vf_point *)(void *)obj;

	pset = (struct nv_pmu_clk_clk_vf_point_boardobj_set *)
		pmu_obj;


	pset->vfe_equ_idx = pclk_vf_point->vfe_equ_idx;
	pset->volt_rail_idx = pclk_vf_point->volt_rail_idx;
	return status;
}

#ifdef CONFIG_NVGPU_CLK_ARB
int nvgpu_clk_arb_find_slave_points(struct nvgpu_clk_arb *arb,
		struct nvgpu_clk_slave_freq *vf_point)
{

	u16 gpc2clk_target;
	struct nvgpu_clk_vf_table *table;
	u32 index;
	int status = 0;
	do {
		gpc2clk_target = vf_point->gpc_mhz;

		table = NV_READ_ONCE(arb->current_vf_table);
		/* pointer to table can be updated by callback */
		nvgpu_smp_rmb();

		if (table == NULL) {
			continue;
		}
		if ((table->gpc2clk_num_points == 0U)) {
			nvgpu_err(arb->g, "found empty table");
			status = -EINVAL; ;
		}

		/* round up the freq requests */
		for (index = 0; index < table->gpc2clk_num_points; index++) {
			if ((table->gpc2clk_points[index].gpc_mhz >=
							gpc2clk_target)) {
				gpc2clk_target =
					table->gpc2clk_points[index].gpc_mhz;
				vf_point->sys_mhz =
					table->gpc2clk_points[index].sys_mhz;
				vf_point->xbar_mhz =
					table->gpc2clk_points[index].xbar_mhz;
				vf_point->nvd_mhz =
					table->gpc2clk_points[index].nvd_mhz;
				vf_point->host_mhz =
					table->gpc2clk_points[index].host_mhz;
				break;
			}
		}
		/*
		 * If the requested freq is lower than available
		 * one in VF table, use the VF table freq
		 */
		if (gpc2clk_target > vf_point->gpc_mhz) {
			vf_point->gpc_mhz = gpc2clk_target;
		}
	} while ((table == NULL) ||
		(NV_READ_ONCE(arb->current_vf_table) != table));

	return status;

}

/*get latest vf point data from PMU */
int nvgpu_clk_vf_point_cache(struct gk20a *g)
{
	struct nvgpu_clk_vf_points *pclk_vf_points;
	struct boardobjgrp *pboardobjgrp;
	struct pmu_board_obj *obj = NULL;
	int status;
	struct clk_vf_point *pclk_vf_point;
	u8 index;
	u32 voltage_min_uv,voltage_step_size_uv;
	u32 gpcclk_clkmhz=0, gpcclk_voltuv=0;

	nvgpu_log_info(g, " ");
	pclk_vf_points = g->pmu->clk_pmu->clk_vf_pointobjs;
	pboardobjgrp = &pclk_vf_points->super.super;

	voltage_min_uv = nvgpu_pmu_clk_fll_get_lut_min_volt(g->pmu->clk_pmu);
	voltage_step_size_uv =
			nvgpu_pmu_clk_fll_get_lut_step_size(g->pmu->clk_pmu);
	BOARDOBJGRP_FOR_EACH(pboardobjgrp, struct pmu_board_obj*, obj, index) {
		pclk_vf_point = (struct clk_vf_point *)(void *)obj;
		gpcclk_voltuv =
				voltage_min_uv + index * voltage_step_size_uv;
		status = nvgpu_clk_domain_volt_to_freq(g, 0, &gpcclk_clkmhz,
				&gpcclk_voltuv, CTRL_VOLT_DOMAIN_LOGIC);
		if (status != 0) {
			nvgpu_err(g,
				"Failed to get freq for requested voltage");
			return status;
		}

		pclk_vf_point->pair.freq_mhz = (u16)gpcclk_clkmhz;
		pclk_vf_point->pair.voltage_uv = gpcclk_voltuv;
	}
	return status;
}
#endif

int clk_vf_point_init_pmupstate(struct gk20a *g)
{
	/* If already allocated, do not re-allocate */
	if (g->pmu->clk_pmu->clk_vf_pointobjs != NULL) {
		return 0;
	}

	g->pmu->clk_pmu->clk_vf_pointobjs = nvgpu_kzalloc(g,
			sizeof(*g->pmu->clk_pmu->clk_vf_pointobjs));
	if (g->pmu->clk_pmu->clk_vf_pointobjs == NULL) {
		return -ENOMEM;
	}

	return 0;
}

void clk_vf_point_free_pmupstate(struct gk20a *g)
{
	nvgpu_kfree(g, g->pmu->clk_pmu->clk_vf_pointobjs);
	g->pmu->clk_pmu->clk_vf_pointobjs = NULL;
}
