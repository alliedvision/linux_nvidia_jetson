/*
 * Copyright (c) 2018-2021, NVIDIA CORPORATION.  All rights reserved.
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

#include "page_table.h"
#include <unit/io.h>
#include <unit/core.h>
#include <unit/unit.h>
#include <unit/unit-requirement-ids.h>

#include <nvgpu/gk20a.h>
#include <nvgpu/types.h>
#include <nvgpu/sizes.h>
#include <nvgpu/gmmu.h>
#include <nvgpu/mm.h>
#include <nvgpu/vm.h>
#include <nvgpu/nvgpu_sgt.h>
#include <os/posix/os_posix.h>
#include <nvgpu/posix/posix-fault-injection.h>

#include "hal/mm/mmu_fault/mmu_fault_gv11b.h"
#include <hal/mm/mm_gv11b.h>
#include <hal/mm/cache/flush_gk20a.h>
#include <hal/mm/cache/flush_gv11b.h>
#include <hal/mm/gmmu/gmmu_gp10b.h>
#include <hal/mm/gmmu/gmmu_gv11b.h>
#include <hal/fb/fb_gp10b.h>
#include <hal/fb/fb_gm20b.h>
#include <hal/fifo/ramin_gk20a.h>
#include <hal/fifo/ramin_gv11b.h>

#include <nvgpu/hw/gv11b/hw_gmmu_gv11b.h>

#define TEST_PA_ADDRESS      0xEFAD80000000
#define TEST_GPU_VA          0x102040600000
#define TEST_PA_ADDRESS_64K  0x1FAD80010000
#define TEST_PA_ADDRESS_4K   0x2FAD80001000
#define TEST_HOLE_SIZE       0x100000
#define TEST_COMP_TAG        0xEF
#define TEST_INVALID_ADDRESS 0xAAC0000000
#define TEST_PTE_SIZE        2U

/* Size of the buffer to map. It must be a multiple of 4KB */
#define TEST_SIZE (1 * SZ_1M)
#define TEST_SIZE_64KB_PAGES 16

/* Some special failure cases */
#define SPECIAL_MAP_FAIL_FI_NULL_SGT		0
#define SPECIAL_MAP_FAIL_PD_ALLOCATE		1
#define SPECIAL_MAP_FAIL_PD_ALLOCATE_CHILD	2
#define SPECIAL_MAP_FAIL_TLB_INVALIDATE		3

/* Consts for requirements C1/C2 testing */
#define REQ_C1_NUM_MEMS		3
#define REQ_C1_IDX_64K_ALIGN	0
#define REQ_C1_IDX_4K_ALIGN	1
#define REQ_C1_IDX_MIXED	2

/* Check if address is aligned at the requested boundary */
#define IS_ALIGNED(addr, align)	((addr & (align - 1U)) == 0U)

struct test_parameters {
	enum nvgpu_aperture aperture;
	bool is_iommuable;
	bool is_sgt_iommuable;
	enum gk20a_mem_rw_flag rw_flag;
	u32 flags;
	bool priv;
	u32 page_size;
	u32 offset_pages;
	bool sparse;
	u32 ctag_offset;
	/* Below are flags for special cases, default to disabled */
	bool special_null_phys;
	bool special_map_fixed;
	bool special_sgl_skip;
	bool special_unmap_tbl_invalidate_fail;
};

static struct test_parameters test_iommu_sysmem = {
	.aperture = APERTURE_SYSMEM,
	.is_iommuable = true,
	.rw_flag = gk20a_mem_flag_none,
	.flags = NVGPU_VM_MAP_CACHEABLE,
	.priv = true,
};

static struct test_parameters test_iommu_sysmem_ro = {
	.aperture = APERTURE_SYSMEM,
	.is_iommuable = true,
	.rw_flag = gk20a_mem_flag_read_only,
	.flags = NVGPU_VM_MAP_CACHEABLE,
	.priv = true,
};

static struct test_parameters test_iommu_sysmem_ro_fixed = {
	.aperture = APERTURE_SYSMEM,
	.is_iommuable = true,
	.rw_flag = gk20a_mem_flag_read_only,
	.flags = NVGPU_VM_MAP_CACHEABLE,
	.priv = true,
	.special_map_fixed = true,
};

static struct test_parameters test_iommu_sysmem_coh = {
	.aperture = APERTURE_SYSMEM,
	.is_iommuable = true,
	.rw_flag = gk20a_mem_flag_none,
	.flags = NVGPU_VM_MAP_CACHEABLE | NVGPU_VM_MAP_IO_COHERENT,
	.priv = false,
};

static struct test_parameters test_no_iommu_sysmem = {
	.aperture = APERTURE_SYSMEM,
	.is_iommuable = false,
	.rw_flag = gk20a_mem_flag_none,
	.flags = NVGPU_VM_MAP_CACHEABLE,
	.priv = true,
};

static struct test_parameters test_iommu_sysmem_adv = {
	.aperture = APERTURE_SYSMEM,
	.is_iommuable = true,
	.rw_flag = gk20a_mem_flag_none,
	.flags = NVGPU_VM_MAP_CACHEABLE,
	.priv = true,
	.page_size = GMMU_PAGE_SIZE_KERNEL,
	.offset_pages = 0,
	.sparse = false,
};

static struct test_parameters test_iommu_sysmem_adv_ctag = {
	.aperture = APERTURE_SYSMEM,
	.is_iommuable = true,
	.rw_flag = gk20a_mem_flag_none,
	.flags = NVGPU_VM_MAP_CACHEABLE,
	.priv = true,
	.page_size = GMMU_PAGE_SIZE_KERNEL,
	.offset_pages = 10,
	.sparse = false,
	.ctag_offset = TEST_COMP_TAG,
};

static struct test_parameters test_iommu_sysmem_sgl_skip = {
	.aperture = APERTURE_SYSMEM,
	.is_iommuable = true,
	.rw_flag = gk20a_mem_flag_none,
	.flags = NVGPU_VM_MAP_CACHEABLE,
	.priv = true,
	.offset_pages = 32,
	.sparse = false,
	.special_sgl_skip = true,
};

static struct test_parameters test_iommu_sysmem_adv_big = {
	.aperture = APERTURE_SYSMEM,
	.is_iommuable = true,
	.rw_flag = gk20a_mem_flag_none,
	.flags = NVGPU_VM_MAP_CACHEABLE,
	.priv = true,
	.page_size = GMMU_PAGE_SIZE_BIG,
	.offset_pages = 0,
	.sparse = false,
};

