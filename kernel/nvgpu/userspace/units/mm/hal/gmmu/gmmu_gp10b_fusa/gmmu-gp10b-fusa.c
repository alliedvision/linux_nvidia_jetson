/*
 * Copyright (c) 2018-2020, NVIDIA CORPORATION.  All rights reserved.
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

#include <unit/io.h>
#include <unit/unit.h>

#include <nvgpu/io.h>
#include <nvgpu/posix/io.h>

#include <nvgpu/gk20a.h>
#include <nvgpu/types.h>
#include <nvgpu/vm.h>
#include <nvgpu/nvgpu_mem.h>
#include <nvgpu/gmmu.h>
#include <nvgpu/hw/gv11b/hw_gmmu_gv11b.h>

#include "hal/mm/gmmu/gmmu_gp10b.h"

#include "gmmu-gp10b-fusa.h"

static u32 max_page_table_levels;
static const struct gk20a_mmu_level *mmu_level;

int test_gp10b_mm_get_default_big_page_size(struct unit_module *m,
						struct gk20a *g, void *args)
{
	u32 ret_pgsz;
	int ret = UNIT_FAIL;

	ret_pgsz = nvgpu_gmmu_default_big_page_size();
	unit_assert(ret_pgsz == U32(SZ_64K), goto done);

	ret = UNIT_SUCCESS;

done:
	if (ret != UNIT_SUCCESS) {
		unit_err(m, "%s: big page size != 64K as expected\n", __func__);
	}

	return ret;
}

int test_gp10b_mm_get_iommu_bit(struct unit_module *m,
						struct gk20a *g, void *args)
{
	u32 ret_bit;
	int ret = UNIT_FAIL;

	ret_bit = gp10b_mm_get_iommu_bit(g);
	unit_assert(ret_bit == 36U, goto done);

	ret = UNIT_SUCCESS;

done:
	if (ret != UNIT_SUCCESS) {
		unit_err(m, "%s: iommu bit != 36 as expected\n", __func__);
	}

	return ret;
}

int test_gp10b_get_max_page_table_levels(struct unit_module *m,
						struct gk20a *g, void *args)
{
	int ret = UNIT_FAIL;

	max_page_table_levels = gp10b_get_max_page_table_levels(g);
	unit_assert(max_page_table_levels == 5U, goto done);

	ret = UNIT_SUCCESS;

done:
	if (ret != UNIT_SUCCESS) {
		unit_err(m, "%s: max page table levels != 5 as expected\n",
								__func__);
	}

	return ret;
}

int test_gp10b_mm_get_mmu_levels(struct unit_module *m,
						struct gk20a *g, void *args)
{
	int ret = UNIT_FAIL;
	const struct gk20a_mmu_level *l;
	u32 i;

	l = gp10b_mm_get_mmu_levels(g, SZ_64K);

	for (i = 0; i < max_page_table_levels; i++) {
		unit_assert((l->update_entry != NULL), goto done);
		l++;
	}
	unit_assert(l->update_entry == NULL, goto done);

	/* If get mmu_levels is successful, copy mmu_levels for future use */
	mmu_level = gp10b_mm_get_mmu_levels(g, SZ_64K);

	ret = UNIT_SUCCESS;

done:
	if (ret != UNIT_SUCCESS) {
		unit_err(m, "%s: max page table levels != 5 as expected\n",
								__func__);
	}

	return ret;
}

int test_update_gmmu_pde3_locked(struct unit_module *m,
						struct gk20a *g, void *args)
{
	int ret = UNIT_FAIL;
	int err;
	struct vm_gk20a vm;
	struct nvgpu_gmmu_pd pd;
	struct nvgpu_gmmu_attrs attrs;
	const struct gk20a_mmu_level *l = mmu_level;
	u64 vaddr, size = SZ_4K;
	u32 data = 0U;
	u32 *data_ptr = NULL;

	unit_assert(l != NULL, goto done);

	unit_assert(g->mm.pd_cache == NULL, goto done);

	vm.mm = &g->mm;
	vm.mm->g = g;
	err = nvgpu_pd_cache_init(g);
	unit_assert(err == 0, goto done);

	err = nvgpu_pd_alloc(&vm, &pd, size);
	unit_assert(err == 0, goto done);

	vaddr = nvgpu_pd_gpu_addr(g, &pd);
	unit_assert(vaddr != 0ULL, goto done);

