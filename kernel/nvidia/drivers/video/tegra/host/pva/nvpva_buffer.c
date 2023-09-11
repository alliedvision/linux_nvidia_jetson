/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2016-2023, NVIDIA Corporation.  All rights reserved.
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
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/dma-buf.h>
#include <linux/cvnas.h>
#include <linux/nvhost.h>

#include "pva.h"
#include "nvpva_buffer.h"

/**
 * nvpva_vm_buffer - Virtual mapping information for a buffer
 *
 * @attach:		Pointer to dma_buf_attachment struct
 * @dmabuf:		Pointer to dma_buf struct
 * @sgt:		Pointer to sg_table struct
 * @addr:		Physical address of the buffer
 * @size:		Size of the buffer
 * @user_map_count:	Buffer reference count from user space
 * @submit_map_count:	Buffer reference count from task submit
 * @rb_node:		pinned buffer node
 * @list_head:		List entry
 *
 */
struct nvpva_vm_buffer {
	struct dma_buf_attachment	*attach;
	struct dma_buf			*dmabuf;
	struct sg_table			*sgt;
	dma_addr_t			addr;
	size_t				size;
	enum				nvpva_buffers_heap heap;
	s32				user_map_count;
	s32				submit_map_count;
	u32				id;
	dma_addr_t			user_addr;
	u64				user_offset;
	u64				user_size;
	struct				rb_node rb_node;
	struct				rb_node rb_node_id;
	struct				list_head list_head;
};

static uint32_t get_unique_id(struct nvpva_buffers *nvpva_buffers)
{
	struct nvhost_device_data *pdata =
		platform_get_drvdata(nvpva_buffers->pdev);
	struct pva *pva = pdata->private_data;
	uint32_t id = rmos_find_first_zero_bit(nvpva_buffers->ids,
					 NVPVA_MAX_NUM_UNIQUE_IDS);
	if (id == NVPVA_MAX_NUM_UNIQUE_IDS) {
		nvpva_dbg_fn(pva, "No buffer ID available");
		id = 0;
		goto out;
	}

	rmos_set_bit32((id%NVPVA_ID_SEGMENT_SIZE),
		&nvpva_buffers->ids[id/NVPVA_ID_SEGMENT_SIZE]);

	++(nvpva_buffers->num_assigned_ids);
	id |= 0x554c0000;
out:
	return id;
}

static int32_t put_unique_id(struct nvpva_buffers *nvpva_buffers, uint32_t id)
{
	id &= (~0x554c0000);
	if (!rmos_test_bit32((id % 32), &nvpva_buffers->ids[id / 32]))
		return -1;

	rmos_clear_bit32((id % 32), &nvpva_buffers->ids[id/32]);
	--(nvpva_buffers->num_assigned_ids);

	return 0;
}

#define COMPARE_AND_ASSIGN(a1, a2, b1, b2, c1, c2, curr, n1, n2)	\
	do {								\
		is_equal = false;					\
		if ((a1) > (a2))					\
			(curr) = (n1);					\
		else if ((a1) < (a2))					\
			(curr) = (n2);					\
		else if ((b1) > (b2))					\
			(curr) = (n1);					\
		else if ((b1) < (b2))					\
			(curr) = (n2);					\
		else if ((c1) > (c2))					\
			(curr) = (n1);					\
		else if ((c1) < (c2))					\
			(curr) = (n2);					\
		else							\
			is_equal = true;				\
	} while (0)



static struct nvpva_vm_buffer *
nvpva_find_map_buffer(struct nvpva_buffers *nvpva_buffers,
		      u64 offset,
		      u64 size,
		      struct dma_buf *dmabuf)
{
	struct rb_root *root = &nvpva_buffers->rb_root;
	struct rb_node *node = root->rb_node;
	struct nvpva_vm_buffer *vm;
	bool is_equal = false;

	/* check in a sorted tree */
	while (node) {
		vm = rb_entry(node, struct nvpva_vm_buffer,
						rb_node);
		COMPARE_AND_ASSIGN(vm->dmabuf,
				   dmabuf,
				   vm->user_offset,
				   offset,
				   vm->user_size,
				   size,
				   node,
				   node->rb_left,
				   node->rb_right);
		if (is_equal)
			return vm;
	}

	return NULL;
}

static struct nvpva_vm_buffer *nvpva_find_map_buffer_id(
	struct nvpva_buffers *nvpva_buffers, u32 id)
{
	struct rb_root *root = &nvpva_buffers->rb_root_id;
	struct rb_node *node = root->rb_node;
	struct nvpva_vm_buffer *vm;

	/* check in a sorted tree */
	while (node) {
		vm = rb_entry(node, struct nvpva_vm_buffer,
						rb_node_id);

		if (vm->id > id)
			node = node->rb_left;
		else if (vm->id != id)
			node = node->rb_right;
		else
			return vm;
	}

