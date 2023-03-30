/*
 * Copyright (c) 2020-2022, NVIDIA CORPORATION.  All rights reserved.
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

#include <linux/dma-fence.h>
#include <linux/file.h>
#include <linux/host1x-next.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/sync_file.h>

#include <nvgpu/os_fence.h>
#include <nvgpu/os_fence_syncpts.h>
#include <nvgpu/nvhost.h>
#include <nvgpu/gk20a.h>

#include "nvhost_priv.h"

#define TEGRA194_SYNCPT_PAGE_SIZE 0x1000
#define TEGRA194_SYNCPT_SHIM_BASE 0x60000000
#define TEGRA194_SYNCPT_SHIM_SIZE 0x00400000
#define TEGRA234_SYNCPT_PAGE_SIZE 0x10000
#define TEGRA234_SYNCPT_SHIM_BASE 0x60000000
#define TEGRA234_SYNCPT_SHIM_SIZE 0x04000000

static const struct of_device_id host1x_match[] = {
	{ .compatible = "nvidia,tegra186-host1x", },
	{ .compatible = "nvidia,tegra194-host1x", },
	{ .compatible = "nvidia,tegra234-host1x", },
	{},
};

int nvgpu_get_nvhost_dev(struct gk20a *g)
{
	struct platform_device *host1x_pdev;
	struct device_node *np;

	np = of_find_matching_node(NULL, host1x_match);
	if (!np) {
		nvgpu_warn(g, "Failed to find host1x, syncpt support disabled");
		nvgpu_set_enabled(g, NVGPU_HAS_SYNCPOINTS, false);
		return -ENOSYS;
	}

	host1x_pdev = of_find_device_by_node(np);
	if (!host1x_pdev) {
		nvgpu_warn(g, "host1x device not available");
		return -EPROBE_DEFER;
	}

	g->nvhost = nvgpu_kzalloc(g, sizeof(struct nvgpu_nvhost_dev));
	if (!g->nvhost)
		return -ENOMEM;

	g->nvhost->host1x_pdev = host1x_pdev;

	return 0;
}

int nvgpu_nvhost_module_busy_ext(struct nvgpu_nvhost_dev *nvhost_dev)
{
	return 0;
}

void nvgpu_nvhost_module_idle_ext(struct nvgpu_nvhost_dev *nvhost_dev) { }

void nvgpu_nvhost_debug_dump_device(struct nvgpu_nvhost_dev *nvhost_dev) { }

const char *nvgpu_nvhost_syncpt_get_name(struct nvgpu_nvhost_dev *nvhost_dev,
					 int id)
{
	return NULL;
}

bool nvgpu_nvhost_syncpt_is_valid_pt_ext(struct nvgpu_nvhost_dev *nvhost_dev,
					 u32 id)
{
	struct host1x_syncpt *sp;
	struct host1x *host1x;

	host1x = platform_get_drvdata(nvhost_dev->host1x_pdev);
	if (WARN_ON(!host1x))
		return false;

	sp = host1x_syncpt_get_by_id_noref(host1x, id);
	if (!sp)
		return false;

	return true;
}

bool nvgpu_nvhost_syncpt_is_expired_ext(struct nvgpu_nvhost_dev *nvhost_dev,
					u32 id, u32 thresh)
{
	struct host1x_syncpt *sp;
	struct host1x *host1x;

	host1x = platform_get_drvdata(nvhost_dev->host1x_pdev);
	if (WARN_ON(!host1x))
		return true;

	sp = host1x_syncpt_get_by_id_noref(host1x, id);
	if (WARN_ON(!sp))
		return true;

	if (host1x_syncpt_wait(sp, thresh, 0, NULL))
		return false;

	return true;
}

struct nvgpu_host1x_cb {
	struct dma_fence_cb cb;
	struct work_struct work;
	void (*notifier)(void *, int);
	void *notifier_data;
};

static void nvgpu_host1x_work_func(struct work_struct *work)
{
	struct nvgpu_host1x_cb *host1x_cb = container_of(work, struct nvgpu_host1x_cb, work);

	host1x_cb->notifier(host1x_cb->notifier_data, 0);
	kfree_rcu(host1x_cb);
}

static void nvgpu_host1x_cb_func(struct dma_fence *f, struct dma_fence_cb *cb)
{
	struct nvgpu_host1x_cb *host1x_cb = container_of(cb, struct nvgpu_host1x_cb, cb);

	schedule_work(&host1x_cb->work);
	dma_fence_put(f);
}

int nvgpu_nvhost_intr_register_notifier(struct nvgpu_nvhost_dev *nvhost_dev,
					u32 id, u32 thresh,
					void (*notifier)(void *, int),
					void *notifier_data)
{
	struct dma_fence *fence;
	struct nvgpu_host1x_cb *cb;
	struct host1x_syncpt *sp;
	struct host1x *host1x;
	int err;

	host1x = platform_get_drvdata(nvhost_dev->host1x_pdev);
	if (!host1x)
		return -ENODEV;

	sp = host1x_syncpt_get_by_id_noref(host1x, id);
	if (!sp)
		return -EINVAL;

	fence = host1x_fence_create(sp, thresh, true);
	if (IS_ERR(fence)) {
		pr_err("error %d during construction of fence!",
			(int)PTR_ERR(fence));
		return PTR_ERR(fence);
	}

	cb = kzalloc(sizeof(*cb), GFP_KERNEL);
	if (!cb)
		return -ENOMEM;

	cb->notifier = notifier;
	cb->notifier_data = notifier_data;

	INIT_WORK(&cb->work, nvgpu_host1x_work_func);

	err = dma_fence_add_callback(fence, &cb->cb, nvgpu_host1x_cb_func);
	if (err < 0) {
		dma_fence_put(fence);
		kfree(cb);
	}

	return err;
}

void nvgpu_nvhost_syncpt_set_minval(struct nvgpu_nvhost_dev *nvhost_dev,
				    u32 id, u32 val)
{
	struct host1x_syncpt *sp;
	struct host1x *host1x;
	u32 cur;

	host1x = platform_get_drvdata(nvhost_dev->host1x_pdev);
	if (WARN_ON(!host1x))
		return;

	sp = host1x_syncpt_get_by_id_noref(host1x, id);
	if (WARN_ON(!sp))
		return;

	cur = host1x_syncpt_read(sp);

	while (cur++ < val) {
		host1x_syncpt_incr(sp);
	}
}

void nvgpu_nvhost_syncpt_put_ref_ext(struct nvgpu_nvhost_dev *nvhost_dev,
				     u32 id)
{
	struct host1x_syncpt *sp;
	struct host1x *host1x;

	host1x = platform_get_drvdata(nvhost_dev->host1x_pdev);
	if (WARN_ON(!host1x))
		return;

	sp = host1x_syncpt_get_by_id_noref(host1x, id);
	if (WARN_ON(!sp))
		return;

	host1x_syncpt_put(sp);
}

u32 nvgpu_nvhost_get_syncpt_client_managed(struct nvgpu_nvhost_dev *nvhost_dev,
					   const char *syncpt_name)
{
	struct host1x_syncpt *sp;
	struct host1x *host1x;

	host1x = platform_get_drvdata(nvhost_dev->host1x_pdev);
	if (!host1x)
		return 0;

	sp = host1x_syncpt_alloc(host1x, HOST1X_SYNCPT_CLIENT_MANAGED,
				 syncpt_name);
	if (!sp)
		return 0;

	return host1x_syncpt_id(sp);
}

int nvgpu_nvhost_syncpt_wait_timeout_ext(struct nvgpu_nvhost_dev *nvhost_dev,
					 u32 id, u32 thresh, u32 timeout,
					 u32 waiter_index)
{
	struct host1x_syncpt *sp;
	struct host1x *host1x;

	host1x = platform_get_drvdata(nvhost_dev->host1x_pdev);
	if (!host1x)
		return -ENODEV;

	sp = host1x_syncpt_get_by_id_noref(host1x, id);
	if (!sp)
		return -EINVAL;

	return host1x_syncpt_wait(sp, thresh, timeout, NULL);
}

int nvgpu_nvhost_syncpt_read_ext_check(struct nvgpu_nvhost_dev *nvhost_dev,
				       u32 id, u32 *val)
{
	struct host1x_syncpt *sp;
	struct host1x *host1x;

	host1x = platform_get_drvdata(nvhost_dev->host1x_pdev);
	if (!host1x)
		return -ENODEV;

	sp = host1x_syncpt_get_by_id_noref(host1x, id);
	if (!sp)
		return -EINVAL;

	*val = host1x_syncpt_read(sp);

	return 0;
}

void nvgpu_nvhost_syncpt_set_safe_state(struct nvgpu_nvhost_dev *nvhost_dev,
					u32 id)
{
	struct host1x_syncpt *sp;
	struct host1x *host1x;
	u32 val, cur;

	host1x = platform_get_drvdata(nvhost_dev->host1x_pdev);
	if (WARN_ON(!host1x))
		return;

	/*
	 * Add large number of increments to current value
	 * so that all waiters on this syncpoint are released
	 */

	sp = host1x_syncpt_get_by_id_noref(host1x, id);
	if (WARN_ON(!sp))
		return;

	cur = host1x_syncpt_read(sp);
	val = cur + 1000;

	while (cur++ != val)
		host1x_syncpt_incr(sp);
}

