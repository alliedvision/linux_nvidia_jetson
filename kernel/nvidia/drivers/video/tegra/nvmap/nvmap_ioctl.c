/*
 * drivers/video/tegra/nvmap/nvmap_ioctl.c
 *
 * User-space interface to nvmap
 *
 * Copyright (c) 2011-2022, NVIDIA CORPORATION. All rights reserved.
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

#include <linux/dma-mapping.h>
#include <linux/export.h>
#include <linux/fs.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/nvmap.h>
#include <linux/vmalloc.h>
#include <linux/highmem.h>
#include <linux/mm.h>
#include <linux/mman.h>

#include <asm/io.h>
#include <asm/memory.h>
#include <asm/uaccess.h>
#include <soc/tegra/common.h>
#include <trace/events/nvmap.h>

#ifdef NVMAP_CONFIG_SCIIPC
#include <linux/nvscierror.h>
#include <linux/nvsciipc_interface.h>
#include "nvmap_sci_ipc.h"
#endif

#include "nvmap_ioctl.h"
#include "nvmap_priv.h"
#include "nvmap_heap.h"

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0)
#include <linux/syscalls.h>
#ifndef NVMAP_LOADABLE_MODULE
#include <linux/dma-map-ops.h>
#endif /* !NVMAP_LOADABLE_MODULE */
#endif

extern struct device tegra_vpr_dev;

static ssize_t rw_handle(struct nvmap_client *client, struct nvmap_handle *h,
			 int is_read, unsigned long h_offs,
			 unsigned long sys_addr, unsigned long h_stride,
			 unsigned long sys_stride, unsigned long elem_size,
			 unsigned long count);

struct nvmap_handle *nvmap_handle_get_from_id(struct nvmap_client *client,
		u32 id)
{
	struct nvmap_handle *handle = ERR_PTR(-EINVAL);
	struct nvmap_handle_info *info;
	struct dma_buf *dmabuf;

	if (WARN_ON(!client))
		return ERR_PTR(-EINVAL);

	if (client->ida) {
		dmabuf = dma_buf_get((int)id);
		/*
		 * id is dmabuf fd created from foreign dmabuf
		 * but handle as ID is enabled, hence it doesn't belong
		 * to nvmap_handle, bail out early.
		 */
		if (!IS_ERR_OR_NULL(dmabuf)) {
			dma_buf_put(dmabuf);
			return NULL;
		}

		dmabuf = nvmap_id_array_get_dmabuf_from_id(client->ida, id);
	} else {
		dmabuf = dma_buf_get((int)id);
	}
	if (IS_ERR_OR_NULL(dmabuf))
		return ERR_CAST(dmabuf);

	if (dmabuf_is_nvmap(dmabuf)) {
		info = dmabuf->priv;
		handle = info->handle;
		if (!nvmap_handle_get(handle))
			handle = ERR_PTR(-EINVAL);
	}

	dma_buf_put(dmabuf);

	if (!IS_ERR(handle))
		return handle;

	return	NULL;
}

struct nvmap_handle *nvmap_handle_get_from_fd(int fd)
{
	struct nvmap_handle *h;

	h = nvmap_handle_get_from_dmabuf_fd(NULL, fd);
	if (!IS_ERR(h))
		return h;
	return NULL;
}

static int nvmap_install_fd(struct nvmap_client *client,
	struct nvmap_handle *handle, int fd, void __user *arg,
	void *op, size_t op_size, bool free, struct dma_buf *dmabuf)
{
	int err = 0;
	struct nvmap_handle_info *info;

	if (!dmabuf) {
		err = -EFAULT;
		goto dmabuf_fail;
	}
	info = dmabuf->priv;
	if (IS_ERR_VALUE((uintptr_t)fd)) {
		err = fd;
		goto fd_fail;
	}

	if (copy_to_user(arg, op, op_size)) {
		err = -EFAULT;
		goto copy_fail;
	}

	fd_install(fd, dmabuf->file);
	return err;

copy_fail:
	put_unused_fd(fd);
fd_fail:
	if (dmabuf)
		dma_buf_put(dmabuf);
	if (free && handle)
		nvmap_free_handle(client, handle, info->is_ro);
dmabuf_fail:
	return err;
}

int nvmap_ioctl_getfd(struct file *filp, void __user *arg)
{
	struct nvmap_handle *handle = NULL;
	struct nvmap_create_handle op;
	struct nvmap_client *client = filp->private_data;
	struct dma_buf *dmabuf;
	int ret = 0;
	bool is_ro;

	if (copy_from_user(&op, arg, sizeof(op)))
		return -EFAULT;

	is_ro = is_nvmap_id_ro(client, op.handle);

	handle = nvmap_handle_get_from_id(client, op.handle);
	if (!IS_ERR_OR_NULL(handle)) {
		op.fd = nvmap_get_dmabuf_fd(client, handle, is_ro);
		nvmap_handle_put(handle);
		dmabuf = IS_ERR_VALUE((uintptr_t)op.fd) ?
			 NULL : (is_ro ? handle->dmabuf_ro : handle->dmabuf);
	} else {
		/*
		 * if we get an error, the fd might be non-nvmap dmabuf fd.
		 * Don't attach nvmap handle with this fd.
		 */
		dmabuf = dma_buf_get(op.handle);
		if (IS_ERR(dmabuf))
			return PTR_ERR(dmabuf);
		op.fd = nvmap_dmabuf_duplicate_gen_fd(client, dmabuf);
	}

	ret = nvmap_install_fd(client, handle,
				op.fd, arg, &op, sizeof(op), 0, dmabuf);

	if (!ret && !IS_ERR_OR_NULL(handle))
		trace_refcount_getfd(handle, dmabuf,
				atomic_read(&handle->ref),
				atomic_long_read(&dmabuf->file->f_count),
				is_ro ? "RO" : "RW");
	return ret;
}

