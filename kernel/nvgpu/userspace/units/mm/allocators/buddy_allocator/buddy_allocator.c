/*
 * Copyright (c) 2019-2020, NVIDIA CORPORATION.  All rights reserved.
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

#include <nvgpu/gk20a.h>
#include <nvgpu/sizes.h>
#include <nvgpu/types.h>
#include <nvgpu/allocator.h>
#include <nvgpu/posix/kmem.h>
#include <nvgpu/posix/posix-fault-injection.h>

#include "common/mm/allocators/buddy_allocator_priv.h"

#include <hal/bus/bus_gk20a.h>
#include <hal/mm/gmmu/gmmu_gp10b.h>
#include <hal/pramin/pramin_init.h>
#include <nvgpu/hw/gk20a/hw_pram_gk20a.h>

#include "buddy_allocator.h"

#define SZ_8K			(SZ_4K << 1)
#define SZ_16K			(SZ_4K << 2)
#define BA_DEFAULT_BASE		SZ_4K
#define BA_DEFAULT_SIZE		SZ_1M
#define BA_DEFAULT_BLK_SIZE	SZ_4K

static struct nvgpu_allocator *na;


/*
 * Free vm and nvgpu_allocator
 */
static void free_vm_env(struct unit_module *m, struct gk20a *g,
						struct vm_gk20a *test_vm)
{
	/* Free allocated vm */
	nvgpu_vm_put(test_vm);
	nvgpu_kfree(g, na);
}

/*
 * Initialize vm structure and allocate nvgpu_allocator
 */
static struct vm_gk20a *init_vm_env(struct unit_module *m, struct gk20a *g,
					bool big_pages, const char *name)
{
	u64 flags = 0ULL;
	u64 low_hole, aperture_size;
	struct vm_gk20a *test_vm = NULL;

	/* Initialize vm */

	/* Minimum HALs for vm_init */
	g->ops.mm.gmmu.get_default_big_page_size =
		nvgpu_gmmu_default_big_page_size;
	g->ops.mm.gmmu.get_mmu_levels = gp10b_mm_get_mmu_levels;
	g->ops.mm.gmmu.get_max_page_table_levels = gp10b_get_max_page_table_levels;

#ifdef CONFIG_NVGPU_DGPU
	/* Minimum HAL init for PRAMIN */
	g->ops.bus.set_bar0_window = gk20a_bus_set_bar0_window;
	nvgpu_pramin_ops_init(g);
	unit_assert(g->ops.pramin.data032_r != NULL, return NULL);
#endif

	/* vm should init with SYSMEM */
	nvgpu_set_enabled(g, NVGPU_MM_UNIFIED_MEMORY, true);

	/*
	 * Initialize VM space for system memory to be used throughout this
	 * unit module.
	 * Values below are similar to those used in nvgpu_init_system_vm()
	 */
	low_hole = SZ_4K * 16UL;
	aperture_size = GK20A_PMU_VA_SIZE;

	flags |= GPU_ALLOC_GVA_SPACE;

	/* Init vm with big_pages disabled */
	test_vm = nvgpu_vm_init(g, g->ops.mm.gmmu.get_default_big_page_size(),
				   low_hole,
				   0ULL,
				   nvgpu_safe_sub_u64(aperture_size, low_hole),
				   0ULL,
				   big_pages,
				   false,
				   false,
				   name);

	if (test_vm == NULL) {
		unit_err(m, "Could not allocate vm\n");
		return NULL;
	}

	na = (struct nvgpu_allocator *)
			nvgpu_kzalloc(g, sizeof(struct nvgpu_allocator));
	if (na == NULL) {
		nvgpu_vm_put(test_vm);
		unit_err(m, "Could not allocate nvgpu_allocator\n");
		return NULL;
	}

	return test_vm;
}

/*
 * nvgpu_buddy_allocator initialized with big pages enabled vm
 * Test buddy allocator functionality with big pages
 */
int test_buddy_allocator_with_big_pages(struct unit_module *m,
					struct gk20a *g, void *args)
{
	u64 base = 0x4000000;	/* PDE aligned */
	u64 size = SZ_256M;
	u64 blk_size = BA_DEFAULT_BLK_SIZE;
	u64 max_order = GPU_BALLOC_MAX_ORDER;
	u64 flags = GPU_ALLOC_GVA_SPACE;
	u64 addr, addr1, addr2, addr3;
	struct vm_gk20a *vm_big_pages = init_vm_env(m, g, true, "vm_big_pages");

	if (vm_big_pages == NULL) {
		unit_return_fail(m, "couldn't init vm big pages env\n");
	}

