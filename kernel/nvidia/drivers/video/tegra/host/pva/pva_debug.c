/*
 * PVA Debug Information file
 *
 * Copyright (c) 2017-2023, NVIDIA Corporation.  All rights reserved.
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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/nvhost.h>
#include <linux/uaccess.h>
#include <linux/dma-mapping.h>
#include <linux/types.h>
#include <linux/slab.h>
#include "pva.h"
#include <uapi/linux/nvpva_ioctl.h>
#include "pva_vpu_ocd.h"
#include "pva-fw-address-map.h"

static void pva_read_crashdump(struct seq_file *s, struct pva_seg_info *seg_info)
{
	int i = 0;
	u32 *seg_addr = (u32 *) seg_info->addr;

	if (!seg_addr)
		return;

	for (i = 0; i < (seg_info->size >> 4);) {
		seq_printf(s, "0x%x 0x%x 0x%x 0x%x\n",
			seg_addr[i], seg_addr[i+1],
			seg_addr[i+2], seg_addr[i+3]);
		i = i + 4;
	}
}

static int pva_crashdump(struct seq_file *s, void *data)
{
	int err = 0;
	struct pva_crashdump_debugfs_entry *entry =
			(struct pva_crashdump_debugfs_entry *)s->private;
	struct pva *pva = entry->pva;

	err = nvhost_module_busy(pva->pdev);
	if (err) {
		nvpva_dbg_info(pva, "err in powering up pva\n");
		goto err_poweron;
	}

	pva_read_crashdump(s, &entry->seg_info);

	nvhost_module_idle(pva->pdev);

err_poweron:
	return err;
}

static int crashdump_open(struct inode *inode, struct file *file)
{
	return single_open(file, pva_crashdump, inode->i_private);
}

static const struct file_operations pva_crashdump_fops = {
	.open = crashdump_open,
	.read = seq_read,
	.release = single_release,
};

struct pva_fw_debug_log_iter {
	struct pva *pva;
	u8 *buffer;
	loff_t pos;
	size_t size;
};

static void *log_seq_start(struct seq_file *s, loff_t *pos)
{
	struct pva_fw_debug_log_iter *iter;

	iter = s->private;
	if (*pos >= iter->size)
		return NULL;

	iter->pos = *pos;
	return iter;
}

static void log_seq_stop(struct seq_file *s, void *v)
{
}

static void *log_seq_next(struct seq_file *s, void *v, loff_t *pos)
{
	struct pva_fw_debug_log_iter *iter = v;

	iter->pos += 1;
	*pos = iter->pos;

	if (iter->pos >= iter->size)
		return NULL;

	return iter;
}

static int log_seq_show(struct seq_file *s, void *v)
{
	struct pva_fw_debug_log_iter *iter = v;

	seq_putc(s, iter->buffer[iter->pos]);
	return 0;
}

static struct seq_operations const log_seq_ops = { .start = log_seq_start,
						   .stop = log_seq_stop,
						   .next = log_seq_next,
						   .show = log_seq_show };

static int fw_debug_log_open(struct inode *inode, struct file *file)
{
	struct pva_fw_debug_log_iter *iter =
		__seq_open_private(file, &log_seq_ops, sizeof(*iter));
	int err = 0;
	struct pva *pva = inode->i_private;

	if (IS_ERR_OR_NULL(iter)) {
		err = -ENOMEM;
		goto err_out;
	}

	iter->pva = pva;

	if (pva->booted) {
		err = nvhost_module_busy(pva->pdev);
		if (err) {
			nvpva_err(&pva->pdev->dev, "err in powering up pva");
			err = -EIO;
			goto free_iter;
		}

		save_fw_debug_log(pva);

		nvhost_module_idle(pva->pdev);
	}

	iter->buffer = pva->fw_debug_log.saved_log;
	iter->size =
		strnlen(pva->fw_debug_log.saved_log, pva->fw_debug_log.size);
	iter->pos = 0;

	return 0;
free_iter:
	kfree(iter);
err_out:
	return err;
}

static const struct file_operations pva_fw_debug_log_fops = {
	.open = fw_debug_log_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release_private
};

static inline void print_version(struct seq_file *s,
				 const char *version_str,
				 const u32 version)
{
	const char type = PVA_EXTRACT(version, 31, 24, u8);
	const u32 major = PVA_EXTRACT(version, 23, 16, u32);
	const u32 minor = PVA_EXTRACT(version, 15, 8, u32);
	const u32 subminor = PVA_EXTRACT(version, 7, 0, u32);

	seq_printf(s, "%s: %c.%02u.%02u.%02u\n", version_str,
		   type, major, minor, subminor);
}

static int print_firmware_versions(struct seq_file *s, void *data)
{
	struct pva *pva = s->private;
	struct pva_version_info info;
	int ret = 0;

	ret = nvhost_module_busy(pva->pdev);
	if (ret < 0)
		goto err_poweron;

	ret = pva_get_firmware_version(pva, &info);
	if (ret < 0)
		goto err_get_firmware_version;

	nvhost_module_idle(pva->pdev);

	print_version(s, "pva_r5_version", info.pva_r5_version);
	print_version(s, "pva_compat_version", info.pva_compat_version);
	seq_printf(s, "pva_revision: %x\n", info.pva_revision);
	seq_printf(s, "pva_built_on: %u\n", info.pva_built_on);

	return 0;

err_get_firmware_version:
	nvhost_module_idle(pva->pdev);
err_poweron:
	return ret;
}

static int print_version_open(struct inode *inode, struct file *file)
{
	return single_open(file, print_firmware_versions, inode->i_private);
}

static const struct file_operations print_version_fops = {
	.open = print_version_open,
	.read = seq_read,
	.release = single_release,
};

static int get_log_level(void *data, u64 *val)
{
	struct pva *pva = (struct pva *) data;

	*val = pva->log_level;
	return 0;
}

static int set_log_level(void *data, u64 val)
{
	struct pva *pva = (struct pva *) data;

	pva->log_level = val;
	if (pva->booted)
		return pva_set_log_level(pva, val, false);
	else
		return 0;
}

DEFINE_DEBUGFS_ATTRIBUTE(log_level_fops, get_log_level, set_log_level, "%llu");

static void update_vpu_stats(struct pva *pva, bool stats_enabled)
{
	u32 flags = PVA_CMD_INT_ON_ERR | PVA_CMD_INT_ON_COMPLETE;
	struct pva_cmd_status_regs status = {};
	struct pva_cmd_s cmd = {};
	int err = 0;
	u32 nregs;
	u64 duration = 0;
	struct pva_vpu_stats_s *stats_buf =
		pva->vpu_util_info.stats_fw_buffer_va;
	u64 *vpu_stats = pva->vpu_util_info.vpu_stats;

	if (vpu_stats == 0)
		goto err_out;

	err = nvhost_module_busy(pva->pdev);
	if (err < 0) {
		dev_err(&pva->pdev->dev, "error in powering up pva %d",
			err);
		vpu_stats[0] = 0;
		vpu_stats[1] = 0;
		return;
	}

	nregs = pva_cmd_get_vpu_stats(&cmd,
				      pva->vpu_util_info.stats_fw_buffer_iova,
				      flags, stats_enabled);
	err = pva_mailbox_send_cmd_sync(pva, &cmd, nregs, &status);
	if (err < 0) {
		nvpva_warn(&pva->pdev->dev, "get vpu stats cmd failed: %d\n",
			    err);
		goto err_out;
	}

	if (stats_enabled == false)
		goto err_out;

	duration = stats_buf->window_end_time - stats_buf->window_start_time;
	if (duration == 0)
		goto err_out;

	vpu_stats[0] =
		(10000ULL * stats_buf->total_utilization_time[0]) / duration;
	vpu_stats[1] =
		(10000ULL * stats_buf->total_utilization_time[1]) / duration;
	pva->vpu_util_info.start_stamp = stats_buf->window_start_time;
	pva->vpu_util_info.end_stamp = stats_buf->window_end_time;
	goto out;
err_out:
	vpu_stats[0] = 0;
	vpu_stats[1] = 0;
out:
	nvhost_module_idle(pva->pdev);
}

static int print_vpu_stats(struct seq_file *s, void *data)
{
	struct pva *pva = s->private;

	update_vpu_stats(pva, pva->stats_enabled);
	seq_printf(s, "%llu\n%llu\n%llu\n%llu\n",
		   pva->vpu_util_info.start_stamp,
		   pva->vpu_util_info.end_stamp,
		   pva->vpu_util_info.vpu_stats[0],
		   pva->vpu_util_info.vpu_stats[1]);

	return 0;
}

static int pva_stats_open(struct inode *inode, struct file *file)
{
	return single_open(file, print_vpu_stats, inode->i_private);
}

static const struct file_operations pva_stats_fops = {
	.open = pva_stats_open,
	.read = seq_read,
	.release = single_release,
};

static int get_authentication(void *data, u64 *val)
{
	struct pva *pva = (struct pva *) data;

	*val = pva->pva_auth.pva_auth_enable ? 1 : 0;

	return 0;
}

static int set_authentication(void *data, u64 val)
{
	struct pva *pva = (struct pva *) data;

	pva->pva_auth.pva_auth_enable = (val == 1) ? true : false;

	if (pva->pva_auth.pva_auth_enable)
		pva->pva_auth.pva_auth_allow_list_parsed = false;

	return 0;
}

DEFINE_DEBUGFS_ATTRIBUTE(pva_auth_fops, get_authentication, set_authentication, "%llu");

static long vpu_ocd_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
{
	struct pva_vpu_dbg_block *dbg_block = f->f_inode->i_private;
	int err = 0;

	switch (cmd) {
	case PVA_OCD_IOCTL_VPU_IO: {
		struct pva_ocd_ioctl_vpu_io_param io_param;

		if (copy_from_user(&io_param, (void __user *)arg,
				   sizeof(io_param))) {
			pr_err("failed copy ioctl buffer from user; size: %u",
			       _IOC_SIZE(cmd));
			err = -EFAULT;
			goto out;
		}
		err = pva_vpu_ocd_io(dbg_block, io_param.instr,
				     &io_param.data[0], io_param.n_write,
				     &io_param.data[0], io_param.n_read);
		if (err)
			goto out;

		err = copy_to_user((void __user *)arg, &io_param,
				   sizeof(io_param));
		if (err)
			goto out;

		break;
	}
	default:
		err = -ENOIOCTLCMD;
		break;
	}

out:
	return err;
}

static const struct file_operations pva_vpu_ocd_fops = {
	.unlocked_ioctl = vpu_ocd_ioctl
};

void pva_debugfs_deinit(struct pva *pva)
{
	if (pva->vpu_util_info.stats_fw_buffer_va != NULL) {
		dma_free_coherent(&pva->aux_pdev->dev,
				  sizeof(struct pva_vpu_stats_s),
				  pva->vpu_util_info.stats_fw_buffer_va,
				  pva->vpu_util_info.stats_fw_buffer_iova);
		pva->vpu_util_info.stats_fw_buffer_va = 0;
		pva->vpu_util_info.stats_fw_buffer_iova = 0;
	}

	if (pva->fw_debug_log.saved_log != NULL) {
		mutex_destroy(&pva->fw_debug_log.saved_log_lock);
		kfree(pva->fw_debug_log.saved_log);
	}
}

void pva_debugfs_init(struct platform_device *pdev)
{
	struct nvhost_device_data *pdata = platform_get_drvdata(pdev);
	struct pva *pva = pdata->private_data;
	struct dentry *de = pdata->debugfs;
	static const char *vpu_ocd_names[NUM_VPU_BLOCKS] = { "ocd_vpu0",
							     "ocd_vpu1" };
	int i, err;

	if (!de)
		return;

	pva->debugfs_entry_r5.pva = pva;
	pva->debugfs_entry_vpu0.pva = pva;
	pva->debugfs_entry_vpu1.pva = pva;

	debugfs_create_file("r5_crashdump", S_IRUGO, de,
				&pva->debugfs_entry_r5, &pva_crashdump_fops);
	debugfs_create_file("vpu0_crashdump", S_IRUGO, de,
				&pva->debugfs_entry_vpu0, &pva_crashdump_fops);
	debugfs_create_file("vpu1_crashdump", S_IRUGO, de,
				&pva->debugfs_entry_vpu1, &pva_crashdump_fops);
	debugfs_create_u32("submit_task_mode", S_IRUGO | S_IWUSR, de,
				 &pva->submit_task_mode);
	debugfs_create_bool("vpu_debug", 0644, de,
			   &pva->vpu_debug_enabled);
	debugfs_create_u32("r5_dbg_wait", 0644, de,
			   &pva->r5_dbg_wait);
	debugfs_create_bool("r5_timeout_enable", 0644, de,
			    &pva->timeout_enabled);
	debugfs_create_file("firmware_version", S_IRUGO, de, pva,
			    &print_version_fops);
	debugfs_create_u32("cg_disable", 0644, de, &pva->slcg_disable);
	debugfs_create_bool("vpu_printf_enabled", 0644, de,
			    &pva->vpu_printf_enabled);
	debugfs_create_file("fw_log_level", 0644, de, pva, &log_level_fops);
	debugfs_create_u32("driver_log_mask", 0644, de, &pva->driver_log_mask);
	debugfs_create_file("vpu_app_authentication", 0644, de, pva,
			    &pva_auth_fops);
	debugfs_create_u32("profiling_level", 0644, de, &pva->profiling_level);
	debugfs_create_bool("stats_enabled", 0644, de, &pva->stats_enabled);
	debugfs_create_file("vpu_stats", 0644, de, pva, &pva_stats_fops);

	mutex_init(&pva->fw_debug_log.saved_log_lock);
	pva->fw_debug_log.size = FW_DEBUG_LOG_BUFFER_SIZE;
	pva->fw_debug_log.saved_log =
		kzalloc(FW_DEBUG_LOG_BUFFER_SIZE, GFP_KERNEL);
	if (IS_ERR_OR_NULL(pva->fw_debug_log.saved_log)) {
		dev_err(&pva->pdev->dev,
			"failed to allocate memory for saving debug log");
		pva->fw_debug_log.saved_log = NULL;
		mutex_destroy(&pva->fw_debug_log.saved_log_lock);
	} else {
		debugfs_create_file("fw_debug_log", 0444, de, pva,
				    &pva_fw_debug_log_fops);
	}

	pva->vpu_util_info.stats_fw_buffer_va = dma_alloc_coherent(
		&pva->aux_pdev->dev, sizeof(struct pva_vpu_stats_s),
		&pva->vpu_util_info.stats_fw_buffer_iova, GFP_KERNEL);
	if (IS_ERR_OR_NULL(pva->vpu_util_info.stats_fw_buffer_va)) {
		err = PTR_ERR(pva->vpu_util_info.stats_fw_buffer_va);
		dev_err(&pva->pdev->dev,
			"err = %d. failed to allocate stats buffer\n", err);
		pva->vpu_util_info.stats_fw_buffer_va = 0;
		pva->vpu_util_info.stats_fw_buffer_iova = 0;
	}

	err = pva_vpu_ocd_init(pva);
	if (err == 0) {
		for (i = 0; i < NUM_VPU_BLOCKS; i++)
			debugfs_create_file(vpu_ocd_names[i], 0644, de,
					    &pva->vpu_dbg_blocks[i],
					    &pva_vpu_ocd_fops);
	} else {
		dev_err(&pva->pdev->dev, "VPU OCD initialization failed\n");
	}
}
