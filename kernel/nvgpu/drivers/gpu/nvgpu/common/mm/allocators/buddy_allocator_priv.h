/*
 * Copyright (c) 2016-2019, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef NVGPU_MM_BUDDY_ALLOCATOR_PRIV_H
#define NVGPU_MM_BUDDY_ALLOCATOR_PRIV_H

/**
 * @file
 *
 * Implementation of the buddy allocator.
 */

#include <nvgpu/rbtree.h>
#include <nvgpu/list.h>
#include <nvgpu/static_analysis.h>

struct nvgpu_kmem_cache;
struct nvgpu_allocator;
struct vm_gk20a;

/**
 * Structure that defines each buddy as an element in a binary tree.
 */
struct nvgpu_buddy {
	/**
	 * Parent node.
	 */
	struct nvgpu_buddy *parent;

	/**
	 * This node's buddy.
	 */
	struct nvgpu_buddy *buddy;

	/**
	 * Lower address sub-node.
	 */
	struct nvgpu_buddy *left;

	/**
	 * Higher address sub-node.
	 */
	struct nvgpu_buddy *right;

	/**
	 * List entry for various lists.
	 */
	struct nvgpu_list_node buddy_entry;

	/**
	 * RB tree of allocations.
	 */
	struct nvgpu_rbtree_node alloced_entry;

	/**
	 * Start address of this buddy.
	 */
	u64 start;

	/**
	 * End address of this buddy.
	 */
	u64 end;

	/**
	 * Buddy order.
	 */
	u64 order;

	/**
	 * Possible flags to use in the buddy allocator. Set in the #flags
	 * member.
	 * @addtogroup BALLOC_BUDDY_FLAGS
	 * @{
	 */
#define BALLOC_BUDDY_ALLOCED	0x1U
#define BALLOC_BUDDY_SPLIT	0x2U
#define BALLOC_BUDDY_IN_LIST	0x4U
	/**@}*/

	/**
	 * Buddy flags among the @ref BALLOC_BUDDY_FLAGS
	 */
	u32 flags;


	/**
	 * Possible PDE sizes. This allows for grouping like sized allocations
	 * into the same PDE. Set in the #pte_size member.
	 * @addtogroup BALLOC_PTE_SIZE
	 * @{
	 */
#define BALLOC_PTE_SIZE_ANY	(~0U)
#define BALLOC_PTE_SIZE_INVALID	0U
#define BALLOC_PTE_SIZE_SMALL	1U
#define BALLOC_PTE_SIZE_BIG	2U
	/**@}*/

	/**
	 * Size of the PDE this buddy is using. Possible values in
	 * @ref BALLOC_PTE_SIZE
	 */
	u32 pte_size;
};

/**
 * @brief Given a list node, retrieve the buddy.
 *
 * @param[in] node	Pointer to the list node.
 *
 * @return pointer to the struct nvgpu_buddy of the node.
 */
static inline struct nvgpu_buddy *
nvgpu_buddy_from_buddy_entry(struct nvgpu_list_node *node)
{
	return (struct nvgpu_buddy *)
		((uintptr_t)node - offsetof(struct nvgpu_buddy, buddy_entry));
};

/**
 * @brief Given a tree node, retrieve the buddy.
 *
 * @param[in] node	Pointer to the tree node.
 *
 * @return pointer to the struct nvgpu_buddy of the node.
 */
static inline struct nvgpu_buddy *
nvgpu_buddy_from_rbtree_node(struct nvgpu_rbtree_node *node)
{
	return (struct nvgpu_buddy *)
		((uintptr_t)node - offsetof(struct nvgpu_buddy, alloced_entry));
};

/**
 * @brief Macro generator to create is/set/clr operations for each of the
 * flags in @ref BALLOC_BUDDY_FLAGS.
 *
 * The created functions are:
 *
 *     bool buddy_is_alloced(struct nvgpu_buddy *b);
 *     void buddy_set_alloced(struct nvgpu_buddy *b);
 *     void buddy_clr_alloced(struct nvgpu_buddy *b);
 *
 *     bool buddy_is_split(struct nvgpu_buddy *b);
 *     void buddy_set_split(struct nvgpu_buddy *b);
 *     void buddy_clr_split(struct nvgpu_buddy *b);
 *
 *     bool buddy_is_in_list(struct nvgpu_buddy *b);
 *     void buddy_set_in_list(struct nvgpu_buddy *b);
 *     void buddy_clr_in_list(struct nvgpu_buddy *b);
 *
 * @param[in] flag	One of is, set or clr
 * @param[in] flag_up	One of the @ref BALLOC_BUDDY_FLAGS
 *
 * @{
 */
