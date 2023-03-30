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

#include <nvgpu/pmu/volt.h>

static int get_curr_voltage(void *data, u64 *val)
{
	struct gk20a *g = (struct gk20a *)data;
	u32 readval;
	int err;

	err = nvgpu_pmu_volt_get_curr_volt_ps35(g, &readval);
	if (!err)
		*val = readval;

	return err;
}
DEFINE_SIMPLE_ATTRIBUTE(curr_volt_ctrl_fops, get_curr_voltage, NULL, "%llu\n");

static int get_min_voltage(void *data, u64 *val)
{
	struct gk20a *g = (struct gk20a *)data;
	u32 minval, maxval;
	int err;

	err = nvgpu_pmu_volt_get_vmin_vmax_ps35(g, &minval, &maxval);
	if (!err)
		*val = minval;

	return err;
}
DEFINE_SIMPLE_ATTRIBUTE(min_volt_ctrl_fops, get_min_voltage, NULL, "%llu\n");

static int get_max_voltage(void *data, u64 *val)
{
	struct gk20a *g = (struct gk20a *)data;
	u32 minval, maxval;
	int err;

	err = nvgpu_pmu_volt_get_vmin_vmax_ps35(g, &minval, &maxval);
	if (!err)
		*val = maxval;

	return err;
}
DEFINE_SIMPLE_ATTRIBUTE(max_volt_ctrl_fops, get_max_voltage, NULL, "%llu\n");

int nvgpu_volt_init_debugfs(struct gk20a *g)
{
	struct nvgpu_os_linux *l = nvgpu_os_linux_from_gk20a(g);
	struct dentry *dbgentry, *volt_root;

	volt_root = debugfs_create_dir("volt", l->debugfs);

	dbgentry = debugfs_create_file(
		"current_voltage", S_IRUGO, volt_root, g, &curr_volt_ctrl_fops);
	if (!dbgentry) {
		pr_err("%s: Failed to make debugfs node\n", __func__);
		return -ENOMEM;
	}

	dbgentry = debugfs_create_file("minimum_voltage",
			S_IRUGO, volt_root, g, &min_volt_ctrl_fops);
	if (!dbgentry) {
		pr_err("%s: Failed to make debugfs node\n", __func__);
		return -ENOMEM;
	}

	dbgentry = debugfs_create_file("maximum_voltage",
			S_IRUGO, volt_root, g, &max_volt_ctrl_fops);
	if (!dbgentry) {
		pr_err("%s: Failed to make debugfs node\n", __func__);
		return -ENOMEM;
	}

	return 0;
}