static struct test_parameters test_iommu_sysmem_adv_big_offset = {
	.aperture = APERTURE_SYSMEM,
	.is_iommuable = true,
	.rw_flag = gk20a_mem_flag_none,
	.flags = NVGPU_VM_MAP_CACHEABLE,
	.priv = true,
	.page_size = GMMU_PAGE_SIZE_BIG,
	.offset_pages = 10,
	.sparse = false,
};

static struct test_parameters test_no_iommu_sysmem_adv_big_offset_large = {
	.aperture = APERTURE_SYSMEM,
	.is_iommuable = false,
	.rw_flag = gk20a_mem_flag_none,
	.flags = NVGPU_VM_MAP_CACHEABLE,
	.priv = true,
	.page_size = GMMU_PAGE_SIZE_BIG,
	.offset_pages = TEST_SIZE_64KB_PAGES+1,
	.sparse = false,
};

static struct test_parameters test_iommu_sysmem_adv_small_sparse = {
	.aperture = APERTURE_SYSMEM,
	.is_iommuable = true,
	.rw_flag = gk20a_mem_flag_none,
	.flags = NVGPU_VM_MAP_CACHEABLE,
	.priv = true,
	.page_size = GMMU_PAGE_SIZE_SMALL,
	.offset_pages = 0,
	.sparse = true,
	.special_null_phys = true,
};

static struct test_parameters test_unmap_invalidate_fail = {
	.aperture = APERTURE_SYSMEM,
	.is_iommuable = true,
	.rw_flag = gk20a_mem_flag_none,
	.flags = NVGPU_VM_MAP_CACHEABLE,
	.priv = true,
	.special_unmap_tbl_invalidate_fail = true,
};

#ifdef CONFIG_NVGPU_DGPU
static struct test_parameters test_no_iommu_vidmem = {
	.aperture = APERTURE_VIDMEM,
	.is_iommuable = false,
	.rw_flag = gk20a_mem_flag_none,
	.flags = NVGPU_VM_MAP_CACHEABLE,
	.priv = false,
};
#endif

static struct test_parameters test_no_iommu_sysmem_noncacheable = {
	.aperture = APERTURE_SYSMEM,
	.is_iommuable = false,
	.rw_flag = gk20a_mem_flag_none,
	.flags = 0,
	.priv = false,
};

static struct test_parameters test_no_iommu_unmapped = {
	.aperture = APERTURE_SYSMEM,
	.is_iommuable = false,
	.rw_flag = gk20a_mem_flag_none,
	.flags = NVGPU_VM_MAP_UNMAPPED_PTE,
	.priv = false,
};

static struct test_parameters test_sgt_iommu_sysmem = {
	.aperture = APERTURE_SYSMEM,
	.is_iommuable = true,
	.is_sgt_iommuable = true,
	.rw_flag = NVGPU_VM_MAP_CACHEABLE,
	.flags = 0,
	.priv = false,
};

/*
 * mvgpu_mem ops function used in the test_iommu_sysmem_sgl_skip test case.
 * It will return IPA=PA and a length that is always half the page offset.
 * This is used to test a corner case in __nvgpu_gmmu_do_update_page_table()
 */
static u64 nvgpu_mem_sgl_ipa_to_pa_by_half(struct gk20a *g,
		void *sgl, u64 ipa, u64 *pa_len)
{
	*pa_len = test_iommu_sysmem_sgl_skip.offset_pages * SZ_4K / 2;

	return nvgpu_mem_sgl_phys(g, sgl);
}

/* SGT ops for test_iommu_sysmem_sgl_skip test case */
static const struct nvgpu_sgt_ops nvgpu_sgt_posix_ops = {
	.sgl_next	= nvgpu_mem_sgl_next,
	.sgl_phys	= nvgpu_mem_sgl_phys,
	.sgl_ipa	= nvgpu_mem_sgl_phys,
	.sgl_ipa_to_pa	= nvgpu_mem_sgl_ipa_to_pa_by_half,
	.sgl_dma	= nvgpu_mem_sgl_dma,
	.sgl_length	= nvgpu_mem_sgl_length,
	.sgl_gpu_addr	= nvgpu_mem_sgl_gpu_addr,
	.sgt_iommuable	= nvgpu_mem_sgt_iommuable,
	.sgt_free	= nvgpu_mem_sgt_free,
};

/* Helper HAL function to make the g->ops.fb.tlb_invalidate op fail */
static int hal_fb_tlb_invalidate_fail(struct gk20a *g, struct nvgpu_mem *pdb)
{
	return -ETIMEDOUT;
}

static void init_platform(struct unit_module *m, struct gk20a *g, bool is_iGPU)
{
	if (is_iGPU) {
		nvgpu_set_enabled(g, NVGPU_MM_UNIFIED_MEMORY, true);
		/* Features below are mostly to cover corner cases */
		nvgpu_set_enabled(g, NVGPU_USE_COHERENT_SYSMEM, true);
		nvgpu_set_enabled(g, NVGPU_SUPPORT_NVLINK, true);
	} else {
		nvgpu_set_enabled(g, NVGPU_MM_UNIFIED_MEMORY, false);
	}
}

/*
 * Init the minimum set of HALs to run GMMU tests, then call the init_mm
 * base function.
 */
