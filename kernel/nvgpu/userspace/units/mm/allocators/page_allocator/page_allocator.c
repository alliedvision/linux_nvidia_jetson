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

#include <nvgpu/page_allocator.h>
#include "page_allocator.h"

#ifdef CONFIG_NVGPU_DGPU

#define BA_DEFAULT_BASE		SZ_4K
#define BA_DEFAULT_LENGTH	SZ_1M
#define BA_DEFAULT_BLK_SIZE	SZ_4K
#define SZ_2K			(SZ_1K << 1)
#define SZ_8K			(SZ_4K << 1)
#define SZ_16K			(SZ_4K << 2)
#define SZ_32K			(SZ_64K >> 1)

static struct nvgpu_allocator *na;

/*
 * @fault_enable- Enable/Disable fault injection
 * @fault_at 	- If fault is enabled,
			fault_at contains fault_counter value
		  otherwise,
			fault_at should be set to 0
 * @base 	- Base address of allocation in case of fixed allocation
 * @len 	- Length of memory to be allocated
 * @flags 	- Additional flags to be enabled
 * @ret_addr	- Return address of allocation, this is used in free()
 * @expected_zero - Expected result of test
 * @error_msg	- Message to be displayed if test fails
 */
struct test_parameters {
	bool fault_enable;
	u32 fault_at;
	u64 base;
	u64 len;
	u64 flags;
	u64 ret_addr;
	bool expected_zero;
	char *error_msg;
};

static struct test_parameters fault_at_alloc_cache = {
	.fault_enable = true,
	.fault_at = 0,
	.base = 0ULL,
	.len = SZ_32K,
	.flags = 0ULL,
	.expected_zero = true,
	.error_msg = "alloced despite fault injection at alloc_cache",
};

static struct test_parameters fault_at_nvgpu_alloc = {
	.fault_enable = true,
	.fault_at = 1,
	.base = 0ULL,
	.len = SZ_32K,
	.flags = 0ULL,
	.expected_zero = true,
	.error_msg = "alloced despite fault injection at nvgpu_alloc",
};

static struct test_parameters fault_at_sgl_alloc = {
	.fault_enable = true,
	.fault_at = 1,
	.base = 0ULL,
	.len = SZ_32K,
	.flags = 0ULL,
	.expected_zero = true,
	.error_msg = "alloced despite fault injection at sgl alloc",
};

static struct test_parameters fault_at_page_cache = {
	.fault_enable = true,
	.fault_at = 2,
	.base = 0ULL,
	.len = SZ_32K,
	.flags = 0ULL,
	.expected_zero = true,
	.error_msg = "alloced despite fault injection at page_cache",
};

static struct test_parameters first_simple_alloc_32K = {
	.fault_enable = false,
	.fault_at = 0,
	.base = 0ULL,
	.len = SZ_32K,
	.flags = 0ULL,
	.expected_zero = false,
	.error_msg = "first instance of 32K alloc failed",
};

static struct test_parameters second_simple_alloc_32K = {
	.fault_enable = false,
	.fault_at = 0,
	.base = 0ULL,
	.len = SZ_32K,
	.flags = 0ULL,
	.expected_zero = false,
	.error_msg = "second instance of 32K alloc failed",
};

static struct test_parameters third_simple_alloc_32K = {
	.fault_enable = false,
	.fault_at = 0,
	.base = 0ULL,
	.len = SZ_32K,
	.flags = 0ULL,
	.expected_zero = false,
	.error_msg = "third instance of 32K alloc failed",
};

static struct test_parameters fourth_simple_alloc_32K = {
	.fault_enable = false,
	.fault_at = 0,
	.base = 0ULL,
	.len = SZ_32K,
	.flags = 0ULL,
	.expected_zero = false,
	.error_msg = "fourth instance of 32K alloc failed",
};

static struct test_parameters failing_alloc_16K = {
	.fault_enable = false,
	.fault_at = 0,
	.base = 0ULL,
	.len = SZ_16K,
	.flags = 0ULL,
	.expected_zero = true,
	.error_msg = "16K alloc is supposed to fail",
};