	return NULL;
}
static void nvpva_buffer_insert_map_buffer(
				struct nvpva_buffers *nvpva_buffers,
				struct nvpva_vm_buffer *new_vm)
{
	struct rb_node **new_node = &(nvpva_buffers->rb_root.rb_node);
	struct rb_node *parent = NULL;
	bool is_equal = false;

	/* Figure out where to put the new node */
	while (*new_node) {
		struct nvpva_vm_buffer *vm =
			rb_entry(*new_node, struct nvpva_vm_buffer,
						rb_node);
		parent = *new_node;

		COMPARE_AND_ASSIGN(vm->dmabuf,
				   new_vm->dmabuf,
				   vm->user_offset,
				   new_vm->user_offset,
				   vm->user_size,
				   new_vm->user_size,
				   new_node,
				   &((*new_node)->rb_left),
				   &((*new_node)->rb_right));
		if (is_equal)
			new_node = &((*new_node)->rb_right);
	}

	/* Add new node and rebalance tree */
	rb_link_node(&new_vm->rb_node, parent, new_node);
	rb_insert_color(&new_vm->rb_node, &nvpva_buffers->rb_root);

	/* Add the node into a list  */
	list_add_tail(&new_vm->list_head, &nvpva_buffers->list_head);
}

static void nvpva_buffer_insert_map_buffer_id(
				struct nvpva_buffers *nvpva_buffers,
				struct nvpva_vm_buffer *new_vm)
{
	struct rb_node **new_node = &(nvpva_buffers->rb_root_id.rb_node);
	struct rb_node *parent = NULL;

	/* Figure out where to put the new node */
	while (*new_node) {
		struct nvpva_vm_buffer *vm =
			rb_entry(*new_node, struct nvpva_vm_buffer,
						rb_node_id);
		parent = *new_node;

		if (vm->id > new_vm->id)
			new_node = &((*new_node)->rb_left);
		else
			new_node = &((*new_node)->rb_right);
	}

	/* Add new node and rebalance tree */
	rb_link_node(&new_vm->rb_node_id, parent, new_node);
	rb_insert_color(&new_vm->rb_node_id, &nvpva_buffers->rb_root_id);
}

static int
nvpva_buffer_map(struct platform_device *pdev,
		 struct platform_device *pdev_priv,
		 struct platform_device *pdev_user,
		 struct dma_buf *dmabuf,
		 u64 offset,
		 u64 size,
		 struct nvpva_vm_buffer *vm,
		 bool is_user)
{

	struct nvhost_device_data *pdata = platform_get_drvdata(pdev);
	struct pva *pva = pdata->private_data;
	const dma_addr_t cvnas_begin = nvcvnas_get_cvsram_base();
	const dma_addr_t cvnas_end = cvnas_begin + nvcvnas_get_cvsram_size();
	struct dma_buf_attachment *attach;
	struct sg_table *sgt;
	dma_addr_t dma_addr;
	dma_addr_t phys_addr;
	int err = 0;

	nvpva_dbg_fn(pva, "");

	get_dma_buf(dmabuf);
	if (is_user)
		attach = dma_buf_attach(dmabuf, &pdev_user->dev);
	else
		attach = dma_buf_attach(dmabuf, &pdev_priv->dev);

	if (IS_ERR_OR_NULL(attach)) {
		err = PTR_ERR(dmabuf);
		dev_err(&pdev->dev, "dma_attach failed: %d\n", err);
		goto buf_attach_err;
	}

	sgt = dma_buf_map_attachment(attach, DMA_BIDIRECTIONAL);
	if (IS_ERR_OR_NULL(sgt)) {
		err = PTR_ERR(sgt);
		dev_err(&pdev->dev, "dma mapping failed: %d\n", err);
		goto buf_map_err;
	}

	phys_addr = sg_phys(sgt->sgl);
	dma_addr = sg_dma_address(sgt->sgl);

	/* Determine the heap */
	if (phys_addr >= cvnas_begin && phys_addr < cvnas_end)
		vm->heap = NVPVA_BUFFERS_HEAP_CVNAS;
	else
		vm->heap = NVPVA_BUFFERS_HEAP_DRAM;

	/*
	 * If dma address is not available or heap is in CVNAS, use the
	 * physical address.
	 */
	if (!dma_addr || vm->heap == NVPVA_BUFFERS_HEAP_CVNAS)
		dma_addr = phys_addr;

	vm->sgt		= sgt;
	vm->attach	= attach;
	vm->dmabuf	= dmabuf;
	vm->addr	= dma_addr;
	vm->user_addr	= dma_addr + offset;

	vm->size = dmabuf->size;
	vm->user_offset = offset;
	vm->user_size = size;
	vm->user_map_count = 1;

	if (is_user)
		nvpva_dbg_fn(pva, "mapped user @ base %llx,  uaddr %llx,  size %llx\n",
			     (u64) dma_addr, (u64) vm->user_addr, size);
	else
		nvpva_dbg_fn(pva, "mapped priv @ base %llx,  uaddr  %llx,  size %llx\n",
			     (u64) dma_addr, (u64) vm->user_addr, size);

	return err;

buf_map_err:
	dma_buf_detach(dmabuf, attach);
buf_attach_err:
	dma_buf_put(dmabuf);
	return err;
}