static int init_mm(struct unit_module *m, struct gk20a *g)
{
	u64 low_hole, aperture_size;
	struct nvgpu_os_posix *p = nvgpu_os_posix_from_gk20a(g);
	struct mm_gk20a *mm = &g->mm;

	p->mm_is_iommuable = true;

	g->ops.mm.gmmu.get_default_big_page_size =
		nvgpu_gmmu_default_big_page_size;
	g->ops.mm.gmmu.get_mmu_levels = gp10b_mm_get_mmu_levels;
	g->ops.mm.gmmu.get_max_page_table_levels = gp10b_get_max_page_table_levels;
	g->ops.mm.init_inst_block = gv11b_mm_init_inst_block;
	g->ops.mm.gmmu.map = nvgpu_gmmu_map_locked;
	g->ops.mm.gmmu.unmap = nvgpu_gmmu_unmap_locked;
	g->ops.mm.gmmu.get_iommu_bit = gp10b_mm_get_iommu_bit;
	g->ops.mm.gmmu.gpu_phys_addr = gv11b_gpu_phys_addr;
	g->ops.mm.is_bar1_supported = gv11b_mm_is_bar1_supported;
	g->ops.mm.cache.l2_flush = gv11b_mm_l2_flush;
	g->ops.mm.cache.fb_flush = gk20a_mm_fb_flush;
#ifdef CONFIG_NVGPU_COMPRESSION
	g->ops.fb.compression_page_size = gp10b_fb_compression_page_size;
#endif
	g->ops.fb.tlb_invalidate = gm20b_fb_tlb_invalidate;
	g->ops.ramin.init_pdb = gv11b_ramin_init_pdb;
	g->ops.ramin.alloc_size = gk20a_ramin_alloc_size;

	if (g->ops.mm.is_bar1_supported(g)) {
		unit_return_fail(m, "BAR1 is not supported on Volta+\n");
	}

	/*
	 * Initialize one VM space for system memory to be used throughout this
	 * unit module.
	 * Values below are similar to those used in nvgpu_init_system_vm()
	 */
	low_hole = SZ_4K * 16UL;
	aperture_size = GK20A_PMU_VA_SIZE;
	mm->pmu.aperture_size = GK20A_PMU_VA_SIZE;
	g->ops.mm.get_default_va_sizes(NULL, &mm->channel.user_size,
		&mm->channel.kernel_size);

	mm->bar1.aperture_size = bar1_aperture_size_mb_gk20a() << 20;
	mm->bar1.vm = nvgpu_vm_init(g,
			g->ops.mm.gmmu.get_default_big_page_size(),
			low_hole,
			0ULL,
			nvgpu_safe_sub_u64(mm->bar1.aperture_size, low_hole),
			0ULL,
			true, false, false,
			"bar1");
	if (mm->bar1.vm == NULL) {
		unit_return_fail(m, "nvgpu_vm_init failed\n");
	}

	mm->pmu.vm = nvgpu_vm_init(g,
				   g->ops.mm.gmmu.get_default_big_page_size(),
				   low_hole,
				   0ULL,
				   nvgpu_safe_sub_u64(aperture_size, low_hole),
				   0ULL,
				   true,
				   false,
				   false,
				   "system");
	if (mm->pmu.vm == NULL) {
		unit_return_fail(m, "nvgpu_vm_init failed\n");
	}

	return UNIT_SUCCESS;
}

int test_nvgpu_gmmu_init(struct unit_module *m, struct gk20a *g, void *args)
{
	int debug_level = verbose_lvl(m);

	g->log_mask = 0;
	if (debug_level >= 1) {
		g->log_mask = gpu_dbg_map;
	}
	if (debug_level >= 2) {
		g->log_mask |= gpu_dbg_map_v;
	}
	if (debug_level >= 3) {
		g->log_mask |= gpu_dbg_pte;
	}

	init_platform(m, g, true);

	if (nvgpu_pd_cache_init(g) != 0) {
		unit_return_fail(m, "PD cache initialization failed\n");
	}

	if (init_mm(m, g) != 0) {
		unit_return_fail(m, "nvgpu_init_mm_support failed\n");
	}

	return UNIT_SUCCESS;
}

int test_nvgpu_gmmu_clean(struct unit_module *m, struct gk20a *g, void *args)
{
	g->log_mask = 0;
	nvgpu_vm_put(g->mm.pmu.vm);
	nvgpu_vm_put(g->mm.bar1.vm);

	return UNIT_SUCCESS;
}


/*
 * Define a few helper functions to decode PTEs.
 * These function rely on functions imported from hw_gmmu_* header files. As a
 * result, when updating this unit test, you must ensure that the HAL functions
 * used to write PTEs are for the same chip as the gmmu_new_pte* functions used
 * below.
 */
static bool pte_is_valid(u32 *pte)
{
	return (pte[0] & gmmu_new_pte_valid_true_f());
}

static bool pte_is_read_only(u32 *pte)
{
	return (pte[0] & gmmu_new_pte_read_only_true_f());
}

static bool pte_is_rw(u32 *pte)
{
	return !(pte[0] & gmmu_new_pte_read_only_true_f());
}

static bool pte_is_priv(u32 *pte)
{
	return (pte[0] & gmmu_new_pte_privilege_true_f());
}

static bool pte_is_volatile(u32 *pte)
{
	return (pte[0] & gmmu_new_pte_vol_true_f());
}

static u64 pte_get_phys_addr(u32 *pte)
{
	u64 addr_bits = ((u64) (pte[1] & 0x00FFFFFF)) << 32;

	addr_bits |= (u64) (pte[0] & ~0xFF);
	addr_bits >>= 8;
	return (addr_bits << gmmu_new_pde_address_shift_v());
}