static struct test_parameters simple_alloc_8K = {
	.fault_enable = false,
	.fault_at = 0,
	.base = 0ULL,
	.len = SZ_8K,
	.flags = 0ULL,
	.expected_zero = false,
	.error_msg = "8K alloc failed",
};

static struct test_parameters failing_alloc_8K = {
	.fault_enable = false,
	.fault_at = 0,
	.base = SZ_64K,
	.len = SZ_8K,
	.flags = 0ULL,
	.expected_zero = true,
	.error_msg = "8K alloc supposed to fail",
};

static struct test_parameters alloc_no_scatter_gather = {
	.fault_enable = false,
	.fault_at = 0,
	.base = SZ_64K,
	.len = SZ_32K,
	.flags = GPU_ALLOC_NO_SCATTER_GATHER,
	.expected_zero = false,
	.error_msg = "32K alloc failed with no_scatter_gather enabled",
};

static struct test_parameters simple_alloc_128K = {
	.fault_enable = false,
	.fault_at = 0,
	.base = SZ_128K << 2,
	.len = SZ_128K,
	.flags = 0ULL,
	.expected_zero = false,
	.error_msg = "128K alloc failed",
};

static struct test_parameters alloc_contiguous = {
	.fault_enable = false,
	.fault_at = 0,
	.base = 0ULL,
	.len = SZ_128K << 2,
	.flags = GPU_ALLOC_FORCE_CONTIG,
	.expected_zero = true,
	.error_msg = "contiguous alloc should have failed",
};

static struct test_parameters simple_alloc_512K = {
	.fault_enable = false,
	.fault_at = 0,
	.base = 0ULL,
	.len = SZ_128K << 2,
	.flags = 0ULL,
	.expected_zero = false,
	.error_msg = "8K alloc failed",
};

static struct test_parameters alloc_more_than_available = {
	.fault_enable = false,
	.fault_at = 0,
	.base = 0ULL,
	.len = SZ_1M,
	.flags = 0ULL,
	.expected_zero = true,
	.error_msg = "Allocated more than available memory",
};

int test_page_alloc(struct unit_module *m, struct gk20a *g, void *args)
{
	struct test_parameters *param = (struct test_parameters *) args;
	struct nvgpu_page_allocator *pa = page_allocator(na);
	struct nvgpu_posix_fault_inj *kmem_fi =
		nvgpu_kmem_get_fault_injection();

	pa->flags |= param->flags;

	nvgpu_posix_enable_fault_injection(kmem_fi,
					param->fault_enable, param->fault_at);

	param->ret_addr = na->ops->alloc(na, param->len);

	nvgpu_posix_enable_fault_injection(kmem_fi, false, 0);
	pa->flags &= ~(param->flags);

	if ((param->expected_zero && (param->ret_addr == 0)) ||
		(!param->expected_zero && (param->ret_addr != 0))) {
		return UNIT_SUCCESS;
	} else {
		unit_return_fail(m, "%s", param->error_msg);
	}
}

int test_page_free(struct unit_module *m, struct gk20a *g, void *args)
{
	struct test_parameters *param = (struct test_parameters *) args;
	struct nvgpu_page_allocator *pa = page_allocator(na);

	pa->flags |= param->flags;
	na->ops->free_alloc(na, param->ret_addr);
	pa->flags &= ~(param->flags);

	return UNIT_SUCCESS;
}

int test_page_alloc_fixed(struct unit_module *m, struct gk20a *g, void *args)
{
	struct test_parameters *param = (struct test_parameters *) args;
	struct nvgpu_page_allocator *pa = page_allocator(na);
	struct nvgpu_posix_fault_inj *kmem_fi =
		nvgpu_kmem_get_fault_injection();

	pa->flags |= param->flags;

	nvgpu_posix_enable_fault_injection(kmem_fi,
					param->fault_enable, param->fault_at);

	/* page_size = SZ_4K ignored in the function */
	param->ret_addr = na->ops->alloc_fixed(na,
						param->base, param->len, SZ_4K);

	nvgpu_posix_enable_fault_injection(kmem_fi, false, 0);
	pa->flags &= ~(param->flags);

	if ((param->expected_zero && (param->ret_addr == 0)) ||
		(!param->expected_zero && (param->ret_addr != 0))) {
		return UNIT_SUCCESS;
	} else {
		unit_return_fail(m, "%s", param->error_msg);
	}
}

