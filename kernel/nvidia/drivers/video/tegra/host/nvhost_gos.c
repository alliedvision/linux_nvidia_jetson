/*
 * GoS support
 *
 * Copyright (c) 2016-2020, NVIDIA Corporation.  All rights reserved.
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

#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/nvhost.h>
#include <linux/errno.h>
#include <linux/iommu.h>
#include <linux/io.h>
#include <linux/nvmap_t19x.h>
#include <linux/nvhost_t194.h>
#include <linux/slab.h>

#include "bus_client_t194.h"
#include "nvhost_syncpt_unit_interface.h"

struct syncpt_gos_backing {
	struct rb_node syncpt_gos_backing_entry; /* backing entry */

	u32 syncpt_id;	/* syncpoint id */

	u32 gos_id;	/* GoS id corresponding to syncpt (0..11) */
	u32 gos_offset;	/* Word-offset of syncpt within GoS (0..63) */
};

/**
 * nvhost_syncpt_get_cv_dev_address_table() - Get details of CV devices address table
 *
 * @engine_pdev:	Pointer to a host1x engine
 * @count:		Pointer for storing count value
 * @table:		Pointer to address table
 *
 * Returns:		0 on success, a negative error code otherwise.
 *
 * This function will return details of all CV devices' addresses.
 * Count is the number of entries in the table with index of array
 * as GoS ID
 * Entries in the table store base IOVA address for each GoS
 */
int nvhost_syncpt_get_cv_dev_address_table(struct platform_device *engine_pdev,
				   int *count,
				   dma_addr_t **table)
{
	struct nvhost_device_data *pdata = platform_get_drvdata(engine_pdev);
	struct nvhost_syncpt_unit_interface *syncpt_unit_interface =
			pdata->syncpt_unit_interface;
	struct cv_dev_info *cv_dev_info;
	struct sg_table *sgt;
	int i;

	/* table is already prepared ? */
	if (syncpt_unit_interface->cv_dev_count)
		goto finish;

	/* fetch and store the address table */
	cv_dev_info = nvmap_fetch_cv_dev_info(&engine_pdev->dev);
	if (!cv_dev_info) {
		nvhost_err(&engine_pdev->dev, "failed to fetch_cv_dev_info");
		return -EFAULT;
	}

	for (i = 0; i < cv_dev_info->count; ++i) {
		sgt = cv_dev_info->sgt + i;
		syncpt_unit_interface->cv_dev_address_table[i] =
			sg_dma_address(sgt->sgl);
	}

	syncpt_unit_interface->cv_dev_count = cv_dev_info->count;

finish:
	*table = syncpt_unit_interface->cv_dev_address_table;
	*count = syncpt_unit_interface->cv_dev_count;

	return 0;
}

/**
 * nvhost_syncpt_find_gos_backing() - Find GoS backing in a global list
 *
 * @host:		Pointer to nvhost_master
 * @syncpt_id:		Syncpoint id to be searched
 *
 * Returns:		0 on success, a negative error code otherwise.
 *
 * This function will find and return syncpoint backing of a syncpoint
 * in global list.
 * Return NULL if no backing is found
 */
static struct syncpt_gos_backing *
nvhost_syncpt_find_gos_backing(struct nvhost_master *host, u32 syncpt_id)
{
	struct rb_root *root = &host->syncpt_backing_head;
	struct rb_node *node = root->rb_node;
	struct syncpt_gos_backing *syncpt_gos_backing;

	while (node) {
		syncpt_gos_backing =
			container_of(node, struct syncpt_gos_backing,
			syncpt_gos_backing_entry);

		if (syncpt_gos_backing->syncpt_id > syncpt_id)
			node = node->rb_left;
		else if (syncpt_gos_backing->syncpt_id != syncpt_id)
			node = node->rb_right;
		else
			return syncpt_gos_backing;
	}

	return NULL;
}

