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

/**
 * @file
 *
 * This header contains the OS agnostic APIs for dealing with VMs. Most of the
 * VM implementation is system specific - it must translate from a platform's
 * representation of DMA'able memory to our nvgpu_mem notion.
 *
 * However, some stuff is platform agnostic. VM ref-counting and the VM struct
 * itself are platform agnostic. Also, the initialization and destruction of
 * VMs is the same across all platforms (for now).
 *
 * VM Design:
 * ----------------
 *
 *   The VM managment in nvgpu is split up as follows: a vm_gk20a struct which
 * defines an address space. Each address space is a set of page tables and a
 * GPU Virtual Address (GVA) allocator. Any number of channels may bind to a VM.
 *
 *     +----+  +----+     +----+     +-----+     +-----+
 *     | C1 |  | C2 | ... | Cn |     | VM1 | ... | VMn |
 *     +-+--+  +-+--+     +-+--+     +--+--+     +--+--+
 *       |       |          |           |           |
 *       |       |          +----->-----+           |
 *       |       +---------------->-----+           |
 *       +------------------------>-----------------+
 *
 *   Each VM also manages a set of mapped buffers (struct nvgpu_mapped_buf)
 * which corresponds to _user space_ buffers which have been mapped into this
 * VM. Kernel space mappings (created by nvgpu_gmmu_map()) are not tracked by
 * VMs. This may be an architectural bug, but for now it seems to be OK. VMs can
 * be closed in various ways - refs counts hitting zero, direct calls to the
 * remove routine, etc. Note: this is going to change. VM cleanup is going to be
 * homogonized around ref-counts. When a VM is closed all mapped buffers in the
 * VM are unmapped from the GMMU. This means that those mappings will no longer
 * be valid and any subsequent access by the GPU will fault. That means one must
 * ensure the VM is not in use before closing it.
 *
 *   VMs may also contain VM areas (struct nvgpu_vm_area) which are created for
 * the purpose of sparse and/or fixed mappings. If userspace wishes to create a
 * fixed mapping it must first create a VM area - either with a fixed address or
 * not. VM areas are reserved - other mapping operations will not use the space.
 * Userspace may then create fixed mappings within that VM area.
 */

#ifndef NVGPU_VM_H
#define NVGPU_VM_H

#include <nvgpu/kref.h>
#include <nvgpu/list.h>
#include <nvgpu/rbtree.h>
#include <nvgpu/types.h>
#include <nvgpu/gmmu.h>
#include <nvgpu/pd_cache.h>
#include <nvgpu/nvgpu_mem.h>
#include <nvgpu/allocator.h>

struct vm_gk20a;
struct nvgpu_vm_area;
struct nvgpu_sgt;
struct gk20a_comptag_allocator;
struct nvgpu_channel;

/*
 * Defined by each OS. Allows the common VM code do things to the OS specific
 * buffer structures.
 */
struct nvgpu_os_buffer;

#ifdef __KERNEL__
#include <nvgpu/linux/vm.h>
#elif defined(__NVGPU_POSIX__)
#include <nvgpu/posix/vm.h>
#else
/* QNX include goes here. */
#include <nvgpu_rmos/include/vm.h>
#endif

#define NVGPU_VM_NAME_LEN	32U

/**
 * This structure describes the properties of batch mapping/unmapping.
 */
struct vm_gk20a_mapping_batch {

	/**
	 * When we are unmapping a buffer from GPU address space, the
	 * translations will be teared down from GPU page table. The
	 * contents of the physical address need to be removed from L2 cache
	 * of the GPU core.
	 * The field describes whether the cache flushing is needed or not.
	 */
	bool gpu_l2_flushed;

	/**
	 * When we are unmapping a buffer from GPU address space, the
	 * translations will be teared down from GPU page table. The cached
	 * contents of the deleted translations of the page table need to
	 * invalidated from the translation look aside buffer.
	 * The field describes whether the TLB invalidation is needed or not.
	 */
	bool need_tlb_invalidate;
};

