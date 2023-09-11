/*
 * dma_buf exporter for nvmap
 *
 * Copyright (c) 2012-2022, NVIDIA CORPORATION.  All rights reserved.
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

#include <linux/list.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/export.h>
#include <linux/nvmap.h>
#include <linux/dma-buf.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/atomic.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/stringify.h>
#include <linux/of.h>
#include <linux/version.h>
#include <linux/iommu.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 18, 0)
#include <linux/iosys-map.h>
#endif

#include <trace/events/nvmap.h>

#include "nvmap_priv.h"
#include "nvmap_ioctl.h"

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0)
#define NVMAP_DMABUF_ATTACH  nvmap_dmabuf_attach
#else
#define NVMAP_DMABUF_ATTACH  __nvmap_dmabuf_attach
#endif

static int __nvmap_dmabuf_attach(struct dma_buf *dmabuf, struct device *dev,
			       struct dma_buf_attachment *attach)
{
	struct nvmap_handle_info *info = dmabuf->priv;

	trace_nvmap_dmabuf_attach(dmabuf, dev);

	dev_dbg(dev, "%s() 0x%p\n", __func__, info->handle);
	return 0;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0)
static int nvmap_dmabuf_attach(struct dma_buf *dmabuf,
			       struct dma_buf_attachment *attach)
{
	return __nvmap_dmabuf_attach(dmabuf, attach->dev, attach);
}
#endif

static void nvmap_dmabuf_detach(struct dma_buf *dmabuf,
				struct dma_buf_attachment *attach)
{
	struct nvmap_handle_info *info = dmabuf->priv;

	trace_nvmap_dmabuf_detach(dmabuf, attach->dev);

	dev_dbg(attach->dev, "%s() 0x%p\n", __func__, info->handle);
}

static inline bool access_vpr_phys(struct device *dev)
{
	if (!iommu_get_domain_for_dev(dev))
		return true;

	/*
	 * Assumes gpu nodes always have DT entry, this is valid as device
	 * specifying access-vpr-phys will do so through its DT entry.
	 */
	if (!dev->of_node)
		return false;

	return !!of_find_property(dev->of_node, "access-vpr-phys", NULL);
}

struct sg_table *_nvmap_dmabuf_map_dma_buf(
	struct dma_buf_attachment *attach, enum dma_data_direction dir)
{
	struct nvmap_handle_info *info = attach->dmabuf->priv;
	int ents = 0;
	struct sg_table *sgt;
#ifdef NVMAP_CONFIG_DEBUG_MAPS
	char *device_name = NULL;
	u32 heap_type;
	u64 dma_mask;
#endif /* NVMAP_CONFIG_DEBUG_MAPS */
	DEFINE_DMA_ATTRS(attrs);

	trace_nvmap_dmabuf_map_dma_buf(attach->dmabuf, attach->dev);

	/*
	 * If the exported buffer is foreign buffer(alloc_from_va) and
	 * has RO access, don't map it in device space.
	 * Return error as no access.
	 */
	if (info->handle->from_va && info->handle->is_ro &&
		(dir != DMA_TO_DEVICE))
		return ERR_PTR(-EACCES);

	nvmap_lru_reset(info->handle);
	mutex_lock(&info->maps_lock);

	atomic_inc(&info->handle->pin);

	sgt = __nvmap_sg_table(NULL, info->handle);
	if (IS_ERR(sgt)) {
		atomic_dec(&info->handle->pin);
		mutex_unlock(&info->maps_lock);
		return sgt;
	}

	if (!info->handle->alloc) {
		goto err_map;
	} else if (!(nvmap_dev->dynamic_dma_map_mask &
			info->handle->heap_type)) {
		sg_dma_address(sgt->sgl) = info->handle->carveout->base;
	} else if (info->handle->heap_type == NVMAP_HEAP_CARVEOUT_VPR &&
			access_vpr_phys(attach->dev)) {
		sg_dma_address(sgt->sgl) = 0;
	} else {
		dma_set_attr(DMA_ATTR_SKIP_CPU_SYNC, __DMA_ATTR(attrs));
		ents = dma_map_sg_attrs(attach->dev, sgt->sgl,
					sgt->nents, dir, __DMA_ATTR(attrs));
		if (ents <= 0)
			goto err_map;
	}

