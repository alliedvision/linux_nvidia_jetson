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

#include <nvgpu/bug.h>
#include <nvgpu/log.h>
#include <nvgpu/log2.h>
#include <nvgpu/dma.h>
#include <nvgpu/vm.h>
#include <nvgpu/vm_area.h>
#include <nvgpu/gmmu.h>
#include <nvgpu/lock.h>
#include <nvgpu/list.h>
#include <nvgpu/rbtree.h>
#include <nvgpu/semaphore.h>
#include <nvgpu/enabled.h>
#include <nvgpu/sizes.h>
#include <nvgpu/timers.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/nvgpu_sgt.h>
#include <nvgpu/vgpu/vm_vgpu.h>
#include <nvgpu/cbc.h>
#include <nvgpu/static_analysis.h>
#include <nvgpu/power_features/pg.h>
#include <nvgpu/nvhost.h>
#include <nvgpu/string.h>

struct nvgpu_ctag_buffer_info {
	u64			size;
	u32			pgsz_idx;
	u32			flags;

#ifdef CONFIG_NVGPU_COMPRESSION
	u32			ctag_offset;
	s16			compr_kind;
#endif
	s16			incompr_kind;

};

#ifdef CONFIG_NVGPU_COMPRESSION
static int nvgpu_vm_compute_compression(struct vm_gk20a *vm,
					struct nvgpu_ctag_buffer_info *binfo);
#endif

static void nvgpu_vm_do_unmap(struct nvgpu_mapped_buf *mapped_buffer,
			      struct vm_gk20a_mapping_batch *batch);

/*
 * Attempt to find a reserved memory area to determine PTE size for the passed
 * mapping. If no reserved area can be found use small pages.
 */
static u32 nvgpu_vm_get_pte_size_fixed_map(struct vm_gk20a *vm, u64 base)
{
	struct nvgpu_vm_area *vm_area;

	vm_area = nvgpu_vm_area_find(vm, base);
	if (vm_area == NULL) {
		return GMMU_PAGE_SIZE_SMALL;
	}

	return vm_area->pgsz_idx;
}

/*
 * This is for when the address space does not support unified address spaces.
 */
static u32 nvgpu_vm_get_pte_size_split_addr(struct vm_gk20a *vm,
					    u64 base, u64 size)
{
	if (base == 0ULL) {
		if (size >= vm->gmmu_page_sizes[GMMU_PAGE_SIZE_BIG]) {
			return GMMU_PAGE_SIZE_BIG;
		}
		return GMMU_PAGE_SIZE_SMALL;
	} else {
		if (base < nvgpu_gmmu_va_small_page_limit()) {
			return GMMU_PAGE_SIZE_SMALL;
		} else {
			return GMMU_PAGE_SIZE_BIG;
		}
	}
}

/*
 * This determines the PTE size for a given alloc. Used by both the GVA space
 * allocator and the mm core code so that agreement can be reached on how to
 * map allocations.
 *
 * The page size of a buffer is this:
 *
 *   o  If the VM doesn't support large pages then obviously small pages
 *      must be used.
 *   o  If the base address is non-zero (fixed address map):
 *      - Attempt to find a reserved memory area and use the page size
 *        based on that.
 *      - If no reserved page size is available, default to small pages.
 *   o  If the base is zero and we have an SMMU:
 *      - If the size is larger than or equal to the big page size, use big
 *        pages.
 *      - Otherwise use small pages.
 *   o If there's no SMMU:
 *      - Regardless of buffer size use small pages since we have no
 *      - guarantee of contiguity.
 */
static u32 nvgpu_vm_get_pte_size(struct vm_gk20a *vm, u64 base, u64 size)
{
	struct gk20a *g = gk20a_from_vm(vm);

	if (!vm->big_pages) {
		return GMMU_PAGE_SIZE_SMALL;
	}

	if (!vm->unified_va) {
		return nvgpu_vm_get_pte_size_split_addr(vm, base, size);
	}

	if (base != 0ULL) {
		return nvgpu_vm_get_pte_size_fixed_map(vm, base);
	}

	if ((size >= vm->gmmu_page_sizes[GMMU_PAGE_SIZE_BIG]) &&
	    nvgpu_iommuable(g)) {
		return GMMU_PAGE_SIZE_BIG;
	}
	return GMMU_PAGE_SIZE_SMALL;
}

int vm_aspace_id(struct vm_gk20a *vm)
{
	return (vm->as_share != NULL) ? vm->as_share->id : -1;
}

int nvgpu_vm_bind_channel(struct vm_gk20a *vm, struct nvgpu_channel *ch)
{
	if (ch == NULL) {
		return -EINVAL;
	}

	nvgpu_log_fn(ch->g, " ");

	nvgpu_vm_get(vm);
	ch->vm = vm;
	nvgpu_channel_commit_va(ch);

	nvgpu_log(gk20a_from_vm(vm), gpu_dbg_map, "Binding ch=%d -> VM:%s",
		  ch->chid, vm->name);

	return 0;
}

/*
 * Determine how many bits of the address space each last level PDE covers. For
 * example, for gp10b, with a last level address bit PDE range of 28 to 21 the
 * amount of memory each last level PDE addresses is 21 bits - i.e 2MB.
 */
u32 nvgpu_vm_pde_coverage_bit_count(struct gk20a *g, u64 big_page_size)
{
	int final_pde_level = 0;
	const struct gk20a_mmu_level *mmu_levels =
		g->ops.mm.gmmu.get_mmu_levels(g, big_page_size);

	/*
	 * Find the second to last level of the page table programming
	 * heirarchy: the last level is PTEs so we really want the level
	 * before that which is the last level of PDEs.
	 */
	while (mmu_levels[final_pde_level + 2].update_entry != NULL) {
		final_pde_level++;
	}

	return mmu_levels[final_pde_level].lo_bit[0];
}

NVGPU_COV_WHITELIST_BLOCK_BEGIN(deviate, 1, NVGPU_MISRA(Rule, 17_2), "TID-278")
static void nvgpu_vm_do_free_entries(struct vm_gk20a *vm,
				     struct nvgpu_gmmu_pd *pd,
				     u32 level)
{
	struct gk20a *g = gk20a_from_vm(vm);
	u32 i;

	/* This limits recursion */
	nvgpu_assert(level < g->ops.mm.gmmu.get_max_page_table_levels(g));

	if (pd->mem != NULL) {
		nvgpu_pd_free(vm, pd);
		pd->mem = NULL;
	}

	if (pd->entries != NULL) {
		for (i = 0; i < pd->num_entries; i++) {
			nvgpu_assert(level < U32_MAX);
			nvgpu_vm_do_free_entries(vm, &pd->entries[i],
						 level + 1U);
		}
		nvgpu_vfree(vm->mm->g, pd->entries);
		pd->entries = NULL;
	}
}
NVGPU_COV_WHITELIST_BLOCK_END(NVGPU_MISRA(Rule, 17_2))

static void nvgpu_vm_free_entries(struct vm_gk20a *vm,
				  struct nvgpu_gmmu_pd *pdb)
{
	struct gk20a *g = vm->mm->g;
	u32 i;

	nvgpu_pd_free(vm, pdb);

	if (pdb->entries == NULL) {
		return;
	}

	for (i = 0; i < pdb->num_entries; i++) {
		nvgpu_vm_do_free_entries(vm, &pdb->entries[i], 1U);
	}

	nvgpu_vfree(g, pdb->entries);
	pdb->entries = NULL;
}

