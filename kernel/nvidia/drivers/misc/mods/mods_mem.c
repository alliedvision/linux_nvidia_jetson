// SPDX-License-Identifier: GPL-2.0
/*
 * This file is part of NVIDIA MODS kernel driver.
 *
 * Copyright (c) 2008-2022, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA MODS kernel driver is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * NVIDIA MODS kernel driver is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NVIDIA MODS kernel driver.
 * If not, see <http://www.gnu.org/licenses/>.
 */

#include "mods_internal.h"

#include <linux/pagemap.h>

#if defined(MODS_HAS_SET_DMA_MASK)
#include <linux/dma-mapping.h>
#include <linux/of.h>
#endif

#ifdef CONFIG_ARM64
#include <linux/cache.h>
#endif

static int mods_post_alloc(struct mods_client     *client,
			   struct MODS_PHYS_CHUNK *chunk,
			   u64                     phys_addr,
			   struct MODS_MEM_INFO   *p_mem_info);

#if defined(MODS_HAS_TEGRA)
static int mods_smmu_unmap_memory(struct mods_client   *client,
				  struct MODS_MEM_INFO *p_mem_info);
#endif

/****************************
 * DMA MAP HELPER FUNCTIONS *
 ****************************/

/*
 * Starting on Power9 systems, DMA addresses for NVLink are no longer
 * the same as used over PCIE.
 *
 * Power9 supports a 56-bit Real Address. This address range is compressed
 * when accessed over NvLink to allow the GPU to access all of memory using
 * its 47-bit Physical address.
 *
 * If there is an NPU device present on the system, it implies that NvLink
 * sysmem links are present and we need to apply the required address
 * conversion for NvLink within the driver. This is intended to be temporary
 * to ease the transition to kernel APIs to handle NvLink DMA mappings
 * via the NPU device.
 *
 * Note, a deviation from the documented compression scheme is that the
 * upper address bits (i.e. bit 56-63) instead of being set to zero are
 * preserved during NvLink address compression so the orignal PCIE DMA
 * address can be reconstructed on expansion. These bits can be safely
 * ignored on NvLink since they are truncated by the GPU.
 */
#if defined(CONFIG_PPC64) && defined(CONFIG_PCI)
static u64 mods_compress_nvlink_addr(struct pci_dev *dev, u64 addr)
{
	u64 addr47 = addr;

	/* Note, one key difference from the documented compression scheme
	 * is that BIT59 used for TCE bypass mode on PCIe is preserved during
	 * NVLink address compression to allow for the resulting DMA address to
	 * be used transparently on PCIe.
	 */
	if (has_npu_dev(dev, 0)) {
		addr47 = addr & (1LLU << 59);
		addr47 |= ((addr >> 45) & 0x3) << 43;
		addr47 |= ((addr >> 49) & 0x3) << 45;
		addr47 |= addr & ((1LLU << 43) - 1);
	}

	return addr47;
}
#else
#define mods_compress_nvlink_addr(dev, addr) (addr)
#endif

#if defined(CONFIG_PPC64) && defined(CONFIG_PCI)
static u64 mods_expand_nvlink_addr(struct pci_dev *dev, u64 addr47)
{
	u64 addr = addr47;

	if (has_npu_dev(dev, 0)) {
		addr = addr47 & ((1LLU << 43) - 1);
		addr |= (addr47 & (3ULL << 43)) << 2;
		addr |= (addr47 & (3ULL << 45)) << 4;
		addr |= addr47 & ~((1ULL << 56) - 1);
	}

	return addr;
}
#else
#define mods_expand_nvlink_addr(dev, addr) (addr)
#endif

#ifdef CONFIG_PCI
/* Unmap a page if it was mapped */
static void mods_dma_unmap_page(struct mods_client *client,
				struct pci_dev     *dev,
				u64                 dev_addr,
				u32                 order)
{
	dev_addr = mods_expand_nvlink_addr(dev, dev_addr);

	pci_unmap_page(dev,
		       dev_addr,
		       PAGE_SIZE << order,
		       DMA_BIDIRECTIONAL);

	cl_debug(DEBUG_MEM_DETAILED,
		 "dma unmap dev_addr=0x%llx on dev %04x:%02x:%02x.%x\n",
		 (unsigned long long)dev_addr,
		 pci_domain_nr(dev->bus),
		 dev->bus->number,
		 PCI_SLOT(dev->devfn),
		 PCI_FUNC(dev->devfn));
}

/* Unmap and delete the specified DMA mapping */
static void dma_unmap_and_free(struct mods_client   *client,
			       struct MODS_MEM_INFO *p_mem_info,
			       struct MODS_DMA_MAP  *p_del_map)

{
	u32 i;

	for (i = 0; i < p_mem_info->num_chunks; i++)
		mods_dma_unmap_page(client,
				    p_del_map->dev,
				    p_del_map->dev_addr[i],
				    p_mem_info->pages[i].order);

	pci_dev_put(p_del_map->dev);

	kfree(p_del_map);
	atomic_dec(&client->num_allocs);
}
#endif

/* Unmap and delete all DMA mappings on the specified allocation */
static int dma_unmap_all(struct mods_client   *client,
			 struct MODS_MEM_INFO *p_mem_info,
			 struct pci_dev       *dev)
{
#ifdef CONFIG_PCI
	int               err  = OK;
	struct list_head *head = &p_mem_info->dma_map_list;
	struct list_head *iter;
	struct list_head *tmp;

	list_for_each_safe(iter, tmp, head) {
		struct MODS_DMA_MAP *p_dma_map;

		p_dma_map = list_entry(iter, struct MODS_DMA_MAP, list);

		if (!dev || (p_dma_map->dev == dev)) {
			list_del(iter);

			dma_unmap_and_free(client, p_mem_info, p_dma_map);
			if (dev)
				break;
		}
	}

	return err;
#else
	return OK;
#endif
}

#ifdef CONFIG_PCI
static int pci_map_chunk(struct mods_client     *client,
			 struct pci_dev         *dev,
			 struct MODS_PHYS_CHUNK *chunk,
			 u64                    *out_dev_addr)
{
	u64 dev_addr = pci_map_page(dev,
				    chunk->p_page,
				    0,
				    PAGE_SIZE << chunk->order,
				    DMA_BIDIRECTIONAL);

	int err = pci_dma_mapping_error(dev, dev_addr);

	if (err) {
		cl_error(
			"failed to map 2^%u pages at 0x%llx to dev %04x:%02x:%02x.%x with dma mask 0x%llx\n",
			chunk->order,
			(unsigned long long)chunk->dma_addr,
			pci_domain_nr(dev->bus),
			dev->bus->number,
			PCI_SLOT(dev->devfn),
			PCI_FUNC(dev->devfn),
			(unsigned long long)dma_get_mask(&dev->dev));

		return err;
	}

	*out_dev_addr = mods_compress_nvlink_addr(dev, dev_addr);

	return OK;
}

/* DMA map all pages in an allocation */
static int mods_dma_map_pages(struct mods_client   *client,
			      struct MODS_MEM_INFO *p_mem_info,
			      struct MODS_DMA_MAP  *p_dma_map)
{
	int i;
	struct pci_dev *dev = p_dma_map->dev;

	for (i = 0; i < p_mem_info->num_chunks; i++) {
		struct MODS_PHYS_CHUNK *chunk = &p_mem_info->pages[i];
		u64                     dev_addr;

		int err = pci_map_chunk(client, dev, chunk, &dev_addr);

		if (err) {
			while (--i >= 0)
				mods_dma_unmap_page(client,
						    dev,
						    p_dma_map->dev_addr[i],
						    chunk->order);

			return err;
		}

		p_dma_map->dev_addr[i] = dev_addr;

		cl_debug(DEBUG_MEM_DETAILED,
			 "dma map dev_addr=0x%llx, phys_addr=0x%llx on dev %04x:%02x:%02x.%x\n",
			 (unsigned long long)dev_addr,
			 (unsigned long long)chunk->dma_addr,
			 pci_domain_nr(dev->bus),
			 dev->bus->number,
			 PCI_SLOT(dev->devfn),
			 PCI_FUNC(dev->devfn));
	}

	return OK;
}

/* Create a DMA map on the specified allocation for the pci device.
 * Lazy-initialize the map list structure if one does not yet exist.
 */
static int mods_create_dma_map(struct mods_client   *client,
			       struct MODS_MEM_INFO *p_mem_info,
			       struct pci_dev       *dev)
{
	struct MODS_DMA_MAP *p_dma_map;
	u32                  alloc_size;
	int                  err;

	alloc_size = sizeof(*p_dma_map) +
		     (p_mem_info->num_chunks - 1) * sizeof(u64);

	p_dma_map = kzalloc(alloc_size, GFP_KERNEL | __GFP_NORETRY);
	if (unlikely(!p_dma_map)) {
		cl_error("failed to allocate device map data\n");
		return -ENOMEM;
	}
	atomic_inc(&client->num_allocs);

	p_dma_map->dev = pci_dev_get(dev);
	err = mods_dma_map_pages(client, p_mem_info, p_dma_map);

	if (unlikely(err)) {
		pci_dev_put(dev);
		kfree(p_dma_map);
		atomic_dec(&client->num_allocs);
	} else
		list_add(&p_dma_map->list, &p_mem_info->dma_map_list);

	return err;
}

static int mods_dma_map_default_page(struct mods_client     *client,
				     struct MODS_PHYS_CHUNK *chunk,
				     struct pci_dev         *dev)
{
	u64 dev_addr;
	int err = pci_map_chunk(client, dev, chunk, &dev_addr);

	if (unlikely(err))
		return err;

	chunk->dev_addr = dev_addr;
	chunk->mapped   = 1;

