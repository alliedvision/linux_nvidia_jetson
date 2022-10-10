/*
 * Copyright (c) 2019-2021, NVIDIA CORPORATION. All rights reserved.
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

#ifndef __NVSCIIPC_KERNEL_H__
#define __NVSCIIPC_KERNEL_H__

#include <linux/nvscierror.h>
#include <linux/nvsciipc_interface.h>
#include <uapi/linux/nvsciipc_ioctl.h>

#define ERR(...) pr_err("nvsciipc: " __VA_ARGS__)
#define DBG(...) pr_info("nvsciipc: " __VA_ARGS__)
#define INFO(...) pr_info("nvsciipc: " __VA_ARGS__)

#define MODULE_NAME             "nvsciipc"
#define MAX_NAME_SIZE           64

struct nvsciipc {
	struct device *dev;

	dev_t dev_t;
	struct class *nvsciipc_class;
	struct cdev cdev;
	struct device *device;
	char device_name[MAX_NAME_SIZE];

	int num_eps;
	struct nvsciipc_config_entry **db;
};

/***********************************************************************/
/********************* Functions declaration ***************************/
/***********************************************************************/

static void nvsciipc_cleanup(struct nvsciipc *ctx);

static int nvsciipc_dev_open(struct inode *inode, struct file *filp);
static int nvsciipc_dev_release(struct inode *inode, struct file *filp);
static long nvsciipc_dev_ioctl(struct file *filp, unsigned int cmd,
			       unsigned long arg);
static int nvsciipc_ioctl_get_vuid(struct nvsciipc *ctx, unsigned int cmd,
				   unsigned long arg);
static int nvsciipc_ioctl_set_db(struct nvsciipc *ctx, unsigned int cmd,
				 unsigned long arg);

#endif /* __NVSCIIPC_KERNEL_H__ */