static void nvpva_free_buffers(struct kref *kref)
{
	struct nvpva_buffers *nvpva_buffers =
		container_of(kref, struct nvpva_buffers, kref);

	kfree(nvpva_buffers);
}

static void nvpva_buffer_unmap(struct nvpva_buffers *nvpva_buffers,
				struct nvpva_vm_buffer *vm)
{
	struct nvhost_device_data *pdata = platform_get_drvdata(nvpva_buffers->pdev);
	struct pva *pva = pdata->private_data;

	nvpva_dbg_fn(pva, "");

	if ((vm->user_map_count != 0) || (vm->submit_map_count != 0))
		return;

	dma_buf_unmap_attachment(vm->attach, vm->sgt, DMA_BIDIRECTIONAL);
	dma_buf_detach(vm->dmabuf, vm->attach);
	dma_buf_put(vm->dmabuf);

	rb_erase(&vm->rb_node, &nvpva_buffers->rb_root);
	list_del(&vm->list_head);
	rb_erase(&vm->rb_node_id, &nvpva_buffers->rb_root_id);
	put_unique_id(nvpva_buffers, vm->id);

	kfree(vm);
}

struct nvpva_buffers
*nvpva_buffer_init(struct platform_device *pdev,
		   struct platform_device *pdev_priv,
		   struct platform_device *pdev_user)
{
	struct nvpva_buffers *nvpva_buffers;
	int err = 0;

	nvpva_buffers = kzalloc(sizeof(struct nvpva_buffers), GFP_KERNEL);
	if (!nvpva_buffers) {
		err = -ENOMEM;
		goto nvpva_buffer_init_err;
	}

	nvpva_buffers->pdev = pdev;
	nvpva_buffers->pdev_priv = pdev_priv;
	nvpva_buffers->pdev_user = pdev_user;
	mutex_init(&nvpva_buffers->mutex);
	nvpva_buffers->rb_root = RB_ROOT;
	nvpva_buffers->rb_root_id = RB_ROOT;
	INIT_LIST_HEAD(&nvpva_buffers->list_head);
	kref_init(&nvpva_buffers->kref);
	memset(nvpva_buffers->ids, 0, sizeof(nvpva_buffers->ids));
	nvpva_buffers->num_assigned_ids = 0;

	return nvpva_buffers;

nvpva_buffer_init_err:
	return ERR_PTR(err);
}

int nvpva_buffer_submit_pin_id(struct nvpva_buffers *nvpva_buffers,
			       u32 *ids,
			       u32 count,
			       struct dma_buf **dmabuf,
			       dma_addr_t *paddr,
			       u64 *psize,
			       enum nvpva_buffers_heap *heap)
{
	struct nvpva_vm_buffer *vm;
	int i = 0;

	kref_get(&nvpva_buffers->kref);

	mutex_lock(&nvpva_buffers->mutex);

	for (i = 0; i < count; i++) {
		vm = nvpva_find_map_buffer_id(nvpva_buffers, ids[i]);
		if (vm == NULL)
			goto submit_err;

		vm->submit_map_count++;
		paddr[i]  = vm->user_addr;
		dmabuf[i] = vm->dmabuf;
		psize[i]  = vm->user_size;

		/* Return heap only if requested */
		if (heap != NULL)
			heap[i] = vm->heap;
	}

	mutex_unlock(&nvpva_buffers->mutex);
	return 0;

submit_err:
	mutex_unlock(&nvpva_buffers->mutex);

	count = i;

	nvpva_buffer_submit_unpin_id(nvpva_buffers, ids, count);

	return -EINVAL;
}