int nvmap_ioctl_alloc(struct file *filp, void __user *arg)
{
	struct nvmap_alloc_handle op;
	struct nvmap_client *client = filp->private_data;
	struct nvmap_handle *handle;
	struct dma_buf *dmabuf = NULL;
	bool is_ro;
	int err;

	if (copy_from_user(&op, arg, sizeof(op)))
		return -EFAULT;

	if (op.align & (op.align - 1))
		return -EINVAL;

	if (!op.handle)
		return -EINVAL;

	handle = nvmap_handle_get_from_id(client, op.handle);
	if (IS_ERR_OR_NULL(handle))
		return -EINVAL;

	if (!is_nvmap_memory_available(handle->size, op.heap_mask)) {
		nvmap_handle_put(handle);
		return -ENOMEM;
	}

	/* user-space handles are aligned to page boundaries, to prevent
	 * data leakage. */
	op.align = max_t(size_t, op.align, PAGE_SIZE);

	err = nvmap_alloc_handle(client, handle, op.heap_mask, op.align,
				  0, /* no kind */
				  op.flags & (~NVMAP_HANDLE_KIND_SPECIFIED),
				  NVMAP_IVM_INVALID_PEER);
	nvmap_handle_put(handle);
	is_ro = is_nvmap_id_ro(client, op.handle);
	dmabuf = is_ro ? handle->dmabuf_ro : handle->dmabuf;

	if (!err)
		trace_refcount_alloc(handle, dmabuf,
				atomic_read(&handle->ref),
				atomic_long_read(&dmabuf->file->f_count),
				is_ro ? "RO" : "RW");
	return err;
}

int nvmap_ioctl_alloc_ivm(struct file *filp, void __user *arg)
{
	struct nvmap_client *client = filp->private_data;
	struct nvmap_alloc_ivm_handle op;
	struct nvmap_handle *handle;
	int err;

	if (copy_from_user(&op, arg, sizeof(op)))
		return -EFAULT;

	if (op.align & (op.align - 1))
		return -EINVAL;

	handle = nvmap_handle_get_from_id(client, op.handle);
	if (IS_ERR_OR_NULL(handle))
		return -EINVAL;

	/* user-space handles are aligned to page boundaries, to prevent
	 * data leakage. */
	op.align = max_t(size_t, op.align, PAGE_SIZE);

	err = nvmap_alloc_handle(client, handle, op.heap_mask, op.align,
				  0, /* no kind */
				  op.flags & (~NVMAP_HANDLE_KIND_SPECIFIED),
				  op.peer);
	nvmap_handle_put(handle);
	return err;
}

int nvmap_ioctl_vpr_floor_size(struct file *filp, void __user *arg)
{
	int err=0;
	u32 floor_size;

	if (copy_from_user(&floor_size, arg, sizeof(floor_size)))
		return -EFAULT;
#ifdef NVMAP_CONFIG_VPR_RESIZE
	err = dma_set_resizable_heap_floor_size(&tegra_vpr_dev, floor_size);
#endif
	return err;
}

int nvmap_ioctl_create(struct file *filp, unsigned int cmd, void __user *arg)
{
	struct nvmap_create_handle op;
	struct nvmap_handle_ref *ref = NULL;
	struct nvmap_client *client = filp->private_data;
	struct dma_buf *dmabuf = NULL;
	struct nvmap_handle *handle = NULL;
	int fd = -1, ret = 0;
	u32 id = 0;
	bool is_ro = false;

	if (copy_from_user(&op, arg, sizeof(op)))
		return -EFAULT;

	if (!client)
		return -ENODEV;

	if (cmd == NVMAP_IOC_CREATE)
		op.size64 = op.size;

	if ((cmd == NVMAP_IOC_CREATE) || (cmd == NVMAP_IOC_CREATE_64)) {
		ref = nvmap_create_handle(client, op.size64, false);
		if (!IS_ERR(ref))
			ref->handle->orig_size = op.size64;
	} else if (cmd == NVMAP_IOC_FROM_FD) {
		is_ro = is_nvmap_dmabuf_fd_ro(op.fd);
		ref = nvmap_create_handle_from_fd(client, op.fd);
		/* if we get an error, the fd might be non-nvmap dmabuf fd */
		if (IS_ERR_OR_NULL(ref)) {
			dmabuf = dma_buf_get(op.fd);
			if (IS_ERR(dmabuf))
				return PTR_ERR(dmabuf);
			fd = nvmap_dmabuf_duplicate_gen_fd(client, dmabuf);
			if (fd < 0)
				return fd;
		}
	} else {
		return -EINVAL;
	}

	if (!IS_ERR(ref)) {
		handle = ref->handle;
		dmabuf = is_ro ? handle->dmabuf_ro :  handle->dmabuf;

		if (client->ida) {

			if (nvmap_id_array_id_alloc(client->ida,
				&id, dmabuf) < 0) {
				if (dmabuf)
					dma_buf_put(dmabuf);
				nvmap_free_handle(client, handle, is_ro);
				return -ENOMEM;
			}
			if (cmd == NVMAP_IOC_CREATE_64)
				op.handle64 = id;
			else
				op.handle = id;

			if (copy_to_user(arg, &op, sizeof(op))) {
				if (dmabuf)
					dma_buf_put(dmabuf);
				nvmap_free_handle(client, handle, is_ro);
				nvmap_id_array_id_release(client->ida, id);
				return -EFAULT;
			}
			ret = 0;
			goto out;
		}

		fd = nvmap_get_dmabuf_fd(client, ref->handle, is_ro);
	} else if (!dmabuf) {
		return PTR_ERR(ref);
	}

	if (cmd == NVMAP_IOC_CREATE_64)
		op.handle64 = fd;
	else
		op.handle = fd;

	ret = nvmap_install_fd(client, handle, fd,
				arg, &op, sizeof(op), 1, dmabuf);

out:
	if (!ret && !IS_ERR_OR_NULL(handle)) {
		if (cmd == NVMAP_IOC_FROM_FD)
			trace_refcount_create_handle_from_fd(handle, dmabuf,
				atomic_read(&handle->ref),
				atomic_long_read(&dmabuf->file->f_count),
				is_ro ? "RO" : "RW");
		else
			trace_refcount_create_handle(handle, dmabuf,
				atomic_read(&handle->ref),
				atomic_long_read(&dmabuf->file->f_count),
				is_ro ? "RO" : "RW");
	}

	return ret;
}