int test_nvgpu_gmmu_map_unmap(struct unit_module *m, struct gk20a *g,
	void *args)
{
	struct nvgpu_mem mem = { };
	u32 pte[TEST_PTE_SIZE];
	int result;
	struct nvgpu_os_posix *p = nvgpu_os_posix_from_gk20a(g);
	struct test_parameters *params = (struct test_parameters *) args;

	p->mm_is_iommuable = params->is_iommuable;
	p->mm_sgt_is_iommuable = params->is_sgt_iommuable;
	mem.size = TEST_SIZE;
	mem.cpu_va = (void *) TEST_PA_ADDRESS;

	if (params->special_map_fixed) {
		/* Special case: use a fixed address */
		mem.gpu_va = nvgpu_gmmu_map_fixed(g->mm.pmu.vm, &mem,
				TEST_PA_ADDRESS, mem.size,
				params->flags, params->rw_flag, params->priv,
				params->aperture);
	} else {
		mem.gpu_va = nvgpu_gmmu_map(g->mm.pmu.vm, &mem,
				params->flags, params->rw_flag, params->priv,
				params->aperture);
	}

	if (mem.gpu_va == 0) {
		unit_return_fail(m, "Failed to map GMMU page\n");
	}

	if (!IS_ALIGNED(mem.gpu_va, SZ_4K)) {
		unit_return_fail(m, "Mapped VA is not 4KB-aligned\n");
	}

	unit_info(m, "Mapped VA=%p", (void *) mem.gpu_va);

	/*
	 * Based on the VA returned from gmmu_map, lookup the corresponding
	 * PTE
	 */
	result = nvgpu_get_pte(g, g->mm.pmu.vm, mem.gpu_va, &pte[0]);
	if (result != 0) {
		unit_return_fail(m, "PTE lookup failed with code=%d\n", result);
	}
	unit_info(m, "Found PTE=%08x %08x", pte[1], pte[0]);

	/* Make sure PTE is valid */
	if (!pte_is_valid(pte) &&
		(!(params->flags & NVGPU_VM_MAP_UNMAPPED_PTE))) {
		unit_return_fail(m, "Unexpected invalid PTE\n");
	}

	/* Make sure PTE corresponds to the PA we wanted to map */
	if (pte_get_phys_addr(pte) != TEST_PA_ADDRESS) {
		unit_return_fail(m, "Unexpected physical address in PTE\n");
	}

	/* Check RO, WO, RW */
	switch (params->rw_flag) {
	case gk20a_mem_flag_none:
		if (!pte_is_rw(pte) &&
			(!(params->flags & NVGPU_VM_MAP_UNMAPPED_PTE))) {
			unit_return_fail(m, "PTE is not RW as expected.\n");
		}
		break;
	case gk20a_mem_flag_write_only:
		/* WO is not supported anymore in Pascal+ */
		break;
	case gk20a_mem_flag_read_only:
		if (!pte_is_read_only(pte)) {
			unit_return_fail(m, "PTE is not RO as expected.\n");
		}
		break;
	default:
		unit_return_fail(m, "Unexpected params->rw_flag value.\n");
		break;
	}

	/* Check privileged bit */
	if ((params->priv) && (!pte_is_priv(pte))) {
		unit_return_fail(m, "PTE is not PRIV as expected.\n");
	} else if (!(params->priv) && (pte_is_priv(pte))) {
		unit_return_fail(m, "PTE is PRIV when it should not.\n");
	}

	/* Check if cached */
	if ((params->flags & NVGPU_VM_MAP_CACHEABLE) && pte_is_volatile(pte)) {
		unit_return_fail(m, "PTE is not cacheable as expected.\n");
	} else if ((params->flags & NVGPU_VM_MAP_CACHEABLE) &&
			pte_is_volatile(pte)) {
		unit_return_fail(m, "PTE is not volatile as expected.\n");
	}

	/* Now unmap the buffer and make sure the PTE is now invalid */
	nvgpu_gmmu_unmap(g->mm.pmu.vm, &mem);

	result = nvgpu_get_pte(g, g->mm.pmu.vm, mem.gpu_va, &pte[0]);
	if (result != 0) {
		unit_return_fail(m, "PTE lookup failed with code=%d\n", result);
	}

	if (pte_is_valid(pte)) {
		unit_return_fail(m, "PTE still valid for unmapped memory\n");
	}

	return UNIT_SUCCESS;
}


int test_nvgpu_gmmu_map_unmap_map_fail(struct unit_module *m, struct gk20a *g,
					void *args)
{
	struct nvgpu_mem mem = { };
	struct nvgpu_posix_fault_inj *kmem_fi =
		nvgpu_kmem_get_fault_injection();
	struct nvgpu_os_posix *p = nvgpu_os_posix_from_gk20a(g);
	u64 scenario = (u64) args;

	p->mm_is_iommuable = true;
	mem.size = TEST_SIZE;
	mem.cpu_va = (void *) TEST_PA_ADDRESS;
	mem.priv.sgt = NULL;

	if (scenario == SPECIAL_MAP_FAIL_FI_NULL_SGT) {
		/* Special case: use fault injection to trigger a NULL SGT */
		nvgpu_posix_enable_fault_injection(kmem_fi, true, 0);
	}

	if (scenario == SPECIAL_MAP_FAIL_PD_ALLOCATE) {
		/*
		 * Special case: use fault injection to trigger a failure in
		 * pd_allocate(). It is the 3rd malloc.
		 */
		nvgpu_posix_enable_fault_injection(kmem_fi, true, 3);
	}

	if (scenario == SPECIAL_MAP_FAIL_PD_ALLOCATE_CHILD) {
		/*
		 * Special case: use fault injection to trigger a failure in
		 * pd_allocate_children(). It is the 3rd malloc (assuming the
		 * SPECIAL_MAP_FAIL_PD_ALLOCATE case ran first)
		 */
		nvgpu_posix_enable_fault_injection(kmem_fi, true, 3);
	}

	if (scenario == SPECIAL_MAP_FAIL_TLB_INVALIDATE) {
		g->ops.fb.tlb_invalidate = hal_fb_tlb_invalidate_fail;
	}

	mem.gpu_va = nvgpu_gmmu_map(g->mm.pmu.vm, &mem,
				NVGPU_VM_MAP_CACHEABLE, gk20a_mem_flag_none,
				true, APERTURE_SYSMEM);

	if (scenario == SPECIAL_MAP_FAIL_TLB_INVALIDATE) {
		/* Restore previous op */
		g->ops.fb.tlb_invalidate = gm20b_fb_tlb_invalidate;
	}

	nvgpu_posix_enable_fault_injection(kmem_fi, false, 0);

	if (mem.gpu_va != 0) {
		unit_return_fail(m, "map did not fail as expected\n");
	}

	return UNIT_SUCCESS;
}

int test_nvgpu_gmmu_init_page_table_fail(struct unit_module *m,
					struct gk20a *g, void *args)
{
	int err;
	struct nvgpu_posix_fault_inj *kmem_fi =
		nvgpu_kmem_get_fault_injection();

	nvgpu_posix_enable_fault_injection(kmem_fi, true, 0);
	err = nvgpu_gmmu_init_page_table(g->mm.pmu.vm);
	nvgpu_posix_enable_fault_injection(kmem_fi, false, 0);

	if (err == 0) {
		unit_return_fail(m,
			"nvgpu_gmmu_init_page_table didn't fail as expected\n");
	}

	return UNIT_SUCCESS;
}

int test_nvgpu_gmmu_set_pte(struct unit_module *m, struct gk20a *g, void *args)
{
	struct nvgpu_mem mem = { };
	u32 pte[TEST_PTE_SIZE];
	u32 pte_size;
	int result;
	struct nvgpu_os_posix *p = nvgpu_os_posix_from_gk20a(g);
	struct test_parameters *params = (struct test_parameters *) args;

	p->mm_is_iommuable = params->is_iommuable;
	mem.size = TEST_SIZE;
	mem.cpu_va = (void *) TEST_PA_ADDRESS;
	mem.gpu_va = nvgpu_gmmu_map(g->mm.pmu.vm, &mem,
			params->flags, params->rw_flag, params->priv,
			params->aperture);

	if (mem.gpu_va == 0) {
		unit_return_fail(m, "Failed to map GMMU page");
	}

	pte_size = nvgpu_pte_words(g);
	if (pte_size != TEST_PTE_SIZE) {
		unit_return_fail(m, "PTE size unexpected: %d/%d\n", pte_size,
			TEST_PTE_SIZE);
	}

	result = nvgpu_get_pte(g, g->mm.pmu.vm, mem.gpu_va, &pte[0]);
	if (result != 0) {
		unit_return_fail(m, "PTE lookup failed with code=%d\n", result);
	}

	/* Flip the valid bit of the PTE */
	pte[0] &= ~(gmmu_new_pte_valid_true_f());

	/* Test error case where the VA is not mapped */
	result = nvgpu_set_pte(g, g->mm.pmu.vm, TEST_INVALID_ADDRESS,
				&pte[0]);
	if (result == 0) {
		unit_return_fail(m, "Set PTE succeeded with invalid VA\n");
	}

	/* Now rewrite PTE of the already mapped page */
	result = nvgpu_set_pte(g, g->mm.pmu.vm, mem.gpu_va, &pte[0]);
	if (result != 0) {
		unit_return_fail(m, "Set PTE failed with code=%d\n", result);
	}

	result = nvgpu_get_pte(g, g->mm.pmu.vm, mem.gpu_va, &pte[0]);
	if (result != 0) {
		unit_return_fail(m, "PTE lookup failed with code=%d\n", result);
	}

	if (pte_is_valid(pte)) {
		unit_return_fail(m, "Unexpected valid PTE\n");
	}

	return UNIT_SUCCESS;
}

