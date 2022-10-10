/*
 * Copyright (c) 2017-2022, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <nvgpu/gk20a.h>
#include <nvgpu/gr/ctx.h>
#include <nvgpu/nvgpu_init.h>

#include "common/gr/ctx_priv.h"
#include "common/gr/gr_priv.h"

#include "debug_gr.h"
#include "os_linux.h"

#include <linux/uaccess.h>
#include <linux/capability.h>
#include <linux/debugfs.h>

#ifdef CONFIG_NVGPU_COMPRESSION
static int cbc_status_debug_show(struct seq_file *s, void *unused)
{
	struct gk20a *g = s->private;
	struct nvgpu_cbc *cbc = g->cbc;

	if (!cbc) {
		nvgpu_err(g, "cbc is not initialized");
		return -EBADFD;
	}
	seq_printf(s, "cbc.compbit_backing_size: %u\n",
		cbc->compbit_backing_size);
	seq_printf(s, "cbc.comptags_per_cacheline: %u\n",
		cbc->comptags_per_cacheline);
	seq_printf(s, "cbc.gobs_per_comptagline_per_slice: %u\n",
		cbc->gobs_per_comptagline_per_slice);
	seq_printf(s, "cbc.max_comptag_lines: %u\n",
		cbc->max_comptag_lines);
	seq_printf(s, "cbc.comp_tags.size: %lu\n",
		cbc->comp_tags.size);
	seq_printf(s, "cbc.compbit_store.base_hw: %llu\n",
		cbc->compbit_store.base_hw);
	if (nvgpu_mem_is_valid(&cbc->compbit_store.mem)) {
		seq_printf(s, "cbc.compbit_store.mem.aperture: %u\n",
			cbc->compbit_store.mem.aperture);
		seq_printf(s, "cbc.compbit_store.mem.size: %zu\n",
			cbc->compbit_store.mem.size);
		seq_printf(s, "cbc.compbit_store.mem.aligned_size: %zu\n",
			cbc->compbit_store.mem.aligned_size);
		seq_printf(s, "cbc.compbit_store.mem.gpu_va: %llu\n",
			cbc->compbit_store.mem.gpu_va);
		seq_printf(s, "cbc.compbit_store.mem.skip_wmb: %u\n",
			cbc->compbit_store.mem.skip_wmb);
		seq_printf(s, "cbc.compbit_store.mem.free_gpu_va: %u\n",
			cbc->compbit_store.mem.free_gpu_va);
		seq_printf(s, "cbc.compbit_store.mem.mem_flags: %lu\n",
			cbc->compbit_store.mem.mem_flags);
		seq_printf(s, "cbc.compbit_store.mem.cpu_va: %px\n",
			cbc->compbit_store.mem.cpu_va);
		seq_printf(s, "cbc.compbit_store.mem.pa: %llx\n",
			nvgpu_mem_get_addr(g, &cbc->compbit_store.mem));
	} else {
		seq_printf(s, "cbc.compbit_store.mem: invalid\n");
	}
	return 0;
}

static int cbc_status_debug_open(struct inode *inode, struct file *file)
{
	int err = 0;
	struct gk20a *g = (struct gk20a *)inode->i_private;

	if (!capable(CAP_SYS_ADMIN)) {
		return -EPERM;
	}
	g = nvgpu_get(g);
	if (!g) {
		return -ENODEV;
	}
	err = gk20a_busy(g);
	if (err < 0) {
		nvgpu_err(g, "Couldn't power-up gpu");
		goto put;
	}
	err = nvgpu_cbc_init_support(g);
	if (err < 0) {
		nvgpu_err(g, "cbc init failed");
		goto idle;
	}
	err = single_open(file, cbc_status_debug_show, inode->i_private);
	if (err < 0) {
		nvgpu_err(g, "single open failed");
		goto idle;
	}
	return err;

idle:
	gk20a_idle(g);
put:
	nvgpu_put(g);
	return err;
}

static int cbc_status_debug_release(struct inode *inode, struct file *file)
{
	struct gk20a *g = (struct gk20a *)inode->i_private;

	gk20a_idle(g);
	nvgpu_put(g);
	return single_release(inode, file);
}

static const struct file_operations cbc_status_debug_fops = {
	.open           = cbc_status_debug_open,
	.read           = seq_read,
	.llseek         = seq_lseek,
	.release        = cbc_status_debug_release,
};

static ssize_t cbc_ctrl_debug_write_cmd(struct file *f, const char __user *cmd, size_t len, loff_t *off)
{
	char cmd_buf[32];
	struct gk20a *g = f->private_data;
	struct nvgpu_cbc *cbc = g->cbc;
	const size_t original_len = len;
	int err = 0;

	if (!cbc) {
		nvgpu_err(g, "cbc is not initialized");
		return -EINVAL;
	}
	if (len == 0 || len >= sizeof(cmd_buf)) {
		nvgpu_err(g, "invalid cmd len: %zu", len);
		return -EINVAL;
	}
	if (copy_from_user(cmd_buf, cmd, len)) {
		nvgpu_err(g, "failed to read cmd");
		return -EFAULT;
	}
	if (cmd_buf[len - 1] == '\n') {
		len = len - 1;
	}
	cmd_buf[len] = '\x00';

	if (strncmp("cbc_clean", cmd_buf, len) == 0) {
		// Flush the comptag store to L2.
		err = g->ops.cbc.ctrl(g, nvgpu_cbc_op_clean, 0, 0);
		if (err == 0) {
			// From from L2 to memory.
			err = g->ops.mm.cache.l2_flush(g, false);
		}
	} else {
		nvgpu_err(g, "Unknown cmd: %s", cmd_buf);
		err = -EINVAL;
	}
	return err < 0 ? err : (ssize_t)original_len;
}

static int cbc_ctrl_debug_open(struct inode *inode, struct file *file)
{
	int err = 0;
	struct gk20a *g = inode->i_private;

	if (!capable(CAP_SYS_ADMIN)) {
		return -EPERM;
	}
	g = nvgpu_get(g);
	if (!g) {
		return -ENODEV;
	}
	err = gk20a_busy(g);
	if (err < 0) {
		nvgpu_err(g, "Couldn't power-up gpu");
		goto put;
	}
	err = nvgpu_cbc_init_support(g);
	if (err < 0) {
		nvgpu_err(g, "cbc init failed");
		goto idle;
	}
	file->private_data = g;
	return err;

idle:
	gk20a_idle(g);
put:
	nvgpu_put(g);
	return err;
}

static int cbc_ctrl_debug_release(struct inode *inode, struct file *file)
{
	struct gk20a *g = file->private_data;

	if (g) {
		gk20a_idle(g);
		nvgpu_put(g);
	}
	return 0;
}

static int cbc_ctrl_debug_mmap_cbc_store(struct file *f, struct vm_area_struct *vma)
{
	struct gk20a *g = f->private_data;
	struct nvgpu_cbc *cbc = g->cbc;
	unsigned long mapping_size = 0U;
	int err = 0;
	u64 cbc_store_pa = 0;
	pgprot_t prot = pgprot_noncached(vma->vm_page_prot);

	if (vma->vm_flags & VM_WRITE) {
		return -EPERM;
	}
	if (!(vma->vm_flags & VM_SHARED)) {
		return -EINVAL;
	}
	if (!cbc) {
		nvgpu_err(g, "cbc is not initialized");
		err = -EINVAL;
		goto done;
	}
	if (nvgpu_mem_is_valid(&cbc->compbit_store.mem) == 0) {
		nvgpu_err(g, "cbc compbit store memory is not valid");
		err = -EINVAL;
		goto done;
	}
	mapping_size = (vma->vm_end - vma->vm_start);
	if (mapping_size != cbc->compbit_store.mem.size) {
		nvgpu_err(g, "mapping size (%lx) is unequal to store size (%lx)",
			  mapping_size, cbc->compbit_store.mem.size);
		err = -EINVAL;
		goto done;
	}
	if (vma->vm_pgoff != 0UL) {
		err = -EINVAL;
		goto done;
	}
	vma->vm_flags |= VM_DONTCOPY | VM_DONTEXPAND | VM_NORESERVE |
			 VM_DONTDUMP | VM_PFNMAP;
	vma->vm_flags &= ~VM_MAYWRITE;
	cbc_store_pa = nvgpu_mem_get_addr(g, &cbc->compbit_store.mem);
	err = remap_pfn_range(vma, vma->vm_start, cbc_store_pa >> PAGE_SHIFT,
			      mapping_size, prot);
	if (err < 0) {
		nvgpu_err(g, "Failed to remap %llx to user space", cbc_store_pa);
	}

done:
	return err;
}

static const struct file_operations cbc_ctrl_debug_fops = {
	.open           = cbc_ctrl_debug_open,
	.release        = cbc_ctrl_debug_release,
	.write          = cbc_ctrl_debug_write_cmd,
	.mmap           = cbc_ctrl_debug_mmap_cbc_store,
};
#endif /* CONFIG_NVGPU_COMPRESSION */

