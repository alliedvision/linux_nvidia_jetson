/*
 * Copyright (c) 2016-2021, NVIDIA CORPORATION.  All rights reserved.
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

#include <nvgpu/bitops.h>
#include <nvgpu/allocator.h>
#include <nvgpu/kmem.h>
#include <nvgpu/bug.h>
#include <nvgpu/barrier.h>

#include "bitmap_allocator_priv.h"

static u64 nvgpu_bitmap_alloc_length(struct nvgpu_allocator *a)
{
	struct nvgpu_bitmap_allocator *ba = a->priv;

	return ba->length;
}

static u64 nvgpu_bitmap_alloc_base(struct nvgpu_allocator *a)
{
	struct nvgpu_bitmap_allocator *ba = a->priv;

	return ba->base;
}

static bool nvgpu_bitmap_alloc_inited(struct nvgpu_allocator *a)
{
	struct nvgpu_bitmap_allocator *ba = a->priv;
	bool inited = ba->inited;

	nvgpu_smp_rmb();
	return inited;
}

static u64 nvgpu_bitmap_alloc_end(struct nvgpu_allocator *a)
{
	struct nvgpu_bitmap_allocator *ba = a->priv;

	return nvgpu_safe_add_u64(ba->base, ba->length);
}

/*
 * @page_size is ignored.
 */
static u64 nvgpu_bitmap_balloc_fixed(struct nvgpu_allocator *na,
				    u64 base, u64 len, u32 page_size)
{
	struct nvgpu_bitmap_allocator *a = bitmap_allocator(na);
	u64 blks, offs, ret;

	/* Compute the bit offset and make sure it's aligned to a block.  */
	offs = base >> a->blk_shift;
	if (nvgpu_safe_mult_u64(offs, a->blk_size) != base) {
		return 0;
	}

	offs = nvgpu_safe_sub_u64(offs, a->bit_offs);

	blks = len >> a->blk_shift;
	if (nvgpu_safe_mult_u64(blks, a->blk_size) != len) {
		blks++;
	}
	nvgpu_assert(blks <= U32_MAX);

	alloc_lock(na);

	/* Check if the space requested is already occupied. */
	ret = bitmap_find_next_zero_area(a->bitmap, a->num_bits, offs,
					(u32)blks, 0UL);
	if (ret != offs) {
		goto fail;
	}

	nvgpu_assert(blks <= U32_MAX);
	nvgpu_bitmap_set(a->bitmap, (u32)offs, U32(blks));

	a->bytes_alloced = nvgpu_safe_add_u64(a->bytes_alloced,
				nvgpu_safe_mult_u64(blks, a->blk_size));
NVGPU_COV_WHITELIST(false_positive, NVGPU_MISRA(Rule, 14_3), "Bug 2615925")
	nvgpu_assert(a->nr_fixed_allocs < U64_MAX);
	a->nr_fixed_allocs++;
	alloc_unlock(na);

	alloc_dbg(na, "Alloc-fixed 0x%-10llx 0x%-5llx [bits=0x%llx (%llu)]",
		  base, len, blks, blks);
	return base;

fail:
	alloc_unlock(na);
	alloc_dbg(na, "Alloc-fixed failed! (0x%llx)", base);
	return 0;
}

/*
 * Two possibilities for this function: either we are freeing a fixed allocation
 * or we are freeing a regular alloc but with GPU_ALLOC_NO_ALLOC_PAGE defined.
 *
 * Note: this function won't do much error checking. Thus you could really
 * confuse the allocator if you misuse this function.
 */
static void nvgpu_bitmap_free_fixed(struct nvgpu_allocator *na,
				    u64 base, u64 len)
{
	struct nvgpu_bitmap_allocator *a = bitmap_allocator(na);
	u64 blks, offs;

	offs = base >> a->blk_shift;
	if (nvgpu_safe_mult_u64(offs, a->blk_size) != base) {
		nvgpu_do_assert();
		return;
	}

	offs = nvgpu_safe_sub_u64(offs, a->bit_offs);

	blks = len >> a->blk_shift;
	if (nvgpu_safe_mult_u64(blks, a->blk_size) != len) {
		blks++;
	}

	alloc_lock(na);
	nvgpu_assert(offs <= U32_MAX);
	nvgpu_assert(blks <= (u32)INT_MAX);
	nvgpu_bitmap_clear(a->bitmap, (u32)offs, (u32)blks);
	a->bytes_freed = nvgpu_safe_add_u64(a->bytes_freed,
				nvgpu_safe_mult_u64(blks, a->blk_size));
	alloc_unlock(na);

	alloc_dbg(na, "Free-fixed 0x%-10llx 0x%-5llx [bits=0x%llx (%llu)]",
		  base, len, blks, blks);
}

