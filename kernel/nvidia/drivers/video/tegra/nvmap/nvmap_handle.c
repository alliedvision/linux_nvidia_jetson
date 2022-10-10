/*
 * drivers/video/tegra/nvmap/nvmap_handle.c
 *
 * Handle allocation and freeing routines for nvmap
 *
 * Copyright (c) 2009-2022, NVIDIA CORPORATION. All rights reserved.
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

#define pr_fmt(fmt)	"%s: " fmt, __func__

#include <linux/err.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/mm.h>
#include <linux/rbtree.h>
#include <linux/dma-buf.h>
#include <linux/moduleparam.h>
#include <linux/nvmap.h>
#include <linux/version.h>
#if KERNEL_VERSION(4, 15, 0) > LINUX_VERSION_CODE
#include <soc/tegra/chip-id.h>
#else
#include <soc/tegra/fuse.h>
#endif

#include <asm/pgtable.h>

#include <trace/events/nvmap.h>

#include "nvmap_priv.h"
#include "nvmap_ioctl.h"

/*
 * Verifies that the passed ID is a valid handle ID. Then the passed client's
 * reference to the handle is returned.
 *
 * Note: to call this function make sure you own the client ref lock.
 */
struct nvmap_handle_ref *__nvmap_validate_locked(struct nvmap_client *c,
						 struct nvmap_handle *h,
						 bool is_ro)
{
	struct rb_node *n = c->handle_refs.rb_node;

	while (n) {
		struct nvmap_handle_ref *ref;
		ref = rb_entry(n, struct nvmap_handle_ref, node);
		if ((ref->handle == h) && (ref->is_ro == is_ro))
			return ref;
		else if ((uintptr_t)h > (uintptr_t)ref->handle)
			n = n->rb_right;
		else
			n = n->rb_left;
	}

	return NULL;
}
/* adds a newly-created handle to the device master tree */
void nvmap_handle_add(struct nvmap_device *dev, struct nvmap_handle *h)
{
	struct rb_node **p;
	struct rb_node *parent = NULL;

	spin_lock(&dev->handle_lock);
	p = &dev->handles.rb_node;
	while (*p) {
		struct nvmap_handle *b;

		parent = *p;
		b = rb_entry(parent, struct nvmap_handle, node);
		if (h > b)
			p = &parent->rb_right;
		else
			p = &parent->rb_left;
	}
	rb_link_node(&h->node, parent, p);
	rb_insert_color(&h->node, &dev->handles);
	nvmap_lru_add(h);
	spin_unlock(&dev->handle_lock);
}

/* remove a handle from the device's tree of all handles; called
 * when freeing handles. */
int nvmap_handle_remove(struct nvmap_device *dev, struct nvmap_handle *h)
{
	spin_lock(&dev->handle_lock);

	/* re-test inside the spinlock if the handle really has no clients;
	 * only remove the handle if it is unreferenced */
	if (atomic_add_return(0, &h->ref) > 0) {
		spin_unlock(&dev->handle_lock);
		return -EBUSY;
	}
	smp_rmb();
	BUG_ON(atomic_read(&h->ref) < 0);
	BUG_ON(atomic_read(&h->pin) != 0);

	nvmap_lru_del(h);
	rb_erase(&h->node, &dev->handles);

	spin_unlock(&dev->handle_lock);
	return 0;
}

/* Validates that a handle is in the device master tree and that the
 * client has permission to access it. */
struct nvmap_handle *nvmap_validate_get(struct nvmap_handle *id)
{
	struct nvmap_handle *h = NULL;
	struct rb_node *n;

	spin_lock(&nvmap_dev->handle_lock);

	n = nvmap_dev->handles.rb_node;

	while (n) {
		h = rb_entry(n, struct nvmap_handle, node);
		if (h == id) {
			h = nvmap_handle_get(h);
			spin_unlock(&nvmap_dev->handle_lock);
			return h;
		}
		if (id > h)
			n = n->rb_right;
		else
			n = n->rb_left;
	}
	spin_unlock(&nvmap_dev->handle_lock);
	return NULL;
}