/*
 * Helper function used to create custom SGTs from a provided nvgpu_mem with
 * the option of providing a list of SGLs as well.
 * The created SGT needs to be explicitly freed once used.
 */
static struct nvgpu_sgt *custom_sgt_create(struct unit_module *m,
			struct gk20a *g, struct nvgpu_mem *mem,
			struct nvgpu_mem_sgl *sgl_list, u32 nr_sgls)
{
	struct nvgpu_sgt *sgt;

	if (sgl_list != NULL) {
		if (nvgpu_mem_posix_create_from_list(g, mem, sgl_list,
							nr_sgls) != 0) {
			unit_err(m, "Failed to create mem from SGL list\n");
			return 0;
		}
		sgt = nvgpu_sgt_create_from_mem(g, mem);
		if (sgt == NULL) {
			unit_err(m, "Failed to create SGT\n");
			return 0;
		}

		sgt->ops = &nvgpu_sgt_posix_ops;
	} else {
		sgt = nvgpu_sgt_create_from_mem(g, mem);

		if (sgt == NULL) {
			unit_err(m, "Failed to create SGT\n");
			return 0;
		}
	}

	return sgt;
}

/*
 * Helper function to wrap calls to g->ops.mm.gmmu.map and thus giving
 * access to more parameters
 */
static u64 gmmu_map_advanced(struct unit_module *m, struct gk20a *g,
	struct nvgpu_mem *mem, struct test_parameters *params,
	struct vm_gk20a_mapping_batch *batch, struct vm_gk20a *vm,
	struct nvgpu_sgt *sgt)
{
	u64 vaddr;

	struct nvgpu_os_posix *p = nvgpu_os_posix_from_gk20a(g);
	size_t offset = params->offset_pages *
		vm->gmmu_page_sizes[params->page_size];

	p->mm_is_iommuable = params->is_iommuable;

	if (params->sparse && params->special_null_phys) {
		mem->cpu_va = NULL;
	}

	if (nvgpu_is_enabled(g, NVGPU_USE_COHERENT_SYSMEM)) {
		params->flags |= NVGPU_VM_MAP_IO_COHERENT;
	}

	nvgpu_mutex_acquire(&vm->update_gmmu_lock);

	vaddr = g->ops.mm.gmmu.map(vm, (u64) mem->cpu_va,
				   sgt,
				   offset,
				   mem->size,
				   params->page_size,
				   0,      /* kind */
				   params->ctag_offset,
				   params->flags,
				   params->rw_flag,
				   false,  /* clear_ctags (unused) */
				   params->sparse,
				   params->priv,
				   batch,
				   params->aperture);
	nvgpu_mutex_release(&vm->update_gmmu_lock);

	return vaddr;
}

/*
 * Helper function to wrap calls to g->ops.mm.gmmu.unmap and thus giving
 * access to more parameters
 */
static void gmmu_unmap_advanced(struct vm_gk20a *vm, struct nvgpu_mem *mem,
	u64 gpu_va, struct test_parameters *params,
	struct vm_gk20a_mapping_batch *batch)
{
	struct gk20a *g = gk20a_from_vm(vm);

	nvgpu_mutex_acquire(&vm->update_gmmu_lock);

	g->ops.mm.gmmu.unmap(vm,
			     gpu_va,
			     mem->size,
			     params->page_size,
			     mem->free_gpu_va,
			     gk20a_mem_flag_none,
			     false,
			     batch);

	nvgpu_mutex_release(&vm->update_gmmu_lock);
}

int test_nvgpu_gmmu_map_unmap_adv(struct unit_module *m,
					struct gk20a *g, void *args)
{
	struct nvgpu_mem mem = { };
	u64 vaddr;
	u32 nr_sgls = 0;
	struct nvgpu_mem_sgl *sgl_list = NULL;
	struct nvgpu_sgt *sgt = NULL;

	struct test_parameters *params = (struct test_parameters *) args;

	mem.size = TEST_SIZE;
	mem.cpu_va = (void *) TEST_PA_ADDRESS;

	struct nvgpu_mem_sgl special_sgl_list[] = {
		{ .length = mem.size, .phys = (u64) mem.cpu_va, },
		{ .length = mem.size, .phys = ((u64) mem.cpu_va) +
			mem.size, },
	};


	if (params->special_sgl_skip) {
		nr_sgls = ARRAY_SIZE(special_sgl_list);
		sgl_list = special_sgl_list;
	}

	sgt = custom_sgt_create(m, g, &mem, sgl_list, nr_sgls);
	if (sgt == 0) {
		return UNIT_FAIL;
	}

	vaddr = gmmu_map_advanced(m, g, &mem, params, NULL, g->mm.pmu.vm, sgt);

	nvgpu_sgt_free(g, sgt);

	if (vaddr == 0ULL) {
		unit_return_fail(m, "Failed to map buffer\n");
	}

	if (!IS_ALIGNED(vaddr, SZ_4K)) {
		unit_return_fail(m, "Mapped VA is not 4KB-aligned\n");
	}

	if (params->special_unmap_tbl_invalidate_fail) {
		g->ops.fb.tlb_invalidate = hal_fb_tlb_invalidate_fail;
	}

	nvgpu_gmmu_unmap_addr(g->mm.pmu.vm, &mem, vaddr);

	if (params->special_unmap_tbl_invalidate_fail) {
		/* Restore previous op */
		g->ops.fb.tlb_invalidate = gm20b_fb_tlb_invalidate;
	}