/*
 * Add the passed alloc to the tree of stored allocations.
 */
static void insert_alloc_metadata(struct nvgpu_bitmap_allocator *a,
				  struct nvgpu_bitmap_alloc *alloc)
{
	alloc->alloc_entry.key_start = alloc->base;
	alloc->alloc_entry.key_end = nvgpu_safe_add_u64(alloc->base,
							alloc->length);

	nvgpu_rbtree_insert(&alloc->alloc_entry, &a->allocs);
}

/*
 * Find and remove meta-data from the outstanding allocations.
 */
static struct nvgpu_bitmap_alloc *find_alloc_metadata(
	struct nvgpu_bitmap_allocator *a, u64 addr)
{
	struct nvgpu_bitmap_alloc *alloc;
	struct nvgpu_rbtree_node *node = NULL;

	nvgpu_rbtree_search(addr, &node, a->allocs);
	if (node == NULL) {
		return NULL;
	}

	alloc = nvgpu_bitmap_alloc_from_rbtree_node(node);

	nvgpu_rbtree_unlink(node, &a->allocs);

	return alloc;
}

/*
 * Tree of alloc meta data stores the address of the alloc not the bit offset.
 */
static int nvgpu_bitmap_store_alloc(struct nvgpu_bitmap_allocator *a,
				      u64 addr, u64 len)
{
	struct nvgpu_bitmap_alloc *alloc =
		nvgpu_kmem_cache_alloc(a->meta_data_cache);

	if (alloc == NULL) {
		return -ENOMEM;
	}

	alloc->base = addr;
	alloc->length = len;

	insert_alloc_metadata(a, alloc);

	return 0;
}

/*
 * @len is in bytes. This routine will figure out the right number of bits to
 * actually allocate. The return is the address in bytes as well.
 *
 * This is a find-first-fit allocator.
 * Check the input parameter validity.
 * Acquire the alloc_lock.
 * Searche a bitmap for the first space that is large enough to satisfy the
 *  requested size of bits by walking the next available free blocks by
 *  bitmap_find_next_zero_area().
 * Release the alloc_lock.
 */
static u64 nvgpu_bitmap_balloc(struct nvgpu_allocator *na, u64 len)
{
	u64 tmp_u64, addr;
	u32 blks;
	unsigned long offs, adjusted_offs, limit;
	struct nvgpu_bitmap_allocator *a = bitmap_allocator(na);

	if (len == 0ULL) {
		alloc_dbg(na, "len = 0, Alloc failed!");
		return 0;
	}

	tmp_u64 = len >> a->blk_shift;
	nvgpu_assert(tmp_u64 <= U32_MAX);
	blks = (u32)tmp_u64;

	if (nvgpu_safe_mult_u64(blks, a->blk_size) != len) {
		blks++;
	}

	alloc_lock(na);

	/*
	 * First look from next_blk and onwards...
	 */
	offs = bitmap_find_next_zero_area(a->bitmap, a->num_bits,
					  a->next_blk, blks, 0);
	if (offs >= a->num_bits) {
		/*
		 * If that didn't work try the remaining area. Since there can
		 * be available space that spans across a->next_blk we need to
		 * search up to the first set bit after that.
		 */
		limit = find_next_bit(a->bitmap, a->num_bits, a->next_blk);
		offs = bitmap_find_next_zero_area(a->bitmap, limit,
						  0, blks, 0);
		if (offs >= a->next_blk) {
			goto fail;
		}
	}

	nvgpu_assert(offs <= U32_MAX);
	nvgpu_bitmap_set(a->bitmap, (u32)offs, blks);
	a->next_blk = offs + blks;

	adjusted_offs = nvgpu_safe_add_u64(offs, a->bit_offs);
	addr = nvgpu_safe_mult_u64(((u64)adjusted_offs), a->blk_size);

	/*
	 * Only do meta-data storage if we are allowed to allocate storage for
	 * that meta-data. The issue with using malloc and friends is that
	 * in latency and success critical paths an alloc_page() call can either
	 * sleep for potentially a long time or fail. Since we might not want
	 * either of these possibilities assume that the caller will keep what
	 * data it needs around to successfully free this allocation.
	 */
	if ((a->flags & GPU_ALLOC_NO_ALLOC_PAGE) == 0ULL) {
		if (nvgpu_bitmap_store_alloc(a, addr,
						blks * a->blk_size) != 0) {
			goto fail_reset_bitmap;
		}
	}

	alloc_dbg(na, "Alloc 0x%-10llx 0x%-5llx [bits=0x%x (%u)]",
		  addr, len, blks, blks);

NVGPU_COV_WHITELIST(false_positive, NVGPU_MISRA(Rule, 14_3), "Bug 2615925")
	nvgpu_assert(a->nr_allocs < U64_MAX);
	a->nr_allocs++;
	a->bytes_alloced = nvgpu_safe_add_u64(a->bytes_alloced,
				nvgpu_safe_mult_u64(blks, a->blk_size));
	alloc_unlock(na);

	return addr;

fail_reset_bitmap:
	nvgpu_assert(blks <= (u32)INT_MAX);
	nvgpu_assert(offs <= U32_MAX);
	nvgpu_bitmap_clear(a->bitmap, (u32)offs, blks);
fail:
	a->next_blk = 0;
	alloc_unlock(na);
	alloc_dbg(na, "Alloc failed!");
	return 0;
}

