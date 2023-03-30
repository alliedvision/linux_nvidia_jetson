/*
 * drivers/video/tegra/nvmap/nvmap_cache_t19x.c
 *
 * Copyright (c) 2016-2021, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#define pr_fmt(fmt)	"nvmap: %s() " fmt, __func__

#include <linux/nvmap_t19x.h>

#include "nvmap_priv.h"

void nvmap_handle_get_cacheability(struct nvmap_handle *h,
		bool *inner, bool *outer)
{
	struct nvmap_handle_t19x *handle_t19x;
	struct device *dev = nvmap_dev->dev_user.parent;

	handle_t19x = nvmap_dmabuf_get_drv_data(h->dmabuf, dev);
	if (handle_t19x && atomic_read(&handle_t19x->nc_pin)) {
		*inner = *outer = false;
		return;
	}

	*inner = h->flags == NVMAP_HANDLE_CACHEABLE ||
		 h->flags == NVMAP_HANDLE_INNER_CACHEABLE;
	*outer = h->flags == NVMAP_HANDLE_CACHEABLE;
}