u64 nvgpu_vm_alloc_va(struct vm_gk20a *vm, u64 size, u32 pgsz_idx)
{
	struct gk20a *g = vm->mm->g;
	struct nvgpu_allocator *vma = NULL;
	u64 addr;
	u32 page_size = vm->gmmu_page_sizes[pgsz_idx];

	vma = vm->vma[pgsz_idx];

	if (pgsz_idx >= GMMU_NR_PAGE_SIZES) {
		nvgpu_err(g, "(%s) invalid page size requested", vma->name);
		return 0;
	}

	if ((pgsz_idx == GMMU_PAGE_SIZE_BIG) && !vm->big_pages) {
		nvgpu_err(g, "(%s) unsupportd page size requested", vma->name);
		return 0;
	}

	/* Be certain we round up to page_size if needed */
	size = NVGPU_ALIGN(size, page_size);

	addr = nvgpu_alloc_pte(vma, size, page_size);
	if (addr == 0ULL) {
		nvgpu_err(g, "(%s) oom: sz=0x%llx", vma->name, size);
		return 0;
	}

	return addr;
}

void nvgpu_vm_free_va(struct vm_gk20a *vm, u64 addr, u32 pgsz_idx)
{
	struct nvgpu_allocator *vma = vm->vma[pgsz_idx];

	nvgpu_free(vma, addr);
}

void nvgpu_vm_mapping_batch_start(struct vm_gk20a_mapping_batch *mapping_batch)
{
	(void) memset(mapping_batch, 0, sizeof(*mapping_batch));
	mapping_batch->gpu_l2_flushed = false;
	mapping_batch->need_tlb_invalidate = false;
}

void nvgpu_vm_mapping_batch_finish_locked(
	struct vm_gk20a *vm, struct vm_gk20a_mapping_batch *mapping_batch)
{
	int err;

	/* hanging kref_put batch pointer? */
	WARN_ON(vm->kref_put_batch == mapping_batch);

	if (mapping_batch->need_tlb_invalidate) {
		struct gk20a *g = gk20a_from_vm(vm);
		err = nvgpu_pg_elpg_ms_protected_call(g, g->ops.fb.tlb_invalidate(g, vm->pdb.mem));
		if (err != 0) {
			nvgpu_err(g, "fb.tlb_invalidate() failed err=%d", err);
		}
	}
}

void nvgpu_vm_mapping_batch_finish(struct vm_gk20a *vm,
				   struct vm_gk20a_mapping_batch *mapping_batch)
{
	nvgpu_mutex_acquire(&vm->update_gmmu_lock);
	nvgpu_vm_mapping_batch_finish_locked(vm, mapping_batch);
	nvgpu_mutex_release(&vm->update_gmmu_lock);
}

/*
 * Determine if the passed address space can support big pages or not.
 */
bool nvgpu_big_pages_possible(struct vm_gk20a *vm, u64 base, u64 size)
{
	u64 pde_size = BIT64(nvgpu_vm_pde_coverage_bit_count(
				gk20a_from_vm(vm), vm->big_page_size));
	u64 mask = nvgpu_safe_sub_u64(pde_size, 1ULL);
	u64 base_big_page = base & mask;
	u64 size_big_page = size & mask;

	if ((base_big_page != 0ULL) || (size_big_page != 0ULL)) {
		return false;
	}
	return true;
}

#ifdef CONFIG_NVGPU_SW_SEMAPHORE
/*
 * Initialize a semaphore pool. Just return successfully if we do not need
 * semaphores (i.e when sync-pts are active).
 */
static int nvgpu_init_sema_pool(struct vm_gk20a *vm)
{
	struct nvgpu_semaphore_sea *sema_sea;
	struct mm_gk20a *mm = vm->mm;
	struct gk20a *g = mm->g;
	int err;

	/*
	 * Don't waste the memory on semaphores if we don't need them.
	 */
	if (nvgpu_has_syncpoints(g)) {
		return 0;
	}

	if (vm->sema_pool != NULL) {
		return 0;
	}

	sema_sea = nvgpu_semaphore_sea_create(g);
	if (sema_sea == NULL) {
		return -ENOMEM;
	}

	err = nvgpu_semaphore_pool_alloc(sema_sea, &vm->sema_pool);
	if (err != 0) {
		return err;
	}

	/*
	 * Allocate a chunk of GPU VA space for mapping the semaphores. We will
	 * do a fixed alloc in the kernel VM so that all channels have the same
	 * RO address range for the semaphores.
	 *
	 * !!! TODO: cleanup.
	 */
	nvgpu_semaphore_sea_allocate_gpu_va(sema_sea, &vm->kernel,
					nvgpu_safe_sub_u64(vm->va_limit,
						mm->channel.kernel_size),
					512U * NVGPU_CPU_PAGE_SIZE,
					nvgpu_safe_cast_u64_to_u32(SZ_4K));
	if (nvgpu_semaphore_sea_get_gpu_va(sema_sea) == 0ULL) {
		nvgpu_free(&vm->kernel,
			nvgpu_semaphore_sea_get_gpu_va(sema_sea));
		nvgpu_vm_put(vm);
		return -ENOMEM;
	}

	err = nvgpu_semaphore_pool_map(vm->sema_pool, vm);
	if (err != 0) {
		nvgpu_semaphore_pool_unmap(vm->sema_pool, vm);
		nvgpu_free(vm->vma[GMMU_PAGE_SIZE_SMALL],
			   nvgpu_semaphore_pool_gpu_va(vm->sema_pool, false));
		return err;
	}

	return 0;
}
#endif

static int nvgpu_vm_init_user_vma(struct gk20a *g, struct vm_gk20a *vm,
			u64 user_vma_start, u64 user_vma_limit,
			const char *name)
{
	int err = 0;
	char alloc_name[NVGPU_ALLOC_NAME_LEN];
	size_t name_len;

	name_len  = strlen("gk20a_") + strlen(name);
	if (name_len >= NVGPU_ALLOC_NAME_LEN) {
		nvgpu_err(g, "Invalid MAX_NAME_SIZE %lu %u", name_len,
			NVGPU_ALLOC_NAME_LEN);
		return -EINVAL;
	}

	/*
	 * User VMA.
	 */
	if (user_vma_start < user_vma_limit) {
		(void) strcpy(alloc_name, "gk20a_");
		(void) strcat(alloc_name, name);
		err = nvgpu_allocator_init(g, &vm->user,
						 vm, alloc_name,
						 user_vma_start,
						 user_vma_limit -
						 user_vma_start,
						 SZ_4K,
						 GPU_BALLOC_MAX_ORDER,
						 GPU_ALLOC_GVA_SPACE,
						 BUDDY_ALLOCATOR);
		if (err != 0) {
			return err;
		}
	} else {
		/*
		 * Make these allocator pointers point to the kernel allocator
		 * since we still use the legacy notion of page size to choose
		 * the allocator.
		 */
		vm->vma[0] = &vm->kernel;
		vm->vma[1] = &vm->kernel;
	}
	return 0;
}

