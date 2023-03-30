/*
 * Copyright (c) 2019, NVIDIA CORPORATION.  All rights reserved.
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

#include <nvgpu/sizes.h>
#include <nvgpu/types.h>
#include <nvgpu/allocator.h>
#include <nvgpu/posix/kmem.h>
#include <nvgpu/posix/posix-fault-injection.h>

#include "common/mm/allocators/bitmap_allocator_priv.h"
#include "bitmap_allocator.h"

#define BA_DEFAULT_BASE		SZ_1K
#define BA_DEFAULT_LENGTH	(SZ_64K << 1)
#define BA_DEFAULT_BLK_SIZE	SZ_1K
#define SZ_2K			(SZ_1K << 1)
#define SZ_8K			(SZ_4K << 1)
#define SZ_16K			(SZ_4K << 2)
#define SZ_32K			(SZ_64K >> 1)

static struct nvgpu_allocator *na;

int test_nvgpu_bitmap_allocator_critical(struct unit_module *m,
					struct gk20a *g, void *args)
{
	u64 base = BA_DEFAULT_BASE;
	u64 length = BA_DEFAULT_LENGTH;
	u64 blk_size = BA_DEFAULT_BLK_SIZE;
	u64 flags = GPU_ALLOC_NO_ALLOC_PAGE;
	u64 addr, addr1;

	na = (struct nvgpu_allocator *)
			nvgpu_kzalloc(g, sizeof(struct nvgpu_allocator));
	if (na == NULL) {
		unit_return_fail(m, "Could not allocate nvgpu_allocator\n");
	}

	if (nvgpu_allocator_init(g, na, NULL, "test_bitmap", base, length,
				blk_size, 0ULL, flags, BITMAP_ALLOCATOR) != 0) {
		nvgpu_kfree(g, na);
		unit_return_fail(m, "bitmap_allocator init failed\n");
	}

	addr1 = na->ops->alloc(na, SZ_2K);
	if (addr1 == 0) {
		unit_err(m, "%d: couldn't allocate 2K bits\n", __LINE__);
		goto fail;
	}

	addr = na->ops->alloc_fixed(na, SZ_4K, SZ_8K, SZ_1K);
	if (addr == 0) {
		unit_err(m, "%d: alloc_fixed failed to allocate 8K\n",
			__LINE__);
		goto fail;
	}

	/*
	 * Alloate 0 bytes at 64K
	 * Note: 0 bytes are actually allocated. But error handling should
	 * be done by the user.
	 */
	addr = na->ops->alloc_fixed(na, SZ_64K, 0ULL, SZ_1K);
	if (addr == 0) {
		unit_err(m, "%d: alloc_fixed couldn't alloc 0 bytes at 64K\n",
			__LINE__);
		goto fail;
	}

	addr1 = na->ops->alloc(na, SZ_2K + 4);
	if (addr1 == 0) {
		unit_err(m, "%d: alloc failed to allocate 2052 bits\n",
			__LINE__);
		goto fail;
	}

	na->ops->free_alloc(na, addr1);

	na->ops->free_fixed(na, SZ_4K, SZ_8K);

	na->ops->fini(na);
	nvgpu_kfree(g, na);

	return UNIT_SUCCESS;

fail:
	na->ops->fini(na);
	nvgpu_kfree(g, na);

	return UNIT_FAIL;

}

int test_nvgpu_bitmap_allocator_alloc(struct unit_module *m,
					struct gk20a *g, void *args)
{
	u64 alloc0, alloc3k, alloc4k, alloc_at64, addr, addr_fail;
	struct nvgpu_posix_fault_inj *kmem_fi =
		nvgpu_kmem_get_fault_injection();

	/*
	 * len = 0
	 * Expect to fail
	 */
	alloc0 = na->ops->alloc(na, 0);
	if (alloc0 != 0) {
		unit_err(m, "ops->alloc allocated with len = 0\n");
	}


	alloc3k = na->ops->alloc(na, SZ_2K + 4);
	if (alloc3k == 0) {
		unit_return_fail(m, "couldn't allocate 2052 bits\n");
	}

	/*
	 * 2M is more than available for bitmap
	 * Expect to fail
	 */
	addr_fail = na->ops->alloc(na, (SZ_1M << 1));
	if (addr_fail != 0) {
		unit_return_fail(m,
			"bitmap allocated more than available memory\n");
	}

	/* Fault injection at nvgpu_bitmap_store_alloc */
	nvgpu_posix_enable_fault_injection(kmem_fi, true, 0);
	addr_fail = na->ops->alloc(na, (SZ_1K << 1));
	if (addr_fail != 0) {
		unit_return_fail(m,
			"ops->alloc allocated despite fault injection\n");
	}
	nvgpu_posix_enable_fault_injection(kmem_fi, false, 0);

	na->ops->free_alloc(na, alloc3k);

