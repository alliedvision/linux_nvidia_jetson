/*
 * Copyright (c) 2017-2021, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef NVGPU_VM_AREA_H
#define NVGPU_VM_AREA_H

#include <nvgpu/list.h>
#include <nvgpu/types.h>

struct vm_gk20a;
struct gk20a_as_share;
struct nvgpu_as_alloc_space_args;
struct nvgpu_as_free_space_args;
struct nvgpu_vm_remap_vpool;

/**
 * Carve out virtual address space from virtual memory context.
 * This is needed for fixed address mapping.
 */
struct nvgpu_vm_area {
	/**
	 * Entry into the list of VM areas owned by a virtual
	 * memory context.
	 */
	struct nvgpu_list_node vm_area_list;
	/**
	 * List of buffers mapped into this vm_area.
	 */
	struct nvgpu_list_node buffer_list_head;
	/**
	 * Flags used for allocation of vm area.
	 */
	u32 flags;
	/**
	 * Page size to be used for GPU mapping.
	 */
	u32 pgsz_idx;
	/**
	 * The base address of the vm_area.
	 */
	u64 addr;
	/**
	 * size of the vm_area.
	 */
	u64 size;
	/**
	 * Mark the vm area as sparse.
	 *
	 * @sa NVGPU_VM_AREA_ALLOC_SPARSE
 	 */
	bool sparse;

#ifdef CONFIG_NVGPU_REMAP
	/**
	 * Virtual pool for remap support of sparse VM areas.
	 */
	struct nvgpu_vm_remap_vpool *vpool;
#endif
};

static inline struct nvgpu_vm_area *
nvgpu_vm_area_from_vm_area_list(struct nvgpu_list_node *node)
{
	return (struct nvgpu_vm_area *)
		((uintptr_t)node - offsetof(struct nvgpu_vm_area,
					    vm_area_list));
};

/**
 * Allocation of vm area at fixed address.
 */
#define NVGPU_VM_AREA_ALLOC_FIXED_OFFSET		BIT32(0)

/**
 * Mark the vmarea as sparse: this means that the vmarea's entire range of PTEs
 * is mapped as sparse. Sparse mappings are mappings in which the valid bit is
 * set to 0, but the volatile (cached) bit is set to 1.
 *
 * The purpose here is to allow an oversubscription of physical memory
 * for a particular texture or other object.
 */
#define NVGPU_VM_AREA_ALLOC_SPARSE			BIT32(1)

/**
 * Enable REMAP control of the the vmarea.  REMAP uses a virtual
 * memory pool that provides control over each page in the vmarea.
 * Note that REMAP is only permitted with SPARSE vmareas.
 */
#define NVGPU_VM_AREA_ALLOC_REMAP			BIT32(2)

/**
 * @brief Allocate a virtual memory area with given size and start.
 *
 * @param vm [in]	Pointer virtual memory context from the range to be
 *			allocated.
 * @param pages [in]	Number of pages to be allocated.
 * @param page_size [in]Page size to be used for allocation.
 * @param addr [in/out]	Start address of the range.
 * @param flags [in]	Flags used for allocation.
 *			Flags are
 *			 NVGPU_VM_AREA_ALLOC_FIXED_OFFSET,
 *			 NVGPU_VM_AREA_ALLOC_SPARSE
 *
 * - Check the input flags.
 * - Find the page size index for the given @page_size.
 * - Find the page allocator for for the computed index.
 * - Initialise the memory for vm area and call #nvgpu_vm_area_alloc_memory()
 *   to allocate vm area.
 * - Fill the details in allocated vm_area.
 * - Add the vm_area to the list of vm areas in virtual memory context.
 * - If NVGPU_VM_AREA_ALLOC_SPARSE is set, unmap the allocated vm_area
 *   from the GPU.
 *
 * @return		Zero, for successful allocation.
 *			Suitable error code for failures.
 */
int nvgpu_vm_area_alloc(struct vm_gk20a *vm, u64 pages, u32 page_size,
			u64 *addr, u32 flags);

/**
 * @brief Free the virtual memory area.
 *
 * @param vm [in]	Pointer virtual memory context from
 *			the range to be allocated.
 * @param addr [in/out]	Start address of the vm area to be freed.
 *
 * - Find the vm_area by calling #nvgpu_vm_area_find().
 * - Remove the vm_area from the list.
 * - Remove and unmap the buffers associated with the vm_area by
 *   walking the list of buffers associated with it.
 * - Free the vm_area.
 *
 * @return		Zero.
 */
int nvgpu_vm_area_free(struct vm_gk20a *vm, u64 addr);

/**
 * @brief Find the virtual memory area from vm context.
 *
 * @param vm [in]	Pointer virtual memory context.
 * @param addr [in]	Base address of the vm area.
 *
 * - Walk the vm.vm_area_list, find the corresponding
 *   #nvgpu_vm_area struct.
 *
 * @return		#nvgpu_vm_area struct, for success.
 *			NULL, if it fails to find the vm_area.
 */
struct nvgpu_vm_area *nvgpu_vm_area_find(struct vm_gk20a *vm, u64 addr);

/**
 * @brief Find and validate the virtual memory area. This helps
 *  to validate the fixed offset buffer while GPU unmapping.
 *
 * @param vm [in]		Pointer virtual memory context.
 * @param addr [in]		Address of the mapped buffer.
 * @param map_size [in] 	Size of the mapping.
 * @param pgsz_idx [in]		Page size index used for mapping.
 *                              - Min: GMMU_PAGE_SIZE_SMALL
 *                              - Max: GMMU_PAGE_SIZE_KERNEL
 * @param pvm_area [out]	#nvgpu_vm_area struct.
 *
 * - Find the vm_area from the vm context.
 * - Validate the buffer mapping is not overlapping with other
 *   mappings in the vm_area.
 *
 * @return			#nvgpu_vm_area struct, for success.
 * @retval -EINVAL in case of invalid #map_size.
 * @retval -EINVAL in case of #map_addr is not page size aligned.
 * @retval -EINVAL if it fails to find the vm_area.
 */
int nvgpu_vm_area_validate_buffer(struct vm_gk20a *vm,
				  u64 map_addr, u64 map_size, u32 pgsz_idx,
				  struct nvgpu_vm_area **pvm_area);

#endif /* NVGPU_VM_AREA_H */
