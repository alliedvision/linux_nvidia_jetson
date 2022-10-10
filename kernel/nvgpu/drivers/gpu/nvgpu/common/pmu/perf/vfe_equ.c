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
#include <nvgpu/boardobjgrp.h>
#include <nvgpu/boardobjgrp_e32.h>
#include <nvgpu/boardobjgrp_e255.h>
#include <nvgpu/boardobjgrpmask.h>
#include <nvgpu/pmu/boardobjgrp_classes.h>
#include <nvgpu/string.h>
#include <nvgpu/pmu/clk/clk.h>
#include <nvgpu/pmu/perf.h>
#include <nvgpu/pmu/cmd.h>

#include "ucode_perf_vfe_inf.h"
#include "vfe_equ.h"
#include "vfe_var.h"
#include "perf.h"

static int vfe_equ_node_depending_mask_combine(struct gk20a *g,
		struct boardobjgrp *pboardobjgrp, u8 equ_idx,
		struct boardobjgrpmask *pmask_dst)
{
	int status = 0;
	struct vfe_equ *tmp_vfe_equ;

	while (equ_idx != CTRL_BOARDOBJ_IDX_INVALID) {
		tmp_vfe_equ = (struct vfe_equ *)(void *)
				BOARDOBJGRP_OBJ_GET_BY_IDX(
						pboardobjgrp, equ_idx);
		status = tmp_vfe_equ->mask_depending_build(g, pboardobjgrp ,
				tmp_vfe_equ);
		if (status != 0) {
			nvgpu_err(g, " Failed calling vfeequ[%d].mskdpningbld",
					equ_idx);
			return status;
		}

		status = nvgpu_boardobjmask_or(pmask_dst, pmask_dst,
				&(tmp_vfe_equ->mask_depending_vars.super));
		if (status != 0) {
			nvgpu_err(g, " Failed calling vfeequ boardobjmask_or");
			return status;
		}

		equ_idx = tmp_vfe_equ->equ_idx_next;
	}
	return status;
}

static int vfe_equ_build_depending_mask_minmax(struct gk20a *g,
		struct boardobjgrp *pboardobjgrp,
		struct vfe_equ *pvfe_equ)
{
	struct vfe_equ_minmax *pequ_mm =
			(struct vfe_equ_minmax *)(void *)pvfe_equ;
	int status;

	status = vfe_equ_node_depending_mask_combine(g, pboardobjgrp,
			pequ_mm->equ_idx0, &pvfe_equ->mask_depending_vars.super);
	if (status != 0) {
		nvgpu_err(g, " Failed calling depending_mask_combine for idx0");
		return status;
	}

	status = vfe_equ_node_depending_mask_combine(g, pboardobjgrp,
			pequ_mm->equ_idx1, &pvfe_equ->mask_depending_vars.super);
	if (status != 0) {
		nvgpu_err(g, " Failed calling depending_mask_combine for idx1");
		return status;
	}

	return status;
}

static int vfe_equ_build_depending_mask_super(struct gk20a *g,
		struct vfe_equ *pvfe_equ)
{
	struct vfe_var *tmp_vfe_var;
	struct boardobjgrp *pboardobjgrp =
			&g->pmu->perf_pmu->vfe_varobjs.super.super;

	tmp_vfe_var = (struct vfe_var *)(void *)BOARDOBJGRP_OBJ_GET_BY_IDX(
			pboardobjgrp, pvfe_equ->var_idx);

	pvfe_equ->mask_depending_vars = tmp_vfe_var->mask_depending_vars;

	return 0;
}

static int vfe_equ_build_depending_mask_compare(struct gk20a *g,
		struct boardobjgrp *pboardobjgrp,
		struct vfe_equ *pvfe_equ)
{
	struct vfe_equ_compare *pequ_cmp =
			(struct vfe_equ_compare *)(void *)pvfe_equ;
	int status;

	status = vfe_equ_build_depending_mask_super(g, pvfe_equ);
	if (status != 0) {
		nvgpu_err(g, " Failed calling depending_mask_super");
		return status;
	}

	status = vfe_equ_node_depending_mask_combine(g, pboardobjgrp,
			pequ_cmp->equ_idx_true,
			&pvfe_equ->mask_depending_vars.super);
	if (status != 0) {
		nvgpu_err(g, " Failed calling depending_mask_combine for idx1");
		return status;
	}

	status = vfe_equ_node_depending_mask_combine(g, pboardobjgrp,
			pequ_cmp->equ_idx_false,
			&pvfe_equ->mask_depending_vars.super);
	if (status != 0) {
		nvgpu_err(g, " Failed calling depending_mask_combine for idx1");
		return status;
	}

	return status;
}

static int vfe_equ_build_depending_mask_quad(struct gk20a *g,
		struct boardobjgrp *pboardobjgrp,
		struct vfe_equ *pvfe_equ)
{
	(void)pboardobjgrp;
	return vfe_equ_build_depending_mask_super(g, pvfe_equ);
}

