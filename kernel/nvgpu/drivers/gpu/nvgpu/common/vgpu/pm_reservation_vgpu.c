/*
 * Copyright (c) 2020, NVIDIA CORPORATION.  All rights reserved.
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

#include <nvgpu/vgpu/vgpu_ivc.h>
#include <nvgpu/vgpu/tegra_vgpu.h>
#include <nvgpu/vgpu/vgpu.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/pm_reservation.h>

#include "pm_reservation_vgpu.h"
#include "common/vgpu/ivc/comm_vgpu.h"

int vgpu_pm_reservation_acquire(struct gk20a *g, u32 reservation_id,
	enum nvgpu_profiler_pm_resource_type pm_resource,
	enum nvgpu_profiler_pm_reservation_scope scope,
	u32 vmid)
{
	struct tegra_vgpu_cmd_msg msg;
	struct tegra_vgpu_prof_mgt_params *p = &msg.params.prof_management;
	int err = 0;

	msg.cmd = TEGRA_VGPU_CMD_PROF_MGT;
	msg.handle = vgpu_get_handle(g);

	p->mode = TEGRA_VGPU_PROF_PM_RESERVATION_ACQUIRE;
	p->reservation_id = reservation_id;
	p->pm_resource = pm_resource;
	p->scope = scope;

	err = vgpu_comm_sendrecv(&msg, sizeof(msg), sizeof(msg));
	err = err ? err : msg.ret;
	return err;
}

int vgpu_pm_reservation_release(struct gk20a *g, u32 reservation_id,
	enum nvgpu_profiler_pm_resource_type pm_resource,
	u32 vmid)
{
	struct tegra_vgpu_cmd_msg msg;
	struct tegra_vgpu_prof_mgt_params *p = &msg.params.prof_management;
	int err = 0;

	msg.cmd = TEGRA_VGPU_CMD_PROF_MGT;
	msg.handle = vgpu_get_handle(g);

	p->mode = TEGRA_VGPU_PROF_PM_RESERVATION_RELEASE;
	p->reservation_id = reservation_id;
	p->pm_resource = pm_resource;

	err = vgpu_comm_sendrecv(&msg, sizeof(msg), sizeof(msg));
	err = err ? err : msg.ret;
	return err;
}
