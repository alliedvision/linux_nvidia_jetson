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

#ifndef NVGPU_GOPS_MM_H
#define NVGPU_GOPS_MM_H

/**
 * @file
 *
 * MM HAL interface.
 */

#include <nvgpu/types.h>

struct gk20a;

/**
 * This structure describes the HAL functions related to
 * GMMU fault handling.
 */
struct gops_mm_mmu_fault {
	/**
	 * @brief HAL to initialize the software setup of
	 *        GMMU fault buffer.
	 *
	 * @param g [in]	The GPU.
	 *
	 * Initializes the software setup of GMMU fault buffer:
	 * - Initializes the hub isr mutex to avoid race during
	 *   GMMU fault buffer read/write handling from
	 *   nvgpu software side.
	 * - Allocates memory to store the non replayable
	 *   GMMU fault information.
	 *
	 * @return 0 in case of success, < 0 in case of failure.
	 * Possible failure case:
	 * - Insufficient system memory (ENOMEM).
	 */
	int (*setup_sw)(struct gk20a *g);

	/**
	 * @brief HAL to initialize the hardware setup of
	 *        GMMU fault buffer.
	 *
	 * @param g [in]	The GPU.
	 *
	 * Initializes the hardware setup of GMMU fault buffer:
	 * - Configures the GMMU fault buffer base address and its
	 *   size information in fbhubmmu specific BAR0 register.
	 */
	void (*setup_hw)(struct gk20a *g);

	/**
	 * @brief HAL to free the GMMU fault buffer.
	 *
	 * @param g [in]	The GPU.
	 *
	 * Free the GMMU fault buffer:
	 * - Free the GMMU fault buffer memory.
	 * - Destroy the hub isr mutex.
	 */
	void (*info_mem_destroy)(struct gk20a *g);

	/**
	 * @brief HAL to disable the hardware setup of GMMU
	 *        fault buffer.
	 *
	 * @param g [in]	The GPU.
	 *
	 * Disable the hardware setup of GMMU fault buffer.
	 */
	void (*disable_hw)(struct gk20a *g);

	/**
	 * @brief HAL to parse mmu fault info read from h/w.
	 *
	 * @param mmufault [in]	Pointer to memory containing info
	 *                      to be parsed.
	 *
	 */
	void (*parse_mmu_fault_info)(struct mmu_fault_info *mmufault);
};

/**
 * This structure describes the HAL functions related to
 * fb and L2 hardware operations.
 */
struct gops_mm_cache {
	/**
	 * @brief HAL to flush the frame buffer memory.
	 *
	 * @param g [in]	The GPU.
	 *
	 * Flush the frame buffer memory:
	 * - Flushes the FB. Then, waits for completion (by polling)
	 *   upto polling timeout.
	 *
	 * @return 0 in case of success, < 0 in case of failure.
	 * Possible failure case:
	 * - CPU polling timeout during FB flush operation (-EBUSY).
	 */
	int (*fb_flush)(struct gk20a *g);

	/**
	 * @brief HAL to invalidate the L2.
	 *
	 * @param g [in]	The GPU.
	 *
	 * Invalidate the L2:
	 * - Trigger the L2 invalidate operation. Then, waits for
	 *   completion (by polling) upto polling timeout.
	 *
	 * Note: It does not return error. But CPU polling can timeout.
	 */
	void (*l2_invalidate)(struct gk20a *g);

	/**
	 * @brief HAL to flush and invalidate the L2 and fb.
	 *
	 * @param g          [in]	The GPU.
	 * @param invalidate [in]	true if L2 invalidate is also
	 *                              required.
	 *
	 * Flush and invalidate the L2 and fb:
	 * - Trigger the fb flush operation. Then, waits for completion
	 *   (by polling) upto polling timeout.
	 * - Trigger the L2 invalidate operation. Then, waits for
	 *   completion (by polling) upto polling timeout.
	 *
	 * @return 0 in case of success, < 0 in case of failure.
	 * Possible failure cases:
	 * - CPU polling timeout during FB flush operation (-EBUSY).
	 * - CPU polling timeout during L2 invalidate operation
	 *   (-EBUSY).
	 */
	int (*l2_flush)(struct gk20a *g, bool invalidate);
#ifdef CONFIG_NVGPU_COMPRESSION
	/**
	 * @brief HAL to flush Compression Bit Cache memory.
	 *
	 * @param g [in]	The GPU.
	 *
	 * Flush and invalidate the L2 and fb:
	 * - Trigger all dirty lines from the CBC to L2.
	 *   Then, waits for completion (by polling) upto
	 *   polling timeout.
	 *
	 * Note: It does not return error. But CPU polling can timeout.
	 */
	void (*cbc_clean)(struct gk20a *g);
#endif
};

