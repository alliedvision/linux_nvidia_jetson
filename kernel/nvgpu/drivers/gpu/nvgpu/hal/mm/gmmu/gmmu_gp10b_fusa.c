/*
 * Copyright (c) 2019-2022, NVIDIA CORPORATION.  All rights reserved.
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

#include <nvgpu/gk20a.h>
#include <nvgpu/gmmu.h>
#include <nvgpu/static_analysis.h>
#include <nvgpu/string.h>

#include <nvgpu/hw/gp10b/hw_gmmu_gp10b.h>

#include "gmmu_gk20a.h"
#include "gmmu_gp10b.h"

/*
 * Compression support is provided for 64GB memory.
 * 36 bits (0 to 35) are required for addressing compression memory.
 * Use 36th bit to describe l3_alloc or iommu bit.
 */
#define GP10B_MM_IOMMU_BIT		36U

u32 gp10b_mm_get_iommu_bit(struct gk20a *g)
{
	(void)g;
	return GP10B_MM_IOMMU_BIT;
}

static void update_gmmu_pde3_locked(struct vm_gk20a *vm,
				    const struct gk20a_mmu_level *l,
				    struct nvgpu_gmmu_pd *pd,
				    u32 pd_idx,
				    u64 virt_addr,
				    u64 phys_addr,
				    struct nvgpu_gmmu_attrs *attrs)
{
	struct gk20a *g = gk20a_from_vm(vm);
	struct nvgpu_gmmu_pd *next_pd = &pd->entries[pd_idx];
	u32 pd_offset = nvgpu_pd_offset_from_index(l, pd_idx);
	u32 pde_v[2] = {0, 0};

	phys_addr >>= gmmu_new_pde_address_shift_v();

	pde_v[0] |= nvgpu_aperture_mask(g, next_pd->mem,
					gmmu_new_pde_aperture_sys_mem_ncoh_f(),
					gmmu_new_pde_aperture_sys_mem_coh_f(),
					gmmu_new_pde_aperture_video_memory_f());
	pde_v[0] |= gmmu_new_pde_address_sys_f(u64_lo32(phys_addr));
	pde_v[0] |= gmmu_new_pde_vol_true_f();
	pde_v[1] |= nvgpu_safe_cast_u64_to_u32(phys_addr >> 24);

	nvgpu_pd_write(g, pd, (size_t)nvgpu_safe_add_u32(pd_offset, 0U),
								pde_v[0]);
	nvgpu_pd_write(g, pd, (size_t)nvgpu_safe_add_u32(pd_offset, 1U),
								pde_v[1]);

	pte_dbg(g, attrs,
		"PDE: i=%-4u size=%-2u offs=%-4u pgsz: -- | "
		"GPU %#-12llx  phys %#-12llx "
		"[0x%08x, 0x%08x]",
		pd_idx, l->entry_size, pd_offset,
		virt_addr, phys_addr,
		pde_v[1], pde_v[0]);
}

static void update_gmmu_pde0_locked(struct vm_gk20a *vm,
				    const struct gk20a_mmu_level *l,
				    struct nvgpu_gmmu_pd *pd,
				    u32 pd_idx,
				    u64 virt_addr,
				    u64 phys_addr,
				    struct nvgpu_gmmu_attrs *attrs)
{
	struct gk20a *g = gk20a_from_vm(vm);
	struct nvgpu_gmmu_pd *next_pd = &pd->entries[pd_idx];
	bool small_valid, big_valid;
	u32 small_addr = 0, big_addr = 0;
	u32 pd_offset = nvgpu_pd_offset_from_index(l, pd_idx);
	u32 pde_v[4] = {0, 0, 0, 0};
	u64 tmp_addr;

	small_valid = attrs->pgsz == GMMU_PAGE_SIZE_SMALL;
	big_valid   = attrs->pgsz == GMMU_PAGE_SIZE_BIG;

	if (small_valid) {
		tmp_addr = phys_addr >> gmmu_new_dual_pde_address_shift_v();
		nvgpu_assert(u64_hi32(tmp_addr) == 0U);
		small_addr = (u32)tmp_addr;

		pde_v[2] |=
			gmmu_new_dual_pde_address_small_sys_f(small_addr);
		pde_v[2] |= nvgpu_aperture_mask(g, next_pd->mem,
			gmmu_new_dual_pde_aperture_small_sys_mem_ncoh_f(),
			gmmu_new_dual_pde_aperture_small_sys_mem_coh_f(),
			gmmu_new_dual_pde_aperture_small_video_memory_f());
		pde_v[2] |= gmmu_new_dual_pde_vol_small_true_f();
		pde_v[3] |= small_addr >> 24;
	}

	if (big_valid) {
		tmp_addr = phys_addr >> gmmu_new_dual_pde_address_big_shift_v();
		nvgpu_assert(u64_hi32(tmp_addr) == 0U);
		big_addr = (u32)tmp_addr;

		pde_v[0] |= gmmu_new_dual_pde_address_big_sys_f(big_addr);
		pde_v[0] |= gmmu_new_dual_pde_vol_big_true_f();
		pde_v[0] |= nvgpu_aperture_mask(g, next_pd->mem,
			gmmu_new_dual_pde_aperture_big_sys_mem_ncoh_f(),
			gmmu_new_dual_pde_aperture_big_sys_mem_coh_f(),
			gmmu_new_dual_pde_aperture_big_video_memory_f());
		pde_v[1] |= big_addr >> 28;
	}

