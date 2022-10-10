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

#ifndef NVGPU_GMMU_H
#define NVGPU_GMMU_H

/**
 * @file
 *
 * GMMU interface.
 */

#include <nvgpu/types.h>
#include <nvgpu/list.h>
#include <nvgpu/rbtree.h>
#include <nvgpu/lock.h>

/**
 * This is the GMMU API visible to blocks outside of the GMMU. Basically this
 * API supports all the different types of mappings that might be done in the
 * GMMU.
 */

struct gk20a;
struct vm_gk20a;
struct nvgpu_mem;
struct nvgpu_sgt;
struct nvgpu_gmmu_pd;
struct vm_gk20a_mapping_batch;

/**
 * Small page size (4KB) index in the page size table
 */
#define GMMU_PAGE_SIZE_SMALL	0U

/**
 * Big page size (64KB) index in the page size table
 */
#define GMMU_PAGE_SIZE_BIG	1U

/**
 * Kernel page size index in the page size table
 */
#define GMMU_PAGE_SIZE_KERNEL	2U

/**
 * Maximum number of page size index in the page size table
 */
#define GMMU_NR_PAGE_SIZES	3U

/**
 * This flag variable designates where the memory actually was allocated from.
 */
enum nvgpu_aperture {
	/**
	 * Unallocated or invalid memory structure.
	 */
	APERTURE_INVALID = 0,
	/**
	 * This memory is located in SYSMEM.
	 */
	APERTURE_SYSMEM,
	/**
	 * This coherent memory is located in SYSMEM. Note: This type is used
	 * internally. Use APERTURE_SYSMEM.
	 */
	APERTURE_SYSMEM_COH,
	/**
	 * This memory is located in VIDMEM.
	 */
	APERTURE_VIDMEM,
	/**
	 * This gives maximum type of memory locations. Note: This should always
	 * be defined last.
	 */
	APERTURE_MAX_ENUM
};

/**
 * This flag designates the requested GMMU mapping.
 */
enum gk20a_mem_rw_flag {
	/**
	 * By default READ_WRITE
	 */
	gk20a_mem_flag_none = 0,
	/**
	 * READ only
	 */
	gk20a_mem_flag_read_only = 1,
	/**
	 * WRITE only
	 */
	gk20a_mem_flag_write_only = 2,
};

/**
 * This structure describes the number of arguments getting passed through
 * the various levels of GMMU mapping functions.
 */
struct nvgpu_gmmu_attrs {
	/**
	 * Index into the page size table.
	 * Min: GMMU_PAGE_SIZE_SMALL
	 * Max: GMMU_PAGE_SIZE_KERNEL
	 */
	u32			pgsz;
	/**
	 * Kind attributes for mapping.
	 */
	u32			kind_v;
#ifdef CONFIG_NVGPU_COMPRESSION
	/**
	 * Comptag line in the comptag cache.
	 * updated every time we write a PTE.
	 */
	u64			ctag;
	/**
	 * True if cbc policy is comptagline_mode
	 */
	bool			cbc_comptagline_mode;
#endif
	/**
	 * Cacheability of the mapping.
	 * Cacheable if this flag is set to true, else non-cacheable.
	 */
	bool			cacheable;
	/**
	 * Flag from enum #gk20a_mem_rw_flag
	 * (i.e gk20a_mem_flag_none, gk20a_mem_flag_read_only, ...).
	 */
	enum gk20a_mem_rw_flag	rw_flag;
	/**
	 * True if the mapping should be sparse.
	 */
	bool			sparse;
	/**
	 * True if the mapping should be Privileged.
	 */
	bool			priv;
	/**
	 * True if the PTE should be marked valid.
	 */
	bool			valid;
	/**
	 * This flag variable designates where the memory actually
	 * was allocated from. #nvgpu_aperture.
	 * (i.e APERTURE_SYSMEM, APERTURE_VIDMEM, ...).
	 */
	enum nvgpu_aperture	aperture;
	/**
	 * When set (i.e True) print debugging info.
	 */
	bool			debug;
	/**
	 * True if l3_alloc flag is valid.
	 */
	bool			l3_alloc;
	/**
	 * True if tegra_raw flag is valid.
	 */
	bool			tegra_raw;
	/**
	 * True if platform_atomic flag is valid.
	 */
	bool			platform_atomic;
};