static void add_handle_ref(struct nvmap_client *client,
			   struct nvmap_handle_ref *ref)
{
	struct rb_node **p, *parent = NULL;

	nvmap_ref_lock(client);
	p = &client->handle_refs.rb_node;
	while (*p) {
		struct nvmap_handle_ref *node;
		parent = *p;
		node = rb_entry(parent, struct nvmap_handle_ref, node);
		if (ref->handle > node->handle)
			p = &parent->rb_right;
		else
			p = &parent->rb_left;
	}
	rb_link_node(&ref->node, parent, p);
	rb_insert_color(&ref->node, &client->handle_refs);
	client->handle_count++;
	if (client->handle_count > nvmap_max_handle_count)
		nvmap_max_handle_count = client->handle_count;
	atomic_inc(&ref->handle->share_count);
	nvmap_ref_unlock(client);
}

struct nvmap_handle_ref *nvmap_create_handle_from_va(struct nvmap_client *client,
						     ulong vaddr, size_t size,
						     u32 flags)
{
	struct vm_area_struct *vma;
	struct nvmap_handle_ref *ref;
	vm_flags_t vm_flags;
	struct mm_struct *mm = current->mm;

	/* don't allow non-page aligned addresses. */
	if (vaddr & ~PAGE_MASK)
		return ERR_PTR(-EINVAL);

	nvmap_acquire_mmap_read_lock(mm);
	vma = find_vma(mm, vaddr);
	if (unlikely(!vma)) {
		nvmap_release_mmap_read_lock(mm);
		return ERR_PTR(-EINVAL);
	}

	if (!size)
		size = vma->vm_end - vaddr;

	/* Don't allow exuberantly large sizes. */
	if (!is_nvmap_memory_available(size, NVMAP_HEAP_IOVMM)) {
		pr_debug("Cannot allocate %zu bytes.\n", size);
		nvmap_release_mmap_read_lock(mm);
		return ERR_PTR(-ENOMEM);
	}

	vm_flags = vma->vm_flags;
	nvmap_release_mmap_read_lock(mm);

	/*
	 * If buffer is malloc/mprotect as RO but alloc flag is not passed
	 * as RO, don't create handle.
	 */
	if (!(vm_flags & VM_WRITE) && !(flags & NVMAP_HANDLE_RO))
		return ERR_PTR(-EINVAL);

	ref = nvmap_create_handle(client, size, flags & NVMAP_HANDLE_RO);
	if (!IS_ERR(ref))
		ref->handle->orig_size = size;

	return ref;
}

struct nvmap_handle_ref *nvmap_create_handle(struct nvmap_client *client,
					     size_t size, bool ro_buf)
{
	void *err = ERR_PTR(-ENOMEM);
	struct nvmap_handle *h;
	struct nvmap_handle_ref *ref = NULL;
	struct dma_buf *dmabuf;

	if (!client)
		return ERR_PTR(-EINVAL);

	if (!size)
		return ERR_PTR(-EINVAL);

	h = kzalloc(sizeof(*h), GFP_KERNEL);
	if (!h)
		return ERR_PTR(-ENOMEM);

	ref = kzalloc(sizeof(*ref), GFP_KERNEL);
	if (!ref)
		goto ref_alloc_fail;

	atomic_set(&h->ref, 1);
	atomic_set(&h->pin, 0);
	h->owner = client;
	BUG_ON(!h->owner);
	h->orig_size = size;
	h->size = PAGE_ALIGN(size);
	h->flags = NVMAP_HANDLE_WRITE_COMBINE;
	h->peer = NVMAP_IVM_INVALID_PEER;
	mutex_init(&h->lock);
	INIT_LIST_HEAD(&h->vmas);
	INIT_LIST_HEAD(&h->lru);
	INIT_LIST_HEAD(&h->dmabuf_priv);

	/*
	 * This takes out 1 ref on the dambuf. This corresponds to the
	 * handle_ref that gets automatically made by nvmap_create_handle().
	 */
	dmabuf = __nvmap_make_dmabuf(client, h, ro_buf);
	if (IS_ERR(dmabuf)) {
		err = dmabuf;
		goto make_dmabuf_fail;
	}
	if (!ro_buf)
		h->dmabuf = dmabuf;
	else
		h->dmabuf_ro = dmabuf;

	nvmap_handle_add(nvmap_dev, h);

	/*
	 * Major assumption here: the dma_buf object that the handle contains
	 * is created with a ref count of 1.
	 */
	atomic_set(&ref->dupes, 1);
	ref->handle = h;
	add_handle_ref(client, ref);
	if (ro_buf)
		ref->is_ro = true;
	else
		ref->is_ro = false;
	trace_nvmap_create_handle(client, client->name, h, size, ref);
	return ref;

make_dmabuf_fail:
	kfree(ref);
ref_alloc_fail:
	kfree(h);
	return err;
}