/**
 * This structure describes the HAL functions related to
 * GMMU operations.
 */
struct gops_mm_gmmu {
	/**
	 * @brief HAL to get the GMMU level mapping info structure.
	 *
	 * @param g             [in]	The GPU.
	 * @param big_page_size [in]	Big page size supported by GMMU.
	 *
	 * @return Pointer to GMMU level mapping info structure.
	 */
	const struct gk20a_mmu_level *
		(*get_mmu_levels)(struct gk20a *g, u64 big_page_size);

	/**
	 * @brief HAL to get maximum page table levels supported by the
	 * GMMU HW.
	 *
	 * @param g             [in]	The GPU.
	 *
	 * @return Page table levels supported by GPU.
	 */
	u32 (*get_max_page_table_levels)(struct gk20a *g);

	/**
	 * @brief Map #sgt into the GPU address space described by #vm.
	 *
	 * @param vm            [in]	Pointer to virtual memory
	 *                              structure.
	 * @param map_offset    [in]	GPU virtual address.
	 * @param sgt           [in]	Pointer to scatter gather table
	 *                              for direct "physical" nvgpu_mem
	 *                              structures.
	 * @param buffer_offset [in]	Offset address from start of
	 *                              the memory.
	 * @param size          [in]	Size of the buffer in bytes.
	 * @param pgsz_idx      [in]	Index into the page size table.
	 *                              - Min: GMMU_PAGE_SIZE_SMALL
	 *                              - Max: GMMU_PAGE_SIZE_KERNEL
	 * @param kind_v        [in]	Kind attributes for mapping.
	 * @param ctag_offset   [in]	Size of the buffer in bytes.
	 * @param flags         [in]	Mapping flags.
	 *                              Min:NVGPU_VM_MAP_FIXED_OFFSET
	 *                              Max:NVGPU_VM_MAP_PLATFORM_ATOMIC
	 * @param rw_flag       [in]	Flag designates the requested
	 *                              GMMU mapping.
	 * @param clear_ctags   [in]	True if ctags clear is required.
	 * @param sparse        [in]	True if the mapping should be
	 *                              sparse.
	 * @param priv          [in]	True if the mapping should be
	 *                              privileged.
	 * @param batch         [in]	Mapping_batch handle. Structure
	 *                              which track whether the L2 flush
	 *                              and TLB invalidate are required
	 *                              or not during map/unmap.
	 * @param aperture      [in]	Where the memory actually was
	 *                              allocated from.
	 *
	 * Locked version of GMMU Map routine:
	 * - Decodes the Mapping flags, rw_flag, priv and aperture for
	 *   GMMU mapping.
	 * - Allocates a new GPU VA range for a specific size
	 *   if vaddr is 0.
	 *   #nvgpu_vm_alloc_va() reserves the GPU VA.
	 * - Program PDE and PTE entry with PA/IPA, mapping flags,
	 *   rw_flag and aperture information.
	 *   #nvgpu_gmmu_update_page_table does the pde and pte updates.
	 * - Chip specific stuff is handled at the PTE/PDE
	 *   programming HAL layer.
	 *   GMMU level entry format will be different for each
	 *   GPU family (i.e, gv11b, gp10b).
	 * - Invalidates the GPU TLB, gm20b_fb_tlb_invalidate does the
	 *   tlb invalidate.
	 *
	 * @return valid GMMU VA start address in case of success.
	 * @return 0 in case of all possible failures.
	 * Possible Failure cases:
	 * - No free GPU VA space (GPU VA space full).
	 * - TLB invalidate timeout.
	 */
	u64 (*map)(struct vm_gk20a *vm,
			u64 map_offset,
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
	 * @brief Unmap #vaddr into the GPU address space described by #vm.
	 *
	 * @param vm            [in]	Pointer to virtual memory
	 *                              structure.
	 * @param vaddr         [in]	GPU virtual address.
	 * @param size          [in]	Size of the buffer in bytes.
	 * @param pgsz_idx      [in]	Index into the page size table.
	 *                              - Min: GMMU_PAGE_SIZE_SMALL
	 *                              - Max: GMMU_PAGE_SIZE_KERNEL
	 * @param va_allocated  [in]	Indicates if gpu_va address is
	 *                              valid/allocated.
	 * @param rw_flag       [in]	Flag designates the requested
	 *                              GMMU mapping.
	 * @param sparse        [in]	True if the mapping should be
	 *                              sparse.
	 * @param batch         [in]	Mapping_batch handle. Structure
	 *                              which track whether the L2 flush
	 *                              and TLB invalidate are required
	 *                              or not during map/unmap.
	 *
	 * Locked version of GMMU Unmap routine:
	 * - Free the reserved GPU VA space staring at @gpu_va.
	 *   #nvgpu_vm_free_va does free the GPU VA space.
	 * - Program PDE and PTE entry with default information which is
	 *   internally frees up the GPU VA space.
	 * - Chip specific stuff is handled at the PTE/PDE
	 *   programming HAL layer.
	 *   GMMU level entry format will be different for
	 *   each GPU family (i.e, gv11b).
	 * - Flush the GPU L2. gv11b_mm_l2_flush does the L2 flush.
	 * - Invalidates the GPU TLB, #gm20b_fb_tlb_invalidate() does
	 *   the tlb invalidate.
	 */
	void (*unmap)(struct vm_gk20a *vm,
				u64 vaddr,
				u64 size,
				u32 pgsz_idx,
				bool va_allocated,
				enum gk20a_mem_rw_flag rw_flag,
				bool sparse,
				struct vm_gk20a_mapping_batch *batch);

	/**
	 * @brief HAL to get the available big page sizes.
	 *
	 * @param g [in]	The GPU.
	 *
	 * Get the available big page sizes:
	 *  - Bitwise OR of all available big page sizes.
	 *  - Big page size will be different for each GPU family
	 *    (i.e. gv11b, tu104).
	 *
	 * @return Valid bitwise OR of all available big page sizes
	 *         if big page support is enabled.
	 * @return 0 if big page support is disabled.
	 */
	u32 (*get_big_page_sizes)(void);

	/**
	 * @brief HAL to get the default big page size in bytes.
	 *
	 * @param g [in]	The GPU.
	 *
	 * Default big page size:
	 *  - Big page size will be different for each GPU family
	 *    (i.e. gv11b, tu104).
	 *
	 * @return Valid big page size if big page support is enabled.
	 * @return 0 if big page support is disabled.
	 */
	u32 (*get_default_big_page_size)(void);

	/**
	 * @brief HAL to get the iommu physical bit position.
	 *
	 * @param g [in]	The GPU.
	 *
	 * This HAL is used to get the iommu physical bit position.
	 *
	 * @return iommu physical bit position.
	 */
	u32 (*get_iommu_bit)(struct gk20a *g);

	/**
	 * @brief HAL to convert from tegra_phys to gpu_phys.
	 *
	 * @param g     [in]	The GPU.
	 * @param attrs [in]	Pointer to gmmu attributes.
	 * @param phys  [in]	Tegra physical address.
	 *
	 * This HAL is used to convert from tegra_phys to gpu_phys
	 * for GMMU programming.
	 *
	 * Notes:
	 * On Volta the GPU determines whether to do L3 allocation
	 * for a mapping by checking bit 36 of the phsyical address.
	 * So if a mapping should allocate lines in the L3 then
	 * this bit must be set.
	 *
	 * @return gpu physical address for GMMU programming.
	 */
	u64 (*gpu_phys_addr)(struct gk20a *g,
					struct nvgpu_gmmu_attrs *attrs,
					u64 phys);
};

struct gops_mm {
	/**
	 * @brief HAL to initialize an internal structure which is used to
	 *        track pd_cache.
	 *
	 * @param g [in]	The GPU.
	 *
	 * Initialize the pd_cache:
	 * - Allocates the zero initialized memory area for #nvgpu_pd_cache.
	 * - Initializes the mutexes and list nodes for pd_cache
	 *   tracking.
	 *
	 * @return 0 in case of success.
	 * @return -ENOMEM (< 0) in case of failure.
	 * Possible failure case:
	 * - Insufficient system memory (ENOMEM).
	 */
	int (*pd_cache_init)(struct gk20a *g);