int test_page_free_fixed(struct unit_module *m, struct gk20a *g, void *args)
{
	struct test_parameters *param = (struct test_parameters *) args;
	struct nvgpu_page_allocator *pa = page_allocator(na);

	pa->flags |= param->flags;
	na->ops->free_fixed(na, param->ret_addr, param->len);
	pa->flags &= ~(param->flags);

	return UNIT_SUCCESS;
}

int test_page_allocator_init_slabs(struct unit_module *m,
					struct gk20a *g, void *args)
{
	u64 base = SZ_64K;
	u64 length = SZ_128K;
	u64 blk_size = SZ_64K;
	u64 flags = GPU_ALLOC_4K_VIDMEM_PAGES;
	struct nvgpu_posix_fault_inj *kmem_fi =
		nvgpu_kmem_get_fault_injection();

	na = (struct nvgpu_allocator *)
			nvgpu_kzalloc(g, sizeof(struct nvgpu_allocator));
	if (na == NULL) {
		unit_return_fail(m, "Could not allocate nvgpu_allocator\n");
	}

	/* Fault injection at init_slabs */
	nvgpu_posix_enable_fault_injection(kmem_fi, true, 3);
	if (nvgpu_allocator_init(g, na, NULL, "test_page", base, length,
			blk_size, 0ULL, flags, PAGE_ALLOCATOR) == 0) {
		unit_return_fail(m,
			"pa with slabs inited despite fault injection\n");
	}
	nvgpu_posix_enable_fault_injection(kmem_fi, false, 0);

	/*
	 * Expect to fail as blk_size is odd
	 */
	if (nvgpu_allocator_init(g, na, NULL, "test_page", base, length,
			SZ_4K + 1ULL, 0ULL, flags, PAGE_ALLOCATOR) == 0) {
		unit_return_fail(m,
			"vidmem page allocator inited with odd blk_size\n");
	}

	if (nvgpu_allocator_init(g, na, NULL, "test_page", base, length,
			SZ_4K, 0ULL, flags, PAGE_ALLOCATOR) != 0) {
		unit_return_fail(m,
			"vidmem page allocator inited with odd blk_size\n");
	} else {
		na->ops->fini(na);
	}

	/*
	 * Initialize page allocator
	 * This will be used for further tests.
	 */
	if (nvgpu_allocator_init(g, na, NULL, "test_page", base, length,
			blk_size, 0ULL, flags, PAGE_ALLOCATOR) != 0) {
		unit_return_fail(m, "init with slabs failed\n");
	}

	return UNIT_SUCCESS;
}

int test_page_allocator_sgt_ops(struct unit_module *m,
					struct gk20a *g, void *args)
{
	u64 addr;
	void *sgl = NULL;
	struct nvgpu_page_alloc *alloc = NULL;

	addr = na->ops->alloc(na, SZ_32K);
	if (addr == 0) {
		unit_return_fail(m, "couldn't allocate 32K");
	}

	/* Test page allocator sgt ops */
	alloc = (struct nvgpu_page_alloc *) addr;
	sgl = alloc->sgt.sgl;

	if (alloc->sgt.ops->sgl_next(sgl) != NULL) {
		unit_return_fail(m, "sgl_next should be NULL\n");
	}

	if (alloc->sgt.ops->sgl_phys(g, sgl) != alloc->base) {
		unit_return_fail(m, "sgl_phys != base address\n");
	}

	if (alloc->sgt.ops->sgl_ipa(g, sgl) != alloc->base) {
		unit_return_fail(m, "sgl_ipa != base address\n");
	}

	if (alloc->sgt.ops->sgl_dma(sgl) != alloc->base) {
		unit_return_fail(m, "sgl_dma != base address\n");
	}

	if (alloc->sgt.ops->sgl_gpu_addr(g, sgl, NULL) != alloc->base) {
		unit_return_fail(m, "sgl_gpu_addr != base address\n");
	}

