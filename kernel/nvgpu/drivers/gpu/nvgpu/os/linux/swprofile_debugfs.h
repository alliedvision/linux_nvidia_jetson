/*
 * Copyright (c) 2020, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef __NVGPU_SWPROFILE_DEBUGFS_H__
#define __NVGPU_SWPROFILE_DEBUGFS_H__

struct dentry;

struct gk20a;
struct nvgpu_swprofiler;

void nvgpu_debugfs_swprofile_init(struct gk20a *g,
				  struct dentry *root,
				  struct nvgpu_swprofiler *p,
				  const char *name);

#endif /* __NVGPU_SWPROFILE_DEBUGFS_H__ */
