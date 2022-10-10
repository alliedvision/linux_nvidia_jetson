// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2022, NVIDIA Corporation.  All rights reserved.
 */

#include <linux/nvhost.h>
#include <linux/nvhost_t194.h>
#include <linux/dma-buf.h>
#include <linux/dma-mapping.h>
#include <uapi/linux/nvhost_ioctl.h>

#include "host1x/host1x.h"
#include "nvhost_syncpt.h"
#include "syncpt_fd.h"

#define SYNCPT_FULL 0xFFFFFFFFU

struct nvhost_syncpt_dmabuf_data {
	struct nvhost_master *host;
	u32 syncpt_id;
	phys_addr_t shim_pa;
	size_t size;
};

static void nvhost_syncpt_dmabuf_release(struct dma_buf *dmabuf)
{
	struct nvhost_syncpt_dmabuf_data *data = dmabuf->priv;

	if (data->syncpt_id != SYNCPT_FULL)
		nvhost_syncpt_put_ref(&data->host->syncpt, data->syncpt_id);

	kfree(data);
}

static struct sg_table *nvhost_syncpt_map_dmabuf(
					struct dma_buf_attachment *attachment,
					enum dma_data_direction direction)
{
	struct nvhost_syncpt_dmabuf_data *data = attachment->dmabuf->priv;
	struct sg_table *sgt;
	int ret;

	sgt = kmalloc(sizeof(*sgt), GFP_KERNEL);
	if (!sgt)
		return ERR_PTR(-ENOMEM);

	/* Single contiguous physical region */
	ret = sg_alloc_table(sgt, 1, GFP_KERNEL);
	if (ret)
		goto free_sgt;

	/* Mapping of full shim region is supported only for read */
	if ((data->syncpt_id == SYNCPT_FULL) && (direction != DMA_TO_DEVICE)) {
		dev_err(attachment->dev,
			"dma mapping of full shim is allowed only for read");
		ret = -EPERM;
		goto free_sgt;
	}

	sg_set_page(sgt->sgl, phys_to_page(data->shim_pa), data->size, 0);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 8, 0)
	ret = dma_map_sgtable(attachment->dev, sgt, direction,
				DMA_ATTR_SKIP_CPU_SYNC);
	if (ret) {
#else
	ret = dma_map_sg_attrs(attachment->dev, sgt->sgl, 1, direction,
				DMA_ATTR_SKIP_CPU_SYNC);
	/* dma_map_sg_attrs returns 0 on errors */
	if (ret <= 0) {
		ret = -ENOMEM;
#endif
		dev_err(attachment->dev, "dma mapping of syncpoint shim failed");
		goto free_table;
	}

	return sgt;

free_table:
	sg_free_table(sgt);

free_sgt:
	kfree(sgt);
	return ERR_PTR(ret);
}

static void nvhost_syncpt_unmap_dmabuf(struct dma_buf_attachment *attachment,
					struct sg_table *sgt,
					enum dma_data_direction direction)
{
	struct nvhost_syncpt_dmabuf_data *data = attachment->dmabuf->priv;
	u64 attrs = 0;

	attrs = DMA_ATTR_SKIP_CPU_SYNC;
	if (data->syncpt_id == SYNCPT_FULL)
		attrs |= DMA_ATTR_READ_ONLY;

	dma_unmap_sg_attrs(attachment->dev, sgt->sgl, 1, direction, attrs);
	sg_free_table(sgt);
	kfree(sgt);
}

static const struct dma_buf_ops syncpoint_dmabuf_ops = {
	.map_dma_buf = nvhost_syncpt_map_dmabuf,
	.unmap_dma_buf = nvhost_syncpt_unmap_dmabuf,
	.release = nvhost_syncpt_dmabuf_release,
};

int nvhost_syncpt_dmabuf_alloc(
	struct nvhost_master *host,
	struct nvhost_ctrl_syncpt_dmabuf_args *args)
{
	struct dma_buf *dmabuf;
	DEFINE_DMA_BUF_EXPORT_INFO(exp_info);
	struct nvhost_syncpt_dmabuf_data *data;
	phys_addr_t base;
	size_t size;
	u32 offset, nb_syncpts;
	int fd, err;

	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->host = host;

	err = nvhost_syncpt_unit_interface_get_aperture(host->dev,
							&base, &size);
	if (err)
		goto free_data;

	if (args->is_full_shim) {
		data->shim_pa = base;
		data->size = size;
		data->syncpt_id = SYNCPT_FULL;
		nb_syncpts = host->info.nb_hw_pts;
	} else {
		err = nvhost_syncpt_fd_get(args->syncpt_fd,
					&host->syncpt, &data->syncpt_id);
		if (err) {
			dev_err(&host->dev->dev, "invalid syncpoint fd");
			goto free_data;
		}

		offset = nvhost_syncpt_unit_interface_get_byte_offset(data->syncpt_id);
		data->shim_pa = base + offset;
		data->size = host->info.syncpt_page_size;
		nb_syncpts = 1;
	}

	exp_info.ops = &syncpoint_dmabuf_ops;
	exp_info.size = data->size;
	exp_info.flags = O_RDWR;
	exp_info.priv = data;

	dmabuf = dma_buf_export(&exp_info);
	if (IS_ERR(dmabuf)) {
		err = PTR_ERR(dmabuf);
		goto put_syncpt;
	}

	fd = dma_buf_fd(dmabuf, O_CLOEXEC);
	if (fd < 0) {
		err = fd;
		dma_buf_put(dmabuf);
		goto put_syncpt;
	}
	args->dmabuf_fd = fd;
	args->nb_syncpts = nb_syncpts;
	args->syncpt_page_size = host->info.syncpt_page_size;

	return 0;

put_syncpt:
	if (data->syncpt_id != SYNCPT_FULL)
		nvhost_syncpt_put_ref(&host->syncpt, data->syncpt_id);

free_data:
	kfree(data);
	return err;
}

