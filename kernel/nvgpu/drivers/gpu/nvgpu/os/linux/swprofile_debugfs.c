/*
 * Copyright (c) 2020-2021, NVIDIA CORPORATION.  All rights reserved.
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

#include <nvgpu/swprofile.h>
#include <nvgpu/debug.h>

static int nvgpu_debugfs_swprofile_enable(void *data, u64 val)
{
	struct nvgpu_swprofiler *p = (struct nvgpu_swprofiler *) data;

	if (val == 0) {
		nvgpu_swprofile_close(p);
		return 0;
	} else {
		return nvgpu_swprofile_open(p->g, p);
	}
}

DEFINE_SIMPLE_ATTRIBUTE(
	nvgpu_debugfs_swprofile_enable_debugfs_fops,
	NULL,
	nvgpu_debugfs_swprofile_enable,
	"%llu\n"
);

static void nvgpu_debugfs_write_to_seqfile_no_nl(void *ctx, const char *str)
{
	seq_printf((struct seq_file *)ctx, str);
}

static int nvgpu_debugfs_swprofile_stats(struct seq_file *s, void *unused)
{
	struct nvgpu_swprofiler *p = s->private;
	struct nvgpu_debug_context o = {
		.fn = nvgpu_debugfs_write_to_seqfile_no_nl,
		.ctx = s,
	};

	nvgpu_swprofile_print_ranges(p->g, p, &o);

	return 0;
}

static int nvgpu_debugfs_swprofile_stats_open(struct inode *inode, struct file *file)
{
	return single_open(file, nvgpu_debugfs_swprofile_stats, inode->i_private);
}

static const struct file_operations nvgpu_debugfs_swprofile_stats_debugfs_fops = {
	.open		= nvgpu_debugfs_swprofile_stats_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int nvgpu_debugfs_swprofile_raw_data(struct seq_file *s, void *unused)
{
	struct nvgpu_swprofiler *p = s->private;
	struct nvgpu_debug_context o = {
		.fn = nvgpu_debugfs_write_to_seqfile_no_nl,
		.ctx = s,
	};

	nvgpu_swprofile_print_raw_data(p->g, p, &o);

	return 0;
}

static int nvgpu_debugfs_swprofile_raw_data_open(struct inode *inode, struct file *file)
{
	return single_open(file, nvgpu_debugfs_swprofile_raw_data, inode->i_private);
}

static const struct file_operations nvgpu_debugfs_swprofile_raw_data_debugfs_fops = {
	.open		= nvgpu_debugfs_swprofile_raw_data_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int nvgpu_debugfs_swprofile_basic_stats(struct seq_file *s, void *unused)
{
	struct nvgpu_swprofiler *p = s->private;
	struct nvgpu_debug_context o = {
		.fn = nvgpu_debugfs_write_to_seqfile_no_nl,
		.ctx = s,
	};

	nvgpu_swprofile_print_basic_stats(p->g, p, &o);

	return 0;
}

static int nvgpu_debugfs_swprofile_basic_stats_open(struct inode *inode, struct file *file)
{
	return single_open(file, nvgpu_debugfs_swprofile_basic_stats, inode->i_private);
}

static const struct file_operations nvgpu_debugfs_swprofile_basic_stats_debugfs_fops = {
	.open		= nvgpu_debugfs_swprofile_basic_stats_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

void nvgpu_debugfs_swprofile_init(struct gk20a *g,
				  struct dentry *root,
				  struct nvgpu_swprofiler *p,
				  const char *name)
{
	struct dentry *swprofile_root;

	swprofile_root = debugfs_create_dir(name, root);
	if (IS_ERR_OR_NULL(swprofile_root))
		return;

	debugfs_create_file("enable", 0200, swprofile_root, p,
		&nvgpu_debugfs_swprofile_enable_debugfs_fops);

	debugfs_create_file("percentiles", 0400, swprofile_root, p,
		&nvgpu_debugfs_swprofile_stats_debugfs_fops);

	debugfs_create_file("raw_data", 0400, swprofile_root, p,
		&nvgpu_debugfs_swprofile_raw_data_debugfs_fops);

	debugfs_create_file("basic_stats", 0400, swprofile_root, p,
		&nvgpu_debugfs_swprofile_basic_stats_debugfs_fops);
}
