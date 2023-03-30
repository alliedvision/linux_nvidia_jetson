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
#include <nvgpu/types.h>
#include <nvgpu/os_fence.h>
#include <nvgpu/linux/os_fence_dma.h>
#include <nvgpu/channel.h>
#include <nvgpu/nvhost.h>

#include <linux/dma-fence.h>
#include <linux/sync_file.h>
#include <linux/file.h>

#include "os_fence_priv.h"

inline struct dma_fence *nvgpu_get_dma_fence(struct nvgpu_os_fence *s)
{
	struct dma_fence *fence = (struct dma_fence *)s->priv;
	return fence;
}

void nvgpu_os_fence_dma_drop_ref(struct nvgpu_os_fence *s)
{
	struct dma_fence *fence = nvgpu_get_dma_fence(s);

	dma_fence_put(fence);

	nvgpu_os_fence_clear(s);
}

int nvgpu_os_fence_dma_install_fd(struct nvgpu_os_fence *s, int fd)
{
	struct dma_fence *fence = nvgpu_get_dma_fence(s);
	struct sync_file *file = sync_file_create(fence);

	if (file == NULL) {
		return -ENOMEM;
	}

	fd_install(fd, file->file);
	return 0;
}

void nvgpu_os_fence_dma_dup(struct nvgpu_os_fence *s)
{
	struct dma_fence *fence = nvgpu_get_dma_fence(s);

	dma_fence_get(fence);
}

int nvgpu_os_fence_fdget(struct nvgpu_os_fence *fence_out,
	struct nvgpu_channel *c, int fd)
{
	int err = -ENOSYS;

#ifdef CONFIG_TEGRA_GK20A_NVHOST
	if (nvgpu_has_syncpoints(c->g)) {
		err = nvgpu_os_fence_syncpt_fdget(fence_out, c, fd);
	}
#endif

	if (err) {
		err = nvgpu_os_fence_sema_fdget(fence_out, c, fd);
	}

	if (err) {
		nvgpu_err(c->g, "error obtaining fence from fd %d", fd);
	}

	return err;
}