	pd.entries = (struct nvgpu_gmmu_pd *) nvgpu_kzalloc(g,
						sizeof(struct nvgpu_gmmu_pd));
	unit_assert(pd.entries != NULL, goto done);

	pd.entries->mem = (struct nvgpu_mem *) nvgpu_kzalloc(g,
						sizeof(struct nvgpu_mem));
	unit_assert(pd.entries->mem != NULL, goto done);

	nvgpu_set_enabled(g, NVGPU_MM_HONORS_APERTURE, true);
	pd.entries->mem->aperture = APERTURE_SYSMEM;

	l[0].update_entry(&vm, l, &pd, 0U, vaddr, size, &attrs);

	/* Compute data written to pd->mem */
	/* pd.entries->mem is SYSMEM with HONORS_APERTURE */
	data_ptr = pd.mem->cpu_va;

	data |= gmmu_new_pde_aperture_sys_mem_ncoh_f();
	data |= gmmu_new_pde_address_sys_f(size >>
						gmmu_new_pde_address_shift_v());
	data |= gmmu_new_pde_vol_true_f();

	unit_assert(data == data_ptr[0], goto done);

	ret = UNIT_SUCCESS;

done:
	if (ret != UNIT_SUCCESS) {
		unit_err(m, "%s: failed\n", __func__);
	}
	if (pd.entries->mem != NULL) {
		nvgpu_kfree(g, pd.entries->mem);
	}
	if (pd.entries != NULL) {
		nvgpu_kfree(g, pd.entries);
	}
	nvgpu_set_enabled(g, NVGPU_MM_HONORS_APERTURE, false);
	nvgpu_pd_free(&vm, &pd);
	nvgpu_pd_cache_fini(g);
	return ret;
}

#define F_UPDATE_GMMU_PDE0_SMALL_PAGE		0ULL
#define F_UPDATE_GMMU_PDE0_BIG_PAGE		1ULL

static const char *f_gmmu_pde0_locked[] = {
	"gmmu_small_page_size",
	"gmmu_big_page_size",
};

int test_update_gmmu_pde0_locked(struct unit_module *m,
						struct gk20a *g, void *args)
{
	int ret = UNIT_FAIL;
	int err;
	u64 branch = (u64)args;
	struct vm_gk20a vm;
	struct nvgpu_gmmu_pd pd;
	struct nvgpu_gmmu_attrs attrs;
	const struct gk20a_mmu_level *l = mmu_level;
	u64 vaddr, size = SZ_4K;
	u32 data = 0U;
	u32 *data_ptr = NULL;

	unit_assert(l != NULL, goto done);

	unit_assert(g->mm.pd_cache == NULL, goto done);

	vm.mm = &g->mm;
	err = nvgpu_pd_cache_init(g);
	unit_assert(err == 0, goto done);

	err = nvgpu_pd_alloc(&vm, &pd, size);
	unit_assert(err == 0, goto done);

	vaddr = nvgpu_pd_gpu_addr(g, &pd);
	unit_assert(vaddr != 0ULL, goto done);

	pd.entries = (struct nvgpu_gmmu_pd *) nvgpu_kzalloc(g,
						sizeof(struct nvgpu_gmmu_pd));
	unit_assert(pd.entries != NULL, goto done);

	pd.entries->mem = (struct nvgpu_mem *) nvgpu_kzalloc(g,
						sizeof(struct nvgpu_mem));
	unit_assert(pd.entries->mem != NULL, goto done);

	nvgpu_set_enabled(g, NVGPU_MM_HONORS_APERTURE, true);
	pd.entries->mem->aperture = APERTURE_SYSMEM;
	attrs.pgsz = branch == F_UPDATE_GMMU_PDE0_SMALL_PAGE ?
			GMMU_PAGE_SIZE_SMALL : GMMU_PAGE_SIZE_BIG;

	l[3].update_entry(&vm, l, &pd, 0U, vaddr, size, &attrs);

	/* Compute data written to pd->mem */
	/* pd.entries->mem is SYSMEM with HONORS_APERTURE */
	data_ptr = pd.mem->cpu_va;