static int gr_default_attrib_cb_size_show(struct seq_file *s, void *data)
{
	struct gk20a *g = s->private;

	/* HAL might not be initialized yet */
	if (g->ops.gr.init.get_attrib_cb_default_size == NULL)
		return -EFAULT;

	seq_printf(s, "%u\n", g->ops.gr.init.get_attrib_cb_default_size(g));

	return 0;
}

static int gr_default_attrib_cb_size_open(struct inode *inode,
	struct file *file)
{
	return single_open(file, gr_default_attrib_cb_size_show,
		inode->i_private);
}

static const struct file_operations gr_default_attrib_cb_size_fops= {
	.open		= gr_default_attrib_cb_size_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static ssize_t force_preemption_gfxp_read(struct file *file,
		char __user *user_buf, size_t count, loff_t *ppos)
{
	char buf[3];
	struct gk20a *g = file->private_data;

	if (g->gr->gr_ctx_desc == NULL) {
		return -EFAULT;
	}

	if (g->gr->gr_ctx_desc->force_preemption_gfxp) {
		buf[0] = 'Y';
	} else {
		buf[0] = 'N';
	}

	buf[1] = '\n';
	buf[2] = 0x00;

	return simple_read_from_buffer(user_buf, count, ppos, buf, 2);
}

static ssize_t force_preemption_gfxp_write(struct file *file,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	char buf[32];
	int buf_size;
	bool val;
	struct gk20a *g = file->private_data;

	if (g->gr->gr_ctx_desc == NULL) {
		return -EFAULT;
	}

	buf_size = min(count, (sizeof(buf)-1));
	if (copy_from_user(buf, user_buf, buf_size)) {
		return -EFAULT;
	}

	if (strtobool(buf, &val) == 0) {
		g->gr->gr_ctx_desc->force_preemption_gfxp = val;
	}

	return count;
}

static struct file_operations force_preemption_gfxp_fops = {
	.open =		simple_open,
	.read =		force_preemption_gfxp_read,
	.write =	force_preemption_gfxp_write,
};

static ssize_t force_preemption_cilp_read(struct file *file,
		 char __user *user_buf, size_t count, loff_t *ppos)
{
	char buf[3];
	struct gk20a *g = file->private_data;

	if (g->gr->gr_ctx_desc == NULL) {
		return -EFAULT;
	}

	if (g->gr->gr_ctx_desc->force_preemption_cilp) {
		buf[0] = 'Y';
	} else {
		buf[0] = 'N';
	}

	buf[1] = '\n';
	buf[2] = 0x00;

	return simple_read_from_buffer(user_buf, count, ppos, buf, 2);
}

static ssize_t force_preemption_cilp_write(struct file *file,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	char buf[32];
	int buf_size;
	bool val;
	struct gk20a *g = file->private_data;

	if (g->gr->gr_ctx_desc == NULL) {
		return -EFAULT;
	}

	buf_size = min(count, (sizeof(buf)-1));
	if (copy_from_user(buf, user_buf, buf_size)) {
		return -EFAULT;
	}

	if (strtobool(buf, &val) == 0) {
		g->gr->gr_ctx_desc->force_preemption_cilp = val;
	}

	return count;
}

static struct file_operations force_preemption_cilp_fops = {
	.open =		simple_open,
	.read =		force_preemption_cilp_read,
	.write =	force_preemption_cilp_write,
};

static ssize_t dump_ctxsw_stats_on_channel_close_read(struct file *file,
		char __user *user_buf, size_t count, loff_t *ppos)
{
	char buf[3];
	struct gk20a *g = file->private_data;

	if (g->gr->gr_ctx_desc == NULL) {
		return -EFAULT;
	}

	if (g->gr->gr_ctx_desc->dump_ctxsw_stats_on_channel_close) {
		buf[0] = 'Y';
	} else {
		buf[0] = 'N';
	}

	buf[1] = '\n';
	buf[2] = 0x00;

	return simple_read_from_buffer(user_buf, count, ppos, buf, 2);
}

static ssize_t dump_ctxsw_stats_on_channel_close_write(struct file *file,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	char buf[32];
	int buf_size;
	bool val;
	struct gk20a *g = file->private_data;

	if (g->gr->gr_ctx_desc == NULL) {
		return -EFAULT;
	}

	buf_size = min(count, (sizeof(buf)-1));
	if (copy_from_user(buf, user_buf, buf_size)) {
		return -EFAULT;
	}

	if (strtobool(buf, &val) == 0) {
		g->gr->gr_ctx_desc->dump_ctxsw_stats_on_channel_close = val;
	}

	return count;
}

static struct file_operations dump_ctxsw_stats_on_channel_close_fops = {
	.open =		simple_open,
	.read =		dump_ctxsw_stats_on_channel_close_read,
	.write =	dump_ctxsw_stats_on_channel_close_write,
};

int gr_gk20a_debugfs_init(struct gk20a *g)
{
	struct nvgpu_os_linux *l = nvgpu_os_linux_from_gk20a(g);
	struct dentry *d;

	d = debugfs_create_file(
		"gr_default_attrib_cb_size", S_IRUGO, l->debugfs, g,
				&gr_default_attrib_cb_size_fops);
	if (!d)
		return -ENOMEM;

	d = debugfs_create_file(
		"force_preemption_gfxp", S_IRUGO|S_IWUSR, l->debugfs, g,
				&force_preemption_gfxp_fops);
	if (!d)
		return -ENOMEM;

	d = debugfs_create_file(
		"force_preemption_cilp", S_IRUGO|S_IWUSR, l->debugfs, g,
				&force_preemption_cilp_fops);
	if (!d)
		return -ENOMEM;

#ifdef CONFIG_NVGPU_COMPRESSION
	d = debugfs_create_file("cbc_status", S_IRUSR, l->debugfs, g,
		&cbc_status_debug_fops);
	if (!d)
		return -ENOMEM;
	/* Using debugfs_create_file_unsafe to allow mmap */
	d = debugfs_create_file_unsafe("cbc_ctrl", S_IWUSR,
		l->debugfs, g, &cbc_ctrl_debug_fops);
	if (!d)
		return -ENOMEM;
#endif /* CONFIG_NVGPU_COMPRESSION */

	if (!g->is_virtual) {
		d = debugfs_create_file(
			"dump_ctxsw_stats_on_channel_close", S_IRUGO|S_IWUSR,
				l->debugfs, g,
				&dump_ctxsw_stats_on_channel_close_fops);
		if (!d)
			return -ENOMEM;
	}

	return 0;
}

