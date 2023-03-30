/*
 * Copyright (c) 2019-2020, NVIDIA Corporation. All rights reserved.
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

#include <linux/debugfs.h>
#include "os_linux.h"
#include "include/nvgpu/bios.h"

#include <nvgpu/pmu/clk/clk.h>
#include <nvgpu/pmu/perf.h>

static int get_s_param_info(void *data, u64 *val)
{
	struct gk20a *g = (struct gk20a *)data;
	int status = 0;

	status = nvgpu_pmu_perf_vfe_get_s_param(g, val);
	if(status != 0) {
		nvgpu_err(g, "Vfe_var get s_param failed");
		return status;
	}
	return status;
}
DEFINE_SIMPLE_ATTRIBUTE(s_param_fops, get_s_param_info , NULL, "%llu\n");

int nvgpu_s_param_init_debugfs(struct gk20a *g)
{
	struct nvgpu_os_linux *l = nvgpu_os_linux_from_gk20a(g);
	struct dentry *dbgentry;

	dbgentry = debugfs_create_file(
		"s_param", S_IRUGO, l->debugfs, g, &s_param_fops);
	if (!dbgentry) {
		pr_err("%s: Failed to make debugfs node\n", __func__);
		return -ENOMEM;
	}

	return 0;
}