/**
 * This structure describes buffer mapped by the GPU.
 * When we map a buffer to GPU address space by calling #nvgpu_vm_map(), this
 * structure will be populated. It will be inserted in to virtual memory
 * context.
 * It is needed to avoid duplicate mapping of the same buffer in the same
 * virtual memory context.
 */
struct nvgpu_mapped_buf {
	/**
	 * Pointer to the #vm_gk20a struct where the buffer is mapped.
	 */
	struct vm_gk20a *vm;
	/**
	 * Pointer to nvgpu_vm_area.
	 * It helps for fixed offset mappings. If the user wants to do fixed
	 * address mappings, the user need to reserve an address space in the vm
	 * context by calling #vm_area_alloc(). The vm_area that belongs to the
	 * mapped buffer will be stored in this field.
	 */
	struct nvgpu_vm_area *vm_area;
	/**
	 * Number of references to the same buffer.
	 * If the new mapping already exists in the vm context, mapping
	 * call will just increment the reference count by one.
	 */
	struct nvgpu_ref ref;
	/**
	 * Red black tree node to the buffer.
	 */
	struct nvgpu_rbtree_node node;
	/** List of buffers. */
	struct nvgpu_list_node buffer_list;
	/**
	 * GPU virtual address used by the buffer mapping.
	 */
	u64 addr;
	/**
	 * Size of the buffer mapping.
	 */
	u64 size;
	/**
	 * Page size index used for mapping(4KB/64KB).
	 */
	u32 pgsz_idx;
	/**
	 * Flags describes the mapping properties.
	 */
	u32 flags;
	/**
	 * kind used for mapping.
	 */
	s16 kind;
	/**
	 * User provided GPU virtual address or not.
	 * It helps to identify whether the address space is managed
	 * by user space or not.
	 */
	bool va_allocated;
	/**
	 * Offset into compression tags pool if compression enabled.
	 */
	u32 ctag_offset;
	/**
	 * GMMU read/write flags specified when mapping was created.
	 */
	enum gk20a_mem_rw_flag rw_flag;
	/**
	 * Aperture specified when mapping was created
	 */
	enum nvgpu_aperture aperture;

	/**
	 * Os specific buffer structure.
	 * Separate from the nvgpu_os_buffer struct to clearly distinguish
	 * lifetime. A nvgpu_mapped_buf_priv will _always_ be wrapped by a
	 * struct nvgpu_mapped_buf; however, there are times when a struct
	 * nvgpu_os_buffer would be separate. This aims to prevent dangerous
	 * usage of container_of() or the like in OS code.
	 */
	struct nvgpu_mapped_buf_priv os_priv;
};

static inline struct nvgpu_mapped_buf *
nvgpu_mapped_buf_from_buffer_list(struct nvgpu_list_node *node)
{
	return (struct nvgpu_mapped_buf *)
		((uintptr_t)node - offsetof(struct nvgpu_mapped_buf,
					    buffer_list));
}

static inline struct nvgpu_mapped_buf *
mapped_buffer_from_rbtree_node(struct nvgpu_rbtree_node *node)
{
	return (struct nvgpu_mapped_buf *)
		  ((uintptr_t)node - offsetof(struct nvgpu_mapped_buf, node));
}

/**
 * Virtual Memory context.
 * It describes the address information, synchronisation objects and
 * information about the allocators.
 */
struct vm_gk20a {
	/**
	 * Pointer to the GPU's memory management state.
	 */
	struct mm_gk20a *mm;
	/**
	 * This describes the address space id of the
	 * address space allocated.
	 */
	struct gk20a_as_share *as_share;
	/** Name of the Virtual Memory context. */
	char name[NVGPU_VM_NAME_LEN];

	/** Start GPU address of the context. */
	u64 virtaddr_start;
	/** End GPU address of the context. */
	u64 va_limit;

	/** Number of buffers using the context. */
	u32 num_user_mapped_buffers;

	/**
	 * To enable large page support (64KB).
	 */
	bool big_pages;
	/**
	 * Enable Compression tags.
	 * It is not enabled for safety build.
	 */
	bool enable_ctag;

	/** Page size used for mappings with this address space. */
	u32 big_page_size;

	/** Whether this address space is managed by user space or not. */
	bool userspace_managed;
	/** GPU and CPU using same address space or not. */
	bool unified_va;