	nvgpu_pd_write(g, pd, (size_t)nvgpu_safe_add_u32(pd_offset, 0U),
								pde_v[0]);
	nvgpu_pd_write(g, pd, (size_t)nvgpu_safe_add_u32(pd_offset, 1U),
								pde_v[1]);
	nvgpu_pd_write(g, pd, (size_t)nvgpu_safe_add_u32(pd_offset, 2U),
								pde_v[2]);
	nvgpu_pd_write(g, pd, (size_t)nvgpu_safe_add_u32(pd_offset, 3U),
								pde_v[3]);

	pte_dbg(g, attrs,
		"PDE: i=%-4u size=%-2u offs=%-4u pgsz: %c%c | "
		"GPU %#-12llx  phys %#-12llx "
		"[0x%08x, 0x%08x, 0x%08x, 0x%08x]",
		pd_idx, l->entry_size, pd_offset,
		small_valid ? 'S' : '-',
		big_valid ?   'B' : '-',
		virt_addr, phys_addr,
		pde_v[3], pde_v[2], pde_v[1], pde_v[0]);
}

static void update_pte(struct vm_gk20a *vm,
		       u32 *pte_w,
		       u64 phys_addr,
		       struct nvgpu_gmmu_attrs *attrs)
{
	struct gk20a *g = gk20a_from_vm(vm);
#ifdef CONFIG_NVGPU_COMPRESSION
	u64 ctag_granularity = g->ops.fb.compression_page_size(g);
	u32 page_size = vm->gmmu_page_sizes[attrs->pgsz];
#endif
	u32 pte_valid = attrs->valid ?
		gmmu_new_pte_valid_true_f() :
		gmmu_new_pte_valid_false_f();
	u64 phys_shifted = phys_addr >> gmmu_new_pte_address_shift_v();
	u32 pte_addr = (attrs->aperture == APERTURE_SYSMEM) ?
		gmmu_new_pte_address_sys_f(u64_lo32(phys_shifted)) :
		gmmu_new_pte_address_vid_f(u64_lo32(phys_shifted));
	u32 pte_tgt = nvgpu_gmmu_aperture_mask(g,
					attrs->aperture,
					attrs->platform_atomic,
					gmmu_new_pte_aperture_sys_mem_ncoh_f(),
					gmmu_new_pte_aperture_sys_mem_coh_f(),
					gmmu_new_pte_aperture_video_memory_f());
	u64 tmp_addr;

	pte_w[0] = pte_valid | pte_addr | pte_tgt;

	if (attrs->priv) {
		pte_w[0] |= gmmu_new_pte_privilege_true_f();
	}

	tmp_addr = phys_addr >> (24U + gmmu_new_pte_address_shift_v());
	nvgpu_assert(u64_hi32(tmp_addr) == 0U);
	pte_w[1] = (u32)tmp_addr |
		gmmu_new_pte_kind_f(attrs->kind_v);

#ifdef CONFIG_NVGPU_COMPRESSION
	pte_w[1] |= gmmu_new_pte_comptagline_f(
		    nvgpu_safe_cast_u64_to_u32(attrs->ctag / ctag_granularity));

	if (attrs->ctag != 0ULL) {
		attrs->ctag += page_size;
	}
#endif

	if (attrs->rw_flag == gk20a_mem_flag_read_only) {
		pte_w[0] |= gmmu_new_pte_read_only_true_f();
	}

	if (!attrs->valid && !attrs->cacheable) {
		pte_w[0] |= gmmu_new_pte_read_only_true_f();
	} else {
		if (!attrs->cacheable) {
			pte_w[0] |= gmmu_new_pte_vol_true_f();
		}
	}
}

static void update_pte_sparse(u32 *pte_w)
{
	pte_w[0] = gmmu_new_pte_valid_false_f();
	pte_w[0] |= gmmu_new_pte_vol_true_f();
}

static void update_gmmu_pte_locked(struct vm_gk20a *vm,
				   const struct gk20a_mmu_level *l,
				   struct nvgpu_gmmu_pd *pd,
				   u32 pd_idx,
				   u64 virt_addr,
				   u64 phys_addr,
				   struct nvgpu_gmmu_attrs *attrs)
{
	struct gk20a *g = vm->mm->g;
	u32 page_size  = vm->gmmu_page_sizes[attrs->pgsz];
	u32 pd_offset = nvgpu_pd_offset_from_index(l, pd_idx);
	u32 pte_w[2] = {0, 0};

	if (phys_addr != 0ULL) {
		update_pte(vm, pte_w, phys_addr, attrs);
	} else {
		if (attrs->sparse) {
			update_pte_sparse(pte_w);
		}
	}

	nvgpu_pte_dbg_print(g, attrs, vm->name, pd_idx, l->entry_size,
		virt_addr, phys_addr, page_size, pte_w);