	attach->priv = sgt;

#ifdef NVMAP_CONFIG_DEBUG_MAPS
	/* Insert device name into the carveout's device name rb tree */
	heap_type = info->handle->heap_type;
	device_name = (char *)dev_name(attach->dev);
	dma_mask = *(attach->dev->dma_mask);
	if (device_name && !nvmap_is_device_present(device_name, heap_type)) {
		/* If the device name is not already present in the tree, then only add */
		nvmap_add_device_name(device_name, dma_mask, heap_type);
	}
#endif /* NVMAP_CONFIG_DEBUG_MAPS */
	mutex_unlock(&info->maps_lock);
	return sgt;

err_map:
	__nvmap_free_sg_table(NULL, info->handle, sgt);
	atomic_dec(&info->handle->pin);
	mutex_unlock(&info->maps_lock);
	return ERR_PTR(-ENOMEM);
}

__weak struct sg_table *nvmap_dmabuf_map_dma_buf(
	struct dma_buf_attachment *attach, enum dma_data_direction dir)
{
	return _nvmap_dmabuf_map_dma_buf(attach, dir);
}

void _nvmap_dmabuf_unmap_dma_buf(struct dma_buf_attachment *attach,
				       struct sg_table *sgt,
				       enum dma_data_direction dir)
{
	struct nvmap_handle_info *info = attach->dmabuf->priv;
#ifdef NVMAP_CONFIG_DEBUG_MAPS
	char *device_name = NULL;
	u32 heap_type = 0;
#endif /* NVMAP_CONFIG_DEBUG_MAPS */

	trace_nvmap_dmabuf_unmap_dma_buf(attach->dmabuf, attach->dev);

	mutex_lock(&info->maps_lock);
	if (!atomic_add_unless(&info->handle->pin, -1, 0)) {
		mutex_unlock(&info->maps_lock);
		WARN(1, "Unpinning handle that has yet to be pinned!\n");
		return;
	}

	if (!(nvmap_dev->dynamic_dma_map_mask & info->handle->heap_type)) {
		sg_dma_address(sgt->sgl) = 0;
	} else if (info->handle->heap_type == NVMAP_HEAP_CARVEOUT_VPR &&
			access_vpr_phys(attach->dev)) {
		sg_dma_address(sgt->sgl) = 0;
	} else {
		dma_unmap_sg_attrs(attach->dev,
				   sgt->sgl, sgt->nents,
				   dir, DMA_ATTR_SKIP_CPU_SYNC);
	}
	__nvmap_free_sg_table(NULL, info->handle, sgt);

#ifdef NVMAP_CONFIG_DEBUG_MAPS
	/* Remove the device name from the list of carveout accessing devices */
	heap_type = info->handle->heap_type;
	device_name = (char *)dev_name(attach->dev);
	if (device_name)
		nvmap_remove_device_name(device_name, heap_type);
#endif /* NVMAP_CONFIG_DEBUG_MAPS */
	mutex_unlock(&info->maps_lock);
}

__weak void nvmap_dmabuf_unmap_dma_buf(struct dma_buf_attachment *attach,
				       struct sg_table *sgt,
				       enum dma_data_direction dir)
{
	_nvmap_dmabuf_unmap_dma_buf(attach, sgt, dir);
}

static void nvmap_dmabuf_release(struct dma_buf *dmabuf)
{
	struct nvmap_handle_info *info = dmabuf->priv;

	trace_nvmap_dmabuf_release(info->handle->owner ?
				   info->handle->owner->name : "unknown",
				   info->handle,
				   dmabuf);

	mutex_lock(&info->handle->lock);
	if (info->is_ro) {
		BUG_ON(dmabuf != info->handle->dmabuf_ro);
		info->handle->dmabuf_ro = NULL;
	} else {
		BUG_ON(dmabuf != info->handle->dmabuf);
		info->handle->dmabuf = NULL;
	}
	mutex_unlock(&info->handle->lock);

	nvmap_handle_put(info->handle);
	kfree(info);
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0)
static int __nvmap_dmabuf_end_cpu_access(struct dma_buf *dmabuf,
				       enum dma_data_direction dir)
{
	struct nvmap_handle_info *info = dmabuf->priv;

	trace_nvmap_dmabuf_end_cpu_access(dmabuf, 0, dmabuf->size);
	return __nvmap_do_cache_maint(NULL, info->handle,
				   0, dmabuf->size,
				   NVMAP_CACHE_OP_WB, false);
}