	if (branch == F_UPDATE_GMMU_PDE0_SMALL_PAGE) {
		data |= gmmu_new_dual_pde_aperture_small_sys_mem_ncoh_f();
		data |= gmmu_new_dual_pde_address_small_sys_f(size >>
					gmmu_new_dual_pde_address_shift_v());
		data |= gmmu_new_dual_pde_vol_small_true_f();

		unit_assert(data == data_ptr[2], goto done);
	} else {
		data |= gmmu_new_dual_pde_aperture_big_sys_mem_ncoh_f();
		data |= gmmu_new_dual_pde_address_big_sys_f(size >>
				gmmu_new_dual_pde_address_big_shift_v());
		data |= gmmu_new_dual_pde_vol_big_true_f();

		unit_assert(data == data_ptr[0], goto done);
	}

	ret = UNIT_SUCCESS;

done:
	if (ret != UNIT_SUCCESS) {
		unit_err(m, "%s: %s failed\n", __func__,
						f_gmmu_pde0_locked[branch]);
	}
	if (pd.entries->mem != NULL) {
		nvgpu_kfree(g, pd.entries->mem);
	}
	if (pd.entries != NULL) {
		nvgpu_kfree(g, pd.entries);
	}
	nvgpu_set_enabled(g, NVGPU_MM_HONORS_APERTURE, false);
	nvgpu_pd_free(&vm, &pd);
	nvgpu_pd_cache_fini(g);
	return ret;
}

#define F_ATTRS_PRIV			0x1ULL
#define F_ATTRS_READ_ONLY		0x2ULL
#define F_ATTRS_VALID			0x4ULL
#define F_ATTRS_CACHEABLE		0x8ULL
#define F_ATTRS_APERTURE_VIDMEM		0x10ULL
#define F_PLATFORM_ATOMIC		0x20ULL
#define F_UPDATE_PTE			0x40ULL
#define F_UPDATE_PTE_SPARSE		0x80ULL

#define F_UPDATE_PTE_PHYS_ADDR_ZERO		0x00ULL
/* F_UPDATE_PTE */
#define F_UPDATE_PTE_DEFAULT			0x40ULL
/* F_UPDATE_PTE | F_ATTRS_PRIV | F_ATTRS_READ_ONLY */
#define F_UPDATE_PTE_ATTRS_PRIV_READ_ONLY	0x43ULL
/* F_UPDATE_PTE | F_ATTRS_VALID */
#define F_UPDATE_PTE_ATTRS_VALID		0x44ULL
/* F_UPDATE_PTE | F_ATTRS_CACHEABLE */
#define F_UPDATE_PTE_ATTRS_CACHEABLE		0x48ULL
/* F_UPDATE_PTE | F_ATTRS_APERTURE_VIDMEM */
#define F_UPDATE_PTE_ATTRS_VIDMEM		0x50ULL
/* F_UPDATE_PTE | F_PLATFORM_ATOMIC */
#define F_UPDATE_PTE_PLATFORM_ATOMIC		0x60ULL

static const char *f_gmmu_pte_locked[] = {
	[F_UPDATE_PTE_DEFAULT] = "update_pte_default",
	[F_UPDATE_PTE_ATTRS_PRIV_READ_ONLY] = "update_pte_attrs_priv_read_only",
	[F_UPDATE_PTE_ATTRS_VALID] = "update_pte_attrs_valid",
	[F_UPDATE_PTE_ATTRS_CACHEABLE] = "update_pte_attrs_cacheable",
	[F_UPDATE_PTE_ATTRS_VIDMEM] = "update_pte_attrs_vidmem",
	[F_UPDATE_PTE_PLATFORM_ATOMIC] = "update_pte_platform_atomic",
	[F_UPDATE_PTE_SPARSE] = "update_pte_sparse",
};

int test_update_gmmu_pte_locked(struct unit_module *m,
						struct gk20a *g, void *args)
{
	int ret = UNIT_FAIL;
	int err;
	u64 branch = (u64)args;
	struct vm_gk20a vm;
	struct nvgpu_gmmu_pd pd;
	struct nvgpu_gmmu_attrs attrs = {0};
	const struct gk20a_mmu_level *l = mmu_level;
	u64 vaddr, size = SZ_4K, paddr = 0;
	u32 data = 0U;
	u32 *data_ptr = NULL;

	unit_assert(l != NULL, goto done);

	unit_assert(g->mm.pd_cache == NULL, goto done);

	vm.mm = &g->mm;
	err = nvgpu_pd_cache_init(g);
	unit_assert(err == 0, goto done);

