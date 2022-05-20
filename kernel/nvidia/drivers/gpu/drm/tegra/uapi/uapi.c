// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2020 NVIDIA Corporation */

#include <linux/host1x-next.h>
#include <linux/iommu.h>
#include <linux/list.h>

#include <drm/drm_drv.h>
#include <drm/drm_file.h>

#include "../uapi.h"
#include "../drm.h"

struct tegra_drm_channel_ctx *
tegra_drm_channel_ctx_lock(struct tegra_drm_file *file, u32 id)
{
	struct tegra_drm_channel_ctx *ctx;

	mutex_lock(&file->lock);
	ctx = xa_load(&file->contexts, id);
	if (!ctx)
		mutex_unlock(&file->lock);

	return ctx;
}

static void tegra_drm_mapping_release(struct kref *ref)
{
	struct tegra_drm_mapping *mapping =
		container_of(ref, struct tegra_drm_mapping, ref);

	host1x_bo_unpin(mapping->map);
	host1x_bo_put(mapping->bo);

	kfree(mapping);
}

void tegra_drm_mapping_put(struct tegra_drm_mapping *mapping)
{
	kref_put(&mapping->ref, tegra_drm_mapping_release);
}

static void tegra_drm_channel_ctx_close(struct tegra_drm_channel_ctx *ctx)
{
	unsigned long mapping_id;
	struct tegra_drm_mapping *mapping;

	xa_for_each(&ctx->mappings, mapping_id, mapping)
		tegra_drm_mapping_put(mapping);

	xa_destroy(&ctx->mappings);

	host1x_channel_put(ctx->channel);

	kfree(ctx);
}

int close_channel_ctx(int id, void *p, void *data)
{
	struct tegra_drm_channel_ctx *ctx = p;

	tegra_drm_channel_ctx_close(ctx);

	return 0;
}

void tegra_drm_uapi_close_file(struct tegra_drm_file *file)
{
	unsigned long ctx_id;
	struct tegra_drm_channel_ctx *ctx;

	xa_for_each(&file->contexts, ctx_id, ctx)
		tegra_drm_channel_ctx_close(ctx);

	xa_destroy(&file->contexts);
}

int tegra_drm_ioctl_channel_open(struct drm_device *drm, void *data,
				 struct drm_file *file)
{
	struct tegra_drm_file *fpriv = file->driver_priv;
	struct tegra_drm *tegra = drm->dev_private;
	struct drm_tegra_channel_open *args = data;
	struct tegra_drm_client *client = NULL;
	struct tegra_drm_channel_ctx *ctx;
	int err;

	if (args->flags)
		return -EINVAL;

	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	err = -ENODEV;
	list_for_each_entry(client, &tegra->clients, list) {
		if (client->base.class == args->host1x_class) {
			err = 0;
			break;
		}
	}
	if (err)
		goto free_ctx;

	if (client->shared_channel) {
		ctx->channel = host1x_channel_get(client->shared_channel);
	} else {
		ctx->channel = host1x_channel_request(&client->base);
		if (!ctx->channel) {
			err = -EBUSY;
			goto free_ctx;
		}
	}

	err = xa_alloc(&fpriv->contexts, &args->channel_ctx, ctx,
		       XA_LIMIT(1, U32_MAX), GFP_KERNEL);
	if (err < 0)
		goto put_channel;

	ctx->client = client;
	xa_init_flags(&ctx->mappings, XA_FLAGS_ALLOC1);

	args->hardware_version = client->version;

	return 0;

put_channel:
	host1x_channel_put(ctx->channel);
free_ctx:
	kfree(ctx);

	return err;
}

int tegra_drm_ioctl_channel_close(struct drm_device *drm, void *data,
				  struct drm_file *file)
{
	struct tegra_drm_file *fpriv = file->driver_priv;
	struct drm_tegra_channel_close *args = data;
	struct tegra_drm_channel_ctx *ctx;

	ctx = tegra_drm_channel_ctx_lock(fpriv, args->channel_ctx);
	if (!ctx)
		return -EINVAL;

	xa_erase(&fpriv->contexts, args->channel_ctx);

