/*
 * drivers/video/tegra/nvmap/nvmap_mm.c
 *
 * Some MM related functionality specific to nvmap.
 *
 * Copyright (c) 2013-2021, NVIDIA CORPORATION. All rights reserved.
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

#include <trace/events/nvmap.h>
#include <linux/version.h>

#include <asm/pgtable.h>

#include "nvmap_priv.h"


#ifndef NVMAP_LOADABLE_MODULE
void nvmap_zap_handle(struct nvmap_handle *handle, u64 offset, u64 size)
{
	struct list_head *vmas;
	struct nvmap_vma_list *vma_list;
	struct vm_area_struct *vma;

	if (!handle->heap_pgalloc)
		return;

	/* if no dirty page is present, no need to zap */
	if (nvmap_handle_track_dirty(handle) && !atomic_read(&handle->pgalloc.ndirty))
		return;

	if (!size) {
		offset = 0;
		size = handle->size;
	}

	size = PAGE_ALIGN((offset & ~PAGE_MASK) + size);

	mutex_lock(&handle->lock);
	vmas = &handle->vmas;
	list_for_each_entry(vma_list, vmas, list) {
		struct nvmap_vma_priv *priv;
		size_t vm_size = size;

		vma = vma_list->vma;
		priv = vma->vm_private_data;
		if ((offset + size) > (vma->vm_end - vma->vm_start))
			vm_size = vma->vm_end - vma->vm_start - offset;

		if (priv->offs || vma->vm_pgoff)
			/* vma mapping starts in the middle of handle memory.
			 * zapping needs special care. zap entire range for now.
			 * FIXME: optimze zapping.
			 */
			zap_page_range(vma, vma->vm_start,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
				vma->vm_end - vma->vm_start);
#else
				vma->vm_end - vma->vm_start, NULL);
#endif
		else
			zap_page_range(vma, vma->vm_start + offset,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
				vm_size);
#else
				vm_size, NULL);
#endif
	}
	mutex_unlock(&handle->lock);
}
#else
void nvmap_zap_handle(struct nvmap_handle *handle, u64 offset, u64 size)
{
	pr_debug("%s is not supported!\n", __func__);
}
#endif /* !NVMAP_LOADABLE_MODULE */
