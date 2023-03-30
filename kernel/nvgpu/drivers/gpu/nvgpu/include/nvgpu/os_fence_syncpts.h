/*
 * nvgpu os fence syncpts
 *
 * Copyright (c) 2018-2022, NVIDIA CORPORATION.  All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef NVGPU_OS_FENCE_SYNCPT_H
#define NVGPU_OS_FENCE_SYNCPT_H

#if defined(CONFIG_TEGRA_GK20A_NVHOST_HOST1X)
#include <linux/types.h>
#endif

#include <nvgpu/errno.h>

struct nvgpu_os_fence;
struct nvgpu_nvhost_dev;

struct nvgpu_os_fence_syncpt {
	struct nvgpu_os_fence *fence;
};

#if defined(CONFIG_TEGRA_GK20A_NVHOST_HOST1X)
struct nvhost_ctrl_sync_fence_info {
	__u32 id;
	__u32 thresh;
};
#else
struct nvhost_ctrl_sync_fence_info;
#endif

#if defined(CONFIG_TEGRA_GK20A_NVHOST) && !defined(CONFIG_NVGPU_SYNCFD_NONE)

int nvgpu_os_fence_syncpt_create(struct nvgpu_os_fence *fence_out,
	struct nvgpu_channel *c, struct nvgpu_nvhost_dev *nvhost_device,
	u32 id, u32 thresh);

/*
 * Return a struct of nvgpu_os_fence_syncpt only if the underlying os_fence
 * object is backed by syncpoints, else return empty object.
 */
int nvgpu_os_fence_get_syncpts(struct nvgpu_os_fence_syncpt *fence_syncpt_out,
	struct nvgpu_os_fence *fence_in);

/*
 * Go through the id/value pairs inside a sync fd and call the supplied iter
 * callback for each, providing the given data pointer as the second argument.
 * The first argument contains the syncpt id and threshold for each individual
 * fence.
 */
int nvgpu_os_fence_syncpt_foreach_pt(
	struct nvgpu_os_fence_syncpt *fence,
	int (*iter)(struct nvhost_ctrl_sync_fence_info, void *),
	void *data);

/*
 * This method returns the number of underlying syncpoints
 * and should be called only on a valid instance of type
 * nvgpu_os_fence_syncpt.
 */
u32 nvgpu_os_fence_syncpt_get_num_syncpoints(
	struct nvgpu_os_fence_syncpt *fence);

#else /* CONFIG_TEGRA_GK20A_NVHOST && !CONFIG_NVGPU_SYNCFD_NONE */

static inline int nvgpu_os_fence_syncpt_create(
	struct nvgpu_os_fence *fence_out, struct nvgpu_channel *c,
	struct nvgpu_nvhost_dev *nvhost_device,
	u32 id, u32 thresh)
{
	(void)fence_out;
	(void)c;
	(void)nvhost_device;
	(void)id;
	(void)thresh;
	return -ENOSYS;
}

static inline int nvgpu_os_fence_get_syncpts(
	struct nvgpu_os_fence_syncpt *fence_syncpt_out,
	struct nvgpu_os_fence *fence_in)
{
	(void)fence_syncpt_out;
	(void)fence_in;
	return -EINVAL;
}

static inline u32 nvgpu_os_fence_syncpt_get_num_syncpoints(
	struct nvgpu_os_fence_syncpt *fence)
{
	(void)fence;
	return 0;
}

#endif /* CONFIG_TEGRA_GK20A_NVHOST && !CONFIG_NVGPU_SYNCFD_NONE */

#endif /* NVGPU_OS_FENCE_SYNPT_H */
