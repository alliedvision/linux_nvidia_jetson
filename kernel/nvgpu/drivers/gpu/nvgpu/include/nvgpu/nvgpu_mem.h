/*
 * Copyright (c) 2017-2022, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef NVGPU_MEM_H
#define NVGPU_MEM_H

#include <nvgpu/types.h>
#include <nvgpu/list.h>
#include <nvgpu/enabled.h>
#include <nvgpu/gmmu.h>

#ifdef __KERNEL__
#include <nvgpu/linux/nvgpu_mem.h>
#elif defined(__NVGPU_POSIX__)
#include <nvgpu/posix/nvgpu_mem.h>
#else
#include <nvgpu_rmos/include/nvgpu_mem.h>
#endif

/**
 * Memory Interface for all GPU accessible memory.
 */

struct page;
struct sg_table;
struct gk20a;
struct nvgpu_allocator;
struct nvgpu_gmmu_attrs;
struct nvgpu_page_alloc;
struct nvgpu_sgt;

/**
 * The nvgpu_mem structure defines abstracted GPU accessible memory regions.
 */
struct nvgpu_mem {
	/**
	 * Indicates memory type of original allocation.
	 */
	enum nvgpu_aperture			 aperture;
	/**
	 * Size of memory segment requested during creation.
	 */
	size_t					 size;
	/**
	 * Total amount of memory allocated after aligning requested size.
	 */
	size_t					 aligned_size;
	/**
	 * Address of mapped GPU memory, if any.
	 */
	u64					 gpu_va;
	/**
	 * Flag to indicate write memory barrier requirement.
	 */
	bool					 skip_wmb;
	/**
	 * Indicates if gpu_va address is valid.
	 */
	bool					 free_gpu_va;

	/**
	 * Set when a nvgpu_mem struct is not a "real" nvgpu_mem struct. Instead
	 * the struct is just a copy of another nvgpu_mem struct.
	 */
#define NVGPU_MEM_FLAG_SHADOW_COPY		 BIT64(0)

	/**
	 * Specify that the GVA mapping is a fixed mapping - that is the caller
	 * chose the GPU VA, not the GMMU mapping function. Only relevant for
	 * VIDMEM.
	 */
#define NVGPU_MEM_FLAG_FIXED			 BIT64(1)

	/**
	 * Set for user generated VIDMEM allocations. This triggers a special
	 * cleanup path that clears the vidmem on free. Given that the VIDMEM is
	 * zeroed on boot this means that all user vidmem allocations are
	 * therefor zeroed (to prevent leaking information in VIDMEM buffers).
	 */
#define NVGPU_MEM_FLAG_USER_MEM			 BIT64(2)

	/**
	 * Internal flag that specifies this struct has not been made with DMA
	 * memory and as a result should not try to use the DMA routines for
	 * freeing the backing memory.
	 *
	 * However, this will not stop the DMA API from freeing other parts of
	 * nvgpu_mem in a system specific way.
	 */
#define NVGPU_MEM_FLAG_NO_DMA			 BIT64(3)

	/**
	 * Some nvgpu_mem objects act as facades to memory buffers owned by
	 * someone else. This internal flag specifies that the sgt field is
	 * "borrowed", and it must not be freed by us.
	 *
	 * Of course the caller will have to make sure that the sgt owner
	 * outlives the nvgpu_mem.
	 */
#define NVGPU_MEM_FLAG_FOREIGN_SGT		 BIT64(4)

	/**
	 * Store flag bits indicating conditions for nvgpu_mem struct instance.
	 */
	unsigned long				 mem_flags;

	/**
	 * Pointer to sysmem memory address. Only populated for a sysmem
	 * allocation.
	 */
	void					*cpu_va;

#ifdef CONFIG_NVGPU_DGPU

	/**
	 * Pointer to allocated chunks of pages constituting requested vidmem
	 * type memory. This memory is allocated from GPU vidmem memory.
	 * Evidently, only populated for vidmem allocations.
	 */
	struct nvgpu_page_alloc			*vidmem_alloc;

	/**
	 * Pointer to GPU vidmem allocator. This allocator is used to allocate
	 * memory pointed by #vidmem_alloc. Only populated for vidmem
	 * allocations.
	 */
	struct nvgpu_allocator			*allocator;

	/**
	 * Clear list entry node. This node is used to register this nvgpu_mem
	 * vidmem instance in GPU vidmem allocations list. This node can be used
	 * to search outstanding vidmem allocations. Only populated for vidmem
	 * allocations.
	 */
	struct nvgpu_list_node			 clear_list_entry;
#endif

