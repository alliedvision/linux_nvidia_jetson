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
#include <nvgpu/list.h>
#include <nvgpu/dma.h>
#include <nvgpu/gmmu.h>
#include <nvgpu/pd_cache.h>
#include <nvgpu/nvgpu_mem.h>
#include <nvgpu/nvgpu_sgt.h>
#include <nvgpu/enabled.h>
#include <nvgpu/page_allocator.h>
#include <nvgpu/barrier.h>
#include <nvgpu/vidmem.h>
#include <nvgpu/sizes.h>
#include <nvgpu/types.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/static_analysis.h>
#include <nvgpu/errata.h>
#include <nvgpu/power_features/pg.h>

#ifdef CONFIG_NVGPU_TRACE
#define nvgpu_gmmu_dbg(g, attrs, fmt, args...)				\
	do {								\
		if ((attrs)->debug) {					\
			nvgpu_info(g, fmt, ##args);			\
		} else {						\
			nvgpu_log(g, gpu_dbg_map, fmt, ##args);		\
		}							\
NVGPU_COV_WHITELIST(false_positive, NVGPU_MISRA(Rule, 14_4), "Bug 2623654") \
	} while (false)

#define nvgpu_gmmu_dbg_v(g, attrs, fmt, args...)			\
	do {								\
		if ((attrs)->debug) {					\
			nvgpu_info(g, fmt, ##args);			\
		} else {						\
			nvgpu_log(g, gpu_dbg_map_v, fmt, ##args);	\
		}							\
NVGPU_COV_WHITELIST(false_positive, NVGPU_MISRA(Rule, 14_4), "Bug 2623654") \
	} while (false)
#endif

static int pd_allocate(struct vm_gk20a *vm,
		       struct nvgpu_gmmu_pd *pd,
		       const struct gk20a_mmu_level *l,
		       struct nvgpu_gmmu_attrs *attrs);
static u32 pd_get_size(const struct gk20a_mmu_level *l,
		struct nvgpu_gmmu_attrs *attrs);
/*
 * Core GMMU map function for the kernel to use. If @addr is 0 then the GPU
 * VA will be allocated for you. If addr is non-zero then the buffer will be
 * mapped at @addr.
 */
static u64 nvgpu_gmmu_map_core(struct vm_gk20a *vm,
			       struct nvgpu_mem *mem,
			       u64 addr,
			       u64 size,
			       u32 flags,
			       enum gk20a_mem_rw_flag rw_flag,
			       bool priv,
			       enum nvgpu_aperture aperture)
{
	struct gk20a *g = gk20a_from_vm(vm);
	u64 vaddr;

	struct nvgpu_sgt *sgt = nvgpu_sgt_create_from_mem(g, mem);

	if (sgt == NULL) {
		return 0;
	}

	/*
	 * Later on, when we free this nvgpu_mem's GPU mapping, we are going to
	 * potentially have to free the GPU VA space. If the address passed in
	 * is non-zero then this API is not expected to manage the VA space and
	 * therefor we should not try and free it. But otherwise, if we do
	 * manage the VA alloc, we obviously must free it.
	 */
	if (addr != 0U) {
		mem->free_gpu_va = false;
	} else {
		mem->free_gpu_va = true;
	}

	nvgpu_mutex_acquire(&vm->update_gmmu_lock);
	vaddr = g->ops.mm.gmmu.map(vm, addr,
				   sgt,    /* sg list */
				   0,      /* sg offset */
				   size,
				   GMMU_PAGE_SIZE_KERNEL,
				   0,      /* kind */
				   0,      /* ctag_offset */
				   flags, rw_flag,
				   false,  /* clear_ctags */
				   false,  /* sparse */
				   priv,   /* priv */
				   NULL,   /* mapping_batch handle */
				   aperture);
	nvgpu_mutex_release(&vm->update_gmmu_lock);

	nvgpu_sgt_free(g, sgt);

	if (vaddr == 0ULL) {
		nvgpu_err(g, "failed to map buffer!");
		return 0;
	}

	return vaddr;
}

/*
 * Map a nvgpu_mem into the GMMU. This is for kernel space to use.
 */
u64 nvgpu_gmmu_map_partial(struct vm_gk20a *vm,
		   struct nvgpu_mem *mem,
		   u64 size,
		   u32 flags,
		   enum gk20a_mem_rw_flag rw_flag,
		   bool priv,
		   enum nvgpu_aperture aperture)
{
	return nvgpu_gmmu_map_core(vm, mem, 0, size, flags, rw_flag, priv,
				   aperture);
}

/*
 * Map a nvgpu_mem into the GMMU. This is for kernel space to use.
 */
u64 nvgpu_gmmu_map(struct vm_gk20a *vm,
		   struct nvgpu_mem *mem,
		   u32 flags,
		   enum gk20a_mem_rw_flag rw_flag,
		   bool priv,
		   enum nvgpu_aperture aperture)
{
	return nvgpu_gmmu_map_core(vm, mem, 0, mem->size, flags, rw_flag, priv,
				   aperture);
}

/*
 * Like nvgpu_gmmu_map() except this can work on a fixed address.
 */
u64 nvgpu_gmmu_map_fixed(struct vm_gk20a *vm,
			 struct nvgpu_mem *mem,
			 u64 addr,
			 u64 size,
			 u32 flags,
			 enum gk20a_mem_rw_flag rw_flag,
			 bool priv,
			 enum nvgpu_aperture aperture)
{
	return nvgpu_gmmu_map_core(vm, mem, addr, size, flags, rw_flag, priv,
				   aperture);
}

void nvgpu_gmmu_unmap_addr(struct vm_gk20a *vm, struct nvgpu_mem *mem, u64 gpu_va)
{
	struct gk20a *g = gk20a_from_vm(vm);

	nvgpu_mutex_acquire(&vm->update_gmmu_lock);
	g->ops.mm.gmmu.unmap(vm,
			     gpu_va,
			     mem->size,
			     GMMU_PAGE_SIZE_KERNEL,
			     mem->free_gpu_va,
			     gk20a_mem_flag_none,
			     false,
			     NULL);

	nvgpu_mutex_release(&vm->update_gmmu_lock);
}

void nvgpu_gmmu_unmap(struct vm_gk20a *vm, struct nvgpu_mem *mem)
{
	nvgpu_gmmu_unmap_addr(vm, mem, mem->gpu_va);
}

int nvgpu_gmmu_init_page_table(struct vm_gk20a *vm)
{
	u32 pdb_size;
	int err;

	/*
	 * Need this just for page size. Everything else can be ignored. Also
	 * note that we can just use pgsz 0 (i.e small pages) since the number
	 * of bits present in the top level PDE are the same for small/large
	 * page VMs.
	 */
	struct nvgpu_gmmu_attrs attrs = {
		.pgsz = 0,
	};

	/*
	 * PDB size here must be at least 4096 bytes so that its address is 4K
	 * aligned. Although lower PDE tables can be aligned at 256B boundaries
	 * the PDB must be 4K aligned.
	 *
	 * Currently NVGPU_CPU_PAGE_SIZE is used, even when 64K, to work around an issue
	 * with the PDB TLB invalidate code not being pd_cache aware yet.
	 *
	 * Similarly, we can't use nvgpu_pd_alloc() here, because the top-level
	 * PD must have mem_offs be 0 for the invalidate code to work, so we
	 * can't use the PD cache.
	 */
	pdb_size = NVGPU_ALIGN(pd_get_size(&vm->mmu_levels[0], &attrs), NVGPU_CPU_PAGE_SIZE);

	err = nvgpu_pd_cache_alloc_direct(vm->mm->g, &vm->pdb, pdb_size);
	if (err != 0) {
		return err;
	}
	vm->pdb.pd_size = pdb_size;

	/*
	 * One nvgpu_mb() is done after all mapping operations. Don't need
	 * individual barriers for each PD write.
	 */
	vm->pdb.mem->skip_wmb = true;

	return err;
}

/*
 * Return the aligned length based on the page size in attrs.
 */
static u64 nvgpu_align_map_length(struct vm_gk20a *vm, u64 length,
				  struct nvgpu_gmmu_attrs *attrs)
{
	u64 page_size = vm->gmmu_page_sizes[attrs->pgsz];

	return NVGPU_ALIGN(length, page_size);
}

static u32 pd_entries(const struct gk20a_mmu_level *l,
		      struct nvgpu_gmmu_attrs *attrs)
{
	/*
	 * Number of entries in a PD is easy to compute from the number of bits
	 * used to index the page directory. That is simply 2 raised to the
	 * number of bits.
	 */

	u32 bit;

	bit = nvgpu_safe_sub_u32(l->hi_bit[attrs->pgsz],
			l->lo_bit[attrs->pgsz]);
	bit = nvgpu_safe_add_u32(bit, 1U);
	return BIT32(bit);
}

/*
 * Computes the size of a PD table (in bytes).
 */
static u32 pd_get_size(const struct gk20a_mmu_level *l,
		   struct nvgpu_gmmu_attrs *attrs)
{
	return nvgpu_safe_mult_u32(pd_entries(l, attrs), l->entry_size);
}

/*
 * Allocate a physically contiguous region big enough for a gmmu page table
 * of the specified level and page size. The whole range is zeroed so that any
 * accesses will fault until proper values are programmed.
 */
static int pd_allocate(struct vm_gk20a *vm,
		       struct nvgpu_gmmu_pd *pd,
		       const struct gk20a_mmu_level *l,
		       struct nvgpu_gmmu_attrs *attrs)
{
	int err;

	/*
	 * Same basic logic as in pd_allocate_children() except we (re)allocate
	 * the underlying DMA memory here.
	 */
	if (pd->mem != NULL) {
		if (pd->pd_size >= pd_get_size(l, attrs)) {
			return 0;
		}
		nvgpu_pd_free(vm, pd);
		pd->mem = NULL;
	}

	err = nvgpu_pd_alloc(vm, pd, pd_get_size(l, attrs));
	if (err != 0) {
		nvgpu_info(vm->mm->g, "error allocating page directory!");
		return err;
	}

	/*
	 * One nvgpu_mb() is done after all mapping operations. Don't need
	 * individual barriers for each PD write.
	 */
	pd->mem->skip_wmb = true;

	return 0;
}

/*
 * Compute what page directory index at the passed level the passed virtual
 * address corresponds to. @attrs is necessary for determining the page size
 * which is used to pick the right bit offsets for the GMMU level.
 */
static u32 pd_index(const struct gk20a_mmu_level *l, u64 virt,
		    struct nvgpu_gmmu_attrs *attrs)
{
	u64 pd_mask;
	u32 pd_shift;
	u64 tmp_index;

	nvgpu_assert(attrs->pgsz < ARRAY_SIZE(l->lo_bit));
	pd_shift = l->lo_bit[attrs->pgsz];

	pd_mask = BIT64(nvgpu_safe_add_u64((u64)l->hi_bit[attrs->pgsz], 1ULL));
	pd_mask = nvgpu_safe_sub_u64(pd_mask, 1ULL);

	/*
	 * For convenience we don't bother computing the lower bound of the
	 * mask; it's easier to just shift it off.
	 */
	tmp_index = (virt & pd_mask) >> pd_shift;
	nvgpu_assert(tmp_index <= U64(U32_MAX));
	return U32(tmp_index);
}

static int pd_allocate_children(struct vm_gk20a *vm,
				const struct gk20a_mmu_level *l,
				struct nvgpu_gmmu_pd *pd,
				struct nvgpu_gmmu_attrs *attrs)
{
	struct gk20a *g = gk20a_from_vm(vm);

	/*
	 * Check that we have already allocated enough pd_entries for this
	 * page directory. There's 4 possible cases:
	 *
	 *   1. This pd is new and therefor has no entries.
	 *   2. This pd does not have enough entries.
	 *   3. This pd has exactly the right number of entries.
	 *   4. This pd has more than enough entries.
	 *
	 * (3) and (4) are easy: just return. Case (1) is also straight forward:
	 * just allocate enough space for the number of pd_entries.
	 *
	 * Case (2) is rare but can happen. It occurs when we have a PD that has
	 * already been allocated for some VA range with a page size of 64K. If
	 * later on we free that VA range and then remap that VA range with a
	 * 4K page size map then we now need more pd space. As such we need to
	 * reallocate this pd entry array.
	 *
	 * Critically case (2) should _only_ ever happen when the PD is not in
	 * use. Obviously blowing away a bunch of previous PDEs would be
	 * catastrophic. But the buddy allocator logic prevents mixing page
	 * sizes within a single last level PD range. Therefor we should only
	 * ever see this once the entire PD range has been freed - otherwise
	 * there would be mixing (which, remember, is prevented by the buddy
	 * allocator).
	 */
	if (pd->num_entries >= pd_entries(l, attrs)) {
		return 0;
	}

	if (pd->entries != NULL) {
		nvgpu_vfree(g, pd->entries);
	}

	pd->num_entries = pd_entries(l, attrs);
	pd->entries = nvgpu_vzalloc(g,
		nvgpu_safe_mult_u64(sizeof(struct nvgpu_gmmu_pd),
				(unsigned long)pd->num_entries));
	if (pd->entries == NULL) {
		pd->num_entries = 0;
		return -ENOMEM;
	}

	return 0;
}

/*
 * If the next level has an update_entry function then we know
 * that _this_ level points to PDEs (not PTEs). Thus we need to
 * have a bunch of children PDs.
 */
static int nvgpu_set_pd_level_is_next_level_pde(struct vm_gk20a *vm,
				struct nvgpu_gmmu_pd *pd,
				struct nvgpu_gmmu_pd **next_pd_ptr,
				const struct gk20a_mmu_level *l,
				const struct gk20a_mmu_level *next_l,
				u32 pd_idx,
				struct nvgpu_gmmu_attrs *attrs)
{
	struct nvgpu_gmmu_pd *next_pd = *next_pd_ptr;

	if (next_l->update_entry != NULL) {
		int err = 0;

		if (pd_allocate_children(vm, l, pd, attrs) != 0) {
			return -ENOMEM;
		}

		/*
		 * Get the next PD so that we know what to put in this
		 * current PD. If the next level is actually PTEs then
		 * we don't need this - we will just use the real
		 * physical target.
		 */
		next_pd = &pd->entries[pd_idx];

		/*
		 * Allocate the backing memory for next_pd.
		 */
		err = pd_allocate(vm, next_pd, next_l, attrs);
		if (err != 0) {
			return err;
		}
	}
	*next_pd_ptr = next_pd;
	return 0;
}

/*
 * This function programs the GMMU based on two ranges: a physical range and a
 * GPU virtual range. The virtual is mapped to the physical. Physical in this
 * case can mean either a real physical sysmem address or a IO virtual address
 * (for instance when a system has an IOMMU running).
 *
 * The rest of the parameters are for describing the actual mapping itself.
 *
 * This function recursively calls itself for handling PDEs. At the final level
 * a PTE handler is called. The phys and virt ranges are adjusted for each
 * recursion so that each invocation of this function need only worry about the
 * range it is passed.
 *
 * phys_addr will always point to a contiguous range - the discontiguous nature
 * of DMA buffers is taken care of at the layer above this.
 */
NVGPU_COV_WHITELIST_BLOCK_BEGIN(deviate, 1, NVGPU_MISRA(Rule, 17_2), "TID-278")
static int nvgpu_set_pd_level(struct vm_gk20a *vm,
			      struct nvgpu_gmmu_pd *pd,
			      u32 lvl,
			      u64 phys_addr,
			      u64 virt_addr, u64 length,
			      struct nvgpu_gmmu_attrs *attrs)
{
	int err = 0;
	u64 pde_range;
	struct gk20a *g = gk20a_from_vm(vm);
	struct nvgpu_gmmu_pd *next_pd = NULL;
	const struct gk20a_mmu_level *l      = &vm->mmu_levels[lvl];
	const struct gk20a_mmu_level *next_l =
				&vm->mmu_levels[nvgpu_safe_add_u32(lvl, 1)];

	/*
	 * 5 levels for Pascal+. For pre-pascal we only have 2. This puts
	 * offsets into the page table debugging code which makes it easier to
	 * see what level prints are from.
	 */
#ifdef CONFIG_NVGPU_TRACE
	static const char *lvl_debug[] = {
		"",          /* L=0 */
		"  ",        /* L=1 */
		"    ",      /* L=2 */
		"      ",    /* L=3 */
		"        ",  /* L=4 */
	};

	nvgpu_gmmu_dbg_v(g, attrs,
			 "L=%d   %sGPU virt %#-12llx +%#-9llx -> phys %#-12llx",
			 lvl,
			 lvl_debug[lvl],
			 virt_addr,
			 length,
			 phys_addr);
#endif /* CONFIG_NVGPU_TRACE */

	/* This limits recursion */
	nvgpu_assert(lvl < g->ops.mm.gmmu.get_max_page_table_levels(g));

	pde_range = 1ULL << (u64)l->lo_bit[attrs->pgsz];

	/*
	 * Iterate across the mapping in chunks the size of this level's PDE.
	 * For each of those chunks program our level's PDE and then, if there's
	 * a next level, program the next level's PDEs/PTEs.
	 */
	while (length != 0ULL) {
		u32 pd_idx = pd_index(l, virt_addr, attrs);
		u64 chunk_size;
		u64 target_addr;
		u64 tmp_len;

		/*
		 * Truncate the pde_range when the virtual address does not
		 * start at a PDE boundary.
		 */
		nvgpu_assert(pde_range >= 1ULL);
		tmp_len = nvgpu_safe_sub_u64(pde_range,
				virt_addr & (pde_range - 1U));
		chunk_size = min(length, tmp_len);

		err = nvgpu_set_pd_level_is_next_level_pde(vm, pd, &next_pd,
						l, next_l, pd_idx, attrs);
		if (err != 0) {
			return err;
		}

		/*
		 * This is the address we want to program into the actual PDE/
		 * PTE. When the next level is PDEs we need the target address
		 * to be the table of PDEs. When the next level is PTEs the
		 * target addr is the real physical address we are aiming for.
		 */
		target_addr = (next_pd != NULL) ?
			nvgpu_pd_gpu_addr(g, next_pd) : phys_addr;

		l->update_entry(vm, l,
				pd, pd_idx,
				virt_addr,
				target_addr,
				attrs);

		if (next_l->update_entry != NULL) {
			err = nvgpu_set_pd_level(vm, next_pd,
						 nvgpu_safe_add_u32(lvl, 1U),
						 phys_addr,
						 virt_addr,
						 chunk_size,
						 attrs);

			if (err != 0) {
				return err;
			}
		}

		virt_addr = nvgpu_safe_add_u64(virt_addr, chunk_size);

		/*
		 * Only add to phys_addr if it's non-zero. A zero value implies
		 * we are unmapping as as a result we don't want to place
		 * non-zero phys addresses in the PTEs. A non-zero phys-addr
		 * would also confuse the lower level PTE programming code.
		 */
		if (phys_addr != 0ULL) {
			phys_addr += chunk_size;
		}
		length -= chunk_size;
	}

#ifdef CONFIG_NVGPU_TRACE
	nvgpu_gmmu_dbg_v(g, attrs, "L=%d   %s%s", lvl, lvl_debug[lvl],
			 "ret!");
#endif /* CONFIG_NVGPU_TRACE */

	return 0;
}
NVGPU_COV_WHITELIST_BLOCK_END(NVGPU_MISRA(Rule, 17_2))

static int nvgpu_gmmu_do_update_page_table_sgl(struct vm_gk20a *vm,
				struct nvgpu_sgt *sgt, void *sgl,
				u64 *space_to_skip_ptr,
				u64 *virt_addr_ptr, u64 *length_ptr,
				u64 phys_addr_val, u64 ipa_addr_val,
				u64 phys_length_val, u64 sgl_length_val,
				struct nvgpu_gmmu_attrs *attrs)
{
	struct gk20a *g = gk20a_from_vm(vm);
	u64 space_to_skip = *space_to_skip_ptr;
	u64 virt_addr = *virt_addr_ptr;
	u64 length = *length_ptr;
	u64 phys_addr = phys_addr_val;
	u64 ipa_addr = ipa_addr_val;
	u64 phys_length = phys_length_val;
	u64 sgl_length = sgl_length_val;
	int err;

	while ((sgl_length > 0ULL) && (length > 0ULL)) {
		/*
		 * Holds the size of the portion of SGL that is backed
		 * with physically contiguous memory.
		 */
		u64 sgl_contiguous_length;
		/*
		 * Number of bytes of the SGL entry that is actually
		 * mapped after accounting for space_to_skip.
		 */
		u64 mapped_sgl_length;

		/*
		 * For virtualized OSes translate IPA to PA. Retrieve
		 * the size of the underlying physical memory chunk to
		 * which SGL has been mapped.
		 */
		phys_addr = nvgpu_sgt_ipa_to_pa(g, sgt, sgl, ipa_addr,
				&phys_length);
		phys_addr = nvgpu_safe_add_u64(
				g->ops.mm.gmmu.gpu_phys_addr(g, attrs,
							     phys_addr),
				space_to_skip);

		/*
		 * For virtualized OSes when phys_length is less than
		 * sgl_length check if space_to_skip exceeds phys_length
		 * if so skip this memory chunk
		 */
		if (space_to_skip >= phys_length) {
			space_to_skip -= phys_length;
			ipa_addr = nvgpu_safe_add_u64(ipa_addr,
						      phys_length);
			sgl_length -= phys_length;
			continue;
		}

		sgl_contiguous_length = min(phys_length, sgl_length);
		mapped_sgl_length = min(length, sgl_contiguous_length -
				space_to_skip);

		err = nvgpu_set_pd_level(vm, &vm->pdb,
					 0U,
					 phys_addr,
					 virt_addr,
					 mapped_sgl_length,
					 attrs);
		if (err != 0) {
			return err;
		}

		/*
		 * Update the map pointer and the remaining length.
		 */
		virt_addr  = nvgpu_safe_add_u64(virt_addr,
						mapped_sgl_length);
		length     = nvgpu_safe_sub_u64(length,
						mapped_sgl_length);
		sgl_length = nvgpu_safe_sub_u64(sgl_length,
				nvgpu_safe_add_u64(mapped_sgl_length,
						   space_to_skip));
		ipa_addr   = nvgpu_safe_add_u64(ipa_addr,
				nvgpu_safe_add_u64(mapped_sgl_length,
						   space_to_skip));

		/*
		 * Space has been skipped so zero this for future
		 * chunks.
		 */
		space_to_skip = 0;
	}
	*space_to_skip_ptr = space_to_skip;
	*virt_addr_ptr = virt_addr;
	*length_ptr = length;
	return 0;
}

static int nvgpu_gmmu_do_update_page_table_no_iommu(struct vm_gk20a *vm,
				struct nvgpu_sgt *sgt, u64 space_to_skip_val,
				u64 virt_addr_val, u64 length_val,
				struct nvgpu_gmmu_attrs *attrs)
{
	struct gk20a *g = gk20a_from_vm(vm);
	void *sgl;
	u64 space_to_skip = space_to_skip_val;
	u64 virt_addr = virt_addr_val;
	u64 length = length_val;
	int err;

	nvgpu_sgt_for_each_sgl(sgl, sgt) {
		/*
		 * ipa_addr == phys_addr for non virtualized OSes.
		 */
		u64 phys_addr = 0ULL;
		u64 ipa_addr = 0ULL;
		/*
		 * For non virtualized OSes SGL entries are contiguous in
		 * physical memory (sgl_length == phys_length). For virtualized
		 * OSes SGL entries are mapped to intermediate physical memory
		 * which may subsequently point to discontiguous physical
		 * memory. Therefore phys_length may not be equal to sgl_length.
		 */
		u64 phys_length = 0ULL;
		u64 sgl_length = 0ULL;

		/*
		 * Cut out sgl ents for space_to_skip.
		 */
		if ((space_to_skip != 0ULL) &&
			(space_to_skip >= nvgpu_sgt_get_length(sgt, sgl))) {
			space_to_skip -= nvgpu_sgt_get_length(sgt, sgl);
			continue;
		}

		/*
		 * IPA and PA have 1:1 mapping for non virtualized OSes.
		 */
		ipa_addr = nvgpu_sgt_get_ipa(g, sgt, sgl);

		/*
		 * For non-virtualized OSes SGL entries are contiguous and hence
		 * sgl_length == phys_length. For virtualized OSes the
		 * phys_length will be updated by nvgpu_sgt_ipa_to_pa.
		 */
		sgl_length = nvgpu_sgt_get_length(sgt, sgl);
		phys_length = sgl_length;

		err = nvgpu_gmmu_do_update_page_table_sgl(vm, sgt, sgl,
				&space_to_skip, &virt_addr, &length,
				phys_addr, ipa_addr,
				phys_length, sgl_length,
				attrs);
		if (err != 0) {
			return err;
		}

		if (length == 0ULL) {
			break;
		}
	}
	return 0;
}

static struct nvgpu_gmmu_attrs gmmu_unmap_attrs(u32 pgsz)
{
	/*
	 * Most fields are not relevant for unmapping (zero physical address)
	 * because the lowest PTE-level entries are written with only zeros.
	 */
	return (struct nvgpu_gmmu_attrs){
		/*
		 * page size has to match the original mapping, so that we'll
		 * reach the correct PDEs/PTEs.
		 */
		.pgsz = pgsz,
		/* just in case as this is an enum */
		.aperture = APERTURE_INVALID,
		/*
		 * note: mappings with zero phys addr may be sparse; access to
		 * such memory would not fault, so we'll want this to be false
		 * explicitly.
		 */
		.sparse = false,
	};
}

static int nvgpu_gmmu_do_update_page_table(struct vm_gk20a *vm,
					   struct nvgpu_sgt *sgt,
					   u64 space_to_skip,
					   u64 virt_addr,
					   u64 length,
					   struct nvgpu_gmmu_attrs *attrs)
{
	struct gk20a *g = gk20a_from_vm(vm);
	bool is_iommuable, sgt_is_iommuable;
	int err = 0;

	if (sgt == NULL) {
		/*
		 * This is considered an unmap. Just pass in 0 as the physical
		 * address for the entire GPU range.
		 */
		nvgpu_assert(virt_addr != 0ULL);

		err = nvgpu_set_pd_level(vm, &vm->pdb,
					 0U,
					 0,
					 virt_addr, length,
					 attrs);
		if (err != 0) {
			nvgpu_err(g, "Failed!");
		}
		return err;
	}

	/*
	 * At this point we have a scatter-gather list pointing to some number
	 * of discontiguous chunks of memory. We must iterate over that list and
	 * generate a GMMU map call for each chunk. There are several
	 * possibilities:
	 *
	 *  1. IOMMU enabled, IOMMU addressing (typical iGPU)
	 *  2. IOMMU enabled, IOMMU bypass     (NVLINK bypasses SMMU)
	 *  3. IOMMU disabled                  (less common but still supported)
	 *  4. VIDMEM
	 *
	 * For (1) we can assume that there's really only one actual SG chunk
	 * since the IOMMU gives us a single contiguous address range. However,
	 * for (2), (3) and (4) we have to actually go through each SG entry and
	 * map each chunk individually.
	 */
	is_iommuable = nvgpu_iommuable(g);
	sgt_is_iommuable = nvgpu_sgt_iommuable(g, sgt);
	if (nvgpu_aperture_is_sysmem(attrs->aperture) &&
	    is_iommuable && sgt_is_iommuable) {
		u64 io_addr = nvgpu_sgt_get_gpu_addr(g, sgt, sgt->sgl, attrs);

		io_addr = nvgpu_safe_add_u64(io_addr, space_to_skip);

		err = nvgpu_set_pd_level(vm, &vm->pdb,
					 0U,
					 io_addr,
					 virt_addr,
					 length,
					 attrs);
	} else {
		/*
		 * Handle cases (2), (3), and (4): do the no-IOMMU mapping. In this case
		 * we really are mapping physical pages directly.
		 */
		err = nvgpu_gmmu_do_update_page_table_no_iommu(vm, sgt, space_to_skip,
							virt_addr, length, attrs);
	}

	if (err < 0) {
		struct nvgpu_gmmu_attrs unmap_attrs = gmmu_unmap_attrs(attrs->pgsz);
		int err_unmap;

		nvgpu_err(g, "Map failed! Backing off.");
		err_unmap = nvgpu_set_pd_level(vm, &vm->pdb,
					 0U,
					 0,
					 virt_addr, length,
					 &unmap_attrs);
		/*
		 * If the mapping attempt failed, this unmap attempt may also
		 * fail, but it can only up to the point where the map did,
		 * correctly undoing what was mapped at first. Log and discard
		 * this error code.
		 */
		if (err_unmap != 0) {
			nvgpu_err(g, "unmap err: %d", err_unmap);
		}
	}

	return err;
}

static int nvgpu_gmmu_cache_maint_map(struct gk20a *g, struct vm_gk20a *vm,
		struct vm_gk20a_mapping_batch *batch)
{
	int err = 0;

	if (batch == NULL) {
		err = nvgpu_pg_elpg_ms_protected_call(g,
				g->ops.fb.tlb_invalidate(g, vm->pdb.mem));
		if (err != 0) {
			nvgpu_err(g, "fb.tlb_invalidate() failed err=%d", err);
		}
	} else {
		batch->need_tlb_invalidate = true;
	}

	return err;
}

static int nvgpu_gmmu_cache_maint_unmap(struct gk20a *g, struct vm_gk20a *vm,
		struct vm_gk20a_mapping_batch *batch)
{
	int err = 0;

	if (batch == NULL) {
		if ((nvgpu_pg_elpg_ms_protected_call(g,
			g->ops.mm.cache.l2_flush(g, true))) != 0) {
			nvgpu_err(g, "gk20a_mm_l2_flush[1] failed");
		}
		err = nvgpu_pg_elpg_ms_protected_call(g,
				g->ops.fb.tlb_invalidate(g, vm->pdb.mem));
		if (err != 0) {
			nvgpu_err(g, "fb.tlb_invalidate() failed err=%d", err);
		}
	} else {
		if (!batch->gpu_l2_flushed) {
			if ((nvgpu_pg_elpg_ms_protected_call(g,
				g->ops.mm.cache.l2_flush(g, true))) != 0) {
				nvgpu_err(g, "gk20a_mm_l2_flush[2] failed");
			}
			batch->gpu_l2_flushed = true;
		}
		batch->need_tlb_invalidate = true;
	}

	return err;
}

/*
 * This is the true top level GMMU mapping logic. This breaks down the incoming
 * scatter gather table and does actual programming of GPU virtual address to
 * physical* address.
 *
 * The update of each level of the page tables is farmed out to chip specific
 * implementations. But the logic around that is generic to all chips. Every
 * chip has some number of PDE levels and then a PTE level.
 *
 * Each chunk of the incoming SGL is sent to the chip specific implementation
 * of page table update.
 *
 * [*] Note: the "physical" address may actually be an IO virtual address in the
 *     case of SMMU usage.
 */
static void nvgpu_gmmu_update_page_table_dbg_print(struct gk20a *g,
		struct nvgpu_gmmu_attrs *attrs, struct vm_gk20a *vm,
		struct nvgpu_sgt *sgt, u64 space_to_skip,
		u64 virt_addr, u64 length, u32 page_size)
{
#ifdef CONFIG_NVGPU_TRACE
	nvgpu_gmmu_dbg(g, attrs,
		"vm=%s "
		"%-5s GPU virt %#-12llx +%#-9llx    phys %#-12llx "
		"phys offset: %#-4llx;  pgsz: %3dkb perm=%-2s | "
		"kind=%#02x APT=%-6s %c%c%c%c%c",
		vm->name,
		(sgt != NULL) ? "MAP" : "UNMAP",
		virt_addr,
		length,
		(sgt != NULL) ?
		nvgpu_sgt_get_phys(g, sgt, sgt->sgl) : 0ULL,
		space_to_skip,
		page_size >> 10,
		nvgpu_gmmu_perm_str(attrs->rw_flag),
		attrs->kind_v,
		nvgpu_aperture_str(attrs->aperture),
		attrs->cacheable ? 'C' : '-',
		attrs->sparse    ? 'S' : '-',
		attrs->priv      ? 'P' : '-',
		attrs->valid     ? 'V' : '-',
		attrs->platform_atomic ? 'A' : '-');
#else
	(void)g;
	(void)attrs;
	(void)vm;
	(void)sgt;
	(void)space_to_skip;
	(void)virt_addr;
	(void)length;
	(void)page_size;
#endif /* CONFIG_NVGPU_TRACE */
}

static int nvgpu_gmmu_update_page_table(struct vm_gk20a *vm,
					struct nvgpu_sgt *sgt,
					u64 space_to_skip,
					u64 virt_addr,
					u64 length,
					struct nvgpu_gmmu_attrs *attrs)
{
	struct gk20a *g = gk20a_from_vm(vm);
	u32 page_size;
	int err;

	/* note: here we need to map kernel to small, since the
	 * low-level mmu code assumes 0 is small and 1 is big pages */
	if (attrs->pgsz == GMMU_PAGE_SIZE_KERNEL) {
		attrs->pgsz = GMMU_PAGE_SIZE_SMALL;
	}

	page_size = vm->gmmu_page_sizes[attrs->pgsz];

	if ((page_size == 0U) ||
		((space_to_skip & (U64(page_size) - U64(1))) != 0ULL)) {
		return -EINVAL;
	}

	/*
	 * Update length to be aligned to the passed page size.
	 */
	length = nvgpu_align_map_length(vm, length, attrs);

	nvgpu_gmmu_update_page_table_dbg_print(g, attrs, vm, sgt,
				space_to_skip, virt_addr, length, page_size);

	err = nvgpu_gmmu_do_update_page_table(vm,
					      sgt,
					      space_to_skip,
					      virt_addr,
					      length,
					      attrs);
	if (err != 0) {
		nvgpu_err(g, "nvgpu_gmmu_do_update_page_table returned error");
	}

	nvgpu_mb();

#ifdef CONFIG_NVGPU_TRACE
	nvgpu_gmmu_dbg(g, attrs, "%-5s Done!",
				(sgt != NULL) ? "MAP" : "UNMAP");
#endif /* CONFIG_NVGPU_TRACE */

	return err;
}

/**
 * nvgpu_gmmu_map_locked - Map a buffer into the GMMU
 *
 * This is for non-vGPU chips. It's part of the HAL at the moment but really
 * should not be. Chip specific stuff is handled at the PTE/PDE programming
 * layer. The rest of the logic is essentially generic for all chips.
 *
 * To call this function you must have locked the VM lock: vm->update_gmmu_lock.
 * However, note: this function is not called directly. It's used through the
 * mm.gmmu_lock() HAL. So before calling the mm.gmmu_lock() HAL make sure you
 * have the update_gmmu_lock aquired.
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
			  enum nvgpu_aperture aperture)
{
	struct gk20a *g = gk20a_from_vm(vm);
	int err = 0;
	int err_maint;
	bool allocated = false;
	struct nvgpu_gmmu_attrs attrs = {
		.pgsz      = pgsz_idx,
		.kind_v    = kind_v,
		.cacheable = ((flags & NVGPU_VM_MAP_CACHEABLE) != 0U),
		.rw_flag   = rw_flag,
		.sparse    = sparse,
		.priv      = priv,
		.valid     = (flags & NVGPU_VM_MAP_UNMAPPED_PTE) == 0U,
		.aperture  = aperture,
		.platform_atomic = (flags & NVGPU_VM_MAP_PLATFORM_ATOMIC) != 0U
	};
#ifdef CONFIG_NVGPU_COMPRESSION
	u64 ctag_granularity = g->ops.fb.compression_page_size(g);

	attrs.ctag      = (u64)ctag_offset * ctag_granularity;
	/*
	 * We need to add the buffer_offset within compression_page_size so that
	 * the programmed ctagline gets increased at compression_page_size
	 * boundaries.
	 */
	if (attrs.ctag != 0ULL) {
		nvgpu_assert(ctag_granularity >= 1ULL);
		attrs.ctag = nvgpu_safe_add_u64(attrs.ctag,
				buffer_offset & (ctag_granularity - U64(1)));
	}

	attrs.cbc_comptagline_mode =
		g->ops.fb.is_comptagline_mode_enabled != NULL ?
			g->ops.fb.is_comptagline_mode_enabled(g) : true;
#endif

	attrs.l3_alloc  = ((flags & NVGPU_VM_MAP_L3_ALLOC)  != 0U);

	if (nvgpu_is_enabled(g, NVGPU_SUPPORT_TEGRA_RAW)) {
#ifdef CONFIG_NVGPU_TRACE
		nvgpu_gmmu_dbg_v(g, &attrs,
			"TEGRA_RAW format is requested");
#endif /* CONFIG_NVGPU_TRACE */
		attrs.tegra_raw = ((flags & NVGPU_VM_MAP_TEGRA_RAW) != 0U);
	}
#if defined(CONFIG_NVGPU_NON_FUSA)
	if (nvgpu_is_errata_present(g, NVGPU_ERRATA_3288192) &&
							(attrs.l3_alloc)) {
#ifdef CONFIG_NVGPU_TRACE
		nvgpu_gmmu_dbg_v(g, &attrs,
			"L3 alloc is requested when L3 cache is not supported");
#endif
		attrs.l3_alloc = false;
	}
#endif
	(void)clear_ctags;
	(void)ctag_offset;

	/*
	 * Only allocate a new GPU VA range if we haven't already been passed a
	 * GPU VA range. This facilitates fixed mappings.
	 */
	if (vaddr == 0ULL) {
		vaddr = nvgpu_vm_alloc_va(vm, size, pgsz_idx);
		if (vaddr == 0ULL) {
			nvgpu_err(g, "failed to allocate va space");
			err = -ENOMEM;
			goto fail_alloc;
		}
		allocated = true;
	}

	err = nvgpu_gmmu_update_page_table(vm, sgt, buffer_offset,
					   vaddr, size, &attrs);
	if (err != 0) {
		nvgpu_err(g, "failed to update ptes on map, err=%d", err);
		/*
		 * The PTEs were partially filled and then unmapped again. Act
		 * as if this was an unmap to guard against concurrent GPU
		 * accesses to the buffer.
		 */
		err_maint = nvgpu_gmmu_cache_maint_unmap(g, vm, batch);
		if (err_maint != 0) {
			nvgpu_err(g,
				  "failed cache maintenance on failed map, err=%d",
				  err_maint);
			err = err_maint;
		}
	} else {
		err_maint = nvgpu_gmmu_cache_maint_map(g, vm, batch);
		if (err_maint != 0) {
			nvgpu_err(g,
				  "failed cache maintenance on map! Backing off, err=%d",
				  err_maint);
			/*
			 * Record this original error, and log and discard the
			 * below if anything goes further wrong.
			 */
			err = err_maint;

			/*
			 * This should not fail because the PTEs were just
			 * filled successfully above.
			 */
			attrs = gmmu_unmap_attrs(pgsz_idx);
			err_maint = nvgpu_gmmu_update_page_table(vm, NULL, 0, vaddr,
					size, &attrs);
			if (err_maint != 0) {
				nvgpu_err(g,
					  "failed to update gmmu ptes, err=%d",
					  err_maint);
			}

			/* Try the unmap maintenance in any case */
			err_maint = nvgpu_gmmu_cache_maint_unmap(g, vm, batch);
			if (err_maint != 0) {
				nvgpu_err(g,
					  "failed cache maintenance twice, err=%d",
					  err_maint);
			}
		}
	}

	if (err != 0) {
		goto fail_free_va;
	}

	return vaddr;

fail_free_va:
	if (allocated) {
		nvgpu_vm_free_va(vm, vaddr, pgsz_idx);
	}
fail_alloc:
	nvgpu_err(g, "%s: failed with err=%d", __func__, err);
	return 0;
}

void nvgpu_gmmu_unmap_locked(struct vm_gk20a *vm,
			     u64 vaddr,
			     u64 size,
			     u32 pgsz_idx,
			     bool va_allocated,
			     enum gk20a_mem_rw_flag rw_flag,
			     bool sparse,
			     struct vm_gk20a_mapping_batch *batch)
{
	int err = 0;
	struct gk20a *g = gk20a_from_vm(vm);
	struct nvgpu_gmmu_attrs attrs = gmmu_unmap_attrs(pgsz_idx);

	(void)rw_flag;

	attrs.sparse = sparse;

	if (va_allocated) {
		nvgpu_vm_free_va(vm, vaddr, pgsz_idx);
	}

	err = nvgpu_gmmu_update_page_table(vm, NULL, 0,
					   vaddr, size, &attrs);
	if (err != 0) {
		nvgpu_err(g, "failed to update gmmu ptes on unmap");
	}

	(void)nvgpu_gmmu_cache_maint_unmap(g, vm, batch);
}

u32 nvgpu_pte_words(struct gk20a *g)
{
	const struct gk20a_mmu_level *l =
		g->ops.mm.gmmu.get_mmu_levels(g, SZ_64K);
	const struct gk20a_mmu_level *next_l;

	/*
	 * Iterate to the bottom GMMU level - the PTE level. The levels array
	 * is always NULL terminated (by the update_entry function).
	 */
	do {
		next_l = l + 1;
		if (next_l->update_entry == NULL) {
			break;
		}

		l++;
	} while (true);

	return l->entry_size / (u32)sizeof(u32);
}

/* Walk last level of page table to find PTE */
static int nvgpu_locate_pte_last_level(struct gk20a *g,
				struct nvgpu_gmmu_pd *pd,
				const struct gk20a_mmu_level *l,
				struct nvgpu_gmmu_pd **pd_out,
				u32 *pd_idx_out, u32 *pd_offs_out,
				u32 *data, u32 pd_idx)
{
	u32 pte_base;
	u32 pte_size;
	u32 idx;

	if (pd->mem == NULL) {
		return -EINVAL;
	}

	/*
	 * Take into account the real offset into the nvgpu_mem
	 * since the PD may be located at an offset other than 0
	 * (due to PD packing).
	 */
	pte_base = nvgpu_safe_add_u32(
		pd->mem_offs / (u32)sizeof(u32),
		nvgpu_pd_offset_from_index(l, pd_idx));
	pte_size = l->entry_size / (u32)sizeof(u32);

	if (data != NULL) {
		for (idx = 0; idx < pte_size; idx++) {
			u32 tmp_word = nvgpu_safe_add_u32(idx,
						pte_base);
			data[idx] = nvgpu_mem_rd32(g, pd->mem,
					tmp_word);
		}
	}

	if (pd_out != NULL) {
		*pd_out = pd;
	}

	if (pd_idx_out != NULL) {
		*pd_idx_out = pd_idx;
	}

	if (pd_offs_out != NULL) {
		*pd_offs_out = nvgpu_pd_offset_from_index(l,
				pd_idx);
	}
	return 0;
}

/*
 * Recursively walk the pages tables to find the PTE.
 */
static int nvgpu_locate_pte(struct gk20a *g, struct vm_gk20a *vm,
			    struct nvgpu_gmmu_pd *pd,
			    u64 vaddr, u32 lvl,
			    struct nvgpu_gmmu_attrs *attrs,
			    u32 *data,
			    struct nvgpu_gmmu_pd **pd_out, u32 *pd_idx_out,
			    u32 *pd_offs_out)
{
	const struct gk20a_mmu_level *l;
	const struct gk20a_mmu_level *next_l;
	u32 pd_idx;
	bool done = false;

	do {
		l = &vm->mmu_levels[lvl];
		next_l = &vm->mmu_levels[nvgpu_safe_add_u32(lvl, 1)];
		pd_idx = pd_index(l, vaddr, attrs);
		/*
		 * If this isn't the final level (i.e there's a valid next level)
		 * then find the next level PD and recurse.
		 */
		if (next_l->update_entry != NULL) {
			struct nvgpu_gmmu_pd *pd_next;

			/* Not mapped yet, invalid entry */
			if (pd->entries == NULL) {
				return -EINVAL;
			}

			pd_next = pd->entries + pd_idx;

			/* Invalid entry! */
			if (pd_next->mem == NULL) {
				return -EINVAL;
			}

			attrs->pgsz = l->get_pgsz(g, l, pd, pd_idx);

			if (attrs->pgsz >= GMMU_NR_PAGE_SIZES) {
				return -EINVAL;
			}

			pd = pd_next;
			lvl = nvgpu_safe_add_u32(lvl, 1);
		} else {
			int err = nvgpu_locate_pte_last_level(g, pd, l, pd_out,
				pd_idx_out, pd_offs_out, data, pd_idx);
			if (err != 0) {
				return err;
			}

			done = true;
		}
	} while (!done);

	return 0;
}

int nvgpu_get_pte(struct gk20a *g, struct vm_gk20a *vm, u64 vaddr, u32 *pte)
{
	int err = 0;
	struct nvgpu_gmmu_attrs attrs = {
		.pgsz = 0,
	};

	err = nvgpu_locate_pte(g, vm, &vm->pdb,
				vaddr, 0U, &attrs,
				pte, NULL, NULL, NULL);
	if (err < 0) {
		nvgpu_err(g, "Failed!");
	}
	return err;
}

int nvgpu_set_pte(struct gk20a *g, struct vm_gk20a *vm, u64 vaddr, u32 *pte)
{
	struct nvgpu_gmmu_pd *pd = NULL;
	u32 pd_idx = 0;
	u32 pd_offs = 0;
	u32 pte_size, i;
	int err;
	struct nvgpu_gmmu_attrs attrs = {
		.pgsz = 0,
	};
#ifdef CONFIG_NVGPU_TRACE
	struct nvgpu_gmmu_attrs *attrs_ptr = &attrs;
#endif /* CONFIG_NVGPU_TRACE */

	err = nvgpu_locate_pte(g, vm, &vm->pdb,
			       vaddr, 0U, &attrs,
			       NULL, &pd, &pd_idx, &pd_offs);
	if (err != 0) {
		return err;
	}

	pte_size = nvgpu_pte_words(g);

	for (i = 0; i < pte_size; i++) {
		nvgpu_pd_write(g, pd, (size_t)pd_offs + (size_t)i, pte[i]);

#ifdef CONFIG_NVGPU_TRACE
		pte_dbg(g, attrs_ptr,
			"PTE: idx=%-4u (%d) 0x%08x", pd_idx, i, pte[i]);
#endif /* CONFIG_NVGPU_TRACE */
	}

	/*
	 * Ensures the pd_write()s are done. The pd_write() does not do this
	 * since generally there's lots of pd_write()s called one after another.
	 * There probably also needs to be a TLB invalidate as well but we leave
	 * that to the caller of this function.
	 */
	nvgpu_wmb();

	return 0;
}
