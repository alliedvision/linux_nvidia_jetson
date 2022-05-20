/*
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

#ifndef NVGPU_POWER_LINUX_H
#define NVGPU_POWER_LINUX_H

#include <nvgpu/types.h>

int gk20a_power_open(struct inode *inode, struct file *filp);
ssize_t gk20a_power_read(struct file *filp, char __user *buf,
		                size_t size, loff_t *off);
ssize_t gk20a_power_write(struct file *filp, const char __user *buf,
		                size_t size, loff_t *off);
int gk20a_power_release(struct inode *inode, struct file *filp);

#endif