	/**
	 * @brief This HAL function initializes the Memory Management unit.
	 *        @sa #nvgpu_init_mm_support().
	 *
	 * @param g [in]	The GPU.
	 *
	 * MM init:
	 * - MM S/W init:
	 *  - Resets the current pramin window index to 0.
	 *  - Initializes the vidmem page allocator with size, flags and etc.
	 *  - Allocates vidmem memory for acr blob from bootstrap region.
	 *  - Creates the CE vidmem clear thread for vidmem clear operations
	 *    during vidmem free.
	 *  - Allocates memory for sysmem flush operation.
	 *  - Initializes the GMMU virtual memory region for BAR1.
	 *  - Allocates and initializes the BAR1 instance block.
	 *  - Initializes the GMMU virtual memory region for PMU.
	 *  - Allocates and initializes the PMU instance block.
	 *  - Initializes the GMMU virtual memory region for CE.
	 *  - Allocates the GMMU debug write and read buffer (4K size).
	 * - MM H/W setup:
	 *  - Configures the GMMU debug buffer location in fbhubmmu register.
	 *  - Enables the fbhubmmu mc interrupt.
	 *  - Binds the BAR1 inst block and checks whether the bind
	 *    operation is successful.
	 *  - Flushes the FB. Then, waits for completion (by polling)
	 *    upto polling timeout.
	 *  - Configures the GMMU fault buffer location in fbhubmmu register.
	 *
	 * @return 0 in case of success, < 0 in case of failure.
	 * @retval Error returned by setup_sw and setup_hw routines.
	 * Possible failure cases:
	 * - Insufficient system memory (ENOMEM).
	 * - CPU polling timeout during FB flush operation (-EBUSY).
	 */
	int (*init_mm_support)(struct gk20a *g);