static int vfe_equ_build_depending_mask_equ_scalar(struct gk20a *g,
		struct boardobjgrp *pboardobjgrp,
		struct vfe_equ *pvfe_equ)
{
	struct vfe_equ_scalar *pequ_escalar =
			(struct vfe_equ_scalar *)(void *)pvfe_equ;
	int status;

	status = vfe_equ_build_depending_mask_super(g, pvfe_equ);
	if (status != 0) {
		nvgpu_err(g, " Failed calling depending_mask_super");
		return status;
	}

	status = vfe_equ_node_depending_mask_combine(g, pboardobjgrp,
			pequ_escalar->equ_idx_to_scale,
			&pvfe_equ->mask_depending_vars.super);
	if (status != 0) {
		nvgpu_err(g, " Failed calling depending_mask_combine for idx1");
		return status;
	}

	return status;
}

static int vfe_equ_dependency_mask_build(struct gk20a *g,
		struct vfe_equs *pvfe_equs, struct vfe_vars *pvfe_vars)
{
	int status = 0;
	struct vfe_equ *tmp_vfe_equ;
	struct vfe_var *tmp_vfe_var;
	u8 index_1, index_2;
	struct pmu_board_obj *obj_tmp_1 = NULL, *obj_tmp_2 = NULL;
	struct boardobjgrp *pboardobjgrp_equ = &(pvfe_equs->super.super);
	struct boardobjgrp *pboardobjgrp_var = &(pvfe_vars->super.super);

	/* Initialize mask_depending_vars */
	BOARDOBJGRP_FOR_EACH(pboardobjgrp_equ, struct pmu_board_obj*,
			obj_tmp_1, index_1) {
		tmp_vfe_equ = (struct vfe_equ *)(void *)obj_tmp_1;
		status = tmp_vfe_equ->mask_depending_build(g, pboardobjgrp_equ,
				tmp_vfe_equ);
		if (status != 0) {
			nvgpu_err(g, "failure in calling vfeequ[%d].depmskbld",
					index_1);
			return status;
		}
	}
	/* Initialize mask_dependent_vars */
	BOARDOBJGRP_FOR_EACH(pboardobjgrp_equ, struct pmu_board_obj*,
			obj_tmp_1, index_1) {
		tmp_vfe_equ = (struct vfe_equ *)(void *)obj_tmp_1;
		BOARDOBJGRP_ITERATOR(pboardobjgrp_var, struct pmu_board_obj*,
				obj_tmp_2, index_2,
				&tmp_vfe_equ->mask_depending_vars.super) {
			tmp_vfe_var = (struct vfe_var *)(void *)obj_tmp_2;
			status = nvgpu_boardobjgrpmask_bit_set(
					&tmp_vfe_var->mask_dependent_equs.super,
					index_1);
			if (status != 0) {
				nvgpu_err(g, "failing boardobjgrpmask_bit_set");
				return status;
			}
		}
	}
	return status;

}

static int vfe_equs_pmudatainit(struct gk20a *g,
				 struct boardobjgrp *pboardobjgrp,
				 struct nv_pmu_boardobjgrp_super *pboardobjgrppmu)
{
	int status = 0;

	status = boardobjgrp_pmu_data_init_e255(g, pboardobjgrp, pboardobjgrppmu);
	if (status != 0) {
		nvgpu_err(g, "error updating pmu boardobjgrp for vfe equ 0x%x",
			  status);
		goto done;
	}

done:
	return status;
}

static int vfe_equs_pmudata_instget(struct gk20a *g,
				     struct nv_pmu_boardobjgrp *pmuboardobjgrp,
				     struct nv_pmu_boardobj **pmu_obj,
				     u8 idx)
{
	struct nv_pmu_perf_vfe_equ_boardobj_grp_set  *pgrp_set =
		(struct nv_pmu_perf_vfe_equ_boardobj_grp_set *)(void *)pmuboardobjgrp;

	/* check whether pmuboardobjgrp has a valid boardobj in index */
	if (idx >= CTRL_BOARDOBJGRP_E255_MAX_OBJECTS) {
		return -EINVAL;
	}

	*pmu_obj = (struct nv_pmu_boardobj *)
		&pgrp_set->objects[idx].data.obj;
	nvgpu_log_info(g, " Done");
	return 0;
}



static int vfe_equ_pmudatainit_super(struct gk20a *g,
				      struct pmu_board_obj *obj,
				      struct nv_pmu_boardobj *pmu_obj)
{
	int status = 0;
	struct vfe_equ *pvfe_equ;
	struct nv_pmu_vfe_equ *pset;

	status = pmu_board_obj_pmu_data_init_super(g, obj, pmu_obj);
	if (status != 0) {
		return status;
	}