	cl_debug(DEBUG_MEM_DETAILED,
		 "auto dma map dev_addr=0x%llx, phys_addr=0x%llx on dev %04x:%02x:%02x.%x\n",
		 (unsigned long long)dev_addr,
		 (unsigned long long)chunk->dma_addr,
		 pci_domain_nr(dev->bus),
		 dev->bus->number,
		 PCI_SLOT(dev->devfn),
		 PCI_FUNC(dev->devfn));

	return OK;
}

/* DMA-map memory to the device for which it has been allocated, if it hasn't
 * been mapped already.
 */
static int mods_create_default_dma_map(struct mods_client   *client,
				       struct MODS_MEM_INFO *p_mem_info)
{
	int             err = OK;
	unsigned int    i;
	struct pci_dev *dev = p_mem_info->dev;

	for (i = 0; i < p_mem_info->num_chunks; i++) {
		struct MODS_PHYS_CHUNK *chunk = &p_mem_info->pages[i];

		if (chunk->mapped) {
			cl_debug(DEBUG_MEM_DETAILED,
				 "memory %p already mapped to dev %04x:%02x:%02x.%x\n",
				 p_mem_info,
				 pci_domain_nr(dev->bus),
				 dev->bus->number,
				 PCI_SLOT(dev->devfn),
				 PCI_FUNC(dev->devfn));
			return OK;
		}

		err = mods_dma_map_default_page(client, chunk, dev);
		if (unlikely(err))
			break;
	}

	return err;
}
#endif /* CONFIG_PCI */

/* Find the dma mapping chunk for the specified memory. */
static struct MODS_DMA_MAP *find_dma_map(struct MODS_MEM_INFO  *p_mem_info,
					 struct mods_pci_dev_2 *pcidev)
{
	struct MODS_DMA_MAP *p_dma_map = NULL;
	struct list_head    *head      = &p_mem_info->dma_map_list;
	struct list_head    *iter;

	if (!head)
		return NULL;

	list_for_each(iter, head) {
		p_dma_map = list_entry(iter, struct MODS_DMA_MAP, list);

		if (mods_is_pci_dev(p_dma_map->dev, pcidev))
			return p_dma_map;
	}
	return NULL;
}

/* In order to map pages as UC or WC to the CPU, we need to change their
 * attributes by calling set_memory_uc()/set_memory_wc(), respectively.
 * On some CPUs this operation is extremely slow.  In order to incur
 * this penalty only once, we save pages mapped as UC or WC so that
 * we can reuse them later.
 */
static int save_non_wb_chunks(struct mods_client   *client,
			      struct MODS_MEM_INFO *p_mem_info)
{
	u32 ichunk;
	int err = 0;

	if (p_mem_info->cache_type == MODS_ALLOC_CACHED)
		return 0;

	err = mutex_lock_interruptible(&client->mtx);
	if (unlikely(err))
		return err;

	/* Steal the chunks from MODS_MEM_INFO and put them on free list. */

	for (ichunk = 0; ichunk < p_mem_info->num_chunks; ichunk++) {

		struct MODS_PHYS_CHUNK      *chunk = &p_mem_info->pages[ichunk];
		struct MODS_FREE_PHYS_CHUNK *free_chunk;

		if (!chunk->wc)
			continue;

		free_chunk = kzalloc(sizeof(struct MODS_FREE_PHYS_CHUNK),
				     GFP_KERNEL | __GFP_NORETRY);

		if (unlikely(!free_chunk)) {
			err = -ENOMEM;
			break;
		}
		atomic_inc(&client->num_allocs);

		free_chunk->numa_node  = p_mem_info->numa_node;
		free_chunk->order      = chunk->order;
		free_chunk->cache_type = p_mem_info->cache_type;
		free_chunk->dma32      = p_mem_info->dma32;
		free_chunk->p_page     = chunk->p_page;

		chunk->p_page = NULL;

		cl_debug(DEBUG_MEM_DETAILED,
			 "save 0x%llx 2^%u pages %s\n",
			 (unsigned long long)(size_t)free_chunk->p_page,
			 chunk->order,
			 p_mem_info->cache_type == MODS_ALLOC_WRITECOMBINE
			     ? "WC" : "UC");

#ifdef CONFIG_PCI
		if (chunk->mapped) {
			mods_dma_unmap_page(client,
					    p_mem_info->dev,
					    chunk->dev_addr,
					    chunk->order);
			chunk->mapped = 0;
		}
#endif

		list_add(&free_chunk->list, &client->free_mem_list);
	}

	mutex_unlock(&client->mtx);

	return err;
}

static int mods_restore_cache_one_chunk(struct page *p_page, u8 order)
{
	int final_err = 0;
	u32 num_pages = 1U << order;
	u32 i;

	for (i = 0; i < num_pages; i++) {
		void *ptr = kmap(p_page + i);
		int   err = -ENOMEM;

		if (likely(ptr))
			err = MODS_SET_MEMORY_WB((unsigned long)ptr, 1);

		kunmap(ptr);

		if (likely(!final_err))
			final_err = err;
	}

	return final_err;
}

static int release_free_chunks(struct mods_client *client)
{
	struct list_head *head;
	struct list_head *iter;
	struct list_head *next;
	int               final_err = 0;

	mutex_lock(&client->mtx);

	head = &client->free_mem_list;

	list_for_each_prev_safe(iter, next, head) {

		struct MODS_FREE_PHYS_CHUNK *free_chunk;
		int                          err;

		free_chunk = list_entry(iter,
					struct MODS_FREE_PHYS_CHUNK,
					list);

		list_del(iter);

		err = mods_restore_cache_one_chunk(free_chunk->p_page,
						   free_chunk->order);
		if (likely(!final_err))
			final_err = err;

		__free_pages(free_chunk->p_page, free_chunk->order);
		atomic_sub(1u << free_chunk->order, &client->num_pages);

		kfree(free_chunk);
		atomic_dec(&client->num_allocs);
	}

	mutex_unlock(&client->mtx);

	if (unlikely(final_err))
		cl_error("failed to restore cache attributes\n");

	return final_err;
}

static int mods_restore_cache(struct mods_client   *client,
			      struct MODS_MEM_INFO *p_mem_info)
{
	unsigned int i;
	int          final_err = 0;

	if (p_mem_info->cache_type == MODS_ALLOC_CACHED)
		return 0;

	for (i = 0; i < p_mem_info->num_chunks; i++) {

		struct MODS_PHYS_CHUNK *chunk = &p_mem_info->pages[i];
		int                     err;

		if (!chunk->p_page || !chunk->wc)
			continue;

		err = mods_restore_cache_one_chunk(chunk->p_page, chunk->order);
		if (likely(!final_err))
			final_err = err;
	}

	if (unlikely(final_err))
		cl_error("failed to restore cache attributes\n");

	return final_err;
}

static void mods_free_pages(struct mods_client   *client,
			    struct MODS_MEM_INFO *p_mem_info)
{
	unsigned int i;

	mods_restore_cache(client, p_mem_info);

#if defined(MODS_HAS_TEGRA)
	if (p_mem_info->iommu_mapped)
		mods_smmu_unmap_memory(client, p_mem_info);
#endif

	/* release in reverse order */
	for (i = p_mem_info->num_chunks; i > 0; ) {
		struct MODS_PHYS_CHUNK *chunk;

		--i;
		chunk = &p_mem_info->pages[i];
		if (!chunk->p_page)
			continue;

#ifdef CONFIG_PCI
		if (chunk->mapped) {
			mods_dma_unmap_page(client,
					    p_mem_info->dev,
					    chunk->dev_addr,
					    chunk->order);
			chunk->mapped = 0;
		}
#endif

		__free_pages(chunk->p_page, chunk->order);
		atomic_sub(1u << chunk->order, &client->num_pages);

		chunk->p_page = NULL;
	}
}

static gfp_t mods_alloc_flags(struct MODS_MEM_INFO *p_mem_info, u32 order)
{
	gfp_t flags = GFP_KERNEL | __GFP_NORETRY | __GFP_NOWARN;

	if (p_mem_info->force_numa)
		flags |= __GFP_THISNODE;

	if (order)
		flags |= __GFP_COMP;

	if (p_mem_info->dma32)
#ifdef CONFIG_ZONE_DMA32
		flags |= __GFP_DMA32;
#else
		flags |= __GFP_DMA;
#endif
	else
		flags |= __GFP_HIGHMEM;

	return flags;
}

static struct page *mods_alloc_pages(struct mods_client   *client,
				     struct MODS_MEM_INFO *p_mem_info,
				     u32                   order,
				     int                  *need_cup)
{
	struct page *p_page     = NULL;
	u8           cache_type = p_mem_info->cache_type;
	u8           dma32      = p_mem_info->dma32;
	int          numa_node  = p_mem_info->numa_node;

	if ((cache_type != MODS_MEMORY_CACHED) &&
	    likely(!mutex_lock_interruptible(&client->mtx))) {

		struct list_head            *iter;
		struct list_head            *head = &client->free_mem_list;
		struct MODS_FREE_PHYS_CHUNK *free_chunk = NULL;

		list_for_each(iter, head) {
			free_chunk = list_entry(iter,
						struct MODS_FREE_PHYS_CHUNK,
						list);

			if (free_chunk->cache_type == cache_type &&
			    free_chunk->dma32      == dma32 &&
			    free_chunk->numa_node  == numa_node &&
			    free_chunk->order      == order) {

				list_del(iter);
				break;
			}

			free_chunk = NULL;
		}

		mutex_unlock(&client->mtx);

		if (free_chunk) {
			p_page = free_chunk->p_page;
			kfree(free_chunk);
			atomic_dec(&client->num_allocs);

			cl_debug(DEBUG_MEM_DETAILED,
				 "reuse 0x%llx 2^%u pages %s\n",
				 (unsigned long long)(size_t)p_page,
				 order,
				 cache_type == MODS_ALLOC_WRITECOMBINE
				     ? "WC" : "UC");

			*need_cup = 0;
			return p_page;
		}
	}

	p_page = alloc_pages_node(p_mem_info->numa_node,
				  mods_alloc_flags(p_mem_info, order),
				  order);

	*need_cup = 1;

	if (likely(p_page))
		atomic_add(1u << order, &client->num_pages);

	return p_page;
}