	/**
	 * Pointer to scatter gather table for direct "physical" nvgpu_mem
	 * structs.
	 */
	struct nvgpu_sgt			*phys_sgt;

	/**
	 * Structure containing system specific memory pointers.
	 * This is defined by the system specific header. It can be empty if
	 * there's no system specific stuff for a given system.
	 */
	struct nvgpu_mem_priv			 priv;
};

#ifdef CONFIG_NVGPU_DGPU
static inline struct nvgpu_mem *
nvgpu_mem_from_clear_list_entry(struct nvgpu_list_node *node)
{
	return (struct nvgpu_mem *)
		((uintptr_t)node - offsetof(struct nvgpu_mem,
					    clear_list_entry));
};
#endif

/**
 * @brief Convert aperture type to string.
 *
 * @param[in] aperture		Aperture type.
 *
 * @return Aperture type string.
 */
static inline const char *nvgpu_aperture_str(enum nvgpu_aperture aperture)
{
	const char *aperture_name[APERTURE_MAX_ENUM + 1] = {
		[APERTURE_INVALID]	= "INVAL",
		[APERTURE_SYSMEM]	= "SYSTEM",
		[APERTURE_SYSMEM_COH]	= "SYSCOH",
		[APERTURE_VIDMEM]	= "VIDMEM",
		[APERTURE_MAX_ENUM]	= "UNKNOWN",
	};

	if ((aperture < APERTURE_INVALID) || (aperture >= APERTURE_MAX_ENUM)) {
		return aperture_name[APERTURE_MAX_ENUM];
	}
	return aperture_name[aperture];
}

/**
 * @brief Check if given aperture is of type SYSMEM.
 *
 * @param[in] ap	Aperture type.
 *
 * @return True if aperture is SYSMEM type, false otherwise.
 */
bool nvgpu_aperture_is_sysmem(enum nvgpu_aperture ap);

/**
 * @brief Check if given memory is of SYSMEM type.
 *
 * @param[in] mem	Pointer to nvgpu_mem structure.
 *
 * @return True if memory is SYSMEM type, false otherwise.
 */
bool nvgpu_mem_is_sysmem(struct nvgpu_mem *mem);

/**
 * @brief Check if given nvgpu_mem structure is valid for subsequent use.
 *
 * @param[in] mem	Pointer to nvgpu_mem structure.
 *			Cannot be NULL.
 *
 * @return True if the passed nvgpu_mem has been allocated, false otherwise.
 */
static inline bool nvgpu_mem_is_valid(struct nvgpu_mem *mem)
{
	/*
	 * Internally the DMA APIs must set/unset the aperture flag when
	 * allocating/freeing the buffer. So check that to see if the *mem
	 * has been allocated or not.
	 *
	 * This relies on mem_descs being zeroed before being initialized since
	 * APERTURE_INVALID is equal to 0.
	 */
	return mem->aperture != APERTURE_INVALID;

}

/**
 * @brief Create a new nvgpu_mem struct from an old one.
 *
 * @param[in] g			Pointer to GPU structure
 * @param[out] dest		Pointer to destination nvgpu_mem to hold
 *				resulting memory description.
 * @param[in] src		Pointer to source memory.
 *				Cannot be NULL. Must be valid.
 * @param[in] start_page	Start page of created memory.
 * @param[in] nr_pages		Number of pages to place in the new nvgpu_mem.
 *
 * Create a new nvgpu_mem struct describing a subsection of the @src nvgpu_mem.
 * This will create an nvpgu_mem object starting at @start_page and is @nr_pages
 * long. This currently only works on SYSMEM nvgpu_mems. If this is called on a
 * VIDMEM nvgpu_mem then this will return an error.
 *
 * There is a _major_ caveat to this API: if the source buffer is freed before
 * the copy is freed then the copy will become invalid. This is a result from
 * how typical DMA APIs work: we can't call free on the buffer multiple times.
 * Nor can we call free on parts of a buffer. Thus the only way to ensure that
 * the entire buffer is actually freed is to call free once on the source
 * buffer. Since these nvgpu_mem structs are not ref-counted in anyway it is up
 * to the caller of this API to _ensure_ that the resulting nvgpu_mem buffer
 * from this API is freed before the source buffer. Otherwise there can and will
 * be memory corruption.
 *
 * The resulting nvgpu_mem should be released with the nvgpu_dma_free() or the
 * nvgpu_dma_unmap_free() function depending on whether or not the resulting
 * nvgpu_mem has been mapped.
 *
 * @return 0 in case of success, < 0 in case of failure.
 * @retval -EINVAl in case of incorrect input values.
 * @retval -ENOMEM in case there is not enough memory for scatter gather table
 * creation.
 */