static int __nvmap_dmabuf_begin_cpu_access(struct dma_buf *dmabuf,
					  enum dma_data_direction dir)
{
	struct nvmap_handle_info *info = dmabuf->priv;

	trace_nvmap_dmabuf_begin_cpu_access(dmabuf, 0, dmabuf->size);
	return __nvmap_do_cache_maint(NULL, info->handle, 0, dmabuf->size,
				      NVMAP_CACHE_OP_WB_INV, false);
}
#define NVMAP_DMABUF_BEGIN_CPU_ACCESS           __nvmap_dmabuf_begin_cpu_access
#define NVMAP_DMABUF_END_CPU_ACCESS 		__nvmap_dmabuf_end_cpu_access
#else
static int nvmap_dmabuf_begin_cpu_access(struct dma_buf *dmabuf,
					  size_t start, size_t len,
					  enum dma_data_direction dir)
{
	struct nvmap_handle_info *info = dmabuf->priv;

	trace_nvmap_dmabuf_begin_cpu_access(dmabuf, start, len);
	return __nvmap_do_cache_maint(NULL, info->handle, start, start + len,
				      NVMAP_CACHE_OP_WB_INV, false);
}

static void nvmap_dmabuf_end_cpu_access(struct dma_buf *dmabuf,
				       size_t start, size_t len,
				       enum dma_data_direction dir)
{
	struct nvmap_handle_info *info = dmabuf->priv;

	trace_nvmap_dmabuf_end_cpu_access(dmabuf, start, len);
	__nvmap_do_cache_maint(NULL, info->handle,
				   start, start + len,
				   NVMAP_CACHE_OP_WB, false);
}
#define NVMAP_DMABUF_BEGIN_CPU_ACCESS           nvmap_dmabuf_begin_cpu_access
#define NVMAP_DMABUF_END_CPU_ACCESS 		nvmap_dmabuf_end_cpu_access
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0)
static void *nvmap_dmabuf_kmap(struct dma_buf *dmabuf, unsigned long page_num)
{
	struct nvmap_handle_info *info = dmabuf->priv;

	trace_nvmap_dmabuf_kmap(dmabuf);
	return __nvmap_kmap(info->handle, page_num);
}

static void nvmap_dmabuf_kunmap(struct dma_buf *dmabuf,
		unsigned long page_num, void *addr)
{
	struct nvmap_handle_info *info = dmabuf->priv;

	trace_nvmap_dmabuf_kunmap(dmabuf);
	__nvmap_kunmap(info->handle, page_num, addr);
}

static void *nvmap_dmabuf_kmap_atomic(struct dma_buf *dmabuf,
				      unsigned long page_num)
{
	WARN(1, "%s() can't be called from atomic\n", __func__);
	return NULL;
}
#endif

int __nvmap_map(struct nvmap_handle *h, struct vm_area_struct *vma)
{
	struct nvmap_vma_priv *priv;

	h = nvmap_handle_get(h);
	if (!h)
		return -EINVAL;

	if (!(h->heap_type & nvmap_dev->cpu_access_mask)) {
		nvmap_handle_put(h);
		return -EPERM;
	}

	/*
	 * If the handle is RO and RW mapping is requested, then
	 * return error.
	 */
	if (h->from_va && h->is_ro && (vma->vm_flags & VM_WRITE)) {
		nvmap_handle_put(h);
		return -EPERM;
	}
	/*
	 * Don't allow mmap on VPR memory as it would be mapped
	 * as device memory. User space shouldn't be accessing
	 * device memory.
	 */
	if (h->heap_type == NVMAP_HEAP_CARVEOUT_VPR)  {
		nvmap_handle_put(h);
		return -EPERM;
	}

	priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv) {
		nvmap_handle_put(h);
		return -ENOMEM;
	}
	priv->handle = h;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 3, 0)
	vm_flags_set(vma, VM_SHARED | VM_DONTEXPAND |
			  VM_DONTDUMP | VM_DONTCOPY |
			  (h->heap_pgalloc ? 0 : VM_PFNMAP));
#else
	vma->vm_flags |= VM_SHARED | VM_DONTEXPAND |
			  VM_DONTDUMP | VM_DONTCOPY |
			  (h->heap_pgalloc ? 0 : VM_PFNMAP);