	/*
	 * Initialize buddy allocator, base not pde aligned
	 * Expect to fail
	 */
	if (nvgpu_allocator_init(g, na, vm_big_pages, "test", SZ_1K, size,
			blk_size, max_order, flags, BUDDY_ALLOCATOR) == 0) {
		free_vm_env(m, g, vm_big_pages);
		unit_return_fail(m, "ba inited with unaligned pde\n");
	}

	/*
	 * Initialize buddy allocator, base = 0
	 * Expect to fail
	 */
	if (nvgpu_allocator_init(g, na, vm_big_pages, "test", 0ULL, size,
			blk_size, max_order, flags, BUDDY_ALLOCATOR) == 0) {
		free_vm_env(m, g, vm_big_pages);
		unit_return_fail(m, "ba_big_pages inited "
			"despite base=0, blk_size not pde aligned\n");
	}

	/*
	 * Initialize buddy allocator, base = 256M
	 * Expect to fail
	 */
	if (nvgpu_allocator_init(g, na, vm_big_pages, "test", SZ_256M, SZ_64K,
			blk_size, max_order, flags, BUDDY_ALLOCATOR) == 0) {
		free_vm_env(m, g, vm_big_pages);
		unit_return_fail(m, "ba_big_pages inited "
			"despite base=0, blk_size not pde aligned\n");
	}

	/* Initialize buddy allocator with big pages for this test */
	if (nvgpu_allocator_init(g, na, vm_big_pages, "test", base, size,
			blk_size, max_order, flags, BUDDY_ALLOCATOR) != 0) {
		free_vm_env(m, g, vm_big_pages);
		unit_return_fail(m, "ba_big_pages init failed\n");
	}

	/*
	 * alloc_pte(), len = 0
	 * Expect to fail
	 */
	addr = na->ops->alloc_pte(na, 0ULL, SZ_4K);
	if (addr != 0) {
		unit_err(m, "%d: ba_big_pages alloced with len = 0\n",
			__LINE__);
		goto fail;
	}

	addr1 = na->ops->alloc(na, SZ_4K);
	if (addr1 == 0) {
		unit_err(m, "%d: ba_big_pages alloc() couldn't allocate\n",
			__LINE__);
		goto fail;
	}

	na->ops->free_alloc(na, addr1);

	/*
	 * alloc_pte()
	 * Allocated buddy PTE_size will be 2 (64K page)
	 *
	 * Observation: addr2 is same as addr1 (previous allocation).
	 * When addr1 is freed, buddies with PTE_size = 1 will be merged
	 * to higher order buddies with PTE_SIZE_ANY.
	 */
	addr2 = na->ops->alloc_pte(na, SZ_4K, SZ_64K);
	if (addr2 == 0) {
		unit_err(m, "%d: ba_big_pages alloc() couldn't allocate\n",
			__LINE__);
		goto fail;
	}

	/*
	 * alloc_pte(), page_size != (big or small page_size)
	 * Expect to fail
	 */
	addr = na->ops->alloc_pte(na, SZ_1K, SZ_1K);
	if (addr != 0) {
		unit_err(m, "%d: ba_big_pages alloced with 1K page\n",
			__LINE__);
		goto fail;
	}

	addr = na->ops->alloc_pte(na, SZ_1M, vm_big_pages->big_page_size);
	if (addr == 0) {
		unit_err(m,
			"%d: ba_big_pages couldn't allocate 1M big page\n",
			__LINE__);
		goto fail;
	}

	addr1 = na->ops->alloc_pte(na, SZ_1K, SZ_4K);
	if (addr1 == 0) {
		unit_err(m,
			"%d: ba_big_pages couldn't allocate 4K small page\n",
			__LINE__);
		goto fail;
	}

	addr2 = na->ops->alloc_pte(na, SZ_64K, vm_big_pages->big_page_size);
	if (addr2 == 0) {
		unit_err(m,
			"%d: ba_big_pages couldn't allocate 64K big page\n",
			__LINE__);
		goto fail;
	}

	/*
	 * alloc_fixed() - start at 8K
	 * Expect to fail - as buddy allocator base starts at 64M
	 */
	addr = na->ops->alloc_fixed(na, SZ_8K, SZ_8K, SZ_64K);
	if (addr != 0) {
		unit_err(m,
			"%d: ba_big_pages alloced at 8K despite base = 64M\n",
			__LINE__);
		goto fail;
	}

	addr3 = na->ops->alloc_pte(na, SZ_1M, SZ_4K);
	if (addr3 == 0) {
		unit_err(m,
			"%d: ba_big_pages couldn't allocate 1M small page\n",
			__LINE__);
		goto fail;
	}