int nvgpu_mem_create_from_mem(struct gk20a *g,
			      struct nvgpu_mem *dest, struct nvgpu_mem *src,
			      u64 start_page, size_t nr_pages);

/**
 * @brief Create an nvgpu_mem structure from given physical memory.
 *
 * @param[in] g			Pointer to GPU structure.
 * @param[out] dest		Memory pointer to be populated after mem create.
 * @param[in] src_phys		Start address of physical memory.
 * @param[in] nr_pages		Number of pages requested.
 *
 * Create a new nvgpu_mem struct from a physical memory aperture. The physical
 * memory aperture needs to be contiguous for requested @nr_pages. This API
 * only works for SYSMEM. This also assumes a 4K page granule since the GMMU
 * always supports 4K pages. If _system_ pages are larger than 4K then the
 * resulting nvgpu_mem will represent less than 1 OS page worth of memory
 *
 * The resulting nvgpu_mem should be released with the nvgpu_dma_free() or the
 * nvgpu_dma_unmap_free() function depending on whether or not the resulting
 * nvgpu_mem has been mapped.
 *
 * @return 0 in case of success, < 0 in case of failure.
 * @retval -ENOMEM in case there is not enough memory to allocate agt and agl.
 */
int nvgpu_mem_create_from_phys(struct gk20a *g, struct nvgpu_mem *dest,
			       u64 src_phys, u64 nr_pages);

#ifdef CONFIG_NVGPU_DGPU
/**
 * @brief Free vidmem buffer.
 *
 * @param[in] g			Pointer to GPU structure.
 * @param[in] vidmem		Pointer to nvgou_mem structure to be freed.
 *
 * Really free a vidmem buffer. There's a fair amount of work involved in
 * freeing vidmem buffers in the DMA API. This handles none of that - it only
 * frees the underlying vidmem specific structures used in vidmem buffers.
 *
 * This is implemented in the OS specific code. If it's not necessary it can
 * be a noop. But the symbol must at least be present.
 */
void nvgpu_mem_free_vidmem_alloc(struct gk20a *g, struct nvgpu_mem *vidmem);
#endif

/*
 * Buffer accessors. Sysmem buffers always have a CPU mapping and vidmem
 * buffers are accessed via PRAMIN.
 */

/**
 * @brief Read data word from memory.
 *
 * @param[in] g		Pointer to GPU structure.
 * @param[in] mem	Pointer to nvgpu_mem structure.
 * @param[in] w		Word index.
 *
 * @return Data word read from memory.
 * @retval 0 in case of invalid #mem.
 */
u32 nvgpu_mem_rd32(struct gk20a *g, struct nvgpu_mem *mem, u64 w);

/**
 * @brief Read two data words from memory.
 *
 * @param[in] g         Pointer to GPU structure.
 * @param[in] mem       Pointer to nvgpu_mem structure.
 * @param[in] lo	Low word index.
 * @param[in] hi	High word index.
 *
 * @return Double data word read from memory.
 * @retval 0 in case of invalid #mem.
 */
u64 nvgpu_mem_rd32_pair(struct gk20a *g, struct nvgpu_mem *mem,
		u32 lo, u32 hi);

/**
 * @brief Read data word from memory containing data at given byte offset.
 *
 * @param[in] g         Pointer to GPU structure.
 * @param[in] mem       Pointer to nvgpu_mem structure.
 * @param[in] offset	Byte offset (32b-aligned).
 *
 * @return Data word read from memory.
 * @retval 0 in case of invalid #mem.
 */
u32 nvgpu_mem_rd(struct gk20a *g, struct nvgpu_mem *mem, u64 offset);

/**
 * @brief Copy size amount bytes from memory to cpu.
 *
 * @param[in] g         Pointer to GPU structure.
 * @param[in] mem       Pointer to nvgpu_mem structure.
 * @param[in] offset    Byte offset (32b-aligned).
 * @param[out] dest	Pointer to destination cpu memory.
 * @param[in] size	Number of bytes to be read (32b-aligned).
 */
void nvgpu_mem_rd_n(struct gk20a *g, struct nvgpu_mem *mem, u64 offset,
		void *dest, u64 size);

/**
 * @brief Write data word to memory.
 *
 * @param[in] g		Pointer to GPU structure.
 * @param[in] mem	Pointer to nvgpu_mem structure.
 * @param[in] w		Word index.
 * @param[in] data	Data word to be written.
 */
void nvgpu_mem_wr32(struct gk20a *g, struct nvgpu_mem *mem, u64 w, u32 data);