	nvgpu_pd_write(g, pd, (size_t)nvgpu_safe_add_u32(pd_offset, 0U),
								pte_w[0]);
	nvgpu_pd_write(g, pd, (size_t)nvgpu_safe_add_u32(pd_offset, 1U),
								pte_w[1]);
}

#define GP10B_PDE0_ENTRY_SIZE 16U

/*
 * Calculate the pgsz of the pde level
 * Pascal+ implements a 5 level page table structure with only the last
 * level having a different number of entries depending on whether it holds
 * big pages or small pages.
 */
static u32 gp10b_get_pde0_pgsz(struct gk20a *g, const struct gk20a_mmu_level *l,
				struct nvgpu_gmmu_pd *pd, u32 pd_idx)
{
	u32 pde_base = pd->mem_offs / (u32)sizeof(u32);
	u32 pde_offset = nvgpu_safe_add_u32(pde_base,
					nvgpu_pd_offset_from_index(l, pd_idx));
	u32 pde_v[GP10B_PDE0_ENTRY_SIZE >> 2];
	u32 idx;
	u32 pgsz = GMMU_NR_PAGE_SIZES;

	if (pd->mem == NULL) {
		return pgsz;
	}

	for (idx = 0; idx < (GP10B_PDE0_ENTRY_SIZE >> 2); idx++) {
		pde_v[idx] =
			nvgpu_mem_rd32(g, pd->mem, (u64)pde_offset + (u64)idx);
	}

	/*
	 * Check if the aperture AND address are set
	 */
	if ((pde_v[2] &
		(gmmu_new_dual_pde_aperture_small_sys_mem_ncoh_f() |
		 gmmu_new_dual_pde_aperture_small_sys_mem_coh_f() |
		 gmmu_new_dual_pde_aperture_small_video_memory_f())) != 0U) {
		u32 new_pde_addr_small_sys =
			gmmu_new_dual_pde_address_small_sys_f(~U32(0U));
		u64 addr = ((U64(pde_v[3]) << U64(32)) |
			(U64(pde_v[2]) & U64(new_pde_addr_small_sys))) <<
			U64(gmmu_new_dual_pde_address_shift_v());

		if (addr != 0ULL) {
			pgsz = GMMU_PAGE_SIZE_SMALL;
		}
	}

	if ((pde_v[0] &
		(gmmu_new_dual_pde_aperture_big_sys_mem_ncoh_f() |
		 gmmu_new_dual_pde_aperture_big_sys_mem_coh_f() |
		 gmmu_new_dual_pde_aperture_big_video_memory_f())) != 0U) {
		u32 new_pde_addr_big_sys =
				gmmu_new_dual_pde_address_big_sys_f(~U32(0U));
		u64 addr = ((U64(pde_v[1]) << U64(32)) |
			(U64(pde_v[0]) & U64(new_pde_addr_big_sys))) <<
			U64(gmmu_new_dual_pde_address_big_shift_v());

		if (addr != 0ULL) {
			/*
			 * If small is set that means that somehow MM allowed
			 * both small and big to be set, the PDE is not valid
			 * and may be corrupted
			 */
			if (pgsz == GMMU_PAGE_SIZE_SMALL) {
				nvgpu_err(g,
					"both small and big apertures enabled");
				return GMMU_NR_PAGE_SIZES;
			}
			pgsz = GMMU_PAGE_SIZE_BIG;
		}
	}

	return pgsz;
}

static const struct gk20a_mmu_level gp10b_mm_levels[] = {
	{.hi_bit = {48, 48},
	 .lo_bit = {47, 47},
	 .update_entry = update_gmmu_pde3_locked,
	 .entry_size = 8,
	 .get_pgsz = gk20a_get_pde_pgsz},
	{.hi_bit = {46, 46},
	 .lo_bit = {38, 38},
	 .update_entry = update_gmmu_pde3_locked,
	 .entry_size = 8,
	 .get_pgsz = gk20a_get_pde_pgsz},
	{.hi_bit = {37, 37},
	 .lo_bit = {29, 29},
	 .update_entry = update_gmmu_pde3_locked,
	 .entry_size = 8,
	 .get_pgsz = gk20a_get_pde_pgsz},
	{.hi_bit = {28, 28},
	 .lo_bit = {21, 21},
	 .update_entry = update_gmmu_pde0_locked,
	 .entry_size = GP10B_PDE0_ENTRY_SIZE,
	 .get_pgsz = gp10b_get_pde0_pgsz},
	{.hi_bit = {20, 20},
	 .lo_bit = {12, 16},
	 .update_entry = update_gmmu_pte_locked,
	 .entry_size = 8,
	 .get_pgsz = gk20a_get_pte_pgsz},
	{.update_entry = NULL}
};

const struct gk20a_mmu_level *gp10b_mm_get_mmu_levels(struct gk20a *g,
	u64 big_page_size)
{
	(void)g;
	(void)big_page_size;
	return gp10b_mm_levels;
}

u32 gp10b_get_max_page_table_levels(struct gk20a *g)
{
	(void)g;
	return 5U;
}