	err = nvgpu_pd_alloc(&vm, &pd, size);
	unit_assert(err == 0, goto done);

	vaddr = nvgpu_pd_gpu_addr(g, &pd);
	unit_assert(vaddr != 0ULL, goto done);

	pd.entries = (struct nvgpu_gmmu_pd *) nvgpu_kzalloc(g,
						sizeof(struct nvgpu_gmmu_pd));
	unit_assert(pd.entries != NULL, goto done);

	pd.entries->mem = (struct nvgpu_mem *) nvgpu_kzalloc(g,
						sizeof(struct nvgpu_mem));
	unit_assert(pd.entries->mem != NULL, goto done);

	nvgpu_set_enabled(g, NVGPU_MM_HONORS_APERTURE, true);
	pd.entries->mem->aperture = APERTURE_SYSMEM;
	attrs.pgsz = GMMU_PAGE_SIZE_SMALL;
	vm.gmmu_page_sizes[GMMU_PAGE_SIZE_SMALL] = SZ_4K;

	paddr = branch & F_UPDATE_PTE ? size : 0ULL;
	nvgpu_set_enabled(g, NVGPU_SUPPORT_PLATFORM_ATOMIC,
				(branch & F_PLATFORM_ATOMIC ? true : false));

	attrs.platform_atomic = branch & F_PLATFORM_ATOMIC ? true : false;
	attrs.aperture = branch & F_ATTRS_APERTURE_VIDMEM ?
				APERTURE_VIDMEM : APERTURE_SYSMEM;
	attrs.priv = branch & F_ATTRS_PRIV ? true : false;
	attrs.rw_flag = branch & F_ATTRS_READ_ONLY ?
			gk20a_mem_flag_read_only : gk20a_mem_flag_none;
	attrs.valid = branch & F_ATTRS_VALID ? true : false;
	attrs.cacheable = branch & F_ATTRS_CACHEABLE ? true : false;
	attrs.sparse = branch & F_UPDATE_PTE_SPARSE ? true : false;

	l[4].update_entry(&vm, l, &pd, 0U, vaddr, paddr, &attrs);

	/* Compute data written to pd->mem */
	/* pd.entries->mem is SYSMEM with HONORS_APERTURE */
	data_ptr = pd.mem->cpu_va;

	if (branch & F_UPDATE_PTE) {
		data |= branch & F_ATTRS_APERTURE_VIDMEM ?
			gmmu_new_pte_address_vid_f(paddr >>
					gmmu_new_pte_address_shift_v()) :
			gmmu_new_pte_address_sys_f(paddr >>
					gmmu_new_pte_address_shift_v());
		data |= branch & F_PLATFORM_ATOMIC ?
			gmmu_new_pte_aperture_sys_mem_coh_f() :
			branch & F_ATTRS_APERTURE_VIDMEM ?
				gmmu_new_pte_aperture_video_memory_f() :
				gmmu_new_pte_aperture_sys_mem_ncoh_f();
		data |= branch & F_ATTRS_VALID ? gmmu_new_pte_valid_true_f() :
						gmmu_new_pte_valid_false_f();
		data |= branch & F_ATTRS_PRIV ?
					gmmu_new_pte_privilege_true_f() : 0U;
		data |= branch & F_ATTRS_READ_ONLY ?
					gmmu_new_pte_read_only_true_f() : 0U;

		if (!(branch & F_ATTRS_CACHEABLE)) {
			data |= branch & F_ATTRS_VALID ?
						gmmu_new_pte_vol_true_f() :
						gmmu_new_pte_read_only_true_f();
		}
	} else if (branch & F_UPDATE_PTE_SPARSE) {
		data = gmmu_new_pte_valid_false_f();
		data |= gmmu_new_pte_vol_true_f();
	}

	unit_assert(data == data_ptr[0], goto done);

	ret = UNIT_SUCCESS;

done:
	if (ret != UNIT_SUCCESS) {
		unit_err(m, "%s: %s failed\n", __func__,
						f_gmmu_pte_locked[branch]);
	}
	if (pd.entries->mem != NULL) {
		nvgpu_kfree(g, pd.entries->mem);
	}
	if (pd.entries != NULL) {
		nvgpu_kfree(g, pd.entries);
	}
	nvgpu_set_enabled(g, NVGPU_MM_HONORS_APERTURE, false);
	nvgpu_pd_free(&vm, &pd);
	nvgpu_pd_cache_fini(g);
	return ret;
}