int nvmap_ioctl_create_from_va(struct file *filp, void __user *arg)
{
	int fd = -1;
	u32 id = 0;
	int err;
	struct nvmap_create_handle_from_va op;
	struct nvmap_handle_ref *ref = NULL;
	struct nvmap_client *client = filp->private_data;
	struct dma_buf *dmabuf = NULL;
	struct nvmap_handle *handle = NULL;
	bool is_ro = false;

	if (copy_from_user(&op, arg, sizeof(op)))
		return -EFAULT;

	if (!client)
		return -ENODEV;

	is_ro = op.flags & NVMAP_HANDLE_RO;
	ref = nvmap_create_handle_from_va(client, op.va,
			op.size ? op.size : op.size64,
			op.flags);
	if (IS_ERR(ref))
		return PTR_ERR(ref);
	handle = ref->handle;

	err = nvmap_alloc_handle_from_va(client, handle,
					 op.va, op.flags);
	if (err) {
		nvmap_free_handle(client, handle, is_ro);
		return err;
	}

	dmabuf = is_ro ? ref->handle->dmabuf_ro : ref->handle->dmabuf;
	if (client->ida) {

		err = nvmap_id_array_id_alloc(client->ida, &id,
			dmabuf);
		if (err < 0) {
			if (dmabuf)
				dma_buf_put(dmabuf);
			nvmap_free_handle(client, ref->handle, is_ro);
			return -ENOMEM;
		}
		op.handle = id;
		if (copy_to_user(arg, &op, sizeof(op))) {
			if (dmabuf)
				dma_buf_put(dmabuf);
			nvmap_free_handle(client, ref->handle, is_ro);
			nvmap_id_array_id_release(client->ida, id);
			return -EFAULT;
		}
		err = 0;
		goto out;
	}

	fd = nvmap_get_dmabuf_fd(client, ref->handle, is_ro);

	op.handle = fd;

	err = nvmap_install_fd(client, ref->handle, fd,
				arg, &op, sizeof(op), 1, dmabuf);

out:
	if (!err)
		trace_refcount_create_handle_from_va(handle, dmabuf,
				atomic_read(&handle->ref),
				atomic_long_read(&dmabuf->file->f_count),
				is_ro ? "RO" : "RW");

	return err;
}

int nvmap_ioctl_rw_handle(struct file *filp, int is_read, void __user *arg,
			  size_t op_size)
{
	struct nvmap_client *client = filp->private_data;
	struct nvmap_rw_handle __user *uarg = arg;
	struct nvmap_rw_handle op;
#ifdef CONFIG_COMPAT
	struct nvmap_rw_handle_32 __user *uarg32 = arg;
	struct nvmap_rw_handle_32 op32;
#endif
	struct nvmap_handle *h;
	ssize_t copied;
	int err = 0;
	unsigned long addr, offset, elem_size, hmem_stride, user_stride;
	unsigned long count;
	int handle;

#ifdef CONFIG_COMPAT
	if (op_size == sizeof(op32)) {
		if (copy_from_user(&op32, arg, sizeof(op32)))
			return -EFAULT;
		addr = op32.addr;
		handle = op32.handle;
		offset = op32.offset;
		elem_size = op32.elem_size;
		hmem_stride = op32.hmem_stride;
		user_stride = op32.user_stride;
		count = op32.count;
	} else
#endif
	{
		if (copy_from_user(&op, arg, sizeof(op)))
			return -EFAULT;
		addr = op.addr;
		handle = op.handle;
		offset = op.offset;
		elem_size = op.elem_size;
		hmem_stride = op.hmem_stride;
		user_stride = op.user_stride;
		count = op.count;
	}

	if (!addr || !count || !elem_size)
		return -EINVAL;

	h = nvmap_handle_get_from_id(client, handle);
	if (IS_ERR_OR_NULL(h))
		return -EINVAL;

	/* Don't allow write on RO handle */
	if (!is_read && is_nvmap_id_ro(client, handle)) {
		nvmap_handle_put(h);
		return -EPERM;
	}

	if (is_read && h->heap_type == NVMAP_HEAP_CARVEOUT_VPR) {
		nvmap_handle_put(h);
		return -EPERM;
	}

	/*
	 * If Buffer is RO and write operation is asked from the buffer,
	 * return error.
	 */
	if (h->is_ro && !is_read) {
		nvmap_handle_put(h);
		return -EPERM;
	}

	nvmap_kmaps_inc(h);
	trace_nvmap_ioctl_rw_handle(client, h, is_read, offset,
				    addr, hmem_stride,
				    user_stride, elem_size, count);
	copied = rw_handle(client, h, is_read, offset,
			   addr, hmem_stride,
			   user_stride, elem_size, count);
	nvmap_kmaps_dec(h);

	if (copied < 0) {
		err = copied;
		copied = 0;
	} else if (copied < (count * elem_size))
		err = -EINTR;

#ifdef CONFIG_COMPAT
	if (op_size == sizeof(op32))
		__put_user(copied, &uarg32->count);
	else
#endif
		__put_user(copied, &uarg->count);

	nvmap_handle_put(h);

	return err;
}