	/**
	 * Describes the GPU page table levels.
	 * It describes number of bits required for every level of gpu
	 * page table and provides method to update the entries in the
	 * corresponding levels.
	 */
	const struct gk20a_mmu_level *mmu_levels;

	/** Number of references to this context.*/
	struct nvgpu_ref ref;
	/**
	 * Lock to synchronise the operations like add and delete of a
	 * page table entry and walking the page table in this VM context.
	 */
	struct nvgpu_mutex update_gmmu_lock;
	/**
	 * GMMU page directory for this context.
	 * It describes the list of PDEs or PTEs associated in the GMMU.
	 */
	struct nvgpu_gmmu_pd pdb;

	/**
	 * Pointers to different types of page allocators.
	 * These structs define the address spaces. In some cases it's possible
	 * to merge address spaces (user and user_lp) and in other cases it's
	 * not. vma[] allows the code to be agnostic to this by always using
	 * address spaces through this pointer array.
	 * #nvgpu_vm_init_vma() will initialise this allocators
	 * for different address ranges provided.
	 */
	struct nvgpu_allocator *vma[GMMU_NR_PAGE_SIZES];
	struct nvgpu_allocator kernel;
	struct nvgpu_allocator user;
	struct nvgpu_allocator user_lp;

	/**
	 * RB tree having the buffers associated with this vm context.
	 */
	struct nvgpu_rbtree_node *mapped_buffers;
	/**
	 * List of vm_area associated with this vm context.
	 */
	struct nvgpu_list_node vm_area_list;

#ifdef CONFIG_NVGPU_GR_VIRTUALIZATION
	u64 handle;
#endif
	/** Supported page sizes. */
	u32 gmmu_page_sizes[GMMU_NR_PAGE_SIZES];

	/**
	 * If non-NULL, kref_put will use this batch when
	 * unmapping. Must hold vm->update_gmmu_lock.
	 */
	struct vm_gk20a_mapping_batch *kref_put_batch;

#ifdef CONFIG_NVGPU_SW_SEMAPHORE

	/*
	 * For safety it is not enabled.
	 * Each address space needs to have a semaphore pool.
	 */
	struct nvgpu_semaphore_pool *sema_pool;
#endif
	/**
	 * Create sync point read only map for sync point range.
	 * Channels sharing same vm will also share same sync point ro map.
	 */
	u64 syncpt_ro_map_gpu_va;
	/**
	 * Protect allocation of sync point map.
	 */
	struct nvgpu_mutex syncpt_ro_map_lock;
};

/*
 * Mapping flags.
 */
#define NVGPU_VM_MAP_FIXED_OFFSET			BIT32(0)
#define NVGPU_VM_MAP_CACHEABLE				BIT32(1)
#define NVGPU_VM_MAP_IO_COHERENT			BIT32(2)
#define NVGPU_VM_MAP_UNMAPPED_PTE			BIT32(3)
#define NVGPU_VM_MAP_DIRECT_KIND_CTRL			BIT32(4)
#define NVGPU_VM_MAP_L3_ALLOC				BIT32(5)
#define NVGPU_VM_MAP_PLATFORM_ATOMIC			BIT32(6)
#define NVGPU_VM_MAP_TEGRA_RAW				BIT32(7)

#define NVGPU_VM_MAP_ACCESS_DEFAULT			0U
#define NVGPU_VM_MAP_ACCESS_READ_ONLY			1U
#define NVGPU_VM_MAP_ACCESS_READ_WRITE			2U

#define NVGPU_KIND_INVALID				S16(-1)

/**
 * @brief Get the reference to the virtual memory context.
 *
 * @param vm [in]	Pointer to virtual memory context.
 *
 * - Increment the reference in the associated context.
 *
 * @return		None.
 */
void nvgpu_vm_get(struct vm_gk20a *vm);

/**
 * @brief Release the reference to the virtual memory context.
 *
 * @param vm [in]	Pointer to virtual memory context.
 *
 * - Decrement the reference in the associated context.
 *
 * @return		None.
 */
