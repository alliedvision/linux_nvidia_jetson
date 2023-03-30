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

#include <nvgpu/types.h>
#include <nvgpu/sizes.h>
#include <nvgpu/allocator.h>

#include "nvgpu_allocator.h"

#define OP_ALLOC		0
#define OP_FREE			1
#define OP_ALLOC_PTE		2
#define OP_ALLOC_FIXED		3
#define OP_FREE_FIXED		4
#define OP_RESERVE_CARVEOUT	5
#define OP_RELEASE_CARVEOUT	6
#define OP_BASE			7
#define OP_LENGTH		8
#define OP_END			9
#define OP_INITED		10
#define OP_SPACE		11
#define OP_NUMBER		12
static bool dummy_op_called[OP_NUMBER];
static const char *ops_str[] = {
	"alloc",
	"free_alloc",
	"alloc_pte",
	"alloc_fixed",
	"free fixed",
	"reserve_carveout",
	"release_carveout",
	"base",
	"length",
	"end",
	"inited",
	"space",
};

static u64 dummy_alloc(struct nvgpu_allocator *allocator, u64 len)
{
	dummy_op_called[OP_ALLOC] = true;
	return 0ULL;
}

static void dummy_free(struct nvgpu_allocator *allocator, u64 addr)
{
	dummy_op_called[OP_FREE] = true;
}

static u64 dummy_alloc_pte(struct nvgpu_allocator *allocator, u64 len,
			  u32 page_size)
{
	dummy_op_called[OP_ALLOC_PTE] = true;
	return 0ULL;
}

static u64 dummy_alloc_fixed(struct nvgpu_allocator *allocator,
			     u64 base, u64 len, u32 page_size)
{
	dummy_op_called[OP_ALLOC_FIXED] = true;
	return 0ULL;
}

static void dummy_free_fixed(struct nvgpu_allocator *allocator,
			     u64 base, u64 len)
{
	dummy_op_called[OP_FREE_FIXED] = true;
}

static int dummy_reserve_carveout(struct nvgpu_allocator *allocator,
				  struct nvgpu_alloc_carveout *co)
{
	dummy_op_called[OP_RESERVE_CARVEOUT] = true;
	return 0;
}

static void dummy_release_carveout(struct nvgpu_allocator *allocator,
				   struct nvgpu_alloc_carveout *co)
{
	dummy_op_called[OP_RELEASE_CARVEOUT] = true;
}

static u64 dummy_base(struct nvgpu_allocator *allocator)
{
	dummy_op_called[OP_BASE] = true;
	return 0ULL;
}

static u64 dummy_length(struct nvgpu_allocator *allocator)
{
	dummy_op_called[OP_LENGTH] = true;
	return 0ULL;
}

static u64 dummy_end(struct nvgpu_allocator *allocator)
{
	dummy_op_called[OP_END] = true;
	return 0ULL;
}

static bool dummy_inited(struct nvgpu_allocator *allocator)
{
	dummy_op_called[OP_INITED] = true;
	return false;
}

static u64 dummy_space(struct nvgpu_allocator *allocator)
{
	dummy_op_called[OP_SPACE] = true;
	return false;
}

static void dummy_fini(struct nvgpu_allocator *allocator)
{
}

static struct nvgpu_allocator_ops dummy_ops = {
	.alloc            = dummy_alloc,
	.free_alloc       = dummy_free,
	.alloc_pte        = dummy_alloc_pte,
	.alloc_fixed      = dummy_alloc_fixed,
	.free_fixed       = dummy_free_fixed,
	.reserve_carveout = dummy_reserve_carveout,
	.release_carveout = dummy_release_carveout,
	.base             = dummy_base,
	.length           = dummy_length,
	.end              = dummy_end,
	.inited           = dummy_inited,
	.space            = dummy_space,
	.fini             = dummy_fini
};

int test_nvgpu_alloc_ops_present(struct unit_module *m,
					struct gk20a *g, void *args)
{
	u32 i;
	int err;
	bool failed;
	struct nvgpu_allocator a;

	memset(dummy_op_called, 0, sizeof(dummy_op_called));

	err = nvgpu_alloc_common_init(&a, NULL, "test",
				      NULL, false, &dummy_ops);
	if (err)
		unit_return_fail(m, "Unexpected common_init() fail!\n");

	/*
	 * Now that we have the allocator just call all the alloc functions and
	 * make sure that the associated bool is true.
	 */
	nvgpu_alloc(&a, 0UL);
	nvgpu_alloc_pte(&a, 0UL, 0U);
	nvgpu_alloc_fixed(&a, 0UL, 0UL, 0U);
	nvgpu_free(&a, 0UL);
	nvgpu_free_fixed(&a, 0UL, 0UL);

	nvgpu_alloc_reserve_carveout(&a, NULL);
	nvgpu_alloc_release_carveout(&a, NULL);

	nvgpu_alloc_base(&a);
	nvgpu_alloc_length(&a);
	nvgpu_alloc_end(&a);
	nvgpu_alloc_initialized(&a);
	nvgpu_alloc_space(&a);

	failed = false;
	for (i = 0; i < OP_NUMBER; i++) {
		if (!dummy_op_called[i]) {
			failed = true;
			unit_info(m, "%s did not call op function!\n",
				  ops_str[i]);
		}
	}

	if (failed)
		unit_return_fail(m, "OPs uncalled!\n");

	/*
	 * Next make sure that if the ops are NULL we don't crash or anything
	 * like that.
	 *
	 * Note that not all ops have if NULL checks. We skip these in the unit
	 * test.
	 */
	memset(dummy_op_called, 0, sizeof(dummy_op_called));
	memset(&dummy_ops, 0, sizeof(dummy_ops));

