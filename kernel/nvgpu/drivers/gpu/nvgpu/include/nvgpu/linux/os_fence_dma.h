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

#ifndef NVGPU_LINUX_OS_FENCE_DMA_H
#define NVGPU_LINUX_OS_FENCE_DMA_H

struct gk20a;
struct nvgpu_os_fence;
struct dma_fence;
struct nvgpu_channel;

struct dma_fence *nvgpu_get_dma_fence(struct nvgpu_os_fence *s);

void nvgpu_os_fence_dma_drop_ref(struct nvgpu_os_fence *s);

int nvgpu_os_fence_sema_fdget(struct nvgpu_os_fence *fence_out,
		struct nvgpu_channel *c, int fd);

int nvgpu_os_fence_dma_install_fd(struct nvgpu_os_fence *s, int fd);
void nvgpu_os_fence_dma_dup(struct nvgpu_os_fence *s);

int nvgpu_os_fence_syncpt_fdget(struct nvgpu_os_fence *fence_out,
		struct nvgpu_channel *c, int fd);

#endif /* NVGPU_LINUX_OS_FENCE_DMA_H */