static int mods_alloc_contig_sys_pages(struct mods_client   *client,
				       struct MODS_MEM_INFO *p_mem_info)
{
	int          err      = -ENOMEM;
	u64          phys_addr;
	u64          dma_addr;
	u64          end_addr = 0;
	u32          order    = 0;
	int          is_wb    = 1;
	struct page *p_page;

	LOG_ENT();

	while ((1U << order) < p_mem_info->num_pages)
		order++;
	p_mem_info->pages[0].order = order;

	p_page = mods_alloc_pages(client, p_mem_info, order, &is_wb);

	if (unlikely(!p_page))
		goto failed;

	p_mem_info->pages[0].p_page = p_page;

	if (!is_wb)
		p_mem_info->pages[0].wc = 1;

	phys_addr = page_to_phys(p_page);
	if (unlikely(phys_addr == 0)) {
		cl_error("failed to determine physical address\n");
		goto failed;
	}
	dma_addr = MODS_PHYS_TO_DMA(phys_addr);

	if (unlikely(dma_addr >= (1ULL << DMA_BITS))) {
		cl_error("dma_addr 0x%llx exceeds supported range\n",
			 dma_addr);
		goto failed;
	}

	p_mem_info->pages[0].dma_addr = dma_addr;

	cl_debug(DEBUG_MEM,
		 "alloc contig 0x%lx bytes, 2^%u pages, %s, node %d,%s phys 0x%llx\n",
		 (unsigned long)p_mem_info->num_pages << PAGE_SHIFT,
		 p_mem_info->pages[0].order,
		 mods_get_prot_str(p_mem_info->cache_type),
		 p_mem_info->numa_node,
		 p_mem_info->dma32 ? " dma32," : "",
		 (unsigned long long)dma_addr);

	end_addr = dma_addr +
		   ((unsigned long)p_mem_info->num_pages << PAGE_SHIFT);
	if (unlikely(p_mem_info->dma32 && (end_addr > 0x100000000ULL))) {
		cl_error("allocation exceeds 32-bit addressing\n");
		goto failed;
	}

	err = mods_post_alloc(client, p_mem_info->pages, phys_addr, p_mem_info);

failed:
	LOG_EXT();
	return err;
}

static u32 mods_get_max_order_needed(u32 num_pages)
{
	u32 order = 0;

	while (order < 10 && (1U<<(order+1)) <= num_pages)
		++order;
	return order;
}

static int mods_alloc_noncontig_sys_pages(struct mods_client   *client,
					  struct MODS_MEM_INFO *p_mem_info)
{
	int err;
	u32 pages_left = p_mem_info->num_pages;
	u32 num_chunks = 0;

	LOG_ENT();

	memset(p_mem_info->pages, 0,
	       p_mem_info->num_chunks * sizeof(p_mem_info->pages[0]));

	while (pages_left > 0) {
		u64 phys_addr = 0;
		u64 dma_addr  = 0;
		u32 order     = mods_get_max_order_needed(pages_left);
		int is_wb     = 1;
		struct MODS_PHYS_CHUNK *chunk = &p_mem_info->pages[num_chunks];

		/* Fail if memory fragmentation is very high */
		if (unlikely(num_chunks >= p_mem_info->num_chunks)) {
			cl_error("detected high memory fragmentation\n");
			err = -ENOMEM;
			goto failed;
		}

		for (;;) {
			chunk->p_page = mods_alloc_pages(client,
							 p_mem_info,
							 order,
							 &is_wb);
			if (chunk->p_page)
				break;
			if (order == 0)
				break;
			--order;
		}

		if (unlikely(!chunk->p_page)) {
			cl_error("out of memory\n");
			err = -ENOMEM;
			goto failed;
		}

		if (!is_wb)
			chunk->wc = 1;

		pages_left -= 1U << order;
		chunk->order = order;

		phys_addr = page_to_phys(chunk->p_page);
		if (unlikely(phys_addr == 0)) {
			cl_error("phys addr lookup failed\n");
			err = -ENOMEM;
			goto failed;
		}
		dma_addr = MODS_PHYS_TO_DMA(phys_addr);

		if (unlikely(dma_addr >= (1ULL << DMA_BITS))) {
			cl_error("dma_addr 0x%llx exceeds supported range\n",
				 dma_addr);
			err = -ENOMEM;
			goto failed;
		}

		chunk->dma_addr = dma_addr;
		cl_debug(DEBUG_MEM,
			 "alloc 0x%lx bytes [%u], 2^%u pages, %s, node %d,%s phys 0x%llx\n",
			 (unsigned long)p_mem_info->num_pages << PAGE_SHIFT,
			 (unsigned int)num_chunks,
			 chunk->order,
			 mods_get_prot_str(p_mem_info->cache_type),
			 p_mem_info->numa_node,
			 p_mem_info->dma32 ? " dma32," : "",
			 (unsigned long long)chunk->dma_addr);

		++num_chunks;

		err = mods_post_alloc(client, chunk, phys_addr, p_mem_info);
		if (unlikely(err))
			goto failed;
	}

	err = 0;

failed:
	LOG_EXT();
	return err;
}

static int mods_register_alloc(struct mods_client   *client,
			       struct MODS_MEM_INFO *p_mem_info)
{
	int err = mutex_lock_interruptible(&client->mtx);

	if (unlikely(err))
		return err;
	list_add(&p_mem_info->list, &client->mem_alloc_list);
	mutex_unlock(&client->mtx);
	return OK;
}

static int validate_mem_handle(struct mods_client   *client,
			       struct MODS_MEM_INFO *p_mem_info)
{
	struct list_head *head = &client->mem_alloc_list;
	struct list_head *iter;

	list_for_each(iter, head) {
		struct MODS_MEM_INFO *p_mem =
			list_entry(iter, struct MODS_MEM_INFO, list);

		if (p_mem == p_mem_info)
			return true;
	}

	return false;
}

static int mods_unregister_and_free(struct mods_client   *client,
				    struct MODS_MEM_INFO *p_del_mem)
{
	struct MODS_MEM_INFO *p_mem_info;
	struct list_head     *head;
	struct list_head     *iter;

	cl_debug(DEBUG_MEM_DETAILED, "free %p\n", p_del_mem);

	mutex_lock(&client->mtx);

	head = &client->mem_alloc_list;

	list_for_each(iter, head) {
		p_mem_info = list_entry(iter, struct MODS_MEM_INFO, list);

		if (p_del_mem == p_mem_info) {
			list_del(iter);

			mutex_unlock(&client->mtx);

			dma_unmap_all(client, p_mem_info, NULL);
			save_non_wb_chunks(client, p_mem_info);
			mods_free_pages(client, p_mem_info);
			pci_dev_put(p_mem_info->dev);

			kfree(p_mem_info);
			atomic_dec(&client->num_allocs);

			return OK;
		}
	}

	mutex_unlock(&client->mtx);

	cl_error("failed to unregister allocation %p\n", p_del_mem);
	return -EINVAL;
}

int mods_unregister_all_alloc(struct mods_client *client)
{
	int               final_err = OK;
	int               err;
	struct list_head *head      = &client->mem_alloc_list;
	struct list_head *iter;
	struct list_head *tmp;

	list_for_each_safe(iter, tmp, head) {

		struct MODS_MEM_INFO *p_mem_info;

		p_mem_info = list_entry(iter, struct MODS_MEM_INFO, list);
		err = mods_unregister_and_free(client, p_mem_info);
		if (likely(!final_err))
			final_err = err;
	}

	err = release_free_chunks(client);
	if (likely(!final_err))
		final_err = err;

	return final_err;
}

static int get_addr_range(struct mods_client                 *client,
			  struct MODS_GET_PHYSICAL_ADDRESS_3 *p,
			  struct mods_pci_dev_2              *pcidev)
{
	struct MODS_MEM_INFO *p_mem_info;
	struct MODS_DMA_MAP  *p_dma_map = NULL;
	u64                  *out;
	u32                   num_out   = 1;
	u32                   skip_pages;
	u32                   i;
	int                   err       = OK;
	u32                   page_offs;

	LOG_ENT();

	p_mem_info = (struct MODS_MEM_INFO *)(size_t)p->memory_handle;
	if (unlikely(!p_mem_info)) {
		cl_error("no allocation given\n");
		LOG_EXT();
		return -EINVAL;
	}

	if (unlikely(pcidev && (pcidev->bus > 0xFFU ||
				pcidev->device > 0xFFU))) {
		cl_error("dev %04x:%02x:%02x.%x not found\n",
			 pcidev->domain,
			 pcidev->bus,
			 pcidev->device,
			 pcidev->function);
		LOG_EXT();
		return -EINVAL;
	}

	out = &p->physical_address;

	if (pcidev) {
		if (mods_is_pci_dev(p_mem_info->dev, pcidev)) {
			if (!p_mem_info->pages[0].mapped)
				err = -EINVAL;
		} else {
			p_dma_map = find_dma_map(p_mem_info, pcidev);
			if (!p_dma_map)
				err = -EINVAL;
		}

		if (err) {
			cl_error(
				"allocation %p is not mapped to dev %04x:%02x:%02x.%x\n",
				p_mem_info,
				pcidev->domain,
				pcidev->bus,
				pcidev->device,
				pcidev->function);
			LOG_EXT();
			return err;
		}
	}

	page_offs  = p->offset & (~PAGE_MASK);
	skip_pages = p->offset >> PAGE_SHIFT;