	nvgpu_alloc_fixed(&a, 0UL, 0UL, 0U);
	nvgpu_free_fixed(&a, 0UL, 0UL);

	nvgpu_alloc_reserve_carveout(&a, NULL);
	nvgpu_alloc_release_carveout(&a, NULL);

	nvgpu_alloc_base(&a);
	nvgpu_alloc_length(&a);
	nvgpu_alloc_end(&a);
	nvgpu_alloc_initialized(&a);
	nvgpu_alloc_space(&a);

	failed = false;
	for (i = 0; i < OP_NUMBER; i++) {
		if (dummy_op_called[i]) {
			failed = true;
			unit_info(m, "op function %s called despite null op!\n",
				  ops_str[i]);
		}
	}

	if (failed)
		unit_return_fail(m, "OPs called!\n");

	return UNIT_SUCCESS;
}

int test_nvgpu_alloc_common_init(struct unit_module *m,
					struct gk20a *g, void *args)
{
	struct nvgpu_allocator a;
	struct nvgpu_allocator_ops ops = { };
	void *dummy_priv = (void *)0x10000;
	void *dummy_g = (void *)0x1000;

	if (nvgpu_alloc_common_init(NULL, NULL, NULL, NULL, false, NULL) == 0)
		unit_return_fail(m, "Made NULL allocator!?\n");

	/*
	 * Hit all the invalid ops struct criteria.
	 */
	if (nvgpu_alloc_common_init(&a, NULL, "test", NULL, false, &ops) == 0)
		unit_return_fail(m, "common_init passes despite empty ops\n");

	ops.alloc = dummy_alloc;
	if (nvgpu_alloc_common_init(&a, NULL, "test", NULL, false, &ops) == 0)
		unit_return_fail(m,
			"common_init passes despite missing free(),fini()\n");

	ops.free_alloc = dummy_free;
	if (nvgpu_alloc_common_init(&a, NULL, "test", NULL, false, &ops) == 0)
		unit_return_fail(m,
			"common_init passes despite missing fini()\n");

	ops.fini = dummy_fini;
	if (0 != nvgpu_alloc_common_init(&a, dummy_g, "test",
					 dummy_priv, true, &ops))
		unit_return_fail(m, "common_init should have passed\n");

	/*
	 * Verify that the allocator struct actually is made correctly.
	 */
	if (a.g != dummy_g  || a.priv != dummy_priv ||
	    a.debug != true || a.ops != &ops)
		unit_return_fail(m, "Invalid data in allocator\n");

	if (strcmp(a.name, "test") != 0)
		unit_return_fail(m, "Invalid name in allocator\n");

	return UNIT_SUCCESS;
}

int test_nvgpu_alloc_destroy(struct unit_module *m,
					struct gk20a *g, void *args)
{
	struct nvgpu_allocator a;
	struct nvgpu_allocator zero_a = { };
	struct nvgpu_allocator_ops ops = {
		.alloc = dummy_alloc,
		.free_alloc = dummy_free,
		.fini = dummy_fini,
	};


	if (nvgpu_alloc_common_init(&a, NULL, "test", NULL, false, &ops) != 0)
		unit_return_fail(m, "common_init failed with valid input\n");

	nvgpu_alloc_destroy(&a);


	if (memcmp(&a, &zero_a, sizeof(a)) != 0)
		unit_return_fail(m, "Allocator has not been memset to 0\n");

	return UNIT_SUCCESS;
}

int test_nvgpu_allocator_init(struct unit_module *m,
						struct gk20a *g, void *args)
{
	struct nvgpu_allocator a;
	u64 base = SZ_4K;
	u64 size = SZ_64K;
	u64 blk_size = SZ_4K;
	u64 max_order = 0;
	u64 flags = 0ULL;

	if (nvgpu_allocator_init(g, &a, NULL, "buddy", base, size, blk_size,
				max_order, flags, BUDDY_ALLOCATOR) != 0) {
		unit_return_fail(m, "failed to init buddy_allocator\n");
	} else {
		a.ops->fini(&a);
	}

#ifdef CONFIG_NVGPU_DGPU
	if (nvgpu_allocator_init(g, &a, NULL, "page", base, size, blk_size,
				max_order, flags, PAGE_ALLOCATOR) != 0) {
		unit_return_fail(m, "failed to init page_allocator\n");
	} else {
		a.ops->fini(&a);
	}
#endif

	if (nvgpu_allocator_init(g, &a, NULL, "bitmap", base, size, blk_size,
				max_order, flags, BITMAP_ALLOCATOR) != 0) {
		unit_return_fail(m, "failed to init bitmap_allocator\n");
	} else {
		a.ops->fini(&a);
	}

	/* Initialize invalid allocator */
	if (nvgpu_allocator_init(g, &a, NULL, "invalid", base, size, blk_size,
				max_order, flags, -1) != -EINVAL) {
		unit_return_fail(m, "initialized invalid allocator\n");
	}

	return UNIT_SUCCESS;
}

struct unit_module_test nvgpu_allocator_tests[] = {
	UNIT_TEST(common_init,      test_nvgpu_alloc_common_init,  NULL, 0),
	UNIT_TEST(alloc_destroy,    test_nvgpu_alloc_destroy,      NULL, 0),
	UNIT_TEST(alloc_ops,        test_nvgpu_alloc_ops_present,  NULL, 0),
	UNIT_TEST(allocator_init,   test_nvgpu_allocator_init,     NULL, 0),
};

UNIT_MODULE(nvgpu_allocator, nvgpu_allocator_tests, UNIT_PRIO_NVGPU_TEST);