#define nvgpu_buddy_allocator_flag_ops(flag, flag_up)			\
	static inline bool buddy_is_ ## flag(struct nvgpu_buddy *b)	\
	{								\
		return (b->flags & BALLOC_BUDDY_ ## flag_up) != 0U;	\
	}								\
	static inline void buddy_set_ ## flag(struct nvgpu_buddy *b)	\
	{								\
		b->flags |= BALLOC_BUDDY_ ## flag_up;			\
	}								\
	static inline void buddy_clr_ ## flag(struct nvgpu_buddy *b)	\
	{								\
		b->flags &= ~BALLOC_BUDDY_ ## flag_up;			\
	}

nvgpu_buddy_allocator_flag_ops(alloced, ALLOCED);
nvgpu_buddy_allocator_flag_ops(split,   SPLIT);
nvgpu_buddy_allocator_flag_ops(in_list, IN_LIST);
/**@} */

/**
 * Structure to keep information for a fixed allocation.
 */
struct nvgpu_fixed_alloc {
	/**
	 * List of buddies.
	 */
	struct nvgpu_list_node buddies;
	/**
	 * RB tree of fixed allocations.
	 */
	struct nvgpu_rbtree_node alloced_entry;
	/**
	 * Start of fixed block.
	 */
	u64 start;
	/**
	 * End address.
	 */
	u64 end;
};

/**
 * @brief Given a tree node, retrieve the fixed allocation.
 *
 * @param[in] node	Pointer to the tree node.
 *
 * @return pointer to the struct nvgpu_fixed_alloc of the node.
 */
static inline struct nvgpu_fixed_alloc *
nvgpu_fixed_alloc_from_rbtree_node(struct nvgpu_rbtree_node *node)
{
	return (struct nvgpu_fixed_alloc *)
	((uintptr_t)node - offsetof(struct nvgpu_fixed_alloc, alloced_entry));
};

/**
 * GPU buddy allocator for the various GPU address spaces. Each addressable unit
 * doesn't have to correspond to a byte. In some cases each unit is a more
 * complex object such as a comp_tag line or the like.
 *
 * The max order is computed based on the size of the minimum order and the size
 * of the address space.
 *
 * #blk_size is the size of an order 0 buddy.
 */
struct nvgpu_buddy_allocator {
	/**
	 * Pointer to the common allocator structure.
	 */
	struct nvgpu_allocator *owner;
	/**
	 * Parent VM - can be NULL.
	 */
	struct vm_gk20a *vm;

	/**
	 * Base address of the space.
	 */
	u64 base;
	/**
	 * Length of the space.
	 */
	u64 length;
	/**
	 * Size of order 0 allocation.
	 */
	u64 blk_size;
	/**
	 * Shift to divide by blk_size.
	 */
	u64 blk_shift;

	/**
	 * Internal: real start (aligned to #blk_size).
	 */
	u64 start;
	/**
	 * Internal: real end, trimmed if needed.
	 */
	u64 end;
	/**
	 * Internal: count of objects in space.
	 */
	u64 count;
	/**
	 * Internal: count of blks in the space.
	 */
	u64 blks;
	/**
	 * Internal: specific maximum order.
	 */
	u64 max_order;

	/**
	 * Outstanding allocations.
	 */
	struct nvgpu_rbtree_node *alloced_buddies;
	/**
	 * Outstanding fixed allocations.
	 */
	struct nvgpu_rbtree_node *fixed_allocs;

	/**
	 * List of carveouts.
	 */
	struct nvgpu_list_node co_list;

	/**
	 * Cache of allocations (contains address and size of allocations).
	 */
	struct nvgpu_kmem_cache *buddy_cache;

	/**
	 * Impose an upper bound on the maximum order.
	 */
#define GPU_BALLOC_ORDER_LIST_LEN	(GPU_BALLOC_MAX_ORDER + 1U)

	/**
	 * List of buddies.
	 */
	struct nvgpu_list_node buddy_list[GPU_BALLOC_ORDER_LIST_LEN];
	/**
	 * Length of the buddy list.
	 */
	u64 buddy_list_len[GPU_BALLOC_ORDER_LIST_LEN];
	/**
	 * Number of split nodes.
	 */
	u64 buddy_list_split[GPU_BALLOC_ORDER_LIST_LEN];
	/**
	 * Number of allocated nodes.
	 */
	u64 buddy_list_alloced[GPU_BALLOC_ORDER_LIST_LEN];

	/**
	 * This is for when the allocator is managing a GVA space (the
	 * #GPU_ALLOC_GVA_SPACE bit is set in #flags). This requires
	 * that we group like sized allocations into PDE blocks.
	 */
	u64 pte_blk_order;

	/**
	 * Boolean to indicate if the allocator has been fully initialized.
	 */
	bool initialized;
	/**
	 * Boolean set to true after the first allocation is made.
	 */
	bool alloc_made;

	/**
	 * Flags in used by the allocator as defined by @ref GPU_ALLOC_FLAGS
	 */
	u64 flags;

	/**
	 * Statistics: total number of bytes allocated.
	 */
	u64 bytes_alloced;
	/**
	 * Statistics: total number of bytes allocated taking into account the
	 * buddy order.
	 */
	u64 bytes_alloced_real;
	/**
	 * Statistics: total number of bytes freed.
	 */
	u64 bytes_freed;
};

/**
 * @brief Given a generic allocator context, retrieve a pointer to the buddy
 * allocator context structure.
 *
 * @param[in] a		Pointer to nvgpu allocator.
 *
 * @return pointer to the struct nvgpu_bitmap_allocator.
 */
static inline struct nvgpu_buddy_allocator *buddy_allocator(
	struct nvgpu_allocator *a)
{
	return (struct nvgpu_buddy_allocator *)(a)->priv;
}

/**
 * @brief Given a buddy allocator, retrieve the list of buddies of the chosen
 * order.
 *
 * @param[in] a		Pointer to the buddy allocator.
 * @param[in] order	Buddy order.
 *
 * @return list of buddies whose order is \a order.
 */
static inline struct nvgpu_list_node *balloc_get_order_list(
	struct nvgpu_buddy_allocator *a, u64 order)
{
	return &a->buddy_list[order];
}

/**
 * @brief Convert a buddy order to a length in bytes, based on the block size.
 *
 * @param[in] a		Pointer to the buddy allocator.
 * @param[in] order	Buddy order.
 *
 * @return length in bytes.
 */
static inline u64 balloc_order_to_len(struct nvgpu_buddy_allocator *a,
				      u64 order)
{
	return nvgpu_safe_mult_u64(BIT64(order), a->blk_size);
}

/**
 * @brief Given a base address, shift it by the base address of the buddy.
 *
 * @param[in] a		Pointer to the buddy allocator.
 * @param[in] order	Base address.
 *
 * @return shifted address.
 */
static inline u64 balloc_base_shift(struct nvgpu_buddy_allocator *a,
				    u64 base)
{
	return nvgpu_safe_sub_u64(base, a->start);
}

/**
 * @brief Given a shifted address, unshift it by the base address of the buddy.
 *
 * @param[in] a		Pointer to the buddy allocator.
 * @param[in] order	Shifted address.
 *
 * @return unshifted address.
 */
static inline u64 balloc_base_unshift(struct nvgpu_buddy_allocator *a,
				      u64 base)
{
	return nvgpu_safe_add_u64(base, a->start);
}

/**
 * @brief Given a buddy allocator context, retrieve a pointer to the generic
 * allocator context structure.
 *
 * @param[in] a		Pointer to nvgpu buddy allocator.
 *
 * @return pointer to the struct nvgpu_allocator.
 */
static inline struct nvgpu_allocator *balloc_owner(
	struct nvgpu_buddy_allocator *a)
{
	return a->owner;
}

#endif /* NVGPU_MM_BUDDY_ALLOCATOR_PRIV_H */