	for (i = 0; i < p_mem_info->num_chunks && num_out; i++) {
		u32 num_pages;
		u64 addr;
		struct MODS_PHYS_CHUNK *chunk = &p_mem_info->pages[i];

		num_pages = 1U << chunk->order;
		if (num_pages <= skip_pages) {
			skip_pages -= num_pages;
			continue;
		}

		addr = pcidev ?
			(p_dma_map ? p_dma_map->dev_addr[i] : chunk->dev_addr)
			: chunk->dma_addr;

		if (skip_pages) {
			num_pages -= skip_pages;
			addr += skip_pages << PAGE_SHIFT;
			skip_pages = 0;
		}

		if (num_pages > num_out)
			num_pages = num_out;

		while (num_pages) {
			*out = addr + page_offs;
			++out;
			--num_out;
			addr += PAGE_SIZE;
			--num_pages;
		}
	}

	if (unlikely(num_out)) {
		cl_error("invalid offset 0x%llx requested for allocation %p\n",
			 p->offset, p_mem_info);
		err = -EINVAL;
	}

	LOG_EXT();
	return err;
}

/* Returns an offset within an allocation deduced from physical address.
 * If dma address doesn't belong to the allocation, returns non-zero.
 */
static int get_alloc_offset(struct MODS_MEM_INFO *p_mem_info,
			    u64			  dma_addr,
			    u64			 *ret_offs)
{
	u32 i;
	u64 offset = 0;

	for (i = 0; i < p_mem_info->num_chunks; i++) {
		struct MODS_PHYS_CHUNK *chunk = &p_mem_info->pages[i];
		u64 addr = chunk->dma_addr;
		u32 size = PAGE_SIZE << chunk->order;

		if (dma_addr >= addr &&
		    dma_addr <  addr + size) {
			*ret_offs = dma_addr - addr + offset;
			return 0;
		}

		offset += size;
	}

	/* The physical address doesn't belong to the allocation */
	return -EINVAL;
}

struct MODS_MEM_INFO *mods_find_alloc(struct mods_client *client, u64 phys_addr)
{
	struct list_head     *plist_head = &client->mem_alloc_list;
	struct list_head     *plist_iter;
	struct MODS_MEM_INFO *p_mem_info;
	u64                   offset;

	list_for_each(plist_iter, plist_head) {
		p_mem_info = list_entry(plist_iter,
					struct MODS_MEM_INFO,
					list);
		if (!get_alloc_offset(p_mem_info, phys_addr, &offset))
			return p_mem_info;
	}

	/* The physical address doesn't belong to any allocation */
	return NULL;
}

/* Estimate the initial number of chunks supported, assuming medium memory
 * fragmentation.
 */
static u32 estimate_num_chunks(u32 num_pages)
{
	u32 num_chunks = 0;
	u32 bit_scan;

	/* Count each contiguous block <=256KB */
	for (bit_scan = num_pages; bit_scan && num_chunks < 6; bit_scan >>= 1)
		++num_chunks;

	/* Count remaining contiguous blocks >256KB */
	num_chunks += bit_scan;

	/* 4x slack for medium memory fragmentation */
	num_chunks <<= 2;

	/* No sense to allocate more chunks than pages */
	if (num_chunks > num_pages)
		num_chunks = num_pages;

	return num_chunks;
}

/* For large non-contiguous allocations, we typically use significantly less
 * chunks than originally estimated.  This function reallocates the
 * MODS_MEM_INFO struct so that it uses only as much memory as it needs.
 */
static struct MODS_MEM_INFO *optimize_chunks(struct mods_client   *client,
					     struct MODS_MEM_INFO *p_mem_info)
{
	u32 i;
	u32 num_chunks;
	u32 alloc_size = 0;
	struct MODS_MEM_INFO *p_new_mem_info = NULL;

	for (i = 0; i < p_mem_info->num_chunks; i++)
		if (!p_mem_info->pages[i].p_page)
			break;

	num_chunks = i;

	if (num_chunks < p_mem_info->num_chunks) {
		alloc_size = sizeof(*p_mem_info) +
			 (num_chunks - 1) * sizeof(struct MODS_PHYS_CHUNK);

		p_new_mem_info = kzalloc(alloc_size,
					 GFP_KERNEL | __GFP_NORETRY);
	}

	if (p_new_mem_info) {
		memcpy(p_new_mem_info, p_mem_info, alloc_size);
		p_new_mem_info->num_chunks = num_chunks;
		INIT_LIST_HEAD(&p_new_mem_info->dma_map_list);
		kfree(p_mem_info);
		p_mem_info = p_new_mem_info;
	}

	return p_mem_info;
}

/************************
 * ESCAPE CALL FUNCTONS *
 ************************/

int esc_mods_alloc_pages_2(struct mods_client        *client,
			   struct MODS_ALLOC_PAGES_2 *p)
{
	int                   err        = -EINVAL;
	struct MODS_MEM_INFO *p_mem_info = NULL;
	u32                   num_pages;
	u32                   alloc_size;
	u32                   num_chunks;

	LOG_ENT();

	p->memory_handle = 0;

	cl_debug(DEBUG_MEM_DETAILED,
		 "alloc 0x%llx bytes flags=0x%x (%s %s%s%s%s%s) node=%d on dev %04x:%02x:%02x.%x\n",
		 (unsigned long long)p->num_bytes,
		 p->flags,
		 mods_get_prot_str(p->flags & MODS_ALLOC_CACHE_MASK),
		 (p->flags & MODS_ALLOC_CONTIGUOUS) ? "contiguous" :
						      "noncontiguous",
		 (p->flags & MODS_ALLOC_DMA32) ? " dma32" : "",
		 (p->flags & MODS_ALLOC_USE_NUMA) ? " usenuma" : "",
		 (p->flags & MODS_ALLOC_FORCE_NUMA) ? " forcenuma" : "",
		 (p->flags & MODS_ALLOC_MAP_DEV) ? " dmamap" : "",
		 p->numa_node,
		 p->pci_device.domain,
		 p->pci_device.bus,
		 p->pci_device.device,
		 p->pci_device.function);

	if (unlikely(!p->num_bytes)) {
		cl_error("zero bytes requested\n");
		goto failed;
	}

	num_pages = (u32)((p->num_bytes + PAGE_SIZE - 1) >> PAGE_SHIFT);
	if (p->flags & MODS_ALLOC_CONTIGUOUS)
		num_chunks = 1;
	else
		num_chunks = estimate_num_chunks(num_pages);
	alloc_size = sizeof(*p_mem_info) +
		     (num_chunks - 1) * sizeof(struct MODS_PHYS_CHUNK);

	if (unlikely(((u64)num_pages << PAGE_SHIFT) < p->num_bytes)) {
		cl_error("invalid allocation size requested: 0x%llx\n",
			 (unsigned long long)p->num_bytes);
		goto failed;
	}

	if (unlikely((p->flags & MODS_ALLOC_USE_NUMA) &&
		     (p->numa_node != MODS_ANY_NUMA_NODE) &&
		      ((unsigned int)p->numa_node >=
		       (unsigned int)num_possible_nodes()))) {

		cl_error("invalid NUMA node: %d\n", p->numa_node);
		goto failed;
	}

#ifdef CONFIG_PPC64
	if (unlikely((p->flags & MODS_ALLOC_CACHE_MASK) != MODS_ALLOC_CACHED)) {
		cl_error("unsupported cache attr %u (%s)\n",
			 p->flags & MODS_ALLOC_CACHE_MASK,
			 mods_get_prot_str(p->flags & MODS_ALLOC_CACHE_MASK));
		err = -ENOMEM;
		goto failed;
	}
#endif

	p_mem_info = kzalloc(alloc_size, GFP_KERNEL | __GFP_NORETRY);
	if (unlikely(!p_mem_info)) {
		cl_error("failed to allocate auxiliary 0x%x bytes\n",
			 alloc_size);
		err = -ENOMEM;
		goto failed;
	}
	atomic_inc(&client->num_allocs);

	p_mem_info->num_chunks = num_chunks;
	p_mem_info->num_pages  = num_pages;
	p_mem_info->cache_type = p->flags & MODS_ALLOC_CACHE_MASK;
	p_mem_info->dma32      = (p->flags & MODS_ALLOC_DMA32) ? true : false;
	p_mem_info->contig     = (p->flags & MODS_ALLOC_CONTIGUOUS)
				 ? true : false;
	p_mem_info->force_numa = (p->flags & MODS_ALLOC_FORCE_NUMA)
				 ? true : false;
#ifdef MODS_HASNT_NUMA_NO_NODE
	p_mem_info->numa_node  = numa_node_id();
#else
	p_mem_info->numa_node  = NUMA_NO_NODE;
#endif
	p_mem_info->dev        = NULL;

	if ((p->flags & MODS_ALLOC_USE_NUMA) &&
	    p->numa_node != MODS_ANY_NUMA_NODE)
		p_mem_info->numa_node = p->numa_node;

	INIT_LIST_HEAD(&p_mem_info->dma_map_list);

#ifdef CONFIG_PCI
	if (!(p->flags & MODS_ALLOC_USE_NUMA) ||
	    (p->flags & MODS_ALLOC_MAP_DEV)) {

		struct pci_dev *dev = NULL;

		err = mods_find_pci_dev(client, &p->pci_device, &dev);
		if (unlikely(err)) {
			cl_error("dev %04x:%02x:%02x.%x not found\n",
				 p->pci_device.domain,
				 p->pci_device.bus,
				 p->pci_device.device,
				 p->pci_device.function);
			goto failed;
		}

		p_mem_info->dev = dev;
		if (!(p->flags & MODS_ALLOC_USE_NUMA))
			p_mem_info->numa_node = dev_to_node(&dev->dev);

#ifdef CONFIG_PPC64
		if (!mods_is_nvlink_sysmem_trained(client, dev)) {
			/* Until NvLink is trained, we must use memory
			 * on node 0.
			 */
			if (has_npu_dev(dev, 0))
				p_mem_info->numa_node = 0;
		}
#endif
		cl_debug(DEBUG_MEM_DETAILED,
			 "affinity dev %04x:%02x:%02x.%x node %d\n",
			 p->pci_device.domain,
			 p->pci_device.bus,
			 p->pci_device.device,
			 p->pci_device.function,
			 p_mem_info->numa_node);

		if (!(p->flags & MODS_ALLOC_MAP_DEV)) {
			pci_dev_put(p_mem_info->dev);
			p_mem_info->dev = NULL;
		}
	}
#endif

