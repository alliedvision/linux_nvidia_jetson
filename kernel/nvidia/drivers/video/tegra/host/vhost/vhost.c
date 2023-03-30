/*
 * Tegra Graphics Virtualization Host functions for HOST1X
 *
 * Copyright (c) 2014-2021, NVIDIA Corporation. All rights reserved.
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

#include <linux/slab.h>
#include <linux/tegra_vhost.h>
#include <linux/nvhost.h>

#include "vhost.h"
#include "../host1x/host1x.h"

static inline int vhost_comm_init(struct platform_device *pdev,
					  bool channel_management_in_guest)
{
	size_t queue_sizes[] = { TEGRA_VHOST_QUEUE_SIZES };
	unsigned int num_queues =
		channel_management_in_guest ?
		1 : ARRAY_SIZE(queue_sizes);

	return tegra_gr_comm_init(pdev, num_queues, queue_sizes,
			TEGRA_VHOST_QUEUE_CMD, num_queues);
}

static inline void vhost_comm_deinit(bool channel_management_in_guest)
{
	size_t queue_sizes[] = { TEGRA_VHOST_QUEUE_SIZES };
	unsigned int num_queues =
		channel_management_in_guest ?
		1 : ARRAY_SIZE(queue_sizes);

	tegra_gr_comm_deinit(TEGRA_VHOST_QUEUE_CMD, num_queues);
}

int vhost_virt_moduleid(int moduleid)
{
	switch (moduleid) {
	case NVHOST_MODULE_NONE:
		return TEGRA_VHOST_MODULE_HOST;
	case NVHOST_MODULE_ISP:
		return TEGRA_VHOST_MODULE_ISP;
	case NVHOST_MODULE_ISPB:
		return TEGRA_VHOST_MODULE_ISPB;
	case NVHOST_MODULE_VI:
		return TEGRA_VHOST_MODULE_VI;
	case NVHOST_MODULE_VI2:
		return TEGRA_VHOST_MODULE_VI2;
	case NVHOST_MODULE_MSENC:
		return TEGRA_VHOST_MODULE_MSENC;
	case NVHOST_MODULE_VIC:
		return TEGRA_VHOST_MODULE_VIC;
	case NVHOST_MODULE_NVDEC:
		return TEGRA_VHOST_MODULE_NVDEC;
	case NVHOST_MODULE_NVJPG:
		return TEGRA_VHOST_MODULE_NVJPG;
	case NVHOST_MODULE_NVDEC1:
		return TEGRA_VHOST_MODULE_NVDEC1;
	case NVHOST_MODULE_NVENC1:
		return TEGRA_VHOST_MODULE_NVENC1;
	case NVHOST_MODULE_NVCSI:
		return TEGRA_VHOST_MODULE_NVCSI;
	case NVHOST_MODULE_NVJPG1:
		return TEGRA_VHOST_MODULE_NVJPG1;
	case NVHOST_MODULE_OFA:
		return TEGRA_VHOST_MODULE_OFA;
	default:
		pr_err("module %d not virtualized\n", moduleid);
		return -1;
	}
}

int vhost_moduleid_virt_to_hw(int moduleid)
{
	switch (moduleid) {
	case TEGRA_VHOST_MODULE_HOST:
		return NVHOST_MODULE_NONE;
	case TEGRA_VHOST_MODULE_ISP:
		return NVHOST_MODULE_ISP;
	case TEGRA_VHOST_MODULE_ISPB:
		return NVHOST_MODULE_ISPB;
	case TEGRA_VHOST_MODULE_VI:
		return NVHOST_MODULE_VI;
	case TEGRA_VHOST_MODULE_VI2:
		return NVHOST_MODULE_VI2;
	case TEGRA_VHOST_MODULE_MSENC:
		return NVHOST_MODULE_MSENC;
	case TEGRA_VHOST_MODULE_VIC:
		return NVHOST_MODULE_VIC;
	case TEGRA_VHOST_MODULE_NVDEC:
		return NVHOST_MODULE_NVDEC;
	case TEGRA_VHOST_MODULE_NVJPG:
		return NVHOST_MODULE_NVJPG;
	case TEGRA_VHOST_MODULE_NVCSI:
		return NVHOST_MODULE_NVCSI;
	case TEGRA_VHOST_MODULE_NVJPG1:
		return NVHOST_MODULE_NVJPG1;
	case TEGRA_VHOST_MODULE_OFA:
		return NVHOST_MODULE_OFA;
	default:
		pr_err("unknown virtualized module %d\n", moduleid);
		return -1;
	}
}

static u64 vhost_virt_connect(int moduleid)
{
	struct tegra_vhost_cmd_msg msg;
	struct tegra_vhost_connect_params *p = &msg.connect;
	int err;

	msg.cmd = TEGRA_VHOST_CMD_CONNECT;
	p->module = vhost_virt_moduleid(moduleid);
	if (p->module == -1)
		return 0;

	err = vhost_sendrecv(&msg);

	return (err || msg.ret) ? 0 : p->connection_id;
}

int vhost_sendrecv(struct tegra_vhost_cmd_msg *msg)
{
	void *handle;
	size_t size = sizeof(*msg);
	size_t size_out = size;
	void *data = msg;
	int err;

	err = tegra_gr_comm_sendrecv(tegra_gr_comm_get_server_vmid(),
				TEGRA_VHOST_QUEUE_CMD, &handle, &data, &size);
	if (!err) {
		WARN_ON(size < size_out);
		memcpy(msg, data, size_out);
		tegra_gr_comm_release(handle);
	}

	return err;
}

static void vhost_fake_debug_show_channel_cdma(struct nvhost_master *m,
		struct nvhost_channel *ch, struct output *o, int chid)
{
}

static void vhost_fake_debug_show_channel_fifo(struct nvhost_master *m,
		struct nvhost_channel *ch, struct output *o, int chid)
{
}

static void vhost_fake_debug_show_mlocks(struct nvhost_master *m,
		struct output *o)
{
}

void vhost_init_host1x_debug_ops(struct nvhost_debug_ops *ops)
{
	/* Does not support hw debug on VM side */
	ops->show_channel_cdma = vhost_fake_debug_show_channel_cdma;
	ops->show_channel_fifo = vhost_fake_debug_show_channel_fifo;
	ops->show_mlocks = vhost_fake_debug_show_mlocks;
}

