/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * NVDLA channel submission
 *
 * Copyright (c) 2022, NVIDIA Corporation.  All rights reserved.
 *
 */

#ifndef __NVHOST_NVDLA_CHANNEL_H__
#define __NVHOST_NVDLA_CHANNEL_H__

#include "dla_queue.h"
#include "nvdla.h"

#if IS_ENABLED(CONFIG_TEGRA_NVDLA_CHANNEL)
struct platform_device *nvdla_channel_map(struct platform_device *pdev,
					  struct nvdla_queue *queue);
void nvdla_putchannel(struct nvdla_queue *queue);
int nvdla_send_cmd_channel(struct platform_device *pdev,
			   struct nvdla_queue *queue,
			   struct nvdla_cmd_data *cmd_data,
			   struct nvdla_task *task);
#else
static inline struct platform_device *nvdla_channel_map(struct platform_device *pdev,
					  struct nvdla_queue *queue)
{
	return NULL;
}
static inline void nvdla_putchannel(struct nvdla_queue *queue) { }
static inline int nvdla_send_cmd_channel(struct platform_device *pdev,
			struct nvdla_queue *queue,
			struct nvdla_cmd_data *cmd_data,
			struct nvdla_task *task)
{
	return -EOPNOTSUPP;
}
#endif

#endif /* End of __NVHOST_NVDLA_CHANNEL_H__ */