int nvmap_ioctl_cache_maint(struct file *filp, void __user *arg, int op_size)
{
	struct nvmap_client *client = filp->private_data;
	struct nvmap_cache_op op;
	struct nvmap_cache_op_64 op64;
#ifdef CONFIG_COMPAT
	struct nvmap_cache_op_32 op32;
#endif

#ifdef CONFIG_COMPAT
	if (op_size == sizeof(op32)) {
		if (copy_from_user(&op32, arg, sizeof(op32)))
			return -EFAULT;
		op64.addr = op32.addr;
		op64.handle = op32.handle;
		op64.len = op32.len;
		op64.op = op32.op;
	} else
#endif
	{
		if (op_size == sizeof(op)) {
			if (copy_from_user(&op, arg, sizeof(op)))
				return -EFAULT;
			op64.addr = op.addr;
			op64.handle = op.handle;
			op64.len = op.len;
			op64.op = op.op;
		} else {
			if (copy_from_user(&op64, arg, sizeof(op64)))
				return -EFAULT;
		}
	}

	return __nvmap_cache_maint(client, &op64);
}

int nvmap_ioctl_free(struct file *filp, unsigned long arg)
{
	struct nvmap_client *client = filp->private_data;
	struct dma_buf *dmabuf = NULL;

	if (!arg || IS_ERR_OR_NULL(client))
		return 0;

	nvmap_free_handle_from_fd(client, arg);

	if (client->ida) {
		dmabuf = dma_buf_get(arg);
		/* id is dmabuf fd created from foreign dmabuf */
		if (!IS_ERR_OR_NULL(dmabuf)) {
			dma_buf_put(dmabuf);
			goto close_fd;
		}
		return 0;
	}
close_fd:
	return SYS_CLOSE(arg);
}

static ssize_t rw_handle(struct nvmap_client *client, struct nvmap_handle *h,
			 int is_read, unsigned long h_offs,
			 unsigned long sys_addr, unsigned long h_stride,
			 unsigned long sys_stride, unsigned long elem_size,
			 unsigned long count)
{
	ssize_t copied = 0;
	void *tmp = NULL;
	void *addr;
	int ret = 0;

	if (!(h->heap_type & nvmap_dev->cpu_access_mask))
		return -EPERM;

	if (!elem_size || !count)
		return -EINVAL;

	if (!h->alloc)
		return -EFAULT;

	if (elem_size == h_stride && elem_size == sys_stride && (h_offs % 8 == 0)) {
		elem_size *= count;
		h_stride = elem_size;
		sys_stride = elem_size;
		count = 1;
	}

	if (elem_size > h->size ||
		h_offs >= h->size ||
		elem_size > sys_stride ||
		elem_size > h_stride ||
		sys_stride > (h->size - h_offs) / count ||
		h_offs + h_stride * (count - 1) + elem_size > h->size)
		return -EINVAL;

	if (!h->vaddr) {
		if (!__nvmap_mmap(h))
			return -ENOMEM;
		__nvmap_munmap(h, h->vaddr);
	}

	addr = h->vaddr + h_offs;

	/* Allocate buffer to cache data for VPR write */
	if (!is_read && h->heap_type == NVMAP_HEAP_CARVEOUT_VPR) {
		tmp = vmalloc(elem_size);
		if (!tmp)
			return -ENOMEM;
	}

	while (count--) {
		if (h_offs + elem_size > h->size) {
			pr_warn("read/write outside of handle\n");
			ret = -EFAULT;
			break;
		}
		if (is_read &&
		    !(h->userflags & NVMAP_HANDLE_CACHE_SYNC_AT_RESERVE))
			__nvmap_do_cache_maint(client, h, h_offs,
				h_offs + elem_size, NVMAP_CACHE_OP_INV, false);

		if (is_read)
			ret = copy_to_user((void __user *)sys_addr, addr, elem_size);
		else {
			if (h->heap_type == NVMAP_HEAP_CARVEOUT_VPR) {
				ret = copy_from_user(tmp, (void __user *)sys_addr,
						     elem_size);
				if (!ret)
					kasan_memcpy_toio((void __iomem *)addr, tmp, elem_size);
			} else
				ret = copy_from_user(addr, (void __user *)sys_addr, elem_size);
		}

		if (ret)
			break;

		if (!is_read &&
		    !(h->userflags & NVMAP_HANDLE_CACHE_SYNC_AT_RESERVE))
			__nvmap_do_cache_maint(client, h, h_offs,
				h_offs + elem_size, NVMAP_CACHE_OP_WB_INV,
				false);

		copied += elem_size;
		sys_addr += sys_stride;
		h_offs += h_stride;
		addr += h_stride;
	}

	/* Release the buffer used for VPR write */
	if (!is_read && h->heap_type == NVMAP_HEAP_CARVEOUT_VPR && tmp)
		vfree(tmp);

	return ret ?: copied;
}

int nvmap_ioctl_get_ivcid(struct file *filp, void __user *arg)
{
	struct nvmap_client *client = filp->private_data;
	struct nvmap_create_handle op;
	struct nvmap_handle *h = NULL;

	if (copy_from_user(&op, arg, sizeof(op)))
		return -EFAULT;

	h = nvmap_handle_get_from_id(client, op.ivm_handle);
	if (IS_ERR_OR_NULL(h))
		return -EINVAL;

	if (!h->alloc) { /* || !h->ivm_id) { */
		nvmap_handle_put(h);
		return -EFAULT;
	}

	op.ivm_id = h->ivm_id;

	nvmap_handle_put(h);

	return copy_to_user(arg, &op, sizeof(op)) ? -EFAULT : 0;
}

int nvmap_ioctl_get_ivc_heap(struct file *filp, void __user *arg)
{
	struct nvmap_device *dev = nvmap_dev;
	int i;
	unsigned int heap_mask = 0;

	for (i = 0; i < dev->nr_carveouts; i++) {
		struct nvmap_carveout_node *co_heap = &dev->heaps[i];
		int peer;

		if (!(co_heap->heap_bit & NVMAP_HEAP_CARVEOUT_IVM))
			continue;

		peer = nvmap_query_heap_peer(co_heap->carveout);
		if (peer < 0)
			return -EINVAL;

		heap_mask |= BIT(peer);
	}

	if (copy_to_user(arg, &heap_mask, sizeof(heap_mask)))
		return -EFAULT;

	return 0;
}