/**
 * This structure describes the GMMU level entry format which is used for
 * GMMU mapping understandable by GMMU H/W.
 */
struct gk20a_mmu_level {
	/**
	 * MSB bit position of the page table entry (pde, pte).
	 * [0] - GMMU_PAGE_SIZE_SMALL, [1] - GMMU_PAGE_SIZE_BIG.
	 */
	u32 hi_bit[2];
	/**
	 * LSB bit position of the page table entry (pde, pte).
	 * [0] - GMMU_PAGE_SIZE_SMALL, [1] - GMMU_PAGE_SIZE_BIG.
	 */
	u32 lo_bit[2];

	/**
	 * Function ptr which points to the address of
	 * page table entry update routine for pde and pte entry.
	 */
	void (*update_entry)(struct vm_gk20a *vm,
			     const struct gk20a_mmu_level *l,
			     struct nvgpu_gmmu_pd *pd,
			     u32 pd_idx,
			     u64 phys_addr,
			     u64 virt_addr,
			     struct nvgpu_gmmu_attrs *attrs);
	/**
	 * GMMU level entry size. GMMU level entry format will be different
	 * for each GPU family (i.e, gv11b, gp10b, ...).
	 */
	u32 entry_size;
	/**
	 * Function ptr which points to the address of
	 * get pde/pte page size routine.
	 */
	u32 (*get_pgsz)(struct gk20a *g, const struct gk20a_mmu_level *l,
				struct nvgpu_gmmu_pd *pd, u32 pd_idx);
};

/**
 * @brief Get the printable const string from #gk20a_mem_rw_flag for logging.
 *
 * @param p [in]	flag designates the requested GMMU mapping.
 *
 * @return Pointer to a printable valid const string in case of success.
 * Pointer to a printable invalid const string
 * in case of invalid #gk20a_mem_rw_flag.
 */
static inline const char *nvgpu_gmmu_perm_str(enum gk20a_mem_rw_flag p)
{
	const char *str;

	switch (p) {
	case gk20a_mem_flag_none:
		str = "RW";
		break;
	case gk20a_mem_flag_write_only:
		str = "WO";
		break;
	case gk20a_mem_flag_read_only:
		str = "RO";
		break;
	default:
		str = "??";
		break;
	}
	return str;
}

/**
 * @brief Setup a VM page table base format for GMMU mapping.
 *
 * @param vm	[in]	Pointer to virtual memory structure.
 *
 * Init Page Table:
 * Allocates the DMA memory for a page directory.
 * This handles the necessary PD cache logistics. Since on Parker and
 *  later GPUs some of the page  directories are smaller than a page packing
 *  these PDs together saves a lot of memory.
 * #nvgpu_pd_alloc() does the pd cache allocation.
 *
 * PDB size here must be at least 4096 bytes so that its address is 4K
 * aligned. Although lower PDE tables can be aligned at 256B boundaries
 * the PDB must be 4K aligned.
 *
 * Currently NVGPU_CPU_PAGE_SIZE is used, even when 64K, to work around an issue
 * with the PDB TLB invalidate code not being pd_cache aware yet.
 *
 * @return	0 in case of success.
 * @retval	-ENOMEM For any allocation failures from kzalloc and dma_alloc
 * 		functions.
 */
int nvgpu_gmmu_init_page_table(struct vm_gk20a *vm);