	pvfe_equ = (struct vfe_equ *)(void *)obj;

	pset = (struct nv_pmu_vfe_equ *)(void *)
		pmu_obj;

	pset->var_idx      = pvfe_equ->var_idx;
	pset->equ_idx_next  = pvfe_equ->equ_idx_next;
	pset->output_type  = pvfe_equ->output_type;
	pset->out_range_min = pvfe_equ->out_range_min;
	pset->out_range_max = pvfe_equ->out_range_max;

	return status;
}

static int vfe_equ_construct_super(struct gk20a *g,
				   struct pmu_board_obj **obj,
				   size_t size, void *pargs)
{
	struct vfe_equ *pvfeequ;
	struct vfe_equ *ptmpequ = (struct vfe_equ *)pargs;
	int status = 0;

	pvfeequ = nvgpu_kzalloc(g, size);
	if (pvfeequ == NULL) {
		return -ENOMEM;
	}

	status = pmu_board_obj_construct_super(g,
			(struct pmu_board_obj *)(void *)pvfeequ, pargs);
	if (status != 0) {
		return -EINVAL;
	}

	*obj = (struct pmu_board_obj *)(void *)pvfeequ;

	status = boardobjgrpmask_e32_init(&pvfeequ->mask_depending_vars, NULL);
	pvfeequ->super.pmudatainit =
			vfe_equ_pmudatainit_super;

	pvfeequ->var_idx = ptmpequ->var_idx;
	pvfeequ->equ_idx_next = ptmpequ->equ_idx_next;
	pvfeequ->output_type = ptmpequ->output_type;
	pvfeequ->out_range_min = ptmpequ->out_range_min;
	pvfeequ->out_range_max = ptmpequ->out_range_max;

	return status;
}

static int vfe_equ_pmudatainit_compare(struct gk20a *g,
					struct pmu_board_obj *obj,
					struct nv_pmu_boardobj *pmu_obj)
{
	int status = 0;
	struct vfe_equ_compare *pvfe_equ_compare;
	struct nv_pmu_vfe_equ_compare *pset;

	status = vfe_equ_pmudatainit_super(g, obj, pmu_obj);
	if (status != 0) {
		return status;
	}

	pvfe_equ_compare = (struct vfe_equ_compare *)(void *)obj;

	pset = (struct nv_pmu_vfe_equ_compare *)(void *)pmu_obj;

	pset->func_id = pvfe_equ_compare->func_id;
	pset->equ_idx_true = pvfe_equ_compare->equ_idx_true;
	pset->equ_idx_false = pvfe_equ_compare->equ_idx_false;
	pset->criteria = pvfe_equ_compare->criteria;

	return status;
}


static int vfe_equ_construct_compare(struct gk20a *g,
				     struct pmu_board_obj **obj,
				     size_t size, void *pargs)
{
	struct pmu_board_obj *obj_tmp = (struct pmu_board_obj *)pargs;
	struct vfe_equ_compare *pvfeequ;
	struct vfe_equ_compare *ptmpequ =
			(struct vfe_equ_compare *)pargs;
	int status = 0;

	if (pmu_board_obj_get_type(pargs) != CTRL_PERF_VFE_EQU_TYPE_COMPARE) {
		return -EINVAL;
	}

	obj_tmp->type_mask |= (u32)BIT(CTRL_PERF_VFE_EQU_TYPE_COMPARE);
	status = vfe_equ_construct_super(g, obj, size, pargs);
	if (status != 0) {
		return -EINVAL;
	}

	pvfeequ = (struct vfe_equ_compare *)(void *)*obj;
	pvfeequ->super.mask_depending_build =
			vfe_equ_build_depending_mask_compare;
	pvfeequ->super.super.pmudatainit =
			vfe_equ_pmudatainit_compare;

	pvfeequ->func_id = ptmpequ->func_id;
	pvfeequ->equ_idx_true = ptmpequ->equ_idx_true;
	pvfeequ->equ_idx_false = ptmpequ->equ_idx_false;
	pvfeequ->criteria = ptmpequ->criteria;


	return status;
}

static int vfe_equ_pmudatainit_minmax(struct gk20a *g,
				       struct pmu_board_obj *obj,
				       struct nv_pmu_boardobj *pmu_obj)
{
	int status = 0;
	struct vfe_equ_minmax *pvfe_equ_minmax;
	struct nv_pmu_vfe_equ_minmax *pset;

	status = vfe_equ_pmudatainit_super(g, obj, pmu_obj);
	if (status != 0) {
		return status;
	}

	pvfe_equ_minmax = (struct vfe_equ_minmax *)(void *)obj;

	pset = (struct nv_pmu_vfe_equ_minmax *)(void *)
		pmu_obj;

	pset->b_max = pvfe_equ_minmax->b_max;
	pset->equ_idx0 = pvfe_equ_minmax->equ_idx0;
	pset->equ_idx1 = pvfe_equ_minmax->equ_idx1;

