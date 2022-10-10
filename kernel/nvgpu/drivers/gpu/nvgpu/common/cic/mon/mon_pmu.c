/*
 * Copyright (c) 2021-2022, NVIDIA CORPORATION.  All rights reserved.
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
#include <nvgpu/nvgpu_init.h>
#include <nvgpu/nvgpu_err.h>
#include <nvgpu/nvgpu_err_info.h>
#include <nvgpu/cic_mon.h>

#include "cic_mon_priv.h"

void nvgpu_report_pmu_err(struct gk20a *g, u32 hw_unit, u32 err_id,
		u32 sub_err_type, u32 status)
{
	int err = 0;
	struct nvgpu_err_desc *err_desc = NULL;
	struct nvgpu_err_msg err_pkt;

	if (g->ops.cic_mon.report_err == NULL) {
		cic_dbg(g, "CIC does not support reporting error "
			       "to safety services");
		return;
	}

	if (hw_unit != NVGPU_ERR_MODULE_PMU) {
		nvgpu_err(g, "invalid hw module (%u)", hw_unit);
		err = -EINVAL;
		goto handle_report_failure;
	}

	err = nvgpu_cic_mon_get_err_desc(g, hw_unit, err_id, &err_desc);
	if (err != 0) {
		nvgpu_err(g, "Failed to get err_desc for "
				"err_id (%u) for hw module (%u)",
				err_id, hw_unit);
		goto handle_report_failure;
	}

	nvgpu_init_pmu_err_msg(&err_pkt);
	err_pkt.hw_unit_id = hw_unit;
	err_pkt.err_id = err_desc->error_id;
	err_pkt.is_critical = err_desc->is_critical;
	err_pkt.err_info.pmu_err_info.status = status;
	err_pkt.err_info.pmu_err_info.header.sub_err_type = sub_err_type;
	err_pkt.err_desc = err_desc;
	err_pkt.err_size = nvgpu_safe_cast_u64_to_u8(
			sizeof(err_pkt.err_info.pmu_err_info));

handle_report_failure:
	if (err != 0) {
		nvgpu_sw_quiesce(g);
	}
}

void nvgpu_inject_pmu_swerror(struct gk20a *g, u32 hw_unit,
		u32 err_index, u32 sub_err_type)
{
	u32 err_info;

	err_info = (u32)ERR_INJECT_TEST_PATTERN;

	nvgpu_report_pmu_err(g, hw_unit, err_index, sub_err_type, err_info);
}
