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

#ifndef NVGPU_MM_H
#define NVGPU_MM_H

/**
 * @file
 * @page unit-mm Unit MM
 *
 * Overview
 * ========
 *
 * The MM unit is responsible for managing memory in nvgpu. Memory consists
 * primarily of two types:
 *
 *   + Regular kernel memory
 *   + Device accessible memory (DMA memory)
 *
 * The MM code also makes sure that all of the necessary SW and HW
 * initialization for any memory subsystems are taken care of before the GPU
 * begins executing work.
 *
 * Regular Kernel Memory
 * ---------------------
 *
 * The MM unit generally relies on the underlying system to manage kernel
 * memory. The nvgpu_kmalloc() and friends implementation is handled by
 * kmalloc() on Linux for example.
 *
 * See include/nvgpu/kmem.h for more details.
 *
 * DMA
 * ---
 *
 * DMA memory is more complex since it depends on both the GPU hardware and the
 * underlying operating system to handle mapping of DMA memory into the GMMU
 * (GPU Memory Management Unit). See the following documents for a reference
 * describing DMA support in nvgpu-driver:
 *
 *   + include/nvgpu/dma.h
 *   + include/nvgpu/vm.h
 *   + include/nvgpu/gmmu.h
 *   + include/nvgpu/nvgpu_mem.h
 *   + include/nvgpu/nvgpu_sgt.h
 *
 * Data Structures
 * ===============
 *
 * The major data structures exposed to users of the MM unit in nvgpu all relate
 * to managing DMA buffers and mapping DMA buffers into a GMMU context. The
 * following is a list of these structures:
 *
 *   + struct mm_gk20a
 *
 *       struct mm_gk20a defines a single GPU's memory context. It contains
 *       descriptions of various system GMMU contexts and other GPU global
 *       locks, descriptions, etc.
 *
 *   + struct vm_gk20a
 *
 *       struct vm_gk20a describes a single GMMU context. This is made up of a
 *       page directory base (PDB) and other meta data necessary for managing
 *       GPU memory mappings within this context.
 *
 *   + struct nvgpu_mem
 *
 *       struct nvgpu_mem abstracts all forms of GPU accessible memory which may
 *       or may not be backed by an SGT/SGL. This structure forms the basis for
 *       all GPU accessible memory within nvgpu-common.
 *
 *   + struct nvgpu_sgt
 *
 *       In most modern operating systems a DMA buffer may actually be comprised
 *       of many smaller buffers. This is because in a system running for
 *       extended periods of time the memory starts to become fragmented at page
 *       level granularity. Thus when trying to allocate a buffer larger than a
 *       page it's possible that there won't be a large enough contiguous region
 *       capable of satisfying the allocation despite there apparently being
 *       more than enough available space.
 *
 *       This classic fragmentation problem is solved by using lists or tables
 *       of sub-allocations, that together, form a single DMA buffer. To manage
 *       these buffers the notion of a scatter-gather list or scatter gather
 *       table (SGL and SGT, respectively) are introduced.
 *
 *   + struct nvgpu_mapped_buf
 *
 *       This describes a mapping of a userspace provided buffer.
 *
 * Static Design
 * =============
 *
 * Details of static design.
 *
 * Resource utilization
 * --------------------
 *
 * External APIs
 * -------------
 *
 *   + nvgpu_mm_setup_hw()
 *
 *
 * Supporting Functionality
 * ========================
 *
 * There's a fair amount of supporting functionality:
 *
 *   + Allocators
 *     - Buddy allocator
 *     - Page allocator
 *     - Bitmap allocator
 *   + vm_area
 *   + gmmu
 *     # pd_cache
 *     # page_table
 *
 * Documentation for this will be filled in!
 *
 * Dependencies
 * ------------
 *
 * Dynamic Design
 * ==============
 *
 * Use case descriptions go here. Some potentials:
 *
 *   - nvgpu_vm_map()
 *   - nvgpu_gmmu_map()
 *   - nvgpu_dma_alloc()
 *
 * Open Items
 * ==========
 *
 * Any open items can go here.
 */