int nvpva_buffer_pin(struct nvpva_buffers *nvpva_buffers,
		     struct dma_buf **dmabufs,
		     u64 *offset,
		     u64 *size,
		     u32 segment,
		     u32 count,
		     u32 *id,
		     u32 *eerr)
{
	struct nvpva_vm_buffer *vm;
	int i = 0;
	int err = 0;

	*eerr = 0;

	if (segment >= NVPVA_SEGMENT_MAX)
		return -EINVAL;

	mutex_lock(&nvpva_buffers->mutex);

	for (i = 0; i < count; i++) {
		u64 limit;

		if (U64_MAX - size[i] < offset[i]) {
			err = -EFAULT;
			goto unpin;
		} else {
			limit = (size[i] + offset[i]);
		}

		if (dmabufs[i]->size < limit) {
			err = -EFAULT;
			goto unpin;
		}

		vm = nvpva_find_map_buffer(nvpva_buffers,
					   offset[i],
					   size[i],
					   dmabufs[i]);
		if (vm) {
			vm->user_map_count++;
			id[i] = vm->id;
			continue;
		}

		vm = kzalloc(sizeof(struct nvpva_vm_buffer), GFP_KERNEL);
		if (!vm) {
			err = -ENOMEM;
			goto unpin;
		}

		vm->id = get_unique_id(nvpva_buffers);
		if (vm->id  == 0) {
			*eerr = NVPVA_ENOSLOT;
			err = -EINVAL;
			goto free_vm;
		}

		err = nvpva_buffer_map(nvpva_buffers->pdev,
				       nvpva_buffers->pdev_priv,
				       nvpva_buffers->pdev_user,
				       dmabufs[i],
				       offset[i],
				       size[i],
				       vm,
				       (segment == NVPVA_SEGMENT_USER));
		if (err) {
			put_unique_id(nvpva_buffers, vm->id);
			goto free_vm;
		}

		nvpva_buffer_insert_map_buffer(nvpva_buffers, vm);
		nvpva_buffer_insert_map_buffer_id(nvpva_buffers, vm);
		id[i] = vm->id;
	}

	mutex_unlock(&nvpva_buffers->mutex);

	return err;

free_vm:
	kfree(vm);
unpin:
	mutex_unlock(&nvpva_buffers->mutex);

	/* free pinned buffers */
	count = i;
	nvpva_buffer_unpin(nvpva_buffers, dmabufs, offset, size, count);

	return err;
}

void nvpva_buffer_submit_unpin_id(struct nvpva_buffers *nvpva_buffers,
				u32 *ids, u32 count)
{
	struct nvpva_vm_buffer *vm;
	int i = 0;

	mutex_lock(&nvpva_buffers->mutex);

	for (i = 0; i < count; i++) {

		vm = nvpva_find_map_buffer_id(nvpva_buffers, ids[i]);
		if (vm == NULL)
			continue;

		--vm->submit_map_count;
		if ((vm->submit_map_count) < 0)
			vm->submit_map_count = 0;

		nvpva_buffer_unmap(nvpva_buffers, vm);
	}

	mutex_unlock(&nvpva_buffers->mutex);

	kref_put(&nvpva_buffers->kref, nvpva_free_buffers);
}

void
nvpva_buffer_unpin(struct nvpva_buffers *nvpva_buffers,
		   struct dma_buf **dmabufs,
		   u64 *offset,
		   u64 *size,
		   u32 count)
{
	int i = 0;

	mutex_lock(&nvpva_buffers->mutex);

	for (i = 0; i < count; i++) {
		struct nvpva_vm_buffer *vm = NULL;

		vm = nvpva_find_map_buffer(nvpva_buffers,
					   offset[i],
					   size[i],
					   dmabufs[i]);
		if (vm == NULL)
			continue;

		--vm->user_map_count;
		if (vm->user_map_count < 0)
			vm->user_map_count = 0;

		nvpva_buffer_unmap(nvpva_buffers, vm);
	}

	mutex_unlock(&nvpva_buffers->mutex);
}

void nvpva_buffer_unpin_id(struct nvpva_buffers *nvpva_buffers,
			 u32 *ids, u32 count)
{
	int i = 0;

	mutex_lock(&nvpva_buffers->mutex);

	for (i = 0; i < count; i++) {
		struct nvpva_vm_buffer *vm = NULL;

		vm = nvpva_find_map_buffer_id(nvpva_buffers, ids[i]);
		if (vm == NULL)
			continue;

		--vm->user_map_count;
		if (vm->user_map_count < 0)
			vm->user_map_count = 0;

		nvpva_buffer_unmap(nvpva_buffers, vm);
	}

	mutex_unlock(&nvpva_buffers->mutex);
}

void nvpva_buffer_release(struct nvpva_buffers *nvpva_buffers)
{
	struct nvpva_vm_buffer *vm, *n;

	/* Go through each entry and remove it safely */
	mutex_lock(&nvpva_buffers->mutex);
	list_for_each_entry_safe(vm, n, &nvpva_buffers->list_head,
				 list_head) {
		vm->user_map_count = 0;
		nvpva_buffer_unmap(nvpva_buffers, vm);
	}
	mutex_unlock(&nvpva_buffers->mutex);

	kref_put(&nvpva_buffers->kref, nvpva_free_buffers);
}