struct nvmap_handle_ref *nvmap_try_duplicate_by_ivmid(
		struct nvmap_client *client, u64 ivm_id,
		struct nvmap_heap_block **block)
{
	struct nvmap_handle *h = NULL;
	struct nvmap_handle_ref *ref = NULL;
	struct rb_node *n;

	spin_lock(&nvmap_dev->handle_lock);

	n = nvmap_dev->handles.rb_node;
	for (n = rb_first(&nvmap_dev->handles); n; n = rb_next(n)) {
		h = rb_entry(n, struct nvmap_handle, node);
		if (h->ivm_id == ivm_id) {
			BUG_ON(!virt_addr_valid(h));
			/* get handle's ref only if non-zero */
			if (atomic_inc_not_zero(&h->ref) == 0) {
				*block = h->carveout;
				/* strip handle's block and fail duplication */
				h->carveout = NULL;
				break;
			}
			spin_unlock(&nvmap_dev->handle_lock);
			goto found;
		}
	}

	spin_unlock(&nvmap_dev->handle_lock);
	/* handle is either freed or being freed, don't duplicate it */
	goto finish;

	/*
	 * From this point, handle and its buffer are valid and won't be
	 * freed as a reference is taken on it. The dmabuf can still be
	 * freed anytime till reference is taken on it below.
	 */
found:
	mutex_lock(&h->lock);
	/*
	 * Save this block. If dmabuf's reference is not held in time,
	 * this can be reused to avoid the delay to free the buffer
	 * in this old handle and allocate it for a new handle from
	 * the ivm allocation ioctl.
	 */
	*block = h->carveout;
	if (!h->dmabuf)
		goto fail;
	BUG_ON(!h->dmabuf->file);
	/* This is same as get_dma_buf() if file->f_count was non-zero */
	if (atomic_long_inc_not_zero(&h->dmabuf->file->f_count) == 0)
		goto fail;
	mutex_unlock(&h->lock);

	/* h->dmabuf can't be NULL anymore. Duplicate the handle. */
	ref = nvmap_duplicate_handle(client, h, true, false);
	/* put the extra ref taken using get_dma_buf. */
	dma_buf_put(h->dmabuf);
finish:
	return ref;
fail:
	/* free handle but not its buffer */
	h->carveout = NULL;
	mutex_unlock(&h->lock);
	nvmap_handle_put(h);
	return NULL;
}

