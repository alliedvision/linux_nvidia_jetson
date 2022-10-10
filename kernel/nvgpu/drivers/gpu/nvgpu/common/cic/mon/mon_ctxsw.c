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
#include <nvgpu/string.h>

#include "cic_mon_priv.h"

void nvgpu_report_ctxsw_err(struct gk20a *g, u32 hw_unit, u32 err_id,
		void *data)
{
	int err = 0;
	struct nvgpu_err_desc *err_desc = NULL;
	struct nvgpu_err_msg err_pkt;
	u32 inst = 0;
	struct ctxsw_err_info *err_info = (struct ctxsw_err_info *)data;

	if (g->ops.cic_mon.report_err == NULL) {
		cic_dbg(g, "CIC does not support reporting error "
			       "to safety services");
		return;
	}

	if (hw_unit != NVGPU_ERR_MODULE_FECS) {
		nvgpu_err(g, "invalid hw module (%u)", hw_unit);
		err = -EINVAL;
		goto handle_report_failure;
	}

	err = nvgpu_cic_mon_get_err_desc(g, hw_unit, err_id, &err_desc);
	if (err != 0) {
		nvgpu_err(g, "Failed to get err_desc for"
			       " err_id (%u) for hw module (%u)",
				err_id, hw_unit);
		goto handle_report_failure;
	}

	nvgpu_init_ctxsw_err_msg(&err_pkt);
	err_pkt.hw_unit_id = hw_unit;
	err_pkt.err_id = err_desc->error_id;
	err_pkt.is_critical = err_desc->is_critical;
	err_pkt.err_info.ctxsw_info.header.sub_unit_id = inst;
	err_pkt.err_info.ctxsw_info.curr_ctx = err_info->curr_ctx;
	err_pkt.err_info.ctxsw_info.chid = err_info->chid;
	err_pkt.err_info.ctxsw_info.ctxsw_status0 = err_info->ctxsw_status0;
	err_pkt.err_info.ctxsw_info.ctxsw_status1 = err_info->ctxsw_status1;
	err_pkt.err_info.ctxsw_info.mailbox_value = err_info->mailbox_value;
	err_pkt.err_desc = err_desc;
	err_pkt.err_size = nvgpu_safe_cast_u64_to_u8(
			sizeof(err_pkt.err_info.ctxsw_info));

handle_report_failure:
	if (err != 0) {
		nvgpu_sw_quiesce(g);
	}
}

void nvgpu_inject_ctxsw_swerror(struct gk20a *g, u32 hw_unit,
		u32 err_index, u32 inst)
{
	struct ctxsw_err_info err_info;

	(void)inst;

	(void)memset(&err_info, ERR_INJECT_TEST_PATTERN, sizeof(err_info));

	nvgpu_report_ctxsw_err(g, hw_unit, err_index, (void *)&err_info);
}