int nvgpu_nvhost_get_syncpt_aperture(struct nvgpu_nvhost_dev *nvhost_dev,
				     u64 *base, size_t *size)
{
	struct device_node *np = nvhost_dev->host1x_pdev->dev.of_node;

	if (of_device_is_compatible(np, "nvidia,tegra194-host1x")) {
		*base = TEGRA194_SYNCPT_SHIM_BASE;
		*size = TEGRA194_SYNCPT_SHIM_SIZE;
		return 0;
	}

	if (of_device_is_compatible(np, "nvidia,tegra234-host1x")) {
		*base = TEGRA234_SYNCPT_SHIM_BASE;
		*size = TEGRA234_SYNCPT_SHIM_SIZE;
		return 0;
	}

	return -ENOTSUPP;
}

u32 nvgpu_nvhost_syncpt_unit_interface_get_byte_offset(struct gk20a *g,
						       u32 syncpt_id)
{
	struct platform_device *host1x_pdev = g->nvhost->host1x_pdev;
	struct device_node *np = host1x_pdev->dev.of_node;

	if (of_device_is_compatible(np, "nvidia,tegra194-host1x"))
		return syncpt_id * TEGRA194_SYNCPT_PAGE_SIZE;

	if (of_device_is_compatible(np, "nvidia,tegra234-host1x"))
		return syncpt_id * TEGRA234_SYNCPT_PAGE_SIZE;

	return 0;
}