struct nvmap_handle_ref *nvmap_duplicate_handle(struct nvmap_client *client,
					struct nvmap_handle *h, bool skip_val,
					bool is_ro)
{
	struct nvmap_handle_ref *ref = NULL;

	BUG_ON(!client);

	if (!skip_val)
		/* on success, the reference count for the handle should be
		 * incremented, so the success paths will not call
		 * nvmap_handle_put */
		h = nvmap_validate_get(h);

	if (!h) {
		pr_debug("%s duplicate handle failed\n",
			    current->group_leader->comm);
		return ERR_PTR(-EPERM);
	}

	if (!h->alloc) {
		pr_err("%s duplicating unallocated handle\n",
			current->group_leader->comm);
		nvmap_handle_put(h);
		return ERR_PTR(-EINVAL);
	}

	nvmap_ref_lock(client);
	ref = __nvmap_validate_locked(client, h, is_ro);

	if (ref) {
		atomic_inc(&ref->dupes);
		nvmap_ref_unlock(client);
		goto out;
	}

	nvmap_ref_unlock(client);

	ref = kzalloc(sizeof(*ref), GFP_KERNEL);
	if (!ref) {
		nvmap_handle_put(h);
		return ERR_PTR(-ENOMEM);
	}

	atomic_set(&ref->dupes, 1);
	ref->handle = h;
	add_handle_ref(client, ref);

	if (is_ro) {
		ref->is_ro = true;
		get_dma_buf(h->dmabuf_ro);
	} else {
		ref->is_ro = false;
		get_dma_buf(h->dmabuf);
	}

out:
	NVMAP_TAG_TRACE(trace_nvmap_duplicate_handle,
		NVMAP_TP_ARGS_CHR(client, h, ref));
	return ref;
}

struct nvmap_handle_ref *nvmap_create_handle_from_id(
			struct nvmap_client *client, u32 id)
{
	struct nvmap_handle *handle;
	struct nvmap_handle_ref *ref;

	if (WARN_ON(!client))
		return ERR_PTR(-EINVAL);

	if (is_nvmap_id_ro(client, id))
		return nvmap_dup_handle_ro(client, id);

	handle = nvmap_handle_get_from_id(client, id);
	if (IS_ERR_OR_NULL(handle)) {
		/* fd might be dmabuf fd received from parent process.
		 * Its entry is not made in id_array.
		 */
		handle = nvmap_handle_get_from_dmabuf_fd(client, id);
		if (IS_ERR(handle))
			return ERR_CAST(handle);
	}

	ref = nvmap_duplicate_handle(client, handle, false, false);
	nvmap_handle_put(handle);
	return ref;
}

struct nvmap_handle_ref *nvmap_create_handle_from_fd(
			struct nvmap_client *client, int fd)
{
	struct nvmap_handle *handle;
	struct nvmap_handle_ref *ref;
	bool is_ro;

	if (WARN_ON(!client))
		return ERR_PTR(-EINVAL);

	handle = nvmap_handle_get_from_dmabuf_fd(client, fd);
	if (IS_ERR(handle))
		return ERR_CAST(handle);

	is_ro = is_nvmap_dmabuf_fd_ro(fd);
	if (is_ro)
		ref = nvmap_duplicate_handle(client, handle, false, true);
	else
		ref = nvmap_duplicate_handle(client, handle, false, false);

	nvmap_handle_put(handle);
	return ref;
}

struct nvmap_handle_ref *nvmap_dup_handle_ro(struct nvmap_client *client,
					     int id)
{
	struct nvmap_handle *h;
	struct nvmap_handle_ref *ref = NULL;
	bool dmabuf_created = false;

	if (!client)
		return ERR_PTR(-EINVAL);

	h = nvmap_handle_get_from_id(client, id);
	if (IS_ERR_OR_NULL(h)) {
		/* fd might be dmabuf fd received from parent process.
		 * Its entry is not made in id_array.
		 */
		h = nvmap_handle_get_from_dmabuf_fd(client, id);
		if (IS_ERR(h))
			return ERR_CAST(h);
	}

	if (h->dmabuf_ro == NULL) {
		h->dmabuf_ro = __nvmap_make_dmabuf(client, h, true);
		if (IS_ERR(h->dmabuf_ro)) {
			nvmap_handle_put(h);
			return ERR_CAST(h->dmabuf_ro);
		}
		dmabuf_created = true;
	}

	ref = nvmap_duplicate_handle(client, h, false, true);
	if (!ref) {
		nvmap_handle_put(h);
		return ref;
	}
	/*
	 * When new dmabuf created (only RO dmabuf is getting created in this function)
	 * it's counter is incremented one extra time in nvmap_duplicate_handle. Hence
	 * decrement it by one.
	 */
	if (dmabuf_created)
		dma_buf_put(h->dmabuf_ro);

	nvmap_handle_put(h);

	return ref;
}