#endif
	vma->vm_ops = &nvmap_vma_ops;
	BUG_ON(vma->vm_private_data != NULL);
	vma->vm_private_data = priv;
	vma->vm_page_prot = nvmap_pgprot(h, vma->vm_page_prot);
	nvmap_vma_open(vma);
	return 0;
}

static int nvmap_dmabuf_mmap(struct dma_buf *dmabuf, struct vm_area_struct *vma)
{
	struct nvmap_handle_info *info = dmabuf->priv;

	trace_nvmap_dmabuf_mmap(dmabuf);

	return __nvmap_map(info->handle, vma);
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 11, 0)
static void *nvmap_dmabuf_vmap(struct dma_buf *dmabuf)
{
	struct nvmap_handle_info *info = dmabuf->priv;

	trace_nvmap_dmabuf_vmap(dmabuf);

	/* Don't allow vmap on RO buffers */
	if (info->is_ro)
		return ERR_PTR(-EPERM);

	return __nvmap_mmap(info->handle);
}

static void nvmap_dmabuf_vunmap(struct dma_buf *dmabuf, void *vaddr)
{
	struct nvmap_handle_info *info = dmabuf->priv;

	trace_nvmap_dmabuf_vunmap(dmabuf);
	__nvmap_munmap(info->handle, vaddr);
}
#else
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 18, 0)
static int nvmap_dmabuf_vmap(struct dma_buf *dmabuf, struct iosys_map *map)
#else
static int nvmap_dmabuf_vmap(struct dma_buf *dmabuf, struct dma_buf_map *map)
#endif
{
	struct nvmap_handle_info *info = dmabuf->priv;
	void *res;
	int ret = 0;

	trace_nvmap_dmabuf_vmap(dmabuf);

	/* Don't allow vmap on RO buffers */
	if (info->is_ro)
		return -EPERM;

	res = __nvmap_mmap(info->handle);
	if (res != NULL) {
		map->vaddr = res;
		map->is_iomem = false;
	}
	else {
		ret = -ENOMEM;
	}
	return ret;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 18, 0)
static void nvmap_dmabuf_vunmap(struct dma_buf *dmabuf, struct iosys_map *map)
#else
static void nvmap_dmabuf_vunmap(struct dma_buf *dmabuf, struct dma_buf_map *map)
#endif
{
       struct nvmap_handle_info *info = dmabuf->priv;

       trace_nvmap_dmabuf_vunmap(dmabuf);
       __nvmap_munmap(info->handle, info->handle->vaddr);
}
#endif

int nvmap_dmabuf_set_drv_data(struct dma_buf *dmabuf,
		struct device *dev, void *priv, void (*delete)(void *priv))
{
	struct nvmap_handle_info *info = dmabuf->priv;
	struct nvmap_handle *handle = info->handle;
	struct nvmap_handle_dmabuf_priv *curr = NULL;
	int ret = 0;

	mutex_lock(&handle->lock);
	list_for_each_entry(curr, &handle->dmabuf_priv, list) {
		if (curr->dev == dev) {
			ret = -EEXIST;
			goto unlock;
		}
	}

	curr = kmalloc(sizeof(*curr), GFP_KERNEL);
	if (!curr) {
		ret = -ENOMEM;
		goto unlock;
	}
	curr->priv = priv;
	curr->dev = dev;
	curr->priv_release = delete;
	list_add_tail(&curr->list, &handle->dmabuf_priv);
unlock:
	mutex_unlock(&handle->lock);
	return ret;
}

void *nvmap_dmabuf_get_drv_data(struct dma_buf *dmabuf,
		struct device *dev)
{
	struct nvmap_handle_dmabuf_priv *curr = NULL;
	struct nvmap_handle_info *info;
	struct nvmap_handle *handle;
	void *priv = NULL;

	if (dmabuf && dmabuf->priv)
		info = dmabuf->priv;
	else
		return NULL;

	if (info && info->handle)
		handle = info->handle;
	else
		return NULL;

	mutex_lock(&handle->lock);
	list_for_each_entry(curr, &handle->dmabuf_priv, list) {
		if (curr->dev == dev) {
			priv = curr->priv;
			goto unlock;
		}
	}
unlock:
	mutex_unlock(&handle->lock);
	return priv;
}

