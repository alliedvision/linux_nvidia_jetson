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

#include "debug_allocator.h"
#include "os_linux.h"

#include <linux/debugfs.h>
#include <linux/seq_file.h>

#include <nvgpu/allocator.h>

static int __alloc_show(struct seq_file *s, void *unused)
{
	struct nvgpu_allocator *a = s->private;

	nvgpu_alloc_print_stats(a, s, 1);

	return 0;
}

static int __alloc_open(struct inode *inode, struct file *file)
{
	return single_open(file, __alloc_show, inode->i_private);
}

static const struct file_operations __alloc_fops = {
	.open = __alloc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

void nvgpu_init_alloc_debug(struct gk20a *g, struct nvgpu_allocator *a)
{
	struct nvgpu_os_linux *l = nvgpu_os_linux_from_gk20a(g);

	if (!l->debugfs_allocators)
		return;

	a->debugfs_entry = debugfs_create_file(a->name, S_IRUGO,
					       l->debugfs_allocators,
					       a, &__alloc_fops);
}

void nvgpu_fini_alloc_debug(struct nvgpu_allocator *a)
{
	struct nvgpu_os_linux *l = nvgpu_os_linux_from_gk20a(a->g);

	if (!l->debugfs_allocators)
		return;

	debugfs_remove(a->debugfs_entry);
}

void nvgpu_alloc_debugfs_init(struct gk20a *g)
{
	struct nvgpu_os_linux *l = nvgpu_os_linux_from_gk20a(g);

	l->debugfs_allocators = debugfs_create_dir("allocators", l->debugfs);
	if (IS_ERR_OR_NULL(l->debugfs_allocators)) {
		l->debugfs_allocators = NULL;
		return;
	}
}