/**
 * @brief Map memory into the GMMU. This is required to make the particular
 * context on the GR, CE to access the given virtual address.
 *
 * @param vm		[in]	Pointer to virtual memory structure.
 * @param mem		[in]	The previously allocated buffer to map.
 * @param size		[in]	Size of the mapping in bytes.
 * @param flags		[in]	Mapping flags.
 *                              - Min: NVGPU_VM_MAP_FIXED_OFFSET
 *                              - Max: NVGPU_VM_MAP_PLATFORM_ATOMIC
 * @param rw_flag	[in]	Flag designates the requested GMMU mapping.
 * 				- Min: gk20a_mem_flag_none
 * 				- Max: gk20a_mem_flag_write_only
 * @param priv		[in]	True if the mapping should be Privileged.
 * @param aperture	[in]	Where the memory actually was allocated from.
 *				- Min: APERTURE_SYSMEM
 *				- Max: APERTURE_VIDMEM
 *
 * Core GMMU map function for the nvgpu to use. The GPU VA will be
 * allocated for client.
 *
 * GMMU Map:
 * Retrives the nvgpu_sgt which contains the memory handle information.
 * Acquires the VM GMMU lock to the avoid race.
 * Decodes the Mapping flags, rw_flag, priv and aperture for GMMU mapping.
 * Allocates a new GPU VA range for a specific size.#nvgpu_vm_alloc_va() reserves
 *  the GPU VA.
 * Program PDE and PTE entry with PA/IPA, mapping flags, rw_flag and aperture
 *  information. #nvgpu_gmmu_update_page_table() does the pde and pte updates.
 * Chip specific stuff is handled at the PTE/PDE programming HAL layer. GMMU level
 *  entry format will be different for each GPU family (i.e, gv11b, gp10b).
 *  Internally nvgpu_set_pd_level() program the different level of page table.
 * Invalidates the GPU TLB, gm20b_fb_tlb_invalidate does the tlb invalidate.
 * Release the VM GMMU lock.
 *
 * Note that mem->gpu_va is not updated.
 *
 * @return	valid GMMU VA start address in case of success.
 * @retval	0 in case of all possible failures.
 * 		Possible Failure cases:
 * 			- Memory handle is invalid.
 * 			- No free GPU VA space (GPU VA space full).
 * 			- TLB invalidate timeout.
 * 			- invalid inputs.
 *
 */
u64 nvgpu_gmmu_map_partial(struct vm_gk20a *vm,
		   struct nvgpu_mem *mem,
		   u64 size,
		   u32 flags,
		   enum gk20a_mem_rw_flag rw_flag,
		   bool priv,
		   enum nvgpu_aperture aperture);

/**
 * @brief Map a whole buffer into the GMMU.
 *
 * This is like nvgpu_gmmu_map_partial() but with the full requested size of
 * the buffer in nvgpu_mem.size.
 */
u64 nvgpu_gmmu_map(struct vm_gk20a *vm,
		   struct nvgpu_mem *mem,
		   u32 flags,
		   enum gk20a_mem_rw_flag rw_flag,
		   bool priv,
		   enum nvgpu_aperture aperture);

/**
 * @brief Map memory into the GMMU at a fixed address. This is required to
 * make the parrticular context on the GR, CE to access the given virtual
 * address.
 *
 * @param vm		[in]	Pointer to virtual memory structure.
 * @param mem		[in]	Structure for storing the memory information.
 * @param addr		[in]	Fixed GPU VA start address requested by client.
 * @param size		[in]	Size of the buffer in bytes.
 * @param flags		[in]	Mapping flags.
 *                              - Min: NVGPU_VM_MAP_FIXED_OFFSET
 *                              - Max: NVGPU_VM_MAP_PLATFORM_ATOMIC
 * @param rw_flag	[in]	Flag designates the requested GMMU mapping.
 * 				- Min: gk20a_mem_flag_none
 * 				- Max: gk20a_mem_flag_write_only
 * @param priv		[in]	True if the mapping should be Privileged.
 * @param aperture	[in]	Where the memory actually was allocated from.
 *				- Min: APERTURE_SYSMEM
 *				- Max: APERTURE_VIDMEM
 *
 * GMMU Map at a fixed address:
 * Retrives the nvgpu_sgt which contains the memory handle information.
 * Acquires the VM GMMU lock to the avoid race.
 * Decodes the Mapping flags, rw_flag, priv and aperture for GMMU mapping.
 * Program PDE and PTE entry with PA/IPA, mapping flags, rw_flag and aperture
 *  information. #nvgpu_gmmu_update_page_table does the pde and pte updates.
 * Chip specific stuff is handled at the PTE/PDE programming HAL layer.
 *  GMMU level entry format will be different for each GPU family (i.e, gv11b, gp10b).
 *  Internally nvgpu_set_pd_level() will be called to program the different level of
 *  the page table.
 * Invalidates the GPU TLB, gm20b_fb_tlb_invalidate does the tlb invalidate.
 * Release the VM GMMU lock.
 *
 * @return	valid GMMU VA start address in case of success.
 * @return 	0 in case of all possible failures.
 * 		Possible Failure cases:
 * 			- Memory handle is invalid.
 * 			- No free GPU VA space at @addr passed by client.
 * 			- TLB invalidate timeout.
 * 			- invalid inputs.
 */
