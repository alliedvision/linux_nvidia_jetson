/*
 * drivers/video/tegra/nvmap/nvmap_core.c
 *
 * Memory manager for Tegra GPU
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

#define pr_fmt(fmt)	"nvmap: %s() " fmt, __func__

#include <linux/err.h>
#include <linux/highmem.h>
#include <linux/io.h>
#include <linux/rbtree.h>
#include <linux/vmalloc.h>
#include <linux/wait.h>
#include <linux/slab.h>
#include <linux/export.h>

#include <asm/pgtable.h>
#include <asm/tlbflush.h>

#include <linux/nvmap.h>
#include <trace/events/nvmap.h>

#include "nvmap_priv.h"

static phys_addr_t handle_phys(struct nvmap_handle *h)
{
	if (h->heap_pgalloc)
		BUG();
	return h->carveout->base;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0)
void *__nvmap_kmap(struct nvmap_handle *h, unsigned int pagenum)
{
	phys_addr_t paddr;
	unsigned long kaddr;
	void __iomem *addr;
	pgprot_t prot;

	if (!virt_addr_valid(h))
		return NULL;

	h = nvmap_handle_get(h);
	if (!h)
		return NULL;
	/*
	 * If the handle is RO and virtual mapping is requested in
	 * kernel address space, return error.
	 */
	if (h->from_va && h->is_ro)
		goto put_handle;

	if (!h->alloc)
		goto put_handle;

	if (!(h->heap_type & nvmap_dev->cpu_access_mask))
		goto put_handle;

	nvmap_kmaps_inc(h);
	if (pagenum >= h->size >> PAGE_SHIFT)
		goto out;

	if (h->vaddr) {
		kaddr = (unsigned long)h->vaddr + pagenum * PAGE_SIZE;
	} else {
		prot = nvmap_pgprot(h, PG_PROT_KERNEL);
		if (h->heap_pgalloc)
			paddr = page_to_phys(nvmap_to_page(
						h->pgalloc.pages[pagenum]));
		else
			paddr = h->carveout->base + pagenum * PAGE_SIZE;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 0, 0)
		addr = ioremap_prot(phys_addr, PAGE_SIZE, pgprot_val(prot));
#else
		addr = __ioremap(paddr, PAGE_SIZE, prot);
#endif
		if (addr == NULL)
			goto out;
		kaddr = (unsigned long)addr;
	}
	return (void *)kaddr;
out:
	nvmap_kmaps_dec(h);
put_handle:
	nvmap_handle_put(h);
	return NULL;
}

void __nvmap_kunmap(struct nvmap_handle *h, unsigned int pagenum,
		  void *addr)
{
	phys_addr_t paddr;

	if (!h || !h->alloc ||
	    WARN_ON(!virt_addr_valid(h)) ||
	    WARN_ON(!addr) ||
	    !(h->heap_type & nvmap_dev->cpu_access_mask))
		return;

	if (WARN_ON(pagenum >= h->size >> PAGE_SHIFT))
		return;

	if (h->vaddr && (h->vaddr == (addr - pagenum * PAGE_SIZE)))
		goto out;

	if (h->heap_pgalloc)
		paddr = page_to_phys(nvmap_to_page(h->pgalloc.pages[pagenum]));
	else
		paddr = h->carveout->base + pagenum * PAGE_SIZE;

	if (h->flags != NVMAP_HANDLE_UNCACHEABLE &&
	    h->flags != NVMAP_HANDLE_WRITE_COMBINE) {
		__dma_flush_area(addr, PAGE_SIZE);
		outer_flush_range(paddr, paddr + PAGE_SIZE); /* FIXME */
	}
	iounmap((void __iomem *)addr);
out:
	nvmap_kmaps_dec(h);
	nvmap_handle_put(h);
}
#endif

