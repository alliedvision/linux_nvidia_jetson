// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2020 NVIDIA Corporation */

#include <linux/dma-fence-array.h>
#include <linux/file.h>
#include <linux/host1x-next.h>
#include <linux/iommu.h>
#include <linux/kref.h>
#include <linux/list.h>
#include <linux/nospec.h>
#include <linux/pm_runtime.h>
#include <linux/sync_file.h>

#include <drm/drm_drv.h>
#include <drm/drm_file.h>

#include "../uapi.h"
#include "../drm.h"
#include "../gem.h"

#include "gather_bo.h"
#include "submit.h"

static struct tegra_drm_mapping *
tegra_drm_mapping_get(struct tegra_drm_channel_ctx *ctx, u32 id)
{
	struct tegra_drm_mapping *mapping;

	xa_lock(&ctx->mappings);
	mapping = xa_load(&ctx->mappings, id);
	if (mapping)
		kref_get(&mapping->ref);
	xa_unlock(&ctx->mappings);

	return mapping;
}

static void *alloc_copy_user_array(void __user *from, size_t count, size_t size)
{
	unsigned long copy_err;
	size_t copy_len;
	void *data;

	if (check_mul_overflow(count, size, &copy_len))
		return ERR_PTR(-EINVAL);

	if (copy_len > 0x4000)
		return ERR_PTR(-E2BIG);

	data = kvmalloc(copy_len, GFP_KERNEL);
	if (!data)
		return ERR_PTR(-ENOMEM);

	copy_err = copy_from_user(data, from, copy_len);
	if (copy_err) {
		kvfree(data);
		return ERR_PTR(-EFAULT);
	}

	return data;
}

static int submit_copy_gather_data(struct drm_device *drm,
				   struct gather_bo **pbo,
				   struct drm_tegra_channel_submit *args)
{
	unsigned long copy_err;
	struct gather_bo *bo;
	size_t copy_len;

	if (args->gather_data_words == 0) {
		drm_info(drm, "gather_data_words can't be 0");
		return -EINVAL;
	}

	if (check_mul_overflow((size_t)args->gather_data_words, (size_t)4, &copy_len))
		return -EINVAL;

	bo = kzalloc(sizeof(*bo), GFP_KERNEL);
	if (!bo)
		return -ENOMEM;

	kref_init(&bo->ref);
	host1x_bo_init(&bo->base, &gather_bo_ops);
	bo->dev = drm->dev;

	bo->gather_data = dma_alloc_attrs(bo->dev, copy_len, &bo->gather_data_dma,
					  GFP_KERNEL | __GFP_NOWARN, 0);
	if (!bo->gather_data) {
		kfree(bo);
		return -ENOMEM;
	}

	copy_err = copy_from_user(bo->gather_data,
				  u64_to_user_ptr(args->gather_data_ptr),
				  copy_len);
	if (copy_err) {
		dma_free_attrs(drm->dev, copy_len, bo->gather_data, bo->gather_data_dma, 0);
		kfree(bo->gather_data);
		kfree(bo);
		return -EFAULT;
	}

	bo->gather_data_words = args->gather_data_words;

	*pbo = bo;

	return 0;
}

static int submit_write_reloc(struct gather_bo *bo,
			      struct drm_tegra_submit_buf *buf,
			      struct tegra_drm_mapping *mapping)
{
	/* TODO check that target_offset is within bounds */
	dma_addr_t iova = mapping->iova + buf->reloc.target_offset;
	u32 written_ptr;

#ifdef CONFIG_ARM64
	if (buf->flags & DRM_TEGRA_SUBMIT_BUF_RELOC_BLOCKLINEAR)
		iova |= BIT(39);
#endif

	written_ptr = (u32)(iova >> buf->reloc.shift);

	if (buf->reloc.gather_offset_words >= bo->gather_data_words)
		return -EINVAL;

	buf->reloc.gather_offset_words = array_index_nospec(
		buf->reloc.gather_offset_words, bo->gather_data_words);

	bo->gather_data[buf->reloc.gather_offset_words] = written_ptr;

	return 0;
}

static int submit_process_bufs(struct drm_device *drm, struct gather_bo *bo,
			       struct tegra_drm_submit_data *job_data,
			       struct tegra_drm_channel_ctx *ctx,
			       struct drm_tegra_channel_submit *args)
{
	struct tegra_drm_used_mapping *mappings;
	struct drm_tegra_submit_buf *bufs;
	int err;
	u32 i;

