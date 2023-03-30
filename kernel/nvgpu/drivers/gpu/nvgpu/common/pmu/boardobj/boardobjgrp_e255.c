/*
 * Copyright (c) 2016-2019, NVIDIA CORPORATION.  All rights reserved.
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
#include <nvgpu/boardobjgrp_e255.h>

static int boardobjgrp_pmu_hdr_data_init_e255(struct gk20a *g,
		struct boardobjgrp *pboardobjgrp,
		struct nv_pmu_boardobjgrp_super *pboardobjgrppmu,
		struct boardobjgrpmask *mask)
{
	struct nv_pmu_boardobjgrp_e255 *pgrpe255 =
		(struct nv_pmu_boardobjgrp_e255 *)(void *)pboardobjgrppmu;
	int status;

	nvgpu_log_info(g, " ");

	if (pboardobjgrp == NULL) {
		return -EINVAL;
	}

	if (pboardobjgrppmu == NULL) {
		return -EINVAL;
	}

	status = nvgpu_boardobjgrpmask_export(mask,
				mask->bitcount,
				&pgrpe255->obj_mask.super);
	if (status != 0) {
		nvgpu_err(g, "e255 init:failed export grpmask");
		return status;
	}

	return nvgpu_boardobjgrp_pmu_hdr_data_init_super(g,
			pboardobjgrp, pboardobjgrppmu, mask);
}

int nvgpu_boardobjgrp_construct_e255(struct gk20a *g,
			      struct boardobjgrp_e255 *pboardobjgrp_e255)
{
	int status = 0;
	u8  objslots;

	nvgpu_log_info(g, " ");

	objslots = 255;
	status = boardobjgrpmask_e255_init(&pboardobjgrp_e255->mask, NULL);
	if (status != 0) {
		goto nvgpu_boardobjgrpconstruct_e255_exit;
	}

	pboardobjgrp_e255->super.type      = CTRL_BOARDOBJGRP_TYPE_E255;
	pboardobjgrp_e255->super.ppobjects = pboardobjgrp_e255->objects;
	pboardobjgrp_e255->super.objslots  = objslots;
	pboardobjgrp_e255->super.mask     = &(pboardobjgrp_e255->mask.super);

	status = nvgpu_boardobjgrp_construct_super(g, &pboardobjgrp_e255->super);
	if (status != 0) {
		goto nvgpu_boardobjgrpconstruct_e255_exit;
	}

	pboardobjgrp_e255->super.pmuhdrdatainit =
		boardobjgrp_pmu_hdr_data_init_e255;

nvgpu_boardobjgrpconstruct_e255_exit:
	return status;
}