u64 nvgpu_gmmu_map_fixed(struct vm_gk20a *vm,
			 struct nvgpu_mem *mem,
			 u64 addr,
			 u64 size,
			 u32 flags,
			 enum gk20a_mem_rw_flag rw_flag,
			 bool priv,
			 enum nvgpu_aperture aperture);

/**
 * @brief Unmap a memory mapped by nvgpu_gmmu_map()/nvgpu_gmmu_map_fixed().
 * This is required to remove the translations from the GPU page table.
 *
 * @param vm		[in]	Pointer to virtual memory structure.
 * @param mem		[in]	Structure for storing the memory information.
 * @param gpu_va	[in]	GPU virtual address.
 *
 * Core GMMU unmap function for the nvgpu to use.
 *
 * GMMU Unmap:
 * Acquires the VM GMMU lock to the avoid race.
 * Free the reserved GPU VA space staring at @gpu_va.
 * #nvgpu_vm_free_va does free the GPU VA space.
 * Program PDE and PTE entry with default information which is internally
 * frees up the GPU VA space.
 * Chip specific stuff is handled at the PTE/PDE programming HAL layer.
 * GMMU level entry format will be different for each GPU family
 * (i.e, gv11b).
 * Flush the GPU L2. gv11b_mm_l2_flush does the L2 flush.
 * Invalidates the GPU TLB, gm20b_fb_tlb_invalidate does the tlb invalidate.
 * Release the VM GMMU lock.
 *
 * @return	None.
 */
void nvgpu_gmmu_unmap_addr(struct vm_gk20a *vm,
		      struct nvgpu_mem *mem,
		      u64 gpu_va);

/**
 * @brief Unmap a memory mapped by nvgpu_gmmu_map()/nvgpu_gmmu_map_fixed().
 *
 * This is like nvgpu_gmmu_unmap_addr() but with the address in nvgpu_mem.gpu_va.
 */
void nvgpu_gmmu_unmap(struct vm_gk20a *vm, struct nvgpu_mem *mem);

/**
 * @brief Compute number of words in a PTE.
 *
 * @param g	[in]	The GPU.
 *
 * Compute number of words in a PTE:
 * Iterate to the PTE level. The levels array is always NULL terminated.
 * GMMU level entry format will be different for each GPU family
 * (i.e, gv11b).
 *
 * This computes and returns the size of a PTE for the passed chip.
 *
 * @return	number of words in a PTE in case of success.
 */
u32 nvgpu_pte_words(struct gk20a *g);

/**
 * @brief Get the contents of a PTE by virtual address
 *
 * @param g     [in]	The GPU.
 * @param vm	[in]	Pointer to virtual memory structure.
 * @param vaddr	[in]	GPU virtual address.
 * @param pte	[out]	Set to the contents of the PTE.
 *
 * Get the contents of a PTE:
 * Find a PTE in the passed VM based on the passed GPU virtual address. This
 * will @pte with a copy of the contents of the PTE. @pte must be an array of
 * u32s large enough to contain the PTE. This can be computed using
 * nvgpu_pte_words().
 *
 * If you wish to write to this PTE then you may modify @pte and then use the
 * nvgpu_set_pte().
 *
 * @return	0 if the PTE is found.
 * @retval	-EINVAL If any of the compuation fails inside.
 */
