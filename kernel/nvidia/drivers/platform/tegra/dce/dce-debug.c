/*
 * Copyright (c) 2019-2023, NVIDIA CORPORATION & AFFILIATES.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#include <linux/errno.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <dce.h>
#include <dce-log.h>
#include <dce-util-common.h>
#include <interface/dce-interface.h>

/**
 * dbg_dce_load_fw - loads the fw to DRAM.
 *
 * @d : Pointer to dce struct
 *
 * Return : 0 if successful
 */
static int dbg_dce_load_fw(struct tegra_dce *d)
{
	const char *name = dce_get_fw_name(d);

	d->fw_data = dce_request_firmware(d, name);
	if (!d->fw_data) {
		dce_err(d, "FW Request Failed");
		return -EBUSY;
	}

	dce_set_load_fw_status(d, true);

	return 0;
}

/**
 * dbg_dce_config_ast - Configures the ast and sets the status.
 *
 * @d : Pointer to dce struct
 *
 * Return : Void
 */
static void dbg_dce_config_ast(struct tegra_dce *d)
{
	dce_config_ast(d);
	dce_set_ast_config_status(d, true);
}

/**
 * dbg_dce_reset_dce - Configures the evp in DCE cluster
 *				and brings dce out of reset.
 *
 * @d : Pointer to dce struct
 *
 * Return : 0 if successful
 */
static int dbg_dce_reset_dce(struct tegra_dce *d)
{
	int ret = 0;


	ret = dce_reset_dce(d);
	if (ret) {
		dce_err(d, "DCE Reset Failed");
		return ret;
	}
	dce_set_dce_reset_status(d, true);

	return ret;

}

/**
 * dbg_dce_boot_dce - loads the fw and configures other dce cluster
 *			elements for bringing dce out of reset.
 *
 * @d : Pointer to dce struct
 *
 * Return : 0 if successful
 */
static int dbg_dce_boot_dce(struct tegra_dce *d)
{
	int ret = 0;


	ret = dbg_dce_load_fw(d);
	if (ret) {
		dce_err(d, "DCE Load FW Failed");
		return ret;
	}

	dbg_dce_config_ast(d);

	ret = dbg_dce_reset_dce(d);

	if (ret)
		dce_err(d, "DCE Reset Failed");

	return ret;

}

static ssize_t dbg_dce_load_fw_read(struct file *file,
			char __user *user_buf, size_t count, loff_t *ppos)
{
	char buf[3];
	struct tegra_dce *d = file->private_data;

	if (d->load_complete)
		buf[0] = 'Y';
	else
		buf[0] = 'N';
	buf[1] = '\n';
	buf[2] = 0x00;

	return simple_read_from_buffer(user_buf, count, ppos, buf, 2);
}

static ssize_t dbg_dce_load_fw_write(struct file *file,
			const char __user *user_buf, size_t count, loff_t *ppos)
{
	int ret = 0;
	char buf[32];
	int buf_size;
	bool bv;
	struct tegra_dce *d = file->private_data;

	buf_size = min(count, (sizeof(buf)-1));
	if (copy_from_user(buf, user_buf, buf_size))
		return -EFAULT;

	if (strtobool(buf, &bv) == 0) {
		if (bv == true) {
			ret = dbg_dce_load_fw(d);
			if (ret)
				return ret;
		}
	}

	return count;
}

static const struct file_operations load_firmware_fops = {
	.open =		simple_open,
	.read =		dbg_dce_load_fw_read,
	.write =	dbg_dce_load_fw_write,
};

static ssize_t dbg_dce_config_ast_read(struct file *file,
			char __user *user_buf, size_t count, loff_t *ppos)
{
	char buf[3];
	struct tegra_dce *d = file->private_data;

	if (d->ast_config_complete)
		buf[0] = 'Y';
	else
		buf[0] = 'N';
	buf[1] = '\n';
	buf[2] = 0x00;

	return simple_read_from_buffer(user_buf, count, ppos, buf, 2);
}

static ssize_t dbg_dce_config_ast_write(struct file *file,
			const char __user *user_buf, size_t count, loff_t *ppos)
{
	char buf[32];
	int buf_size;
	bool bv;
	struct tegra_dce *d = file->private_data;

	buf_size = min(count, (sizeof(buf)-1));
	if (copy_from_user(buf, user_buf, buf_size))
		return -EFAULT;

	if (strtobool(buf, &bv) == 0) {
		if (bv == true)
			dbg_dce_config_ast(d);
	}

	return count;
}

static const struct file_operations config_ast_fops = {
	.open =		simple_open,
	.read =		dbg_dce_config_ast_read,
	.write =	dbg_dce_config_ast_write,
};