/**
 * nvhost_syncpt_get_gos() - Get GoS data corresponding to a syncpoint
 *
 * @engine_pdev:	Pointer to a host1x engine
 * @syncpt_id:		Syncpoint id to be checked
 * @gos_id:		Pointer for storing the GoS identifier
 * @gos_offset:		Pointer for storing the word offset within GoS
 *
 * Returns:		0 on success, a negative error code otherwise.
 *
 * This function returns GoS ID and offset of semaphore in syncpoint
 * backing in GoS for a corresponding syncpoint id
 * GoS ID and offset should be referred only if return value is 0
 */
int nvhost_syncpt_get_gos(struct platform_device *engine_pdev,
			      u32 syncpt_id,
			      u32 *gos_id,
			      u32 *gos_offset)
{
	struct nvhost_master *host = nvhost_get_host(engine_pdev);
	struct syncpt_gos_backing *syncpt_gos_backing;

	syncpt_gos_backing = nvhost_syncpt_find_gos_backing(host, syncpt_id);
	if (!syncpt_gos_backing) {
		/*
		 * It is absolutely valid for some dev syncpoints to not to
		 * have GoS backing support. So it is up to the clients to
		 * consider this as real error or not.
		 * Keeping this error message verbose, increases CPU load when
		 * this is called frequently in some usecases.
		 */
		dev_dbg(&engine_pdev->dev, "failed to find gos backing");
		return -EINVAL;
	}

	*gos_id = syncpt_gos_backing->gos_id;
	*gos_offset = syncpt_gos_backing->gos_offset;

	return 0;
}

/**
 * nvhost_syncpt_gos_address() - Get GoS address corresponding to syncpoint id
 *
 * @engine_pdev:	Pointer to a host1x engine
 * @syncpt_id:		syncpoint id
 *
 * Return:	IOVA address of syncpoint in GoS
 *
 * This function returns IOVA address of the syncpoint backing
 * in GoS for corresponding syncpoint id
 * This function returns 0 for all error cases
 */
dma_addr_t nvhost_syncpt_gos_address(struct platform_device *engine_pdev,
				     u32 syncpt_id)
{
	struct nvhost_device_data *pdata = platform_get_drvdata(engine_pdev);
	u32 gos_id, gos_offset;
	struct cv_dev_info *cv_dev_info;
	struct sg_table *sgt;
	int err;

	err = nvhost_syncpt_get_gos(engine_pdev, syncpt_id,
				    &gos_id, &gos_offset);
	if (err)
		return 0;

	/* if context isolation is enabled, GoS is not supported */
	if (pdata->isolate_contexts)
		return 0;

	cv_dev_info = nvmap_fetch_cv_dev_info(&engine_pdev->dev);
	if (!cv_dev_info)
		return 0;

	sgt = cv_dev_info->sgt + gos_id;
	return sg_dma_address(sgt->sgl) + gos_offset * sizeof(u32);
}

/**
 * nvhost_syncpt_insert_syncpt_backing() - insert syncpt_backing into rb_tree
 *
 * @root:			ROOT node of rb-tree
 * @syncpt_gos_backing:		syncpt_backing of new node
 *
 * This function will add new syncpt_gos_backing node into rb_tree
 */
static void nvhost_syncpt_insert_syncpt_backing(struct rb_root *root,
	struct syncpt_gos_backing *syncpt_gos_backing)
{
	struct rb_node **new_node = &(root->rb_node), *parent = NULL;

	while (*new_node) {
		struct syncpt_gos_backing *cmp_with =
			container_of(*new_node, struct syncpt_gos_backing,
			syncpt_gos_backing_entry);

		parent = *new_node;

		if (cmp_with->syncpt_id > syncpt_gos_backing->syncpt_id)
			new_node = &((*new_node)->rb_left);
		else if (cmp_with->syncpt_id != syncpt_gos_backing->syncpt_id)
			new_node = &((*new_node)->rb_right);
		else
			return;
	}

