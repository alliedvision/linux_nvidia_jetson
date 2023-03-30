// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * GSP Debug Nodes
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

#include <nvgpu/nvgpu_init.h>
#include <nvgpu/gsp/gsp_test.h>

#include "debug_gsp.h"
#include "os_linux.h"

#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>

#ifdef CONFIG_NVGPU_GSP_STRESS_TEST

static int gsp_test_iterations_show(struct seq_file *s, void *data)
{
	struct gk20a *g = s->private;
	u32 current_iteration = 0;
	int err = 0;

	if (nvgpu_is_powered_on(g)) {
		err = gk20a_busy(g);
		if (err)
			return err;
		current_iteration = nvgpu_gsp_get_current_iteration(g);
		gk20a_idle(g);
	}

	seq_printf(s, "%d\n", current_iteration);

	return err;
}

static int gsp_test_iterations_open(struct inode *inode, struct file *file)
{
	return single_open(file, gsp_test_iterations_show, inode->i_private);
}

static const struct file_operations nvgpu_gsp_test_iterations_debugfs_fops = {
	.open = gsp_test_iterations_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release
};

static int gsp_current_test_show(struct seq_file *s, void *data)
{
	struct gk20a *g = s->private;
	u32 current_test = 0;
	int err = 0;

	if (nvgpu_is_powered_on(g)) {
		err = gk20a_busy(g);
		if (err)
			return err;
		current_test = nvgpu_gsp_get_current_test(g);
		gk20a_idle(g);
	}

	seq_printf(s, "%d\n", current_test);

	return err;
}

static int gsp_current_test_open(struct inode *inode, struct file *file)
{
	return single_open(file, gsp_current_test_show, inode->i_private);
}

static const struct file_operations nvgpu_gsp_current_test_debugfs_fops = {
	.open = gsp_current_test_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release
};

static int gsp_test_status_show(struct seq_file *s, void *data)
{
	struct gk20a *g = s->private;
	u32 test_status = 0;
	int err = 0;

	if (nvgpu_is_powered_on(g)) {
		err = gk20a_busy(g);
		if (err)
			return err;
		test_status = (u32)nvgpu_gsp_get_test_fail_status(g);
		gk20a_idle(g);
	}

	seq_printf(s, "%d\n", test_status);

	return err;
}

static int gsp_test_status_open(struct inode *inode, struct file *file)
{
	return single_open(file, gsp_test_status_show, inode->i_private);
}

static const struct file_operations nvgpu_gsp_test_status_debugfs_fops = {
	.open = gsp_test_status_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release
};

static int gsp_test_summary_show(struct seq_file *s, void *data)
{
	struct gk20a *g = s->private;
	u32 test_state = 0;
	u32 test_iterations = 0;
	u32 current_test = 0;
	u32 test_fail_status = 0;
	int err = 0;

	if (nvgpu_is_powered_on(g)) {
		err = gk20a_busy(g);
		if (err)
			return err;
		test_state = nvgpu_gsp_get_stress_test_start(g);
		test_iterations = nvgpu_gsp_get_current_iteration(g);
		current_test = nvgpu_gsp_get_current_test(g);
		test_fail_status = (u32)nvgpu_gsp_get_test_fail_status(g);
		gk20a_idle(g);
	}

	seq_printf(s,
		"Test Started: %d\n"
		"Passed Test: %d\n"
		"Test Iterations: %d\n"
		"Test State: %d\n"
		, test_state, current_test, test_iterations, test_fail_status);

	return err;
}

static int gsp_test_summary_open(struct inode *inode, struct file *file)
{
	return single_open(file, gsp_test_summary_show, inode->i_private);
}

static const struct file_operations nvgpu_gsp_test_summary_debugfs_fops = {
	.open = gsp_test_summary_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release
};

static ssize_t gsp_start_test_read(struct file *file, char __user *user_buf,
								   size_t count, loff_t *ppos)
{
	char buf[3];
	struct gk20a *g = file->private_data;


	buf[0] = 'N';
	if (nvgpu_is_powered_on(g)) {
		if (nvgpu_gsp_get_stress_test_start(g))
			buf[0] = 'Y';
	}

	buf[1] = '\n';
	buf[2] = 0x00;

	return simple_read_from_buffer(user_buf, count, ppos, buf, 2);
}

static ssize_t gsp_start_test_write(struct file *file,
									const char __user *user_buf,
									size_t count, loff_t *ppos)
{
	char buf[32];
	int buf_size;
	bool bv;
	struct gk20a *g = file->private_data;
	int err;

	buf_size = min(count, (sizeof(buf)-1));
	if (copy_from_user(buf, user_buf, buf_size))
		return -EFAULT;

	if (strtobool(buf, &bv) == 0) {
		if (nvgpu_is_powered_on(g) && nvgpu_gsp_get_stress_test_load(g)) {
			err = gk20a_busy(g);
			if (err)
				return err;
			err = nvgpu_gsp_set_stress_test_start(g, bv);
			if (err != 0) {
				nvgpu_err(g, "failed to start GSP stress test");
				gk20a_idle(g);
				return -EFAULT;
			}
			gk20a_idle(g);
		} else {
			nvgpu_err(g,
					  "Unable to start GSP stress test, check GPU state");
			return -EFAULT;
		}
	}

	return count;
}