	/**
	 * @brief HAL to suspend the Memory Management unit.
	 *
	 * @param g [in]	The GPU.
	 *
	 * Suspend MM unit:
	 * - Pause the CE vidmem clear thread.
	 * - Flushes the FB and L2. Then, waits for completion (by polling)
	 *   upto polling timeout.
	 * - Invalidate L2.
	 * - Disable the fbhubmmu mc interrupt.
	 * - Disable the mmu fault buffer h/w setup.
	 *
	 * @return 0 in case of success, < 0 in case of failure.
	 * Possible failure case:
	 * - CPU polling timeout during FB or L2 flush operation (-EBUSY).
	 */
	int (*mm_suspend)(struct gk20a *g);

	/**
	 * @brief HAL to bind the virtual memory context to the given channel.
	 *
	 * @param vm [in]	Pointer to virtual memory context.
	 * @param ch [in]	Pointer to nvgpu_channel.
	 *
	 * Bind a channel:
	 * - Increment reference count of virtual memory context.
	 * - Assign the virtual memory context to channel's virtual
	 *   memory context.
	 * - Program the different hardware blocks of GPU with addresses
	 *   associated with virtual memory context.
	 *
	 * @return 0, always.
	 */
	int (*vm_bind_channel)(struct vm_gk20a *vm, struct nvgpu_channel *ch);

	/**
	 * @brief HAL to setup the Memory Management hardware.
	 *
	 * @param g [in]	The GPU.
	 *
	 * MM hardware setup:
	 *  - Configures the GMMU debug buffer location in fbhubmmu register.
	 *  - Enables the fbhubmmu mc interrupt.
	 *  - Binds the BAR1 inst block and checks whether the bind
	 *    operation is successful.
	 *  - Flushes the FB. Then, waits for completion (by polling)
	 *    upto polling timeout.
	 *  - Configures the GMMU fault buffer location in fbhubmmu register.
	 *
	 * @return 0 in case of success, < 0 in case of failure.
	 * Possible failure cases:
	 * - Insufficient system memory (ENOMEM).
	 * - CPU polling timeout during FB flush operation (-EBUSY).
	 */
	int (*setup_hw)(struct gk20a *g);