#define F_PDE_V0_VALUE_SET			0x1ULL
#define F_PDE_V1_VALUE_SET			0x2ULL
#define F_PDE_V2_VALUE_SET			0x4ULL
#define F_PDE_V3_VALUE_SET			0x8ULL

#define F_PDE_BIG_PAGE_APERTURE_SET_ONLY	0x01ULL
#define F_PDE_BIG_PAGE_APERTURE_ADDR_SET	0x03ULL
#define F_PDE_SMALL_PAGE_APERTURE_SET_ONLY	0x04ULL
#define F_PDE_SMALL_PAGE_APERTURE_ADDR_SET	0x0CULL
#define F_PDE_SMALL_BIG_SET			0x0FULL
#define F_PDE0_PGSZ_MEM_NULL			0x10ULL

static const char *f_get_pde0_pgsz[] = {
	[F_PDE_BIG_PAGE_APERTURE_SET_ONLY] =
				"get_pde0_pgsz_big_page_only_aperture_set",
	[F_PDE_BIG_PAGE_APERTURE_ADDR_SET] =
				"get_pde0_pgsz_big_page_aperture_addr_set",
	[F_PDE_SMALL_PAGE_APERTURE_SET_ONLY] =
				"get_pde0_pgsz_small_page_only_aperture_set",
	[F_PDE_SMALL_PAGE_APERTURE_ADDR_SET] =
				"get_pde0_pgsz_small_page_aperture_addr_set",
	[F_PDE_SMALL_BIG_SET] = "get_pde0_pgsz_small_big_set",
	[F_PDE0_PGSZ_MEM_NULL] = "get_pde0_pgsz_mem_null",
};

int test_gp10b_get_pde0_pgsz(struct unit_module *m, struct gk20a *g, void *args)
{
	int ret = UNIT_FAIL;
	int err;
	u64 branch = (u64)args;
	struct vm_gk20a vm;
	struct nvgpu_gmmu_pd pd;
	const struct gk20a_mmu_level *l = mmu_level;
	u64 vaddr, size = SZ_4K;
	u32 *data;
	u32 ret_pgsz;
	struct nvgpu_mem *tmp_mem_ptr = NULL;

	unit_assert(l != NULL, goto done);

	unit_assert(g->mm.pd_cache == NULL, goto done);

	vm.mm = &g->mm;
	err = nvgpu_pd_cache_init(g);
	unit_assert(err == 0, goto done);

	err = nvgpu_pd_alloc(&vm, &pd, size);
	unit_assert(err == 0, goto done);

	vaddr = nvgpu_pd_gpu_addr(g, &pd);
	unit_assert(vaddr != 0ULL, goto done);

	if (branch & F_PDE0_PGSZ_MEM_NULL) {
		tmp_mem_ptr = pd.mem;
		pd.mem = NULL;
	} else {
		data = pd.mem->cpu_va;

		data[0] = branch & F_PDE_V0_VALUE_SET ?
			(gmmu_new_dual_pde_aperture_big_sys_mem_ncoh_f() |
			gmmu_new_dual_pde_aperture_big_sys_mem_coh_f() |
			gmmu_new_dual_pde_aperture_big_video_memory_f()) : 0U;
		data[1] = branch & F_PDE_V1_VALUE_SET ? 1U : 0U;
		data[2] = branch & F_PDE_V2_VALUE_SET ?
			(gmmu_new_dual_pde_aperture_small_sys_mem_ncoh_f() |
			gmmu_new_dual_pde_aperture_small_sys_mem_coh_f() |
			gmmu_new_dual_pde_aperture_small_video_memory_f()) : 0U;
		data[3] = branch & F_PDE_V3_VALUE_SET ? 1U : 0U;
	}

	ret_pgsz = l[3].get_pgsz(g, l, &pd, 0U);

	if (branch == F_PDE_BIG_PAGE_APERTURE_ADDR_SET) {
		unit_assert(ret_pgsz == GMMU_PAGE_SIZE_BIG, goto done);
	} else if (branch == F_PDE_SMALL_PAGE_APERTURE_ADDR_SET) {
		unit_assert(ret_pgsz == GMMU_PAGE_SIZE_SMALL, goto done);
	} else {
		unit_assert(ret_pgsz == GMMU_NR_PAGE_SIZES, goto done);
	}

	if (branch & F_PDE0_PGSZ_MEM_NULL) {
		pd.mem = tmp_mem_ptr;
	}

	ret = UNIT_SUCCESS;

done:
	if (ret != UNIT_SUCCESS) {
		unit_err(m, "%s: %s failed\n", __func__,
						f_get_pde0_pgsz[branch]);
	}
	nvgpu_pd_free(&vm, &pd);
	nvgpu_pd_cache_fini(g);
	return ret;
}

