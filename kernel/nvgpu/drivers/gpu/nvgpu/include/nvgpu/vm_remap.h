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

#ifndef NVGPU_VM_REMAP_H
#define NVGPU_VM_REMAP_H

#include <nvgpu/vm.h>
#include <nvgpu/types.h>

#ifdef __KERNEL__
#include <nvgpu/linux/vm_remap.h>
#elif defined(__NVGPU_POSIX__)
#include <nvgpu/posix/posix-vm_remap.h>
#endif

/*
 * Supported remap operation flags.
 */
#define NVGPU_VM_REMAP_OP_FLAGS_CACHEABLE        BIT32(1)
#define NVGPU_VM_REMAP_OP_FLAGS_ACCESS_NO_WRITE  BIT32(7)
#define NVGPU_VM_REMAP_OP_FLAGS_PAGESIZE_4K      BIT32(12)
#define NVGPU_VM_REMAP_OP_FLAGS_PAGESIZE_64K     BIT32(13)
#define NVGPU_VM_REMAP_OP_FLAGS_PAGESIZE_128K    BIT32(14)
/**
 * This structure describes a single remap operation (either a map or unmap).
 */
struct nvgpu_vm_remap_op {
	/**
	 * When a map/unmap operation is specified this field contains flags
	 * needed to determine the page size used to generate the map/unmap
	 * mem and virt offsets and/or flags used when setting up the mapping.
	 */
	u32 flags;

	/**
	 * When a map operation is specified this field can be used to specify
	 * the compressed kind for the mapping.  If the specified value is
	 * NVGPU_KIND_INVALID then no compression resources are requested and
	 * the #incompr_kind value is used for the mapping.  If a value other
	 * than NVGPU_KIND_INVALID is specified but there are no compression
	 * resources available for the mapping then the #incompr_kind value
	 * is used as a fallback for the mapping.  When an unmap operation
	 * is specified this value must be zero.
	 */
	s16 compr_kind;

	/**
	 * When a map operation is specified and the #compr_kind field is
	 * NVGPU_KIND_INVALID then this field specifies the incompressed
	 * kind to use for the mapping.  When an unmap operation is specified
	 * this value must be zero.
	 */
	s16 incompr_kind;

	/**
	 * This field is used to distinguish between a map and unmap operation.
	 * When this field is non-zero then it indicates a map operation with
	 * the value containing the handle to the physical memory buffer to
	 * map into the virtual pool.  When this field is zero then it
	 * indicates an unmap operation.
	 */
	u32 mem_handle;

	/**
	 * Page offset into the memory buffer referenced by #mem_handle from
	 * which physical memory should be mapped.
	 */
	u64 mem_offset_in_pages;

	/**
	 * Page offset into the virtual pool at which to start the mapping.
	 */
	u64 virt_offset_in_pages;

	/**
	 * Number of pages to map or unmap.
	 */
	u64 num_pages;
};

/*
 * Defined by each OS. Allows the common VM code do things to the OS specific
 * buffer structures.
 */
struct nvgpu_os_vm_remap_buffer;

/**
 * This structure describes a physical memory pool.
 * There is one physical memory pool for each physical memory buffer that
 * is mapped into the corresponding virtual pool.
 */
struct nvgpu_vm_remap_mpool {
	/**
	 * Red black tree node to the memory pool.
	 */
	struct nvgpu_rbtree_node node;

	/**
	  * Number of references to this physical memory pool.  This
	  * value increments for each map operation and decrements with
	  * each unmap operation that references the associated physical
	  * memory buffer tracked by #remap_os_buf.  When the reference
	  * count goes to zero then the reference to the associated
	  * physical memory buffer tracked by #remap_os_buf is released.
	  */
	struct nvgpu_ref ref;

	/**
	 * If non-NULL, the ref put function will check this l2 flag and issue
	 * a flush if necessary when releasing a mapping.
	 */
	bool *l2_flushed;

	/**
	 * OS-specific structure that tracks the associated physical memory
	 * buffer.
	 */
	struct nvgpu_vm_remap_os_buffer remap_os_buf;

	/**
	  * Pointer to virtual pool into which this physical memory pool
	  * is mapped.
	  */
	struct nvgpu_vm_remap_vpool *vpool;
};

static inline struct nvgpu_vm_remap_mpool *
nvgpu_vm_remap_mpool_from_ref(struct nvgpu_ref *ref)
{
	return (struct nvgpu_vm_remap_mpool *)
		((uintptr_t)ref -
			offsetof(struct nvgpu_vm_remap_mpool, ref));
}

static inline u64 nvgpu_vm_remap_page_size(struct nvgpu_vm_remap_op *op)
{
	u64 pagesize = 0;

	/* validate map/unmap_op ensures a single pagesize flag */
	if (op->flags & NVGPU_VM_REMAP_OP_FLAGS_PAGESIZE_4K) {
		pagesize = SZ_4K;
	} else if (op->flags & NVGPU_VM_REMAP_OP_FLAGS_PAGESIZE_64K) {
		pagesize = SZ_64K;
	} else if (op->flags & NVGPU_VM_REMAP_OP_FLAGS_PAGESIZE_128K) {
		pagesize = SZ_128K;
	}

	nvgpu_assert(pagesize);
	return pagesize;
}

