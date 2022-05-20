/*
 * Copyright (c) 2020, NVIDIA CORPORATION.  All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/slab.h>   /* kmalloc() */
#include <linux/errno.h> /* error codes */
#include <linux/delay.h> /* For msleep and usleep_range */
#include <linux/version.h>
#include <uapi/scsi/ufs/ioctl.h>
#include "tegra_vblk.h"
#include "tegra_hv_ufs.h"

#define VBLK_UFS_MAX_IOC_SIZE (256 * 1024)

static int vblk_validate_single_query_io(struct vblk_dev *vblkdev,
					struct ufs_ioc_query_req *query_req,
					size_t *data_len,
					bool *w_flag)
{
	int err = 0;

	switch (query_req->opcode) {
	case UPIU_QUERY_OPCODE_READ_DESC:
		if (query_req->idn >= QUERY_DESC_IDN_MAX) {
			dev_err(vblkdev->device,
					"Desc IDN out of range %d\n",
					query_req->idn);
			err = -EINVAL;
			goto out;
		}

		*data_len = min_t(size_t, QUERY_DESC_MAX_SIZE,
						query_req->buf_size);
		break;

	case UPIU_QUERY_OPCODE_WRITE_DESC:
		if (query_req->idn >= QUERY_DESC_IDN_MAX) {
			err = -EINVAL;
			dev_err(vblkdev->device,
					"Desc IDN out of range %d\n",
					query_req->idn);
			goto out;
		}

		*data_len = min_t(size_t, QUERY_DESC_MAX_SIZE,
					query_req->buf_size);
		*w_flag = true;
		break;

	case UPIU_QUERY_OPCODE_READ_ATTR:
		if (query_req->idn >= QUERY_ATTR_IDN_MAX) {
			err = -EINVAL;
			dev_err(vblkdev->device,
					"ATTR IDN out of range %d\n",
					query_req->idn);
			goto out;
		}

		if (query_req->buf_size != sizeof(u32)) {
			err = -EINVAL;
			dev_err(vblkdev->device,
					"Buf size out of range %d\n",
					query_req->buf_size);
			goto out;
		}
		*data_len = sizeof(u32);
		break;

	case UPIU_QUERY_OPCODE_WRITE_ATTR:
		if (query_req->idn > QUERY_ATTR_IDN_MAX) {
			err = -EINVAL;
			dev_err(vblkdev->device,
					"ATTR IDN out of range %d\n",
					query_req->idn);
			goto out;
		}

		if (query_req->buf_size != sizeof(u32)) {
			err = -EINVAL;
			dev_err(vblkdev->device,
					"Buf size out of range %d\n",
					query_req->buf_size);
			goto out;
		}
		*data_len = sizeof(u32);
		*w_flag = true;
		break;

	case UPIU_QUERY_OPCODE_READ_FLAG:
		if (query_req->idn > QUERY_FLAG_IDN_MAX) {
			err = -EINVAL;
			dev_err(vblkdev->device,
					"Flag IDN out of range %d\n",
					query_req->idn);
			goto out;
		}

		if (query_req->buf_size != sizeof(u8)) {
			err = -EINVAL;
			dev_err(vblkdev->device,
					"Buf size out of range %d\n",
					query_req->buf_size);
			goto out;
		}
		*data_len = sizeof(u8);
		break;

	case UPIU_QUERY_OPCODE_SET_FLAG:
	case UPIU_QUERY_OPCODE_CLEAR_FLAG:
	case UPIU_QUERY_OPCODE_TOGGLE_FLAG:
		if (query_req->idn > QUERY_FLAG_IDN_MAX) {
			err = -EINVAL;
			dev_err(vblkdev->device,
					"Flag IDN out of range %d\n",
					query_req->idn);
			goto out;
		}
			/* TODO: Create buffer to be attached */
		*data_len = 0;
		break;
	default:
		err = -EINVAL;
		dev_err(vblkdev->device, "Invalid opcode %d\n",
						query_req->idn);
		break;
	}
out:
	return err;
}