static void nvgpu_bitmap_free(struct nvgpu_allocator *na, u64 addr)
{
	struct nvgpu_bitmap_allocator *a = bitmap_allocator(na);
	struct nvgpu_bitmap_alloc *alloc = NULL;
	u64 offs, adjusted_offs, blks;

	alloc_lock(na);

	if ((a->flags & GPU_ALLOC_NO_ALLOC_PAGE) != 0ULL) {
		(void) WARN(true,
			"Using wrong free for NO_ALLOC_PAGE bitmap allocator");
		goto done;
	}

	alloc = find_alloc_metadata(a, addr);
	if (alloc == NULL) {
		goto done;
	}

	/*
	 * Address comes from adjusted offset (i.e the bit offset with
	 * a->bit_offs added. So start with that and then work out the real
	 * offs into the bitmap.
	 */
	adjusted_offs = addr >> a->blk_shift;
	offs = adjusted_offs - a->bit_offs;
	blks = alloc->length >> a->blk_shift;

	nvgpu_assert(blks <= (u32)INT_MAX);
	nvgpu_assert(offs <= U32_MAX);
	nvgpu_bitmap_clear(a->bitmap, (u32)offs, (u32)blks);
	alloc_dbg(na, "Free  0x%-10llx", addr);

	a->bytes_freed = nvgpu_safe_add_u64(a->bytes_freed, alloc->length);

done:
	if ((a->meta_data_cache != NULL) && (alloc != NULL)) {
		nvgpu_kmem_cache_free(a->meta_data_cache, alloc);
	}
	alloc_unlock(na);
}

static void nvgpu_bitmap_alloc_destroy(struct nvgpu_allocator *na)
{
	struct nvgpu_bitmap_allocator *a = bitmap_allocator(na);
	struct nvgpu_bitmap_alloc *alloc;
	struct nvgpu_rbtree_node *node = NULL;

	/*
	 * Kill any outstanding allocations.
	 */
	nvgpu_rbtree_enum_start(0, &node, a->allocs);
	while (node != NULL) {
		alloc = nvgpu_bitmap_alloc_from_rbtree_node(node);

		nvgpu_rbtree_unlink(node, &a->allocs);
		nvgpu_kmem_cache_free(a->meta_data_cache, alloc);

		nvgpu_rbtree_enum_start(0, &node, a->allocs);
	}

	nvgpu_kmem_cache_destroy(a->meta_data_cache);
	nvgpu_kfree(nvgpu_alloc_to_gpu(na), a->bitmap);
	nvgpu_kfree(nvgpu_alloc_to_gpu(na), a);
}

#ifdef __KERNEL__
static void nvgpu_bitmap_print_stats(struct nvgpu_allocator *na,
				     struct seq_file *s, int lock)
{
	struct nvgpu_bitmap_allocator *a = bitmap_allocator(na);

	alloc_pstat(s, na, "Bitmap allocator params:");
	alloc_pstat(s, na, "  start = 0x%llx", a->base);
	alloc_pstat(s, na, "  end   = 0x%llx", a->base + a->length);
	alloc_pstat(s, na, "  blks  = 0x%llx", a->num_bits);

	/* Actual stats. */
	alloc_pstat(s, na, "Stats:");
	alloc_pstat(s, na, "  Number allocs = 0x%llx", a->nr_allocs);
	alloc_pstat(s, na, "  Number fixed  = 0x%llx", a->nr_fixed_allocs);
	alloc_pstat(s, na, "  Bytes alloced = 0x%llx", a->bytes_alloced);
	alloc_pstat(s, na, "  Bytes freed   = 0x%llx", a->bytes_freed);
	alloc_pstat(s, na, "  Outstanding   = 0x%llx",
		      a->bytes_alloced - a->bytes_freed);
}
#endif

