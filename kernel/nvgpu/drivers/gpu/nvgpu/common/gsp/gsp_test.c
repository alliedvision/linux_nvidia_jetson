// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * GSP Test Functions
 *
 * Copyright (c) 2021, NVIDIA Corporation.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <nvgpu/firmware.h>
#include <nvgpu/falcon.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/log.h>
#include <nvgpu/gsp.h>
#include <nvgpu/gsp/gsp_test.h>

#include "gsp_priv.h"
#include "gsp_bootstrap.h"

u32 nvgpu_gsp_get_current_iteration(struct gk20a *g)
{
	u32 data = 0;
	struct nvgpu_gsp *gsp = g->gsp;

	nvgpu_log_fn(g, " ");

	data = nvgpu_falcon_mailbox_read(gsp->gsp_flcn, FALCON_MAILBOX_1);

	return data;
}

u32 nvgpu_gsp_get_current_test(struct gk20a *g)
{
	u32 data = 0;
	struct nvgpu_gsp *gsp = g->gsp;

	nvgpu_log_fn(g, " ");

	data = nvgpu_falcon_mailbox_read(gsp->gsp_flcn, FALCON_MAILBOX_0);

	return data;
}

bool nvgpu_gsp_get_test_fail_status(struct gk20a *g)
{
	struct nvgpu_gsp *gsp = g->gsp;

	return gsp->gsp_test.stress_test_fail_status;
}

bool nvgpu_gsp_get_stress_test_start(struct gk20a *g)
{
	struct nvgpu_gsp *gsp = g->gsp;

	return gsp->gsp_test.enable_stress_test;
}

bool nvgpu_gsp_get_stress_test_load(struct gk20a *g)
{
	struct nvgpu_gsp *gsp = g->gsp;

	if (gsp == NULL)
		return false;

	return gsp->gsp_test.load_stress_test;
}

void nvgpu_gsp_set_test_fail_status(struct gk20a *g, bool val)
{
	struct nvgpu_gsp *gsp = g->gsp;

	gsp->gsp_test.stress_test_fail_status = val;
}

int nvgpu_gsp_set_stress_test_start(struct gk20a *g, bool flag)
{
	int err = 0;
	struct nvgpu_gsp *gsp = g->gsp;

	nvgpu_log_fn(g, " ");

	if (flag) {
		nvgpu_info(g, "Enabling GSP test");
		nvgpu_falcon_mailbox_write(gsp->gsp_flcn, FALCON_MAILBOX_1, 0xFFFFFFFF);
	} else {
		nvgpu_info(g, "Halting GSP test");
		nvgpu_gsp_stress_test_halt(g, false);
	}

	gsp->gsp_test.enable_stress_test = flag;

	return err;
}

int nvgpu_gsp_set_stress_test_load(struct gk20a *g, bool flag)
{
	int err = 0;

	nvgpu_log_fn(g, " ");

	if (flag)
		err = nvgpu_gsp_stress_test_bootstrap(g, flag);

	return err;
}