	if (p->flags & MODS_ALLOC_CONTIGUOUS)
		err = mods_alloc_contig_sys_pages(client, p_mem_info);
	else {
		err = mods_alloc_noncontig_sys_pages(client, p_mem_info);

		if (likely(!err))
			p_mem_info = optimize_chunks(client, p_mem_info);
	}

	if (unlikely(err)) {
		cl_error("failed to alloc 0x%lx %s bytes, %s, node %d%s\n",
			 (unsigned long)p_mem_info->num_pages << PAGE_SHIFT,
			 (p->flags & MODS_ALLOC_CONTIGUOUS) ? "contiguous" :
							      "non-contiguous",
			 mods_get_prot_str(p_mem_info->cache_type),
			 p_mem_info->numa_node,
			 p_mem_info->dma32 ? ", dma32" : "");
		goto failed;
	}

	err = mods_register_alloc(client, p_mem_info);
	if (unlikely(err))
		goto failed;

	p->memory_handle = (u64)(size_t)p_mem_info;

	cl_debug(DEBUG_MEM_DETAILED, "alloc %p\n", p_mem_info);

failed:
	if (unlikely(err && p_mem_info)) {
		mods_free_pages(client, p_mem_info);
		pci_dev_put(p_mem_info->dev);
		kfree(p_mem_info);
		atomic_dec(&client->num_allocs);
	}

	LOG_EXT();
	return err;
}

int esc_mods_device_alloc_pages_2(struct mods_client               *client,
				  struct MODS_DEVICE_ALLOC_PAGES_2 *p)
{
	int err;
	u32 flags = 0;
	struct MODS_ALLOC_PAGES_2 dev_alloc_pages = {0};

	LOG_ENT();

	if (p->contiguous)
		flags |= MODS_ALLOC_CONTIGUOUS;

	if (p->address_bits == 32)
		flags |= MODS_ALLOC_DMA32;

	if (p->attrib == MODS_MEMORY_UNCACHED)
		flags |= MODS_ALLOC_UNCACHED;
	else if (p->attrib == MODS_MEMORY_WRITECOMBINE)
		flags |= MODS_ALLOC_WRITECOMBINE;
	else if (unlikely(p->attrib != MODS_MEMORY_CACHED)) {
		cl_error("invalid cache attrib: %u\n", p->attrib);
		LOG_EXT();
		return -ENOMEM;
	}

	if (p->pci_device.bus > 0xFFU || p->pci_device.device > 0xFFU)
		flags |= MODS_ALLOC_USE_NUMA;
	else
		flags |= MODS_ALLOC_MAP_DEV | MODS_ALLOC_FORCE_NUMA;

	dev_alloc_pages.num_bytes  = p->num_bytes;
	dev_alloc_pages.flags      = flags;
	dev_alloc_pages.numa_node  = MODS_ANY_NUMA_NODE;
	dev_alloc_pages.pci_device = p->pci_device;

	err = esc_mods_alloc_pages_2(client, &dev_alloc_pages);
	if (likely(!err))
		p->memory_handle = dev_alloc_pages.memory_handle;

	LOG_EXT();
	return err;
}

int esc_mods_device_alloc_pages(struct mods_client             *client,
				struct MODS_DEVICE_ALLOC_PAGES *p)
{
	int err;
	u32 flags = 0;
	struct MODS_ALLOC_PAGES_2 dev_alloc_pages = {0};

	LOG_ENT();

	if (p->contiguous)
		flags |= MODS_ALLOC_CONTIGUOUS;

	if (p->address_bits == 32)
		flags |= MODS_ALLOC_DMA32;

	if (p->attrib == MODS_MEMORY_UNCACHED)
		flags |= MODS_ALLOC_UNCACHED;
	else if (p->attrib == MODS_MEMORY_WRITECOMBINE)
		flags |= MODS_ALLOC_WRITECOMBINE;
	else if (unlikely(p->attrib != MODS_MEMORY_CACHED)) {
		cl_error("invalid cache attrib: %u\n", p->attrib);
		LOG_EXT();
		return -ENOMEM;
	}

	if (p->pci_device.bus > 0xFFU || p->pci_device.device > 0xFFU)
		flags |= MODS_ALLOC_USE_NUMA;
	else
		flags |= MODS_ALLOC_MAP_DEV | MODS_ALLOC_FORCE_NUMA;

	dev_alloc_pages.num_bytes           = p->num_bytes;
	dev_alloc_pages.flags               = flags;
	dev_alloc_pages.numa_node           = MODS_ANY_NUMA_NODE;
	dev_alloc_pages.pci_device.domain   = 0;
	dev_alloc_pages.pci_device.bus      = p->pci_device.bus;
	dev_alloc_pages.pci_device.device   = p->pci_device.device;
	dev_alloc_pages.pci_device.function = p->pci_device.function;

	err = esc_mods_alloc_pages_2(client, &dev_alloc_pages);
	if (likely(!err))
		p->memory_handle = dev_alloc_pages.memory_handle;

	LOG_EXT();
	return err;
}

int esc_mods_alloc_pages(struct mods_client *client, struct MODS_ALLOC_PAGES *p)
{
	int err;
	u32 flags = MODS_ALLOC_USE_NUMA;
	struct MODS_ALLOC_PAGES_2 dev_alloc_pages = {0};

	LOG_ENT();

	if (p->contiguous)
		flags |= MODS_ALLOC_CONTIGUOUS;

	if (p->address_bits == 32)
		flags |= MODS_ALLOC_DMA32;

	if (p->attrib == MODS_MEMORY_UNCACHED)
		flags |= MODS_ALLOC_UNCACHED;
	else if (p->attrib == MODS_MEMORY_WRITECOMBINE)
		flags |= MODS_ALLOC_WRITECOMBINE;
	else if (unlikely(p->attrib != MODS_MEMORY_CACHED)) {
		cl_error("invalid cache attrib: %u\n", p->attrib);
		LOG_EXT();
		return -ENOMEM;
	}

	dev_alloc_pages.num_bytes           = p->num_bytes;
	dev_alloc_pages.flags               = flags;
	dev_alloc_pages.numa_node           = MODS_ANY_NUMA_NODE;
	dev_alloc_pages.pci_device.domain   = 0xFFFFU;
	dev_alloc_pages.pci_device.bus      = 0xFFFFU;
	dev_alloc_pages.pci_device.device   = 0xFFFFU;
	dev_alloc_pages.pci_device.function = 0xFFFFU;

	err = esc_mods_alloc_pages_2(client, &dev_alloc_pages);
	if (likely(!err))
		p->memory_handle = dev_alloc_pages.memory_handle;

	LOG_EXT();
	return err;
}

int esc_mods_free_pages(struct mods_client *client, struct MODS_FREE_PAGES *p)
{
	int err;

	LOG_ENT();

	err = mods_unregister_and_free(client,
	    (struct MODS_MEM_INFO *)(size_t)p->memory_handle);

	LOG_EXT();

	return err;
}

int esc_mods_merge_pages(struct mods_client      *client,
			 struct MODS_MERGE_PAGES *p)
{
	int          err        = OK;
	u32          num_chunks = 0;
	u32          alloc_size = 0;
	unsigned int i;
	struct MODS_MEM_INFO *p_mem_info;

	LOG_ENT();

	if (unlikely(p->num_in_handles < 2 ||
		     p->num_in_handles > MODS_MAX_MERGE_HANDLES)) {
		cl_error("invalid number of input handles: %u\n",
			 p->num_in_handles);
		LOG_EXT();
		return -EINVAL;
	}

	err = mutex_lock_interruptible(&client->mtx);
	if (unlikely(err)) {
		LOG_EXT();
		return err;
	}

	{
		const char *err_msg = NULL;

		p_mem_info = (struct MODS_MEM_INFO *)(size_t)
			     p->in_memory_handles[0];

		if (!validate_mem_handle(client, p_mem_info)) {
			cl_error("handle 0: invalid handle %p\n", p_mem_info);
			err = -EINVAL;
			goto failed;
		}

		if (unlikely(!list_empty(&p_mem_info->dma_map_list))) {
			cl_error("handle 0: found dma mappings\n");
			err = -EINVAL;
			goto failed;
		}

		num_chunks = p_mem_info->num_chunks;

		for (i = 1; i < p->num_in_handles; i++) {
			unsigned int j;
			struct MODS_MEM_INFO *p_other =
				(struct MODS_MEM_INFO *)(size_t)
					p->in_memory_handles[i];

			if (!validate_mem_handle(client, p_other)) {
				cl_error("handle %u: invalid handle %p\n",
					 i, p);
				err = -EINVAL;
				goto failed;
			}

			for (j = 0; j < i; j++) {
				if (unlikely(p->in_memory_handles[i] ==
					     p->in_memory_handles[j])) {
					err_msg = "duplicate handle";
					break;
				}
			}
			if (err_msg)
				break;

			if (unlikely(p_mem_info->cache_type !=
				     p_other->cache_type)) {
				err_msg = "cache attr mismatch";
				break;
			}

			if (unlikely(p_mem_info->force_numa &&
			    p_mem_info->numa_node != p_other->numa_node)) {
				err_msg = "numa node mismatch";
				break;
			}

			if (unlikely(p_mem_info->dma32 != p_other->dma32)) {
				err_msg = "dma32 mismatch";
				break;
			}

			if (p_mem_info->dev) {
				if (unlikely(p_mem_info->dev !=
					     p_other->dev)) {
					err_msg = "device mismatch";
					break;
				}

				if (unlikely(p_mem_info->pages[0].mapped !=
					     p_other->pages[0].mapped)) {
					err_msg = "dma mapping mismatch";
					break;
				}
			}

			if (unlikely(!list_empty(&p_other->dma_map_list))) {
				err_msg = "found dma mappings";
				break;
			}

			num_chunks += p_other->num_chunks;
		}

		if (unlikely(err_msg)) {
			cl_error("merging handle %u: %s\n", i, err_msg);
			err = -EINVAL;
			goto failed;
		}
	}