#include <nvgpu/vm.h>
#include <nvgpu/types.h>
#include <nvgpu/cond.h>
#include <nvgpu/thread.h>
#include <nvgpu/lock.h>
#include <nvgpu/atomic.h>
#include <nvgpu/nvgpu_mem.h>
#include <nvgpu/allocator.h>
#include <nvgpu/list.h>
#include <nvgpu/sizes.h>
#include <nvgpu/mmu_fault.h>
#include <nvgpu/fb.h>

struct gk20a;
struct vm_gk20a;
struct nvgpu_mem;
struct nvgpu_pd_cache;

/**
 * This flag designates the requested operations on various units
 * (i.e. FB, L2).
 */
enum nvgpu_flush_op {
	/**
	 * No operation.
	 */
	NVGPU_FLUSH_DEFAULT,
	/**
	 * Flush the Frame Buffer memory.
	 */
	NVGPU_FLUSH_FB,
	/**
	 * L2 Cache Invalidate.
	 */
	NVGPU_FLUSH_L2_INV,
	/**
	 * L2 Cache Flush.
	 */
	NVGPU_FLUSH_L2_FLUSH,
	/**
	 * Clear the Compression Bit Cache memory.
	 */
	NVGPU_FLUSH_CBC_CLEAN,
};

/**
 * This structure keeps track of a given GPU's memory management state. Each GPU
 * has exactly one of these structs embedded directly in the gk20a struct. Some
 * memory state is tracked on a per-context basis in the <nvgpu/vm.h> header but
 * for state that's global to a given GPU this is used.
 */
struct mm_gk20a {
	/**
	 * Pointer to GPU device struct.
	 */
	struct gk20a *g;

	/**
	 * This structure describes the default GPU VA sizes for channels.
	 */
	struct {
		/**
		 * Client-usable GPU VA region.
		 */
		u64 user_size;
		/**
		 * Nvgpu-only GPU VA region.
		 */
		u64 kernel_size;
	} channel;

	/**
	 * @defgroup MM_CLIENT_MEMORY_INFORMATION_FORMAT_1
	 *
	 * #aperture_size - Maximum address space size.
	 * #vm            - Pointer to virtual memory structure.
	 * #inst_block    - Instance block memory.
	 *
	 * @defgroup MM_CLIENT_MEMORY_INFORMATION_FORMAT_2
	 *
	 * #vm            - Pointer to virtual memory structure.
	 * #aperture_size - Maximum address space size.
	 *
	 * @defgroup MM_CLIENT_MEMORY_INFORMATION_FORMAT_3
	 *
	 * #vm            - Pointer to virtual memory structure.
	 */

	/**
	 * This structure describes the bar1 specific memory information.
	 *
	 * @ingroup MM_CLIENT_MEMORY_INFORMATION_FORMAT_1
	 */
	struct {
		u32 aperture_size;
		struct vm_gk20a *vm;
		struct nvgpu_mem inst_block;
	} bar1;

	/**
	 * This structure describes the bar2 specific memory information.
	 *
	 * @ingroup MM_CLIENT_MEMORY_INFORMATION_FORMAT_1
	 */
	struct {
		u32 aperture_size;
		struct vm_gk20a *vm;
		struct nvgpu_mem inst_block;
	} bar2;

	/**
	 * This structure describes the falcon specific memory information.
	 *
	 * @ingroup MM_CLIENT_MEMORY_INFORMATION_FORMAT_1
	 */
	struct engine_ucode {
		u32 aperture_size;
		struct vm_gk20a *vm;
		struct nvgpu_mem inst_block;
	} pmu, sec2, gsp;

	/**
	 * This structure describes the Hardware Performance Monitor System
	 * specific memory information.
	 *
	 * @ingroup MM_CLIENT_MEMORY_INFORMATION_FORMAT_3
	 */
	struct {
		struct nvgpu_mem inst_block;
	} hwpm;