static const struct file_operations nvgpu_gsp_start_test_debugfs_fops = {
	.open = simple_open,
	.read = gsp_start_test_read,
	.write = gsp_start_test_write,
};

static ssize_t gsp_load_test_read(struct file *file, char __user *user_buf,
								   size_t count, loff_t *ppos)
{
	char buf[3];
	struct gk20a *g = file->private_data;


	buf[0] = 'N';
	if (nvgpu_is_powered_on(g)) {
		if (nvgpu_gsp_get_stress_test_load(g))
			buf[0] = 'Y';
	}

	buf[1] = '\n';
	buf[2] = 0x00;

	return simple_read_from_buffer(user_buf, count, ppos, buf, 2);
}

static ssize_t gsp_load_test_write(struct file *file,
								   const char __user *user_buf,
								   size_t count, loff_t *ppos)
{
	char buf[32];
	int buf_size;
	bool bv;
	struct gk20a *g = file->private_data;
	int err;

	buf_size = min(count, (sizeof(buf)-1));
	if (copy_from_user(buf, user_buf, buf_size))
		return -EFAULT;

	if (strtobool(buf, &bv) == 0) {
		if (nvgpu_is_powered_on(g)) {
			err = gk20a_busy(g);
			if (err)
				return err;
			err = nvgpu_gsp_set_stress_test_load(g, bv);
			if (err != 0) {
				nvgpu_err(g, "failed to load GSP stress test");
				gk20a_idle(g);
				return -EFAULT;
			}
			gk20a_idle(g);
		} else {
			nvgpu_err(g,
					  "Unable to load GSP stress test, check GPU state");
			return -EFAULT;
		}
	}

	return count;
}

static const struct file_operations nvgpu_gsp_load_test_debugfs_fops = {
	.open = simple_open,
	.read = gsp_load_test_read,
	.write = gsp_load_test_write,
};
#endif

void nvgpu_gsp_debugfs_fini(struct gk20a *g)
{
	struct nvgpu_os_linux *l = nvgpu_os_linux_from_gk20a(g);

	if (!(l->debugfs_gsp == NULL))
		debugfs_remove_recursive(l->debugfs_gsp);
}

int nvgpu_gsp_debugfs_init(struct gk20a *g)
{
	struct nvgpu_os_linux *l = nvgpu_os_linux_from_gk20a(g);
	struct dentry *gpu_root = l->debugfs;
	struct dentry *d;
	int err = 0;

	if (!gpu_root) {
		err = -ENODEV;
		goto exit;
	}

	d = debugfs_create_dir("gsp", gpu_root);
	if (IS_ERR(l->debugfs_gsp)) {
		err = PTR_ERR(d);
		goto exit;
	}

	l->debugfs_gsp = d;

	nvgpu_log(g, gpu_dbg_info, "g=%p", g);

#ifdef CONFIG_NVGPU_GSP_STRESS_TEST
	d = debugfs_create_file("load_test", 0644, l->debugfs_gsp, g,
							&nvgpu_gsp_load_test_debugfs_fops);
	if (IS_ERR(d)) {
		err = PTR_ERR(d);
		goto exit;
	}

	d = debugfs_create_file("start_test", 0644, l->debugfs_gsp, g,
							&nvgpu_gsp_start_test_debugfs_fops);
	if (IS_ERR(d)) {
		err = PTR_ERR(d);
		goto exit;
	}

	d = debugfs_create_file("test_iterations", 0444, l->debugfs_gsp, g,
							&nvgpu_gsp_test_iterations_debugfs_fops);
	if (IS_ERR(d)) {
		err = PTR_ERR(d);
		goto exit;
	}

	d = debugfs_create_file("current_test", 0444, l->debugfs_gsp, g,
							&nvgpu_gsp_current_test_debugfs_fops);
	if (IS_ERR(d)) {
		err = PTR_ERR(d);
		goto exit;
	}

	d = debugfs_create_file("test_status", 0444, l->debugfs_gsp, g,
							&nvgpu_gsp_test_status_debugfs_fops);
	if (IS_ERR(d)) {
		err = PTR_ERR(d);
		goto exit;
	}

	d = debugfs_create_file("test_summary", 0444, l->debugfs_gsp, g,
							&nvgpu_gsp_test_summary_debugfs_fops);
	if (IS_ERR(d)) {
		err = PTR_ERR(d);
		goto exit;
	}
#endif
	return err;
exit:
	nvgpu_gsp_debugfs_fini(g);
	return err;
}
