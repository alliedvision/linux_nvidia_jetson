/*
 * drivers/video/tegra/nvmap/nvmap_cache.c
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

#include <linux/io.h>
#include <linux/debugfs.h>
#include <linux/of.h>
#include <linux/version.h>
#if KERNEL_VERSION(4, 15, 0) > LINUX_VERSION_CODE
#include <soc/tegra/chip-id.h>
#else
#include <soc/tegra/fuse.h>
#endif

#include <linux/sys_soc.h>
#ifdef NVMAP_LOADABLE_MODULE
__weak struct arm64_ftr_reg arm64_ftr_reg_ctrel0;
#endif /*NVMAP_LOADABLE_MODULE */

#include <trace/events/nvmap.h>

#include "nvmap_priv.h"

static struct static_key nvmap_disable_vaddr_for_cache_maint;


/*
 * FIXME:
 *
 *   __clean_dcache_page() is only available on ARM64 (well, we haven't
 *   implemented it on ARMv7).
 */
void nvmap_clean_cache_page(struct page *page)
{
	__clean_dcache_area_poc(page_address(page), PAGE_SIZE);
}

void nvmap_clean_cache(struct page **pages, int numpages)
{
	int i;

	/* Not technically a flush but that's what nvmap knows about. */
	nvmap_stats_inc(NS_CFLUSH_DONE, numpages << PAGE_SHIFT);
	trace_nvmap_cache_flush(numpages << PAGE_SHIFT,
		nvmap_stats_read(NS_ALLOC),
		nvmap_stats_read(NS_CFLUSH_RQ),
		nvmap_stats_read(NS_CFLUSH_DONE));

	for (i = 0; i < numpages; i++)
		nvmap_clean_cache_page(pages[i]);
}

void inner_cache_maint(unsigned int op, void *vaddr, size_t size)
{
	if (op == NVMAP_CACHE_OP_WB_INV)
		__dma_flush_area(vaddr, size);
	else if (op == NVMAP_CACHE_OP_INV)
		__dma_map_area(vaddr, size, DMA_FROM_DEVICE);
	else
		__dma_map_area(vaddr, size, DMA_TO_DEVICE);
}

static void heap_page_cache_maint(
	struct nvmap_handle *h, unsigned long start, unsigned long end,
	unsigned int op, bool inner, bool outer, bool clean_only_dirty)
{
	/* Don't perform cache maint for RO mapped buffers */
	if (h->from_va && h->is_ro)
		return;

	if (h->userflags & NVMAP_HANDLE_CACHE_SYNC) {
		/*
		 * zap user VA->PA mappings so that any access to the pages
		 * will result in a fault and can be marked dirty
		 */
		nvmap_handle_mkclean(h, start, end-start);
		nvmap_zap_handle(h, start, end - start);
	}

	if (static_key_false(&nvmap_disable_vaddr_for_cache_maint))
		goto per_page_cache_maint;

	if (inner) {
		if (!h->vaddr) {
			if (__nvmap_mmap(h))
				__nvmap_munmap(h, h->vaddr);
			else
				goto per_page_cache_maint;
		}
		/* Fast inner cache maintenance using single mapping */
		inner_cache_maint(op, h->vaddr + start, end - start);
		if (!outer)
			return;
		/* Skip per-page inner maintenance in loop below */
		inner = false;

	}
per_page_cache_maint:

	while (start < end) {
		struct page *page;
		phys_addr_t paddr;
		unsigned long next;
		unsigned long off;
		size_t size;
		int ret;

		page = nvmap_to_page(h->pgalloc.pages[start >> PAGE_SHIFT]);
		next = min(((start + PAGE_SIZE) & PAGE_MASK), end);
		off = start & ~PAGE_MASK;
		size = next - start;
		paddr = page_to_phys(page) + off;

		ret = nvmap_cache_maint_phys_range(op, paddr, paddr + size,
				inner, outer);
		WARN_ON(ret != 0);
		start = next;
	}
}

struct cache_maint_op {
	phys_addr_t start;
	phys_addr_t end;
	unsigned int op;
	struct nvmap_handle *h;
	bool inner;
	bool outer;
	bool clean_only_dirty;
};

int nvmap_cache_maint_phys_range(unsigned int op, phys_addr_t pstart,
		phys_addr_t pend, int inner, int outer)
{
	void __iomem *io_addr;
	phys_addr_t loop;

	if (!inner)
		goto do_outer;

	loop = pstart;
	while (loop < pend) {
		phys_addr_t next = (loop + PAGE_SIZE) & PAGE_MASK;
		void *base;
		next = min(next, pend);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 0, 0)
		io_addr = ioremap_prot(loop, PAGE_SIZE, pgprot_val(PAGE_KERNEL));
#else
		io_addr = __ioremap(loop, PAGE_SIZE, PG_PROT_KERNEL);
#endif
		if (io_addr == NULL)
			return -ENOMEM;
		base = (__force void *)io_addr + (loop & ~PAGE_MASK);
		inner_cache_maint(op, base, next - loop);
		iounmap(io_addr);
		loop = next;
	}