static int nvgpu_vm_init_user_lp_vma(struct gk20a *g, struct vm_gk20a *vm,
			u64 user_lp_vma_start, u64 user_lp_vma_limit,
			const char *name)
{
	int err = 0;
	char alloc_name[NVGPU_VM_NAME_LEN];
	size_t name_len;
	const size_t prefix_len = strlen("gk20a_");

	name_len  = nvgpu_safe_add_u64(nvgpu_safe_add_u64(prefix_len,
						strlen(name)), strlen("_lp"));
	if (name_len >= NVGPU_VM_NAME_LEN) {
		nvgpu_err(g, "Invalid MAX_NAME_SIZE %lu %u", name_len,
				  NVGPU_VM_NAME_LEN);
		return -EINVAL;
	}

	/*
	 * User VMA for large pages when a split address range is used.
	 */
	if (user_lp_vma_start < user_lp_vma_limit) {
		(void) strcpy(alloc_name, "gk20a_");
		(void) strncat(alloc_name, name, nvgpu_safe_sub_u64(
					NVGPU_VM_NAME_LEN, prefix_len));
		(void) strcat(alloc_name, "_lp");
		err = nvgpu_allocator_init(g, &vm->user_lp,
						 vm, alloc_name,
						 user_lp_vma_start,
						 user_lp_vma_limit -
						 user_lp_vma_start,
						 vm->big_page_size,
						 GPU_BALLOC_MAX_ORDER,
						 GPU_ALLOC_GVA_SPACE,
						 BUDDY_ALLOCATOR);
		if (err != 0) {
			return err;
		}
	}
	return 0;
}

static int nvgpu_vm_init_kernel_vma(struct gk20a *g, struct vm_gk20a *vm,
			u64 kernel_vma_start, u64 kernel_vma_limit,
			u64 kernel_vma_flags, const char *name)
{
	int err = 0;
	char alloc_name[NVGPU_VM_NAME_LEN];
	size_t name_len;
	const size_t prefix_len = strlen("gk20a_");

	name_len  = nvgpu_safe_add_u64(nvgpu_safe_add_u64(prefix_len,
						strlen(name)),strlen("-sys"));
	if (name_len >= NVGPU_VM_NAME_LEN) {
		nvgpu_err(g, "Invalid MAX_NAME_SIZE %lu %u", name_len,
				  NVGPU_VM_NAME_LEN);
		return -EINVAL;
	}

	/*
	 * Kernel VMA.
	 */
	if (kernel_vma_start < kernel_vma_limit) {
		(void) strcpy(alloc_name, "gk20a_");
		(void) strncat(alloc_name, name, nvgpu_safe_sub_u64(
						NVGPU_VM_NAME_LEN, prefix_len));
		(void) strcat(alloc_name, "-sys");
		err = nvgpu_allocator_init(g, &vm->kernel,
						 vm, alloc_name,
						 kernel_vma_start,
						 kernel_vma_limit -
						 kernel_vma_start,
						 SZ_4K,
						 GPU_BALLOC_MAX_ORDER,
						 kernel_vma_flags,
						 BUDDY_ALLOCATOR);
		if (err != 0) {
			return err;
		}
	}
	return 0;
}

static int nvgpu_vm_init_vma_allocators(struct gk20a *g, struct vm_gk20a *vm,
			u64 user_vma_start, u64 user_vma_limit,
			u64 user_lp_vma_start, u64 user_lp_vma_limit,
			u64 kernel_vma_start, u64 kernel_vma_limit,
			u64 kernel_vma_flags, const char *name)
{
	int err = 0;

	err = nvgpu_vm_init_user_vma(g, vm,
			user_vma_start, user_vma_limit, name);
	if (err != 0) {
		return err;
	}

	err = nvgpu_vm_init_user_lp_vma(g, vm,
			user_lp_vma_start, user_lp_vma_limit, name);
	if (err != 0) {
		goto clean_up_allocators;
	}

	err = nvgpu_vm_init_kernel_vma(g, vm, kernel_vma_start,
			kernel_vma_limit, kernel_vma_flags, name);
	if (err != 0) {
		goto clean_up_allocators;
	}

	return 0;

clean_up_allocators:
	if (nvgpu_alloc_initialized(&vm->kernel)) {
		nvgpu_alloc_destroy(&vm->kernel);
	}
	if (nvgpu_alloc_initialized(&vm->user)) {
		nvgpu_alloc_destroy(&vm->user);
	}
	if (nvgpu_alloc_initialized(&vm->user_lp)) {
		nvgpu_alloc_destroy(&vm->user_lp);
	}
	return err;
}

static void nvgpu_vm_init_check_big_pages(struct vm_gk20a *vm,
				u64 user_vma_start, u64 user_vma_limit,
				u64 user_lp_vma_start, u64 user_lp_vma_limit,
				bool big_pages, bool unified_va)
{
	/*
	 * Determine if big pages are possible in this VM. If a split address
	 * space is used then check the user_lp vma instead of the user vma.
	 */
	if (!big_pages) {
		vm->big_pages = false;
	} else {
		if (unified_va) {
			vm->big_pages = nvgpu_big_pages_possible(vm,
					user_vma_start,
					nvgpu_safe_sub_u64(user_vma_limit,
							user_vma_start));
		} else {
			vm->big_pages = nvgpu_big_pages_possible(vm,
					user_lp_vma_start,
					nvgpu_safe_sub_u64(user_lp_vma_limit,
							user_lp_vma_start));
		}
	}
}

static int nvgpu_vm_init_check_vma_limits(struct gk20a *g, struct vm_gk20a *vm,
				u64 user_vma_start, u64 user_vma_limit,
				u64 user_lp_vma_start, u64 user_lp_vma_limit,
				u64 kernel_vma_start, u64 kernel_vma_limit)
{
	(void)vm;
	if ((user_vma_start > user_vma_limit) ||
		(user_lp_vma_start > user_lp_vma_limit) ||
		(kernel_vma_start >= kernel_vma_limit)) {
		nvgpu_err(g, "Invalid vm configuration");
		nvgpu_do_assert();
		return -EINVAL;
	}

	/*
	 * A "user" area only makes sense for the GVA spaces. For VMs where
	 * there is no "user" area user_vma_start will be equal to
	 * user_vma_limit (i.e a 0 sized space). In such a situation the kernel
	 * area must be non-zero in length.
	 */
	if ((user_vma_start >= user_vma_limit) &&
		(kernel_vma_start >= kernel_vma_limit)) {
		return -EINVAL;
	}

	return 0;
}