int nvmap_ioctl_create_from_ivc(struct file *filp, void __user *arg)
{
	struct nvmap_create_handle op;
	struct nvmap_handle_ref *ref;
	struct nvmap_client *client = filp->private_data;
	int fd;
	phys_addr_t offs;
	size_t size = 0;
	int peer;
	struct nvmap_heap_block *block = NULL;

	/* First create a new handle and then fake carveout allocation */
	if (copy_from_user(&op, arg, sizeof(op)))
		return -EFAULT;

	if (!client)
		return -ENODEV;

	ref = nvmap_try_duplicate_by_ivmid(client, op.ivm_id, &block);
	if (!ref) {
		/*
		 * See nvmap_heap_alloc() for encoding details.
		 */
		offs = ((op.ivm_id &
		       ~((u64)NVMAP_IVM_IVMID_MASK << NVMAP_IVM_IVMID_SHIFT)) >>
			NVMAP_IVM_LENGTH_WIDTH) << (ffs(NVMAP_IVM_ALIGNMENT) - 1);
		size = (op.ivm_id &
			((1ULL << NVMAP_IVM_LENGTH_WIDTH) - 1)) << PAGE_SHIFT;
		peer = (op.ivm_id >> NVMAP_IVM_IVMID_SHIFT);

		ref = nvmap_create_handle(client, size, false);
		if (IS_ERR(ref)) {
			nvmap_heap_free(block);
			return PTR_ERR(ref);
		}
		ref->handle->orig_size = size;

		ref->handle->peer = peer;
		if (!block)
			block = nvmap_carveout_alloc(client, ref->handle,
					NVMAP_HEAP_CARVEOUT_IVM, &offs);
		if (!block) {
			nvmap_free_handle(client, ref->handle, false);
			return -ENOMEM;
		}

		ref->handle->heap_type = NVMAP_HEAP_CARVEOUT_IVM;
		ref->handle->heap_pgalloc = false;
		ref->handle->ivm_id = op.ivm_id;
		ref->handle->carveout = block;
		block->handle = ref->handle;
		mb();
		ref->handle->alloc = true;
		NVMAP_TAG_TRACE(trace_nvmap_alloc_handle_done,
			NVMAP_TP_ARGS_CHR(client, ref->handle, ref));
	}
	if (client->ida) {
		u32 id = 0;

		if (nvmap_id_array_id_alloc(client->ida, &id,
			ref->handle->dmabuf) < 0) {
			if (ref->handle->dmabuf)
				dma_buf_put(ref->handle->dmabuf);
			nvmap_free_handle(client, ref->handle, false);
			return -ENOMEM;
		}
		op.ivm_handle = id;
		if (copy_to_user(arg, &op, sizeof(op))) {
			if (ref->handle->dmabuf)
				dma_buf_put(ref->handle->dmabuf);
			nvmap_free_handle(client, ref->handle, false);
			nvmap_id_array_id_release(client->ida, id);
			return -EFAULT;
		}
		return 0;
	}

	fd = nvmap_get_dmabuf_fd(client, ref->handle, false);

	op.ivm_handle = fd;
	return nvmap_install_fd(client, ref->handle, fd,
				arg, &op, sizeof(op), 1, ref->handle->dmabuf);
}