	bufs = alloc_copy_user_array(u64_to_user_ptr(args->bufs_ptr),
				     args->num_bufs, sizeof(*bufs));
	if (IS_ERR(bufs))
		return PTR_ERR(bufs);

	mappings = kcalloc(args->num_bufs, sizeof(*mappings), GFP_KERNEL);
	if (!mappings) {
		err = -ENOMEM;
		goto done;
	}

	for (i = 0; i < args->num_bufs; i++) {
		struct drm_tegra_submit_buf *buf = &bufs[i];
		struct tegra_drm_mapping *mapping;

		if (buf->flags & ~DRM_TEGRA_SUBMIT_BUF_RELOC_BLOCKLINEAR) {
			err = -EINVAL;
			goto drop_refs;
		}

		mapping = tegra_drm_mapping_get(ctx, buf->mapping_id);
		if (!mapping) {
			drm_info(drm, "invalid mapping_id for buf: %u",
				 buf->mapping_id);
			err = -EINVAL;
			goto drop_refs;
		}

		err = submit_write_reloc(bo, buf, mapping);
		if (err) {
			tegra_drm_mapping_put(mapping);
			goto drop_refs;
		}

		mappings[i].mapping = mapping;
		mappings[i].flags = buf->flags;
	}

	job_data->used_mappings = mappings;
	job_data->num_used_mappings = i;

	err = 0;

	goto done;

drop_refs:
	for (;;) {
		if (i-- == 0)
			break;

		tegra_drm_mapping_put(mappings[i].mapping);
	}

	kfree(mappings);
	job_data->used_mappings = NULL;

done:
	kvfree(bufs);

	return err;
}

static int submit_get_syncpt(struct drm_device *drm, struct host1x_job *job,
			     struct drm_tegra_channel_submit *args)
{
	struct host1x_syncpt *sp;

	if (args->syncpt_incr.flags)
		return -EINVAL;

	/* Syncpt ref will be dropped on job release */
	sp = host1x_syncpt_fd_get(args->syncpt_incr.syncpt_fd);
	if (IS_ERR(sp))
		return PTR_ERR(sp);

	job->syncpt = sp;
	job->syncpt_incrs = args->syncpt_incr.num_incrs;

	return 0;
}

static int submit_job_add_gather(struct host1x_job *job,
				 struct tegra_drm_channel_ctx *ctx,
				 struct drm_tegra_submit_cmd_gather_uptr *cmd,
				 struct gather_bo *bo, u32 *offset,
				 struct tegra_drm_submit_data *job_data,
				 u32 *class)
{
	u32 next_offset;

	if (cmd->reserved[0] || cmd->reserved[1] || cmd->reserved[2])
		return -EINVAL;

	/* Check for maximum gather size */
	if (cmd->words > 16383)
		return -EINVAL;

	if (check_add_overflow(*offset, cmd->words, &next_offset))
		return -EINVAL;

	if (next_offset > bo->gather_data_words)
		return -EINVAL;

	if (tegra_drm_fw_validate(ctx->client, bo->gather_data, *offset,
				  cmd->words, job_data, class))
		return -EINVAL;

	host1x_job_add_gather(job, &bo->base, cmd->words, *offset * 4);

	*offset = next_offset;

	return 0;
}

static int submit_create_job(struct drm_device *drm, struct host1x_job **pjob,
			     struct gather_bo *bo,
			     struct tegra_drm_channel_ctx *ctx,
			     struct drm_tegra_channel_submit *args,
			     struct tegra_drm_submit_data *job_data)
{
	struct drm_tegra_submit_cmd *cmds;
	u32 i, gather_offset = 0, class;
	struct host1x_job *job;
	int err;

	/* Set initial class for firewall. */
	class = ctx->client->base.class;

	cmds = alloc_copy_user_array(u64_to_user_ptr(args->cmds_ptr),
				     args->num_cmds, sizeof(*cmds));
	if (IS_ERR(cmds))
		return PTR_ERR(cmds);

	job = host1x_job_alloc(ctx->channel, args->num_cmds, 0);
	if (!job) {
		err = -ENOMEM;
		goto done;
	}

	err = submit_get_syncpt(drm, job, args);
	if (err < 0)
		goto free_job;