do_outer:
	return 0;
}

static int do_cache_maint(struct cache_maint_op *cache_work)
{
	phys_addr_t pstart = cache_work->start;
	phys_addr_t pend = cache_work->end;
	int err = 0;
	struct nvmap_handle *h = cache_work->h;
	unsigned int op = cache_work->op;

	if (!h || !h->alloc)
		return -EFAULT;

	wmb();
	if (h->flags == NVMAP_HANDLE_UNCACHEABLE ||
	    h->flags == NVMAP_HANDLE_WRITE_COMBINE || pstart == pend)
		goto out;

	trace_nvmap_cache_maint(h->owner, h, pstart, pend, op, pend - pstart);
	if (pstart > h->size || pend > h->size) {
		pr_warn("cache maintenance outside handle\n");
		err = -EINVAL;
		goto out;
	}

	if (h->heap_pgalloc) {
		heap_page_cache_maint(h, pstart, pend, op, true,
			(h->flags == NVMAP_HANDLE_INNER_CACHEABLE) ?
			false : true, cache_work->clean_only_dirty);
		goto out;
	}

	pstart += h->carveout->base;
	pend += h->carveout->base;

	err = nvmap_cache_maint_phys_range(op, pstart, pend, true,
			h->flags != NVMAP_HANDLE_INNER_CACHEABLE);

out:
	if (!err) {
		nvmap_stats_inc(NS_CFLUSH_DONE, pend - pstart);
	}

	trace_nvmap_cache_flush(pend - pstart,
		nvmap_stats_read(NS_ALLOC),
		nvmap_stats_read(NS_CFLUSH_RQ),
		nvmap_stats_read(NS_CFLUSH_DONE));

	return 0;
}

__weak void nvmap_handle_get_cacheability(struct nvmap_handle *h,
		bool *inner, bool *outer)
{
	*inner = h->flags == NVMAP_HANDLE_CACHEABLE ||
		 h->flags == NVMAP_HANDLE_INNER_CACHEABLE;
	*outer = h->flags == NVMAP_HANDLE_CACHEABLE;
}

int __nvmap_do_cache_maint(struct nvmap_client *client,
			struct nvmap_handle *h,
			unsigned long start, unsigned long end,
			unsigned int op, bool clean_only_dirty)
{
	int err;
	struct cache_maint_op cache_op;

	h = nvmap_handle_get(h);
	if (!h)
		return -EFAULT;

	if ((start >= h->size) || (end > h->size)) {
		pr_debug("%s start: %ld end: %ld h->size: %zu\n", __func__,
				start, end, h->size);
		nvmap_handle_put(h);
		return -EFAULT;
	}

	if (!(h->heap_type & nvmap_dev->cpu_access_mask)) {
		pr_debug("%s heap_type %u access_mask 0x%x\n", __func__,
				h->heap_type, nvmap_dev->cpu_access_mask);
		nvmap_handle_put(h);
		return -EPERM;
	}

	nvmap_kmaps_inc(h);
	if (op == NVMAP_CACHE_OP_INV)
		op = NVMAP_CACHE_OP_WB_INV;

	/* clean only dirty is applicable only for Write Back operation */
	if (op != NVMAP_CACHE_OP_WB)
		clean_only_dirty = false;

	cache_op.h = h;
	cache_op.start = start ? start : 0;
	cache_op.end = end ? end : h->size;
	cache_op.op = op;
	nvmap_handle_get_cacheability(h, &cache_op.inner, &cache_op.outer);
	cache_op.clean_only_dirty = clean_only_dirty;

	nvmap_stats_inc(NS_CFLUSH_RQ, end - start);
	err = do_cache_maint(&cache_op);
	nvmap_kmaps_dec(h);
	nvmap_handle_put(h);
	return err;
}

int __nvmap_cache_maint(struct nvmap_client *client,
			       struct nvmap_cache_op_64 *op)
{
	struct vm_area_struct *vma;
	struct nvmap_vma_priv *priv;
	struct nvmap_handle *handle;
	unsigned long start;
	unsigned long end;
	int err = 0;

	if (!op->addr || op->op < NVMAP_CACHE_OP_WB ||
	    op->op > NVMAP_CACHE_OP_WB_INV)
		return -EINVAL;

	handle = nvmap_handle_get_from_id(client, op->handle);
	if (IS_ERR_OR_NULL(handle))
		return -EINVAL;

	nvmap_acquire_mmap_read_lock(current->mm);

	vma = find_vma(current->active_mm, (unsigned long)op->addr);
	if (!vma || !is_nvmap_vma(vma) ||
	    (ulong)op->addr < vma->vm_start ||
	    (ulong)op->addr >= vma->vm_end ||
	    op->len > vma->vm_end - (ulong)op->addr) {
		err = -EADDRNOTAVAIL;
		goto out;
	}

	priv = (struct nvmap_vma_priv *)vma->vm_private_data;