	alloc_size = sizeof(*p_mem_info) +
		     (num_chunks - 1) * sizeof(struct MODS_PHYS_CHUNK);

	p_mem_info = kzalloc(alloc_size, GFP_KERNEL | __GFP_NORETRY);
	if (unlikely(!p_mem_info)) {
		err = -ENOMEM;
		goto failed;
	}
	atomic_inc(&client->num_allocs);

	for (i = 0; i < p->num_in_handles; i++) {
		struct MODS_MEM_INFO *p_other = (struct MODS_MEM_INFO *)(size_t)
						p->in_memory_handles[i];
		u32 other_chunks = p_other->num_chunks;
		u32 other_size = sizeof(*p_other) +
			(other_chunks - 1) * sizeof(struct MODS_PHYS_CHUNK);

		list_del(&p_other->list);

		if (i == 0) {
			memcpy(p_mem_info, p_other, other_size);
			p_mem_info->contig = false;
			INIT_LIST_HEAD(&p_mem_info->dma_map_list);
			list_add(&p_mem_info->list, &client->mem_alloc_list);
		} else {
			memcpy(&p_mem_info->pages[p_mem_info->num_chunks],
			       &p_other->pages[0],
			       other_chunks * sizeof(struct MODS_PHYS_CHUNK));

			p_mem_info->num_chunks += other_chunks;
			p_mem_info->num_pages  += p_other->num_pages;
		}

		kfree(p_other);
		atomic_dec(&client->num_allocs);
	}

	WARN_ON(num_chunks != p_mem_info->num_chunks);

	p->memory_handle = (u64)(size_t)p_mem_info;

failed:
	mutex_unlock(&client->mtx);

	LOG_EXT();

	return err;
}

int esc_mods_set_mem_type(struct mods_client      *client,
			  struct MODS_MEMORY_TYPE *p)
{
	struct MODS_MEM_INFO *p_mem_info;
	u8                    type = MODS_ALLOC_CACHED;
	int                   err;

	LOG_ENT();

	switch (p->type) {
	case MODS_MEMORY_CACHED:
		break;

	case MODS_MEMORY_UNCACHED:
		type = MODS_ALLOC_UNCACHED;
		break;

	case MODS_MEMORY_WRITECOMBINE:
		type = MODS_ALLOC_WRITECOMBINE;
		break;

	default:
		cl_error("unsupported memory type: %u\n", p->type);
		LOG_EXT();
		return -EINVAL;
	}

	err = mutex_lock_interruptible(&client->mtx);
	if (unlikely(err)) {
		LOG_EXT();
		return err;
	}

	p_mem_info = mods_find_alloc(client, p->physical_address);
	if (p_mem_info) {
		mutex_unlock(&client->mtx);
		cl_error("cannot set mem type on phys addr 0x%llx\n",
			 p->physical_address);
		LOG_EXT();
		return -EINVAL;
	}

	client->mem_type.dma_addr = p->physical_address;
	client->mem_type.size     = p->size;
	client->mem_type.type     = type;

	mutex_unlock(&client->mtx);

	LOG_EXT();
	return OK;
}

int esc_mods_get_phys_addr(struct mods_client               *client,
			   struct MODS_GET_PHYSICAL_ADDRESS *p)
{
	struct MODS_GET_PHYSICAL_ADDRESS_3 range;
	int err;

	LOG_ENT();

	range.memory_handle = p->memory_handle;
	range.offset        = p->offset;
	memset(&range.pci_device, 0, sizeof(range.pci_device));

	err = get_addr_range(client, &range, NULL);

	if (!err)
		p->physical_address = range.physical_address;

	LOG_EXT();
	return err;
}

int esc_mods_get_phys_addr_2(struct mods_client                 *client,
			     struct MODS_GET_PHYSICAL_ADDRESS_3 *p)
{
	struct MODS_GET_PHYSICAL_ADDRESS_3 range;
	int err;

	LOG_ENT();

	range.memory_handle = p->memory_handle;
	range.offset        = p->offset;
	memset(&range.pci_device, 0, sizeof(range.pci_device));

	err = get_addr_range(client, &range, NULL);

	if (!err)
		p->physical_address = range.physical_address;

	LOG_EXT();
	return err;
}

int esc_mods_get_mapped_phys_addr(struct mods_client               *client,
				  struct MODS_GET_PHYSICAL_ADDRESS *p)
{
	struct MODS_GET_PHYSICAL_ADDRESS_3 range;
	struct MODS_MEM_INFO *p_mem_info;
	int err;

	LOG_ENT();

	range.memory_handle = p->memory_handle;
	range.offset        = p->offset;

	p_mem_info = (struct MODS_MEM_INFO *)(size_t)p->memory_handle;
	if (p_mem_info->dev) {
		range.pci_device.domain   =
			pci_domain_nr(p_mem_info->dev->bus);
		range.pci_device.bus	   =
			p_mem_info->dev->bus->number;
		range.pci_device.device   =
			PCI_SLOT(p_mem_info->dev->devfn);
		range.pci_device.function =
			PCI_FUNC(p_mem_info->dev->devfn);

		err = get_addr_range(client, &range, &range.pci_device);
	} else {
		memset(&range.pci_device, 0, sizeof(range.pci_device));
		err = get_addr_range(client, &range, NULL);
	}

	if (!err)
		p->physical_address = range.physical_address;

	LOG_EXT();
	return err;
}

int esc_mods_get_mapped_phys_addr_2(struct mods_client                 *client,
				    struct MODS_GET_PHYSICAL_ADDRESS_2 *p)
{
	struct MODS_GET_PHYSICAL_ADDRESS_3 range;
	int err;

	LOG_ENT();

	range.memory_handle = p->memory_handle;
	range.offset        = p->offset;
	range.pci_device    = p->pci_device;

	err = get_addr_range(client, &range, &range.pci_device);

	if (!err)
		p->physical_address = range.physical_address;

	LOG_EXT();
	return err;
}

int esc_mods_get_mapped_phys_addr_3(struct mods_client                 *client,
				    struct MODS_GET_PHYSICAL_ADDRESS_3 *p)
{
	struct MODS_GET_PHYSICAL_ADDRESS_3 range;
	int err;

	LOG_ENT();

	range.memory_handle = p->memory_handle;
	range.offset        = p->offset;
	range.pci_device    = p->pci_device;

	err = get_addr_range(client, &range, &range.pci_device);

	if (!err)
		p->physical_address = range.physical_address;

	LOG_EXT();
	return err;
}

int esc_mods_virtual_to_phys(struct mods_client              *client,
			     struct MODS_VIRTUAL_TO_PHYSICAL *p)
{
	struct MODS_GET_PHYSICAL_ADDRESS get_phys_addr;
	struct list_head                *head;
	struct list_head                *iter;
	int                              err;

	LOG_ENT();

	err = mutex_lock_interruptible(&client->mtx);
	if (unlikely(err)) {
		LOG_EXT();
		return err;
	}

	head = &client->mem_map_list;

	list_for_each(iter, head) {
		struct SYS_MAP_MEMORY *p_map_mem;
		u64		       begin, end;
		u64                    phys_offs;

		p_map_mem = list_entry(iter, struct SYS_MAP_MEMORY, list);

		begin = p_map_mem->virtual_addr;
		end   = p_map_mem->virtual_addr + p_map_mem->mapping_length;

		if (p->virtual_address >= begin && p->virtual_address < end) {

			u64 virt_offs = p->virtual_address - begin;

			/* device memory mapping */
			if (!p_map_mem->p_mem_info) {
				p->physical_address = p_map_mem->dma_addr
						      + virt_offs;
				mutex_unlock(&client->mtx);

				cl_debug(DEBUG_MEM_DETAILED,
					 "get phys: map %p virt 0x%llx -> 0x%llx\n",
					 p_map_mem,
					 p->virtual_address,
					 p->physical_address);

				LOG_EXT();
				return OK;
			}

			if (get_alloc_offset(p_map_mem->p_mem_info,
					     p_map_mem->dma_addr,
					     &phys_offs) != OK)
				break;

			get_phys_addr.memory_handle =
				(u64)(size_t)p_map_mem->p_mem_info;
			get_phys_addr.offset = virt_offs + phys_offs;

			mutex_unlock(&client->mtx);

			err = esc_mods_get_phys_addr(client, &get_phys_addr);
			if (err) {
				LOG_EXT();
				return err;
			}

			p->physical_address = get_phys_addr.physical_address;

			cl_debug(DEBUG_MEM_DETAILED,
				 "get phys: map %p virt 0x%llx -> 0x%llx\n",
				 p_map_mem,
				 p->virtual_address,
				 p->physical_address);

			LOG_EXT();
			return OK;
		}
	}

	mutex_unlock(&client->mtx);

	cl_error("invalid virtual address 0x%llx\n", p->virtual_address);
	LOG_EXT();
	return -EINVAL;
}

int esc_mods_phys_to_virtual(struct mods_client              *client,
			     struct MODS_PHYSICAL_TO_VIRTUAL *p)
{
	struct SYS_MAP_MEMORY *p_map_mem;
	struct list_head      *head;
	struct list_head      *iter;
	u64                    offset;
	u64                    map_offset;
	int                    err;

	LOG_ENT();

	err = mutex_lock_interruptible(&client->mtx);
	if (unlikely(err)) {
		LOG_EXT();
		return err;
	}