void nvgpu_vm_put(struct vm_gk20a *vm);

/**
 * @brief Get the associated address space associated
 *  with virtual memory context..
 *
 * @param vm [in]	Pointer to virtual memory context.
 *
 * - Return the id of address space share associated with the
 *   virtual memory context.
 *
 * @return		address space share id.
 */
int vm_aspace_id(struct vm_gk20a *vm);

/**
 * @brief Bind the virtual memory context to the given channel.
 *
 * @param vm [in]       Pointer to virtual memory context.
 * @param ch [in]	Pointer to nvgpu channel.
 *
 * - Increment reference count of virtual memory context.
 * - Assign the virtual memory context to channel virtual memory context.
 * - Program the different hardware blocks of GPU with addresses associated
 *   with virtual memory context.
 *
 * @return              Zero, always.
 */
int nvgpu_vm_bind_channel(struct vm_gk20a *vm, struct nvgpu_channel *ch);

/**
 * @brief Check big page translation is possible with the given address
 *  and size.
 *
 * @param vm [in]	Pointer to virtual memory context.
 * @param base [in]	The virtual address.
 * @param size [in]	The size.
 *
 * - Get the big page size mask.
 * - Compute the base and size by ANDing with big page size mask.
 * - If the computed base or size is non zero, returns FALSE.
 *
 * @return              TRUE, if it supports.
 *			FALSE, if it is not supported.
 */
bool nvgpu_big_pages_possible(struct vm_gk20a *vm, u64 base, u64 size);

/**
 * @brief Determine how many bits of the address space is covered by
 *  last level PDE.
 *
 * @param g [in]		The GPU.
 * @param big_page_size [in]	Big page size supported by GMMU.
 *
 * - Go to the last level before page table entry level and return
 *   the mmu_levels[x].lo_bit.
 *
 * @return              number of bits with last level of entry.
 */
u32 nvgpu_vm_pde_coverage_bit_count(struct gk20a *g, u64 big_page_size);

/**
 * @brief Eliminates redundant cache flushes and invalidates.
 *
 * @param mapping_batch [in/out]	Pointer to mapping batch.
 *
 * - Set mapping_batch.gpu_l2_flushed and mapping_batch.need_tlb_invalidate
 *   to false.
 *
 * @return			None.
 */
void nvgpu_vm_mapping_batch_start(struct vm_gk20a_mapping_batch *mapping_batch);

/**
 * @brief Flushes cache and invalidates TLB if it needed.
 *
 * @param vm [in]		Pointer to virtual memory context.
 * @param mapping_batch [in]	Pointer to mapping batch.
 *
 * - Get mapping_batch.gpu_l2_flushed and mapping_batch.need_tlb_invalidate.
 * - Flush and invalidate as the values read.
 *
 * @return			None.
 */
void nvgpu_vm_mapping_batch_finish(
	struct vm_gk20a *vm, struct vm_gk20a_mapping_batch *mapping_batch);

/**
 * @brief Does flushes cache and invalidates TLB if it needed.
 *
 * @param vm [in]		Pointer to virtual memory context.
 * @param mapping_batch [in]	Pointer to mapping batch.
 *
 * - Acquire the vm.update_gmmu_lock.
 * - Get mapping_batch.gpu_l2_flushed and mapping_batch.need_tlb_invalidate.
 * - Flush and invalidate as the values read.
 * - Release the lock hold.
 *
 * @return			None.
 */
void nvgpu_vm_mapping_batch_finish_locked(
	struct vm_gk20a *vm, struct vm_gk20a_mapping_batch *mapping_batch);

/**
 * @brief Get the number of buffers mapped in given virtual memory context
 *  and reference to the buffers.
 *
 * @param vm [in]		Pointer to virtual memory context.
 * @param mapped_buffers [out]	Pointer to array of struct nvgpu_mapped_buf.
 * @param num_buffers [out]	number of buffers.
 *
 * - If virtual memory context is managed by user space, return zero.
 * - Acquire the vm.update_gmmu_lock.
 * - Allocate memory for buffer list.
 * - Fill the buffer list by walking the RB tree contains the mapped buffers.
 * - Increment the mapped_buffer.ref by one.
 * - Assign the allocated buffer list to @mapped_buffers and update
 * - @num_buffers.
 * - Get mapping_batch.gpu_l2_flushed and mapping_batch.need_tlb_invalidate.
 * - Flush and invalidate as the values read.
 * - Release the lock hold.
 *
 * @return			Zero.
 */