	job->client = &ctx->client->base;
	job->class = ctx->client->base.class;
	job->serialize = true;

	for (i = 0; i < args->num_cmds; i++) {
		struct drm_tegra_submit_cmd *cmd = &cmds[i];

		if (cmd->type == DRM_TEGRA_SUBMIT_CMD_GATHER_UPTR) {
			err = submit_job_add_gather(job, ctx, &cmd->gather_uptr,
						    bo, &gather_offset,
						    job_data, &class);
			if (err)
				goto free_job;
		} else if (cmd->type == DRM_TEGRA_SUBMIT_CMD_WAIT_SYNCPT) {
			if (cmd->wait_syncpt.reserved[0] ||
			    cmd->wait_syncpt.reserved[1]) {
				err = -EINVAL;
				goto free_job;
			}

			host1x_job_add_wait(job, cmd->wait_syncpt.id,
					    cmd->wait_syncpt.threshold);
		} else {
			err = -EINVAL;
			goto free_job;
		}
	}

	if (gather_offset == 0) {
		drm_info(drm, "Job must have at least one gather");
		err = -EINVAL;
		goto free_job;
	}

	*pjob = job;

	err = 0;
	goto done;

free_job:
	host1x_job_put(job);

done:
	kvfree(cmds);

	return err;
}

static void release_job(struct host1x_job *job)
{
	struct tegra_drm_client *client =
		container_of(job->client, struct tegra_drm_client, base);
	struct tegra_drm_submit_data *job_data = job->user_data;
	u32 i;

	for (i = 0; i < job_data->num_used_mappings; i++)
		tegra_drm_mapping_put(job_data->used_mappings[i].mapping);

	kfree(job_data->used_mappings);
	kfree(job_data);

	pm_runtime_put_autosuspend(client->base.dev);
}

int tegra_drm_ioctl_channel_submit(struct drm_device *drm, void *data,
				   struct drm_file *file)
{
	struct tegra_drm_file *fpriv = file->driver_priv;
	struct drm_tegra_channel_submit *args = data;
	struct tegra_drm_submit_data *job_data;
	struct tegra_drm_channel_ctx *ctx;
	struct host1x_job *job;
	struct gather_bo *bo;
	u32 i;
	int err;

	ctx = tegra_drm_channel_ctx_lock(fpriv, args->channel_ctx);
	if (!ctx)
		return -EINVAL;

	/* Allocate gather BO and copy gather words in. */
	err = submit_copy_gather_data(drm, &bo, args);
	if (err)
		goto unlock;

	job_data = kzalloc(sizeof(*job_data), GFP_KERNEL);
	if (!job_data) {
		err = -ENOMEM;
		goto put_bo;
	}

	/* Get data buffer mappings and do relocation patching. */
	err = submit_process_bufs(drm, bo, job_data, ctx, args);
	if (err)
		goto free_job_data;

	/* Allocate host1x_job and add gathers and waits to it. */
	err = submit_create_job(drm, &job, bo, ctx, args,
				job_data);
	if (err)
		goto free_job_data;

	/* Map gather data for Host1x. */
	err = host1x_job_pin(job, ctx->client->base.dev);
	if (err)
		goto put_job;

	/* Boot engine. */
	err = pm_runtime_get_sync(ctx->client->base.dev);
	if (err < 0)
		goto put_pm_runtime;

	job->user_data = job_data;
	job->release = release_job;
	job->timeout = 10000;

	/*
	 * job_data is now part of job reference counting, so don't release
	 * it from here.
	 */
	job_data = NULL;

	/* Submit job to hardware. */
	err = host1x_job_submit(job);
	if (err)
		goto put_job;

	/* Return postfences to userspace and add fences to DMA reservations. */
	args->syncpt_incr.fence_value = job->syncpt_end;

	goto put_job;

put_pm_runtime:
	if (!job->release)
		pm_runtime_put(ctx->client->base.dev);
	host1x_job_unpin(job);
put_job:
	host1x_job_put(job);
free_job_data:
	if (job_data && job_data->used_mappings) {
		for (i = 0; i < job_data->num_used_mappings; i++)
			tegra_drm_mapping_put(job_data->used_mappings[i].mapping);
		kfree(job_data->used_mappings);
	}
	if (job_data)
		kfree(job_data);
put_bo:
	gather_bo_put(&bo->base);
unlock:
	mutex_unlock(&fpriv->lock);
	return err;
}