	return UNIT_SUCCESS;
}

int test_nvgpu_gmmu_map_unmap_batched(struct unit_module *m,
					struct gk20a *g, void *args)
{
	struct nvgpu_mem mem = { }, mem2 = { };
	u64 vaddr, vaddr2;
	struct vm_gk20a_mapping_batch batch = { };
	struct nvgpu_sgt *sgt;

	struct test_parameters *params = (struct test_parameters *) args;

	mem.size = TEST_SIZE;
	mem.cpu_va = (void *) TEST_PA_ADDRESS;
	mem2.size = TEST_SIZE;
	mem2.cpu_va = (void *) (TEST_PA_ADDRESS + TEST_SIZE);

	sgt = custom_sgt_create(m, g, &mem, NULL, 0);
	if (sgt == 0) {
		return UNIT_FAIL;
	}
	vaddr = gmmu_map_advanced(m, g, &mem, params, &batch, g->mm.pmu.vm,
				  sgt);
	if (vaddr == 0ULL) {
		unit_return_fail(m, "Failed to map buffer\n");
	}
	nvgpu_sgt_free(g, sgt);

	sgt = custom_sgt_create(m, g, &mem2, NULL, 0);
	if (sgt == 0) {
		return UNIT_FAIL;
	}
	vaddr2 = gmmu_map_advanced(m, g, &mem2, params, &batch, g->mm.pmu.vm,
				   sgt);
	if (vaddr2 == 0ULL) {
		unit_return_fail(m, "Failed to map buffer 2\n");
	}
	nvgpu_sgt_free(g, sgt);

	if (!batch.need_tlb_invalidate) {
		unit_return_fail(m, "TLB invalidate flag not set.\n");
	}

	batch.need_tlb_invalidate = false;
	gmmu_unmap_advanced(g->mm.pmu.vm, &mem, vaddr, params, &batch);
	gmmu_unmap_advanced(g->mm.pmu.vm, &mem, vaddr2, params, &batch);

	if (!batch.need_tlb_invalidate) {
		unit_return_fail(m, "TLB invalidate flag not set.\n");
	}

	if (!batch.gpu_l2_flushed) {
		unit_return_fail(m, "GPU L2 not flushed.\n");
	}

	return UNIT_SUCCESS;
}

static int check_pte_valid(struct unit_module *m, struct gk20a *g,
			struct vm_gk20a *vm, struct nvgpu_mem *mem)
{
	u32 pte[TEST_PTE_SIZE];
	int result;

	result = nvgpu_get_pte(g, vm, mem->gpu_va, &pte[0]);
	if (result != 0) {
		unit_return_fail(m, "PTE lookup failed with code=%d\n", result);
	}
	unit_info(m, "Found PTE=%08x %08x", pte[1], pte[0]);

	/* Make sure PTE is valid */
	if (!pte_is_valid(pte)) {
		unit_return_fail(m, "Unexpected invalid PTE\n");
	}

	return 0;
}

static int check_pte_invalidated(struct unit_module *m, struct gk20a *g,
			struct vm_gk20a *vm, struct nvgpu_mem *mem)
{
	u32 pte[TEST_PTE_SIZE];
	int result;

	result = nvgpu_get_pte(g, vm, mem->gpu_va, &pte[0]);
	if (result != 0) {
		unit_return_fail(m, "PTE lookup failed with code=%d\n", result);
	}

	if (pte_is_valid(pte)) {
		unit_return_fail(m, "PTE still valid for unmapped memory\n");
	}

	return 0;
}

/* Create a VM based on requirements described in NVGPU-RQCD-45 */
static struct vm_gk20a *init_test_req_vm(struct gk20a *g)
{
	u64 low_hole, aperture_size, kernel_reserved, user_reserved;
	bool big_pages;

	/* Init some common attributes */
	struct nvgpu_os_posix *p = nvgpu_os_posix_from_gk20a(g);

	p->mm_is_iommuable = true;
	p->mm_sgt_is_iommuable = true;

	/* 1. The VM shall:
	 * 1.1. Support 64KB large pages */
	big_pages = true;
	/* 1.2. Have a low hole of 64KB */
	low_hole = SZ_64K;
	/* 1.3. Have a least 128GB of address space */
	aperture_size = 128 * SZ_1G;
	/* 1.4. Have a 4GB kernel reserved space */
	kernel_reserved = 4 * SZ_1G;
	/* 1.5. User reserved space */
	user_reserved = aperture_size - kernel_reserved - low_hole;

	return nvgpu_vm_init(g, g->ops.mm.gmmu.get_default_big_page_size(),
			     low_hole, user_reserved, kernel_reserved,
			     nvgpu_gmmu_va_small_page_limit(),
			     big_pages, true, true, "testmem");
}