int nvhost_virt_init(struct platform_device *dev, int moduleid)
{
	int err = 0;
	bool channel_management_in_guest = false;
	struct nvhost_master *host = nvhost_get_host(dev);
	struct nvhost_virt_ctx *virt_ctx = kzalloc(sizeof(*virt_ctx), GFP_KERNEL);

	if (!virt_ctx)
		return -ENOMEM;

	if (host->info.vmserver_owns_engines)
		channel_management_in_guest = true;

	/* If host1x, init comm */
	if (moduleid == NVHOST_MODULE_NONE) {
		err = vhost_comm_init(dev, channel_management_in_guest);
		if (err) {
			dev_err(&dev->dev, "failed to init comm interface\n");
			goto fail;
		}
	}

	virt_ctx->handle = vhost_virt_connect(moduleid);
	if (!virt_ctx->handle) {
		dev_err(&dev->dev,
			"failed to connect to server node\n");
		if (moduleid == NVHOST_MODULE_NONE)
			vhost_comm_deinit(channel_management_in_guest);
		err = -ENOMEM;
		goto fail;
	}

	nvhost_set_virt_data(dev, virt_ctx);
	return 0;

fail:
	kfree(virt_ctx);
	return err;
}

void nvhost_virt_deinit(struct platform_device *dev)
{
	struct nvhost_virt_ctx *virt_ctx = nvhost_get_virt_data(dev);
	struct nvhost_master *host = nvhost_get_host(dev);

	if (virt_ctx) {
		/* FIXME: add virt disconnect */
		vhost_comm_deinit(host->info.vmserver_owns_engines);
		kfree(virt_ctx);
	}
}

int vhost_suspend(struct platform_device *pdev)
{
	struct tegra_vhost_cmd_msg msg;
	struct nvhost_virt_ctx *ctx = nvhost_get_virt_data(pdev);

	if (!ctx)
		return 0;

	msg.cmd = TEGRA_VHOST_CMD_SUSPEND;
	msg.connection_id = ctx->handle;
	return vhost_sendrecv(&msg);
}

int vhost_resume(struct platform_device *pdev)
{
	struct tegra_vhost_cmd_msg msg;
	struct nvhost_virt_ctx *ctx = nvhost_get_virt_data(pdev);

	if (!ctx)
		return 0;

	msg.cmd = TEGRA_VHOST_CMD_RESUME;
	msg.connection_id = ctx->handle;
	return vhost_sendrecv(&msg);
}