int nvgpu_vm_get_buffers(struct vm_gk20a *vm,
			 struct nvgpu_mapped_buf ***mapped_buffers,
			 u32 *num_buffers);

/**
 * @brief Release the reference to given list of buffers.
 *
 * @param vm [in]			Pointer to virtual memory context.
 * @param mapped_buffers [in/out]	Pointer to array of struct
 *					#nvgpu_mapped_buf.
 * @param num_buffers [in]		number of buffers.
 *
 * - Acquire the vm.update_gmmu_lock.
 * - Walk the @mapped_buffers list and decrement mapped_buffer.ref by one.
 * - Release the lock hold.
 * - Free the memory @mapped_buffers.
 *
 * @return			None.
 */
void nvgpu_vm_put_buffers(struct vm_gk20a *vm,
			  struct nvgpu_mapped_buf **mapped_buffers,
			  u32 num_buffers);

/**
 * @brief Check the buffer already mapped in the given VM context. It is a
 *  OS specific call.
 *
 * @param vm [in]		Pointer to virtual memory context.
 * @param os_buf [in]		Pointer to #nvgpu_os_buffer struct.
 * @param map_addr [in]		Address where it is mapped. It is valid for
 *				fixed mappings.
 * @param flags [in]		Flags describes the properties of the mapping.
 *                              - Min: NVGPU_VM_MAP_FIXED_OFFSET
 *                              - Max: NVGPU_VM_MAP_PLATFORM_ATOMIC
 * @param kind [in]		Kind parameter.
 *
 * - Acquire the vm.update_gmmu_lock.
 * - Walk the tree and find the buffer by calling nvgpu_vm_find_mapped_buf() or
 *   nvgpu_vm_find_mapped_buf_reverse using the input flags.
 * - Release the lock held.
 *
 * @return			#nvgpu_mapped_buf struct, if it finds.
 * @retval NULL if #nvgpu_mapped_buf struct is not found.
 * @retval NULL in case of invalid #flags.
 */
struct nvgpu_mapped_buf *nvgpu_vm_find_mapping(struct vm_gk20a *vm,
					       struct nvgpu_os_buffer *os_buf,
					       u64 map_addr,
					       u32 flags,
					       s16 kind);


/**
 * @brief Map #sgt into the GPU address space described by #vm.
 *
 * @param vm [in]			Pointer to virtual memory context.
 * @param os_buf [in]			Pointer to #nvgpu_os_buffer struct.
 * @param sgt [in]			Scatter gather table pointer describes
 *					the buffer.
 * @param map_addr [in]			GPU virtual address if it is fixed
 *					mapping.
 * @param map_size [in]			Size of the mapping.
 * @param phys_offset [in]		Offset of the mapping to start.
 * @param rw [in]			Describes the mapping(READ/WRITE).
 *                                     - Min: gk20a_mem_flag_none
 *                                     - Max: gk20a_mem_flag_write_only
 * @param flags [in]			Mapping is fixed or not.
 *                                     - Min: NVGPU_VM_MAP_FIXED_OFFSET
 *                                     - Max: NVGPU_VM_MAP_PLATFORM_ATOMIC
 * @param compr_kind [in]		Default map caching key.
 * @param incompr_kind [in]		Map caching key.
 * @param batch [in]			Describes TLB invalidation and cache
 *					flushing.
 * @param aperture [in]			System memory or VIDMEM.
 *                                      - Min: APERTURE_SYSMEM
 *                                      - Max: APERTURE_VIDMEM
 * @param mapped_buffer_arg [in/out]	Mapped buffer.
 *
 * - Validate the inputs.
 * - Acquire the vm.update_gmmu_lock.
 * - Get the buffer size by calling #nvgpu_os_buf_get_size().
 * - Check the buffer is mapped already by calling #nvgpu_vm_find_mapping().
 * - If the buffer is already mapped, return the buffer.
 * - If it is fixed mapping, validate the @size and @phys_offset.
 * - Check the buffer can be mapped by computing the page table entry size.
 * - Call HAL specific map function to map the buffer.
 * - Increase the reference in mapped buffer.
 * - Insert the mapped buffer in VM context by calling
 *   #nvgpu_insert_mapped_buf().
 * - Release the lock held.
 *
 * @return			Zero, for successful mapping.
 *				Suitable errors, for failures.
 * @retval -EINVAL in case of invalid #rw.
 * @retval -EINVAL in case of invalid #flags.
 * @retval -EINVAL in case of invalid #aperture.
 */