int nvgpu_get_pte(struct gk20a *g, struct vm_gk20a *vm, u64 vaddr, u32 *pte);

/**
 * @brief Set a PTE based on virtual address
 *
 * @param g     [in]	The GPU.
 * @param vm	[in]	Pointer to virtual memory structure.
 * @param vaddr	[in]	GPU virtual address.
 * @param pte	[in]	The contents of the PTE to write.
 *
 * Set the contents of a PTE:
 * Find a PTE and overwrite the contents of that PTE with the passed in data
 * located in @pte by calling nvgpu_locate_pte(). If the PTE does not exist
 * then no writing will happen. That is this function will not fill out the
 * page tables for you. The expectation is that the passed @vaddr has already
 * been mapped and this is just modifying the mapping (for instance changing
 * invalid to valid).
 *
 * @pte must contain at least the required words for the PTE. See
 * nvgpu_pte_words().
 *
 * @return 	0 on success.
 * @retval	-EINVAL for failure.
 */
int nvgpu_set_pte(struct gk20a *g, struct vm_gk20a *vm, u64 vaddr, u32 *pte);

/**
 * Native GPU "HAL" functions.
 */

/**
 * @brief Mutex Locked version of map memory routine.
 *
 * @param vm		[in]	Pointer to virtual memory structure.
 * @param vaddr		[in]	GPU virtual address.
 * @param sgt		[in]	Pointer to scatter gather table for
 *                              direct "physical" nvgpu_mem structures.
 * @param buffer_offset	[in]	Offset address from start of the memory.
 * @param size		[in]	Size of the buffer in bytes.
 * @param pgsz_idx	[in]	Index into the page size table.
 *                              - Min: GMMU_PAGE_SIZE_SMALL
 *                              - Max: GMMU_PAGE_SIZE_KERNEL
 * @param kind_v	[in]	Kind attributes for mapping.
 * @param ctag_offset	[in]	Size of the buffer in bytes.
 * @param flags         [in]	Mapping flags.
 *                              - Min: NVGPU_VM_MAP_FIXED_OFFSET
 *                              - Max: NVGPU_VM_MAP_PLATFORM_ATOMIC
 * @param rw_flag	[in]	Flag designates the requested GMMU mapping.
 * 				- Min: gk20a_mem_flag_none
 * 				- Max: gk20a_mem_flag_write_only
 * @param clear_ctags	[in]	True if ctags clear is required.
 * @param sparse	[in]	True if the mapping should be sparse.
 * @param priv		[in]	True if the mapping should be Privileged.
 * @param batch		[in]	Mapping_batch handle. Structure which track
 *                              whether the L2 flush and TLB invalidate is
 *                              required or not during map/unmap.
 * @param aperture	[in]	Where the memory actually was allocated from.
 *				- Min: APERTURE_SYSMEM
 *				- Max: APERTURE_VIDMEM
 *
 * Native GPU "HAL" functions for GMMU Map.
 *
 * Locked version of GMMU Map routine:
 * Decodes the Mapping flags, rw_flag, priv and aperture for GMMU mapping.
 * Allocates a new GPU VA range for a specific size if vaddr is 0.
 * #nvgpu_vm_alloc_va() reserves the GPU VA.
 * Program PDE and PTE entry with PA/IPA, mapping flags, rw_flag and aperture
 *  information. #nvgpu_gmmu_update_page_table does the pde and pte updates.
 * Chip specific stuff is handled at the PTE/PDE programming HAL layer.
 *  GMMU level entry format will be different for each GPU family
 *  (i.e, gv11b, gp10b).
 * Invalidates the GPU TLB, gm20b_fb_tlb_invalidate does the tlb invalidate.
 *
 * @return	valid GMMU VA start address in case of success.
 * @retval	0 in case of all possible failures.
 * 		Possible Failure cases:
 * 		 - No free GPU VA space (GPU VA space full).
 * 		 - TLB invalidate timeout.
 * 		 - Any of the invlaid input parameters.
 * 		 - Failure inside any of the functions called.
 */