static int nvgpu_vm_init_vma(struct gk20a *g, struct vm_gk20a *vm,
		     u64 user_reserved,
		     u64 kernel_reserved,
		     u64 small_big_split,
		     bool big_pages,
		     bool unified_va,
		     const char *name)
{
	int err = 0;
	u64 kernel_vma_flags = 0ULL;
	u64 user_vma_start, user_vma_limit;
	u64 user_lp_vma_start, user_lp_vma_limit;
	u64 kernel_vma_start, kernel_vma_limit;

	/* Setup vma limits. */
	if (user_reserved > 0ULL) {
		kernel_vma_flags = GPU_ALLOC_GVA_SPACE;
		/*
		 * If big_pages are disabled for this VM then it only makes
		 * sense to make one VM, same as if the unified address flag
		 * is set.
		 */
		if (!big_pages || unified_va) {
			user_vma_start = vm->virtaddr_start;
			user_vma_limit = nvgpu_safe_sub_u64(vm->va_limit,
							kernel_reserved);
			user_lp_vma_start = user_vma_limit;
			user_lp_vma_limit = user_vma_limit;
		} else {
			/*
			 * Ensure small_big_split falls between user vma
			 * start and end.
			 */
			if ((small_big_split <= vm->virtaddr_start) ||
				(small_big_split >=
					nvgpu_safe_sub_u64(vm->va_limit,
							kernel_reserved))) {
				return -EINVAL;
			}

			user_vma_start = vm->virtaddr_start;
			user_vma_limit = small_big_split;
			user_lp_vma_start = small_big_split;
			user_lp_vma_limit = nvgpu_safe_sub_u64(vm->va_limit,
							kernel_reserved);
		}
	} else {
		user_vma_start = 0;
		user_vma_limit = 0;
		user_lp_vma_start = 0;
		user_lp_vma_limit = 0;
	}
	kernel_vma_start = nvgpu_safe_sub_u64(vm->va_limit, kernel_reserved);
	kernel_vma_limit = vm->va_limit;

	nvgpu_log_info(g, "user_vma     [0x%llx,0x%llx)",
		       user_vma_start, user_vma_limit);
	if (!unified_va) {
		nvgpu_log_info(g, "user_lp_vma  [0x%llx,0x%llx)",
			       user_lp_vma_start, user_lp_vma_limit);
	}
	nvgpu_log_info(g, "kernel_vma   [0x%llx,0x%llx)",
		       kernel_vma_start, kernel_vma_limit);

	err = nvgpu_vm_init_check_vma_limits(g, vm,
					user_vma_start, user_vma_limit,
					user_lp_vma_start, user_lp_vma_limit,
					kernel_vma_start, kernel_vma_limit);
	if (err != 0) {
		goto clean_up_page_tables;
	}

	nvgpu_vm_init_check_big_pages(vm, user_vma_start, user_vma_limit,
				user_lp_vma_start, user_lp_vma_limit,
				big_pages, unified_va);

	err = nvgpu_vm_init_vma_allocators(g, vm,
			user_vma_start, user_vma_limit,
			user_lp_vma_start, user_lp_vma_limit,
			kernel_vma_start, kernel_vma_limit,
			kernel_vma_flags, name);
	if (err != 0) {
		goto clean_up_page_tables;
	}

	return 0;

clean_up_page_tables:
	/* Cleans up nvgpu_gmmu_init_page_table() */
	nvgpu_pd_free(vm, &vm->pdb);
	return err;
}

static int nvgpu_vm_init_attributes(struct mm_gk20a *mm,
		     struct vm_gk20a *vm,
		     u32 big_page_size,
		     u64 low_hole,
		     u64 user_reserved,
		     u64 kernel_reserved,
		     bool big_pages,
		     bool userspace_managed,
		     bool unified_va,
		     const char *name)
{
	struct gk20a *g = gk20a_from_mm(mm);
	u64 aperture_size;
	u64 default_aperture_size;

	(void)big_pages;

	g->ops.mm.get_default_va_sizes(&default_aperture_size, NULL, NULL);

	aperture_size = nvgpu_safe_add_u64(kernel_reserved,
		nvgpu_safe_add_u64(user_reserved, low_hole));

	if (aperture_size > default_aperture_size) {
		nvgpu_do_assert_print(g,
			"Overlap between user and kernel spaces");
		return -ENOMEM;
	}

	nvgpu_log_info(g, "Init space for %s: valimit=0x%llx, "
		       "LP size=0x%x lowhole=0x%llx",
		       name, aperture_size,
		       (unsigned int)big_page_size, low_hole);

	vm->mm = mm;

	vm->gmmu_page_sizes[GMMU_PAGE_SIZE_SMALL]  =
					nvgpu_safe_cast_u64_to_u32(SZ_4K);
	vm->gmmu_page_sizes[GMMU_PAGE_SIZE_BIG]    = big_page_size;
	vm->gmmu_page_sizes[GMMU_PAGE_SIZE_KERNEL] =
			nvgpu_safe_cast_u64_to_u32(NVGPU_CPU_PAGE_SIZE);

	/* Set up vma pointers. */
	vm->vma[GMMU_PAGE_SIZE_SMALL]  = &vm->user;
	vm->vma[GMMU_PAGE_SIZE_BIG]    = &vm->user;
	vm->vma[GMMU_PAGE_SIZE_KERNEL] = &vm->kernel;
	if (!unified_va) {
		vm->vma[GMMU_PAGE_SIZE_BIG] = &vm->user_lp;
	}

	vm->virtaddr_start = low_hole;
	vm->va_limit = aperture_size;

	vm->big_page_size     = vm->gmmu_page_sizes[GMMU_PAGE_SIZE_BIG];
	vm->userspace_managed = userspace_managed;
	vm->unified_va        = unified_va;
	vm->mmu_levels        =
		g->ops.mm.gmmu.get_mmu_levels(g, vm->big_page_size);

#ifdef CONFIG_NVGPU_GR_VIRTUALIZATION
	if (g->is_virtual && userspace_managed) {
		nvgpu_err(g, "vGPU: no userspace managed addr space support");
		return -ENOSYS;
	}
#endif
	return 0;
}

/*
 * Initialize a preallocated vm.
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
		     const char *name)
{
	struct gk20a *g = gk20a_from_mm(mm);
	int err = 0;

	err = nvgpu_vm_init_attributes(mm, vm, big_page_size, low_hole,
		user_reserved, kernel_reserved, big_pages, userspace_managed,
		unified_va, name);
	if (err != 0) {
		return err;
	}

	if (g->ops.mm.vm_as_alloc_share != NULL) {
		err = g->ops.mm.vm_as_alloc_share(g, vm);
		if (err != 0) {
			nvgpu_err(g, "Failed to init gpu vm!");
			return err;
		}
	}

	/* Initialize the page table data structures. */
	(void) strncpy(vm->name, name,
		       min(strlen(name), (size_t)(sizeof(vm->name)-1ULL)));
	err = nvgpu_gmmu_init_page_table(vm);
	if (err != 0) {
		goto clean_up_gpu_vm;
	}

	err = nvgpu_vm_init_vma(g, vm, user_reserved, kernel_reserved,
				small_big_split, big_pages, unified_va, name);
	if (err != 0) {
		goto clean_up_gpu_vm;
	}

	vm->mapped_buffers = NULL;

	nvgpu_mutex_init(&vm->syncpt_ro_map_lock);
	nvgpu_mutex_init(&vm->update_gmmu_lock);

	nvgpu_ref_init(&vm->ref);
	nvgpu_init_list_node(&vm->vm_area_list);

#ifdef CONFIG_NVGPU_SW_SEMAPHORE
	/*
	 * This is only necessary for channel address spaces. The best way to
	 * distinguish channel address spaces from other address spaces is by
	 * size - if the address space is 4GB or less, it's not a channel.
	 */
	if (vm->va_limit > 4ULL * SZ_1G) {
		err = nvgpu_init_sema_pool(vm);
		if (err != 0) {
			goto clean_up_gmmu_lock;
		}
	}
#endif

	return 0;

#ifdef CONFIG_NVGPU_SW_SEMAPHORE
clean_up_gmmu_lock:
	nvgpu_mutex_destroy(&vm->update_gmmu_lock);
	nvgpu_mutex_destroy(&vm->syncpt_ro_map_lock);
#endif
clean_up_gpu_vm:
	if (g->ops.mm.vm_as_free_share != NULL) {
		g->ops.mm.vm_as_free_share(vm);
	}
	return err;
}