int nvgpu_vm_map(struct vm_gk20a *vm,
		 struct nvgpu_os_buffer *os_buf,
		 struct nvgpu_sgt *sgt,
		 u64 map_addr,
		 u64 map_size,
		 u64 phys_offset,
		 enum gk20a_mem_rw_flag rw,
		 u32 map_access_requested,
		 u32 flags,
		 s16 compr_kind,
		 s16 incompr_kind,
		 struct vm_gk20a_mapping_batch *batch,
		 enum nvgpu_aperture aperture,
		 struct nvgpu_mapped_buf **mapped_buffer_arg);


/**
 * @brief Unmap #offset into the GPU address space described by #vm.
 *
 * @param vm [in]		Pointer to virtual memory context.
 * @param offset [in]		Offset of the mapping to unmap.
 * @param batch [in]		Describes TLB invalidation and cache
 *				flushing.
 *
 * - Validate the inputs.
 * - Acquire the vm.update_gmmu_lock.
 * - Get the buffer calling #nvgpu_vm_find_mapping().
 * - If it is fixed mapping, wait for all references to leave
 *   by triggering 100ms software timer.
 * - Check the buffer references. When it becomes null release the buffer by
 *   calling HAL specific unmap function to unmap the buffer.
 * - Remove it from VM context.
 * - Call #nvgpu_vm_unmap_system() to free some OS specific data.
 *
 * @return				None.
 */
void nvgpu_vm_unmap(struct vm_gk20a *vm, u64 offset,
		    struct vm_gk20a_mapping_batch *batch);

/**
 * @brief Os specific unmap function.
 *
 * @param mapped_buffer [in]	Pointer to #nvgpu_mapped_buf struct.
 *
 * - Os specific unmap operation to free some OS specific data of
 * - the given dma buffer.
 *
 * @return			None.
 */
void nvgpu_vm_unmap_system(struct nvgpu_mapped_buf *mapped_buffer);

/**
 * @brief Unmap and free the mapped buffer from VM context.
 *
 * @param nvgpu_ref [in]	Pointer to #nvgpu_ref struct.
 *
 * - Get the mapped buffer by calling #nvgpu_mapped_buf_from_ref().
 * - Call #nvgpu_vm_do_unmap() to unmap and free the mapped buffer.
 *   - #nvgpu_vm_do_unmap() will do the following
 *      - Call HAL specific g.ops.mm.gmmu.unmap() to unmap the buffer.
 *      - Remove the mapped buffer from mapped buffer list.
 *      - Free the memory allocated for mapped buffer.
 *
 * @return			None.
 */
void nvgpu_vm_unmap_ref_internal(struct nvgpu_ref *ref);

/**
 * @brief Os specific function to get the size of allocated dma buffer.
 *
 * @param mapped_buffer [in]	Pointer to OS specific
 *  #nvgpu_os_buffer struct.
 *
 * - OS specific function to query the size of allocated dma buffer.
 *
 * @return			Size of the buffer.
 */
u64 nvgpu_os_buf_get_size(struct nvgpu_os_buffer *os_buf);

/*
 * These all require the VM update lock to be held.
 */

/**
 * @brief Find the mapped buffer in VM context for the given address.
 *
 * @param vm [in]	Pointer to VM context.
 * @param addr [in]	GPU virtual address.
 *
 * - Get the root node by accessing vm.mapped_buffers.
 * - Search the RB tree with the given address @addr to get the
 *   mapped buffer by calling #nvgpu_rbtree_search().
 *
 * @return		Pointer to #nvgpu_mapped_buf struct, if
 *			mapping exists.
 *			NULL,if not exists.
 */