	return status;
}

static int vfe_equ_construct_minmax(struct gk20a *g,
				    struct pmu_board_obj **obj,
				    size_t size, void *pargs)
{
	struct pmu_board_obj *obj_tmp = (struct pmu_board_obj *)pargs;
	struct vfe_equ_minmax *pvfeequ;
	struct vfe_equ_minmax *ptmpequ =
			(struct vfe_equ_minmax *)pargs;
	int status = 0;

	if (pmu_board_obj_get_type(pargs) != CTRL_PERF_VFE_EQU_TYPE_MINMAX) {
		return -EINVAL;
	}

	obj_tmp->type_mask |= (u32)BIT(CTRL_PERF_VFE_EQU_TYPE_MINMAX);
	status = vfe_equ_construct_super(g, obj, size, pargs);
	if (status != 0) {
		return -EINVAL;
	}

	pvfeequ = (struct vfe_equ_minmax *)(void *)*obj;
	pvfeequ->super.mask_depending_build =
			vfe_equ_build_depending_mask_minmax;
	pvfeequ->super.super.pmudatainit =
			vfe_equ_pmudatainit_minmax;
	pvfeequ->b_max = ptmpequ->b_max;
	pvfeequ->equ_idx0 = ptmpequ->equ_idx0;
	pvfeequ->equ_idx1 = ptmpequ->equ_idx1;

	return status;
}

static int vfe_equ_pmudatainit_quadratic(struct gk20a *g,
					  struct pmu_board_obj *obj,
					  struct nv_pmu_boardobj *pmu_obj)
{
	int status = 0;
	struct vfe_equ_quadratic *pvfe_equ_quadratic;
	struct nv_pmu_vfe_equ_quadratic *pset;
	u32 i;

	status = vfe_equ_pmudatainit_super(g, obj, pmu_obj);
	if (status != 0) {
		return status;
	}

	pvfe_equ_quadratic = (struct vfe_equ_quadratic *)(void *)obj;

	pset = (struct nv_pmu_vfe_equ_quadratic *)(void *)pmu_obj;

	for (i = 0; i < CTRL_PERF_VFE_EQU_QUADRATIC_COEFF_COUNT; i++) {
		pset->coeffs[i] = pvfe_equ_quadratic->coeffs[i];
	}

	return status;
}

static int vfe_equ_construct_quadratic(struct gk20a *g,
				       struct pmu_board_obj **obj,
				       size_t size, void *pargs)
{
	struct pmu_board_obj *obj_tmp = (struct pmu_board_obj *)pargs;
	struct vfe_equ_quadratic *pvfeequ;
	struct vfe_equ_quadratic *ptmpequ =
			(struct vfe_equ_quadratic *)pargs;
	int status = 0;
	u32 i;

	if (pmu_board_obj_get_type(pargs) != CTRL_PERF_VFE_EQU_TYPE_QUADRATIC) {
		return -EINVAL;
	}

	obj_tmp->type_mask |= (u32)BIT(CTRL_PERF_VFE_EQU_TYPE_QUADRATIC);
	status = vfe_equ_construct_super(g, obj, size, pargs);
	if (status != 0) {
		return -EINVAL;
	}

	pvfeequ = (struct vfe_equ_quadratic *)(void *)*obj;
	pvfeequ->super.mask_depending_build =
			vfe_equ_build_depending_mask_quad;

	pvfeequ->super.super.pmudatainit =
			vfe_equ_pmudatainit_quadratic;

	for (i = 0; i < CTRL_PERF_VFE_EQU_QUADRATIC_COEFF_COUNT; i++) {
		pvfeequ->coeffs[i] = ptmpequ->coeffs[i];
	}

	return status;
}

static int vfe_equ_pmudatainit_scalar(struct gk20a *g,
				       struct pmu_board_obj *obj,
				       struct nv_pmu_boardobj *pmu_obj)
{
	int status = 0;
	struct vfe_equ_scalar *pvfe_equ_scalar;
	struct nv_pmu_vfe_equ_scalar *pset;

	status = vfe_equ_pmudatainit_super(g, obj, pmu_obj);
	if (status != 0) {
		return status;
	}

	pvfe_equ_scalar = (struct vfe_equ_scalar *)(void *)obj;

	pset = (struct nv_pmu_vfe_equ_scalar *)(void *)
		pmu_obj;

	pset->equ_idx_to_scale = pvfe_equ_scalar->equ_idx_to_scale;

	return status;
}