int nvmap_ioctl_cache_maint_list(struct file *filp, void __user *arg)
{
	struct nvmap_client *client = filp->private_data;
	struct nvmap_cache_op_list op;
	u32 *handle_ptr;
	u64 *offset_ptr;
	u64 *size_ptr;
	struct nvmap_handle **refs;
	int err = 0;
	u32 i, n_unmarshal_handles = 0, count = 0;
	size_t bytes;
	size_t elem_size;
	bool is_32;

	if (copy_from_user(&op, arg, sizeof(op)))
		return -EFAULT;

	if (!op.nr || op.nr > UINT_MAX / sizeof(u32))
		return -EINVAL;

	bytes = op.nr * sizeof(*refs);
	if (!ACCESS_OK(VERIFY_READ, (const void __user *)op.handles, op.nr * sizeof(u32)))
		return -EFAULT;

	elem_size  = (op.op & NVMAP_ELEM_SIZE_U64) ?
			sizeof(u64) : sizeof(u32);
	op.op &= ~NVMAP_ELEM_SIZE_U64;
	is_32 = elem_size == sizeof(u32) ? 1 : 0;

	bytes += 2 * op.nr * elem_size;
	bytes += op.nr * sizeof(u32);
	refs = nvmap_altalloc(bytes);
	if (!refs) {
		pr_err("memory allocation failed\n");
		return -ENOMEM;
	}

	offset_ptr = (u64 *)(refs + op.nr);
	size_ptr = (u64 *)(((uintptr_t)offset_ptr) + op.nr * elem_size);
	handle_ptr = (u32 *)(((uintptr_t)size_ptr) + op.nr * elem_size);

	if (!op.handles || !op.offsets || !op.sizes) {
		pr_err("pointers are invalid\n");
		err = -EINVAL;
		goto free_mem;
	}

	if (!IS_ALIGNED((ulong)offset_ptr, elem_size) ||
	    !IS_ALIGNED((ulong)size_ptr, elem_size) ||
	    !IS_ALIGNED((ulong)handle_ptr, sizeof(u32))) {
		pr_err("pointers are not properly aligned!!\n");
		err = -EINVAL;
		goto free_mem;
	}

	if (copy_from_user(handle_ptr, (void __user *)op.handles,
		op.nr * sizeof(u32))) {
		pr_err("Can't copy from user pointer op.handles\n");
		err = -EFAULT;
		goto free_mem;
	}

	if (copy_from_user(offset_ptr, (void __user *)op.offsets,
		op.nr * elem_size)) {
		pr_err("Can't copy from user pointer op.offsets\n");
		err = -EFAULT;
		goto free_mem;
	}

	if (copy_from_user(size_ptr, (void __user *)op.sizes,
		op.nr * elem_size)) {
		pr_err("Can't copy from user pointer op.sizes\n");
		err = -EFAULT;
		goto free_mem;
	}

	for (i = 0; i < op.nr; i++) {
		refs[i] = nvmap_handle_get_from_id(client, handle_ptr[i]);
		if (IS_ERR_OR_NULL(refs[i])) {
			pr_err("invalid handle_ptr[%d] = %u\n",
				i, handle_ptr[i]);
			err = -EINVAL;
			goto free_mem;
		}
		if (!(refs[i]->heap_type & nvmap_dev->cpu_access_mask)) {
			pr_err("heap %x can't be accessed from cpu\n",
				refs[i]->heap_type);
			err = -EPERM;
			goto free_mem;
		}

		n_unmarshal_handles++;
	}

	/*
	 * Either all handles should have NVMAP_HANDLE_CACHE_SYNC_AT_RESERVE
	 * or none should have it.
	 */
	for (i = 0; i < op.nr; i++)
		if (refs[i]->userflags & NVMAP_HANDLE_CACHE_SYNC_AT_RESERVE)
			count++;

	if (op.nr && count % op.nr) {
		pr_err("incorrect CACHE_SYNC_AT_RESERVE mix of handles\n");
		err = -EINVAL;
		goto free_mem;
	}

	/*
	 * when NVMAP_HANDLE_CACHE_SYNC_AT_RESERVE is specified mix can cause
	 * cache WB_INV at unreserve op on iovmm handles increasing overhead.
	 * So, either all handles should have pages from carveout or from iovmm.
	 */
	if (count) {
		for (i = 0; i < op.nr; i++)
			if (refs[i]->heap_pgalloc)
				count++;

		if (op.nr && count % op.nr) {
			pr_err("all or none of the handles should be from heap\n");
			err = -EINVAL;
			goto free_mem;
		}
	}

	err = nvmap_do_cache_maint_list(refs, offset_ptr, size_ptr,
						op.op, op.nr, is_32);

free_mem:
	for (i = 0; i < n_unmarshal_handles; i++)
		nvmap_handle_put(refs[i]);
	nvmap_altfree(refs, bytes);
	return err;
}

int nvmap_ioctl_gup_test(struct file *filp, void __user *arg)
{
	struct nvmap_client *client = filp->private_data;
	int err = -EINVAL;
	struct nvmap_gup_test op;
	struct vm_area_struct *vma;
	struct nvmap_handle *handle;
	size_t i;
	size_t nr_page;
	struct page **pages;
	struct mm_struct *mm = current->mm;

	if (copy_from_user(&op, arg, sizeof(op)))
		return -EFAULT;
	op.result = 1;

	nvmap_acquire_mmap_read_lock(mm);
	vma = find_vma(mm, op.va);
	if (unlikely(!vma) || (unlikely(op.va < vma->vm_start )) ||
		unlikely(op.va >= vma->vm_end)) {
		nvmap_release_mmap_read_lock(mm);
		goto exit;
	}

	handle = nvmap_handle_get_from_id(client, op.handle);
	if (IS_ERR_OR_NULL(handle)) {
		nvmap_release_mmap_read_lock(mm);
		goto exit;
	}

	if (vma->vm_end - vma->vm_start != handle->size) {
		pr_err("handle size(0x%zx) and vma size(0x%lx) don't match\n",
			 handle->size, vma->vm_end - vma->vm_start);
		nvmap_release_mmap_read_lock(mm);
		goto put_handle;
	}

	err = -ENOMEM;
	nr_page = handle->size >> PAGE_SHIFT;
	pages = nvmap_altalloc(nr_page * sizeof(*pages));
	if (IS_ERR_OR_NULL(pages)) {
		err = PTR_ERR(pages);
		nvmap_release_mmap_read_lock(mm);
		goto put_handle;
	}

	err = nvmap_get_user_pages(op.va & PAGE_MASK, nr_page, pages, false, 0);
	if (err) {
		nvmap_release_mmap_read_lock(mm);
		goto free_pages;
	}

	nvmap_release_mmap_read_lock(mm);

	for (i = 0; i < nr_page; i++) {
		if (handle->pgalloc.pages[i] != pages[i]) {
			pr_err("page pointers don't match, %p %p\n",
			       handle->pgalloc.pages[i], pages[i]);
			op.result = 0;
		}
	}

	if (op.result)
		err = 0;

	if (copy_to_user(arg, &op, sizeof(op)))
		err = -EFAULT;

	for (i = 0; i < nr_page; i++) {
		put_page(pages[i]);
	}
free_pages:
	nvmap_altfree(pages, nr_page * sizeof(*pages));
put_handle:
	nvmap_handle_put(handle);
exit:
	pr_info("GUP Test %s\n", err ? "failed" : "passed");
	return err;
}

int nvmap_ioctl_set_tag_label(struct file *filp, void __user *arg)
{
	struct nvmap_set_tag_label op;
	struct nvmap_device *dev = nvmap_dev;
	int err;

	if (copy_from_user(&op, arg, sizeof(op)))
		return -EFAULT;

	if (op.len > NVMAP_TAG_LABEL_MAXLEN)
		op.len = NVMAP_TAG_LABEL_MAXLEN;

	if (op.len)
		err = nvmap_define_tag(dev, op.tag,
			(const char __user *)op.addr, op.len);
	else
		err = nvmap_remove_tag(dev, op.tag);

	return err;
}