/**
 * @brief Write data word to memory.
 *
 * @param[in] g		Pointer to GPU structure.
 * @param[in] mem	Pointer to nvgpu_mem structure.
 * @param[in] w		Word index.
 * @param[in] data	Data word to be written.
 */
void nvgpu_mem_wr(struct gk20a *g, struct nvgpu_mem *mem, u64 offset, u32 data);

/**
 * @brief Copy size amount bytes from cpu to memory.
 *
 * @param[in] g         Pointer to GPU structure.
 * @param[in] mem       Pointer to nvgpu_mem structure.
 * @param[in] offset    Byte offset (32b-aligned).
 * @param[in] src	Pointer to source cpu memory.
 * @param[in] size	Number of bytes to be written (32b-aligned).
 */
void nvgpu_mem_wr_n(struct gk20a *g, struct nvgpu_mem *mem, u64 offset,
		void *src, u64 size);

/**
 * @brief Fill size amount of memory bytes with constant byte value.
 *
 * @param[in] g         Pointer to GPU structure.
 * @param[in] mem       Pointer to nvgpu_mem structure.
 * @param[in] offset    Byte offset (32b-aligned).
 * @param[in] c		Byte value to be written to memory.
 * @param[in] size	Number of bytes to be read (32b-aligned).
 */
void nvgpu_memset(struct gk20a *g, struct nvgpu_mem *mem, u64 offset,
		u32 c, u64 size);

/**
 * @brief Request memory address.
 *
 * @param[in] g		Pointer to GPU structure.
 * @param[in] mem	Pointer to nvgpu_mem structure.
 *
 * @return sysmem address.
 * @retval 0 in case of invalid #mem.
 */
u64 nvgpu_mem_get_addr(struct gk20a *g, struct nvgpu_mem *mem);

/**
 * @brief Request memory address.
 *
 * @param[in] g		Pointer to GPU structure.
 * @param[in] mem	Pointer to nvgpu_mem structure.
 *
 * @return Sysmem address.
 * @retval 0 in case of invalid #mem.
 */
u64 nvgpu_mem_get_phys_addr(struct gk20a *g, struct nvgpu_mem *mem);

/**
 * @brief Get raw aperture mask value.
 *
 * @param[in] g			Pointer to GPU structure.
 * @param[in] aperture		Aperture value.
 *                              - Min: APERTURE_SYSMEM
 *                              - Max: APERTURE_VIDMEM
 * @param[in] sysmem_mask	Mask for sysmem memory.
 * @param[in] sysmem_coh_mask	Mask for coherent sysmem memory.
 * @param[in] vidmem_mask	Mask for vidmem memory.
 *
 * @return Raw aperture mask value in case of success.
 * @retval 0 in case of invalid #aperture.
 */
u32 nvgpu_aperture_mask_raw(struct gk20a *g, enum nvgpu_aperture aperture,
		u32 sysmem_mask, u32 sysmem_coh_mask, u32 vidmem_mask);

/**
 * @brief Get aperture mask value.
 *
 * @param[in] g			Pointer to GPU structure.
 * @param[in] mem		Pointer to nvgpu_mem structure.
 * @param[in] sysmem_mask	Mask for sysmem memory.
 * @param[in] sysmem_coh_mask	Mask for coherent sysmem memory.
 * @param[in] vidmem_mask	Mask for vidmem memory.
 *
 * Right coherency aperture should be used.
 * This function doesn't add any checks.
 *
 * @return Aperture mask value in case of success.
 * @retval 0 in case of invalid #mem.
 */
u32 nvgpu_aperture_mask(struct gk20a *g, struct nvgpu_mem *mem,
		u32 sysmem_mask, u32 sysmem_coh_mask, u32 vidmem_mask);

/**
 * @brief Get iommu memory address.
 *
 * @param[in] g		Pointer to GPU structure.
 * @param[in] phys	Physical memory address.
 *
 * @return IOMMU translated physical address if GPU MM is iommuable, physical
 * address otherwise.
 */
u64 nvgpu_mem_iommu_translate(struct gk20a *g, u64 phys);

/**
 * @brief Get the physical address associated with the physical nvgpu_mem.
 *
 * @param[in]  g	Pointer to GPU structure.
 * @param[in]  mem	Pointer to nvgpu_mem structure holds the physical
 * 			scatter gather table.
 *
 * This fuction should not be used for normal nvgpumem that holds
 * the sgt of intermediate or iova addresses.
 *
 * @return translated physical address.
 */
u64 nvgpu_mem_phys_get_addr(struct gk20a *g, struct nvgpu_mem *mem);

#endif /* NVGPU_MEM_H */