static int vfe_equ_construct_scalar(struct gk20a *g,
				       struct pmu_board_obj **obj,
				       size_t size, void *pargs)
{
	struct pmu_board_obj *obj_tmp = (struct pmu_board_obj *)pargs;
	struct vfe_equ_scalar *pvfeequ;
	struct vfe_equ_scalar *ptmpequ =
			(struct vfe_equ_scalar *)pargs;
	int status = 0;

	if (pmu_board_obj_get_type(pargs) != CTRL_PERF_VFE_EQU_TYPE_SCALAR) {
		return -EINVAL;
	}

	obj_tmp->type_mask |= (u32)BIT(CTRL_PERF_VFE_EQU_TYPE_SCALAR);
	status = vfe_equ_construct_super(g, obj, size, pargs);
	if (status != 0) {
		return -EINVAL;
	}

	pvfeequ = (struct vfe_equ_scalar *)(void *)*obj;
	pvfeequ->super.mask_depending_build =
			vfe_equ_build_depending_mask_equ_scalar;

	pvfeequ->super.super.pmudatainit =
			vfe_equ_pmudatainit_scalar;

	pvfeequ->equ_idx_to_scale = ptmpequ->equ_idx_to_scale;

	return status;
}

static struct vfe_equ *construct_vfe_equ(struct gk20a *g, void *pargs)
{
	struct pmu_board_obj *obj = NULL;
	int status;

	switch (pmu_board_obj_get_type(pargs)) {
	case CTRL_PERF_VFE_EQU_TYPE_COMPARE:
		status = vfe_equ_construct_compare(g, &obj,
			sizeof(struct vfe_equ_compare), pargs);
		break;

	case CTRL_PERF_VFE_EQU_TYPE_MINMAX:
		status = vfe_equ_construct_minmax(g, &obj,
			sizeof(struct vfe_equ_minmax), pargs);
		break;

	case CTRL_PERF_VFE_EQU_TYPE_QUADRATIC:
		status = vfe_equ_construct_quadratic(g, &obj,
			sizeof(struct vfe_equ_quadratic), pargs);
		break;

	case CTRL_PERF_VFE_EQU_TYPE_SCALAR:
		status = vfe_equ_construct_scalar(g, &obj,
			sizeof(struct vfe_equ_scalar), pargs);
		break;

	default:
		status = -EINVAL;
		break;
	}

	if (status != 0) {
		return NULL;
	}

	nvgpu_log_info(g, " Done");

	return (struct vfe_equ *)(void *)obj;
}

static int devinit_get_vfe_equ_table(struct gk20a *g,
				     struct vfe_equs *pvfeequobjs)
{
	int status = 0;
	u8 *vfeequs_tbl_ptr = NULL;
	struct vbios_vfe_3x_header_struct vfeequs_tbl_header = { 0 };
	struct vbios_vfe_3x_equ_entry_struct equ = { 0 };
	u8 *vfeequs_tbl_entry_ptr = NULL;
	u8 *rd_offset_ptr = NULL;
	u32 index = 0;
	struct vfe_equ *pequ;
	u8 equ_type = 0;
	u32 szfmt;
	bool done = false;
	u32 hdrszfmt = 0;
	union {
		struct pmu_board_obj obj;
		struct vfe_equ super;
		struct vfe_equ_compare compare;
		struct vfe_equ_minmax minmax;
		struct vfe_equ_quadratic quadratic;
		struct vfe_equ_scalar scalar;
	} equ_data;

	vfeequs_tbl_ptr = (u8 *)nvgpu_bios_get_perf_table_ptrs(g,
			nvgpu_bios_get_bit_token(g, NVGPU_BIOS_PERF_TOKEN),
			CONTINUOUS_VIRTUAL_BINNING_TABLE);

	if (vfeequs_tbl_ptr == NULL) {
		status = -EINVAL;
		goto done;
	}

	nvgpu_memcpy((u8 *)&vfeequs_tbl_header, vfeequs_tbl_ptr,
			VBIOS_VFE_3X_HEADER_SIZE_09);
	if (vfeequs_tbl_header.header_size == VBIOS_VFE_3X_HEADER_SIZE_09) {
		hdrszfmt = VBIOS_VFE_3X_HEADER_SIZE_09;
		nvgpu_memcpy((u8 *)&vfeequs_tbl_header, vfeequs_tbl_ptr, hdrszfmt);
	} else {
		nvgpu_err(g, "Invalid VFE Table Header size\n");
		status = -EINVAL;
		goto done;
	}

	if (vfeequs_tbl_header.vfe_equ_entry_size ==
			VBIOS_VFE_3X_EQU_ENTRY_SIZE_18) {
		szfmt = VBIOS_VFE_3X_EQU_ENTRY_SIZE_18;
	} else {
		nvgpu_err(g, "Invalid VFE EQU entry size\n");
		status = -EINVAL;
		goto done;
	}

	vfeequs_tbl_entry_ptr = vfeequs_tbl_ptr + hdrszfmt +
		(vfeequs_tbl_header.vfe_var_entry_count *
		 vfeequs_tbl_header.vfe_var_entry_size);