	/**
	 * This structure describes the Performance buffer memory information
	 * which is used by GPU Profiler.
	 *
	 * @ingroup MM_CLIENT_MEMORY_INFORMATION_FORMAT_2
	 */
	struct {
		struct vm_gk20a *vm;
		struct nvgpu_mem inst_block;
		u64 pma_bytes_available_buffer_gpu_va;
		u64 pma_buffer_gpu_va;
	} perfbuf;

	/**
	 * This structure describes the Color Decompression engine
	 * specific memory information.
	 *
	 * @ingroup MM_CLIENT_MEMORY_INFORMATION_FORMAT_3
	 */
	struct {
		struct vm_gk20a *vm;
	} cde;

	/**
	 * This structure describes the Copy engine specific memory information.
	 *
	 * @ingroup MM_CLIENT_MEMORY_INFORMATION_FORMAT_3
	 */
	struct {
		struct vm_gk20a *vm;
	} ce;

	/**
	 * A cache for allocating PD memory. This enables smaller PDs
	 * to be packed into single pages.
	 */
	struct nvgpu_pd_cache *pd_cache;

	/** Lock to serialize L2 operations. */
	struct nvgpu_mutex l2_op_lock;
	/** Lock to serialize TLB operations. */
	struct nvgpu_mutex tlb_lock;

	struct nvgpu_mem bar2_desc;

	/** MMU fault buffer memory. */
	struct nvgpu_mem hw_fault_buf[NVGPU_MMU_FAULT_TYPE_NUM];
	/**
	 * This structure describes the various debug information reported
	 * by GMMU during MMU fault exceptions.
	 */
	struct mmu_fault_info fault_info[NVGPU_MMU_FAULT_TYPE_NUM];
	/** Lock to serialize Hub isr Operations. */
	struct nvgpu_mutex hub_isr_mutex;
#ifdef CONFIG_NVGPU_DGPU
	/**
	 * Separate function to cleanup the CE since it requires a channel to
	 * be closed which must happen before fifo cleanup.
	 */
	void (*remove_ce_support)(struct mm_gk20a *mm);
#endif
	/**
	 * Function ptr which points to the address of
	 * MM deinit routine which is called during the power off
	 * the GPU device.
	 */
	void (*remove_support)(struct mm_gk20a *mm);
	/** True if MM init/setup is ready. */
	bool sw_ready;
	int physical_bits;
	/** True if whole comptag memory is used for compress rendering. */
	bool use_full_comp_tag_line;

#if defined(CONFIG_NVGPU_NON_FUSA) || defined(CONFIG_NVGPU_KERNEL_MODE_SUBMIT)
	/** True if LTC sw setup is ready. */
	bool ltc_enabled_current;
	/** True if LTC hw setup is ready. */
	bool ltc_enabled_target;
#endif

	/** Disable big page support. */
	bool disable_bigpage;

	/**
	 * 4K bytes memory which is used for memory scrubbing
	 * during GPU poweron.
	 */
	struct nvgpu_mem sysmem_flush;

#ifdef CONFIG_NVGPU_DGPU
	/**
	 * Current Privileged Ram Window pointer which is used for accessing
	 * the contiguous 1MB VIDMEM memory block.
	 */
	u32 pramin_window;
	/** Lock to serialize pramin access request. */
	struct nvgpu_mutex pramin_window_lock;

	/**
	 * This structure describes the number of arguments used for VIDMEM
	 * memory management.
	 */
	struct {
		/** VIDMEM memory size in bytes. */
		size_t size;
		/** VIDMEM memory base address. */
		u64 base;
		/** Size of bootstrap-region in bytes. */
		size_t bootstrap_size;
		/** VIDMEM bootstrap-region base address. */
		u64 bootstrap_base;

		/**
		 * Nvgpu global PAGE_ALLOCATOR which does the
		 * VIDMEM memory management for nvgpu's clients.
		 */
		struct nvgpu_allocator allocator;
		/**
		 * Nvgpu bootstrap PAGE_ALLOCATOR which does the
		 * VIDMEM memory management during GPU poweron.
		 * Reserves bootstrap region for WPR which has GPU falcon ucode.
		 */
		struct nvgpu_allocator bootstrap_allocator;