	na->ops->fini(na);
	return UNIT_SUCCESS;

fail:
	na->ops->fini(na);
	free_vm_env(m, g, vm_big_pages);
	return UNIT_FAIL;
}

/*
 * nvgpu_buddy_allocator initialized with big pages disabled
 * Test buddy allocator functionality with big pages
 */
int test_buddy_allocator_with_small_pages(struct unit_module *m,
					struct gk20a *g, void *args)
{
	u64 base = SZ_1K;
	u64 size = SZ_1M;
	u64 blk_size = SZ_1K;
	u64 max_order = 10;
	u64 flags = GPU_ALLOC_GVA_SPACE;
	u64 addr;
	struct nvgpu_buddy_allocator *ba;
	struct nvgpu_posix_fault_inj *kmem_fi =
		nvgpu_kmem_get_fault_injection();

	struct vm_gk20a *vm_small_pages = init_vm_env(m, g, false,
							"vm_small_pages");

	if (vm_small_pages == NULL) {
		unit_return_fail(m, "couldn't init vm small pages env\n");
	}


	/* Initialize buddy allocator with big page disabled for this test */
	if (nvgpu_allocator_init(g, na, vm_small_pages, "test", base, size,
			blk_size, max_order, flags, BUDDY_ALLOCATOR) != 0) {
		free_vm_env(m, g, vm_small_pages);
		unit_return_fail(m, "ba small pages init failed\n");
	}

	/* Check if nvgpu_allocator ops inited */
	if (!na->ops->inited) {
		unit_err(m, "%d: ba_small_pages ops not inited\n", __LINE__);
		goto fail;
	}

	/*
	 * Alloc 2M memory at base 1K
	 * Expect to fail as requested order/size > available size
	 */
	addr = na->ops->alloc_fixed(na, SZ_1K, SZ_1M << 1, SZ_4K);
	if (addr != 0) {
		unit_err(m, "%d: ba_small_pages allocated 1K at base 3K\n",
			__LINE__);
		goto fail;
	}

	/* Alloc 1K memory at base 1K */
	addr = na->ops->alloc_fixed(na, SZ_1K, SZ_1K, SZ_4K);
	if (addr == 0) {
		unit_err(m, "%d: ba_small_pages 1K fixed_alloc failed\n",
			__LINE__);
		goto fail;
	}

	/*
	 * Alloc 1K memory at base 3K
	 * Expect to fail - Buddy PTE size = 4K PTE_size due to previous alloc
	 * Otherwise, memory would have been allocated
	 */
	addr = na->ops->alloc_fixed(na, 0x0C00, SZ_1K,
				vm_small_pages->big_page_size);
	if (addr != 0) {
		unit_err(m, "%d: ba_small_pages allocated 1K at base 3K\n",
			__LINE__);
		goto fail;
	}

	/*
	 * alloc_pte()
	 * Expect to fail - page_size != (big or small page_size)
	 */
	addr = na->ops->alloc_pte(na, SZ_4K, SZ_1K);
	if (addr != 0) {
		unit_err(m, "%d: ba_small_pages alloced 1K page\n", __LINE__);
		goto fail;
	}

	/*
	 * alloc_pte() with len = 0
	 * Expect to fail
	 */
	addr = na->ops->alloc_pte(na, 0ULL, SZ_4K);
	if (addr != 0) {
		unit_err(m, "%d: ba_small_pages alloced with len=0\n",
			__LINE__);
		goto fail;
	}

	/*
	 * alloc_pte(), page_size = vm->big_page_size
	 * Expect to fail - PDE is set to 4K PTE_size because of previous allocs
	 */
	addr = na->ops->alloc_pte(na, SZ_64K, vm_small_pages->big_page_size);
	if (addr != 0) {
		unit_err(m,
			"%d: ba_small_pages alloced with PTE=big_page\n",
			__LINE__);
		goto fail;
	}

	/*
	 * alloc_pte()
	 * Expect to fail as size > ba_length
	 */
	addr = na->ops->alloc_pte(na, SZ_1M, SZ_4K);
	if (addr != 0) {
		unit_err(m,
			"%d: ba_small_pages alloced size > ba_length\n",
			__LINE__);
		goto fail;
	}

	/* Let allocations be freed during cleanup */

	/*
	 * Fault injection in alloc_fixed()
	 * Tests cleanup code in alloc_fixed()
	 *
	 * Note: Purposely testing after some allocs
	 * This will try to allocate list of buddies
	 */
	nvgpu_posix_enable_fault_injection(kmem_fi, true, 5);
	addr = na->ops->alloc_fixed(na, SZ_1K << 1, SZ_8K, SZ_4K);
	if (addr != 0) {
		unit_err(m,
			"%d: Fixed memory alloced despite fault injection\n",
			__LINE__);
		goto fail;
	}
	nvgpu_posix_enable_fault_injection(kmem_fi, false, 0);

