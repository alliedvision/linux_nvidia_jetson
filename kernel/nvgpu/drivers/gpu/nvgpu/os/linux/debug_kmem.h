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

#ifndef __NVGPU_DEBUG_KMEM_H__
#define __NVGPU_DEBUG_KMEM_H__

struct gk20a;
#ifdef CONFIG_NVGPU_TRACK_MEM_USAGE
void nvgpu_kmem_debugfs_init(struct gk20a *g);
#endif

#endif /* __NVGPU_DEBUG_KMEM_H__ */