int nvmap_ioctl_get_available_heaps(struct file *filp, void __user *arg)
{
	struct nvmap_available_heaps op;
	int i;

	memset(&op, 0, sizeof(op));

	for (i = 0; i < nvmap_dev->nr_carveouts; i++)
		op.heaps |= nvmap_dev->heaps[i].heap_bit;

	if (copy_to_user(arg, &op, sizeof(op))) {
		pr_err("copy_to_user failed\n");
		return -EINVAL;
	}

	return 0;
}

int nvmap_ioctl_get_heap_size(struct file *filp, void __user *arg)
{
	struct nvmap_heap_size op;
	struct nvmap_heap *heap;
	int i;
	memset(&op, 0, sizeof(op));

	if (copy_from_user(&op, arg, sizeof(op)))
		return -EFAULT;

	for (i = 0; i < nvmap_dev->nr_carveouts; i++) {
		if (op.heap & nvmap_dev->heaps[i].heap_bit) {
			heap = nvmap_dev->heaps[i].carveout;
			op.size = nvmap_query_heap_size(heap);
			if (copy_to_user(arg, &op, sizeof(op)))
				return -EFAULT;
			return 0;
		}
	}
	return -ENODEV;

}

int nvmap_ioctl_get_handle_parameters(struct file *filp, void __user *arg)
{
	struct nvmap_client *client = filp->private_data;
	struct nvmap_handle_parameters op;
	struct nvmap_handle *handle;
	bool is_ro = false;

	if (copy_from_user(&op, arg, sizeof(op)))
		return -EFAULT;

	handle = nvmap_handle_get_from_id(client, op.handle);
	if (IS_ERR_OR_NULL(handle))
		goto exit;

	if (!handle->alloc) {
		op.heap = 0;
	} else {
		op.heap = handle->heap_type;
	}

	/* heap_number, only valid for IVM carveout */
	op.heap_number = handle->peer;

	op.size = handle->size;

	if (handle->userflags & NVMAP_HANDLE_PHYS_CONTIG) {
		op.contig = 1U;
	} else {
		op.contig = 0U;
	}

	op.align = handle->align;

	op.offset = handle->offs;

	op.coherency = handle->flags;

	is_ro = is_nvmap_id_ro(client, op.handle);
	if (is_ro)
		op.access_flags = NVMAP_HANDLE_RO;

	nvmap_handle_put(handle);

	if (copy_to_user(arg, &op, sizeof(op)))
		return -EFAULT;
	return 0;

exit:
	return -ENODEV;
}

#ifdef NVMAP_CONFIG_SCIIPC
int nvmap_ioctl_get_sci_ipc_id(struct file *filp, void __user *arg)
{
	struct nvmap_client *client = filp->private_data;
	NvSciIpcEndpointVuid pr_vuid, lclu_vuid;
	struct nvmap_handle *handle = NULL;
	struct nvmap_sciipc_map op;
	struct dma_buf *dmabuf = NULL;
	bool is_ro = false;
	int ret = 0;

	if (copy_from_user(&op, arg, sizeof(op)))
		return -EFAULT;

	handle = nvmap_handle_get_from_id(client, op.handle);
	if (IS_ERR_OR_NULL(handle))
		return -ENODEV;

	is_ro = is_nvmap_id_ro(client, op.handle);

	/* Cannot create RW handle from RO handle */
	if (is_ro && (op.flags != PROT_READ)) {
		ret = -EPERM;
		goto exit;
	}

	ret = nvmap_validate_sci_ipc_params(client, op.auth_token,
		&pr_vuid, &lclu_vuid);
	if (ret)
		goto exit;

	ret = nvmap_create_sci_ipc_id(client, handle, op.flags,
			 &op.sci_ipc_id, pr_vuid, is_ro);
	if (ret)
		goto exit;

	if (copy_to_user(arg, &op, sizeof(op))) {
		pr_err("copy_to_user failed\n");
		ret = -EINVAL;
	}
exit:
	nvmap_handle_put(handle);
	dmabuf = is_ro ? handle->dmabuf_ro : handle->dmabuf;

	if (!ret)
		trace_refcount_get_sci_ipc_id(handle, dmabuf,
				atomic_read(&handle->ref),
				atomic_long_read(&dmabuf->file->f_count),
				is_ro ? "RO" : "RW");
	return ret;
}

int nvmap_ioctl_handle_from_sci_ipc_id(struct file *filp, void __user *arg)
{
	struct nvmap_client *client = filp->private_data;
	NvSciIpcEndpointVuid pr_vuid, lclu_vuid;
	struct nvmap_sciipc_map op;
	int ret = 0;

	if (copy_from_user(&op, arg, sizeof(op))) {
		ret =  -EFAULT;
		goto exit;
	}

	ret = nvmap_validate_sci_ipc_params(client, op.auth_token,
		&pr_vuid, &lclu_vuid);
	if (ret)
		goto exit;

	ret = nvmap_get_handle_from_sci_ipc_id(client, op.flags,
			 op.sci_ipc_id, lclu_vuid, &op.handle);
	if (ret)
		goto exit;

	if (copy_to_user(arg, &op, sizeof(op))) {
		pr_err("copy_to_user failed\n");
		ret = -EINVAL;
	}
exit:
	return ret;
}
#else
int nvmap_ioctl_get_sci_ipc_id(struct file *filp, void __user *arg)
{
	return -EPERM;
}
int nvmap_ioctl_handle_from_sci_ipc_id(struct file *filp, void __user *arg)
{
	return -EPERM;
}
#endif

/*
 * This function calculates allocatable free memory using following formula:
 * free_mem = avail mem - cma free - (avail mem - cma free) / 16
 * The CMA memory is not allocatable by NvMap for regular allocations and it
 * is part of Available memory reported, so subtract it from available memory.
 * NvMap allocates 1/16 extra memory in page coloring, so subtract it as well.
 */