/**
 * nvgpu_init_vm() - Initialize an address space.
 *
 * @mm - Parent MM.
 * @vm - The VM to init.
 * @big_page_size - Size of big pages associated with this VM.
 * @low_hole - The size of the low hole (unaddressable memory at the bottom of
 *	       the address space).
 * @user_reserved - Space reserved for user allocations..
 * @kernel_reserved - Space reserved for kernel only allocations.
 * @big_pages - If true then big pages are possible in the VM. Note this does
 *              not guarantee that big pages will be possible.
 * @name - Name of the address space.
 *
 * This function initializes an address space according to the following map:
 *
 *     +--+ 0x0
 *     |  |
 *     +--+ @low_hole
 *     |  |
 *     ~  ~   This is the "user" section.
 *     |  |
 *     +--+ @aperture_size - @kernel_reserved
 *     |  |
 *     ~  ~   This is the "kernel" section.
 *     |  |
 *     +--+ @aperture_size
 *
 * The user section is therefor what ever is left over after the @low_hole and
 * @kernel_reserved memory have been portioned out. The @kernel_reserved is
 * always persent at the top of the memory space and the @low_hole is always at
 * the bottom.
 *
 * For certain address spaces a "user" section makes no sense (bar1, etc) so in
 * such cases the @kernel_reserved and @low_hole should sum to exactly
 * @aperture_size.
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
			       const char *name)
{
	struct vm_gk20a *vm = nvgpu_kzalloc(g, sizeof(*vm));
	int err;

	if (vm == NULL) {
		return NULL;
	}

	err = nvgpu_vm_do_init(&g->mm, vm, big_page_size, low_hole,
			     user_reserved, kernel_reserved, small_big_split,
			     big_pages, userspace_managed, unified_va, name);
	if (err != 0) {
		nvgpu_kfree(g, vm);
		return NULL;
	}

	return vm;
}

/*
 * Cleanup the VM!
 */
static void nvgpu_vm_remove(struct vm_gk20a *vm)
{
	struct nvgpu_mapped_buf *mapped_buffer;
	struct nvgpu_vm_area *vm_area;
	struct nvgpu_rbtree_node *node = NULL;
	struct gk20a *g = vm->mm->g;
	bool done;

#ifdef CONFIG_NVGPU_SW_SEMAPHORE
	/*
	 * Do this outside of the update_gmmu_lock since unmapping the semaphore
	 * pool involves unmapping a GMMU mapping which means aquiring the
	 * update_gmmu_lock.
	 */
	if (!nvgpu_has_syncpoints(g)) {
		if (vm->sema_pool != NULL) {
			nvgpu_semaphore_pool_unmap(vm->sema_pool, vm);
			nvgpu_semaphore_pool_put(vm->sema_pool);
		}
	}
#endif

	if (nvgpu_mem_is_valid(&g->syncpt_mem) &&
		(vm->syncpt_ro_map_gpu_va != 0ULL)) {
		nvgpu_gmmu_unmap_addr(vm, &g->syncpt_mem,
				vm->syncpt_ro_map_gpu_va);
	}

	nvgpu_mutex_acquire(&vm->update_gmmu_lock);

	nvgpu_rbtree_enum_start(0, &node, vm->mapped_buffers);
	while (node != NULL) {
		mapped_buffer = mapped_buffer_from_rbtree_node(node);
		nvgpu_vm_do_unmap(mapped_buffer, NULL);
		nvgpu_rbtree_enum_start(0, &node, vm->mapped_buffers);
	}

	/* destroy remaining reserved memory areas */
	done = false;
	do {
		if (nvgpu_list_empty(&vm->vm_area_list)) {
			done = true;
		} else {
			vm_area = nvgpu_list_first_entry(&vm->vm_area_list,
							 nvgpu_vm_area,
							 vm_area_list);
			nvgpu_list_del(&vm_area->vm_area_list);
			nvgpu_kfree(vm->mm->g, vm_area);
		}
	} while (!done);

	if (nvgpu_alloc_initialized(&vm->kernel)) {
		nvgpu_alloc_destroy(&vm->kernel);
	}
	if (nvgpu_alloc_initialized(&vm->user)) {
		nvgpu_alloc_destroy(&vm->user);
	}
	if (nvgpu_alloc_initialized(&vm->user_lp)) {
		nvgpu_alloc_destroy(&vm->user_lp);
	}

	nvgpu_vm_free_entries(vm, &vm->pdb);

	if (g->ops.mm.vm_as_free_share != NULL) {
		g->ops.mm.vm_as_free_share(vm);
	}

	nvgpu_mutex_release(&vm->update_gmmu_lock);
	nvgpu_mutex_destroy(&vm->update_gmmu_lock);

	nvgpu_mutex_destroy(&vm->syncpt_ro_map_lock);
	nvgpu_kfree(g, vm);
}

static struct vm_gk20a *vm_gk20a_from_ref(struct nvgpu_ref *ref)
{
	return (struct vm_gk20a *)
		((uintptr_t)ref - offsetof(struct vm_gk20a, ref));
}

static void nvgpu_vm_remove_ref(struct nvgpu_ref *ref)
{
	struct vm_gk20a *vm = vm_gk20a_from_ref(ref);

	nvgpu_vm_remove(vm);
}

void nvgpu_vm_get(struct vm_gk20a *vm)
{
	nvgpu_ref_get(&vm->ref);
}

void nvgpu_vm_put(struct vm_gk20a *vm)
{
	nvgpu_ref_put(&vm->ref, nvgpu_vm_remove_ref);
}

void nvgpu_insert_mapped_buf(struct vm_gk20a *vm,
			    struct nvgpu_mapped_buf *mapped_buffer)
{
	mapped_buffer->node.key_start = mapped_buffer->addr;
	mapped_buffer->node.key_end = nvgpu_safe_add_u64(mapped_buffer->addr,
						mapped_buffer->size);

	nvgpu_rbtree_insert(&mapped_buffer->node, &vm->mapped_buffers);
	nvgpu_assert(vm->num_user_mapped_buffers < U32_MAX);
	vm->num_user_mapped_buffers++;
}

static void nvgpu_remove_mapped_buf(struct vm_gk20a *vm,
				    struct nvgpu_mapped_buf *mapped_buffer)
{
	nvgpu_rbtree_unlink(&mapped_buffer->node, &vm->mapped_buffers);
	nvgpu_assert(vm->num_user_mapped_buffers > 0U);
	vm->num_user_mapped_buffers--;
}

struct nvgpu_mapped_buf *nvgpu_vm_find_mapped_buf(
	struct vm_gk20a *vm, u64 addr)
{
	struct nvgpu_rbtree_node *node = NULL;
	struct nvgpu_rbtree_node *root = vm->mapped_buffers;

	nvgpu_rbtree_search(addr, &node, root);
	if (node == NULL) {
		return NULL;
	}

	return mapped_buffer_from_rbtree_node(node);
}

struct nvgpu_mapped_buf *nvgpu_vm_find_mapped_buf_range(
	struct vm_gk20a *vm, u64 addr)
{
	struct nvgpu_rbtree_node *node = NULL;
	struct nvgpu_rbtree_node *root = vm->mapped_buffers;

	nvgpu_rbtree_range_search(addr, &node, root);
	if (node == NULL) {
		return NULL;
	}

	return mapped_buffer_from_rbtree_node(node);
}

struct nvgpu_mapped_buf *nvgpu_vm_find_mapped_buf_less_than(
	struct vm_gk20a *vm, u64 addr)
{
	struct nvgpu_rbtree_node *node = NULL;
	struct nvgpu_rbtree_node *root = vm->mapped_buffers;

	nvgpu_rbtree_less_than_search(addr, &node, root);
	if (node == NULL) {
		return NULL;
	}