	na->ops->free_alloc(na, alloc3k);

	alloc4k = na->ops->alloc(na, SZ_4K);
	if (alloc4k == 0) {
		unit_return_fail(m, "bitmap couldn't allocate 4K");
	}

	addr = na->ops->alloc(na, SZ_8K);
	if (addr == 0) {
		unit_return_fail(m, "bitmap couldn't allocate 8K");
	}

	addr = na->ops->alloc(na, SZ_16K);
	if (addr == 0) {
		unit_return_fail(m, "bitmap couldn't allocate 16K");
	}

	addr = na->ops->alloc(na, SZ_32K);
	if (addr == 0) {
		unit_return_fail(m, "bitmap couldn't allocate 32K");
	}

	/*
	 * Requesting at allocated base address
	 * Expect to fail
	 */
	addr_fail = na->ops->alloc_fixed(na, alloc4k, SZ_4K, SZ_1K);
	if (addr_fail != 0) {
		unit_return_fail(m,
			"allocated at already occupied address\n");
	}

	/*
	 * Unaligned base
	 * Expect to fail
	 */
	addr_fail = na->ops->alloc_fixed(na, (SZ_64K + 1ULL), SZ_4K, SZ_1K);
	if (addr_fail != 0) {
		unit_return_fail(m,
			"ops->alloc_fixed allocated with unaligned base\n");
	}

	alloc_at64 = na->ops->alloc_fixed(na, SZ_64K, (SZ_64K - 1ULL), SZ_1K);
	if (alloc_at64 == 0) {
		unit_return_fail(m,
			"ops->alloc_fixed failed to allocate 4097 bits\n");
	}

	/*
	 * Unaligned base
	 * Expect to fail
	 */
	if (!EXPECT_BUG(na->ops->free_fixed(na, (SZ_64K + 1ULL), SZ_4K))) {
		unit_return_fail(m,
			"freeing unaligned base didn't trigger BUG()\n");
	}

	na->ops->free_alloc(na, alloc4k);

	/*
	 * Allocate 4K
	 * This allocation will require the bitmap allocator to find available
	 * space before next_blk.
	 */
	alloc4k = na->ops->alloc(na, SZ_4K);
	if (alloc4k == 0) {
		unit_return_fail(m, "bitmap couldn't allocate 4K");
	}

	na->ops->free_fixed(na, alloc_at64, (SZ_4K + 1ULL));

	return UNIT_SUCCESS;
}

int test_nvgpu_bitmap_allocator_ops(struct unit_module *m,
					struct gk20a *g, void *args)
{
	u64 addr;

	if (!na->ops->inited(na)) {
		unit_return_fail(m, "bitmap ops->inited incorrect\n");
	}

	addr = na->ops->base(na);
	if (addr != BA_DEFAULT_BASE) {
		unit_return_fail(m, "bitmap ops->base incorrect\n");
	}

	addr = na->ops->length(na);
	if (addr != BA_DEFAULT_LENGTH) {
		unit_return_fail(m, "bitmap ops->length incorrect\n");
	}

	addr = na->ops->end(na);
	if (addr != (BA_DEFAULT_BASE + BA_DEFAULT_LENGTH)) {
		unit_return_fail(m, "bitmap ops->end incorrect\n");
	}

	return UNIT_SUCCESS;
}

int test_nvgpu_bitmap_allocator_destroy(struct unit_module *m,
					struct gk20a *g, void *args)
{
	na->ops->fini(na);
	nvgpu_kfree(g, na);

	return UNIT_SUCCESS;
}

int test_nvgpu_bitmap_allocator_init(struct unit_module *m,
					struct gk20a *g, void *args)
{
	u64 base = BA_DEFAULT_BASE;
	u64 length = BA_DEFAULT_LENGTH;
	u64 blk_size = BA_DEFAULT_BLK_SIZE;
	u64 flags = 0ULL;
	struct nvgpu_bitmap_allocator *ba;
	struct nvgpu_posix_fault_inj *kmem_fi =
		nvgpu_kmem_get_fault_injection();

	na = (struct nvgpu_allocator *)
			nvgpu_kzalloc(g, sizeof(struct nvgpu_allocator));
	if (na == NULL) {
		unit_return_fail(m, "Could not allocate nvgpu_allocator\n");
	}

	/* base = 0, length = 0, blk_size = 0 */
	if (!EXPECT_BUG(nvgpu_allocator_init(g, na, NULL, "test_bitmap", 0ULL,
				0ULL, 0ULL, 0ULL, flags, BITMAP_ALLOCATOR))) {
		na->ops->fini(na);
		unit_return_fail(m,
			"bitmap inited despite blk_size = base = length = 0\n");
	}

