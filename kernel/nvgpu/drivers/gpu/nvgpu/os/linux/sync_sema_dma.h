/*
 * Semaphore Sync Framework Integration
 *
 * Copyright (c) 2020, NVIDIA Corporation.  All rights reserved.
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

#ifndef NVGPU_OS_LINUX_SYNC_SEMA_DMA_H
#define NVGPU_OS_LINUX_SYNC_SEMA_DMA_H

#ifdef CONFIG_NVGPU_SYNCFD_STABLE
#include <nvgpu/types.h>

struct nvgpu_channel;
struct nvgpu_semaphore;
struct dma_fence;

u64 nvgpu_sync_dma_context_create(void);

struct dma_fence *nvgpu_sync_dma_create(struct nvgpu_channel *c,
		struct nvgpu_semaphore *sema);

void nvgpu_sync_dma_signal(struct dma_fence *fence);
u32 nvgpu_dma_fence_length(struct dma_fence *fence);
struct nvgpu_semaphore *nvgpu_dma_fence_nth(struct dma_fence *fence, u32 i);

struct dma_fence *nvgpu_sync_dma_fence_fdget(int fd);
#endif /* CONFIG_NVGPU_SYNCFD_STABLE */

#endif