struct nvgpu_mapped_buf *nvgpu_vm_find_mapped_buf(
	struct vm_gk20a *vm, u64 addr);

/**
 * @brief Find the mapped buffer in VM context with in the given
 *  address.
 *
 * @param vm [in]	Pointer to VM context.
 * @param addr [in]	GPU virtual address.
 *
 * - Get the root node by accessing vm.mapped_buffers.
 * - Search the RB tree with in the range of the given address
 *   @addr to get the mapped buffer by calling #nvgpu_rbtree_range_search().
 *
 * @return		Pointer to #nvgpu_mapped_buf struct, if
 *			mapping exists.
 *			NULL,if not exists.
 */
struct nvgpu_mapped_buf *nvgpu_vm_find_mapped_buf_range(
	struct vm_gk20a *vm, u64 addr);

/**
 * @brief Find the mapped buffer in VM context that is less than
 *  the given address.
 *
 * @param vm [in]	Pointer to VM context.
 * @param addr [in]	GPU virtual address.
 *
 * - Get the root node by accessing vm.mapped_buffers.
 * - Search the RB tree to get the mapped buffer by calling
 *   #nvgpu_rbtree_less_than_search().
 *
 * @return		Pointer to #nvgpu_mapped_buf struct, if
 *			mapping exists.
 *			NULL,if not exists.
 */
struct nvgpu_mapped_buf *nvgpu_vm_find_mapped_buf_less_than(
	struct vm_gk20a *vm, u64 addr);

/**
 * @brief Insert the mapped buffer in VM context.
 *
 * @param vm [in]		Pointer to VM context.
 * @param mapped_buffer [in]	Pointer to #nvgpu_mapped_buf struct.
 *
 * - Create a RB tree node with key_start as mapped_buffer.addr
 *   and key_end as the sum of mapped_buffer.addr and size.
 * - Get the root node by accessing vm.mapped_buffers.
 * - Insert the node using root node by calling #nvgpu_rbtree_insert().
 *
 * @return		None.
 */
void nvgpu_insert_mapped_buf(struct vm_gk20a *vm,
			    struct nvgpu_mapped_buf *mapped_buffer);

/**
 * @brief Initialize a preallocated virtual memory context.
 *
 * @param mm [in]		Pointer to the given GPU's memory management
 *				state.
 * @param vm [in/out]		Pointer to virtual memory context.
 * @param big_page_size [in]	Size of big pages associated with
 *				this VM.
 * @param low_hole [in]		The size of the low hole
 *				(non-addressable memory at the bottom of
 *				the address space).
 * @param user_reserved [in]	Space reserved for user allocations.
 * @param kernel_reserved [in]	Space reserved for kernel only allocations.
 * @param small_big_split [in]	Specifies small big page address split.
 * @param big_pages [in]	If true then big pages are possible in the
 *				VM. Note this does not guarantee that big
 *				pages will be possible.
 * @param unified_va [in]       If true then GPU and CPU are using same address
 *				space.
 * @param name [in]		Name of the address space.
 *
 * - Initialise some basic properties like supported page sizes, name, whether
 *   the address space is unified, address ranges and whether the address space
 *   is managed by user/kernel of the virtual memory context by calling
 *   #nvgpu_vm_init_attributes().
 * - Call HAL specific #vm_as_alloc_share() to allocate share id for the
 *   virtual memory context, if ops.mm.vm_as_alloc_share is not null.
 * - Initialise the page table by calling #nvgpu_gmmu_init_page_table().
 * - Initialise the address ranges and corresponding allocators by calling
 *   #nvgpu_vm_init_vma_allocators().
 * - Initialise synchronisation primitives.
 *
 * @return			Zero, for success.
 *				Suitable errors for failures.
 * @retval -ENOMEM if overlap between user and kernel spaces.
 * @retval -EINVAL if #kernel_reserved is 0.
 */

