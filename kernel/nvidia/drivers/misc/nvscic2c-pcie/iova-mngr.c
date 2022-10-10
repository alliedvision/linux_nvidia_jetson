// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2021-2022, NVIDIA CORPORATION.  All rights reserved.
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

#define pr_fmt(fmt)	"nvscic2c-pcie: iova-mgr: " fmt

#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/printk.h>
#include <linux/slab.h>
#include <linux/types.h>

#include "common.h"
#include "iova-mngr.h"

/*
 * INTERNAL DataStructure that define a single IOVA block/chunk
 * in a pool of IOVA region managed by IOVA manager.
 *
 * IOVA manager chunks entire IOVA space into these blocks/chunks.
 *
 * The chunk/block is also a node for linking to previous and next
 * nodes of a circular doubly linked list - free or reserved.
 */
struct block_t {
	/* for management of this chunk in either avail or reserve lists.*/
	struct list_head node;

	/* block address.*/
	u64 address;

	/* block size.*/
	size_t size;
};

/*
 * INTERNAL datastructure for IOVA space manager.
 *
 * IOVA space manager would fragment and manage the IOVA region
 * using two circular doubly linked lists - reserved list and
 * free list. These lists contain blocks/chunks reserved
 * or free for use by clients (callers) from the overall
 * IOVA region the IOVA manager was configured with.
 */
struct mngr_ctx_t {
	/*
	 * For debug purpose only, all other usage prohibited.
	 * In the event there are multiple iova_managers within
	 * an LKM instance, name helps in identification.
	 */
	char name[NAME_MAX];

	/*
	 * Circular doubly linked list of blocks indicating
	 * available/free IOVA space(s). When IOVA manager is
	 * initialised all of the IOVA space is marked as available
	 * to begin with.
	 */
	struct list_head *free_list;

	/*
	 * Book-keeping of the user IOVA blocks in a circular double
	 * linked list.
	 */
	struct list_head *reserved_list;

	/* Ensuring reserve, free and the list operations are serialized.*/
	struct mutex lock;

	/* base address memory manager is configured with. */
	u64 base_address;
};

/*
 * Reserves a block from the free IOVA regions. Once reserved, the block
 * is marked reserved and appended in the reserved list (no ordering
 * required and trying to do so shall increase the time)
 */
int
iova_mngr_block_reserve(void *mngr_handle, size_t size,
			u64 *address, size_t *offset,
			void **block_handle)
{
	struct mngr_ctx_t *ctx = (struct mngr_ctx_t *)(mngr_handle);
	struct block_t *reserve = NULL, *curr = NULL, *best = NULL;
	int ret = 0;

	if (WARN_ON(!ctx || *block_handle || !size))
		return -EINVAL;

	mutex_lock(&ctx->lock);

	/* if there are no free blocks to reserve. */
	if (list_empty(ctx->free_list)) {
		ret = -ENOMEM;
		pr_err("(%s): No memory available to reserve block of size:(%lu)\n",
		       ctx->name, size);
		goto err;
	}

	/* find the best of all free bocks to reserve.*/
	list_for_each_entry(curr, ctx->free_list, node) {
		if (curr->size >= size) {
			if (!best)
				best = curr;
			else if (curr->size < best->size)
				best = curr;
		}
	}

	/* if there isn't any free block of requested size. */
	if (!best) {
		ret = -ENOMEM;
		pr_err("(%s): No enough mem available to reserve block sz:(%lu)\n",
		       ctx->name, size);
		goto err;
	} else {
		struct block_t *found = NULL;

		/* perfect fit.*/
		if (best->size == size) {
			list_del(&best->node);
			list_add_tail(&best->node, ctx->reserved_list);
			found = best;
		} else {
			/* chunk out a new block, adjust the free block.*/
			reserve = kzalloc(sizeof(*reserve), GFP_KERNEL);
			if (WARN_ON(!reserve)) {
				ret = -ENOMEM;
				goto err;
			}
			reserve->address = best->address;
			reserve->size = size;
			best->address += size;
			best->size -= size;
			list_add_tail(&reserve->node, ctx->reserved_list);
			found = reserve;
		}
		*block_handle = (void *)(found);

		if (address)
			*address = found->address;
		if (offset)
			*offset = (found->address - ctx->base_address);
	}
err:
	mutex_unlock(&ctx->lock);
	return ret;
}

/*
 * Release an already reserved IOVA block/chunk by the caller back to
 * free list.
 */
