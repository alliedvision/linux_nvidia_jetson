/*
 * Tegra GPU Virtualization Interfaces to Server
 *
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

#include <nvgpu/tsg.h>
#include <nvgpu/pm_reservation.h>
#include <nvgpu/vgpu/vgpu.h>
#include <nvgpu/vgpu/tegra_vgpu.h>
#include <nvgpu/gr/ctx.h>
#include <nvgpu/gk20a.h>

#include "common/vgpu/ivc/comm_vgpu.h"
#include "profiler_vgpu.h"

int vgpu_profiler_bind_hwpm(struct gk20a *g,
		u32 gr_instance_id,
		bool is_ctxsw,
		struct nvgpu_tsg *tsg)
{
	struct tegra_vgpu_cmd_msg msg = {};
	struct tegra_vgpu_prof_bind_unbind_params *p =
			&msg.params.prof_bind_unbind;
	int err;

	nvgpu_assert(gr_instance_id == 0U);

	msg.cmd = TEGRA_VGPU_CMD_PROF_BIND_UNBIND;
	msg.handle = vgpu_get_handle(g);

	p->subcmd = TEGRA_VGPU_PROF_BIND_HWPM;
	p->is_ctxsw = is_ctxsw;
	p->tsg_id = tsg != NULL ? tsg->tsgid : NVGPU_INVALID_TSG_ID;

	err = vgpu_comm_sendrecv(&msg, sizeof(msg), sizeof(msg));
	err = err ? err : msg.ret;
	return err;
}

int vgpu_profiler_unbind_hwpm(struct gk20a *g,
		u32 gr_instance_id,
		bool is_ctxsw,
		struct nvgpu_tsg *tsg)
{
	struct tegra_vgpu_cmd_msg msg = {};
	struct tegra_vgpu_prof_bind_unbind_params *p =
			&msg.params.prof_bind_unbind;
	int err;

	nvgpu_assert(gr_instance_id == 0U);

	msg.cmd = TEGRA_VGPU_CMD_PROF_BIND_UNBIND;
	msg.handle = vgpu_get_handle(g);

	p->subcmd = TEGRA_VGPU_PROF_UNBIND_HWPM;
	p->is_ctxsw = is_ctxsw;
	p->tsg_id = tsg != NULL ? tsg->tsgid : NVGPU_INVALID_TSG_ID;

	err = vgpu_comm_sendrecv(&msg, sizeof(msg), sizeof(msg));
	err = err ? err : msg.ret;
	return err;
}

int vgpu_profiler_bind_hwpm_streamout(struct gk20a *g,
		u32 gr_instance_id,
		bool is_ctxsw,
		struct nvgpu_tsg *tsg,
		u64 pma_buffer_va,
		u32 pma_buffer_size,
		u64 pma_bytes_available_buffer_va)
{
	struct tegra_vgpu_cmd_msg msg = {};
	struct tegra_vgpu_prof_bind_unbind_params *p =
			&msg.params.prof_bind_unbind;
	int err;

	nvgpu_assert(gr_instance_id == 0U);

	msg.cmd = TEGRA_VGPU_CMD_PROF_BIND_UNBIND;
	msg.handle = vgpu_get_handle(g);

	p->subcmd = TEGRA_VGPU_PROF_BIND_HWPM_STREAMOUT;
	p->is_ctxsw = is_ctxsw;
	p->tsg_id = tsg != NULL ? tsg->tsgid : NVGPU_INVALID_TSG_ID;
	p->pma_buffer_va = pma_buffer_va;
	p->pma_buffer_size = pma_buffer_size;
	p->pma_bytes_available_buffer_va = pma_bytes_available_buffer_va;

	err = vgpu_comm_sendrecv(&msg, sizeof(msg), sizeof(msg));
	err = err ? err : msg.ret;
	return err;
}

int vgpu_profiler_unbind_hwpm_streamout(struct gk20a *g,
		u32 gr_instance_id,
		bool is_ctxsw,
		struct nvgpu_tsg *tsg,
		void *pma_bytes_available_buffer_cpuva,
		bool smpc_reserved)
{
	struct tegra_vgpu_cmd_msg msg = {};
	struct tegra_vgpu_prof_bind_unbind_params *p =
			&msg.params.prof_bind_unbind;
	int err;

	nvgpu_assert(gr_instance_id == 0U);

	msg.cmd = TEGRA_VGPU_CMD_PROF_BIND_UNBIND;
	msg.handle = vgpu_get_handle(g);

	p->subcmd = TEGRA_VGPU_PROF_UNBIND_HWPM_STREAMOUT;
	p->is_ctxsw = is_ctxsw;
	p->tsg_id = tsg != NULL ? tsg->tsgid : NVGPU_INVALID_TSG_ID;
	p->smpc_reserved = (u8)smpc_reserved;

	err = vgpu_comm_sendrecv(&msg, sizeof(msg), sizeof(msg));
	err = err ? err : msg.ret;
	return err;
}

int vgpu_profiler_bind_smpc(struct gk20a *g,
		u32 gr_instance_id,
		bool is_ctxsw,
		struct nvgpu_tsg *tsg)
{
	struct tegra_vgpu_cmd_msg msg = {};
	struct tegra_vgpu_prof_bind_unbind_params *p =
			&msg.params.prof_bind_unbind;
	int err;

	nvgpu_assert(gr_instance_id == 0U);

	msg.cmd = TEGRA_VGPU_CMD_PROF_BIND_UNBIND;
	msg.handle = vgpu_get_handle(g);

	p->subcmd = TEGRA_VGPU_PROF_BIND_SMPC;
	p->is_ctxsw = is_ctxsw;
	p->tsg_id = tsg != NULL ? tsg->tsgid : NVGPU_INVALID_TSG_ID;

	err = vgpu_comm_sendrecv(&msg, sizeof(msg), sizeof(msg));
	err = err ? err : msg.ret;
	return err;
}

int vgpu_profiler_unbind_smpc(struct gk20a *g,
		u32 gr_instance_id,
		bool is_ctxsw,
		struct nvgpu_tsg *tsg)
{
	struct tegra_vgpu_cmd_msg msg = {};
	struct tegra_vgpu_prof_bind_unbind_params *p =
			&msg.params.prof_bind_unbind;
	int err;

	msg.cmd = TEGRA_VGPU_CMD_PROF_BIND_UNBIND;
	msg.handle = vgpu_get_handle(g);

	p->subcmd = TEGRA_VGPU_PROF_UNBIND_SMPC;
	p->is_ctxsw = is_ctxsw;
	p->tsg_id = tsg != NULL ? tsg->tsgid : NVGPU_INVALID_TSG_ID;

	err = vgpu_comm_sendrecv(&msg, sizeof(msg), sizeof(msg));
	err = err ? err : msg.ret;
	return err;
}
