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

void nvgpu_report_mmu_err(struct gk20a *g, u32 hw_unit, u32 err_id,
		struct mmu_fault_info *fault_info, u32 status, u32 sub_err_type)
{
	int err = 0;
	struct nvgpu_err_desc *err_desc = NULL;
	struct nvgpu_err_msg err_pkt;

	if (g->ops.cic_mon.report_err == NULL) {
		cic_dbg(g, "CIC does not support reporting error "
			       "to safety services");
		return;
	}

	if (hw_unit != NVGPU_ERR_MODULE_HUBMMU) {
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

	nvgpu_init_mmu_err_msg(&err_pkt);
	err_pkt.hw_unit_id = hw_unit;
	err_pkt.err_id = err_desc->error_id;
	err_pkt.is_critical = err_desc->is_critical;
	err_pkt.err_info.mmu_info.header.sub_err_type = sub_err_type;
	err_pkt.err_info.mmu_info.status = status;
	/* Copy contents of mmu_fault_info */
	if (fault_info != NULL) {
		err_pkt.err_info.mmu_info.info.inst_ptr = fault_info->inst_ptr;
		err_pkt.err_info.mmu_info.info.inst_aperture
			= fault_info->inst_aperture;
		err_pkt.err_info.mmu_info.info.fault_addr
			= fault_info->fault_addr;
		err_pkt.err_info.mmu_info.info.fault_addr_aperture
			= fault_info->fault_addr_aperture;
		err_pkt.err_info.mmu_info.info.timestamp_lo
			= fault_info->timestamp_lo;
		err_pkt.err_info.mmu_info.info.timestamp_hi
			= fault_info->timestamp_hi;
		err_pkt.err_info.mmu_info.info.mmu_engine_id
			= fault_info->mmu_engine_id;
		err_pkt.err_info.mmu_info.info.gpc_id = fault_info->gpc_id;
		err_pkt.err_info.mmu_info.info.client_type
			= fault_info->client_type;
		err_pkt.err_info.mmu_info.info.client_id
			= fault_info->client_id;
		err_pkt.err_info.mmu_info.info.fault_type
			= fault_info->fault_type;
		err_pkt.err_info.mmu_info.info.access_type
			= fault_info->access_type;
		err_pkt.err_info.mmu_info.info.protected_mode
			= fault_info->protected_mode;
		err_pkt.err_info.mmu_info.info.replayable_fault
			= fault_info->replayable_fault;
		err_pkt.err_info.mmu_info.info.replay_fault_en
			= fault_info->replay_fault_en;
		err_pkt.err_info.mmu_info.info.valid = fault_info->valid;
		err_pkt.err_info.mmu_info.info.faulted_pbdma =
			fault_info->faulted_pbdma;
		err_pkt.err_info.mmu_info.info.faulted_engine =
			fault_info->faulted_engine;
		err_pkt.err_info.mmu_info.info.faulted_subid =
			fault_info->faulted_subid;
		err_pkt.err_info.mmu_info.info.chid = fault_info->chid;
	}
	err_pkt.err_desc = err_desc;
	err_pkt.err_size = nvgpu_safe_cast_u64_to_u8(
			sizeof(err_pkt.err_info.mmu_info));

handle_report_failure:
	if (err != 0) {
		nvgpu_sw_quiesce(g);
	}
}

void nvgpu_inject_mmu_swerror(struct gk20a *g, u32 hw_unit, u32 err_index,
		u32 sub_err_type)
{
	u32 status = 0U;
	struct mmu_fault_info fault_info;

	(void) memset(&fault_info, ERR_INJECT_TEST_PATTERN, sizeof(fault_info));
	nvgpu_report_mmu_err(g, hw_unit, err_index,
			&fault_info, status, sub_err_type);
}
