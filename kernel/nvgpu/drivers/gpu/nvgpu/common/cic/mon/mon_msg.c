/*
 * Copyright (c) 2021, NVIDIA CORPORATION.  All rights reserved.
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

#include <nvgpu/nvgpu_err_info.h>
#include <nvgpu/string.h>

#include "cic_mon_priv.h"

void nvgpu_init_err_msg_header(struct gpu_err_header *header)
{
	header->version.major = (u16)1U;
	header->version.minor = (u16)0U;
	header->sub_err_type = 0U;
	header->sub_unit_id = 0UL;
	header->address = 0UL;
	header->timestamp_ns = 0UL;
}

void nvgpu_init_err_msg(struct nvgpu_err_msg *msg)
{
	(void) memset(msg, 0, sizeof(struct nvgpu_err_msg));
	msg->hw_unit_id = 0U;
	msg->is_critical = false;
	msg->err_id = (u8)0U;
	msg->err_size = (u8)0U;
}

void nvgpu_init_host_err_msg(struct nvgpu_err_msg *msg)
{
	nvgpu_init_err_msg(msg);
	nvgpu_init_err_msg_header(&msg->err_info.host_info.header);
}

void nvgpu_init_ecc_err_msg(struct nvgpu_err_msg *msg)
{
	nvgpu_init_err_msg(msg);
	nvgpu_init_err_msg_header(&msg->err_info.ecc_info.header);
	msg->err_info.ecc_info.err_cnt = 0UL;
}

void nvgpu_init_pri_err_msg(struct nvgpu_err_msg *msg)
{
	nvgpu_init_err_msg(msg);
	nvgpu_init_err_msg_header(&msg->err_info.pri_info.header);
}

void nvgpu_init_ce_err_msg(struct nvgpu_err_msg *msg)
{
	nvgpu_init_err_msg(msg);
	nvgpu_init_err_msg_header(&msg->err_info.ce_info.header);
}

void nvgpu_init_pmu_err_msg(struct nvgpu_err_msg *msg)
{
	nvgpu_init_err_msg(msg);
	nvgpu_init_err_msg_header(&msg->err_info.pmu_err_info.header);
	msg->err_info.pmu_err_info.status = 0U;
}

void nvgpu_init_gr_err_msg(struct nvgpu_err_msg *msg)
{
	nvgpu_init_err_msg(msg);
	nvgpu_init_err_msg_header(&msg->err_info.gr_info.header);
	msg->err_info.gr_info.curr_ctx = 0U;
	msg->err_info.gr_info.chid = 0U;
	msg->err_info.gr_info.tsgid = 0U;
	msg->err_info.gr_info.status = 0U;
}

void nvgpu_init_ctxsw_err_msg(struct nvgpu_err_msg *msg)
{
	nvgpu_init_err_msg(msg);
	nvgpu_init_err_msg_header(&msg->err_info.ctxsw_info.header);
	msg->err_info.ctxsw_info.curr_ctx = 0U;
	msg->err_info.ctxsw_info.tsgid = 0U;
	msg->err_info.ctxsw_info.chid = 0U;
	msg->err_info.ctxsw_info.ctxsw_status0 = 0U;
	msg->err_info.ctxsw_info.ctxsw_status1 = 0U;
	msg->err_info.ctxsw_info.mailbox_value = 0U;
}

void nvgpu_init_mmu_err_msg(struct nvgpu_err_msg *msg)
{
	nvgpu_init_err_msg(msg);
	nvgpu_init_err_msg_header(&msg->err_info.mmu_info.header);
	msg->err_info.mmu_info.info.inst_ptr = 0UL;
	msg->err_info.mmu_info.info.inst_aperture = 0U;
	msg->err_info.mmu_info.info.fault_addr = 0UL;
	msg->err_info.mmu_info.info.fault_addr_aperture = 0U;
	msg->err_info.mmu_info.info.timestamp_lo = 0U;
	msg->err_info.mmu_info.info.timestamp_hi = 0U;
	msg->err_info.mmu_info.info.mmu_engine_id = 0U;
	msg->err_info.mmu_info.info.gpc_id = 0U;
	msg->err_info.mmu_info.info.client_type = 0U;
	msg->err_info.mmu_info.info.client_id = 0U;
	msg->err_info.mmu_info.info.fault_type = 0U;
	msg->err_info.mmu_info.info.access_type = 0U;
	msg->err_info.mmu_info.info.protected_mode = 0U;
	msg->err_info.mmu_info.info.replayable_fault = false;
	msg->err_info.mmu_info.info.replay_fault_en = 0U;
	msg->err_info.mmu_info.info.valid = false;
	msg->err_info.mmu_info.info.faulted_pbdma = 0U;
	msg->err_info.mmu_info.info.faulted_engine = 0U;
	msg->err_info.mmu_info.info.faulted_subid = 0U;
	msg->err_info.mmu_info.info.chid = 0U;
	msg->err_info.mmu_info.status = 0U;
}