	for (index = 0;
	     index < vfeequs_tbl_header.vfe_equ_entry_count;
	     index++) {
		(void) memset(&equ, 0,
			sizeof(struct vbios_vfe_3x_equ_entry_struct));

		rd_offset_ptr = vfeequs_tbl_entry_ptr +
			(index * vfeequs_tbl_header.vfe_equ_entry_size);

		nvgpu_memcpy((u8 *)&equ, rd_offset_ptr, szfmt);

		equ_data.super.var_idx = (u8)equ.var_idx;
		equ_data.super.equ_idx_next =
			(equ.equ_idx_next == VBIOS_VFE_3X_EQU_ENTRY_IDX_INVALID) ?
			CTRL_BOARDOBJ_IDX_INVALID : (u8)equ.equ_idx_next;
		equ_data.super.out_range_min = equ.out_range_min;
		equ_data.super.out_range_max = equ.out_range_max;

		switch (BIOS_GET_FIELD(u32, equ.param3,
				VBIOS_VFE_3X_EQU_ENTRY_PAR3_OUTPUT_TYPE)) {
		case VBIOS_VFE_3X_EQU_ENTRY_PAR3_OUTPUT_TYPE_UNITLESS:
			equ_data.super.output_type =
				(u8)CTRL_PERF_VFE_EQU_OUTPUT_TYPE_UNITLESS;
			break;

		case VBIOS_VFE_3X_EQU_ENTRY_PAR3_OUTPUT_TYPE_FREQ_MHZ:
			equ_data.super.output_type =
				(u8)CTRL_PERF_VFE_EQU_OUTPUT_TYPE_FREQ_MHZ;
			break;

		case VBIOS_VFE_3X_EQU_ENTRY_PAR3_OUTPUT_TYPE_VOLT_UV:
			equ_data.super.output_type =
				(u8)CTRL_PERF_VFE_EQU_OUTPUT_TYPE_VOLT_UV;
			break;

		case VBIOS_VFE_3X_EQU_ENTRY_PAR3_OUTPUT_TYPE_VF_GAIN:
			equ_data.super.output_type =
				(u8)CTRL_PERF_VFE_EQU_OUTPUT_TYPE_VF_GAIN;
			break;

		case VBIOS_VFE_3X_EQU_ENTRY_PAR3_OUTPUT_TYPE_VOLT_DELTA_UV:
			equ_data.super.output_type =
				(u8)CTRL_PERF_VFE_EQU_OUTPUT_TYPE_VOLT_DELTA_UV;
			break;

		case VBIOS_VFE_3X_EQU_ENTRY_PAR3_OUTPUT_TYPE_WORK_TYPE:
			equ_data.super.output_type =
				(u8)CTRL_PERF_VFE_EQU_OUTPUT_TYPE_WORK_TYPE;
			break;

		case VBIOS_VFE_3X_EQU_ENTRY_PAR3_OUTPUT_TYPE_UTIL_RATIO:
			equ_data.super.output_type =
				(u8)CTRL_PERF_VFE_EQU_OUTPUT_TYPE_UTIL_RATIO;
			break;

		case VBIOS_VFE_3X_EQU_ENTRY_PAR3_OUTPUT_TYPE_WORK_FB_NORM:
			equ_data.super.output_type =
				(u8)CTRL_PERF_VFE_EQU_OUTPUT_TYPE_WORK_FB_NORM;
			break;

		case VBIOS_VFE_3X_EQU_ENTRY_PAR3_OUTPUT_TYPE_POWER_MW:
			equ_data.super.output_type =
				(u8)CTRL_PERF_VFE_EQU_OUTPUT_TYPE_POWER_MW;
			break;

		case VBIOS_VFE_3X_EQU_ENTRY_PAR3_OUTPUT_TYPE_PWR_OVER_UTIL_SLOPE:
			equ_data.super.output_type =
				(u8)CTRL_PERF_VFE_EQU_OUTPUT_TYPE_PWR_OVER_UTIL_SLOPE;
			break;

		case VBIOS_VFE_3X_EQU_ENTRY_PAR3_OUTPUT_TYPE_VIN_CODE:
			equ_data.super.output_type =
				(u8)CTRL_PERF_VFE_EQU_OUTPUT_TYPE_VIN_CODE;
			break;
		case VBIOS_VFE_3X_EQU_ENTRY_PAR3_OUTPUT_TYPE_THRESHOLD:
			equ_data.super.output_type =
				(u8)VBIOS_VFE_3X_EQU_ENTRY_PAR3_OUTPUT_TYPE_THRESHOLD;
			break;

		default:
			nvgpu_err(g, "unrecognized output id @vfeequ index %d",
				  index);
			done = true;
			break;
		}
		/*
		 * Previously we were doing "goto done" from the default case of
		 * the switch-case block above. MISRA however, gets upset about
		 * this because it wants a break statement in the default case.
		 * That's why we had to move the goto statement outside of the
		 * switch-case block.
		 */
		if (done) {
			goto done;
		}

		switch ((u8)equ.type) {
		case VBIOS_VFE_3X_EQU_ENTRY_TYPE_DISABLED:
		case VBIOS_VFE_3X_EQU_ENTRY_TYPE_QUADRATIC_FXP:
		case VBIOS_VFE_3X_EQU_ENTRY_TYPE_MINMAX_FXP:
			continue;
			break;

		case VBIOS_VFE_3X_EQU_ENTRY_TYPE_QUADRATIC:
			equ_type = (u8)CTRL_PERF_VFE_EQU_TYPE_QUADRATIC;
			equ_data.quadratic.coeffs[0] = equ.param0;
			equ_data.quadratic.coeffs[1] = equ.param1;
			equ_data.quadratic.coeffs[2] = equ.param2;
			break;

		case VBIOS_VFE_3X_EQU_ENTRY_TYPE_MINMAX:
			equ_type = (u8)CTRL_PERF_VFE_EQU_TYPE_MINMAX;
			equ_data.minmax.b_max = BIOS_GET_FIELD(bool, equ.param0,
				VBIOS_VFE_3X_EQU_ENTRY_PAR0_MINMAX_CRIT) &&
				(VBIOS_VFE_3X_EQU_ENTRY_PAR0_MINMAX_CRIT_MAX != 0U);
			equ_data.minmax.equ_idx0 = BIOS_GET_FIELD(u8,
				equ.param0,
				VBIOS_VFE_3X_EQU_ENTRY_PAR0_MINMAX_VFE_EQU_IDX_0);
			equ_data.minmax.equ_idx1 = BIOS_GET_FIELD(u8,
				equ.param0,
				VBIOS_VFE_3X_EQU_ENTRY_PAR0_MINMAX_VFE_EQU_IDX_1);
			break;

		case VBIOS_VFE_3X_EQU_ENTRY_TYPE_COMPARE:
		{
			u8 cmp_func = BIOS_GET_FIELD(u8, equ.param1,
				VBIOS_VFE_3X_EQU_ENTRY_PAR1_COMPARE_FUNCTION);
			equ_type = (u8)CTRL_PERF_VFE_EQU_TYPE_COMPARE;

			switch (cmp_func) {
			case VBIOS_VFE_3X_EQU_ENTRY_PAR1_COMPARE_FUNCTION_EQUAL:
				equ_data.compare.func_id =
					(u8)CTRL_PERF_VFE_EQU_COMPARE_FUNCTION_EQUAL;
				break;

			case VBIOS_VFE_3X_EQU_ENTRY_PAR1_COMPARE_FUNCTION_GREATER_EQ:
				equ_data.compare.func_id =
					(u8)CTRL_PERF_VFE_EQU_COMPARE_FUNCTION_GREATER_EQ;
				break;
			case VBIOS_VFE_3X_EQU_ENTRY_PAR1_COMPARE_FUNCTION_GREATER:
				equ_data.compare.func_id =
					(u8)CTRL_PERF_VFE_EQU_COMPARE_FUNCTION_GREATER;
				break;
			default:
				nvgpu_err(g,
					  "invalid vfe compare index %x type %x ",
					  index, cmp_func);
				status = -EINVAL;
				break;
			}
			if (status != 0) {
				goto done;
			}
			equ_data.compare.equ_idx_true = BIOS_GET_FIELD(u8,
				equ.param1,
				VBIOS_VFE_3X_EQU_ENTRY_PAR1_COMPARE_VFE_EQU_IDX_TRUE);
			equ_data.compare.equ_idx_false = BIOS_GET_FIELD(u8,
				equ.param1,
				VBIOS_VFE_3X_EQU_ENTRY_PAR1_COMPARE_VFE_EQU_IDX_FALSE);
			equ_data.compare.criteria = equ.param0;
			break;
		}

		case VBIOS_VFE_3X_EQU_ENTRY_TYPE_EQUATION_SCALAR:
		{
			equ_type = (u8)CTRL_PERF_VFE_EQU_TYPE_SCALAR;
			equ_data.scalar.equ_idx_to_scale =
				BIOS_GET_FIELD(u8, equ.param0,
					VBIOS_VFE_3X_EQU_ENTRY_PAR0_EQUATION_SCALAR_IDX_TO_SCALE);
			break;
		}

		default:
			status = -EINVAL;
			nvgpu_err(g, "Invalid equ[%d].type = 0x%x.",
				index, (u8)equ.type);
			break;
		}
		if (status != 0) {
			goto done;
		}

		equ_data.obj.type = equ_type;
		pequ = construct_vfe_equ(g, (void *)&equ_data);

		if (pequ == NULL) {
			nvgpu_err(g,
			"error constructing vfe_equ boardobj %d", index);
			status = -EINVAL;
			goto done;
		}

		status = boardobjgrp_objinsert(&pvfeequobjs->super.super,
					       (struct pmu_board_obj *)pequ, (u8)index);
		if (status != 0) {
			nvgpu_err(g, "error adding vfe_equ boardobj %d", index);
			status = -EINVAL;
			goto done;
		}
	}
done:
	nvgpu_log_info(g, " done status %x", status);
	return status;
}