static inline u32 nvgpu_vm_remap_page_size_flag(u64 pagesize)
{
	u32 flag = 0;

	if (pagesize == SZ_4K) {
		flag = NVGPU_VM_REMAP_OP_FLAGS_PAGESIZE_4K;
	} else if (pagesize == SZ_64K) {
		flag = NVGPU_VM_REMAP_OP_FLAGS_PAGESIZE_64K;
	} else if (pagesize == SZ_128K) {
		flag = NVGPU_VM_REMAP_OP_FLAGS_PAGESIZE_128K;
	}

	nvgpu_assert(flag);
	return flag;
}

/**
 * This structure describes a virtual memory pool.
 * There is one virtual memory pool for each sparse VM area allocation.
 * A virtual memory pool tracks the association between each mapped page
 * in the pool and the corresponding physical memory.
 */
struct nvgpu_vm_remap_vpool {
	/**
	 * Pointer to associated VM.
	 */
	struct vm_gk20a *vm;

	/**
	 * Pointer to associated VM area.
	 */
	struct nvgpu_vm_area *vm_area;

	/**
	 * Tree of physical memory pools that are currently mapped to this
	 * virtual pool.
	 */
	struct nvgpu_rbtree_node *mpools;

	/**
	 * Base offset in pages within the associated VM context of the
	 * virtual memory pool.  This value is specified to
	 * #nvgpu_vm_remap_vpool_create when the associated VM area is
	 * allocated.
	 */
	u64 base_offset_in_pages;

	/*
	 * Number of pages mapped into the virtual memory pool.
	 */
	u64 num_pages;

	/**
	 * Pointer to array of physical memory pool pointers (one per page
	 * in the virtual memory pool).
	 */
	struct nvgpu_vm_remap_mpool **mpool_by_page;
};

/**
 * @brief Process list of remap operations on a sparse VM area.
 *
 * @param vm [in]		Pointer to VM context.
 * @param ops [in]		Pointer to list of remap operations.
 * @param num_ops [in/out]	On input number of entries in #ops list.
 *				On output number of successful remap
 *				operations.
 *
 * This function processes one or more remap operations on the virtual memory
 * pool associatd with the target sparse VM area.  The operations are first
 * validated and then executed in order.
 *
 * - Acquire the vm.update_gmmu_lock.
 * - Get the associated VM area using #nvgpu_vm_find_area.
 * - Verify VM area is valid and has a valid virtual pool.
 * - Allocate (temporary) storage for a physical memory pool structure
 *   for each operation in the #ops list.
 * - Perform validation of each operation in the #ops list.
 * - For unmap operations validate that the specified virtual memory range
 *   resides entirely within the target virtual memory pool.
 * - For map operations validate that the specified physical memory range
 *   resides entirely within the associated physical memory buffer and that
 *   the specified virtual memory range resides entirely within the target
 *   virtual memory pool.  Also process compression resource requests including
 *   fallback handling if compression resources are desired but not available.
 * - If validation succeeds then proceed to process the remap operations
 *   in sequence.  Unmap operations remove the specified mapping from the
 *   GPU page tables (note this sets the sparse for each unmapped page).  Map
 *   operations insert the specified mapping into the GPU page tables.
 * - Update physical memory pool reference counts for each operation in the
 *   #ops list.  If a memory pool reference count goes to zero then release
 *   the reference to the associated physical memory buffer.
 * - Release the vm.update_gmmu_lock.
 *
 * @return		Zero if the virtual pool init succeeds.
 *			Suitable errors, for failures.
 * @retval -EINVAL if a value of zero is specified for #num_ops.
 * @retval -EINVAL if there is no valid virtual pool for the target VM area.
 * @retval -EINVAL if the specified virtual address range does not reside
 *                 entirely within the target VM area.
 * @retval -EINVAL if the specified physical address range does not reside
 *                 entirely within the source physical memory buffer.
 * @retval -ENOMEM if memory allocation for physical memory pool structures
 *                 fails.
 */
int nvgpu_vm_remap(struct vm_gk20a *vm,	struct nvgpu_vm_remap_op *ops,
		u32 *num_ops);

/**
 * @brief Create a virtual memory pool.
 *
 * @param vm [in]			Pointer to VM context.
 * @param vm_area [in]			Pointer to pointer VM area.
 * @param num_pages [in]		Number of pages in virtual memory pool.
 *
 * - Check that #num_pages is non-zero.
 * - Check that VM area is configured as sparse.
 * - Allocate memory for internal virtual pool management structures.
 * - Initialize virtual pool management structures including storing #vm
 *   and #num_pages arguments.
 * - Store virtual memory pool pointer in #vm_area.
 *
 * @return			Zero if the virtual pool create succeeds.
 *				Suitable errors, for failures.
 * @retval -EINVAL if a value of zero is specified for #num_pages.
 * @retval -EINVAL if the VM area is not configured as sparse.
 * @retval -ENOMEM if memory allocation for internal resources fails.
 */
int nvgpu_vm_remap_vpool_create(struct vm_gk20a *vm,
				struct nvgpu_vm_area *vm_area,
				u64 num_pages);

/**
 * @brief Destroy a virtual memory pool.
 *
 * @param vm [in]	Pointer to VM context.
 * @param vm_area [in]	Pointer to VM area.
 *
 * - Release references to physical memory pools (if any).
 * - Free memory used to track virtual memory pool state.
 *
 * @return		None.
 */
void nvgpu_vm_remap_vpool_destroy(struct vm_gk20a *vm,
				struct nvgpu_vm_area *vm_area);

#endif /* NVGPU_VM_REMAP_H */
