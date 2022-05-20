/*
 * Copyright (c) 2018-2020, NVIDIA CORPORATION.  All rights reserved.
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

#if !defined(CONFIG_TEGRA_GK20A_NVHOST_HOST1X)
#include <uapi/linux/nvhost_ioctl.h>
#endif

#include <linux/err.h>
#include <nvgpu/errno.h>

#include <nvgpu/types.h>
#include <nvgpu/os_fence.h>
#include <nvgpu/os_fence_syncpts.h>
#include <nvgpu/nvhost.h>
#include <nvgpu/atomic.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/channel.h>
#include <nvgpu/channel_sync.h>

#include "os_fence_priv.h"
#include "nvhost_priv.h"

static int nvgpu_os_fence_syncpt_install_fd(struct nvgpu_os_fence *s,
		int fd)
{
	return nvgpu_nvhost_fence_install(s->priv, fd);
}

static void nvgpu_os_fence_syncpt_drop_ref(struct nvgpu_os_fence *s)
{
	nvgpu_nvhost_fence_put(s->priv);
	nvgpu_os_fence_clear(s);
}

static void nvgpu_os_fence_syncpt_dup(struct nvgpu_os_fence *s)
{
	nvgpu_nvhost_fence_dup(s->priv);
}

static const struct nvgpu_os_fence_ops syncpt_ops = {
	.drop_ref = nvgpu_os_fence_syncpt_drop_ref,
	.install_fence = nvgpu_os_fence_syncpt_install_fd,
	.dup = nvgpu_os_fence_syncpt_dup,
};

int nvgpu_os_fence_syncpt_create(struct nvgpu_os_fence *fence_out,
		struct nvgpu_channel *c, struct nvgpu_nvhost_dev *nvhost_device,
		u32 id, u32 thresh)
{
	struct nvhost_ctrl_sync_fence_info pt = {
		.id = id,
		.thresh = thresh,
	};
	struct nvhost_fence *fence;

	fence = nvgpu_nvhost_fence_create(nvhost_device->host1x_pdev, &pt, 1,
					  "fence");
	if (IS_ERR(fence)) {
		nvgpu_err(c->g, "error %d during construction of fence.",
			(int)PTR_ERR(fence));
		return PTR_ERR(fence);
	}

	nvgpu_os_fence_init(fence_out, c->g, &syncpt_ops, fence);

	return 0;
}

int nvgpu_os_fence_syncpt_fdget(struct nvgpu_os_fence *fence_out,
	struct nvgpu_channel *c, int fd)
{
	struct nvhost_fence *fence = nvgpu_nvhost_fence_get(fd);

	if (fence == NULL) {
		return -ENOMEM;
	}

	nvgpu_os_fence_init(fence_out, c->g, &syncpt_ops, fence);

	return 0;
}

int nvgpu_os_fence_get_syncpts(
	struct nvgpu_os_fence_syncpt *fence_syncpt_out,
	struct nvgpu_os_fence *fence_in)
{
	if (fence_in->ops != &syncpt_ops) {
		return -EINVAL;
	}

	fence_syncpt_out->fence = fence_in;

	return 0;
}

u32 nvgpu_os_fence_syncpt_get_num_syncpoints(
	struct nvgpu_os_fence_syncpt *fence)
{
	return nvgpu_nvhost_fence_num_pts(fence->fence->priv);
}

int nvgpu_os_fence_syncpt_foreach_pt(struct nvgpu_os_fence_syncpt *fence,
        int (*iter)(struct nvhost_ctrl_sync_fence_info, void *),
        void *data)
{
	return nvgpu_nvhost_fence_foreach_pt(fence->fence->priv, iter, data);
}