static ssize_t dbg_dce_reset_dce_fops_read(struct file *file,
			char __user *user_buf, size_t count, loff_t *ppos)
{
	char buf[3];
	struct tegra_dce *d = file->private_data;

	if (d->reset_complete)
		buf[0] = 'Y';
	else
		buf[0] = 'N';
	buf[1] = '\n';
	buf[2] = 0x00;

	return simple_read_from_buffer(user_buf, count, ppos, buf, 2);
}

static ssize_t dbg_dce_reset_dce_fops_write(struct file *file,
			const char __user *user_buf, size_t count, loff_t *ppos)
{
	int ret = 0;
	char buf[32];
	int buf_size;
	bool bv;
	struct tegra_dce *d = file->private_data;

	buf_size = min(count, (sizeof(buf)-1));
	if (copy_from_user(buf, user_buf, buf_size))
		return -EFAULT;

	if (strtobool(buf, &bv) == 0) {
		if (bv == true) {
			ret = dbg_dce_reset_dce(d);
			if (ret)
				return ret;
		}
	}

	return count;
}

static const struct file_operations reset_dce_fops = {
	.open =		simple_open,
	.read =		dbg_dce_reset_dce_fops_read,
	.write =	dbg_dce_reset_dce_fops_write,
};

static ssize_t dbg_dce_admin_echo_fops_write(struct file *file,
			const char __user *user_buf, size_t count, loff_t *ppos)
{
	int ret = 0;
	char buf[32];
	int buf_size;
	u32 i, echo_count;
	struct dce_ipc_message *msg = NULL;
	struct dce_admin_ipc_cmd *req_msg;
	struct dce_admin_ipc_resp *resp_msg;
	struct tegra_dce *d = file->private_data;

	buf_size = min(count, (sizeof(buf)-1));
	ret = kstrtou32_from_user(user_buf, buf_size, 10, &echo_count);
	if (ret) {
		dce_err(d, "Admin msg count out of range");
		goto out;
	}

	msg = dce_admin_allocate_message(d);
	if (!msg) {
		dce_err(d, "IPC msg allocation failed");
		goto out;
	}

	req_msg = (struct dce_admin_ipc_cmd *)(msg->tx.data);
	resp_msg = (struct dce_admin_ipc_resp *) (msg->rx.data);

	dce_info(d, "Requested %u echo messages", echo_count);

	for (i = 0; i < echo_count; i++) {
		u32 resp;

		req_msg->args.echo.data = i;
		ret = dce_admin_send_cmd_echo(d, msg);
		if (ret) {
			dce_err(d, "Admin msg failed for seq No : %u", i);
			goto out;
		}

		resp = resp_msg->args.echo.data;

		if (i == resp) {
			dce_info(d, "Received Response:%u for request:%u",
				 resp, i);
		} else {
			dce_err(d, "Invalid response, expected:%u received:%u",
				i, resp);
		}
	}

out:
	if (msg)
		dce_admin_free_message(d, msg);

	return count;
}

static const struct file_operations admin_echo_fops = {
	.open =		simple_open,
	.write =	dbg_dce_admin_echo_fops_write,
};

static ssize_t dbg_dce_boot_dce_fops_read(struct file *file,
			char __user *user_buf, size_t count, loff_t *ppos)
{
	char buf[3];
	struct tegra_dce *d = file->private_data;

	if (d->ast_config_complete &&
		d->reset_complete && d->load_complete)
		buf[0] = 'Y';
	else
		buf[0] = 'N';
	buf[1] = '\n';
	buf[2] = 0x00;

	return simple_read_from_buffer(user_buf, count, ppos, buf, 2);
}

static ssize_t dbg_dce_boot_dce_fops_write(struct file *file,
			const char __user *user_buf, size_t count, loff_t *ppos)
{
	int ret = 0;
	char buf[32];
	int buf_size;
	bool bv;
	struct tegra_dce *d = file->private_data;

	buf_size = min(count, (sizeof(buf)-1));
	if (copy_from_user(buf, user_buf, buf_size))
		return -EFAULT;

	if (strtobool(buf, &bv) == 0) {
		if (bv == true) {
			ret = dbg_dce_boot_dce(d);
			if (ret)
				return ret;
		}
	}

	return count;
}

static const struct file_operations boot_dce_fops = {
	.open =		simple_open,
	.read =		dbg_dce_boot_dce_fops_read,
	.write =	dbg_dce_boot_dce_fops_write,
};