int vblk_prep_ufs_combo_ioc(struct vblk_dev *vblkdev,
	struct vblk_ioctl_req *ioctl_req,
	void __user *user, uint32_t cmd)
{
	int err = 0;
	struct vblk_ufs_combo_info *combo_info;
	struct vblk_ufs_ioc_query_req *combo_cmd;
	int i = 0;
	uint8_t num_cmd;
	struct ufs_ioc_query_req ic;
	struct ufs_ioc_combo_query_req cc;
	struct ufs_ioc_combo_query_req __user *user_cmd;
	struct ufs_ioc_query_req __user *usr_ptr;
	uint32_t combo_cmd_size;
	uint32_t ioctl_bytes = VBLK_UFS_MAX_IOC_SIZE;
	uint8_t *tmpaddr;
	void *ioctl_buf;
	size_t data_len = 0;
	bool w_flag = false;

	ioctl_buf = vmalloc(ioctl_bytes);
	if (ioctl_buf == NULL)
		return -ENOMEM;

	combo_info = (struct vblk_ufs_combo_info *)ioctl_buf;

	user_cmd = (struct ufs_ioc_combo_query_req __user *)user;
	if (copy_from_user(&cc, user_cmd, sizeof(cc))) {
		err = -EFAULT;
		goto free_ioc_buf;
	}
	num_cmd = cc.num_cmds;
	if (num_cmd > MAX_QUERY_CMD_PER_COMBO) {
		err = -EINVAL;
		goto free_ioc_buf;
	}

	usr_ptr = (void * __user)cc.query;
	combo_info->count = num_cmd;
	combo_info->need_cq_empty = cc.need_cq_empty;
	combo_cmd = (struct vblk_ufs_ioc_query_req *)(ioctl_buf +
		sizeof(struct vblk_ufs_combo_info));

	combo_cmd_size = sizeof(struct vblk_ufs_combo_info) +
		sizeof(struct vblk_ufs_ioc_query_req) * combo_info->count;
	if (combo_cmd_size < sizeof(struct vblk_ufs_combo_info)) {
		dev_err(vblkdev->device,
			"combo_cmd_size is overflowing!\n");
		err = -EINVAL;
		goto free_ioc_buf;
	}

	if (combo_cmd_size > ioctl_bytes) {
		dev_err(vblkdev->device,
			" buffer has no enough space to serve ioctl\n");
		err = -EFAULT;
		goto free_ioc_buf;
	}
	memset(&ic, 0, sizeof(ic));
	tmpaddr = (uint8_t *)&ic;
	for (i = 0; i < combo_info->count; i++) {
		if (copy_from_user((void *)tmpaddr, usr_ptr, sizeof(ic))) {
			err = -EFAULT;
			goto free_ioc_buf;
		}

		err = vblk_validate_single_query_io(vblkdev,
				(struct ufs_ioc_query_req*)tmpaddr,
				&data_len, &w_flag);
		if (err) {
			dev_err(vblkdev->device, "Validating request failed\n");
			err = -EFAULT;
			goto free_ioc_buf;
		}

		combo_cmd->opcode = ic.opcode;
		combo_cmd->idn = ic.idn;
		combo_cmd->index = ic.index;
		combo_cmd->selector = ic.selector;
		combo_cmd->buf_size = ic.buf_size;
		combo_cmd->delay = ic.delay;
		combo_cmd->error_status = ic.error_status;
		combo_cmd->buffer_offset = combo_cmd_size;

		combo_cmd_size += data_len;
		if ((combo_cmd_size < data_len) ||
				(combo_cmd_size > ioctl_bytes)) {
			dev_err(vblkdev->device,
				" buffer has no enough space to serve ioctl\n");
			err = -EFAULT;
			goto free_ioc_buf;
		}

		if (w_flag && data_len) {
			if (copy_from_user((
				(void *)ioctl_buf +
				combo_cmd->buffer_offset),
				(void __user *)(unsigned long)ic.buffer,
				(u64)data_len))
			{
				dev_err(vblkdev->device,
					"copy from user failed for data!\n");
				err = -EFAULT;
				goto free_ioc_buf;
			}
		}
		combo_cmd++;
		usr_ptr++;
	}

	ioctl_req->ioctl_id = VBLK_UFS_COMBO_IO_ID;
	ioctl_req->ioctl_buf = ioctl_buf;
	ioctl_req->ioctl_len = ioctl_bytes;

free_ioc_buf:
	if (err && ioctl_buf)
		vfree(ioctl_buf);

	return err;
}