	rb_link_node(&syncpt_gos_backing->syncpt_gos_backing_entry,
		     parent, new_node);
	rb_insert_color(&syncpt_gos_backing->syncpt_gos_backing_entry, root);
}

/**
 * nvhost_syncpt_alloc_gos_backing() - Create GoS backing for a syncpoint
 *
 * @engine_pdev:	Pointer to a host1x engine
 * @syncpt_id:		syncpoint id
 *
 * Return:	0 on success, a negative error code otherwise
 *
 * This function creates a GoS backing for a give syncpoint id.
 * GoS backing is then inserted into a global list for
 * future reference/lookup.
 * A backing will only be created for engines supporting GoS
 * and skipped otherwise.
 */
int nvhost_syncpt_alloc_gos_backing(struct platform_device *engine_pdev,
				     u32 syncpt_id)
{
	struct nvhost_device_data *pdata = platform_get_drvdata(engine_pdev);
	struct nvhost_master *host = nvhost_get_host(engine_pdev);
	struct syncpt_gos_backing *syncpt_gos_backing;
	struct cv_dev_info *cv_dev_info;
	u32 idx, offset;
	u32 *semaphore;

	/* check if engine supports GoS */
	cv_dev_info = nvmap_fetch_cv_dev_info(&engine_pdev->dev);
	if (!cv_dev_info)
		return 0;

	/* if context isolation is enabled, GoS is not supported */
	if (pdata->isolate_contexts) {
		nvhost_err(&engine_pdev->dev,
			   "gos unsupported for engines with context isolation");
		return -EINVAL;
	}

	/* check if backing already exists */
	syncpt_gos_backing = nvhost_syncpt_find_gos_backing(host, syncpt_id);
	if (syncpt_gos_backing)
		return 0;

	/* Allocate and initialize backing */
	syncpt_gos_backing = kzalloc(sizeof(*syncpt_gos_backing), GFP_KERNEL);
	if (!syncpt_gos_backing) {
		nvhost_err(&engine_pdev->dev, "failed to allocate gos backing");
		return -ENOMEM;
	}

	if (nvmap_alloc_gos_slot(&engine_pdev->dev,
				&idx, &offset, &semaphore) < 0) {
		nvhost_err(&engine_pdev->dev, "all gos slots are busy");
		kfree(syncpt_gos_backing);
		return -ENOMEM;
	}

	syncpt_gos_backing->syncpt_id = syncpt_id;
	syncpt_gos_backing->gos_id = idx;
	syncpt_gos_backing->gos_offset = offset;

	/* Initialize semaphore in Grid to syncpoint value */
	*semaphore = nvhost_syncpt_read_min(&host->syncpt, syncpt_id);

	nvhost_syncpt_insert_syncpt_backing(&host->syncpt_backing_head,
			      syncpt_gos_backing);

	return 0;
}

/**
 * nvhost_syncpt_release_gos_backing() - Release GoS backing for a syncpoint
 *
 * @sp:		Pointer to nvhost_syncpt
 * @syncpt_id:	syncpoint id
 *
 * Return:	0 on success, a negative error code otherwise
 *
 * This function finds the GoS backing in global list, removes
 * it from list and then releases the backing
 */
int nvhost_syncpt_release_gos_backing(struct nvhost_syncpt *sp,
				      u32 syncpt_id)
{
	struct nvhost_master *host = syncpt_to_dev(sp);
	struct syncpt_gos_backing *syncpt_gos_backing;
	syncpt_gos_backing = nvhost_syncpt_find_gos_backing(host, syncpt_id);
	if (!syncpt_gos_backing)
		return -EINVAL;

	nvmap_free_gos_slot(syncpt_gos_backing->gos_id,
			syncpt_gos_backing->gos_offset);
	rb_erase(&syncpt_gos_backing->syncpt_gos_backing_entry,
		 &host->syncpt_backing_head);
	kfree(syncpt_gos_backing);

	return 0;
}