static ssize_t dbg_dce_boot_status_fops_read(struct file *file,
					     char __user *user_buf,
					     size_t count, loff_t *ppos)
{
	u8 fsb;
	char buf[32];
	u32 last_status;
	ssize_t len = 0;
	unsigned long *addr;
	struct tegra_dce *d = file->private_data;
	u32 boot_status = d->boot_status;
	hsp_sema_t ss = dce_ss_get_state(d, DCE_BOOT_SEMA);

	if (ss & DCE_BOOT_COMPLETE)
		goto core_boot_done;

	/* Clear BOOT_COMPLETE bit and bits set by OS */
	ss &= ~(DCE_OS_BITMASK | DCE_BOOT_COMPLETE);
	addr = (unsigned long *)&ss;

	fsb = find_first_bit(addr, 32U);
	if (fsb > 31U) {
		dce_info(d, "dce-fw boot not started yet");
		goto core_boot_done;
	}

	last_status = DCE_BIT(fsb);

	switch (last_status) {
	case DCE_HALTED:
		strcpy(buf, "DCE_HALTED");
		break;
	case DCE_BOOT_TCM_COPY:
		strcpy(buf, "TCM_COPY");
		break;
	case DCE_BOOT_HW_INIT:
		strcpy(buf, "HW_INIT");
		break;
	case DCE_BOOT_MPU_INIT:
		strcpy(buf, "MPU_INIT:");
		break;
	case DCE_BOOT_CACHE_INIT:
		strcpy(buf, "CACHE_INIT");
		break;
	case DCE_BOOT_R5_INIT:
		strcpy(buf, "R5_INIT");
		break;
	case DCE_BOOT_DRIVER_INIT:
		strcpy(buf, "DRIVER_INIT");
		break;
	case DCE_BOOT_MAIN_STARTED:
		strcpy(buf, "MAIN_STARTED");
		break;
	case DCE_BOOT_TASK_INIT_START:
		strcpy(buf, "TASK_INIT_STARTED");
		break;
	case DCE_BOOT_TASK_INIT_DONE:
		strcpy(buf, "TASK_INIT_DONE");
		break;
	default:
		strcpy(buf, "STATUS_UNKNOWN");
		break;
	}
	goto done;

core_boot_done:
	/* Clear DCE_STATUS_FAILED bit get actual failure reason*/
	boot_status &= ~DCE_STATUS_FAILED;
	addr = (unsigned long *)&boot_status;
	last_status = DCE_BIT(find_first_bit(addr, 32));

	switch (last_status) {
	case DCE_FW_SUSPENDED:
		strcpy(buf, "DCE_FW_SUSPENDED");
		break;
	case DCE_FW_BOOT_DONE:
		strcpy(buf, "DCE_FW_BOOT_DONE");
		break;
	case DCE_FW_ADMIN_SEQ_DONE:
		strcpy(buf, "DCE_FW_ADMIN_SEQ_DONE");
		break;
	case DCE_FW_ADMIN_SEQ_FAILED:
		strcpy(buf, "DCE_FW_ADMIN_SEQ_FAILED");
		break;
	case DCE_FW_ADMIN_SEQ_START:
		strcpy(buf, "DCE_FW_ADMIN_SEQ_STARTED");
		break;
	case DCE_FW_BOOTSTRAP_DONE:
		strcpy(buf, "DCE_FW_BOOTSTRAP_DONE");
		break;
	case DCE_FW_BOOTSTRAP_FAILED:
		strcpy(buf, "DCE_FW_BOOTSTRAP_FAILED");
		break;
	case DCE_FW_BOOTSTRAP_START:
		strcpy(buf, "DCE_FW_BOOTSTRAP_STARTED");
		break;
	case DCE_FW_EARLY_BOOT_FAILED:
		strcpy(buf, "DCE_FW_EARLY_BOOT_FAILED");
		break;
	case DCE_FW_EARLY_BOOT_DONE:
		strcpy(buf, "DCE_FW_EARLY_BOOT_DONE");
		break;
	case DCE_AST_CONFIG_DONE:
		strcpy(buf, "DCE_AST_CONFIG_DONE");
		break;
	case DCE_AST_CONFIG_START:
		strcpy(buf, "DCE_AST_CONFIG_STARTED");
		break;
	case DCE_EARLY_INIT_DONE:
		strcpy(buf, "DCE_EARLY_INIT_DONE");
		break;
	case DCE_EARLY_INIT_FAILED:
		strcpy(buf, "DCE_EARLY_INIT_FAILED");
		break;
	case DCE_EARLY_INIT_START:
		strcpy(buf, "DCE_EARLY_INIT_STARTED");
		break;
	default:
		strcpy(buf, "STATUS_UNKNOWN");
	}

done:
	len = strlen(buf);
	buf[len] = '\0';
	dce_info(d, "boot status:%s status_val:0x%x\n", buf, d->boot_status);
	return simple_read_from_buffer(user_buf, count, ppos, buf, len + 1);
}

static const struct file_operations boot_status_fops = {
	.open	= simple_open,
	.read	= dbg_dce_boot_status_fops_read,
};

