/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2020 NVIDIA Corporation */

#ifndef _TEGRA_DRM_SUBMIT_GATHER_BO_H
#define _TEGRA_DRM_SUBMIT_GATHER_BO_H

#include <linux/host1x-next.h>
#include <linux/kref.h>

struct gather_bo {
	struct host1x_bo base;

	struct kref ref;

	struct device *dev;
	u32 *gather_data;
	dma_addr_t gather_data_dma;
	size_t gather_data_words;
};

extern const struct host1x_bo_ops gather_bo_ops;
void gather_bo_put(struct host1x_bo *host_bo);

#endif
