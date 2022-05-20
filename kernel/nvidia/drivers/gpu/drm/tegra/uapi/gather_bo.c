// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2020 NVIDIA Corporation */

#include <linux/dma-mapping.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>

#include "gather_bo.h"

static struct host1x_bo *gather_bo_get(struct host1x_bo *host_bo)
{
	struct gather_bo *bo = container_of(host_bo, struct gather_bo, base);

	kref_get(&bo->ref);

	return host_bo;
}

static void gather_bo_release(struct kref *ref)
{
	struct gather_bo *bo = container_of(ref, struct gather_bo, ref);

	dma_free_attrs(bo->dev, bo->gather_data_words * 4, bo->gather_data, bo->gather_data_dma,
		       0);
	kfree(bo);
}

void gather_bo_put(struct host1x_bo *host_bo)
{
	struct gather_bo *bo = container_of(host_bo, struct gather_bo, base);

	kref_put(&bo->ref, gather_bo_release);
}

static struct host1x_bo_mapping *
gather_bo_pin(struct device *dev, struct host1x_bo *bo, enum dma_data_direction direction)
{
	struct gather_bo *gather = container_of(bo, struct gather_bo, base);
	struct host1x_bo_mapping *map;
	int err;

	map = kzalloc(sizeof(*map), GFP_KERNEL);
	if (!map)
		return ERR_PTR(-ENOMEM);

	kref_init(&map->ref);
	map->bo = host1x_bo_get(bo);
	map->direction = direction;
	map->dev = dev;

	map->sgt = kzalloc(sizeof(*map->sgt), GFP_KERNEL);
	if (!map->sgt) {
		err = -ENOMEM;
		goto free;
	}

	err = dma_get_sgtable(gather->dev, map->sgt, gather->gather_data, gather->gather_data_dma,
			      gather->gather_data_words * 4);
	if (err)
		goto free_sgt;

	err = dma_map_sgtable(dev, map->sgt, direction, 0);
	if (err)
		goto free_sgt;

	map->phys = sg_dma_address(map->sgt->sgl);
	map->size = gather->gather_data_words * 4;
	map->chunks = err;

	return map;

free_sgt:
	sg_free_table(map->sgt);
	kfree(map->sgt);
free:
	kfree(map);
	return ERR_PTR(err);
}

static void gather_bo_unpin(struct host1x_bo_mapping *map)
{
	if (!map)
		return;

	dma_unmap_sgtable(map->dev, map->sgt, map->direction, 0);
	sg_free_table(map->sgt);
	kfree(map->sgt);
	host1x_bo_put(map->bo);

	kfree(map);
}

static void *gather_bo_mmap(struct host1x_bo *host_bo)
{
	struct gather_bo *bo = container_of(host_bo, struct gather_bo, base);

	return bo->gather_data;
}

static void gather_bo_munmap(struct host1x_bo *host_bo, void *addr)
{
}

const struct host1x_bo_ops gather_bo_ops = {
	.get = gather_bo_get,
	.put = gather_bo_put,
	.pin = gather_bo_pin,
	.unpin = gather_bo_unpin,
	.mmap = gather_bo_mmap,
	.munmap = gather_bo_munmap,
};