		/** Copy engine context id which is used for VIDMEM clear. */
		u32 ce_ctx_id;
		/** True if whole VIDMEM memory is cleared. */
		volatile bool cleared;
		/** Lock to serialize whole VIDMEM memory clear operation. */
		struct nvgpu_mutex first_clear_mutex;

		/**
		 * List of memory region available for memory clear(memset)
		 * using copy engine.
		 */
		struct nvgpu_list_node clear_list_head;
		/**
		 * Lock to serialize VIDMEM memory clear operations
		 * during VIDMEM memory free.
		 */
		struct nvgpu_mutex clear_list_mutex;

		/**
		 * The condition variable to sleep on. This condition variable
		 * is typically signaled by the thread which updates
		 * the counter.
		 */
		struct nvgpu_cond clearing_thread_cond;
		/*
		 * Simple thread whose sole job is to periodically clear
		 * userspace vidmem allocations that have been recently freed.
		 */
		struct nvgpu_thread clearing_thread;
		/** Lock to serialize the thread state machine. */
		struct nvgpu_mutex clearing_thread_lock;
		/**
		 * On the first increment of the pause_count (0 -> 1),
		 * take the pause lock and prevent the vidmem clearing thread
		 * from processing work items.
		 *
		 * Otherwise the increment is all that's needed - it's
		 * essentially a ref-count for the number of pause() calls.
		 */
		nvgpu_atomic_t pause_count;
		/** Total number of bytes need to be cleared. */
		nvgpu_atomic64_t bytes_pending;
	} vidmem;
#endif
	/** GMMU debug write buffer. */
	struct nvgpu_mem mmu_wr_mem;
	/** GMMU debug read buffer. */
	struct nvgpu_mem mmu_rd_mem;
};

/**
 * Utility to get the GPU device structure from
 * memory management structure.
 */
static inline struct gk20a *
gk20a_from_mm(struct mm_gk20a *mm)
{
	return mm->g;
}

/**
 * Utility to get the GPU device structure from
 * virtual memory structure.
 */
static inline struct gk20a *
gk20a_from_vm(struct vm_gk20a *vm)
{
	return vm->mm->g;
}

/**
 * @brief Get the maximum BAR1 aperture size in MB.
 *
 * Note: 16MB is more than enough.
 *
 * @return maximum BAR1 aperture size in MB.
 * Invalid pd offset in case of invalid/random @pd_idx.
 */
static inline u32 bar1_aperture_size_mb_gk20a(void)
{
	return 16U;
}

 /**
 * @brief Get small page bottom GPU VA address range.
 *
 * When not using unified address spaces, the bottom 56GB of the space are used
 * for small pages, and the remaining high memory is used for large pages.
 *
 * @return small page bottom GPU VA address range.
 */
static inline u64 nvgpu_gmmu_va_small_page_limit(void)
{
	return ((u64)SZ_1G * 56U);
}

#ifdef CONFIG_NVGPU_DGPU
void nvgpu_init_mm_ce_context(struct gk20a *g);
#endif

/**
 * @brief All memory requests made by the GPU (with a few exceptions) are
 *        translated by the GMMU (GPU Memory Management Unit).
 *        Similar to how a CPU MMU works, a hierarchical page table structure
 *        is used to convert from a virtual address to a physical address.
 *        The GPU virtual memory management provided to applications consists of
 *        address space creation, buffer mapping, and buffer unmapping.
 *        This function initializes the GPU Memory Management unit
 *        which is essential to achieve the GPU memory management services.
 *
 * @param g	[in]	The GPU.
 *
 * MM init:
 * - MM S/W init:
 *   - Resets the current pramin window index to 0.
 *   - Initializes the vidmem page allocator with size, flags and etc.
 *   - Allocates vidmem memory for acr blob from bootstrap region.
 *   - Creates the CE vidmem clear thread for vidmem clear operations
 *     during vidmem free.
 *   - Allocates memory for sysmem flush operation.
 *   - Initializes the GMMU virtual memory region for BAR1.
 *   - Allocates and initializes the BAR1 instance block.
 *   - Initializes the GMMU virtual memory region for PMU.
 *   - Allocates and initializes the PMU instance block.
 *   - Initializes the GMMU virtual memory region for CE.
 *   - Allocates the GMMU debug write and read buffer (4K size).
 *   - Allocates ECC counters for fb and fbpa units.
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
int nvgpu_init_mm_support(struct gk20a *g);

/**
 * @brief Allocates a GPU accessible instance block memory.
 *
 * @param g		[in]	The GPU.
 * @param inst_block	[out]	Pointer to instance block memory.
 *
 * Allocates a GPU accessible instance block memory:
 * - Allocates memory suitable for GPU access. Stores the allocation info in
 * #nvgpu_mem.
 *
 * @return 0 on success and a suitable error code when there's an error. This
 * allocates memory specifically in SYSMEM or VIDMEM.
 * @retval -ENOMEM in case of sufficient memory is not available.
 */