	/*
	 * alloc_pte()
	 * Expect to fail as pte_size is invalid
	 */
	addr = na->ops->alloc_fixed(na, SZ_8K, SZ_8K, 0);
	if (addr != 0) {
		unit_err(m, "%d: Allocated with PTE_size invalid\n",
			__LINE__);
		goto fail;
	}

	na->ops->fini(na);

	/* Request align_order > ba->max_order */
	if (nvgpu_allocator_init(g, na, vm_small_pages, "test", base, size,
			blk_size, 5, flags, BUDDY_ALLOCATOR) != 0) {
		free_vm_env(m, g, vm_small_pages);
		unit_return_fail(m, "ba small pages init failed\n");
	}

	ba = na->priv;
	addr = na->ops->alloc_fixed(na, ba->start, SZ_1M, SZ_4K);
	if (addr != 0) {
		unit_err(m, "%d: Allocated with align_order > ba->max_order\n",
			__LINE__);
		goto fail;
	}

	return UNIT_SUCCESS;

fail:
	na->ops->fini(na);
	free_vm_env(m, g, vm_small_pages);
	return UNIT_FAIL;
}

/*
 * Test buddy_allocator allocs
 */
int test_nvgpu_buddy_allocator_alloc(struct unit_module *m,
					struct gk20a *g, void *args)
{
	u64 base = SZ_4K;
	u64 size = SZ_1M;
	u64 blk_size = SZ_1K;
	u64 max_order = 0;
	u64 flags = 0ULL;
	u64 addr;
	struct nvgpu_buddy_allocator *ba;
	struct nvgpu_posix_fault_inj *kmem_fi =
		nvgpu_kmem_get_fault_injection();
	int result = UNIT_FAIL;

	na = (struct nvgpu_allocator *)
			nvgpu_kzalloc(g, sizeof(struct nvgpu_allocator));
	if (na == NULL) {
		unit_return_fail(m, "Could not allocate nvgpu_allocator\n");
	}

	/* Initialize buddy allocator for this test */
	if (nvgpu_allocator_init(g, na, NULL, "test_alloc", base, size,
			blk_size, max_order, flags, BUDDY_ALLOCATOR) != 0) {
		nvgpu_kfree(g, na);
		unit_return_fail(m, "ba init for alloc failed\n");
	}

	ba = na->priv;

	/*
	 * Fault injection in alloc()
	 * Tests cleanup code in alloc()
	 */
	nvgpu_posix_enable_fault_injection(kmem_fi, true, 3);
	addr = na->ops->alloc(na, SZ_4K);
	if (addr != 0) {
		unit_err(m, "%d: alloced despite fault injection at 3\n",
			__LINE__);
		goto cleanup;
	}
	nvgpu_posix_enable_fault_injection(kmem_fi, false, 0);

	/*
	 * Fault injection in alloc()
	 * Tests cleanup code in alloc()
	 */
	nvgpu_posix_enable_fault_injection(kmem_fi, true, 2);
	addr = na->ops->alloc(na, SZ_4K);
	if (addr != 0) {
		unit_err(m, "%d: alloced despite fault injection at 2\n",
			__LINE__);
		goto cleanup;
	}
	nvgpu_posix_enable_fault_injection(kmem_fi, false, 0);

	/*
	 * Fault injection in alloc_fixed()
	 * Tests cleanup branch
	 */
	nvgpu_posix_enable_fault_injection(kmem_fi, true, 0);
	addr = na->ops->alloc_fixed(na, SZ_8K, SZ_8K, SZ_4K);
	if (addr != 0) {
		unit_err(m, "%d: alloc_fixed alloced despite fault injection\n",
			__LINE__);
		goto cleanup;
	}
	nvgpu_posix_enable_fault_injection(kmem_fi, false, 0);

	addr = na->ops->alloc_fixed(na, SZ_8K, SZ_8K, SZ_4K);
	if (addr == 0) {
		unit_err(m, "%d: alloc_fixed couldn't allocate\n", __LINE__);
		goto cleanup;
	}

	/*
	 * Next few allocations test conditions in balloc_is_range_free()
	 */
	/*
	 * Request 6K to 22K to be allocated
	 * Expect to fail - part (8K to 16K) is already allocated
	 */
	addr = na->ops->alloc_fixed(na, 0x1800, SZ_4K << 2, SZ_4K);
	if (addr != 0) {
		unit_err(m, "%d: Alloced 6K to 22K despite overlap\n",
			__LINE__);
		goto cleanup;
	}

