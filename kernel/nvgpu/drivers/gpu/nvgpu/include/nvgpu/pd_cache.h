/*
 * Copyright (c) 2018-2021, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef NVGPU_PD_CACHE_H
#define NVGPU_PD_CACHE_H

/**
 * @file
 *
 * Page directory cache interface.
 */


#include <nvgpu/types.h>

struct gk20a;
struct vm_gk20a;
struct nvgpu_mem;
struct gk20a_mmu_level;

/**
 * GMMU page directory. This is the kernel's tracking of a list of PDEs or PTEs
 * in the GMMU. PD size here must be at least 4096 bytes, but lower tier PDs
 * can be sub-4K aligned. Although lower PDE tables can be aligned at 256B
 * boundaries the PDB must be 4K aligned.
 */
struct nvgpu_gmmu_pd {
	/**
	 * DMA memory describing the PTEs or PDEs.
	 */
	struct nvgpu_mem	*mem;
	/**
	 * Describes the offset of the PDE table in @mem.
	 */
	u32			 mem_offs;
	/**
	 * This PD is using pd_cache memory if this flag is set to true.
	 */
	bool			 cached;
	/**
	 * PD size here must be at least 4096 bytes, but lower tier PDs can be
	 * sub-4K aligned.
	 */
	u32			 pd_size;

	/**
	 * List of pointers to the next level of page tables. Does not
	 * need to be populated when this PD is pointing to PTEs.
	 */
	struct nvgpu_gmmu_pd	*entries;
	/**
	 * Number of entries in a PD is easy to compute from the number of bits
	 * used to index the page directory. That is simply 2 raised to the
	 * number of bits.
	 */
	u32			 num_entries;
};

/**
 * @brief Allocates the DMA memory for a page directory.
 *
 * @param vm	[in]	Pointer to virtual memory structure.
 * @param pd	[in]	Pointer to pd_cache memory structure.
 * @param bytes	[in]	PD size.
 *
 * Allocates a page directory:
 * Allocates the DMA memory for a page directory.
 * This handles the necessary PD cache logistics. Since Parker and
 *  later GPUs, some of the page  directories are smaller than a page.
 *  Hence, packing these PDs together saves a lot of memory.
 * If PD is bigger than a page just do a regular DMA alloc.
 * #nvgpu_pd_cache_alloc_direct() does the pd cache allocation.
 *
 *
 * @return	0 in case of success.
 * @retval	-ENOMEM in case of failure. Reasons can be any one
 * 		of the following
 * 		--kzalloc failure.
 * 		--failures internal to dma alloc* functions.
 *
 */
int  nvgpu_pd_alloc(struct vm_gk20a *vm, struct nvgpu_gmmu_pd *pd, u32 bytes);

/**
 * @brief Free the DMA memory allocated using nvgpu_pd_alloc().
 *
 * @param vm	[in]	Pointer to virtual memory structure.
 * @param pd	[in]	Pointer to pd_cache memory structure.
 *
 * Free the Page Directory DMA memory:
 * Free the DMA memory allocated using nvgpu_pd_alloc by
 *  calling #nvgpu_pd_cache_free_direct().
 * Call #nvgpu_pd_cache_free() if the pd is cached.
 *
 * @return	None
 */
void nvgpu_pd_free(struct vm_gk20a *vm, struct nvgpu_gmmu_pd *pd);

/**
 * @brief Initializes the pd_cache tracking stuff.
 *
 * @param g	[in]	The GPU.
 *
 * Initialize the pd_cache:
 * Allocates the zero initialized memory area for #nvgpu_pd_cache.
 * Initializes the mutexes and list nodes for pd_cache tracking stuff.
 * Make sure not to reinitialize the pd_cache again by initilalizing
 *  mm.pd_cache.
 *
 * @return	0 in case of success.
 * @retval	-ENOMEM in case of kzalloc failure.
 */
int  nvgpu_pd_cache_init(struct gk20a *g);

/**
 * @brief Free the pd_cache tracking stuff allocated by nvgpu_pd_cache_init().
 *
 * @param g	[in]	The GPU.
 *
 * Free the pd_cache:
 * Reset the list nodes used for pd_cache tracking stuff.
 * Free the #nvgpu_pd_cache internal structure allocated
 *  by nvgpu_pd_cache_init().
 * Reset the mm.pd_cache to NULL.
 *
 * @return	None
 */
void nvgpu_pd_cache_fini(struct gk20a *g);

/**
 * @brief Compute the pd offset for GMMU programming.
 *
 * @param l		[in]	Structure describes the GMMU level
 *				entry format which is used for GMMU mapping
 *				understandable by GMMU H/W.
 * @param pd_idx	[in]	Index into the page size table.
 *				- Min: GMMU_PAGE_SIZE_SMALL
 *				- Max: GMMU_PAGE_SIZE_KERNEL
 *
 * Compute the pd offset:
 * ((@pd_idx * GMMU level entry size / 4).
 *
 * @return	pd offset at \a pd_idx.
 *
 */
u32  nvgpu_pd_offset_from_index(const struct gk20a_mmu_level *l, u32 pd_idx);

/**
 * @brief Write data content into pd mem.
 *
 * @param g	[in]	The GPU.
 * @param pd	[in]	Pointer to GMMU page directory structure.
 * @param w	[in]	Word offset from the start of the pd mem.
 * @param data	[in]	Data to write into pd mem.
 *
 * Write data content into pd mem:
 * Offset = ((start address of the pd / 4 + @w).
 * Write data content into offset address by calling #nvgpu_mem_wr32().
 *
 * @return	None
 */
void nvgpu_pd_write(struct gk20a *g, struct nvgpu_gmmu_pd *pd,
		    size_t w, u32 data);

/**
 * @brief Return the _physical_ address of a page directory.
 *
 * @param g	[in]	The GPU.
 * @param pd	[in]	Pointer to GMMU page directory structure.
 *
 * Write data content into pd mem:
 * Return the _physical_ address of a page directory for GMMU programming.
 * PD base in context inst block.
 * #nvgpu_mem_get_addr returns the _physical_ address of pd mem.
 *
 * @return	pd physical address in case of valid pd mem.
 * @retval	Zero in case of invalid/random pd mem.
 */
u64  nvgpu_pd_gpu_addr(struct gk20a *g, struct nvgpu_gmmu_pd *pd);

/**
 * @brief Allocate memory for a page directory.
 *
 * @param g	[in]	The GPU.
 * @param pd	[in]	Pointer to GMMU page directory structure.
 *
 * - Set NVGPU_DMA_PHYSICALLY_ADDRESSED if \a bytes is more than
 *   NVGPU_CPU_PAGE_SIZE.
 * - Call #nvgpu_dma_alloc_flags() to allocate dmaable memory for
 *   pd.
 *
 * @return	Zero For succcess.
 * @retval	-ENOMEM For any allocation failure.
 */
int nvgpu_pd_cache_alloc_direct(struct gk20a *g,
				struct nvgpu_gmmu_pd *pd, u32 bytes);

#endif
