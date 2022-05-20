/*
 * Tegra Graphics Host Virtualization Support
 *
 * Copyright (c) 2014-2019, NVIDIA Corporation.  All rights reserved.
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

#ifndef __VHOST_H
#define __VHOST_H

#include <linux/nvhost.h>

#include "chip_support.h"

struct nvhost_virt_ctx {
	u64 handle;
	struct task_struct *syncpt_handler;
};

#ifdef CONFIG_TEGRA_GRHOST_VHOST

#include <linux/tegra_gr_comm.h>
#include <linux/tegra_vhost.h>

static inline void nvhost_set_virt_data(struct platform_device *dev, void *d)
{
	struct nvhost_device_data *data = platform_get_drvdata(dev);
	data->virt_priv = d;
}

static inline void *nvhost_get_virt_data(struct platform_device *dev)
{
	struct nvhost_device_data *data = platform_get_drvdata(dev);
	return data->virt_priv;
}

void vhost_init_host1x_debug_ops(struct nvhost_debug_ops *ops);
int vhost_sendrecv(struct tegra_vhost_cmd_msg *msg);
int vhost_virt_moduleid(int moduleid);
int vhost_suspend(struct platform_device *pdev);
int vhost_resume(struct platform_device *pdev);
int nvhost_virt_init(struct platform_device *dev, int moduleid);
void nvhost_virt_deinit(struct platform_device *dev);

#else

static inline void vhost_init_host1x_debug_ops(struct nvhost_debug_ops *ops)
{ }
static inline int vhost_virt_moduleid(int moduleid)
{
	return -ENOTSUPP;
}

static inline int vhost_suspend(struct platform_device *pdev)
{
	return 0;
}

static inline int vhost_resume(struct platform_device *pdev)
{
	return 0;
}

static inline void *nvhost_get_virt_data(struct platform_device *dev)
{
	return NULL;
}
static inline int nvhost_virt_init(struct platform_device *dev, int moduleid)
{
	return -ENOTSUPP;
}
static inline void nvhost_virt_deinit(struct platform_device *dev)
{
}

#endif

#endif