	/*
	 * Request 6K to 14K to be allocated
	 * Expect to fail - part (8K to 14K) is already allocated
	 */
	addr = na->ops->alloc_fixed(na, 0x1800, SZ_8K, SZ_4K);
	if (addr != 0) {
		unit_err(m, "%d: Alloced 6K to 14K despite overlap\n",
			__LINE__);
		goto cleanup;
	}

	addr = na->ops->alloc_fixed(na, 0x1800, SZ_1K, SZ_4K);
	if (addr == 0) {
		unit_err(m, "%d: Couldn't allocate range 6K to 7K\n", __LINE__);
		goto cleanup;
	}

	/*
	 * Request 10K to 11K to be allocated
	 * Expect to fail - 10K to 11K already allocated
	 */
	addr = na->ops->alloc_fixed(na, 0x2800, SZ_1K, SZ_4K);
	if (addr != 0) {
		unit_err(m, "%d: Alloced 10K to 11K despite overlap\n",
			__LINE__);
		goto cleanup;
	}

	/*
	 * Request 12K to 20K to be allocated
	 * Expect to fail - 12K to 16K already allocated
	 */
	addr = na->ops->alloc_fixed(na, 0x3000, SZ_8K, SZ_4K);
	if (addr != 0) {
		unit_err(m, "%d: Alloced 12K to 20K despite overlap\n",
			__LINE__);
		goto cleanup;
	}

	/*
	 * Test nvgpu_buddy_allocator_destroy()
	 */
	ba->buddy_list_len[0] = 100;
	if (!EXPECT_BUG(na->ops->fini(na))) {
		unit_err(m, "%d: Excess buddies didn't trigger BUG()\n",
			__LINE__);
		goto cleanup;
	}
	/*
	 * Release the mutex that was left locked when fini() was interrupted
	 * by BUG()
	 */
	alloc_unlock(na);
	ba->buddy_list_len[0] = 0;

	ba->buddy_list_split[0] = 100;
	if (!EXPECT_BUG(na->ops->fini(na))) {
		unit_err(m, "%d: Excess split nodes didn't trigger BUG()\n",
			__LINE__);
		goto cleanup;
	}
	/*
	 * Release the mutex that was left locked when fini() was interrupted
	 * by BUG()
	 */
	alloc_unlock(na);
	ba->buddy_list_split[0] = 0;

	ba->buddy_list_alloced[0] = 100;
	if (!EXPECT_BUG(na->ops->fini(na))) {
		unit_err(m, "%d: Excess alloced nodes didn't trigger BUG()\n",
			__LINE__);
		goto cleanup;
	}
	ba->buddy_list_alloced[0] = 0;

	result = UNIT_SUCCESS;

cleanup:
	/* Clean up might BUG() so protect it with EXPECT_BUG */
	alloc_unlock(na);
	EXPECT_BUG(na->ops->fini(na));
	nvgpu_kfree(g, na);
	return result;
}

/*
 * Tests buddy_allocator carveouts
 */
int test_nvgpu_buddy_allocator_carveout(struct unit_module *m,
					struct gk20a *g, void *args)
{
	int err;
	u64 addr;
	struct nvgpu_alloc_carveout test_co =
				NVGPU_CARVEOUT("test_co", 0ULL, 0ULL);
	struct nvgpu_alloc_carveout test_co1 =
				NVGPU_CARVEOUT("test_co1", 0ULL, 0ULL);
	struct nvgpu_alloc_carveout test_co2 =
				NVGPU_CARVEOUT("test_co2", 0ULL, 0ULL);

	/*
	 * test_co base  < buddy_allocator start
	 * Expect to fail
	 */
	err = na->ops->reserve_carveout(na, &test_co);
	if (err == 0) {
		unit_return_fail(m, "carveout reserved despite base < start\n");
	}

	/*
	 * test_co base + test_co length > buddy allocator end
	 * Expect to fail
	 */
	test_co.base = BA_DEFAULT_BASE;
	test_co.length = BA_DEFAULT_SIZE << 1;

	err = na->ops->reserve_carveout(na, &test_co);
	if (err == 0) {
		unit_return_fail(m,
			"carveout reserved despite base+length > end\n");
	}

	/*
	 * base unaligned
	 * Expect to fail
	 */
	test_co.base =  BA_DEFAULT_BASE + 1ULL;
	test_co.length = SZ_4K;

	err = na->ops->reserve_carveout(na, &test_co);
	if (err == 0) {
		unit_return_fail(m, "carveout reserved with unaligned base\n");
	}