	mutex_unlock(&fpriv->lock);

	tegra_drm_channel_ctx_close(ctx);

	return 0;
}

int tegra_drm_ioctl_channel_map(struct drm_device *drm, void *data,
				struct drm_file *file)
{
	struct tegra_drm_file *fpriv = file->driver_priv;
	struct drm_tegra_channel_map *args = data;
	struct tegra_drm_channel_ctx *ctx;
	struct tegra_drm_mapping *mapping;
	enum dma_data_direction direction;
	struct drm_gem_object *gem;
	u32 mapping_id;
	int err = 0;

	if (args->flags & ~DRM_TEGRA_CHANNEL_MAP_READWRITE)
		return -EINVAL;

	ctx = tegra_drm_channel_ctx_lock(fpriv, args->channel_ctx);
	if (!ctx)
		return -EINVAL;

	mapping = kzalloc(sizeof(*mapping), GFP_KERNEL);
	if (!mapping) {
		err = -ENOMEM;
		goto unlock;
	}

	kref_init(&mapping->ref);

	gem = drm_gem_object_lookup(file, args->handle);
	if (!gem) {
		err = -EINVAL;
		goto free;
	}

	mapping->bo = &container_of(gem, struct tegra_bo, gem)->base;

	if (args->flags & DRM_TEGRA_CHANNEL_MAP_READWRITE)
		direction = DMA_BIDIRECTIONAL;
	else
		direction = DMA_TO_DEVICE;

	mapping->map = host1x_bo_pin(ctx->client->base.dev, mapping->bo, direction, NULL);
	if (IS_ERR(mapping->map)) {
		err = PTR_ERR(mapping->map);
		goto put_gem;
	}

	mapping->iova = mapping->map->phys;
	mapping->iova_end = mapping->iova + gem->size;

	mutex_unlock(&fpriv->lock);

	err = xa_alloc(&ctx->mappings, &mapping_id, mapping,
		       XA_LIMIT(1, U32_MAX), GFP_KERNEL);
	if (err < 0)
		goto unpin;

	args->mapping_id = mapping_id;

	return 0;

unpin:
	host1x_bo_unpin(mapping->map);
put_gem:
	drm_gem_object_put(gem);
free:
	kfree(mapping);
unlock:
	mutex_unlock(&fpriv->lock);
	return err;
}

int tegra_drm_ioctl_channel_unmap(struct drm_device *drm, void *data,
				  struct drm_file *file)
{
	struct tegra_drm_file *fpriv = file->driver_priv;
	struct drm_tegra_channel_unmap *args = data;
	struct tegra_drm_channel_ctx *ctx;
	struct tegra_drm_mapping *mapping;

	ctx = tegra_drm_channel_ctx_lock(fpriv, args->channel_ctx);
	if (!ctx)
		return -EINVAL;

	mapping = xa_erase(&ctx->mappings, args->mapping_id);

	mutex_unlock(&fpriv->lock);

	if (mapping) {
		tegra_drm_mapping_put(mapping);
		return 0;
	} else {
		return -EINVAL;
	}
}

int tegra_drm_ioctl_gem_create(struct drm_device *drm, void *data,
			       struct drm_file *file)
{
	struct drm_tegra_gem_create *args = data;
	struct tegra_bo *bo;

	if (args->flags)
		return -EINVAL;

	bo = tegra_bo_create_with_handle(file, drm, args->size, args->flags,
					 &args->handle);
	if (IS_ERR(bo))
		return PTR_ERR(bo);

	return 0;
}

int tegra_drm_ioctl_gem_mmap(struct drm_device *drm, void *data,
			     struct drm_file *file)
{
	struct drm_tegra_gem_mmap *args = data;
	struct drm_gem_object *gem;
	struct tegra_bo *bo;

	gem = drm_gem_object_lookup(file, args->handle);
	if (!gem)
		return -EINVAL;

	bo = to_tegra_bo(gem);

	args->offset = drm_vma_node_offset_addr(&bo->gem.vma_node);

	drm_gem_object_put(gem);

	return 0;
}