static struct dma_buf_ops nvmap_dma_buf_ops = {
	.attach		= NVMAP_DMABUF_ATTACH,
	.detach		= nvmap_dmabuf_detach,
	.map_dma_buf	= nvmap_dmabuf_map_dma_buf,
	.unmap_dma_buf	= nvmap_dmabuf_unmap_dma_buf,
	.release	= nvmap_dmabuf_release,
	.begin_cpu_access = NVMAP_DMABUF_BEGIN_CPU_ACCESS,
	.end_cpu_access = NVMAP_DMABUF_END_CPU_ACCESS,
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0)
	.kmap_atomic	= nvmap_dmabuf_kmap_atomic,
	.kmap		= nvmap_dmabuf_kmap,
	.kunmap		= nvmap_dmabuf_kunmap,
#elif LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0)
	.map_atomic	= nvmap_dmabuf_kmap_atomic,
	.map		= nvmap_dmabuf_kmap,
	.unmap		= nvmap_dmabuf_kunmap,
#endif
	.mmap		= nvmap_dmabuf_mmap,
	.vmap		= nvmap_dmabuf_vmap,
	.vunmap		= nvmap_dmabuf_vunmap,
#if LINUX_VERSION_CODE > KERNEL_VERSION(5, 4, 0)
	.cache_sgt_mapping = true,
#endif

};

static char dmabuf_name[] = "nvmap_dmabuf";

bool dmabuf_is_nvmap(struct dma_buf *dmabuf)
{
	return dmabuf->exp_name == dmabuf_name;
}
EXPORT_SYMBOL(dmabuf_is_nvmap);

static struct dma_buf *__dma_buf_export(struct nvmap_handle_info *info,
					size_t size, bool ro_buf)
{
	DEFINE_DMA_BUF_EXPORT_INFO(exp_info);

	exp_info.priv = info;
	exp_info.ops = &nvmap_dma_buf_ops;
	exp_info.size = size;

	if (ro_buf) {
		exp_info.flags = O_RDONLY;
	} else {
		exp_info.flags = O_RDWR;
	}

#ifndef NVMAP_UPSTREAM_KERNEL
	/* Disable defer unmap feature only for kstable */
	exp_info.exp_flags = DMABUF_CAN_DEFER_UNMAP |
				DMABUF_SKIP_CACHE_SYNC;
#endif /* !NVMAP_UPSTREAM_KERNEL */
	exp_info.exp_name = dmabuf_name;

	return dma_buf_export(&exp_info);
}

/*
 * Make a dmabuf object for an nvmap handle.
 */
struct dma_buf *__nvmap_make_dmabuf(struct nvmap_client *client,
				    struct nvmap_handle *handle, bool ro_buf)
{
	int err;
	struct dma_buf *dmabuf;
	struct nvmap_handle_info *info;

	info = kzalloc(sizeof(*info), GFP_KERNEL);
	if (!info) {
		err = -ENOMEM;
		goto err_nomem;
	}
	info->handle = handle;
	info->is_ro = ro_buf;
	INIT_LIST_HEAD(&info->maps);
	mutex_init(&info->maps_lock);

	dmabuf = __dma_buf_export(info, handle->size, ro_buf);
	if (IS_ERR(dmabuf)) {
		err = PTR_ERR(dmabuf);
		goto err_export;
	}
	nvmap_handle_get(handle);

	trace_nvmap_make_dmabuf(client->name, handle, dmabuf);
	return dmabuf;

err_export:
	kfree(info);
err_nomem:
	return ERR_PTR(err);
}

int __nvmap_dmabuf_fd(struct nvmap_client *client,
		      struct dma_buf *dmabuf, int flags)
{
#if !defined(NVMAP_CONFIG_HANDLE_AS_ID) && !defined(NVMAP_LOADABLE_MODULE)
	int start_fd = NVMAP_CONFIG_FD_START;
#endif
	int ret;

#ifdef NVMAP_CONFIG_DEFER_FD_RECYCLE
	if (client->next_fd < NVMAP_CONFIG_FD_START)
		client->next_fd = NVMAP_CONFIG_FD_START;
	start_fd = client->next_fd++;
	if (client->next_fd >= NVMAP_CONFIG_DEFER_FD_RECYCLE_MAX_FD)
		client->next_fd = NVMAP_CONFIG_FD_START;
#endif
	if (!dmabuf || !dmabuf->file)
		return -EINVAL;
	/* Allocate fd from start_fd(>=1024) onwards to overcome
	 * __FD_SETSIZE limitation issue for select(),
	 * pselect() syscalls.
	 */
#if defined(NVMAP_LOADABLE_MODULE) || defined(NVMAP_CONFIG_HANDLE_AS_ID)
	ret = get_unused_fd_flags(flags);
#else
	ret =  __alloc_fd(current->files, start_fd, sysctl_nr_open, flags);
#endif
	if (ret == -EMFILE)
		pr_err_ratelimited("NvMap: FD limit is crossed for uid %d\n",
				   from_kuid(current_user_ns(), current_uid()));
	return ret;
}