void dce_remove_debug(struct tegra_dce *d)
{
	struct dce_device *d_dev = dce_device_from_dce(d);

	debugfs_remove(d_dev->debugfs);

	d_dev->debugfs = NULL;
}

int dump_hsp_regs_show(struct seq_file *s, void *unused)
{
	u8 i = 0;
	struct tegra_dce *d = s->private;

	/**
	 * Dump Boot Semaphore Value
	 */
	dce_info(d, "DCE_BOOT_SEMA : 0x%x",
				dce_ss_get_state(d, DCE_BOOT_SEMA));

	/**
	 * Dump Shared Mailboxes Values
	 */
	dce_info(d, "DCE_MBOX_FROM_DCE_RM : 0x%x",
				dce_smb_read(d, DCE_MBOX_FROM_DCE_RM));
	dce_info(d, "DCE_MBOX_TO_DCE_RM: 0x%x",
				dce_smb_read(d, DCE_MBOX_TO_DCE_RM));
	dce_info(d, "DCE_MBOX_FROM_DCE_RM_EVENT_NOTIFY: 0x%x",
				dce_smb_read(d, DCE_MBOX_FROM_DCE_RM_EVENT_NOTIFY));
	dce_info(d, "DCE_MBOX_TO_DCE_RM_EVENT_NOTIFY: 0x%x",
				dce_smb_read(d, DCE_MBOX_TO_DCE_RM_EVENT_NOTIFY));
	dce_info(d, "DCE_MBOX_FROM_DCE_ADMIN: 0x%x",
				dce_smb_read(d, DCE_MBOX_FROM_DCE_ADMIN));
	dce_info(d, "DCE_MBOX_BOOT_CMD: 0x%x",
				dce_smb_read(d, DCE_MBOX_BOOT_CMD));
	dce_info(d, "DCE_MBOX_IRQ: 0x%x",
				dce_smb_read(d, DCE_MBOX_IRQ));

	/**
	 * Dump HSP IE registers Values
	 */

#define DCE_MAX_IE_REGS 5U
	for (i = 0; i < DCE_MAX_IE_REGS; i++)
		dce_info(d, "DCE_HSP_IE_%d : 0x%x", i, dce_hsp_ie_read(d, i));
#undef DCE_MAX_IE_REGS

	/**
	 * Dump HSP IE registers Values
	 */
#define DCE_MAX_SM_FULL_REGS 8U
	for (i = 0; i < DCE_MAX_SM_FULL_REGS; i++) {
		dce_info(d, "DCE_HSP_SM_FULL_%d : 0x%x", i,
			 dce_smb_read_full_ie(d, i));
	}
#undef DCE_MAX_SM_FULL_REGS

	dce_info(d, "DCE_HSP_IR : 0x%x",
					dce_hsp_ir_read(d));
	return 0;
}

static int dump_hsp_regs_open(struct inode *inode, struct file *file)
{
	return single_open(file, dump_hsp_regs_show, inode->i_private);
}

static const struct file_operations dump_hsp_regs_fops = {
	.open		= dump_hsp_regs_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

/**
 * dce_init_debug - Initializes the debug features of dce
 *
 * @d : Pointer to tegra_dce struct.
 *
 * Return : Void
 */
void dce_init_debug(struct tegra_dce *d)
{
	struct dentry *retval;
	struct device *dev = dev_from_dce(d);
	struct dce_device *d_dev = dce_device_from_dce(d);

	d_dev->debugfs = debugfs_create_dir("tegra_dce", NULL);
	if (!d_dev->debugfs)
		return;

	retval = debugfs_create_file("load_fw", 0444,
				     d_dev->debugfs, d, &load_firmware_fops);
	if (!retval)
		goto err_handle;

	retval = debugfs_create_file("config_ast", 0444,
				     d_dev->debugfs, d, &config_ast_fops);
	if (!retval)
		goto err_handle;

	retval = debugfs_create_file("reset", 0444,
				     d_dev->debugfs, d, &reset_dce_fops);
	if (!retval)
		goto err_handle;

	retval = debugfs_create_file("boot", 0444,
				     d_dev->debugfs, d, &boot_dce_fops);
	if (!retval)
		goto err_handle;

	retval = debugfs_create_file("admin_echo", 0444,
				     d_dev->debugfs, d, &admin_echo_fops);
	if (!retval)
		goto err_handle;

	retval = debugfs_create_file("boot_status", 0444,
				     d_dev->debugfs, d, &boot_status_fops);
	if (!retval)
		goto err_handle;

	retval = debugfs_create_file("dump_hsp_regs", 0444,
				     d_dev->debugfs, d, &dump_hsp_regs_fops);
	if (!retval)
		goto err_handle;

	return;

err_handle:
	dev_err(dev, "could not create debugfs\n");
	dce_remove_debug(d);
}