NVGPU_COV_WHITELIST_BLOCK_BEGIN(false_positive, 1, NVGPU_MISRA(Rule, 8_7), "Bug 2823817")
static const struct nvgpu_allocator_ops bitmap_ops = {
NVGPU_COV_WHITELIST_BLOCK_END(NVGPU_MISRA(Rule, 8_7))
	.alloc		= nvgpu_bitmap_balloc,
	.free_alloc	= nvgpu_bitmap_free,

	.alloc_fixed	= nvgpu_bitmap_balloc_fixed,
	.free_fixed	= nvgpu_bitmap_free_fixed,

	.base		= nvgpu_bitmap_alloc_base,
	.length		= nvgpu_bitmap_alloc_length,
	.end		= nvgpu_bitmap_alloc_end,
	.inited		= nvgpu_bitmap_alloc_inited,

	.fini		= nvgpu_bitmap_alloc_destroy,

#ifdef __KERNEL__
	.print_stats	= nvgpu_bitmap_print_stats,
#endif
};

static int nvgpu_bitmap_check_argument_limits(u64 base, u64 length,
								u64 blk_size)
{
	bool is_blk_size_pwr_2;
	bool is_base_aligned;
	bool is_length_aligned;

	nvgpu_assert(blk_size > 0ULL);
	is_blk_size_pwr_2 = (blk_size & (blk_size - 1ULL)) == 0ULL;
	is_base_aligned =  (base & (blk_size - 1ULL)) == 0ULL;
	is_length_aligned = (length & (blk_size - 1ULL)) == 0ULL;

	if (!is_blk_size_pwr_2) {
		nvgpu_do_assert();
		return -EINVAL;
	}

	if (!is_base_aligned || !is_length_aligned) {
		return -EINVAL;
	}

	if (length == 0ULL) {
		return -EINVAL;
	}

	return 0;
}

int nvgpu_bitmap_allocator_init(struct gk20a *g, struct nvgpu_allocator *na,
				const char *name, u64 base, u64 length,
				u64 blk_size, u64 flags)
{
	int err;
	struct nvgpu_bitmap_allocator *a;

	err = nvgpu_bitmap_check_argument_limits(base, length, blk_size);
	if (err != 0) {
		return err;
	}

	if (base == 0ULL) {
		base = blk_size;
		length = nvgpu_safe_sub_u64(length, blk_size);
	}

	a = nvgpu_kzalloc(g, sizeof(struct nvgpu_bitmap_allocator));
	if (a == NULL) {
		return -ENOMEM;
	}

	err = nvgpu_alloc_common_init(na, g, name, a, false, &bitmap_ops);
	if (err != 0) {
		goto fail;
	}

	if ((flags & GPU_ALLOC_NO_ALLOC_PAGE) == 0ULL) {
		a->meta_data_cache = nvgpu_kmem_cache_create(g,
					sizeof(struct nvgpu_bitmap_alloc));
		if (a->meta_data_cache == NULL) {
			err = -ENOMEM;
			goto fail;
		}
	}

	a->base = base;
	a->length = length;
	a->blk_size = blk_size;
	a->blk_shift = nvgpu_safe_sub_u64(nvgpu_ffs(a->blk_size), 1UL);
	a->num_bits = length >> a->blk_shift;
	a->bit_offs = a->base >> a->blk_shift;
	a->flags = flags;
	a->allocs = NULL;

	a->bitmap = nvgpu_kcalloc(g, BITS_TO_LONGS(a->num_bits),
				  sizeof(*a->bitmap));
	if (a->bitmap == NULL) {
		err = -ENOMEM;
		goto fail;
	}

	nvgpu_smp_wmb();
	a->inited = true;

#ifdef CONFIG_DEBUG_FS
	nvgpu_init_alloc_debug(g, na);
#endif
	alloc_dbg(na, "New allocator: type      bitmap");
	alloc_dbg(na, "               base      0x%llx", a->base);
	alloc_dbg(na, "               bit_offs  0x%llx", a->bit_offs);
	alloc_dbg(na, "               size      0x%llx", a->length);
	alloc_dbg(na, "               blk_size  0x%llx", a->blk_size);
	alloc_dbg(na, "               flags     0x%llx", a->flags);

	return 0;

fail:
	if (a->meta_data_cache != NULL) {
		nvgpu_kmem_cache_destroy(a->meta_data_cache);
	}
	nvgpu_kfree(g, a);
	return err;
}