int vblk_complete_ufs_combo_ioc(struct vblk_dev *vblkdev,
		struct vblk_ioctl_req *ioctl_req,
		void __user *user,
		uint32_t cmd)
{
	uint64_t num_cmd;
	struct ufs_ioc_combo_query_req cc;
	struct ufs_ioc_query_req ic;
	struct ufs_ioc_query_req *ic_ptr = &ic;
	struct ufs_ioc_combo_query_req __user *user_cmd;
	struct ufs_ioc_query_req __user *usr_ptr;
	struct vblk_ufs_ioc_query_req *combo_cmd;
	uint32_t i;
	int err = 0;
	size_t data_len;
	bool w_flag = false;

	void *ioctl_buf = ioctl_req->ioctl_buf;

	if (ioctl_req->status) {
		err = ioctl_req->status;
		if (ioctl_req->ioctl_buf)
			vfree(ioctl_req->ioctl_buf);
		goto exit;
	}

	user_cmd = (struct ufs_ioc_combo_query_req __user *)user;
	if (copy_from_user(&cc, user_cmd,
				sizeof(cc))) {
		err = -EFAULT;
		goto free_ioc_buf;
	}
	num_cmd = cc.num_cmds;
	if (num_cmd > MAX_QUERY_CMD_PER_COMBO) {
		err = -EINVAL;
		goto free_ioc_buf;
	}

	usr_ptr = (void * __user)cc.query;

	combo_cmd = (struct vblk_ufs_ioc_query_req *)(ioctl_buf +
			sizeof(struct vblk_ufs_combo_info));

	for (i = 0; i < num_cmd; i++) {
		if (copy_from_user((void *)ic_ptr, usr_ptr,
			sizeof(struct ufs_ioc_query_req))) {
			err = -EFAULT;
			goto free_ioc_buf;
		}

		err = vblk_validate_single_query_io(vblkdev, ic_ptr,
				&data_len, &w_flag);
		if (err) {
			dev_err(vblkdev->device, "Validating request failed\n");
			err = -EFAULT;
			goto free_ioc_buf;
		}

		err = copy_to_user(&usr_ptr->buf_size, &combo_cmd->buf_size,
				sizeof(combo_cmd->buf_size));
		if (err) {
			dev_err(vblkdev->device, "Failed copy_to_user query_req buf_size\n");
			err = -EFAULT;
			goto free_ioc_buf;
		}

		err = copy_to_user(&usr_ptr->error_status, &combo_cmd->error_status,
				sizeof(combo_cmd->error_status));
		if (err) {
			dev_err(vblkdev->device, "Failed copy_to_user query_req status\n");
			err = -EFAULT;
			goto free_ioc_buf;
		}

		if (!w_flag && data_len) {
			if (copy_to_user(
				(void __user *)(unsigned long)ic.buffer,
				(ioctl_buf + combo_cmd->buffer_offset),
				(u64)data_len))
			{
				dev_err(vblkdev->device,
					"copy to user of ioctl data failed!\n");
				err = -EFAULT;
				goto free_ioc_buf;
			}
		}
		combo_cmd++;
		usr_ptr++;
	}

free_ioc_buf:
	if (ioctl_buf)
		vfree(ioctl_buf);

exit:
	return err;
}