	return mapped_buffer_from_rbtree_node(node);
}

int nvgpu_vm_get_buffers(struct vm_gk20a *vm,
			 struct nvgpu_mapped_buf ***mapped_buffers,
			 u32 *num_buffers)
{
	struct nvgpu_mapped_buf *mapped_buffer;
	struct nvgpu_mapped_buf **buffer_list;
	struct nvgpu_rbtree_node *node = NULL;
	u32 i = 0;

	if (vm->userspace_managed) {
		*mapped_buffers = NULL;
		*num_buffers = 0;
		return 0;
	}

	nvgpu_mutex_acquire(&vm->update_gmmu_lock);

	if (vm->num_user_mapped_buffers == 0U) {
		nvgpu_mutex_release(&vm->update_gmmu_lock);
		return 0;
	}

	buffer_list = nvgpu_big_zalloc(vm->mm->g,
				nvgpu_safe_mult_u64(sizeof(*buffer_list),
						vm->num_user_mapped_buffers));
	if (buffer_list == NULL) {
		nvgpu_mutex_release(&vm->update_gmmu_lock);
		return -ENOMEM;
	}

	nvgpu_rbtree_enum_start(0, &node, vm->mapped_buffers);
	while (node != NULL) {
		mapped_buffer = mapped_buffer_from_rbtree_node(node);
		buffer_list[i] = mapped_buffer;
		nvgpu_ref_get(&mapped_buffer->ref);
		nvgpu_assert(i < U32_MAX);
		i++;
		nvgpu_rbtree_enum_next(&node, node);
	}

	if (i != vm->num_user_mapped_buffers) {
		BUG();
	}

	*num_buffers = vm->num_user_mapped_buffers;
	*mapped_buffers = buffer_list;

	nvgpu_mutex_release(&vm->update_gmmu_lock);

	return 0;
}

void nvgpu_vm_put_buffers(struct vm_gk20a *vm,
				 struct nvgpu_mapped_buf **mapped_buffers,
				 u32 num_buffers)
{
	u32 i;
	struct vm_gk20a_mapping_batch batch;

	if (num_buffers == 0U) {
		return;
	}

	nvgpu_mutex_acquire(&vm->update_gmmu_lock);
	nvgpu_vm_mapping_batch_start(&batch);
	vm->kref_put_batch = &batch;

	for (i = 0U; i < num_buffers; ++i) {
		nvgpu_ref_put(&mapped_buffers[i]->ref,
			      nvgpu_vm_unmap_ref_internal);
	}

	vm->kref_put_batch = NULL;
	nvgpu_vm_mapping_batch_finish_locked(vm, &batch);
	nvgpu_mutex_release(&vm->update_gmmu_lock);

	nvgpu_big_free(vm->mm->g, mapped_buffers);
}

static int nvgpu_vm_do_map(struct vm_gk20a *vm,
		 struct nvgpu_os_buffer *os_buf,
		 struct nvgpu_sgt *sgt,
		 u64 *map_addr_ptr,
		 u64 map_size,
		 u64 phys_offset,
		 enum gk20a_mem_rw_flag rw,
		 u32 flags,
		 struct vm_gk20a_mapping_batch *batch,
		 enum nvgpu_aperture aperture,
		 struct nvgpu_ctag_buffer_info *binfo_ptr)
{
	struct gk20a *g = gk20a_from_vm(vm);
	int err = 0;
	bool clear_ctags = false;
	u32 ctag_offset = 0;
	u64 map_addr = *map_addr_ptr;
	/*
	 * The actual GMMU PTE kind
	 */
	u8 pte_kind;

	(void)os_buf;
	(void)flags;
#ifdef CONFIG_NVGPU_COMPRESSION
	err = nvgpu_vm_compute_compression(vm, binfo_ptr);
	if (err != 0) {
		nvgpu_err(g, "failure setting up compression");
		goto ret_err;
	}

	if ((binfo_ptr->compr_kind != NVGPU_KIND_INVALID) &&
	    ((flags & NVGPU_VM_MAP_FIXED_OFFSET) != 0U)) {
		/*
		 * Fixed-address compressible mapping is
		 * requested. Make sure we're respecting the alignment
		 * requirement for virtual addresses and buffer
		 * offsets.
		 *
		 * This check must be done before we may fall back to
		 * the incompressible kind.
		 */

		const u64 offset_mask = g->ops.fb.compression_align_mask(g);

		if ((map_addr & offset_mask) != (phys_offset & offset_mask)) {
			nvgpu_log(g, gpu_dbg_map,
				  "Misaligned compressible-kind fixed-address "
				  "mapping");
			err = -EINVAL;
			goto ret_err;
		}
	}

	if (binfo_ptr->compr_kind != NVGPU_KIND_INVALID) {
		struct gk20a_comptags comptags = { };

		/*
		 * Get the comptags state
		 */
		gk20a_get_comptags(os_buf, &comptags);

		if (!comptags.allocated) {
			nvgpu_log_info(g, "compr kind %d map requested without comptags allocated, allocating...",
				       binfo_ptr->compr_kind);

			/*
			 * best effort only, we don't really care if
			 * this fails
			 */
			gk20a_alloc_or_get_comptags(
				g, os_buf, &g->cbc->comp_tags, &comptags);
		}

		/*
		 * Newly allocated comptags needs to be cleared
		 */
		if (comptags.needs_clear) {
			if (g->ops.cbc.ctrl != NULL) {
				if (gk20a_comptags_start_clear(os_buf)) {
					err = g->ops.cbc.ctrl(
						g, nvgpu_cbc_op_clear,
						comptags.offset,
						(comptags.offset +
						 comptags.lines - 1U));
					gk20a_comptags_finish_clear(
						os_buf, err == 0);
					if (err != 0) {
						goto ret_err;
					}
				}
			} else {
				/*
				 * Cleared as part of gmmu map
				 */
				clear_ctags = true;
			}
		}

		/*
		 * Store the ctag offset for later use if we have the comptags
		 */
		if (comptags.enabled) {
			ctag_offset = comptags.offset;
		}
	}

	/*
	 * Figure out the kind and ctag offset for the GMMU page tables
	 */
	if (binfo_ptr->compr_kind != NVGPU_KIND_INVALID && ctag_offset != 0U) {

		u64 compression_page_size = g->ops.fb.compression_page_size(g);

		nvgpu_assert(compression_page_size > 0ULL);

		/*
		 * Adjust the ctag_offset as per the buffer map offset
		 */
		ctag_offset += (u32)(phys_offset >>
			nvgpu_ilog2(compression_page_size));
		nvgpu_assert((binfo_ptr->compr_kind >= 0) &&
			     (binfo_ptr->compr_kind <= (s16)U8_MAX));
		pte_kind = (u8)binfo_ptr->compr_kind;
		binfo_ptr->ctag_offset = ctag_offset;
	} else
#endif
	if ((binfo_ptr->incompr_kind >= 0) &&
			(binfo_ptr->incompr_kind <= (s16)U8_MAX)) {
		/*
		 * Incompressible kind, ctag offset will not be programmed
		 */
		ctag_offset = 0;
		pte_kind = (u8)binfo_ptr->incompr_kind;
	} else {
		/*
		 * Caller required compression, but we cannot provide it
		 */
		nvgpu_err(g, "No comptags and no incompressible fallback kind");
		err = -ENOMEM;
		goto ret_err;
	}

#ifdef CONFIG_NVGPU_COMPRESSION
	if (clear_ctags) {
		clear_ctags = gk20a_comptags_start_clear(os_buf);
	}
#endif