void *__nvmap_mmap(struct nvmap_handle *h)
{
	pgprot_t prot;
	void *vaddr;
	unsigned long adj_size;
	struct page **pages = NULL;
	int i = 0;

	if (!virt_addr_valid(h))
		return NULL;

	h = nvmap_handle_get(h);
	if (!h)
		return NULL;
	/*
	 * If the handle is RO and virtual mapping is requested in
	 * kernel address space, return error.
	 */
	if (h->from_va && h->is_ro)
		goto put_handle;


	if (!h->alloc)
		goto put_handle;

	if (!(h->heap_type & nvmap_dev->cpu_access_mask))
		goto put_handle;

	if (h->vaddr)
		return h->vaddr;

	nvmap_kmaps_inc(h);
	prot = nvmap_pgprot(h, PG_PROT_KERNEL);

	if (h->heap_pgalloc) {
		pages = nvmap_pages(h->pgalloc.pages, h->size >> PAGE_SHIFT);
		if (!pages)
			goto out;

		vaddr = vmap(pages, h->size >> PAGE_SHIFT, VM_MAP, prot);
		if (!vaddr && !h->vaddr)
			goto out;
		nvmap_altfree(pages, (h->size >> PAGE_SHIFT) * sizeof(*pages));

		if (vaddr && atomic_long_cmpxchg((atomic_long_t *)&h->vaddr, 0, (long)vaddr)) {
			nvmap_kmaps_dec(h);
			vunmap(vaddr);
		}
		return h->vaddr;
	}

	/* carveout - explicitly map the pfns into a vmalloc area */
	adj_size = h->carveout->base & ~PAGE_MASK;
	adj_size += h->size;
	adj_size = PAGE_ALIGN(adj_size);

	if (pfn_valid(__phys_to_pfn(h->carveout->base & PAGE_MASK))) {
		unsigned long pfn;
		struct page *page;
		int nr_pages;

		pfn = ((h->carveout->base) >> PAGE_SHIFT);
		page = pfn_to_page(pfn);
		nr_pages = h->size >> PAGE_SHIFT;

		pages = vmalloc(nr_pages * sizeof(*pages));
		if (!pages)
			goto out;

		for (i = 0; i < nr_pages; i++)
			pages[i] = nth_page(page, i);

		vaddr = vmap(pages, nr_pages, VM_MAP, prot);
	} else {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 0, 0)
		vaddr = ioremap_prot(h->carveout->base, adj_size, pgprot_val(prot));
#else
		vaddr = (__force void *)__ioremap(h->carveout->base, adj_size,
			 prot);
#endif
	}
	if (vaddr == NULL)
		goto out;

	if (vaddr && atomic_long_cmpxchg((atomic_long_t *)&h->vaddr,
						 0, (long)vaddr)) {
		vaddr -= (h->carveout->base & ~PAGE_MASK);
		/*
		 * iounmap calls vunmap for vmalloced address, hence
		 * takes care of vmap/__ioremap freeing part.
		 */
		iounmap((void __iomem *)vaddr);
		nvmap_kmaps_dec(h);
	}

	if (pages)
		vfree(pages);

	/* leave the handle ref count incremented by 1, so that
	 * the handle will not be freed while the kernel mapping exists.
	 * nvmap_handle_put will be called by unmapping this address */
	return h->vaddr;
out:
	if (pages)
		vfree(pages);
	nvmap_kmaps_dec(h);
put_handle:
	nvmap_handle_put(h);
	return NULL;
}

void __nvmap_munmap(struct nvmap_handle *h, void *addr)
{
	if (!h || !h->alloc ||
	    WARN_ON(!virt_addr_valid(h)) ||
	    WARN_ON(!addr) ||
	    !(h->heap_type & nvmap_dev->cpu_access_mask))
		return;

	nvmap_handle_put(h);
}

/*
 * NOTE: this does not ensure the continued existence of the underlying
 * dma_buf. If you want ensure the existence of the dma_buf you must get an
 * nvmap_handle_ref as that is what tracks the dma_buf refs.
 */
struct nvmap_handle *nvmap_handle_get(struct nvmap_handle *h)
{
	int cnt;

	if (WARN_ON(!virt_addr_valid(h))) {
		pr_err("%s: invalid handle\n", current->group_leader->comm);
		return NULL;
	}

	cnt = atomic_inc_return(&h->ref);
	NVMAP_TAG_TRACE(trace_nvmap_handle_get, h, cnt);

	if (unlikely(cnt <= 1)) {
		pr_err("%s: %s attempt to get a freed handle\n",
			__func__, current->group_leader->comm);
		atomic_dec(&h->ref);
		return NULL;
	}

	return h;
}

void nvmap_handle_put(struct nvmap_handle *h)
{
	int cnt;

	if (WARN_ON(!virt_addr_valid(h)))
		return;
	cnt = atomic_dec_return(&h->ref);
	NVMAP_TAG_TRACE(trace_nvmap_handle_put, h, cnt);

	if (WARN_ON(cnt < 0)) {
		pr_err("%s: %s put to negative references\n",
			__func__, current->comm);
	} else if (cnt == 0)
		_nvmap_handle_free(h);
}

struct sg_table *__nvmap_sg_table(struct nvmap_client *client,
		struct nvmap_handle *h)
{
	struct sg_table *sgt = NULL;
	int err, npages;
	struct page **pages;

	if (!virt_addr_valid(h))
		return ERR_PTR(-EINVAL);

	h = nvmap_handle_get(h);
	if (!h)
		return ERR_PTR(-EINVAL);

	if (!h->alloc) {
		err = -EINVAL;
		goto put_handle;
	}

	npages = PAGE_ALIGN(h->size) >> PAGE_SHIFT;
	sgt = kzalloc(sizeof(*sgt), GFP_KERNEL);
	if (!sgt) {
		err = -ENOMEM;
		goto err;
	}

	if (!h->heap_pgalloc) {
		phys_addr_t paddr = handle_phys(h);
		struct page *page = phys_to_page(paddr);

		err = sg_alloc_table(sgt, 1, GFP_KERNEL);
		if (err)
			goto err;

		sg_set_page(sgt->sgl, page, h->size, offset_in_page(paddr));
	} else {
		pages = nvmap_pages(h->pgalloc.pages, npages);
		if (!pages) {
			err = -ENOMEM;
			goto err;
		}
		err = sg_alloc_table_from_pages(sgt, pages,
				npages, 0, h->size, GFP_KERNEL);
		nvmap_altfree(pages, npages * sizeof(*pages));
		if (err)
			goto err;
	}
	nvmap_handle_put(h);
	return sgt;

err:
	kfree(sgt);
put_handle:
	nvmap_handle_put(h);
	return ERR_PTR(err);
}

void __nvmap_free_sg_table(struct nvmap_client *client,
		struct nvmap_handle *h, struct sg_table *sgt)
{
	sg_free_table(sgt);
	kfree(sgt);
}
