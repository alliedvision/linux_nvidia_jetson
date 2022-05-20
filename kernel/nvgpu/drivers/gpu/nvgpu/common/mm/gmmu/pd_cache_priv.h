/*
 * Copyright (c) 2019-2020, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef NVGPU_GMMU_PD_CACHE_PRIV_H
#define NVGPU_GMMU_PD_CACHE_PRIV_H

/**
 * @file
 *
 * Page directory cache private interface
 * --------------------------------------
 *
 * To save memory when using sub-page sized PD levels in Pascal and beyond a way
 * of packing PD tables together is necessary. If a PD table only requires 1024
 * bytes, then it is possible to have 4 of these PDs in one page. This is even
 * more pronounced for 256 byte PD tables.
 *
 * This also matters for page directories on any chip when using a 64K page
 * granule. Having 4K PDs packed into a 64K page saves a bunch of memory. Even
 * more so for the 256B PDs on Pascal+.
 *
 * The pd cache is basially just a slab allocator. Each instance of the nvgpu
 * driver makes one of these structs:
 *
 *   struct nvgpu_pd_cache {
 *      struct nvgpu_list_node		 full[NVGPU_PD_CACHE_COUNT];
 *      struct nvgpu_list_node		 partial[NVGPU_PD_CACHE_COUNT];
 *
 *      struct nvgpu_rbtree_node	*mem_tree;
 *   };
 *
 * There are two sets of lists, the full and the partial. The full lists contain
 * pages of memory for which all the memory in that page is in use. The partial
 * lists contain partially full pages of memory which can be used for more PD
 * allocations. There a couple of assumptions here:
 *
 *   1. PDs greater than or equal to the page size bypass the pd cache.
 *   2. PDs are always power of 2 and greater than %NVGPU_PD_CACHE_MIN bytes.
 *
 * There are NVGPU_PD_CACHE_COUNT full lists and the same number of partial
 * lists. For a 4Kb page NVGPU_PD_CACHE_COUNT is 4. This is enough space for
 * 256, 512, 1024, and 2048 byte PDs.
 *
 * nvgpu_pd_alloc() will allocate a PD for the GMMU. It will check if the PD
 * size is page size or larger and choose the correct allocation scheme - either
 * from the PD cache or directly. Similarly nvgpu_pd_free() will free a PD
 * allocated by nvgpu_pd_alloc().
 */

#include <nvgpu/bug.h>
#include <nvgpu/log.h>
#include <nvgpu/gmmu.h>
#include <nvgpu/nvgpu_mem.h>
#include <nvgpu/list.h>
#include <nvgpu/rbtree.h>
#include <nvgpu/lock.h>

#define pd_dbg(g, fmt, args...) nvgpu_log(g, gpu_dbg_pd_cache, fmt, ##args)

/**
 * Minimum size of a cache. The number of different caches in the nvgpu_pd_cache
 * structure is of course depending on this.
 */
#define NVGPU_PD_CACHE_MIN		256UL
/**
 * MIN_SHIFT is the right number of bits to shift to determine
 * which list to use in the array of lists.
 */
#define NVGPU_PD_CACHE_MIN_SHIFT	9UL

/**
 * Maximum PD cache count. This specifies the number of slabs; since each
 * slab represents a PO2 increase in size a count of 8 leads to:
 *
 *   NVGPU_PD_CACHE_SIZE = 256B * 2^8 = 64KB
 *
 * For Linux with 4K pages, if the cache size is larger than 4KB then we
 * need to allocate from CMA. This puts a lot of pressure on the CMA space.
 * For kernel with a PAGE_SIZE of 64K this isn't the case, so allow the
 * PD cache size to be 64K if PAGE_SIZE > 4K (i.e PAGE_SIZE == 64K).
 */
#ifdef __KERNEL__
#  if NVGPU_CPU_PAGE_SIZE > 4096
#    define NVGPU_PD_CACHE_COUNT	8UL
#  else
#    define NVGPU_PD_CACHE_COUNT	4UL
#  endif
#else
#define NVGPU_PD_CACHE_COUNT		8UL
#endif

#define NVGPU_PD_CACHE_SIZE		(NVGPU_PD_CACHE_MIN * \
						(1UL << NVGPU_PD_CACHE_COUNT))

/**
 * This structure describes a slab within the slab allocator.
 */
struct nvgpu_pd_mem_entry {
	/**
	 * Structure for storing the PD memory information.
	 */
	struct nvgpu_mem		mem;

	/**
	 * Size of the page directories (not the mem).
	 */
	u32				pd_size;
	/**
	 * alloc_map is a bitmap showing which PDs have been allocated.
	 * The size of mem will always
	 * be one page. pd_size will always be a power of 2.
	 */
	DECLARE_BITMAP(alloc_map, NVGPU_PD_CACHE_SIZE / NVGPU_PD_CACHE_MIN);
	/**
	 * Total number of allocations in this PD.
	 */
	u32				allocs;

	/**
	 * This is a list node within the list. The list node will be either from
	 * a full or partial list in #nvgpu_pd_cache.
	 */
	struct nvgpu_list_node		list_entry;
	/**
	 * This is a tree node within the node.
	 */
	struct nvgpu_rbtree_node	tree_entry;
};

/**
 * A cache for allocating PD memory. This enables smaller PDs to be packed
 * into single pages.
 */
struct nvgpu_pd_cache {
	/**
	 * Array of lists of full nvgpu_pd_mem_entries and partially full
	 * nvgpu_pd_mem_entries.
	 */
	struct nvgpu_list_node		 full[NVGPU_PD_CACHE_COUNT];
	/**
	 * Array of lists of empty nvgpu_pd_mem_entries and partially
	 * empty nvgpu_pd_mem_entries.
	 */
	struct nvgpu_list_node		 partial[NVGPU_PD_CACHE_COUNT];

	/**
	 * Tree of all allocated struct nvgpu_mem's for fast look up.
	 */
	struct nvgpu_rbtree_node	*mem_tree;

	/**
	 * All access to the cache much be locked. This protects the lists and
	 * the rb tree.
	 */
	struct nvgpu_mutex		 lock;
};

#endif /* NVGPU_GMMU_PD_CACHE_PRIV_H */