	map_addr = g->ops.mm.gmmu.map(vm,
				      map_addr,
				      sgt,
				      phys_offset,
				      map_size,
				      binfo_ptr->pgsz_idx,
				      pte_kind,
				      ctag_offset,
				      binfo_ptr->flags,
				      rw,
				      clear_ctags,
				      false,
				      false,
				      batch,
				      aperture);

#ifdef CONFIG_NVGPU_COMPRESSION
	if (clear_ctags) {
		gk20a_comptags_finish_clear(os_buf, map_addr != 0U);
	}
#endif

	if (map_addr == 0ULL) {
		err = -ENOMEM;
		goto ret_err;
	}

	*map_addr_ptr = map_addr;

ret_err:
	return err;
}

static int nvgpu_vm_new_mapping(struct vm_gk20a *vm,
			struct nvgpu_os_buffer *os_buf,
			struct nvgpu_mapped_buf *mapped_buffer,
			struct nvgpu_sgt *sgt,
			struct nvgpu_ctag_buffer_info *binfo_ptr,
			u64 map_addr, u64 *map_size_ptr,
			u64 phys_offset, s16 map_key_kind,
			struct nvgpu_mapped_buf **mapped_buffer_arg)
{
	struct gk20a *g = gk20a_from_vm(vm);
	u64 align;
	u64 map_size = *map_size_ptr;

	/*
	 * Check if this buffer is already mapped.
	 */
	if (!vm->userspace_managed) {
		nvgpu_mutex_acquire(&vm->update_gmmu_lock);
		mapped_buffer = nvgpu_vm_find_mapping(vm,
						      os_buf,
						      map_addr,
						      binfo_ptr->flags,
						      map_key_kind);

		if (mapped_buffer != NULL) {
			nvgpu_ref_get(&mapped_buffer->ref);
			nvgpu_mutex_release(&vm->update_gmmu_lock);
			*mapped_buffer_arg = mapped_buffer;
			return 1;
		}
		nvgpu_mutex_release(&vm->update_gmmu_lock);
	}

	/*
	 * Generate a new mapping!
	 */
	mapped_buffer = nvgpu_kzalloc(g, sizeof(*mapped_buffer));
	if (mapped_buffer == NULL) {
		nvgpu_warn(g, "oom allocating tracking buffer");
		return -ENOMEM;
	}
	*mapped_buffer_arg = mapped_buffer;

	align = nvgpu_sgt_alignment(g, sgt);
	if (g->mm.disable_bigpage) {
		binfo_ptr->pgsz_idx = GMMU_PAGE_SIZE_SMALL;
	} else {
		binfo_ptr->pgsz_idx = nvgpu_vm_get_pte_size(vm, map_addr,
					min_t(u64, binfo_ptr->size, align));
	}
	map_size = (map_size != 0ULL) ? map_size : binfo_ptr->size;
	map_size = NVGPU_ALIGN(map_size, SZ_4K);

	if ((map_size > binfo_ptr->size) ||
	    (phys_offset > (binfo_ptr->size - map_size))) {
		return -EINVAL;
	}

	*map_size_ptr = map_size;
	return 0;
}

static int nvgpu_vm_map_check_attributes(struct vm_gk20a *vm,
			struct nvgpu_os_buffer *os_buf,
			struct nvgpu_ctag_buffer_info *binfo_ptr,
			u32 flags,
			s16 compr_kind,
			s16 incompr_kind,
			s16 *map_key_kind_ptr)
{
	struct gk20a *g = gk20a_from_vm(vm);

	(void)compr_kind;

	if (vm->userspace_managed &&
		((flags & NVGPU_VM_MAP_FIXED_OFFSET) == 0U)) {
		nvgpu_err(g,
			  "non-fixed-offset mapping not available on "
			  "userspace managed address spaces");
		return -EINVAL;
	}

	binfo_ptr->flags = flags;
	binfo_ptr->size = nvgpu_os_buf_get_size(os_buf);
	if (binfo_ptr->size == 0UL) {
		nvgpu_err(g, "Invalid buffer size");
		return -EINVAL;
	}
	binfo_ptr->incompr_kind = incompr_kind;

#ifdef CONFIG_NVGPU_COMPRESSION
	if (vm->enable_ctag && compr_kind != NVGPU_KIND_INVALID) {
		binfo_ptr->compr_kind = compr_kind;
	} else {
		binfo_ptr->compr_kind = NVGPU_KIND_INVALID;
	}

	if (compr_kind != NVGPU_KIND_INVALID) {
		*map_key_kind_ptr = compr_kind;
	} else {
		*map_key_kind_ptr = incompr_kind;
	}
#else
	*map_key_kind_ptr = incompr_kind;
#endif
	return 0;
}

int nvgpu_vm_map(struct vm_gk20a *vm,
		 struct nvgpu_os_buffer *os_buf,
		 struct nvgpu_sgt *sgt,
		 u64 map_addr,
		 u64 map_size,
		 u64 phys_offset,
		 enum gk20a_mem_rw_flag buffer_rw_mode,
		 u32 map_access_requested,
		 u32 flags,
		 s16 compr_kind,
		 s16 incompr_kind,
		 struct vm_gk20a_mapping_batch *batch,
		 enum nvgpu_aperture aperture,
		 struct nvgpu_mapped_buf **mapped_buffer_arg)
{
	struct gk20a *g = gk20a_from_vm(vm);
	struct nvgpu_mapped_buf *mapped_buffer = NULL;
	struct nvgpu_ctag_buffer_info binfo = { };
	enum gk20a_mem_rw_flag rw = buffer_rw_mode;
	struct nvgpu_vm_area *vm_area = NULL;
	int err = 0;
	bool va_allocated = true;

	/*
	 * The kind used as part of the key for map caching. HW may
	 * actually be programmed with the fallback kind in case the
	 * key kind is compressible but we're out of comptags.
	 */
	s16 map_key_kind;

	if ((map_access_requested == NVGPU_VM_MAP_ACCESS_READ_WRITE) &&
	    (buffer_rw_mode == gk20a_mem_flag_read_only)) {
		nvgpu_err(g, "RW mapping requested for RO buffer");
		return -EINVAL;
	}

	if (map_access_requested == NVGPU_VM_MAP_ACCESS_READ_ONLY) {
		rw = gk20a_mem_flag_read_only;
	}

	*mapped_buffer_arg = NULL;

	err = nvgpu_vm_map_check_attributes(vm, os_buf, &binfo, flags,
			compr_kind, incompr_kind, &map_key_kind);
	if (err != 0) {
		return err;
	}

	err = nvgpu_vm_new_mapping(vm, os_buf, mapped_buffer, sgt, &binfo,
			map_addr, &map_size, phys_offset, map_key_kind,
			mapped_buffer_arg);

	mapped_buffer = *mapped_buffer_arg;
	if (err < 0) {
		goto clean_up_nolock;
	}
	if (err == 1) {
		return 0;
	}

	nvgpu_mutex_acquire(&vm->update_gmmu_lock);

	/*
	 * Check if we should use a fixed offset for mapping this buffer.
	 */
	if ((flags & NVGPU_VM_MAP_FIXED_OFFSET) != 0U)  {
		err = nvgpu_vm_area_validate_buffer(vm,
						    map_addr,
						    map_size,
						    binfo.pgsz_idx,
						    &vm_area);
		if (err != 0) {
			goto clean_up;
		}

		va_allocated = false;
	}