	/**
	 * @brief HAL to get the BAR1 aperture availability status.
	 *
	 * @param g [in]	The GPU.
	 *
	 * BAR1 status:
	 * - false for gv11b.
	 *
	 * @return True if BAR1 aperture support is available.
	 * @return False if BAR1 aperture support is not available.
	 */
	bool (*is_bar1_supported)(struct gk20a *g);

	/**
	 * @brief HAL to initialize the BAR2 virtual memory.
	 *
	 * @param g [in]	The GPU.
	 *
	 * Initialize BAR2:
	 * - Initializes the GMMU virtual memory region for BAR2.
	 * - Allocates and initializes the BAR2 instance block.
	 *
	 * @return 0 in case of success, < 0 in case of failure.
	 * Possible failure case:
	 * - Insufficient system memory (ENOMEM).
	 */
	int (*init_bar2_vm)(struct gk20a *g);

	/**
	 * @brief HAL to free the BAR2 virtual memory.
	 *
	 * @param g [in]	The GPU.
	 *
	 * Free BAR2 VM:
	 * - Free the BAR2 instance block.
	 * - Free the BAR2 GMMU virtual memory region.
	 */
	void (*remove_bar2_vm)(struct gk20a *g);

	/**
	 * @brief HAL to initialize the instance block memory.
	 *
	 * @param inst_block    [in]	Pointer to instance block memory.
	 * @param vm            [in]	Pointer to virtual memory context.
	 * @param big_page_size [in]	Big page size supported by GMMU.
	 *
	 * Initializes the instance block memory:
	 * - Configures the pdb base, big page size and
	 *   sub context's pdb base in context's instance block memory.
	 */
	void (*init_inst_block)(struct nvgpu_mem *inst_block,
			struct vm_gk20a *vm, u32 big_page_size);

	/**
	 * @brief HAL to initialize the instance block memory.
	 * (for more than one subctx)
	 *
	 * @param inst_block    [in]	Pointer to instance block memory.
	 * @param vm            [in]	Pointer to virtual memory context.
	 * @param big_page_size [in]	Big page size supported by GMMU.
	 * @param max_subctx_count [in] Max number of sub context.
	 *
	 * Initializes the instance block memory:
	 * - Configures the pdb base, big page size and
	 *   sub context's pdb base in context's instance block memory.
	 */
	void (*init_inst_block_for_subctxs)(struct nvgpu_mem *inst_block,
			struct vm_gk20a *vm, u32 big_page_size,
			u32 max_subctx_count);

	/**
	 * @brief HAL to get the maximum flush retry counts.
	 *
	 * @param g  [in]	The GPU.
	 * @param op [in]	Requested operations on various unit.
	 *
	 * Get the maximum retry flush counts (retry timer) for the
	 * following operations:
	 * - Flush the Frame Buffer memory.
	 * - L2 Cache Flush.
	 *
	 * These retries are specific to GPU hardware and vary based on
	 * size of the frame buffer memory.
	 *
	 * @return Maximum flush retry counts for a specific h/w operation.
	 */
	u32 (*get_flush_retries)(struct gk20a *g, enum nvgpu_flush_op op);

	/**
	 * @brief HAL to get default virtual memory sizes.
	 *
	 * @param aperture_size [in]	Pointer to aperture size.
	 * @param user_size [in]	Pointer to user size.
	 * @param kernel_size [in]	Pointer to kernel size.
	 *
	 * Number of bits for virtual address space can vary. This HAL is used
	 * to get default values for virtual address spaces.
	 */
	void (*get_default_va_sizes)(u64 *aperture_size,
			u64 *user_size, u64 *kernel_size);

	/** @cond DOXYGEN_SHOULD_SKIP_THIS */
	u64 (*bar1_map_userd)(struct gk20a *g, struct nvgpu_mem *mem,
								u32 offset);
	int (*vm_as_alloc_share)(struct gk20a *g, struct vm_gk20a *vm);
	void (*vm_as_free_share)(struct vm_gk20a *vm);
	/** @endcond DOXYGEN_SHOULD_SKIP_THIS */

	struct gops_mm_mmu_fault mmu_fault;
	struct gops_mm_cache cache;
	struct gops_mm_gmmu gmmu;
};

#endif
