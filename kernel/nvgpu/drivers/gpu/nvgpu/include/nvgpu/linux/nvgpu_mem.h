/*
 * Copyright (c) 2017, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef __NVGPU_LINUX_NVGPU_MEM_H__
#define __NVGPU_LINUX_NVGPU_MEM_H__

struct page;
struct sg_table;
struct scatterlist;
struct nvgpu_sgt;

struct gk20a;
struct nvgpu_mem;
struct nvgpu_gmmu_attrs;

struct nvgpu_mem_priv {
	struct page **pages;
	struct sg_table *sgt;
	unsigned long flags;
};

u64 nvgpu_mem_get_addr_sgl(struct gk20a *g, struct scatterlist *sgl);
struct nvgpu_sgt *nvgpu_mem_linux_sgt_create(struct gk20a *g,
					   struct sg_table *sgt);
void nvgpu_mem_linux_sgt_free(struct gk20a *g, struct nvgpu_sgt *sgt);
struct nvgpu_sgt *nvgpu_linux_sgt_create(struct gk20a *g,
					   struct sg_table *sgt);

#endif