int nvgpu_alloc_inst_block(struct gk20a *g, struct nvgpu_mem *inst_block);

/**
 * @brief Get a physical address of instance block memory for BAR0 programming.
 *
 * @param g		[in]	The GPU.
 * @param inst_block	[in]	Pointer to instance block memory.
 *
 * Get a physical address of instance block memory:
 * - Get a physical address of instance block memory for BAR0 programming.
 * - #nvgpu_mem can either point to SYSMEM or VIDMEM.
 *
 * @return valid physical address in case of valid #inst_block.
 * @retval 0 in case of invalid physical address.
 */
u64 nvgpu_inst_block_addr(struct gk20a *g, struct nvgpu_mem *inst_block);

/**
 * @brief Get a physical address of instance block memory for BAR0 programming.
 *
 * @param g		[in]	The GPU.
 * @param inst_block	[in]	Pointer to instance block memory.
 *
 * Get a physical address of instance block memory:
 * - Get a physical address of instance block memory for BAR0 programming.
 * - #nvgpu_mem can either point to SYSMEM or VIDMEM.
 *
 * @return valid physical address in case of valid #inst_block.
 * @retval 0 in case of invalid physical address.
 */
u32 nvgpu_inst_block_ptr(struct gk20a *g, struct nvgpu_mem *inst_block);

/**
 * @brief Free a instance block memory allocated by nvgpu_alloc_inst_block.
 *
 * @param g		[in]	The GPU.
 * @param inst_block	[in]	Pointer to instance block memory.
 *
 * Free a instance block memory:
 * - Free a instance block memory allocated by nvgpu_alloc_inst_block
 *   to avoid memleak.
 *
 * @return None.
 */
void nvgpu_free_inst_block(struct gk20a *g, struct nvgpu_mem *inst_block);

/**
 * @brief Suspend the Memory Management unit.
 *
 * @param g	[in]	The GPU.
 *
 * Suspend MM unit:
 * - Pause the CE vidmem clear thread.
 * - Flushes the FB and L2. Then, waits for completion (by polling)
 *   upto polling timeout.
 * - Invalidate L2.
 * - Disable the fbhubmmu mc stalling interrupt and unit interrupts.
 * - Disable the mmu fault buffer h/w setup.
 *
 * @return 0 in case of success, < 0 in case of failure.
 * Possible failure case:
 * - CPU polling timeout during FB or L2 flush operation (-EBUSY).
 */
int nvgpu_mm_suspend(struct gk20a *g);

/**
 * @brief Get the default big page size in bytes.
 *
 * @param g	[in]	The GPU.
 *
 * Default big page size:
 *  - Big page size will be different for each GPU family
 *    (i.e. gv11b, tu104).
 *
 * @return Valid big page size if big page support is enabled.
 * @return 0 if big page support is disabled.
 */
u32 nvgpu_mm_get_default_big_page_size(struct gk20a *g);

/**
 * @brief Get the available big page sizes.
 *
 * @param g	[in]	The GPU.
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
u32 nvgpu_mm_get_available_big_page_sizes(struct gk20a *g);

/**
 * @brief Setup the Memory Management hardware.
 *
 * @param g	[in]	The GPU.
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
int nvgpu_mm_setup_hw(struct gk20a *g);

#endif /* NVGPU_MM_H */
