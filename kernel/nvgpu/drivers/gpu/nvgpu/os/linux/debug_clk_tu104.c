/*
 * Copyright (c) 2018-2020, NVIDIA Corporation. All rights reserved.
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
#include <linux/seq_file.h>

#include "os_linux.h"

#include <nvgpu/boardobjgrp_e32.h>
#include <nvgpu/boardobjgrp_e255.h>
#include <nvgpu/pmu/clk/clk.h>
#include <nvgpu/pmu/volt.h>

/* Dependency of this include will be removed in further CL */
#include "../../common/pmu/boardobj/boardobj.h"

#include "hal/clk/clk_tu104.h"

void nvgpu_clk_arb_pstate_change_lock(struct gk20a *g, bool lock);

static int tu104_get_rate_show(void *data , u64 *val)
{
	struct namemap_cfg *c = (struct namemap_cfg *)data;
	struct gk20a *g = c->g;

	if (!g->ops.clk.get_rate_cntr)
		return -EINVAL;

	*val = c->is_counter ? (u64)c->scale * g->ops.clk.get_rate_cntr(g, c) :
		0 /* TODO PLL read */;

	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(get_rate_fops, tu104_get_rate_show, NULL, "%llu\n");

static int vftable_show(struct seq_file *s, void *unused)
{
	struct gk20a *g = s->private;
	int status;
	u8 index;
	u32 voltage_min_uv, voltage_step_size_uv;
	u32 gpcclk_clkmhz = 0, gpcclk_voltuv = 0;

	voltage_min_uv = nvgpu_pmu_clk_fll_get_lut_step_size(g->pmu->clk_pmu);
	voltage_step_size_uv = nvgpu_pmu_clk_fll_get_lut_step_size(g->pmu->clk_pmu);

	for (index = 0; index < CTRL_CLK_LUT_NUM_ENTRIES_GV10x; index++) {
		gpcclk_voltuv = voltage_min_uv + index * voltage_step_size_uv;
		status = nvgpu_clk_domain_volt_to_freq(g, 0, &gpcclk_clkmhz,
				&gpcclk_voltuv, CTRL_VOLT_DOMAIN_LOGIC);

		if (status != 0) {
			nvgpu_err(g, "Failed to get freq for requested volt");
			return status;
		}
		seq_printf(s, "Voltage: %duV  Frequency: %dMHz\n",
			gpcclk_voltuv, gpcclk_clkmhz);
	}
	return 0;
}

static int vftable_open(struct inode *inode, struct file *file)
{
	return single_open(file, vftable_show, inode->i_private);
}

static const struct file_operations vftable_fops = {
	.open = vftable_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int tu104_change_seq_time(void *data , u64 *val)
{
	struct gk20a *g = (struct gk20a *)data;
	s64 readval;

	if (!g->ops.clk.get_change_seq_time)
		return -EINVAL;

	g->ops.clk.get_change_seq_time(g, &readval);
	*val = (u64)readval;
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(change_seq_fops, tu104_change_seq_time, NULL, "%llu\n");

int tu104_clk_init_debugfs(struct gk20a *g)
{
	struct nvgpu_os_linux *l = nvgpu_os_linux_from_gk20a(g);
	struct dentry *gpu_root = l->debugfs;
	struct dentry *clocks_root, *clk_freq_ctlr_root;
	struct dentry *d;
	unsigned int i;

	if (NULL == (clocks_root = debugfs_create_dir("clocks", gpu_root)))
		return -ENOMEM;

	clk_freq_ctlr_root = debugfs_create_dir("clk_freq_ctlr", gpu_root);
	if (clk_freq_ctlr_root == NULL)
		return -ENOMEM;

	d = debugfs_create_file("change_seq_time_us", S_IRUGO, clocks_root,
				g, &change_seq_fops);

	nvgpu_log(g, gpu_dbg_info, "g=%p", g);

	for (i = 0; i < g->clk.namemap_num; i++) {
		if (g->clk.clk_namemap[i].is_enable) {
			d = debugfs_create_file(
				g->clk.clk_namemap[i].name,
				S_IRUGO,
				clocks_root,
				&g->clk.clk_namemap[i],
				&get_rate_fops);
			if (!d)
				goto err_out;
		}
	}

	d = debugfs_create_file("vftable", S_IRUGO,
			clocks_root, g, &vftable_fops);
	if (!d)
		goto err_out;
	return 0;

err_out:
	pr_err("%s: Failed to make debugfs node\n", __func__);
	debugfs_remove_recursive(clocks_root);
	return -ENOMEM;
}
