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

static void nvpgu_report_fill_err_info(u32 hw_unit,
		struct nvgpu_err_msg *err_pkt, struct gr_err_info *err_info)
{
	if (hw_unit == NVGPU_ERR_MODULE_SM) {
		struct gr_sm_mcerr_info *info = err_info->sm_mcerr_info;

		err_pkt->err_info.sm_info.warp_esr_pc =
			info->hww_warp_esr_pc;
		err_pkt->err_info.sm_info.warp_esr_status =
			info->hww_warp_esr_status;
		err_pkt->err_info.sm_info.curr_ctx =
			info->curr_ctx;
		err_pkt->err_info.sm_info.chid =
			info->chid;
		err_pkt->err_info.sm_info.tsgid =
			info->tsgid;
		err_pkt->err_info.sm_info.gpc =
			info->gpc;
		err_pkt->err_info.sm_info.tpc =
			info->tpc;
		err_pkt->err_info.sm_info.sm =
			info->sm;
	} else {
		struct gr_exception_info *info  = err_info->exception_info;

		err_pkt->err_info.gr_info.curr_ctx = info->curr_ctx;
		err_pkt->err_info.gr_info.chid = info->chid;
		err_pkt->err_info.gr_info.tsgid = info->tsgid;
		err_pkt->err_info.gr_info.status = info->status;
	}
}

void nvgpu_report_gr_err(struct gk20a *g, u32 hw_unit, u32 inst,
		u32 err_id, struct gr_err_info *err_info, u32 sub_err_type)
{
	int err = 0;
	struct nvgpu_err_desc *err_desc = NULL;
	struct nvgpu_err_msg err_pkt;

	if (g->ops.cic_mon.report_err == NULL) {
		cic_dbg(g, "CIC does not support reporting error "
			       "to safety services");
		return;
	}

	if ((hw_unit != NVGPU_ERR_MODULE_SM) &&
			(hw_unit != NVGPU_ERR_MODULE_PGRAPH)) {
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

	nvgpu_init_gr_err_msg(&err_pkt);
	err_pkt.hw_unit_id = hw_unit;
	err_pkt.err_id = err_desc->error_id;
	err_pkt.is_critical = err_desc->is_critical;
	err_pkt.err_desc = err_desc;
	err_pkt.err_info.gr_info.header.sub_err_type = sub_err_type;
	err_pkt.err_info.gr_info.header.sub_unit_id = inst;
	nvpgu_report_fill_err_info(hw_unit, &err_pkt, err_info);
	err_pkt.err_size = nvgpu_safe_cast_u64_to_u8(sizeof(err_pkt.err_info));

handle_report_failure:
	if (err != 0) {
		nvgpu_sw_quiesce(g);
	}
}

void nvgpu_inject_gr_swerror(struct gk20a *g, u32 hw_unit,
		u32 err_index, u32 sub_err_type)
{
	struct gr_err_info err_info;
	struct gr_exception_info gr_error_info;
	struct gr_sm_mcerr_info sm_error_info;
	int err = 0;
	u32 inst = 0U;

	/*
	 * Fill fixed test pattern data for the error message
	 * payload.
	 */
	(void)memset(&gr_error_info, ERR_INJECT_TEST_PATTERN, sizeof(gr_error_info));
	(void)memset(&sm_error_info, ERR_INJECT_TEST_PATTERN, sizeof(sm_error_info));

	switch (hw_unit) {
	case NVGPU_ERR_MODULE_PGRAPH:
		{
			err_info.exception_info = &gr_error_info;
		}
		break;

	case NVGPU_ERR_MODULE_SM:
		{
			err_info.sm_mcerr_info = &sm_error_info;
		}
		break;

	default:
		{
			nvgpu_err(g, "unsupported hw_unit(%u)", hw_unit);
			err = -EINVAL;
		}
		break;
	}
	if (err != 0) {
		return;
	}

	nvgpu_report_gr_err(g, hw_unit, inst, err_index,
			&err_info, sub_err_type);
}