	err = nvgpu_vm_do_map(vm, os_buf, sgt, &map_addr,
				map_size, phys_offset, rw, flags, batch,
				aperture, &binfo);
	if (err != 0) {
		goto clean_up;
	}

	nvgpu_init_list_node(&mapped_buffer->buffer_list);
	nvgpu_ref_init(&mapped_buffer->ref);
	mapped_buffer->addr         = map_addr;
	mapped_buffer->size         = map_size;
	mapped_buffer->pgsz_idx     = binfo.pgsz_idx;
	mapped_buffer->vm           = vm;
	mapped_buffer->flags        = binfo.flags;
	mapped_buffer->kind         = map_key_kind;
	mapped_buffer->va_allocated = va_allocated;
	mapped_buffer->vm_area      = vm_area;
#ifdef CONFIG_NVGPU_COMPRESSION
	mapped_buffer->ctag_offset  = binfo.ctag_offset;
#endif
	mapped_buffer->rw_flag      = rw;
	mapped_buffer->aperture     = aperture;

	nvgpu_insert_mapped_buf(vm, mapped_buffer);

	if (vm_area != NULL) {
		nvgpu_list_add_tail(&mapped_buffer->buffer_list,
			      &vm_area->buffer_list_head);
		mapped_buffer->vm_area = vm_area;
	}

	nvgpu_mutex_release(&vm->update_gmmu_lock);

	return 0;

clean_up:
	nvgpu_mutex_release(&vm->update_gmmu_lock);
clean_up_nolock:
	nvgpu_kfree(g, mapped_buffer);

	return err;
}

/*
 * Really unmap. This does the real GMMU unmap and removes the mapping from the
 * VM map tracking tree (and vm_area list if necessary).
 */
static void nvgpu_vm_do_unmap(struct nvgpu_mapped_buf *mapped_buffer,
			      struct vm_gk20a_mapping_batch *batch)
{
	struct vm_gk20a *vm = mapped_buffer->vm;
	struct gk20a *g = vm->mm->g;

	g->ops.mm.gmmu.unmap(vm,
			     mapped_buffer->addr,
			     mapped_buffer->size,
			     mapped_buffer->pgsz_idx,
			     mapped_buffer->va_allocated,
			     gk20a_mem_flag_none,
			     (mapped_buffer->vm_area != NULL) ?
			     mapped_buffer->vm_area->sparse : false,
			     batch);

	/*
	 * Remove from mapped buffer tree. Then delete the buffer from the
	 * linked list of mapped buffers; though note: not all mapped buffers
	 * are part of a vm_area.
	 */
	nvgpu_remove_mapped_buf(vm, mapped_buffer);
	nvgpu_list_del(&mapped_buffer->buffer_list);

	/*
	 * OS specific freeing. This is after the generic freeing incase the
	 * generic freeing relies on some component of the OS specific
	 * nvgpu_mapped_buf in some abstraction or the like.
	 */
	nvgpu_vm_unmap_system(mapped_buffer);

	nvgpu_kfree(g, mapped_buffer);
}

static struct nvgpu_mapped_buf *nvgpu_mapped_buf_from_ref(struct nvgpu_ref *ref)
{
	return (struct nvgpu_mapped_buf *)
		((uintptr_t)ref - offsetof(struct nvgpu_mapped_buf, ref));
}

/*
 * Note: the update_gmmu_lock of the VM that owns this buffer must be locked
 * before calling nvgpu_ref_put() with this function as the unref function
 * argument since this can modify the tree of maps.
 */
void nvgpu_vm_unmap_ref_internal(struct nvgpu_ref *ref)
{
	struct nvgpu_mapped_buf *mapped_buffer = nvgpu_mapped_buf_from_ref(ref);

	nvgpu_vm_do_unmap(mapped_buffer, mapped_buffer->vm->kref_put_batch);
}

/*
 * For fixed-offset buffers we must sync the buffer. That means we wait for the
 * buffer to hit a ref-count of 1 before proceeding.
 *
 * Note: this requires the update_gmmu_lock to be held since we release it and
 * re-aquire it in this function.
 */
static int nvgpu_vm_unmap_sync_buffer(struct vm_gk20a *vm,
				      struct nvgpu_mapped_buf *mapped_buffer)
{
	struct nvgpu_timeout timeout;
	int ret = 0;
	bool done = false;

	/*
	 * 100ms timer.
	 */
	nvgpu_timeout_init_cpu_timer(vm->mm->g, &timeout, 100);

	nvgpu_mutex_release(&vm->update_gmmu_lock);

	do {
		if (nvgpu_atomic_read(&mapped_buffer->ref.refcount) <= 1) {
			done = true;
		} else if (nvgpu_timeout_expired_msg(&timeout,
			   "sync-unmap failed on 0x%llx",
			   mapped_buffer->addr) != 0) {
			done = true;
		} else {
			nvgpu_msleep(10);
		}
	} while (!done);

	if (nvgpu_atomic_read(&mapped_buffer->ref.refcount) > 1) {
		ret = -ETIMEDOUT;
	}

	nvgpu_mutex_acquire(&vm->update_gmmu_lock);

	return ret;
}

void nvgpu_vm_unmap(struct vm_gk20a *vm, u64 offset,
		    struct vm_gk20a_mapping_batch *batch)
{
	struct nvgpu_mapped_buf *mapped_buffer;

	nvgpu_mutex_acquire(&vm->update_gmmu_lock);

	mapped_buffer = nvgpu_vm_find_mapped_buf(vm, offset);
	if (mapped_buffer == NULL) {
		goto done;
	}

	if ((mapped_buffer->flags & NVGPU_VM_MAP_FIXED_OFFSET) != 0U) {
		if (nvgpu_vm_unmap_sync_buffer(vm, mapped_buffer) != 0) {
			nvgpu_warn(vm->mm->g, "%d references remaining on 0x%llx",
				nvgpu_atomic_read(&mapped_buffer->ref.refcount),
				mapped_buffer->addr);
		}
	}

	/*
	 * Make sure we have access to the batch if we end up calling through to
	 * the unmap_ref function.
	 */
	vm->kref_put_batch = batch;
	nvgpu_ref_put(&mapped_buffer->ref, nvgpu_vm_unmap_ref_internal);
	vm->kref_put_batch = NULL;

done:
	nvgpu_mutex_release(&vm->update_gmmu_lock);
	return;
}

#ifdef CONFIG_NVGPU_COMPRESSION
static int nvgpu_vm_compute_compression(struct vm_gk20a *vm,
					struct nvgpu_ctag_buffer_info *binfo)
{
	bool kind_compressible = (binfo->compr_kind != NVGPU_KIND_INVALID);
	struct gk20a *g = gk20a_from_vm(vm);

	if (kind_compressible &&
	    vm->gmmu_page_sizes[binfo->pgsz_idx] <
	    g->ops.fb.compressible_page_size(g)) {
		/*
		 * Let's double check that there is a fallback kind
		 */
		if (binfo->incompr_kind == NVGPU_KIND_INVALID) {
			nvgpu_err(g,
				  "Unsupported page size for compressible "
				  "kind, but no fallback kind");
			return -EINVAL;
		} else {
			nvgpu_log(g, gpu_dbg_map,
				  "Unsupported page size for compressible "
				  "kind, demoting to incompressible");
			binfo->compr_kind = NVGPU_KIND_INVALID;
		}
	}

	return 0;
}
#endif