	test_co1.base =  BA_DEFAULT_BASE;
	test_co1.length = SZ_4K;
	err = na->ops->reserve_carveout(na, &test_co1);
	if (err < 0) {
		unit_return_fail(m, "couldn't reserve 4K carveout\n");
	}

	na->ops->release_carveout(na, &test_co1);

	test_co1.base =  SZ_4K;
	test_co1.length = SZ_4K;
	err = na->ops->reserve_carveout(na, &test_co1);
	if (err < 0) {
		unit_return_fail(m,
			"couldn't reserve 4K carveout after release\n");
	}

	/*
	 * Allocate 64K carveout at already allocated address
	 * Expect to fail
	 */
	test_co.base =  0x1800;
	test_co.length = SZ_64K;
	err = na->ops->reserve_carveout(na, &test_co);
	if (err == 0) {
		unit_return_fail(m,
			"64K carveout reserved at already allocated address\n");
	}

	test_co2.base =  SZ_16K;
	test_co2.length = SZ_64K;
	err = na->ops->reserve_carveout(na, &test_co2);
	if (err < 0) {
		unit_return_fail(m, "couldn't reserve 64K carveout\n");
	}

	/*
	 * Allocate 8K carveout at already allocated address
	 * Expect to fail
	 */
	test_co.base =  0x1800 + SZ_4K;
	test_co.length = SZ_8K;
	err = na->ops->reserve_carveout(na, &test_co);
	if (err == 0) {
		unit_return_fail(m,
			"8K carveout reserved at already allocated address\n");
	}

	/*
	 * Allocate 4K carveout at already allocated address
	 * Expect to fail
	 */
	test_co.base = SZ_16K;
	test_co.length = SZ_4K;
	err = na->ops->reserve_carveout(na, &test_co);
	if (err == 0) {
		unit_return_fail(m,
			"8K carveout reserved at already allocated address\n");
	}

	/*
	 * Allocate 8K carveout at already allocated address
	 * Expect to fail
	 */
	test_co.base =  0x1800;
	test_co.length = SZ_4K;
	err = na->ops->reserve_carveout(na, &test_co);
	if (err == 0) {
		unit_return_fail(m,
			"8K carveout reserved at already allocated address\n");
	}

	addr = na->ops->alloc(na, (SZ_64K >> 1));
	if (addr == 0) {
		unit_return_fail(m, "couldn't allocate 32K\n");
	}

	/*
	 * Allocate carveout after alloc
	 * Expect to fail
	 */
	test_co.base =  SZ_8K;
	test_co.length = SZ_4K;
	err = na->ops->reserve_carveout(na, &test_co);
	if (err == 0) {
		unit_return_fail(m, "carveout reserve should have failed\n");
	}

	return UNIT_SUCCESS;
}

/*
 * Tests buddy_allocator basic ops and allocations
 */
int test_nvgpu_buddy_allocator_basic_ops(struct unit_module *m,
					struct gk20a *g, void *args)
{
	u64 addr;
	struct nvgpu_buddy_allocator *ba = na->priv;

	if (!na->ops->inited(na)) {
		unit_return_fail(m, "buddy_allocator ops->inited failed\n");
	}

	addr = na->ops->base(na);
	if (addr != ba->start) {
		unit_return_fail(m, "buddy_allocator ops->base failed\n");
	}

	addr = na->ops->length(na);
	if (addr != ba->length) {
		unit_return_fail(m, "buddy_allocator ops->length failed\n");
	}

	addr = na->ops->end(na);
	if (addr != ba->end) {
		unit_return_fail(m, "buddy_allocator ops->end failed\n");
	}

	/*
	 * Space cannot be zero as carveouts are allocated
	 */
	addr = na->ops->space(na);
	if (addr == 0) {
		unit_return_fail(m, "buddy_allocator ops->space failed\n");
	}

	/*
	 * alloc() with len = 0
	 * Expect to fail
	 */
	addr = na->ops->alloc(na, 0);
	if (addr != 0) {
		unit_return_fail(m, "ops->alloc allocated with len = 0\n");
	}

	addr = na->ops->alloc(na, (SZ_64K >> 1));

	na->ops->free_alloc(na, addr);

	na->ops->free_alloc(na, addr);

	na->ops->free_alloc(na, 0ULL);

	/*
	 * len = 2M (requesting more than available memory)
	 * Expect to fail
	 */
	addr = na->ops->alloc_pte(na, (SZ_1M << 2), SZ_1K);
	if (addr != 0) {
		unit_return_fail(m, "ops->alloc_pte allocated with len = 0\n");
	}

