/*
 * Copyright (c) 2021-2022, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef NVGPU_OS_LINUX_IOCTL_NVS_H
#define NVGPU_OS_LINUX_IOCTL_NVS_H

#include <nvgpu/types.h>

struct inode;
struct file;

int     nvgpu_nvs_dev_open(struct inode *inode, struct file *filp);
int     nvgpu_nvs_dev_release(struct inode *inode, struct file *filp);
long    nvgpu_nvs_dev_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);
ssize_t nvgpu_nvs_dev_read(struct file *filp, char __user *buf,
			   size_t size, loff_t *off);
struct nvgpu_nvs_domain *nvgpu_nvs_domain_get_from_file(int fd);

#endif