int nvgpu_vm_do_init(struct mm_gk20a *mm,
		     struct vm_gk20a *vm,
		     u32 big_page_size,
		     u64 low_hole,
		     u64 user_reserved,
		     u64 kernel_reserved,
		     u64 small_big_split,
		     bool big_pages,
		     bool userspace_managed,
		     bool unified_va,
		     const char *name);

/**
 * @brief This function creates an address space for a specific vm.
 *        @sa #nvgpu_init_mm_support().
 *
 * @param g [in]		The GPU super structure.
 * @param big_page_size [in]	Size of big pages associated with
 *				this VM.
 * @param low_hole [in]		The size of the low hole
 *				(non-addressable memory at the bottom of
 *				the address space).
 * @param user_reserved [in]	Space reserved for user allocations.
 * @param kernel_reserved [in]	Space reserved for kernel only allocations.
 * @param small_big_split [in]	Specifies small big page address split.
 * @param big_pages [in]	If true then big pages are possible in the
 *				VM. Note this does not guarantee that big
 *				pages will be possible.
 * @param unified_va [in]	If true then GPU and CPU are using same address
 *				space.
 * @param name [in]		Name of the address space.
 *
 * This function initializes an address space according to the following map:
 *
 *     +--+ 0x0
 *     |  |
 *     +--+ @low_hole
 *     |  |
 *     ~  ~   This is the "user" section, @user_reserved.
 *     |  |
 *     +--+ @aperture_size - @kernel_reserved
 *     |  |
 *     ~  ~   This is the "kernel" section, @kernel_reserved.
 *     |  |
 *     +--+ @aperture_size
 *
 * The user section is there for what ever is left over after the @low_hole and
 * @kernel_reserved memory have been portioned out. The @kernel_reserved is
 * always present at the top of the memory space and the @low_hole is always at
 * the bottom.
 *
 * For certain address spaces a "user" section makes no sense (bar1, etc) so in
 * such cases the @kernel_reserved and @low_hole should sum to exactly
 * @aperture_size.
 *
 * - Call #nvgpu_vm_do_init() to initialise the address space.
 *
 * @return		Pointer to struct #vm_gk20a, if success.
 * @retval NULL insufficient system resources to create vm.
 */
struct vm_gk20a *nvgpu_vm_init(struct gk20a *g,
			       u32 big_page_size,
			       u64 low_hole,
			       u64 user_reserved,
			       u64 kernel_reserved,
			       u64 small_big_split,
			       bool big_pages,
			       bool userspace_managed,
			       bool unified_va,
			       const char *name);

/*
 * These are private to the VM code but are unfortunately used by the vgpu code.
 * It appears to be used for an optimization in reducing the number of server
 * requests to the vgpu server. Basically the vgpu implementation of
 * map_global_ctx_buffers() sends a bunch of VA ranges over to the RM server.
 * Ideally the RM server can just batch mappings but until such a time this
 * will be used by the vgpu code.
 */
/**
 * @brief Allocate virtual address from the VM context.
 *
 * @param vm [in]	Pointer to VM context.
 * @param size [in]	Size to be allocated.
 * @param pgsz_idx [in]	Page size index.
 *			- Min: GMMU_PAGE_SIZE_SMALL
 *			- Max: GMMU_PAGE_SIZE_KERNEL
 *
 * - Get the #nvgpu_allocator struct for the given @pgsz_idx.
 * - Call #nvgpu_alloc_pte() to allocate virtual address from the allocator.
 *
 * @return		Virtual address allocated.
 */
u64 nvgpu_vm_alloc_va(struct vm_gk20a *vm, u64 size, u32 pgsz_idx);

/**
 * @brief Free the virtual address from the VM context.
 *
 * @param vm [in]	Pointer to VM context.
 * @param addr [in]	Virtual address to be freed.
 * @param pgsz_idx [in]	Page size index.
 *
 * - Get the #nvgpu_allocator struct.
 * - Free the @addr using the #nvgpu_allocator by calling #nvgpu_free().
 *
 * @return		None.
 */
void nvgpu_vm_free_va(struct vm_gk20a *vm, u64 addr, u32 pgsz_idx);

#endif /* NVGPU_VM_H */