int test_nvgpu_page_table_c1_full(struct unit_module *m, struct gk20a *g,
	void *args)
{
	u32 mem_i, i;
	struct nvgpu_mem mem[REQ_C1_NUM_MEMS] = {};
	struct nvgpu_sgt *mixed_sgt = NULL;
	u32 nr_sgls = 5;
	struct nvgpu_mem_sgl *mixed_sgl_list = (struct nvgpu_mem_sgl *)
		malloc(sizeof(struct nvgpu_mem_sgl) * nr_sgls);

	/* 1. Initialize a VM.*/
	struct vm_gk20a *vm = init_test_req_vm(g);

	if (vm == NULL) {
		unit_return_fail(m, "nvgpu_vm_init failed\n");
	}

	/* 2. Initialize several nvgpu_mem objects. Should cover: */
	/* 2.1. 64K min alignment */
	mem[REQ_C1_IDX_64K_ALIGN].size = TEST_SIZE;
	mem[REQ_C1_IDX_64K_ALIGN].cpu_va = (void *) TEST_PA_ADDRESS_64K;

	/* 2.2. 4K min alignment */
	mem[REQ_C1_IDX_4K_ALIGN].size = TEST_SIZE;
	mem[REQ_C1_IDX_4K_ALIGN].cpu_va = (void *) TEST_PA_ADDRESS_4K;

	/* 2.3. Multiple discontiguous chunks both 4K, 64K, and a mixture of
	 * 64KB */
	mem[REQ_C1_IDX_MIXED].size = TEST_SIZE;
	mem[REQ_C1_IDX_MIXED].cpu_va = (void *) TEST_PA_ADDRESS;

	mixed_sgl_list[0].length = SZ_64K;
	mixed_sgl_list[0].phys = (u64) mem[2].cpu_va;
	mixed_sgl_list[1].length = SZ_4K;
	mixed_sgl_list[1].phys = mixed_sgl_list[0].phys +
				    mixed_sgl_list[0].length + TEST_HOLE_SIZE;
	mixed_sgl_list[2].length = SZ_64K;
	mixed_sgl_list[2].phys = mixed_sgl_list[1].phys +
				    mixed_sgl_list[1].length + TEST_HOLE_SIZE;
	mixed_sgl_list[3].length = SZ_4K;
	mixed_sgl_list[3].phys = mixed_sgl_list[2].phys +
				    mixed_sgl_list[2].length + TEST_HOLE_SIZE;
	mixed_sgl_list[4].length = SZ_64K * 10;
	mixed_sgl_list[4].phys = mixed_sgl_list[3].phys +
				    mixed_sgl_list[3].length + TEST_HOLE_SIZE;
	for (i = 0; i < nr_sgls; i++) {
		mixed_sgl_list[i].dma = 0;
	}

	mixed_sgt = custom_sgt_create(m, g, &mem[2], mixed_sgl_list,
				nr_sgls);
	if (mixed_sgt == 0) {
		return UNIT_FAIL;
	}

	/* 3. For each of the above nvgpu_mem: */
	for (mem_i = 0; mem_i < REQ_C1_NUM_MEMS; mem_i++) {
		/* 3.1. Map the nvgpu_mem */
		if (mem_i == REQ_C1_IDX_MIXED) {
			mem[mem_i].gpu_va = gmmu_map_advanced(m, g, &mem[mem_i],
				&test_iommu_sysmem, NULL, vm, mixed_sgt);
		} else {
			mem[mem_i].gpu_va = nvgpu_gmmu_map(vm, &mem[mem_i],
				NVGPU_VM_MAP_CACHEABLE,
				gk20a_mem_flag_none, true, APERTURE_SYSMEM);
		}

		if (mem[mem_i].gpu_va == 0) {
			unit_return_fail(m, "Failed to map i=%d", mem_i);
		}

		if (!IS_ALIGNED(mem[mem_i].gpu_va, SZ_4K)) {
			unit_return_fail(m, "Mapped VA is not 4KB-aligned\n");
		}

		/*
		 * 3.2. Verify that the programmed page table attributes are
		 * correct
		 */
		if (check_pte_valid(m, g, vm, &mem[mem_i]) != 0) {
			return UNIT_FAIL;
		}

		/* 3.3. Free the mapping */
		nvgpu_gmmu_unmap(vm, &mem[mem_i]);

		/* 3.4. Verify that the mapping has been cleared */
		if (check_pte_invalidated(m, g, vm, &mem[mem_i]) != 0) {
			return UNIT_FAIL;
		}
	}

	/* 4. Free the VM */
	nvgpu_vm_put(vm);

	return UNIT_SUCCESS;
}

static int c2_fixed_allocation(struct unit_module *m, struct gk20a *g,
			struct vm_gk20a *vm, struct nvgpu_mem *mem_fixed)
{
	/* Map the nvgpu_mem with VA=PA */
	mem_fixed->gpu_va = nvgpu_gmmu_map_fixed(vm, mem_fixed, TEST_GPU_VA,
			mem_fixed->size, NVGPU_VM_MAP_CACHEABLE,
			gk20a_mem_flag_none, true, APERTURE_SYSMEM);

	if (mem_fixed->gpu_va == 0) {
		unit_return_fail(m, "Failed to map mem_fixed");
	}

	if (!IS_ALIGNED(mem_fixed->gpu_va, SZ_4K)) {
		unit_return_fail(m, "Mapped VA is not 4KB-aligned\n");
	}

	/* Verify that the programmed page table attributes are correct */
	if (check_pte_valid(m, g, vm, mem_fixed) != 0) {
		return UNIT_FAIL;
	}

	/* Check that the GPU VA matches the requested address */
	if (mem_fixed->gpu_va != TEST_GPU_VA) {
		unit_return_fail(m, "GPU VA != requested address");
	}

	/* Free the mapping */
	nvgpu_gmmu_unmap(vm, mem_fixed);

	/* Verify that the mapping has been cleared */
	if (check_pte_invalidated(m, g, vm, mem_fixed) != 0) {
		return UNIT_FAIL;
	}

	return UNIT_SUCCESS;
}

int test_nvgpu_page_table_c2_full(struct unit_module *m, struct gk20a *g,
	void *args)
{
	int ret;
	struct nvgpu_mem mem_fixed = {};

	/* Initialize a VM.*/
	struct vm_gk20a *vm = init_test_req_vm(g);

	if (vm == NULL) {
		unit_return_fail(m, "nvgpu_vm_init failed\n");
	}

	mem_fixed.size = TEST_SIZE;
	mem_fixed.cpu_va = (void *) TEST_PA_ADDRESS_64K;

	/*
	 * Perform a first allocation/check/de-allocation
	 */
	ret = c2_fixed_allocation(m, g, vm, &mem_fixed);
	if (ret != UNIT_SUCCESS) {
		return ret;
	}

	/*
	 * Repeat the same allocation to ensure it was properly cleared the
	 * first time
	 */
	ret = c2_fixed_allocation(m, g, vm, &mem_fixed);
	if (ret != UNIT_SUCCESS) {
		return ret;
	}

	/*
	 * Repeat the same allocation but with 4KB alignment to make sure page
	 * markers have been cleared properly during the previous allocations.
	 */
	mem_fixed.cpu_va = (void *) (TEST_PA_ADDRESS_64K + SZ_4K);
	ret = c2_fixed_allocation(m, g, vm, &mem_fixed);
	if (ret != UNIT_SUCCESS) {
		return ret;
	}

	/* Free the VM */
	nvgpu_vm_put(vm);

	return UNIT_SUCCESS;
}

int test_nvgpu_gmmu_perm_str(struct unit_module *m, struct gk20a *g, void *args)
{
	int ret = UNIT_FAIL;
	const char *str;

	str = nvgpu_gmmu_perm_str(gk20a_mem_flag_none);
	if (strcmp(str, "RW") != 0) {
		unit_return_fail(m, "nvgpu_gmmu_perm_str failed (1)\n");
	}

	str = nvgpu_gmmu_perm_str(gk20a_mem_flag_write_only);
	if (strcmp(str, "WO") != 0) {
		unit_return_fail(m, "nvgpu_gmmu_perm_str failed (2)\n");
	}

	str = nvgpu_gmmu_perm_str(gk20a_mem_flag_read_only);
	if (strcmp(str, "RO") != 0) {
		unit_return_fail(m, "nvgpu_gmmu_perm_str failed (3)\n");
	}

	str = nvgpu_gmmu_perm_str(0xFF);
	if (strcmp(str, "??") != 0) {
		unit_return_fail(m, "nvgpu_gmmu_perm_str failed (4)\n");
	}

	ret = UNIT_SUCCESS;


	return ret;
}

