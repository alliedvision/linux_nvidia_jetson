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

#include <nvgpu/errno.h>
#include <nvgpu/types.h>
#include <nvgpu/os_fence.h>
#include <nvgpu/os_fence_semas.h>
#include <nvgpu/linux/os_fence_dma.h>
#include <nvgpu/semaphore.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/channel.h>

#include "os_fence_priv.h"
#include "sync_sema_dma.h"

static const struct nvgpu_os_fence_ops sema_ops = {
	.drop_ref = nvgpu_os_fence_dma_drop_ref,
	.install_fence = nvgpu_os_fence_dma_install_fd,
	.dup = nvgpu_os_fence_dma_dup,
};

int nvgpu_os_fence_get_semas(struct nvgpu_os_fence_sema *fence_sema_out,
		struct nvgpu_os_fence *fence_in)
{
	if (fence_in->ops != &sema_ops) {
		return -EINVAL;
	}

	fence_sema_out->fence = fence_in;

	return 0;
}

u32 nvgpu_os_fence_sema_get_num_semaphores(struct nvgpu_os_fence_sema *fence)
{
	struct dma_fence *f = nvgpu_get_dma_fence(fence->fence);

	return nvgpu_dma_fence_length(f);
}

void nvgpu_os_fence_sema_extract_nth_semaphore(
		struct nvgpu_os_fence_sema *fence, u32 n,
		struct nvgpu_semaphore **semaphore_out)
{
	struct dma_fence *f = nvgpu_get_dma_fence(fence->fence);

	*semaphore_out = nvgpu_dma_fence_nth(f, n);
}

int nvgpu_os_fence_sema_create(struct nvgpu_os_fence *fence_out,
		struct nvgpu_channel *c, struct nvgpu_semaphore *sema)
{
	struct dma_fence *fence = nvgpu_sync_dma_create(c, sema);

	if (!fence) {
		nvgpu_err(c->g, "error constructing new fence");
		return -ENOMEM;
	}

	nvgpu_os_fence_init(fence_out, c->g, &sema_ops, fence);

	return 0;
}

int nvgpu_os_fence_sema_fdget(struct nvgpu_os_fence *fence_out,
		struct nvgpu_channel *c, int fd)
{
	struct dma_fence *fence = nvgpu_sync_dma_fence_fdget(fd);

	if (fence == NULL)
		return -EINVAL;

	nvgpu_os_fence_init(fence_out, c->g, &sema_ops, fence);

	return 0;
}