int
iova_mngr_block_release(void *mngr_handle, void **block_handle)
{
	struct mngr_ctx_t *ctx = (struct mngr_ctx_t *)(mngr_handle);
	struct block_t *release = (struct block_t *)(*block_handle);
	struct block_t *curr = NULL, *prev = NULL;
	bool done = false;
	int ret = 0;

	if (!ctx || !release)
		return -EINVAL;

	mutex_lock(&ctx->lock);

	list_for_each_entry(curr, ctx->free_list, node) {
		if (release->address < curr->address) {
			/* if the immediate next node is available for merge.*/
			if (curr->address == release->address + release->size) {
				curr->address = release->address;
				curr->size += release->size;
				list_del(&release->node);
				kfree(release);
				/*
				 * if the immediate previous node is also
				 * available for merge.
				 */
				if ((prev) &&
				    ((prev->address + prev->size)
				      == curr->address)) {
					prev->size += curr->size;
					list_del(&curr->node);
					kfree(curr);
				}
			} else if ((prev) &&
				   ((prev->address + prev->size)
				    == release->address)) {
				/*
				 * if only the immediate prev node is available
				 */
				prev->size += release->size;
				list_del(&release->node);
				kfree(release);
			} else {
				/*
				 * cannot be merged with either the immediate
				 * prev or the immediate next node. Add it in the
				 * free list before the current node.
				 */
				list_del(&release->node);
				list_add_tail(&release->node, &curr->node);
			}
			done = true;
			break;
		}
		prev = curr;
	}

	/*
	 * Even if after traversing each entry in list, we could not
	 * add the block to be released back in free list, because:
	 */
	if (!done) {
		if (!list_empty(ctx->free_list)) {
			/*
			 * The block to be freed has the highest order
			 * address of all the existing blocks in free list.
			 */
			struct block_t *last =
			list_last_entry(ctx->free_list, struct block_t, node);
			if ((last->address + last->size) == release->address) {
				/* can be merged with last node of list.*/
				last->size += release->size;
				list_del(&release->node);
				kfree(release);
			} else {
				/* cannot be merged, add as the last node.*/
				list_del(&release->node);
				list_add_tail(&release->node, ctx->free_list);
			}
		} else {
			/* free list was empty.*/
			list_del(&release->node);
			list_add_tail(&release->node, ctx->free_list);
		}
	}
	*block_handle = NULL;

	mutex_unlock(&ctx->lock);
	return ret;
}

/*
 * iova_mngr_print
 *
 * DEBUG only.
 *
 * Helper function to print all the reserved and free blocks with
 * their names, size and start address.
 */
void
iova_mngr_print(void *mngr_handle)
{
	struct mngr_ctx_t *ctx = (struct mngr_ctx_t *)(mngr_handle);
	struct block_t *block = NULL;

	if (ctx) {
		mutex_lock(&ctx->lock);
		pr_debug("(%s): Reserved\n", ctx->name);
		list_for_each_entry(block, ctx->reserved_list, node) {
			pr_debug("\t\t (%s): address = 0x%pa[p], size = 0x%lx\n",
				 ctx->name, &block->address, block->size);
		}
		pr_debug("(%s): Free\n", ctx->name);
		list_for_each_entry(block, ctx->free_list, node) {
			pr_debug("\t\t (%s): address = 0x%pa[p], size = 0x%lx\n",
				 ctx->name, &block->address, block->size);
		}
		mutex_unlock(&ctx->lock);
	}
}

/*
 * Initialises the IOVA space manager with the base address + size
 * provided. IOVA manager would use two lists for book-keeping reserved
 * memory blocks and free memory blocks.
 *
 * When initialised all of the IOVA region: base_address + size is free.
 */
int
iova_mngr_init(char *name, u64 base_address, size_t size, void **mngr_handle)
{
	int ret = 0;
	struct block_t *block = NULL;
	struct mngr_ctx_t *ctx = NULL;

	if (WARN_ON(!base_address || !size ||
		    !mngr_handle || *mngr_handle || !name))
		return -EINVAL;

	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
	if (WARN_ON(!ctx)) {
		ret = -ENOMEM;
		goto err;
	}

	ctx->free_list = kzalloc(sizeof(*ctx->free_list), GFP_KERNEL);
	if (WARN_ON(!ctx->free_list)) {
		ret = -ENOMEM;
		goto err;
	}

	ctx->reserved_list = kzalloc(sizeof(*ctx->reserved_list), GFP_KERNEL);
	if (WARN_ON(!ctx->reserved_list)) {
		ret = -ENOMEM;
		goto err;
	}

	if (strlen(name) > (NAME_MAX - 1)) {
		ret = -EINVAL;
		pr_err("name: (%s) long, max char:(%u)\n", name, (NAME_MAX - 1));
		goto err;
	}
	strcpy(ctx->name, name);
	INIT_LIST_HEAD(ctx->reserved_list);
	INIT_LIST_HEAD(ctx->free_list);
	mutex_init(&ctx->lock);
	ctx->base_address = base_address;

	/* add the base_addrss+size as one whole free block.*/
	block = kzalloc(sizeof(*block), GFP_KERNEL);
	if (WARN_ON(!block)) {
		ret = -ENOMEM;
		goto err;
	}
	block->address = base_address;
	block->size = size;
	list_add(&block->node, ctx->free_list);

	*mngr_handle = ctx;
	return ret;
err:
	iova_mngr_deinit((void **)(&ctx));
	return ret;
}

/*
 * iova_mngr_deinit
 *
 * deinitialize the IOVA space manager. Any blocks unreturned from the client
 * (caller) shall become dangling.
 */
void
iova_mngr_deinit(void **mngr_handle)
{
	struct block_t *block = NULL;
	struct list_head *curr = NULL, *next = NULL;
	struct mngr_ctx_t *ctx = (struct mngr_ctx_t *)(*mngr_handle);

	if (ctx) {
		/* debug only to ensure, lists do not have dangling data left.*/
		iova_mngr_print(*mngr_handle);

		/* ideally, all blocks should have returned before this.*/
		if (!list_empty(ctx->reserved_list)) {
			list_for_each_safe(curr, next, ctx->reserved_list) {
				block = list_entry(curr, struct block_t, node);
				iova_mngr_block_release(*mngr_handle,
							(void **)(&block));
			}
		}

		/* ideally, just one whole free block should remain as free.*/
		list_for_each_safe(curr, next, ctx->free_list) {
			block = list_entry(curr, struct block_t, node);
			list_del(&block->node);
			kfree(block);
		}

		mutex_destroy(&ctx->lock);
		kfree(ctx->reserved_list);
		kfree(ctx->free_list);
		kfree(ctx);
		*mngr_handle = NULL;
	}
}
