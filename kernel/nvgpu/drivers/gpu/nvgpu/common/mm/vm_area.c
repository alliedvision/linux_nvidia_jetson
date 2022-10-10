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

#include <nvgpu/vm.h>
#include <nvgpu/vm_area.h>
#include <nvgpu/barrier.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/static_analysis.h>
#ifdef CONFIG_NVGPU_REMAP
#include <nvgpu/vm_remap.h>
#endif

struct nvgpu_vm_area *nvgpu_vm_area_find(struct vm_gk20a *vm, u64 addr)
{
	struct nvgpu_vm_area *vm_area;

	nvgpu_list_for_each_entry(vm_area, &vm->vm_area_list,
				  nvgpu_vm_area, vm_area_list) {
		if (addr >= vm_area->addr) {
			if (addr < nvgpu_safe_add_u64(vm_area->addr,
							vm_area->size)) {
				return vm_area;
			}
		}
	}

	return NULL;
}

int nvgpu_vm_area_validate_buffer(struct vm_gk20a *vm,
				  u64 map_addr, u64 map_size, u32 pgsz_idx,
				  struct nvgpu_vm_area **pvm_area)
{
	struct gk20a *g = vm->mm->g;
	struct nvgpu_vm_area *vm_area;
	struct nvgpu_mapped_buf *buffer;
	u64 map_end;

	/* can wrap around with insane map_size; zero is disallowed too */
	if (((U64_MAX - map_size) < map_addr) || (map_size == 0ULL)) {
		nvgpu_warn(g, "fixed offset mapping with invalid map_size");
		return -EINVAL;
	}
	map_end = map_addr + map_size;

	if ((map_addr &
	     nvgpu_safe_sub_u64(U64(vm->gmmu_page_sizes[pgsz_idx]), U64(1)))
	     != 0ULL) {
		nvgpu_err(g, "map offset must be buffer page size aligned 0x%llx",
			  map_addr);
		return -EINVAL;
	}

	/* Find the space reservation, but it's ok to have none for
	 * userspace-managed address spaces */
	vm_area = nvgpu_vm_area_find(vm, map_addr);
	if ((vm_area == NULL) && !vm->userspace_managed) {
		nvgpu_warn(g, "fixed offset mapping without space allocation");
		return -EINVAL;
	}

	/* Mapped area should fit inside va, if there's one */
	if (vm_area != NULL) {
		if (map_end > nvgpu_safe_add_u64(vm_area->addr,
							    vm_area->size)) {
			nvgpu_warn(g,
				"fixed offset mapping size overflows va node");
			return -EINVAL;
		}
	}

	/* check that this mapping does not collide with existing
	 * mappings by checking the buffer with the highest GPU VA
	 * that is less than our buffer end */
	buffer = nvgpu_vm_find_mapped_buf_less_than(
		vm, map_end);
	if (buffer != NULL) {
		if (nvgpu_safe_add_u64(buffer->addr, buffer->size) > map_addr) {
			nvgpu_warn(g, "overlapping buffer map requested");
			return -EINVAL;
		}
	}

	*pvm_area = vm_area;

	return 0;
}

static int nvgpu_vm_area_alloc_get_pagesize_index(struct vm_gk20a *vm,
					u32 *pgsz_idx_ptr, u32 page_size)
{
	u32 pgsz_idx = *pgsz_idx_ptr;

	for (; pgsz_idx < GMMU_NR_PAGE_SIZES; pgsz_idx++) {
		if (vm->gmmu_page_sizes[pgsz_idx] == page_size) {
			break;
		}
	}

	*pgsz_idx_ptr = pgsz_idx;

	if (pgsz_idx > GMMU_PAGE_SIZE_BIG) {
		return -EINVAL;
	}

	/*
	 * pgsz_idx isn't likely to get too crazy, since it starts at 0 and
	 * increments but this ensures that we still have a definitely valid
	 * page size before proceeding.
	 */
	nvgpu_speculation_barrier();

	if (!vm->big_pages && (pgsz_idx == GMMU_PAGE_SIZE_BIG)) {
		return -EINVAL;
	}

	return 0;
}

static int nvgpu_vm_area_alloc_memory(struct nvgpu_allocator *vma, u64 our_addr,
			u64 pages, u32 page_size, u32 flags,
			u64 *vaddr_start_ptr)
{
	u64 vaddr_start = 0;

	if ((flags & NVGPU_VM_AREA_ALLOC_FIXED_OFFSET) != 0U) {
		vaddr_start = nvgpu_alloc_fixed(vma, our_addr,
						pages *
						(u64)page_size,
						page_size);
	} else {
		vaddr_start = nvgpu_alloc_pte(vma,
					      pages *
					      (u64)page_size,
					      page_size);
	}

	if (vaddr_start == 0ULL) {
		return -ENOMEM;
	}

	*vaddr_start_ptr = vaddr_start;
	return 0;
}

static int nvgpu_vm_area_alloc_gmmu_map(struct vm_gk20a *vm,
			struct nvgpu_vm_area *vm_area, u64 vaddr_start,
			u32 pgsz_idx, u32 flags)
{
	struct gk20a *g = vm->mm->g;

	if ((flags & NVGPU_VM_AREA_ALLOC_SPARSE) != 0U) {
		u64 map_addr = g->ops.mm.gmmu.map(vm, vaddr_start,
					 NULL,
					 0,
					 vm_area->size,
					 pgsz_idx,
					 0,
					 0,
					 flags,
					 gk20a_mem_flag_none,
					 false,
					 true,
					 false,
					 NULL,
					 APERTURE_INVALID);
		if (map_addr == 0ULL) {
			return -ENOMEM;
		}

		vm_area->sparse = true;
	}
	nvgpu_list_add_tail(&vm_area->vm_area_list, &vm->vm_area_list);

	return 0;
}