int system_heap_free_mem(unsigned long *mem_val)
{
	long available_mem = 0;
	unsigned long free_mem = 0;
	unsigned long cma_free = 0;

	available_mem = si_mem_available();
	if (available_mem <= 0) {
		*mem_val = 0;
		return 0;
	}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
	cma_free = global_zone_page_state(NR_FREE_CMA_PAGES) << PAGE_SHIFT;
#else
	cma_free = global_page_state(NR_FREE_CMA_PAGES) << PAGE_SHIFT;
#endif
	if ((available_mem << PAGE_SHIFT) < cma_free) {
		*mem_val = 0;
		return 0;
	}
	free_mem = (available_mem << PAGE_SHIFT) - cma_free;
#ifdef NVMAP_CONFIG_COLOR_PAGES
	free_mem = free_mem - (free_mem >> 4);
#endif /* NVMAP_CONFIG_COLOR_PAGES */
	*mem_val = free_mem;
	return 0;
}

static unsigned long system_heap_total_mem(void)
{
	struct sysinfo sys_heap;

	si_meminfo(&sys_heap);

	return sys_heap.totalram << PAGE_SHIFT;
}

int nvmap_ioctl_query_heap_params(struct file *filp, void __user *arg)
{
	unsigned int carveout_mask = NVMAP_HEAP_CARVEOUT_MASK;
	unsigned int iovmm_mask = NVMAP_HEAP_IOVMM;
	struct nvmap_query_heap_params op;
	struct nvmap_heap *heap;
	unsigned int type;
	int ret = 0;
	int i;
	unsigned long free_mem = 0;

	memset(&op, 0, sizeof(op));
	if (copy_from_user(&op, arg, sizeof(op))) {
		ret =  -EFAULT;
		goto exit;
	}

	type = op.heap_mask;
	WARN_ON(type & (type - 1));

	if (nvmap_convert_carveout_to_iovmm) {
		carveout_mask &= ~NVMAP_HEAP_CARVEOUT_GENERIC;
		iovmm_mask |= NVMAP_HEAP_CARVEOUT_GENERIC;
	} else if (nvmap_convert_iovmm_to_carveout) {
		if (type & NVMAP_HEAP_IOVMM) {
			type &= ~NVMAP_HEAP_IOVMM;
			type |= NVMAP_HEAP_CARVEOUT_GENERIC;
		}
	}

	/* To Do: select largest free block */
	op.largest_free_block = PAGE_SIZE;

	if (type & NVMAP_HEAP_CARVEOUT_MASK) {
		for (i = 0; i < nvmap_dev->nr_carveouts; i++) {
			if (type & nvmap_dev->heaps[i].heap_bit) {
				heap = nvmap_dev->heaps[i].carveout;
				op.total = nvmap_query_heap_size(heap);
				op.free = heap->free_size;
				break;
			}
		}
		/* If queried heap is not present */
		if (i >= nvmap_dev->nr_carveouts)
			return -ENODEV;

	} else if (type & iovmm_mask) {
		op.total = system_heap_total_mem();
		ret = system_heap_free_mem(&free_mem);
		if (ret)
			goto exit;
		op.free = free_mem;
	}

	if (copy_to_user(arg, &op, sizeof(op)))
		ret = -EFAULT;
exit:
	return ret;
}

int nvmap_ioctl_dup_handle(struct file *filp, void __user *arg)
{
	struct nvmap_client *client = filp->private_data;
	struct nvmap_handle_ref *ref = NULL;
	struct nvmap_handle *handle = NULL;
	struct nvmap_duplicate_handle op;
	struct dma_buf *dmabuf = NULL;
	int fd = -1, ret = 0;
	u32 id = 0;
	bool is_ro = false;

	if (copy_from_user(&op, arg, sizeof(op)))
		return -EFAULT;

	if (!client)
		return -ENODEV;

	/* Don't allow duplicating RW handle from RO handle */
	if (is_nvmap_id_ro(client, op.handle) &&
	    op.access_flags != NVMAP_HANDLE_RO)
		return -EPERM;

	is_ro = (op.access_flags == NVMAP_HANDLE_RO);
	if (!is_ro)
		ref = nvmap_create_handle_from_id(client, op.handle);
	else
		ref = nvmap_dup_handle_ro(client, op.handle);

	if (!IS_ERR(ref)) {
		dmabuf = is_ro ? ref->handle->dmabuf_ro : ref->handle->dmabuf;
		handle = ref->handle;
		if (client->ida) {
			if (nvmap_id_array_id_alloc(client->ida,
				&id, dmabuf) < 0) {
				if (dmabuf)
					dma_buf_put(dmabuf);
				if (handle)
					nvmap_free_handle(client, handle,
					is_ro);
				return -ENOMEM;
			}
			op.dup_handle = id;

			if (copy_to_user(arg, &op, sizeof(op))) {
				if (dmabuf)
					dma_buf_put(dmabuf);
				if (handle)
					nvmap_free_handle(client, handle,
					is_ro);
				nvmap_id_array_id_release(client->ida, id);
				return -EFAULT;
			}
			ret = 0;
			goto out;
		}
		fd = nvmap_get_dmabuf_fd(client, ref->handle, is_ro);
	} else {
	/* if we get an error, the fd might be non-nvmap dmabuf fd */
		dmabuf = dma_buf_get(op.handle);
		if (IS_ERR(dmabuf))
			return PTR_ERR(dmabuf);
		fd = nvmap_dmabuf_duplicate_gen_fd(client, dmabuf);
		if (fd < 0)
			return PTR_ERR(ref);
	}

	op.dup_handle = fd;

	ret = nvmap_install_fd(client, handle,
				op.dup_handle, arg, &op, sizeof(op), 0, dmabuf);
out:
	if (!ret && !IS_ERR_OR_NULL(handle))
		trace_refcount_dup_handle(handle, dmabuf,
				atomic_read(&handle->ref),
				atomic_long_read(&dmabuf->file->f_count),
				is_ro ? "RO" : "RW");
	return ret;
}
