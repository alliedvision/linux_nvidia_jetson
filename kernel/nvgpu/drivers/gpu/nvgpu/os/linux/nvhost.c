/*
 * Copyright (c) 2017-2020, NVIDIA CORPORATION.  All rights reserved.
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

#include <linux/nvhost.h>
#include <linux/nvhost_t194.h>
#include <linux/dma-mapping.h>
#include <uapi/linux/nvhost_ioctl.h>
#include <linux/of_platform.h>

#include <nvgpu/gk20a.h>
#include <nvgpu/os_fence.h>
#include <nvgpu/os_fence_syncpts.h>
#include <nvgpu/nvhost.h>
#include <nvgpu/enabled.h>
#include <nvgpu/dma.h>

#include "nvhost_priv.h"

#include "os_linux.h"
#include "module.h"

int nvgpu_get_nvhost_dev(struct gk20a *g)
{
	struct device_node *np = nvgpu_get_node(g);
	struct platform_device *host1x_pdev = NULL;
	const __be32 *host1x_ptr;

	host1x_ptr = of_get_property(np, "nvidia,host1x", NULL);
	if (host1x_ptr) {
		struct device_node *host1x_node =
			of_find_node_by_phandle(be32_to_cpup(host1x_ptr));

		host1x_pdev = of_find_device_by_node(host1x_node);
		if (!host1x_pdev) {
			nvgpu_warn(g, "host1x device not available");
			return -EPROBE_DEFER;
		}

	} else {
		if (nvgpu_has_syncpoints(g)) {
			nvgpu_warn(g, "host1x reference not found. assuming no syncpoints support");
			nvgpu_set_enabled(g, NVGPU_HAS_SYNCPOINTS, false);
		}
		return -ENOSYS;
	}

	g->nvhost = nvgpu_kzalloc(g, sizeof(struct nvgpu_nvhost_dev));
	if (!g->nvhost)
		return -ENOMEM;

	g->nvhost->host1x_pdev = host1x_pdev;

	return 0;
}

int nvgpu_nvhost_module_busy_ext(
	struct nvgpu_nvhost_dev *nvhost_dev)
{
	return nvhost_module_busy_ext(nvhost_dev->host1x_pdev);
}

void nvgpu_nvhost_module_idle_ext(
	struct nvgpu_nvhost_dev *nvhost_dev)
{
	nvhost_module_idle_ext(nvhost_dev->host1x_pdev);
}

void nvgpu_nvhost_debug_dump_device(
	struct nvgpu_nvhost_dev *nvhost_dev)
{
	nvhost_debug_dump_device(nvhost_dev->host1x_pdev);
}

const char *nvgpu_nvhost_syncpt_get_name(
	struct nvgpu_nvhost_dev *nvhost_dev, int id)
{
	return nvhost_syncpt_get_name(nvhost_dev->host1x_pdev, id);
}

bool nvgpu_nvhost_syncpt_is_valid_pt_ext(
	struct nvgpu_nvhost_dev *nvhost_dev, u32 id)
{
	return nvhost_syncpt_is_valid_pt_ext(nvhost_dev->host1x_pdev, id);
}

bool nvgpu_nvhost_syncpt_is_expired_ext(
	struct nvgpu_nvhost_dev *nvhost_dev, u32 id, u32 thresh)
{
	return nvhost_syncpt_is_expired_ext(nvhost_dev->host1x_pdev,
			id, thresh);
}

int nvgpu_nvhost_intr_register_notifier(
	struct nvgpu_nvhost_dev *nvhost_dev, u32 id, u32 thresh,
	void (*callback)(void *, int), void *private_data)
{
	return nvhost_intr_register_notifier(nvhost_dev->host1x_pdev,
			id, thresh,
			callback, private_data);
}

void nvgpu_nvhost_syncpt_set_minval(
	struct nvgpu_nvhost_dev *nvhost_dev, u32 id, u32 val)
{
	nvhost_syncpt_set_minval(nvhost_dev->host1x_pdev, id, val);
}

void nvgpu_nvhost_syncpt_put_ref_ext(
	struct nvgpu_nvhost_dev *nvhost_dev, u32 id)
{
	nvhost_syncpt_put_ref_ext(nvhost_dev->host1x_pdev, id);
}

u32 nvgpu_nvhost_get_syncpt_client_managed(
	struct nvgpu_nvhost_dev *nvhost_dev,
	const char *syncpt_name)
{
	return nvhost_get_syncpt_client_managed(nvhost_dev->host1x_pdev,
			syncpt_name);
}

int nvgpu_nvhost_syncpt_wait_timeout_ext(
	struct nvgpu_nvhost_dev *nvhost_dev, u32 id,
	u32 thresh, u32 timeout, u32 waiter_index)
{
	return nvhost_syncpt_wait_timeout_ext(nvhost_dev->host1x_pdev,
		id, thresh, timeout, NULL, NULL);
}

int nvgpu_nvhost_syncpt_read_ext_check(
	struct nvgpu_nvhost_dev *nvhost_dev, u32 id, u32 *val)
{
	return nvhost_syncpt_read_ext_check(nvhost_dev->host1x_pdev, id, val);
}

void nvgpu_nvhost_syncpt_set_safe_state(
	struct nvgpu_nvhost_dev *nvhost_dev, u32 id)
{
	u32 val = 0;
	int err;

	/*
	 * Add large number of increments to current value
	 * so that all waiters on this syncpoint are released
	 *
	 * We don't expect any case where more than 0x10000 increments
	 * are pending
	 */
	err = nvhost_syncpt_read_ext_check(nvhost_dev->host1x_pdev,
			id, &val);
	if (err != 0) {
		pr_err("%s: syncpt id read failed, cannot reset for safe state",
				__func__);
	} else {
		val += 0x10000;
		nvhost_syncpt_set_minval(nvhost_dev->host1x_pdev, id, val);
	}
}

int nvgpu_nvhost_get_syncpt_aperture(
		struct nvgpu_nvhost_dev *nvhost_dev,
		u64 *base, size_t *size)
{
	return nvhost_syncpt_unit_interface_get_aperture(
		nvhost_dev->host1x_pdev, (phys_addr_t *)base, size);
}

u32 nvgpu_nvhost_syncpt_unit_interface_get_byte_offset(struct gk20a *g,
	u32 syncpt_id)
{
	return nvhost_syncpt_unit_interface_get_byte_offset(syncpt_id);
}

int nvgpu_nvhost_fence_install(struct nvhost_fence *fence, int fd)
{
	return nvhost_fence_install(fence, fd);
}

struct nvhost_fence *nvgpu_nvhost_fence_get(int fd)
{
	return nvhost_fence_get(fd);
}

void nvgpu_nvhost_fence_put(struct nvhost_fence *fence)
{
	nvhost_fence_put(fence);
}

void nvgpu_nvhost_fence_dup(struct nvhost_fence *fence)
{
	nvhost_fence_dup(fence);
}

struct nvhost_fence *nvgpu_nvhost_fence_create(struct platform_device *pdev,
					struct nvhost_ctrl_sync_fence_info *pts,
					u32 num_pts, const char *name)
{
	return nvhost_fence_create(pdev, pts, num_pts, name);
}

u32 nvgpu_nvhost_fence_num_pts(struct nvhost_fence *fence)
{
	return nvhost_fence_num_pts(fence);
}

int nvgpu_nvhost_fence_foreach_pt(struct nvhost_fence *fence,
        int (*iter)(struct nvhost_ctrl_sync_fence_info, void *),
        void *data)
{
	return nvhost_fence_foreach_pt(fence, iter, data);
}
