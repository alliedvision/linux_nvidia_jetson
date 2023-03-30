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
#ifndef LINUX_IOCTL_PROF_H
#define LINUX_IOCTL_PROF_H

#include <nvgpu/types.h>

struct inode;
struct file;
#if defined(CONFIG_NVGPU_NON_FUSA)
struct nvgpu_profiler_object;
#endif

int nvgpu_prof_dev_fops_open(struct inode *inode, struct file *filp);
int nvgpu_prof_ctx_fops_open(struct inode *inode, struct file *filp);
int nvgpu_prof_fops_release(struct inode *inode, struct file *filp);
long nvgpu_prof_fops_ioctl(struct file *filp, unsigned int cmd,
		unsigned long arg);

#endif /* LINUX_IOCTL_PROF_H */