static struct dma_buf *__nvmap_dmabuf_export(struct nvmap_client *client,
				 struct nvmap_handle *handle, bool is_ro)
{
	struct dma_buf *buf;

	handle = nvmap_handle_get(handle);
	if (!handle)
		return ERR_PTR(-EINVAL);
	if (is_ro)
		buf = handle->dmabuf_ro;
	else
		buf = handle->dmabuf;

	if (WARN(!buf, "Attempting to get a freed dma_buf!\n")) {
		nvmap_handle_put(handle);
		return NULL;
	}

	get_dma_buf(buf);

	/*
	 * Don't want to take out refs on the handle here.
	 */
	nvmap_handle_put(handle);

	return buf;
}

int nvmap_get_dmabuf_fd(struct nvmap_client *client, struct nvmap_handle *h,
			bool is_ro)
{
	int fd;
	struct dma_buf *dmabuf;

	dmabuf = __nvmap_dmabuf_export(client, h, is_ro);
	if (IS_ERR(dmabuf))
		return PTR_ERR(dmabuf);

	fd = __nvmap_dmabuf_fd(client, dmabuf, O_CLOEXEC);
	if (IS_ERR_VALUE((uintptr_t)fd))
		dma_buf_put(dmabuf);
	return fd;
}

/*
 * Returns the nvmap handle ID associated with the passed dma_buf's fd. This
 * does not affect the ref count of the dma_buf.
 * NOTE: Callers of this utility function must invoke nvmap_handle_put after
 * using the returned nvmap_handle. Call to nvmap_handle_get is required in
 * this utility function to avoid race conditions in code where nvmap_handle
 * returned by this function is freed concurrently while the caller is still
 * using it.
 */
struct nvmap_handle *nvmap_handle_get_from_dmabuf_fd(
					struct nvmap_client *client, int fd)
{
	struct nvmap_handle *handle = ERR_PTR(-EINVAL);
	struct dma_buf *dmabuf;
	struct nvmap_handle_info *info;

	dmabuf = dma_buf_get(fd);
	if (IS_ERR(dmabuf))
		return ERR_CAST(dmabuf);
	if (dmabuf_is_nvmap(dmabuf)) {
		info = dmabuf->priv;
		handle = info->handle;
		if (!nvmap_handle_get(handle))
			handle = ERR_PTR(-EINVAL);
	}
	dma_buf_put(dmabuf);
	return handle;
}

bool is_nvmap_dmabuf_fd_ro(int fd)
{
	struct dma_buf *dmabuf;
	struct nvmap_handle_info *info = NULL;

	dmabuf = dma_buf_get(fd);
	if (IS_ERR(dmabuf)) {
		return false;
	}
	if (dmabuf_is_nvmap(dmabuf)) {
		info = dmabuf->priv;
	}
	dma_buf_put(dmabuf);

	return (info != NULL) ? info->is_ro : false;
}

/*
 * Duplicates a generic dma_buf fd. nvmap dma_buf fd has to be duplicated
 * using existing code paths to preserve memory accounting behavior, so this
 * function returns -EINVAL on dma_buf fds created by nvmap.
 */
int nvmap_dmabuf_duplicate_gen_fd(struct nvmap_client *client,
		struct dma_buf *dmabuf)
{
	int ret = 0;

	if (dmabuf_is_nvmap(dmabuf)) {
		ret = -EINVAL;
		goto error;
	}

	ret = __nvmap_dmabuf_fd(client, dmabuf, O_CLOEXEC);
	if (ret < 0)
		goto error;

	return ret;

error:
	dma_buf_put(dmabuf);
	return ret;
}
