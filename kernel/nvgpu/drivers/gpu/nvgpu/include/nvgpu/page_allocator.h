/*
 * Copyright (c) 2016-2019, NVIDIA CORPORATION.  All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef PAGE_ALLOCATOR_PRIV_H
#define PAGE_ALLOCATOR_PRIV_H

/**
 * @file
 *
 * Page allocator interface
 * ------------------------
 *
 * This allocator implements the ability to do SLAB style allocation since the
 * GPU has two page sizes available - 4k and 64k/128k. When the default
 * granularity is the large page size (64k/128k) small allocations become very
 * space inefficient. This is most notable in PDE and PTE blocks which are 4k
 * in size.
 *
 * Thus we need the ability to suballocate in 64k pages. The way we do this for
 * the GPU is as follows. We have several buckets for sub-64K allocations:
 *
 *   B0 - 4k
 *   B1 - 8k
 *   B3 - 16k
 *   B4 - 32k
 *   B5 - 64k (for when large pages are 128k)
 *
 * When an allocation comes in for less than the large page size (from now on
 * assumed to be 64k) the allocation is satisfied by one of the buckets.
 */

#ifdef CONFIG_NVGPU_DGPU

#include <nvgpu/allocator.h>
#include <nvgpu/nvgpu_mem.h>
#include <nvgpu/nvgpu_sgt.h>
#include <nvgpu/kmem.h>
#include <nvgpu/list.h>
#include <nvgpu/rbtree.h>

struct nvgpu_allocator;

/**
 * Structure to identify slab allocations.
 */
struct page_alloc_slab {
	/**
	 * List of empty or unallocated pages.
	 */
	struct nvgpu_list_node empty;
	/**
	 * List of partially allocated pages.
	 */
	struct nvgpu_list_node partial;
	/**
	 * List of completely allocated pages.
	 */
	struct nvgpu_list_node full;

	/**
	 * Number of slab pages in empty pages list.
	 */
	u32 nr_empty;
	/**
	 * Number of slab pages in partial pages list.
	 */
	u32 nr_partial;
	/**
	 * Number of slab pages in full pages list.
	 */
	u32 nr_full;
	/**
	 * As slab_size is 32-bits wide,
	 * Maximum possible slab_size is 2^32 i.e. 4Gb.
	 * Slab_size starts from 4K (i.e 4k, 8k, 16k, 32k).
	 */
	u32 slab_size;
};

/**
 * This flag variable designates the state of the slab page.
 */
enum slab_page_state {
	SP_EMPTY,
	SP_PARTIAL,
	SP_FULL,
	SP_NONE
};

/**
 * Structure to describe slab page.
 */
struct page_alloc_slab_page {
	/**
	 * Bitmap to identify status of slab_page.
	 */
	unsigned long bitmap;
	/**
	 * Address of slab page.
	 */
	u64 page_addr;
	/**
	 * As slab_size is 32-bits wide,
	 * Maximum possible slab_size is 2^32 i.e. 4Gb.
	 * Slab_size starts from 4K (i.e 4k, 8k, 16k, 32k).
	 */
	u32 slab_size;

	/**
	 * Total number of objects that can be allocated in this slab.
	 */
	u32 nr_objects;
	/**
	 * Number of allocated objects in the slab page.
	 */
	u32 nr_objects_alloced;

	/**
	 * The state of the slab page (i.e SP_EMPTY, SP_FULL, ...).
	 */
	enum slab_page_state state;

	/**
	 * Parent slab of this page.
	 */
	struct page_alloc_slab *owner;
	/**
	 * List node, used to add this page to slab lists.
	 */
	struct nvgpu_list_node list_entry;
};

/**
 * @brief Get page from slab list.
 *
 * @param node [in] Pointer to slab list head.
 *
 * @return Pointer to slab page structure.
 */
static inline struct page_alloc_slab_page *
page_alloc_slab_page_from_list_entry(struct nvgpu_list_node *node)
{
	return (struct page_alloc_slab_page *)
	((uintptr_t)node - offsetof(struct page_alloc_slab_page, list_entry));
};

/**
 * Struct to handle internal management of page allocation. It holds a list
 * of the chunks of pages that make up the overall allocation - much like a
 * scatter gather table.
 */