	addr = na->ops->alloc_pte(na, (SZ_4K << 2), SZ_1K << 1);

	na->ops->free_alloc(na, addr);

	/*
	 * Unaligned base
	 * Expect to fail
	 */
	addr = na->ops->alloc_fixed(na, (SZ_64K + 1ULL), SZ_4K, SZ_1K);
	if (addr != 0) {
		unit_return_fail(m,
			"ops->alloc_fixed allocated with unaligned base\n");
	}

	/*
	 * alloc_fixed() with len = 0
	 * Expect to fail
	 */
	addr = na->ops->alloc_fixed(na, SZ_4K, 0, SZ_1M);
	if (addr != 0) {
		unit_return_fail(m,
			"ops->alloc_fixed allocated with len = 0\n");
	}

	/*
	 * Carveout already allocated at base = 4K (in previous test)
	 * Expect to fail
	 */
	addr = na->ops->alloc_fixed(na, SZ_4K, SZ_4K, SZ_1K);
	if (addr != 0) {
		unit_return_fail(m, "alloced\n");
	}

	addr = na->ops->alloc_fixed(na, SZ_1M, SZ_4K, SZ_1K);
	if (addr == 0) {
		unit_return_fail(m,
			"couldn't allocate range 1M to (1M + 4K)\n");
	}

	/*
	 * Allocate with 0 pte size
	 * With GVA_space disabled, page_size is ignored
	 */
	addr = na->ops->alloc_fixed(na, SZ_64K << 2, SZ_4K, 0);
	if (addr == 0) {
		unit_return_fail(m, "allocated for page_size=0 request\n");
	}

	return UNIT_SUCCESS;
}

/*
 * De-initialize buddy allocator
 */
int test_nvgpu_buddy_allocator_destroy(struct unit_module *m,
					struct gk20a *g, void *args)
{
	na->ops->fini(na);
	nvgpu_kfree(g, na);
	return UNIT_SUCCESS;
}

/*
 * Tests nvgpu_buddy_allocator_init()
 * This test considers multiple conditions to initialize buddy allocator
 */
int test_nvgpu_buddy_allocator_init(struct unit_module *m,
					struct gk20a *g, void *args)
{
	u64 base = BA_DEFAULT_BASE;
	u64 size = BA_DEFAULT_SIZE;
	u64 blk_size = BA_DEFAULT_BLK_SIZE;
	u64 max_order = GPU_BALLOC_MAX_ORDER;
	u64 flags = 0ULL;
	struct vm_gk20a vm1;
	struct nvgpu_buddy_allocator *ba;
	struct nvgpu_posix_fault_inj *kmem_fi =
		nvgpu_kmem_get_fault_injection();

	na = (struct nvgpu_allocator *)
			nvgpu_kzalloc(g, sizeof(struct nvgpu_allocator));
	if (na == NULL) {
		unit_return_fail(m, "Could not allocate nvgpu_allocator\n");
	}

	/* blk_size = 0 */
	if (nvgpu_allocator_init(g, na, NULL, "test_ba", base, size, 0ULL,
				max_order, flags, BUDDY_ALLOCATOR) == 0) {
		unit_return_fail(m, "ba inited despite blk_size=0\n");
	}

	/* Odd blk_size */
	if (nvgpu_allocator_init(g, na, NULL, "test_ba", base, size, 3ULL,
				max_order, flags, BUDDY_ALLOCATOR) == 0) {
		unit_return_fail(m, "ba inited despite odd blk_size value\n");
	}

	/* max_order > (u64)GPU_BALLOC_MAX_ORDER */
	if (nvgpu_allocator_init(g, na, NULL, "test_ba", base, size, blk_size,
		(u64)GPU_BALLOC_MAX_ORDER + 1, flags, BUDDY_ALLOCATOR) == 0) {
		unit_return_fail(m,
			"ba inited despite max_order > GPU_BALLOC_MAX_ORDER\n");
	}

	/* size = 0 */
	if (nvgpu_allocator_init(g, na, NULL, "test_ba", base, 0ULL, blk_size,
				max_order, flags, BUDDY_ALLOCATOR) == 0) {
		/* If buddy allocator was created, check length */
		ba = buddy_allocator(na);
		if (ba->length == 0ULL) {
			na->ops->fini(na);
			unit_return_fail(m, "ba inited with size = 0\n");
		}
		na->ops->fini(na);
	}