struct unit_module_test nvgpu_gmmu_tests[] = {
	UNIT_TEST(gmmu_init, test_nvgpu_gmmu_init, (void *) 1, 0),

	/*
	 * These 2 tests must run first in the order below to avoid caching
	 * issues */
	UNIT_TEST(map_fail_pd_allocate,
		test_nvgpu_gmmu_map_unmap_map_fail,
		(void *) SPECIAL_MAP_FAIL_PD_ALLOCATE,
		0),
	UNIT_TEST(map_fail_pd_allocate_child,
		test_nvgpu_gmmu_map_unmap_map_fail,
		(void *) SPECIAL_MAP_FAIL_PD_ALLOCATE_CHILD,
		0),

	UNIT_TEST(gmmu_map_unmap_iommu_sysmem, test_nvgpu_gmmu_map_unmap,
		(void *) &test_iommu_sysmem, 0),
	UNIT_TEST(gmmu_map_unmap_iommu_sysmem_ro, test_nvgpu_gmmu_map_unmap,
		(void *) &test_iommu_sysmem_ro, 0),
	UNIT_TEST(gmmu_map_unmap_iommu_sysmem_ro_f, test_nvgpu_gmmu_map_unmap,
		(void *) &test_iommu_sysmem_ro_fixed, 0),
	UNIT_TEST(gmmu_map_unmap_no_iommu_sysmem, test_nvgpu_gmmu_map_unmap,
		(void *) &test_no_iommu_sysmem, 0),
#ifdef CONFIG_NVGPU_DGPU
	UNIT_TEST(gmmu_map_unmap_vidmem, test_nvgpu_gmmu_map_unmap,
		(void *) &test_no_iommu_vidmem, 0),
#endif
	UNIT_TEST(gmmu_map_unmap_iommu_sysmem_coh, test_nvgpu_gmmu_map_unmap,
		(void *) &test_iommu_sysmem_coh, 0),
	UNIT_TEST(gmmu_set_pte, test_nvgpu_gmmu_set_pte,
		(void *) &test_iommu_sysmem, 0),
	UNIT_TEST(gmmu_map_unmap_iommu_sysmem_adv_kernel_pages,
		test_nvgpu_gmmu_map_unmap_adv,
		(void *) &test_iommu_sysmem_adv,
		0),
	UNIT_TEST(gmmu_map_unmap_iommu_sysmem_adv_big_pages,
		test_nvgpu_gmmu_map_unmap_adv,
		(void *) &test_iommu_sysmem_adv_big,
		0),
	UNIT_TEST(gmmu_map_unmap_iommu_sysmem_adv_big_pages_offset,
		test_nvgpu_gmmu_map_unmap_adv,
		(void *) &test_iommu_sysmem_adv_big_offset,
		0),
	UNIT_TEST(gmmu_map_unmap_no_iommu_sysmem_adv_big_pages_offset_large,
		test_nvgpu_gmmu_map_unmap_adv,
		(void *) &test_no_iommu_sysmem_adv_big_offset_large,
		0),
	UNIT_TEST(gmmu_map_unmap_iommu_sysmem_adv_small_pages_sparse,
		test_nvgpu_gmmu_map_unmap_adv,
		(void *) &test_iommu_sysmem_adv_small_sparse,
		0),
	UNIT_TEST(gmmu_map_unmap_no_iommu_sysmem_noncacheable,
		test_nvgpu_gmmu_map_unmap,
		(void *) &test_no_iommu_sysmem_noncacheable,
		0),
	UNIT_TEST(gmmu_map_unmap_sgt_iommu_sysmem,
		test_nvgpu_gmmu_map_unmap,
		(void *) &test_sgt_iommu_sysmem,
		0),
	UNIT_TEST(gmmu_map_unmap_iommu_sysmem_adv_ctag,
		test_nvgpu_gmmu_map_unmap_adv,
		(void *) &test_iommu_sysmem_adv_ctag,
		0),
	UNIT_TEST(gmmu_map_unmap_iommu_sysmem_adv_big_pages_batched,
		test_nvgpu_gmmu_map_unmap_batched,
		(void *) &test_iommu_sysmem_adv_big,
		0),
	UNIT_TEST(gmmu_map_unmap_unmapped, test_nvgpu_gmmu_map_unmap,
		(void *) &test_no_iommu_unmapped,
		0),
	UNIT_TEST(gmmu_map_unmap_iommu_sysmem_adv_sgl_skip,
		test_nvgpu_gmmu_map_unmap_adv,
		(void *) &test_iommu_sysmem_sgl_skip,
		0),
	UNIT_TEST(gmmu_map_unmap_tlb_invalidate_fail,
		test_nvgpu_gmmu_map_unmap_adv,
		(void *) &test_unmap_invalidate_fail,
		0),
	UNIT_TEST(map_fail_fi_null_sgt,
		test_nvgpu_gmmu_map_unmap_map_fail,
		(void *) SPECIAL_MAP_FAIL_FI_NULL_SGT,
		0),
	UNIT_TEST(map_fail_tlb_invalidate,
		test_nvgpu_gmmu_map_unmap_map_fail,
		(void *) SPECIAL_MAP_FAIL_TLB_INVALIDATE,
		0),
	UNIT_TEST(init_page_table_fail,
		test_nvgpu_gmmu_init_page_table_fail,
		NULL,
		0),

	/*
	 * Requirement verification tests.
	 */
	UNIT_TEST_REQ("NVGPU-RQCD-45.C1", PAGE_TABLE_REQ1_UID, "V4",
			req_multiple_alignments, test_nvgpu_page_table_c1_full,
			NULL, 0),
	UNIT_TEST_REQ("NVGPU-RQCD-45.C2", PAGE_TABLE_REQ1_UID, "V4",
			req_fixed_address, test_nvgpu_page_table_c2_full,
			NULL, 0),

	UNIT_TEST(gmmu_perm_str, test_nvgpu_gmmu_perm_str, NULL, 0),
	UNIT_TEST(gmmu_clean, test_nvgpu_gmmu_clean, NULL, 0),
};

UNIT_MODULE(page_table, nvgpu_gmmu_tests, UNIT_PRIO_NVGPU_TEST);
