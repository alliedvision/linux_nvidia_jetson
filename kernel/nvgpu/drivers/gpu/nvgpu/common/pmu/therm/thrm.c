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
#include <nvgpu/pmu/therm.h>
#include <nvgpu/boardobjgrp.h>

#include "thrm.h"

static void therm_unit_rpc_handler(struct gk20a *g, struct nvgpu_pmu *pmu,
		struct nv_pmu_rpc_header *rpc)
{
	switch (rpc->function) {
	case NV_PMU_RPC_ID_THERM_BOARD_OBJ_GRP_CMD:
		nvgpu_pmu_dbg(g,
			"reply NV_PMU_RPC_ID_THERM_BOARD_OBJ_GRP_CMD");
		break;
	default:
		nvgpu_pmu_dbg(g, "reply PMU_UNIT_THERM");
		break;
	}
}

int nvgpu_pmu_therm_sw_setup(struct gk20a *g, struct nvgpu_pmu *pmu)
{
	int status;

	status = therm_device_sw_setup(g);
	if (status != 0) {
		nvgpu_err(g,
			"error creating boardobjgrp for therm devices, status - 0x%x",
			status);
		goto exit;
	}

	status = therm_channel_sw_setup(g);
	if (status != 0) {
		nvgpu_err(g,
			"error creating boardobjgrp for therm channel, status - 0x%x",
			status);
		goto exit;
	}

	pmu->therm_rpc_handler = therm_unit_rpc_handler;

exit:
	return status;
}

int nvgpu_pmu_therm_pmu_setup(struct gk20a *g, struct nvgpu_pmu *pmu)
{
	int status;

	status = therm_device_pmu_setup(g);
	if (status != 0) {
		nvgpu_err(g, "Therm device pmu setup failed - 0x%x", status);
		goto exit;
	}

	status = therm_channel_pmu_setup(g);
	if (status != 0) {
		nvgpu_err(g,"Therm channel pmu setup failed - 0x%x", status);
		goto exit;
	}

exit:
	return status;
}

int nvgpu_pmu_therm_init(struct gk20a *g, struct nvgpu_pmu *pmu)
{
	/* If already allocated, do not re-allocate */
	if (pmu->therm_pmu != NULL) {
		return 0;
	}

	pmu->therm_pmu = nvgpu_kzalloc(g, sizeof(*(pmu->therm_pmu)));
	if (pmu->therm_pmu == NULL) {
		return -ENOMEM;
	}

	return 0;
}

void nvgpu_pmu_therm_deinit(struct gk20a *g, struct nvgpu_pmu *pmu)
{
	nvgpu_kfree(g, pmu->therm_pmu);
	pmu->therm_pmu = NULL;
}