int perf_vfe_equ_sw_setup(struct gk20a *g)
{
	int status;
	struct boardobjgrp *pboardobjgrp = NULL;
	struct vfe_equs *pvfeequobjs;
	struct vfe_vars *pvfevarobjs;

	status = nvgpu_boardobjgrp_construct_e255(g,
			&g->pmu->perf_pmu->vfe_equobjs.super);
	if (status != 0) {
		nvgpu_err(g,
			  "error creating boardobjgrp for clk domain, "
			  "status - 0x%x", status);
		goto done;
	}

	pboardobjgrp = &g->pmu->perf_pmu->vfe_equobjs.super.super;
	pvfeequobjs = &(g->pmu->perf_pmu->vfe_equobjs);
	pvfevarobjs = &(g->pmu->perf_pmu->vfe_varobjs);

	BOARDOBJGRP_PMU_CONSTRUCT(pboardobjgrp, PERF, VFE_EQU);

	status = BOARDOBJGRP_PMU_CMD_GRP_SET_CONSTRUCT(g, pboardobjgrp,
			perf, PERF, vfe_equ, VFE_EQU);
	if (status != 0) {
		nvgpu_err(g,
			"error constructing PMU_BOARDOBJ_CMD_GRP_SET interface - 0x%x",
			status);
		goto done;
	}

	pboardobjgrp->pmudatainit  = vfe_equs_pmudatainit;
	pboardobjgrp->pmudatainstget  = vfe_equs_pmudata_instget;

	status = devinit_get_vfe_equ_table(g, pvfeequobjs);
	if (status != 0) {
		goto done;
	}

	status = vfe_equ_dependency_mask_build(g, pvfeequobjs, pvfevarobjs);
	if (status != 0) {
		goto done;
	}

done:
	nvgpu_log_info(g, " done status %x", status);
	return status;
}