	head = &client->mem_map_list;

	list_for_each(iter, head) {
		p_map_mem = list_entry(iter, struct SYS_MAP_MEMORY, list);

		/* device memory mapping */
		if (!p_map_mem->p_mem_info) {
			u64 end = p_map_mem->dma_addr
				+ p_map_mem->mapping_length;
			if (p->physical_address <  p_map_mem->dma_addr ||
			    p->physical_address >= end)
				continue;

			offset = p->physical_address
				 - p_map_mem->dma_addr;
			p->virtual_address = p_map_mem->virtual_addr
					     + offset;
			mutex_unlock(&client->mtx);

			cl_debug(DEBUG_MEM_DETAILED,
				 "get virt: map %p phys 0x%llx -> 0x%llx\n",
				 p_map_mem,
				 p->physical_address,
				 p->virtual_address);

			LOG_EXT();
			return OK;
		}

		/* offset from the beginning of the allocation */
		if (get_alloc_offset(p_map_mem->p_mem_info,
				     p->physical_address,
				     &offset))
			continue;

		/* offset from the beginning of the mapping */
		if (get_alloc_offset(p_map_mem->p_mem_info,
				     p_map_mem->dma_addr,
				     &map_offset))
			continue;

		if ((offset >= map_offset) &&
		    (offset <  map_offset + p_map_mem->mapping_length)) {
			p->virtual_address = p_map_mem->virtual_addr
					   + offset - map_offset;

			mutex_unlock(&client->mtx);
			cl_debug(DEBUG_MEM_DETAILED,
				 "get virt: map %p phys 0x%llx -> 0x%llx\n",
				 p_map_mem,
				 p->physical_address,
				 p->virtual_address);

			LOG_EXT();
			return OK;
		}
	}

	mutex_unlock(&client->mtx);

	cl_error("phys addr 0x%llx is not mapped\n", p->physical_address);
	LOG_EXT();
	return -EINVAL;
}

#if defined(CONFIG_ARM)
int esc_mods_memory_barrier(struct mods_client *client)
{
	/* Full memory barrier on ARMv7 */
	wmb();
	return OK;
}
#endif

#ifdef CONFIG_PCI
int esc_mods_dma_map_memory(struct mods_client         *client,
			    struct MODS_DMA_MAP_MEMORY *p)
{
	struct MODS_MEM_INFO *p_mem_info;
	struct MODS_DMA_MAP  *p_dma_map;
	struct pci_dev       *dev = NULL;
	int                   err = -EINVAL;

	LOG_ENT();

	p_mem_info = (struct MODS_MEM_INFO *)(size_t)p->memory_handle;
	if (unlikely(!p_mem_info)) {
		cl_error("no allocation given\n");
		LOG_EXT();
		return -EINVAL;
	}

	if (mods_is_pci_dev(p_mem_info->dev, &p->pci_device)) {
		err = mods_create_default_dma_map(client, p_mem_info);
		LOG_EXT();
		return err;
	}

	p_dma_map = find_dma_map(p_mem_info, &p->pci_device);
	if (p_dma_map) {
		cl_debug(DEBUG_MEM_DETAILED,
			 "memory %p already mapped to dev %04x:%02x:%02x.%x\n",
			 p_mem_info,
			 p->pci_device.domain,
			 p->pci_device.bus,
			 p->pci_device.device,
			 p->pci_device.function);
		LOG_EXT();
		return 0;
	}

	err = mods_find_pci_dev(client, &p->pci_device, &dev);
	if (unlikely(err)) {
		if (err == -ENODEV)
			cl_error("dev %04x:%02x:%02x.%x not found\n",
				 p->pci_device.domain,
				 p->pci_device.bus,
				 p->pci_device.device,
				 p->pci_device.function);
		LOG_EXT();
		return err;
	}

	err = mods_create_dma_map(client, p_mem_info, dev);

	pci_dev_put(dev);
	LOG_EXT();
	return err;
}

int esc_mods_dma_unmap_memory(struct mods_client         *client,
			      struct MODS_DMA_MAP_MEMORY *p)
{
	struct MODS_MEM_INFO *p_mem_info;
	struct pci_dev       *dev = NULL;
	int                   err = -EINVAL;

	LOG_ENT();

	p_mem_info = (struct MODS_MEM_INFO *)(size_t)p->memory_handle;
	if (unlikely(!p_mem_info)) {
		cl_error("no allocation given\n");
		LOG_EXT();
		return -EINVAL;
	}

	err = mods_find_pci_dev(client, &p->pci_device, &dev);
	if (unlikely(err)) {
		if (err == -ENODEV)
			cl_error("dev %04x:%02x:%02x.%x not found\n",
				 p->pci_device.domain,
				 p->pci_device.bus,
				 p->pci_device.device,
				 p->pci_device.function);
	} else
		err = dma_unmap_all(client, p_mem_info, dev);

	pci_dev_put(dev);
	LOG_EXT();
	return err;
}
#endif /* CONFIG_PCI */

#ifdef MODS_HAS_TEGRA
/* map dma buffer by iommu */
int esc_mods_iommu_dma_map_memory(struct mods_client               *client,
				  struct MODS_IOMMU_DMA_MAP_MEMORY *p)
{
	struct sg_table     *sgt;
	struct scatterlist  *sg;
	u64                  iova;
	int                  smmudev_idx;
	struct MODS_MEM_INFO *p_mem_info;
	char                 *dev_name = p->dev_name;
	struct mods_smmu_dev *smmu_pdev = NULL;
	u32                  num_chunks = 0;
	int                  ents, i, err = 0;
	size_t iova_offset = 0;

	LOG_ENT();

	if (!(p->flags & MODS_IOMMU_MAP_CONTIGUOUS)) {
		cl_error("contiguous flag not set\n");
		err = -EINVAL;
		goto failed;
	}
	p_mem_info = (struct MODS_MEM_INFO *)(size_t)p->memory_handle;
	if (p_mem_info->iommu_mapped) {
		cl_error("smmu is already mapped\n");
		err = -EINVAL;
		goto failed;
	}

	smmudev_idx = get_mods_smmu_device_index(dev_name);
	if (smmudev_idx >= 0)
		smmu_pdev = get_mods_smmu_device(smmudev_idx);
	if (!smmu_pdev || smmudev_idx < 0) {
		cl_error("smmu device %s is not found\n", dev_name);
		err = -ENODEV;
		goto failed;
	}

	/* do smmu mapping */
	num_chunks = p_mem_info->num_chunks;
	cl_debug(DEBUG_MEM_DETAILED,
		 "smmu_map_sg: dev_name=%s, pages=%u, chunks=%u\n",
		 dev_name,
		 p_mem_info->num_pages,
		 num_chunks);
	sgt = vzalloc(sizeof(*sgt));
	if (!sgt) {
		err = -ENOMEM;
		goto failed;
	}
	err = sg_alloc_table(sgt, num_chunks, GFP_KERNEL);
	if (err) {
		cl_error("failed to allocate sg table, err=%d\n",
		err);
		kvfree(sgt);
		goto failed;
	}
	sg = sgt->sgl;
	for (i = 0; i < num_chunks; i++) {
		u32 size = PAGE_SIZE << p_mem_info->pages[i].order;

		sg_set_page(sg,
			    p_mem_info->pages[i].p_page,
			    size,
			    0);
		sg = sg_next(sg);
	}

	ents = dma_map_sg_attrs(smmu_pdev->dev, sgt->sgl, sgt->nents,
				DMA_BIDIRECTIONAL, 0);
	if (ents <= 0) {
		cl_error("failed to map sg attrs. err=%d\n", ents);
		sg_free_table(sgt);
		kvfree(sgt);
		err = -ENOMEM;
		goto failed;
	}

	p_mem_info->smmudev_idx = smmudev_idx;
	p_mem_info->iommu_mapped = 1;
	iova = sg_dma_address(sgt->sgl);
	p_mem_info->pages[0].dev_addr = iova;
	p_mem_info->sgt = sgt;

	/* Check if IOVAs are contiguous */
	iova_offset = 0;
	for_each_sg(sgt->sgl, sg, sgt->nents, i) {
		iova_offset = iova_offset + sg->offset;
		if (sg_dma_address(sg) != (iova + iova_offset)
		    || sg_dma_len(sg) != sg->length) {
			cl_error("sg not contiguous:dma 0x%llx, iova 0x%llx\n",
				 sg_dma_address(sg),
				 (u64)(iova + iova_offset));
			err = -EINVAL;
			break;
		}
	}
	if (err) {
		dma_unmap_sg_attrs(smmu_pdev->dev, sgt->sgl, sgt->nents,
				   DMA_BIDIRECTIONAL, 0);
		sg_free_table(sgt);
		kvfree(sgt);
		p_mem_info->sgt = NULL;
		p_mem_info->smmudev_idx = 0;
		p_mem_info->iommu_mapped = 0;
		goto failed;
	}

	p->physical_address = iova;
	cl_debug(DEBUG_MEM_DETAILED,
		 "phyaddr = 0x%llx, smmu iova = 0x%llx, ents=%d\n",
		 (u64)p_mem_info->pages[0].dma_addr, iova, ents);
failed:
	LOG_EXT();
	return err;
}

/* unmap dma buffer by iommu */
int esc_mods_iommu_dma_unmap_memory(struct mods_client               *client,
				    struct MODS_IOMMU_DMA_MAP_MEMORY *p)
{
	struct MODS_MEM_INFO *p_mem_info;
	int                  err;

	LOG_ENT();

	p_mem_info = (struct MODS_MEM_INFO *)(size_t)p->memory_handle;
	err = mods_smmu_unmap_memory(client, p_mem_info);

	LOG_EXT();
	return err;
}

static int mods_smmu_unmap_memory(struct mods_client   *client,
				  struct MODS_MEM_INFO *p_mem_info)
{
	struct sg_table      *sgt;
	u32                   smmudev_idx;
	int                   err = 0;
	struct mods_smmu_dev *smmu_pdev = NULL;