	/* base = 0 */
	if (nvgpu_allocator_init(g, na, NULL, "test_ba", 0ULL, size, blk_size,
				max_order, flags, BUDDY_ALLOCATOR) != 0) {
		unit_return_fail(m, "ba init with base=0 failed\n");
	} else {
		/* If buddy allocator was created, check base */
		ba = buddy_allocator(na);
		if (ba->base != blk_size) {
			na->ops->fini(na);
			unit_return_fail(m, "ba init with base=0 "
					"didn't update base = blk_size\n");
		}
		na->ops->fini(na);
	}

	/*
	 * base = 0x0101 (unaligned), GVA_space is disabled
	 * adds base as offset
	 */
	if (nvgpu_allocator_init(g, na, NULL, "test_ba", 0x0101, size,
			blk_size, max_order, flags, BUDDY_ALLOCATOR) != 0) {
		unit_return_fail(m, "ba init with unaligned base failed\n");
	} else {
		na->ops->fini(na);
	}

	/* ba init - GVA_space enabled, no vm */
	if (nvgpu_allocator_init(g, na, NULL, "test_ba", base, size, blk_size,
			max_order, GPU_ALLOC_GVA_SPACE, BUDDY_ALLOCATOR) == 0) {
		unit_return_fail(m, "ba inited "
				"with GPU_ALLOC_GVA_SPACE & vm=NULL\n");
	}

	/* Fault injection at nvgpu_buddy_allocator alloc */
	nvgpu_posix_enable_fault_injection(kmem_fi, true, 0);
	if (nvgpu_allocator_init(g, na, NULL, "test_ba", base, size, blk_size,
				max_order, flags, BUDDY_ALLOCATOR) == 0) {
		unit_return_fail(m,
			"ba inited despite fault injection\n");
	}
	nvgpu_posix_enable_fault_injection(kmem_fi, false, 0);

	/* Fault injection at buddy_cache create */
	nvgpu_posix_enable_fault_injection(kmem_fi, true, 1);
	if (nvgpu_allocator_init(g, na, NULL, "test_ba", base, size, blk_size,
				max_order, flags, BUDDY_ALLOCATOR) == 0) {
		unit_return_fail(m,
			"ba inited despite fault injection\n");
	}
	nvgpu_posix_enable_fault_injection(kmem_fi, false, 0);

	/* Fault injection at balloc_new_buddy */
	nvgpu_posix_enable_fault_injection(kmem_fi, true, 5);
	if (nvgpu_allocator_init(g, na, NULL, "test_ba", 0ULL, size, blk_size,
				max_order, flags, BUDDY_ALLOCATOR) == 0) {
		unit_return_fail(m,
			"buddy_allocator inited despite fault injection\n");
	}
	nvgpu_posix_enable_fault_injection(kmem_fi, false, 0);


	/*
	 * vm un-initialized,
	 * This doesn't complain as GPU_ALLOC_GVA_SPACE is disabled
	 */
	if (nvgpu_allocator_init(g, na, &vm1, "test_ba", base, 0x40000,
			blk_size, max_order, flags, BUDDY_ALLOCATOR) != 0) {
		unit_return_fail(m, "buddy_allocator_init failed\n");
	} else {
		na->ops->fini(na);
	}

	/*
	 * Initialize buddy allocator
	 * This ba will be used for further tests.
	 */
	if (nvgpu_allocator_init(g, na, NULL, "test_ba", base, size, blk_size,
				max_order, flags, BUDDY_ALLOCATOR) != 0) {
		unit_return_fail(m, "buddy_allocator_init failed\n");
	}

	return UNIT_SUCCESS;
}

struct unit_module_test buddy_allocator_tests[] = {

	/* BA initialized in this test is used by next tests */
	UNIT_TEST(init, test_nvgpu_buddy_allocator_init, NULL, 0),

	/* These tests use buddy allocator created in the first test */
	UNIT_TEST(carveout, test_nvgpu_buddy_allocator_carveout, NULL, 0),
	UNIT_TEST(basic_ops, test_nvgpu_buddy_allocator_basic_ops, NULL, 0),
	UNIT_TEST(destroy, test_nvgpu_buddy_allocator_destroy, NULL, 0),

	/* Independent tests */
	/* Tests allocations by buddy allocator */
	UNIT_TEST(alloc, test_nvgpu_buddy_allocator_alloc, NULL, 0),
	/* Tests buddy allocator - GVA_space enabled and big_pages disabled */
	UNIT_TEST(ops_small_pages, test_buddy_allocator_with_small_pages, NULL, 0),
	/* Tests buddy allocator - GVA_space enabled and big_pages enabled */
	UNIT_TEST(ops_big_pages, test_buddy_allocator_with_big_pages, NULL, 0),
};

UNIT_MODULE(buddy_allocator, buddy_allocator_tests, UNIT_PRIO_NVGPU_TEST);