struct nvgpu_page_alloc {
	/**
	 * nvgpu_sgt for describing the actual allocation. Convenient for
	 * GMMU mapping.
	 */
	struct nvgpu_sgt sgt;

	/**
	 * Number of chunks allocated.
	 */
	int nr_chunks;
	/**
	 * Length of allocation.
	 */
	u64 length;

	/**
	 * Base address of the first allocated page.
	 */
	u64 base;

	/**
	 * Tree of outstanding allocations.
	 */
	struct nvgpu_rbtree_node tree_entry;

	/**
	 * Pointer to the slab page that owns this particular allocation.
	 */
	struct page_alloc_slab_page *slab_page;
};

/**
 * @brief Get allocation page from rbtree.
 *
 * @param node [in] Pointer to rbtree node.
 *
 * @return Pointer to allocation page structure.
 */
static inline struct nvgpu_page_alloc *
nvgpu_page_alloc_from_rbtree_node(struct nvgpu_rbtree_node *node)
{
	return (struct nvgpu_page_alloc *)
	      ((uintptr_t)node - offsetof(struct nvgpu_page_alloc, tree_entry));
};

/**
 * Structure describing page allocator.
 */
struct nvgpu_page_allocator {
	/**
	 * Pointer to generic #nvgpu_allocator which is the owner of this
	 * page_allocator.
	 */
	struct nvgpu_allocator *owner;

	/**
	 * Use a buddy allocator to manage the allocation of the underlying
	 * pages. This lets us abstract the discontiguous allocation handling
	 * out of the annoyingly complicated buddy allocator.
	 */
	struct nvgpu_allocator source_allocator;

	/**
	 * Base address of the page allocator.
	 */
	u64 base;
	/**
	 * Size of the pool managed by this page allocator.
	 */
	u64 length;
	/**
	 * Page size of page allocator.
	 */
	u64 page_size;
	/**
	 * Log2 value of \a page_size of page allocator.
	 */
	u32 page_shift;

	/**
	 * RBtree list of outstanding allocations.
	 */
	struct nvgpu_rbtree_node *allocs;

	/**
	 * Pointer to slabs array.
	 */
	struct page_alloc_slab *slabs;
	/**
	 * Number of slabs in the slabs array.
	 */
	u32 nr_slabs;

	/**
	 * Pointer to kmem cache memory structure. This cache is for
	 * #nvgpu_page_alloc structure sized allocations.
	 */
	struct nvgpu_kmem_cache *alloc_cache;
	/**
	 * Pointer to kmem cache memory structure. This cache is for
	 * #nvgpu_page_alloc structure sized allocations.
	 */
	struct nvgpu_kmem_cache *slab_page_cache;

	/**
	 * Additional flags for page allocator.
	 * (i.e GPU_ALLOC_4K_VIDMEM_PAGES, GPU_ALLOC_FORCE_CONTIG, ...).
	 */
	u64 flags;

	/**
	 * Number of generic page allocations.
	 */
	u64 nr_allocs;
	/**
	 * Number of generic pages freed.
	 */
	u64 nr_frees;
	/**
	 * Number of fixed page allocations.
	 */
	u64 nr_fixed_allocs;
	/**
	 * Number of fixed pages freed.
	 */
	u64 nr_fixed_frees;
	/**
	 * Number of slabs allocated.
	 */
	u64 nr_slab_allocs;
	/**
	 * Number of slabs freed.
	 */
	u64 nr_slab_frees;
	/**
	 * Number of pages allocated.
	 */
	u64 pages_alloced;
	/**
	 * Number of pages freed.
	 */
	u64 pages_freed;
};

/**
 * @brief Get #nvgpu_page_allocator pointer from generic #nvgpu_allocator
 * structure.
 *
 * @param a [in] Pointer to top-level allocator structure.
 *
 * @return Page allocator pointer.
 */
static inline struct nvgpu_page_allocator *page_allocator(
	struct nvgpu_allocator *a)
{
	return (struct nvgpu_page_allocator *)(a)->priv;
}

/**
 * @brief Get #nvgpu_allocator pointer from #nvgpu_page_allocator structure.
 *
 * @param a [in] Pointer to page allocator structure.
 *
 * @return Pointer to generic allocator.
 */
static inline struct nvgpu_allocator *palloc_owner(
	struct nvgpu_page_allocator *a)
{
	return a->owner;
}

#endif
#endif
