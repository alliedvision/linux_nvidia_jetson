/*
 * Copyright (C) 2017-2020 NVIDIA CORPORATION.  All rights reserved.
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

#include <linux/version.h>
#include <linux/nvhost.h>

#include <linux/dma-fence.h>
#include <linux/dma-fence-array.h>

#include <linux/fcntl.h>
#include <linux/file.h>
#include <linux/sync_file.h>

#include "dev.h"
#include "nvhost_sync_v2.h"

struct nvhost_fence *nvhost_fence_create(
		struct platform_device *pdev,
		struct nvhost_ctrl_sync_fence_info *pts,
		u32 num_pts,
		const char *name)
{
	struct nvhost_master *master = nvhost_get_host(pdev);
	struct nvhost_syncpt *syncpt = &master->syncpt;

	return (struct nvhost_fence *)nvhost_dma_fence_create(syncpt, pts,
							      num_pts);
}
EXPORT_SYMBOL(nvhost_fence_create);

int nvhost_fence_create_fd(
		struct platform_device *pdev,
		struct nvhost_ctrl_sync_fence_info *pts,
		u32 num_pts,
		const char *name,
		s32 *fence_fd)
{
	struct sync_file *file;
	struct dma_fence *f;
	int err;

	f = (struct dma_fence *)nvhost_fence_create(pdev, pts, num_pts, NULL);
	if (!f)
		return -ENOMEM;

	err = get_unused_fd_flags(O_CLOEXEC);
	if (err < 0)
		goto put_fence;

	file = sync_file_create(f);
	if (!file)
		goto put_fd;

	/* sync_file_create takes an additional ref, so drop ours. */
	dma_fence_put(f);

	fd_install(err, file->file);
	*fence_fd = err;

	return 0;

put_fd:
	put_unused_fd(err);
put_fence:
	dma_fence_put(f);

	return -ENOMEM;
}
EXPORT_SYMBOL(nvhost_fence_create_fd);

int nvhost_fence_install(struct nvhost_fence *fence, int fd)
{
	struct dma_fence *f = (struct dma_fence *)fence;
	struct sync_file *file = sync_file_create(f);

	if (!file)
		return -ENOMEM;

	fd_install(fd, file->file);
	return 0;
}
EXPORT_SYMBOL(nvhost_fence_install);

struct nvhost_fence *nvhost_fence_get(int fd)
{
	struct dma_fence *f = sync_file_get_fence(fd);
	if (!f)
		return NULL;

	if (!nvhost_dma_fence_is_waitable(f)) {
		dma_fence_put(f);
		return NULL;
	}

	return (struct nvhost_fence *)f;
}
EXPORT_SYMBOL(nvhost_fence_get);

struct nvhost_fence *nvhost_fence_dup(struct nvhost_fence *fence)
{
	dma_fence_get((struct dma_fence *)fence);

	return fence;
}
EXPORT_SYMBOL(nvhost_fence_dup);

int nvhost_fence_num_pts(struct nvhost_fence *fence)
{
	struct dma_fence_array *array;

	array = to_dma_fence_array((struct dma_fence *)fence);
	if (!array)
		return 1;

	return array->num_fences;
}
EXPORT_SYMBOL(nvhost_fence_num_pts);

int nvhost_fence_foreach_pt(
	struct nvhost_fence *fence,
	int (*iter)(struct nvhost_ctrl_sync_fence_info, void *),
	void *data)
{
	struct nvhost_ctrl_sync_fence_info info;
	struct dma_fence_array *array;
	int i, err;

	array = to_dma_fence_array((struct dma_fence *)fence);
	if (!array) {
		nvhost_dma_fence_unpack((struct dma_fence *)fence, &info.id,
					&info.thresh);
		return iter(info, data);
	}

	for (i = 0; i < array->num_fences; ++i) {
		nvhost_dma_fence_unpack(array->fences[i], &info.id,
					&info.thresh);
		err = iter(info, data);
		if (err)
			return err;
	}

	return 0;
}
EXPORT_SYMBOL(nvhost_fence_foreach_pt);

int nvhost_fence_get_pt(
	struct nvhost_fence *fence, size_t i,
	u32 *id, u32 *threshold)
{
	struct dma_fence *f = (struct dma_fence *)fence;
	struct dma_fence_array *array;

	array = to_dma_fence_array(f);
	if (!array) {
		if (i != 0)
			return -EINVAL;

		nvhost_dma_fence_unpack(f, id, threshold);

		return 0;
	}

	if (i >= array->num_fences)
		return -EINVAL;

	nvhost_dma_fence_unpack(array->fences[i], id, threshold);

	return 0;
}
EXPORT_SYMBOL(nvhost_fence_get_pt);

void nvhost_fence_put(struct nvhost_fence *fence)
{
	dma_fence_put((struct dma_fence *)fence);
}
EXPORT_SYMBOL(nvhost_fence_put);

void nvhost_fence_wait(struct nvhost_fence *fence, u32 timeout_in_ms)
{
	dma_fence_wait_timeout((struct dma_fence *)fence, true,
				msecs_to_jiffies(timeout_in_ms));
}
EXPORT_SYMBOL(nvhost_fence_wait);
