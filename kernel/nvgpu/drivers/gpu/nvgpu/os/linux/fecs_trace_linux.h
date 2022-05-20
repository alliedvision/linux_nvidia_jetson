/*
 * Copyright (c) 2017-2020, NVIDIA Corporation.  All rights reserved.
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

#ifndef NVGPU_FECS_TRACE_LINUX_H
#define NVGPU_FECS_TRACE_LINUX_H

#include <nvgpu/types.h>

#define GK20A_CTXSW_TRACE_NUM_DEVS			1
#define GK20A_CTXSW_TRACE_MAX_VM_RING_SIZE	(128*NVGPU_CPU_PAGE_SIZE)

struct file;
struct inode;
struct gk20a;
struct nvgpu_tsg;
struct nvgpu_channel;
struct vm_area_struct;
struct poll_table_struct;

int gk20a_ctxsw_trace_init(struct gk20a *g);
void gk20a_ctxsw_trace_cleanup(struct gk20a *g);

int gk20a_ctxsw_dev_mmap(struct file *filp, struct vm_area_struct *vma);

int gk20a_ctxsw_dev_release(struct inode *inode, struct file *filp);
int gk20a_ctxsw_dev_open(struct inode *inode, struct file *filp);
long gk20a_ctxsw_dev_ioctl(struct file *filp,
			 unsigned int cmd, unsigned long arg);
ssize_t gk20a_ctxsw_dev_read(struct file *filp, char __user *buf,
			     size_t size, loff_t *offs);
unsigned int gk20a_ctxsw_dev_poll(struct file *filp,
				  struct poll_table_struct *pts);

#endif /*NVGPU_FECS_TRACE_LINUX_H */