struct unit_module_test mm_gmmu_gp10b_fusa_tests[] = {
	UNIT_TEST(big_pgsz, test_gp10b_mm_get_default_big_page_size, NULL, 0),
	UNIT_TEST(iommu_bit, test_gp10b_mm_get_iommu_bit, NULL, 0),
	UNIT_TEST(max_page_table_levels, test_gp10b_get_max_page_table_levels, NULL, 0),
	UNIT_TEST(mmu_levels, test_gp10b_mm_get_mmu_levels, NULL, 0),
	UNIT_TEST(update_gmmu_pde3_locked, test_update_gmmu_pde3_locked, NULL, 0),
	UNIT_TEST(update_gmmu_pde0_locked_s0, test_update_gmmu_pde0_locked, (void *)F_UPDATE_GMMU_PDE0_SMALL_PAGE, 0),
	UNIT_TEST(update_gmmu_pde0_locked_s1, test_update_gmmu_pde0_locked, (void *)F_UPDATE_GMMU_PDE0_BIG_PAGE, 0),
	UNIT_TEST(update_gmmu_pte_locked_s0, test_update_gmmu_pte_locked, (void *)F_UPDATE_PTE_PHYS_ADDR_ZERO, 0),
	UNIT_TEST(update_gmmu_pte_locked_s1, test_update_gmmu_pte_locked, (void *)F_UPDATE_PTE_DEFAULT, 0),
	UNIT_TEST(update_gmmu_pte_locked_s2, test_update_gmmu_pte_locked, (void *)F_UPDATE_PTE_ATTRS_PRIV_READ_ONLY, 0),
	UNIT_TEST(update_gmmu_pte_locked_s3, test_update_gmmu_pte_locked, (void *)F_UPDATE_PTE_ATTRS_VALID, 0),
	UNIT_TEST(update_gmmu_pte_locked_s4, test_update_gmmu_pte_locked, (void *)F_UPDATE_PTE_ATTRS_CACHEABLE, 0),
	UNIT_TEST(update_gmmu_pte_locked_s5, test_update_gmmu_pte_locked, (void *)F_UPDATE_PTE_ATTRS_VIDMEM, 0),
	UNIT_TEST(update_gmmu_pte_locked_s6, test_update_gmmu_pte_locked, (void *)F_UPDATE_PTE_PLATFORM_ATOMIC, 0),
	UNIT_TEST(update_gmmu_pte_locked_s7, test_update_gmmu_pte_locked, (void *)F_UPDATE_PTE_SPARSE, 0),
	UNIT_TEST(gp10b_get_pde0_pgsz_s0, test_gp10b_get_pde0_pgsz, (void *)F_PDE_BIG_PAGE_APERTURE_SET_ONLY, 0),
	UNIT_TEST(gp10b_get_pde0_pgsz_s1, test_gp10b_get_pde0_pgsz, (void *)F_PDE_BIG_PAGE_APERTURE_ADDR_SET, 0),
	UNIT_TEST(gp10b_get_pde0_pgsz_s2, test_gp10b_get_pde0_pgsz, (void *)F_PDE_SMALL_PAGE_APERTURE_SET_ONLY, 0),
	UNIT_TEST(gp10b_get_pde0_pgsz_s3, test_gp10b_get_pde0_pgsz, (void *)F_PDE_SMALL_PAGE_APERTURE_ADDR_SET, 0),
	UNIT_TEST(gp10b_get_pde0_pgsz_s4, test_gp10b_get_pde0_pgsz, (void *)F_PDE_SMALL_BIG_SET, 0),
	UNIT_TEST(gp10b_get_pde0_pgsz_s5, test_gp10b_get_pde0_pgsz, (void *)F_PDE0_PGSZ_MEM_NULL, 0),
};

UNIT_MODULE(gmmu_gp10b_fusa, mm_gmmu_gp10b_fusa_tests, UNIT_PRIO_NVGPU_TEST);