int nvgpu_nvhost_fence_install(struct nvhost_fence *fence, int fd)
{
	struct dma_fence *f = (struct dma_fence *)fence;
	struct sync_file *file = sync_file_create(f);

	if (!file)
		return -ENOMEM;

	dma_fence_get(f);
	fd_install(fd, file->file);

	return 0;
}

void nvgpu_nvhost_fence_put(struct nvhost_fence *fence)
{
	dma_fence_put((struct dma_fence *)fence);
}

void nvgpu_nvhost_fence_dup(struct nvhost_fence *fence)
{
	dma_fence_get((struct dma_fence *)fence);
}

struct nvhost_fence *nvgpu_nvhost_fence_create(struct platform_device *pdev,
					struct nvhost_ctrl_sync_fence_info *pts,
					u32 num_pts, const char *name)
{
	struct host1x_syncpt *sp;
	struct host1x *host1x;

	if (num_pts != 1)
		return ERR_PTR(-EINVAL);

	host1x = platform_get_drvdata(pdev);
	if (!host1x)
		return ERR_PTR(-ENODEV);

	sp = host1x_syncpt_get_by_id_noref(host1x, pts->id);
	if (WARN_ON(!sp))
		return ERR_PTR(-EINVAL);

	return (struct nvhost_fence *)host1x_fence_create(sp, pts->thresh, true);
}

struct nvhost_fence *nvgpu_nvhost_fence_get(int fd)
{
	return (struct nvhost_fence *)sync_file_get_fence(fd);
}

u32 nvgpu_nvhost_fence_num_pts(struct nvhost_fence *fence)
{
	struct dma_fence_array *array;

	array = to_dma_fence_array((struct dma_fence *)fence);
	if (!array)
		return 1;

	return array->num_fences;
}

int nvgpu_nvhost_fence_foreach_pt(struct nvhost_fence *fence,
	int (*iter)(struct nvhost_ctrl_sync_fence_info, void *),
	void *data)
{
	struct nvhost_ctrl_sync_fence_info info;
	struct dma_fence_array *array;
	int i, err;

	array = to_dma_fence_array((struct dma_fence *)fence);
	if (!array) {
		err = host1x_fence_extract((struct dma_fence *)fence, &info.id,
					   &info.thresh);
		if (err)
			return err;

		return iter(info, data);
	}

	for (i = 0; i < array->num_fences; ++i) {
		err = host1x_fence_extract(array->fences[i], &info.id,
					   &info.thresh);
		if (err)
			return err;

		err = iter(info, data);
		if (err)
			return err;
	}

	return 0;
}