	if (alloc->sgt.ops->sgl_ipa_to_pa(g, sgl, SZ_4K, NULL) != SZ_4K) {
		unit_return_fail(m, "sgl_ipa_to_pa != SZ_4K\n");
	}

	if (alloc->sgt.ops->sgl_length(sgl) != SZ_32K) {
		unit_return_fail(m, "sgl_length != SZ_32K\n");
	}

	alloc->sgt.ops->sgt_free(g, &alloc->sgt);

	na->ops->free_alloc(na, addr);

	return UNIT_SUCCESS;
}

int test_nvgpu_page_allocator_ops(struct unit_module *m,
					struct gk20a *g, void *args)
{
	u64 addr;
	int err;
	struct nvgpu_alloc_carveout test_co =
				NVGPU_CARVEOUT("test_co", 0ULL, 0ULL);
	test_co.base =  BA_DEFAULT_BASE;
	test_co.length = SZ_8K;

	if (!na->ops->inited(na)) {
		unit_return_fail(m, "ops not inited\n");
	}

	addr = na->ops->base(na);
	if (addr != BA_DEFAULT_BASE) {
		unit_return_fail(m, "base incorrect\n");
	}

	addr = na->ops->length(na);
	if (addr != BA_DEFAULT_LENGTH) {
		unit_return_fail(m, "length incorrect\n");
	}

	addr = na->ops->end(na);
	if (addr != (BA_DEFAULT_BASE + BA_DEFAULT_LENGTH)) {
		unit_return_fail(m, "end incorrect\n");
	}

	addr = na->ops->space(na);
	if (addr == 0) {
		unit_return_fail(m, "zero space allocated\n");
	}

	err = na->ops->reserve_carveout(na, &test_co);
	if (err < 0) {
		unit_return_fail(m, "couldn't reserve 8K carveout\n");
	}

	na->ops->release_carveout(na, &test_co);

	addr = na->ops->alloc(na, SZ_32K);
	if (addr == 0) {
		unit_return_fail(m, "couldn't allocate 32K");
	}

	err = na->ops->reserve_carveout(na, &test_co);
	if (err == 0) {
		unit_return_fail(m, "reserved carveout after alloc\n");
	}

	na->ops->free_alloc(na, addr);

	return UNIT_SUCCESS;
}

int test_nvgpu_page_allocator_destroy(struct unit_module *m,
					struct gk20a *g, void *args)
{
	na->ops->fini(na);
	if (na->priv != NULL) {
		unit_return_fail(m, "page allocator destroy failed\n");
	}
	nvgpu_kfree(g, na);

	return UNIT_SUCCESS;
}

int test_nvgpu_page_allocator_init(struct unit_module *m,
					struct gk20a *g, void *args)
{
	u64 base = BA_DEFAULT_BASE;
	u64 length = BA_DEFAULT_LENGTH;
	u64 blk_size = BA_DEFAULT_BLK_SIZE;
	u64 flags = 0ULL;
	struct nvgpu_posix_fault_inj *kmem_fi =
		nvgpu_kmem_get_fault_injection();

	na = (struct nvgpu_allocator *)
			nvgpu_kzalloc(g, sizeof(struct nvgpu_allocator));
	if (na == NULL) {
		unit_return_fail(m, "Could not allocate nvgpu_allocator\n");
	}

	/*
	 * expect to fail as blk_size < SZ_4K
	 */
	if (nvgpu_allocator_init(g, na, NULL, "test_page", base, length,
			0ULL, 0ULL, flags, PAGE_ALLOCATOR) == 0) {
		unit_return_fail(m, "inited despite blk_size = 0\n");
	}

	/* Fault injection at nvgpu_page_allocator allocation */
	nvgpu_posix_enable_fault_injection(kmem_fi, true, 0);
	if (nvgpu_allocator_init(g, na, NULL, "test_page", base, length,
			blk_size, 0ULL, flags, PAGE_ALLOCATOR) == 0) {
		unit_return_fail(m,
			"inited despite fault injection at page_allocator\n");
	}
	nvgpu_posix_enable_fault_injection(kmem_fi, false, 0);