int nvgpu_vm_area_alloc(struct vm_gk20a *vm, u64 pages, u32 page_size,
			u64 *addr, u32 flags)
{
	struct gk20a *g = vm->mm->g;
	struct nvgpu_allocator *vma;
	struct nvgpu_vm_area *vm_area;
	u64 vaddr_start = 0;
	u64 our_addr = *addr;
	u32 pgsz_idx = GMMU_PAGE_SIZE_SMALL;
	int err = 0;

	/*
	 * If we have a fixed address then use the passed address in *addr. This
	 * corresponds to the o_a field in the IOCTL. But since we do not
	 * support specific alignments in the buddy allocator we ignore the
	 * field if it isn't a fixed offset.
	 */
	if ((flags & NVGPU_VM_AREA_ALLOC_FIXED_OFFSET) != 0U) {
		our_addr = *addr;
	}

	nvgpu_log(g, gpu_dbg_map,
		  "ADD vm_area: pgsz=%#-8x pages=%-9llu a/o=%#-14llx flags=0x%x",
		  page_size, pages, our_addr, flags);

	if (nvgpu_vm_area_alloc_get_pagesize_index(vm, &pgsz_idx,
							page_size) != 0) {
		return -EINVAL;
	}

	vm_area = nvgpu_kzalloc(g, sizeof(*vm_area));
	if (vm_area == NULL) {
		return -ENOMEM;
	}

	vma = vm->vma[pgsz_idx];

	err = nvgpu_vm_area_alloc_memory(vma, our_addr, pages, page_size,
					flags, &vaddr_start);
	if (err != 0) {
		goto free_vm_area;
	}

	vm_area->flags = flags;
	vm_area->addr = vaddr_start;
	vm_area->size = (u64)page_size * pages;
	vm_area->pgsz_idx = pgsz_idx;
	nvgpu_init_list_node(&vm_area->buffer_list_head);
	nvgpu_init_list_node(&vm_area->vm_area_list);

#ifdef CONFIG_NVGPU_REMAP
	if ((flags & NVGPU_VM_AREA_ALLOC_SPARSE) != 0U) {
		err = nvgpu_vm_remap_vpool_create(vm, vm_area, pages);
		if (err != 0) {
			goto free_vaddr;
		}
	}
#endif

	nvgpu_mutex_acquire(&vm->update_gmmu_lock);

	err = nvgpu_vm_area_alloc_gmmu_map(vm, vm_area, vaddr_start,
					pgsz_idx, flags);
	if (err != 0) {
		nvgpu_mutex_release(&vm->update_gmmu_lock);
		goto free_vaddr;
	}

	nvgpu_mutex_release(&vm->update_gmmu_lock);

	*addr = vaddr_start;
	return 0;

free_vaddr:
#ifdef CONFIG_NVGPU_REMAP
	if (vm_area->vpool != NULL) {
		nvgpu_vm_remap_vpool_destroy(vm, vm_area);
		vm_area->vpool = NULL;
	}
#endif
	nvgpu_free(vma, vaddr_start);
free_vm_area:
	nvgpu_kfree(g, vm_area);
	return err;
}

int nvgpu_vm_area_free(struct vm_gk20a *vm, u64 addr)
{
	struct gk20a *g = gk20a_from_vm(vm);
	struct nvgpu_mapped_buf *buffer;
	struct nvgpu_vm_area *vm_area;

	nvgpu_mutex_acquire(&vm->update_gmmu_lock);
	vm_area = nvgpu_vm_area_find(vm, addr);
	if (vm_area == NULL) {
		nvgpu_mutex_release(&vm->update_gmmu_lock);
		return 0;
	}
	nvgpu_list_del(&vm_area->vm_area_list);

	nvgpu_log(g, gpu_dbg_map,
		  "DEL vm_area: pgsz=%#-8x pages=%-9llu "
		  "addr=%#-14llx flags=0x%x",
		  vm->gmmu_page_sizes[vm_area->pgsz_idx],
		  vm_area->size / vm->gmmu_page_sizes[vm_area->pgsz_idx],
		  vm_area->addr,
		  vm_area->flags);

	/* Decrement the ref count on all buffers in this vm_area. This
	 * allows userspace to let the kernel free mappings that are
	 * only used by this vm_area. */
	while (!nvgpu_list_empty(&vm_area->buffer_list_head)) {
		buffer = nvgpu_list_first_entry(&vm_area->buffer_list_head,
					nvgpu_mapped_buf, buffer_list);
		nvgpu_list_del(&buffer->buffer_list);
		nvgpu_ref_put(&buffer->ref, nvgpu_vm_unmap_ref_internal);
	}

	/* if this was a sparse mapping, free the va */
	if (vm_area->sparse) {
		g->ops.mm.gmmu.unmap(vm,
				     vm_area->addr,
				     vm_area->size,
				     vm_area->pgsz_idx,
				     false,
				     gk20a_mem_flag_none,
				     true,
				     NULL);
	}

#ifdef CONFIG_NVGPU_REMAP
	/* clean up any remap resources */
	if (vm_area->vpool != NULL) {
		nvgpu_vm_remap_vpool_destroy(vm, vm_area);
	}
#endif

	nvgpu_mutex_release(&vm->update_gmmu_lock);

	nvgpu_free(vm->vma[vm_area->pgsz_idx], vm_area->addr);
	nvgpu_kfree(g, vm_area);

	return 0;
}