	if (priv->handle != handle) {
		err = -EFAULT;
		goto out;
	}

	start = (unsigned long)op->addr - vma->vm_start +
		(vma->vm_pgoff << PAGE_SHIFT);
	end = start + op->len;

	err = __nvmap_do_cache_maint(client, priv->handle, start, end, op->op,
				     false);
out:
	nvmap_release_mmap_read_lock(current->mm);
	nvmap_handle_put(handle);
	return err;
}

/*
 * Perform cache op on the list of memory regions within passed handles.
 * A memory region within handle[i] is identified by offsets[i], sizes[i]
 *
 * sizes[i] == 0  is a special case which causes handle wide operation,
 * this is done by replacing offsets[i] = 0, sizes[i] = handles[i]->size.
 * So, the input arrays sizes, offsets  are not guaranteed to be read-only
 *
 * This will optimze the op if it can.
 * In the case that all the handles together are larger than the inner cache
 * maint threshold it is possible to just do an entire inner cache flush.
 *
 * NOTE: this omits outer cache operations which is fine for ARM64
 */
static int __nvmap_do_cache_maint_list(struct nvmap_handle **handles,
				u64 *offsets, u64 *sizes, int op, u32 nr_ops,
				bool is_32)
{
	u32 i;
	u64 total = 0;
	u64 thresh = ~0;

	WARN(!IS_ENABLED(CONFIG_ARM64),
		"cache list operation may not function properly");

	for (i = 0; i < nr_ops; i++) {
		bool inner, outer;
		u32 *sizes_32 = (u32 *)sizes;
		u64 size = is_32 ? sizes_32[i] : sizes[i];

		nvmap_handle_get_cacheability(handles[i], &inner, &outer);

		if (!inner && !outer)
			continue;

		if ((op == NVMAP_CACHE_OP_WB) && nvmap_handle_track_dirty(handles[i]))
			total += atomic_read(&handles[i]->pgalloc.ndirty);
		else
			total += size ? size : handles[i]->size;
	}

	if (!total)
		return 0;

	/* Full flush in the case the passed list is bigger than our
	 * threshold. */
	if (total >= thresh) {
		for (i = 0; i < nr_ops; i++) {
			if (handles[i]->userflags &
			    NVMAP_HANDLE_CACHE_SYNC) {
				nvmap_handle_mkclean(handles[i], 0,
						     handles[i]->size);
				nvmap_zap_handle(handles[i], 0,
						 handles[i]->size);
			}
		}

		nvmap_stats_inc(NS_CFLUSH_RQ, total);
		nvmap_stats_inc(NS_CFLUSH_DONE, thresh);
		trace_nvmap_cache_flush(total,
					nvmap_stats_read(NS_ALLOC),
					nvmap_stats_read(NS_CFLUSH_RQ),
					nvmap_stats_read(NS_CFLUSH_DONE));
	} else {
		for (i = 0; i < nr_ops; i++) {
			u32 *offs_32 = (u32 *)offsets, *sizes_32 = (u32 *)sizes;
			u64 size = is_32 ? sizes_32[i] : sizes[i];
			u64 offset = is_32 ? offs_32[i] : offsets[i];
			int err;

			size = size ?: handles[i]->size;
			offset = offset ?: 0;
			err = __nvmap_do_cache_maint(handles[i]->owner,
						     handles[i], offset,
						     offset + size,
						     op, false);
			if (err) {
				pr_err("cache maint per handle failed [%d]\n",
						err);
				return err;
			}
		}
	}

	return 0;
}

#if (LINUX_VERSION_CODE > KERNEL_VERSION(4, 9, 0))
static const struct soc_device_attribute tegra194_soc = {
	.soc_id = "TEGRA194",
};

static const struct soc_device_attribute tegra234_soc = {
	.soc_id = "TEGRA234",
};
#endif
inline int nvmap_do_cache_maint_list(struct nvmap_handle **handles,
				u64 *offsets, u64 *sizes, int op, u32 nr_ops,
				bool is_32)
{
	/*
	 * As io-coherency is enabled by default from T194 onwards,
	 * Don't do cache maint from CPU side. The HW, SCF will do.
	 */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0))
	if (!(tegra_get_chip_id() == TEGRA194))
#else
	if (!soc_device_match(&tegra194_soc) &&
		!soc_device_match(&tegra234_soc))
#endif
		return __nvmap_do_cache_maint_list(handles,
				offsets, sizes, op, nr_ops, is_32);
	return 0;
}

int nvmap_cache_debugfs_init(struct dentry *nvmap_root)
{
	struct dentry *cache_root;

	if (!nvmap_root)
		return -ENODEV;

	cache_root = debugfs_create_dir("cache", nvmap_root);
	if (!cache_root)
		return -ENODEV;

	debugfs_create_atomic_t("nvmap_disable_vaddr_for_cache_maint",
				S_IRUSR | S_IWUSR,
				cache_root,
				&nvmap_disable_vaddr_for_cache_maint.enabled);

	return 0;
}