	/* Fault injection at alloc_cache */
	nvgpu_posix_enable_fault_injection(kmem_fi, true, 1);
	if (nvgpu_allocator_init(g, na, NULL, "test_page", base, length,
			blk_size, 0ULL, flags, PAGE_ALLOCATOR) == 0) {
		unit_return_fail(m,
			"inited despite fault injection at alloc_cache\n");
	}
	nvgpu_posix_enable_fault_injection(kmem_fi, false, 0);

	/* Fault injection at slab_page_cache */
	nvgpu_posix_enable_fault_injection(kmem_fi, true, 2);
	if (nvgpu_allocator_init(g, na, NULL, "test_page", base, length,
			blk_size, 0ULL, flags, PAGE_ALLOCATOR) == 0) {
		unit_return_fail(m,
			"inited despite fault injection at slab_page_cache\n");
	}
	nvgpu_posix_enable_fault_injection(kmem_fi, false, 0);

	/*
	 * expect to fail as blk_size is odd
	 */
	if (nvgpu_allocator_init(g, na, NULL, "test_page", base, length,
			SZ_4K + 3ULL, 0ULL, flags, PAGE_ALLOCATOR) == 0) {
		unit_return_fail(m, "inited despite odd blk_size\n");
	}

	/* base = 0 */
	if (nvgpu_allocator_init(g, na, NULL, "test_page", 0ULL, length,
			blk_size, 0ULL, flags, PAGE_ALLOCATOR) != 0) {
		unit_return_fail(m, "init failed with base = 0\n");
	} else {
		na->ops->fini(na);
	}

	/*
	 * Initialize page allocator
	 * This will be used for further tests.
	 */
	if (nvgpu_allocator_init(g, na, NULL, "test_page", base, length,
			blk_size, 0ULL, flags, PAGE_ALLOCATOR) != 0) {
		unit_return_fail(m, "init failed\n");
	}

	return UNIT_SUCCESS;
}
#endif

struct unit_module_test page_allocator_tests[] = {
#ifdef CONFIG_NVGPU_DGPU
	/* These tests create and evaluate page_allocator w/o 4K VIDMEM pages */
	UNIT_TEST(init, test_nvgpu_page_allocator_init, NULL, 0),
	UNIT_TEST(ops, test_nvgpu_page_allocator_ops, NULL, 0),
	UNIT_TEST(sgt_ops, test_page_allocator_sgt_ops, NULL, 0),

	/* Below tests examine page allocation */
	/*
	 * NOTE: The test order should not be changed. Previous test develop
	 * memory allocation arrangement required for later tests.
	 */

	/* These tests check execution with fault injection at various locations */
	UNIT_TEST(fixed_alloc_fault_at_alloc_cache, test_page_alloc_fixed, (void *) &fault_at_alloc_cache, 0),
	UNIT_TEST(fixed_alloc_fault_at_sgl_alloc, test_page_alloc_fixed, (void *) &fault_at_sgl_alloc, 0),
	UNIT_TEST(alloc_fault_at_alloc_cache, test_page_alloc, (void *) &fault_at_alloc_cache, 0),
	UNIT_TEST(alloc_fault_at_nvgpu_alloc, test_page_alloc, (void *) &fault_at_nvgpu_alloc, 0),
	/* Alloc some memory, this ensures fault injection at sgl alloc in next test */
	UNIT_TEST(simple_32K_alloc, test_page_alloc, (void *) &first_simple_alloc_32K, 0),
	UNIT_TEST(alloc_fault_at_sgl_alloc, test_page_alloc, (void *) &fault_at_sgl_alloc, 0),