int perf_vfe_equ_pmu_setup(struct gk20a *g)
{
	int status;
	struct boardobjgrp *pboardobjgrp = NULL;

	pboardobjgrp = &g->pmu->perf_pmu->vfe_equobjs.super.super;

	if (!pboardobjgrp->bconstructed) {
		return -EINVAL;
	}

	status = pboardobjgrp->pmuinithandle(g, pboardobjgrp);

	nvgpu_log_info(g, "Done");
	return status;
}

int nvgpu_pmu_perf_vfe_get_volt_margin(struct gk20a *g, u32 *vmargin_uv)
{
	struct nvgpu_pmu *pmu = g->pmu;
	struct nv_pmu_rpc_struct_perf_vfe_eval rpc;
	int status = 0;
	u8 vmargin_idx;

	vmargin_idx = nvgpu_pmu_volt_get_vmargin_ps35(g);
	if (vmargin_idx == 0U) {
		return 0;
	}

	(void) memset(&rpc, 0, sizeof(rpc));
	rpc.data.equ_idx = vmargin_idx;
	rpc.data.output_type = CTRL_PERF_VFE_EQU_OUTPUT_TYPE_VOLT_DELTA_UV;
	rpc.data.var_count = 0U;
	PMU_RPC_EXECUTE_CPB(status, pmu, PERF, VFE_EQU_EVAL, &rpc, 0);
	if (status != 0) {
		nvgpu_err(g, "Failed to execute RPC status=0x%x",
			status);
		return status;
	}

	*vmargin_uv = rpc.data.result.voltu_v;
	return status;
}

int nvgpu_pmu_perf_vfe_get_freq_margin(struct gk20a *g, u32 *fmargin_mhz)
{
	struct nvgpu_pmu *pmu = g->pmu;
	struct nv_pmu_rpc_struct_perf_vfe_eval rpc;
	int status = 0;
	u8 fmargin_idx;

	fmargin_idx = nvgpu_pmu_clk_fll_get_fmargin_idx(g);
	if (fmargin_idx == 0U) {
		return 0;
	}

	(void) memset(&rpc, 0, sizeof(rpc));
	rpc.data.equ_idx = fmargin_idx;
	rpc.data.output_type = CTRL_PERF_VFE_EQU_OUTPUT_TYPE_FREQ_MHZ;
	rpc.data.var_count = 0U;
	PMU_RPC_EXECUTE_CPB(status, pmu, PERF, VFE_EQU_EVAL, &rpc, 0);
	if (status != 0) {
		nvgpu_err(g, "Failed to execute RPC status=0x%x",
			status);
		return status;
	}

	*fmargin_mhz = rpc.data.result.voltu_v;
	return status;
}
