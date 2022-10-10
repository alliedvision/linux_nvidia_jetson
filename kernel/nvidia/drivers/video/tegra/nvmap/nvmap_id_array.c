// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021-2022 NVIDIA Corporation
 */

#include <linux/xarray.h>
#include <linux/dma-buf.h>
#include "nvmap_priv.h"

#define XA_START (U32_MAX / 2)
/*
 * Initialize xarray mapping
 */
void nvmap_id_array_init(struct xarray *id_arr)
{
	xa_init_flags(id_arr, XA_FLAGS_ALLOC1);
}

/*
 * Remove id to dma_buf mapping
 */
void nvmap_id_array_exit(struct xarray *id_arr)
{
	xa_destroy(id_arr);
}

/*
 * Create mapping between the id(NvRmMemHandle) and dma_buf
 */
int nvmap_id_array_id_alloc(struct xarray *id_arr, u32 *id, struct dma_buf *dmabuf)
{
	if (!id_arr || !dmabuf)
		return -EINVAL;

	return xa_alloc(id_arr, id, dmabuf,
		       XA_LIMIT(XA_START, U32_MAX), GFP_KERNEL);
}

/*
 * Clear mapping between the id(NvRmMemHandle) and dma_buf
 */
struct dma_buf *nvmap_id_array_id_release(struct xarray *id_arr, u32 id)
{
	if (!id_arr || !id)
		return NULL;

	return xa_erase(id_arr, id);
}

/*
 * Return dma_buf from the id.
 */
struct dma_buf *nvmap_id_array_get_dmabuf_from_id(struct xarray *id_arr, u32 id)
{
	struct dma_buf *dmabuf;

	dmabuf = xa_load(id_arr, id);
	if (!IS_ERR_OR_NULL(dmabuf))
		get_dma_buf(dmabuf);

	return dmabuf;
}