	/* Test different allocation scenarios using simple alloc function */
	UNIT_TEST(alloc_no_scatter_gather, test_page_alloc, (void *) &alloc_no_scatter_gather, 0),
	UNIT_TEST(free_no_scatter_gather, test_page_free, (void *) &alloc_no_scatter_gather, 0),
	/* Second free call checks execution when address is NULL */
	UNIT_TEST(free_no_scatter_gather_again, test_page_free, (void *) &alloc_no_scatter_gather, 0),
	UNIT_TEST(free_32K_alloc, test_page_free, (void *) &first_simple_alloc_32K, 0),
	UNIT_TEST(fixed_alloc_128K, test_page_alloc_fixed, (void *) &simple_alloc_128K, 0),
	/* After previous allocations, contiguous 512K memory isn't available */
	UNIT_TEST(contiguous_alloc_512K, test_page_alloc, (void *) &alloc_contiguous, 0),
	UNIT_TEST(simple_alloc_512K, test_page_alloc, (void *) &simple_alloc_512K, 0),
	UNIT_TEST(alloc_more_than_available, test_page_alloc, (void *) &alloc_more_than_available, 0),
	UNIT_TEST(free_alloc_512K, test_page_free, (void *) &simple_alloc_512K, 0),

	UNIT_TEST(alloc_fixed_no_scatter_gather, test_page_alloc_fixed, (void *) &alloc_no_scatter_gather, 0),
	UNIT_TEST(free_fixed_no_scatter_gather, test_page_free_fixed, (void *) &alloc_no_scatter_gather, 0),
	/* Second free call checks execution when address is NULL */
	UNIT_TEST(free_fixed_no_scatter_gather_again, test_page_free_fixed, (void *) &alloc_no_scatter_gather, 0),
	UNIT_TEST(free_fixed_128K, test_page_free_fixed, (void *) &simple_alloc_128K, 0),

	UNIT_TEST(destroy, test_nvgpu_page_allocator_destroy, NULL, 0),

	/* These tests create and evaluate page_allocator w/ 4K VIDMEM pages */
	UNIT_TEST(init_slabs, test_page_allocator_init_slabs, NULL, 0),

	/* Below tests examine slab allocation */
	/*
	 * NOTE: The test order should not be changed. A test contructs
	 * required memory structure for later tests.
	 */

	/* These tests check execution with fault injection at various locations */
	UNIT_TEST(slabs_fault_at_alloc_cache, test_page_alloc, (void *) &fault_at_alloc_cache, 0),
	UNIT_TEST(slabs_fault_at_sgl_alloc, test_page_alloc, (void *) &fault_at_sgl_alloc, 0),
	UNIT_TEST(slabs_fault_at_page_cache, test_page_alloc, (void *) &fault_at_page_cache, 0),

	/* Test different allocation scenarios */
	UNIT_TEST(add_partial_slab, test_page_alloc, (void *) &first_simple_alloc_32K, 0),
	UNIT_TEST(add_full_slab, test_page_alloc, (void *) &second_simple_alloc_32K, 0),
	UNIT_TEST(add_second_partial_slab, test_page_alloc, (void *) &third_simple_alloc_32K, 0),
	UNIT_TEST(add_second_full_slab, test_page_alloc, (void *) &fourth_simple_alloc_32K, 0),
	/* Note: No free memory available for allocation */
	UNIT_TEST(fixed_alloc_8K, test_page_alloc_fixed, (void *) &failing_alloc_8K, 0),
	/* Freeing allocated slabs, adds slabs to empty and free lists */
	UNIT_TEST(revert_partial_slab, test_page_free, (void *) &fourth_simple_alloc_32K, 0),
	UNIT_TEST(revert_second_partial_slab, test_page_free, (void *) &second_simple_alloc_32K, 0),
	UNIT_TEST(add_empty_slab, test_page_free, (void *) &first_simple_alloc_32K, 0),
	UNIT_TEST(free_slab, test_page_free, (void *) &third_simple_alloc_32K, 0),

	UNIT_TEST(slabs_alloc_8K, test_page_alloc, (void *) &simple_alloc_8K, 0),
	UNIT_TEST(slabs_alloc_32K, test_page_alloc, (void *) &first_simple_alloc_32K, 0),
	/*
	 * Note: Page allocator has only 2 slabs.
	 * These are now allocated for 8K and 32K chunks
	 */
	UNIT_TEST(no_more_slabs, test_page_alloc, (void *) &failing_alloc_16K, 0),

	UNIT_TEST(destroy_slabs, test_nvgpu_page_allocator_destroy, NULL, 0),
#endif
};

UNIT_MODULE(page_allocator, page_allocator_tests, UNIT_PRIO_NVGPU_TEST);