u64 nvgpu_gmmu_map_locked(struct vm_gk20a *vm,
			  u64 vaddr,
			  struct nvgpu_sgt *sgt,
			  u64 buffer_offset,
			  u64 size,
			  u32 pgsz_idx,
			  u8 kind_v,
			  u32 ctag_offset,
			  u32 flags,
			  enum gk20a_mem_rw_flag rw_flag,
			  bool clear_ctags,
			  bool sparse,
			  bool priv,
			  struct vm_gk20a_mapping_batch *batch,
			  enum nvgpu_aperture aperture);

/**
 * @brief Mutex Locked version Unmap routine.
 *
 * @param vm			[in]	Pointer to virtual memory structure.
 * @param vaddr			[in]	GPU virtual address.
 * @param size			[in]	Size of the buffer in bytes.
 * @param pgsz_idx		[in]	Index into the page size table.
 *                                      - Min: GMMU_PAGE_SIZE_SMALL
 *                                      - Max: GMMU_PAGE_SIZE_KERNEL
 * @param va_allocated	[in]	Indicates if gpu_va address is valid/allocated.
 * @param rw_flag		[in]	Flag designates the requested GMMU mapping.
 * 					- Min: gk20a_mem_flag_none
 * 					- Max: gk20a_mem_flag_write_only
 * @param sparse		[in]	True if the mapping should be sparse.
 * @param batch			[in]	Mapping_batch handle. Structure which track
 *                                  whether the L2 flush and TLB invalidate is
 *                                  required or not during map/unmap.
 *
 * Native GPU "HAL" functions for GMMU Unmap.
 *
 * Locked version of GMMU Unmap routine:
 * Free the reserved GPU VA space staring at \a gpu_va.
 * #nvgpu_vm_free_va does free the GPU VA space.
 * Program PDE and PTE entry with default information which is internally
 *  frees up the GPU VA space.
 * Chip specific stuff is handled at the PTE/PDE programming HAL layer.
 * GMMU level entry format will be different for each GPU family
 *  (i.e, gv11b).
 * Flush the GPU L2. gv11b_mm_l2_flush does the L2 flush.
 * Invalidates the GPU TLB, gm20b_fb_tlb_invalidate does the tlb invalidate.
 *
 * @return	None.
 */
void nvgpu_gmmu_unmap_locked(struct vm_gk20a *vm,
			     u64 vaddr,
			     u64 size,
			     u32 pgsz_idx,
			     bool va_allocated,
			     enum gk20a_mem_rw_flag rw_flag,
			     bool sparse,
			     struct vm_gk20a_mapping_batch *batch);

/**
 * Internal debugging routines.
 */
#define pte_dbg(g, attrs, fmt, args...)					\
	do {								\
		if (((attrs) != NULL) && ((attrs)->debug)) {		\
			nvgpu_info(g, fmt, ##args);			\
		} else {						\
			nvgpu_log(g, gpu_dbg_pte, fmt, ##args);		\
		}							\
NVGPU_COV_WHITELIST(false_positive, NVGPU_MISRA(Rule, 14_4), "Bug 2623654") \
	} while (false)

/**
 * @brief Function to get the default big page size in bytes.
 *
 * Default big page size:
 *  - Big page size is same for all GPU families.
 *
 * @return Default big page size
 */
u32 nvgpu_gmmu_default_big_page_size(void);

u32 nvgpu_gmmu_aperture_mask(struct gk20a *g,
				enum nvgpu_aperture mem_ap,
				bool platform_atomic_attr,
				u32 sysmem_mask,
				u32 sysmem_coh_mask,
				u32 vidmem_mask);

void nvgpu_pte_dbg_print(struct gk20a *g,
		struct nvgpu_gmmu_attrs *attrs,
		const char *vm_name, u32 pd_idx, u32 mmu_level_entry_size,
		u64 virt_addr, u64 phys_addr, u32 page_size, u32 *pte_w);

#endif /* NVGPU_GMMU_H */