	LOG_ENT();

	if (unlikely(!p_mem_info)) {
		cl_error("%s nullptr\n", __func__);
		err = -EINVAL;
		goto failed;
	}
	if (p_mem_info->sgt == NULL || !p_mem_info->iommu_mapped) {
		cl_error("smmu buffer is not mapped, handle=0x%llx\n",
			 (u64)p_mem_info);
		err = -EINVAL;
		goto failed;
	}

	smmudev_idx = p_mem_info->smmudev_idx;
	smmu_pdev = get_mods_smmu_device(smmudev_idx);
	if (!smmu_pdev) {
		cl_error("smmu device on index %u is not found\n",
			 smmudev_idx);
		err = -ENODEV;
		goto failed;
	}

	sgt = p_mem_info->sgt;
	dma_unmap_sg_attrs(smmu_pdev->dev, sgt->sgl, sgt->nents,
			   DMA_BIDIRECTIONAL, 0);
	cl_debug(DEBUG_MEM_DETAILED,
		 "smmu: dma_unmap_sg_attrs: %s, iova=0x%llx, pages=%d\n",
		 smmu_pdev->dev_name,
		 p_mem_info->pages[0].dev_addr,
		 p_mem_info->num_pages);
	sg_free_table(sgt);
	kvfree(sgt);
	p_mem_info->sgt = NULL;
	p_mem_info->smmudev_idx = 0;
	p_mem_info->iommu_mapped = 0;

failed:
	LOG_EXT();
	return err;
}
#endif /* MODS_HAS_TEGRA */

#ifdef CONFIG_ARM64
static void clear_contiguous_cache(struct mods_client *client,
				   u64                 virt_start,
				   u64                 phys_start,
				   u32                 size)
{
#ifdef MODS_HAS_TEGRA
	__flush_dcache_area((void *)(size_t)(virt_start), size);
#else
	/* __flush_dcache_area is not exported in upstream kernels */
	u64 end = virt_start + size;
	u64 cur;
	u32 d_line_shift = 4; /* Fallback for kernel 5.9 or older */
	u64 d_size;

#ifdef MODS_HAS_ARM64_READ_FTR_REG
	{
		const u64 ctr_el0 = read_sanitised_ftr_reg(SYS_CTR_EL0);

		d_line_shift = cpuid_feature_extract_unsigned_field(ctr_el0,
							CTR_DMINLINE_SHIFT);
	}
#endif

	d_size = 4 << d_line_shift;
	cur = virt_start & ~(d_size - 1);
	do {
		asm volatile("dc civac, %0" : : "r" (cur) : "memory");
	} while (cur += d_size, cur < end);
#endif

	cl_debug(DEBUG_MEM_DETAILED,
		 "clear cache virt 0x%llx phys 0x%llx size 0x%x\n",
		 virt_start, phys_start, size);
}

static void clear_entry_cache_mappings(struct mods_client    *client,
				       struct SYS_MAP_MEMORY *p_map_mem,
				       u64                    virt_offs,
				       u64                    virt_offs_end)
{
	struct MODS_MEM_INFO *p_mem_info = p_map_mem->p_mem_info;
	u64	     cur_vo = p_map_mem->virtual_addr;
	unsigned int i;

	if (!p_mem_info)
		return;

	if (p_mem_info->cache_type != MODS_ALLOC_CACHED)
		return;

	for (i = 0; i < p_mem_info->num_chunks; i++) {
		struct MODS_PHYS_CHUNK *chunk = &p_mem_info->pages[i];
		u32 chunk_offs     = 0;
		u32 chunk_offs_end = PAGE_SIZE << chunk->order;
		u64 cur_vo_end     = cur_vo + chunk_offs_end;

		if (virt_offs_end <= cur_vo)
			break;

		if (virt_offs >= cur_vo_end) {
			cur_vo = cur_vo_end;
			continue;
		}

		if (cur_vo < virt_offs)
			chunk_offs = (u32)(virt_offs - cur_vo);

		if (virt_offs_end < cur_vo_end)
			chunk_offs_end -= (u32)(cur_vo_end - virt_offs_end);

		cl_debug(DEBUG_MEM_DETAILED,
			 "clear cache %p [%u]\n",
			 p_mem_info,
			 i);

		while (chunk_offs < chunk_offs_end) {
			u32 i_page     = chunk_offs >> PAGE_SHIFT;
			u32 page_offs  = chunk_offs - (i_page << PAGE_SHIFT);
			u64 page_va    =
			    (u64)(size_t)kmap(chunk->p_page + i_page);
			u64 clear_va   = page_va + page_offs;
			u64 clear_pa   = MODS_DMA_TO_PHYS(chunk->dma_addr)
							  + chunk_offs;
			u32 clear_size = PAGE_SIZE - page_offs;
			u64 remaining  = chunk_offs_end - chunk_offs;

			if (likely(page_va)) {
				if ((u64)clear_size > remaining)
					clear_size = (u32)remaining;

				cl_debug(DEBUG_MEM_DETAILED,
					 "clear page %u, chunk offs 0x%x, page va 0x%llx\n",
					 i_page,
					 chunk_offs,
					 page_va);

				clear_contiguous_cache(client,
						       clear_va,
						       clear_pa,
						       clear_size);

				kunmap((void *)(size_t)page_va);
			} else
				cl_error("kmap failed\n");

			chunk_offs += clear_size;
		}

		cur_vo = cur_vo_end;
	}
}

int esc_mods_flush_cpu_cache_range(struct mods_client                *client,
				   struct MODS_FLUSH_CPU_CACHE_RANGE *p)
{
	struct list_head *head;
	struct list_head *iter;
	int               err;

	LOG_ENT();

	if (irqs_disabled() || in_interrupt() ||
	    p->virt_addr_start > p->virt_addr_end) {

		cl_debug(DEBUG_MEM_DETAILED, "cannot flush cache\n");
		LOG_EXT();
		return -EINVAL;
	}

	if (p->flags == MODS_INVALIDATE_CPU_CACHE) {
		cl_debug(DEBUG_MEM_DETAILED, "cannot invalidate cache\n");
		LOG_EXT();
		return -EINVAL;
	}

	err = mutex_lock_interruptible(&client->mtx);
	if (unlikely(err)) {
		LOG_EXT();
		return err;
	}

	head = &client->mem_map_list;

	list_for_each(iter, head) {
		struct SYS_MAP_MEMORY *p_map_mem
			= list_entry(iter, struct SYS_MAP_MEMORY, list);

		u64 mapped_va = p_map_mem->virtual_addr;

		/* Note: mapping end points to the first address of next range*/
		u64 mapping_end = mapped_va + p_map_mem->mapping_length;

		int start_on_page = p->virt_addr_start >= mapped_va
				    && p->virt_addr_start < mapping_end;
		int start_before_page = p->virt_addr_start < mapped_va;
		int end_on_page = p->virt_addr_end >= mapped_va
				  && p->virt_addr_end < mapping_end;
		int end_after_page = p->virt_addr_end >= mapping_end;
		u64 virt_start = p->virt_addr_start;

		/* Kernel expects end to point to the first address of next
		 * range
		 */
		u64 virt_end = p->virt_addr_end + 1;

		if ((start_on_page || start_before_page)
			&& (end_on_page || end_after_page)) {

			if (!start_on_page)
				virt_start = p_map_mem->virtual_addr;
			if (!end_on_page)
				virt_end = mapping_end;
			clear_entry_cache_mappings(client,
						   p_map_mem,
						   virt_start,
						   virt_end);
		}
	}
	mutex_unlock(&client->mtx);

	LOG_EXT();
	return OK;
}
#endif /* CONFIG_ARM64 */

static int mods_post_alloc(struct mods_client     *client,
			   struct MODS_PHYS_CHUNK *chunk,
			   u64                     phys_addr,
			   struct MODS_MEM_INFO   *p_mem_info)
{
	int err = 0;

	if ((p_mem_info->cache_type != MODS_ALLOC_CACHED) && !chunk->wc) {
		u32 num_pages = 1U << chunk->order;
		u32 i;

		for (i = 0; i < num_pages; i++) {
			void *ptr;

			ptr = kmap(chunk->p_page + i);
			if (unlikely(!ptr)) {
				cl_error("kmap failed\n");
				return -ENOMEM;
			}
#if defined(MODS_HAS_TEGRA) && !defined(CONFIG_CPA)
			clear_contiguous_cache(client,
					       (u64)(size_t)ptr,
					       phys_addr + (i << PAGE_SHIFT),
					       PAGE_SIZE);
#else
			if (p_mem_info->cache_type == MODS_ALLOC_WRITECOMBINE)
				err = MODS_SET_MEMORY_WC((unsigned long)ptr, 1);
			else
				err = MODS_SET_MEMORY_UC((unsigned long)ptr, 1);
#endif
			kunmap(ptr);
			if (unlikely(err)) {
				cl_error("set cache type failed\n");
				return err;
			}

			/* Set this flag early, so that when an error occurs,
			 * mods_free_pages() will restore cache attributes
			 * for all pages.  It's OK to restore cache attributes
			 * even for chunks where we haven't change them.
			 */
			chunk->wc = 1;
		}
	}

#ifdef CONFIG_PCI
	if (p_mem_info->dev) {
		struct pci_dev *dev = p_mem_info->dev;

		/* On systems with SWIOTLB active, disable default DMA mapping
		 * because we don't support scatter-gather lists.
		 */
#if defined(CONFIG_SWIOTLB) && defined(MODS_HAS_DMA_OPS)
		const struct dma_map_ops *ops = get_dma_ops(&dev->dev);

		if (ops->map_sg == swiotlb_map_sg_attrs)
			return 0;
#endif
		err = mods_dma_map_default_page(client, chunk, dev);
	}
#endif

	return err;
}