	/*
	 * blk_size = 0
	 * Since base and length are not aligned with 0, init fails
	 */
	if (!EXPECT_BUG(nvgpu_allocator_init(g, na, NULL, "test_bitmap", base,
				length, 0ULL, 0ULL, flags, BITMAP_ALLOCATOR))) {
		unit_return_fail(m, "bitmap inited despite blk_size=0\n");
	}

	/* Odd blk_size */
	if (!EXPECT_BUG(nvgpu_allocator_init(g, na, NULL, "test_bitmap", base,
				length, 3ULL, 0ULL, flags, BITMAP_ALLOCATOR))) {
		unit_return_fail(m, "bitmap inited despite odd blk_size\n");
	}

	/* length unaligned */
	if (nvgpu_allocator_init(g, na, NULL, "test_bitmap", base, 0x0010,
				blk_size, 0ULL, flags, BITMAP_ALLOCATOR) == 0) {
		unit_return_fail(m, "bitmap init despite unaligned length\n");
	}

	/* base unaligned */
	if (nvgpu_allocator_init(g, na, NULL, "test_bitmap", 0x0100, length,
				blk_size, 0ULL, flags, BITMAP_ALLOCATOR) == 0) {
		unit_return_fail(m, "bitmap init despite unaligned base\n");
	}

	/* base = 0 */
	if (nvgpu_allocator_init(g, na, NULL, "test_bitmap", 0ULL, length,
				blk_size, 0ULL, flags, BITMAP_ALLOCATOR) != 0) {
		unit_return_fail(m, "bitmap init failed with base = 0\n");
	} else {
		ba = na->priv;
		if (ba->base != ba->blk_size) {
			na->ops->fini(na);
			unit_return_fail(m, "bitmap init with base=0 "
				"didn't update base = blk_size\n");
		}
		ba = NULL;
		na->ops->fini(na);
	}

	/* length = 0 */
	if (nvgpu_allocator_init(g, na, NULL, "test_bitmap", 0ULL, 0ULL,
				blk_size, 0ULL, flags, BITMAP_ALLOCATOR) == 0) {
		unit_return_fail(m, "bitmap inited with length = 0\n");
	}

	/* Fault injection at nvgpu_bitmap_allocator alloc */
	nvgpu_posix_enable_fault_injection(kmem_fi, true, 0);
	if (nvgpu_allocator_init(g, na, NULL, "test_bitmap", base, length,
				blk_size, 0ULL, flags, BITMAP_ALLOCATOR) == 0) {
		unit_return_fail(m, "bitmap inited despite "
			"fault injection at nvgpu_bitmap_allocator alloc\n");
	}
	nvgpu_posix_enable_fault_injection(kmem_fi, false, 0);

	/* Fault injection at meta_data_cache create */
	nvgpu_posix_enable_fault_injection(kmem_fi, true, 1);
	if (nvgpu_allocator_init(g, na, NULL, "test_bitmap", base, length,
				blk_size, 0ULL, flags, BITMAP_ALLOCATOR) == 0) {
		unit_return_fail(m, "bitmap inited despite "
			"fault injection at meta_data_cache\n");
	}
	nvgpu_posix_enable_fault_injection(kmem_fi, false, 0);

	/* Fault injection at bitmap create */
	nvgpu_posix_enable_fault_injection(kmem_fi, true, 2);
	if (nvgpu_allocator_init(g, na, NULL, "test_bitmap", base, length,
				blk_size, 0ULL, flags, BITMAP_ALLOCATOR) == 0) {
		unit_return_fail(m, "bitmap inited despite "
			"fault injection at bitmap create\n");
	}
	nvgpu_posix_enable_fault_injection(kmem_fi, false, 0);

	/*
	 * Initialize bitmap allocator
	 * This ba will be used for further tests.
	 */
	if (nvgpu_allocator_init(g, na, NULL, "test_bitmap", base, length,
				blk_size, 0ULL, flags, BITMAP_ALLOCATOR) != 0) {
		unit_return_fail(m, "bitmap_allocator init failed\n");
	}

	return UNIT_SUCCESS;
}

struct unit_module_test bitmap_allocator_tests[] = {

	/* BA initialized in this test is used by next tests */
	UNIT_TEST(init, test_nvgpu_bitmap_allocator_init, NULL, 0),

	/* These tests use bitmap allocator created in the first test */
	UNIT_TEST(ops, test_nvgpu_bitmap_allocator_ops, NULL, 0),
	UNIT_TEST(alloc, test_nvgpu_bitmap_allocator_alloc, NULL, 0),
	UNIT_TEST(free, test_nvgpu_bitmap_allocator_destroy, NULL, 0),

	/* Tests GPU_ALLOC_NO_ALLOC_PAGE operations by bitmap allocator */
	UNIT_TEST(critical, test_nvgpu_bitmap_allocator_critical, NULL, 0),
};

UNIT_MODULE(bitmap_allocator, bitmap_allocator_tests, UNIT_PRIO_NVGPU_TEST);
