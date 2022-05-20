/*
 * Copyright (c) 2017-2021, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef NVGPU_RBTREE_H
#define NVGPU_RBTREE_H

#include <nvgpu/types.h>

/**
 * @defgroup rbtree
 * @ingroup unit-common-utils
 * @{
 */

/**
 * A node in the rbtree
 */
struct nvgpu_rbtree_node {
	/**
	 * Start of range for the key used for searching/inserting in the tree
	 */
	u64 key_start;
	/**
	 * End of range for the key used for searching/inserting the tree
	 */
	u64 key_end;

	/**
	 * Is this a red node? (!is_red ==> is_black)
	 */
	bool is_red;

	/**
	 * Parent of this node
	 */
	struct nvgpu_rbtree_node *parent;
	/**
	 * Left child of this node (key is less than this node's key)
	 */
	struct nvgpu_rbtree_node *left;
	/**
	 * Right child of this node. (key is greater than this node's key)
	 */
	struct nvgpu_rbtree_node *right;
};

/**
 * @brief Insert a new node into rbtree.
 *
 * - Find the correct location in the tree for this node based on its key value
 *   and insert it by updating the pointers.
 * - Rebalance tree.
 *
 * NOTE: Nodes with duplicate key_start and overlapping ranges are not allowed.
 *
 * @param new_node [in]	Pointer to new node. Function does not insert if this
 *			parameter is a duplicate entry in the tree.
 * @param root [in] Pointer to root of tree. Function does not perform any
 *		    validation of the parameter.
 */
void nvgpu_rbtree_insert(struct nvgpu_rbtree_node *new_node,
		    struct nvgpu_rbtree_node **root);

/**
 * @brief Delete a node from rbtree.
 *
 * - Update tree pointers to remove this node from tree while keeping its
 *   children.
 * - Rebalance tree.
 *
 * @param node [in] Pointer to node to be deleted. Function does not perform
 *		    any validation of the parameter.
 * @param root [in] Pointer to root of tree. Function does not perform any
 *		    validation of the parameter.
 */
void nvgpu_rbtree_unlink(struct nvgpu_rbtree_node *node,
		    struct nvgpu_rbtree_node **root);

/**
 * @brief Search for a given key in rbtree
 *
 * This API will match \a key_start against \a key_start in #nvgpu_rbtree_node
 * for each node. In case of a hit, \a node points to a node with given key.
 * In case of a miss, \a node is NULL.
 *
 * @param key_start [in] Key to be searched in rbtree. Function does not
 *			 perform any validation of the parameter.
 * @param node [out] Node pointer to be returned. Function does not
 *		     perform any validation of the parameter.
 * @param root [in] Pointer to root of tree. Function checks if this
 *		    parameter is NULL. In case of a NULL value, the output
 *		    parameter \a node is populated as NULL.
 */
void nvgpu_rbtree_search(u64 key_start, struct nvgpu_rbtree_node **node,
			     struct nvgpu_rbtree_node *root);

/**
 * @brief Search a node with key falling in range
 *
 * This API will compare the given \a key with \a key_start and \a key_end in
 * #nvgpu_rbtree_node for every node, and finds a node where \a key value
 * falls within the range indicated by \a key_start and \a key_end in
 * #nvgpu_rbtree_node.
 * In case of a hit, \a node points to a node with given key.
 * In case of a miss, \a node is NULL.
 *
 * @param key [in] Key to be searched in rbtree. Function does not perform
 *		   any validation of the parameter.
 * @param node [out] Node pointer to be returned. Function does not perform
 *		     any validation of the parameter.
 * @param root [in] Pointer to root of tree. Function checks if the parameter
 *		    is NULL. In case of a NULL value, the output parameter \a
 *		    node is populated as NULL.
 */
void nvgpu_rbtree_range_search(u64 key,
			       struct nvgpu_rbtree_node **node,
			       struct nvgpu_rbtree_node *root);

/**
 * @brief Search a node with key lesser than given key
 *
 * This API will match the given \a key_start with \a key_start in
 * #nvgpu_rbtree_node for every node and finds a node with highest
 * key value lesser than given \a key_start.
 * In case of a hit, \a node points to the node which matches the
 * search criteria.
 * In case of a miss, \a node is NULL.
 *
 * @param key_start [in] Key to be searched in rbtree. Function does not
 *			 perform any validation of the parameter.
 * @param node [out] Node pointer to be returned. Function does not
 *		     perform any validation of the parameter.
 * @param root [in] Pointer to root of tree. Function performs the search
 *		    only if the parameter is not equal to NULL.
 */
void nvgpu_rbtree_less_than_search(u64 key_start,
			       struct nvgpu_rbtree_node **node,
			       struct nvgpu_rbtree_node *root);

/**
 * @brief Enumerate tree starting at the node with specified value.
 *
 * This API returns \a node pointer pointing to first node in the rbtree to
 * begin enumerating the tree. Call this API once per enumeration. Call
 * #nvgpu_rbtree_enum_next to get the next node. \a node is returned as NULL
 * in case if a valid node cannot be found in the tree.
 *
 * @param key_start [in] Key value to begin enumeration from. Function does
 *			 not perform any validation of the parameter.
 * @param node [out] Pointer to first node in the tree. Function does
 *		     not perform any validation of the parameter.
 * @param root [in] Pointer to root of tree. Function performs the
 *		    enumeration only if the parameter is not NULL.
 */
void nvgpu_rbtree_enum_start(u64 key_start,
			struct nvgpu_rbtree_node **node,
			struct nvgpu_rbtree_node *root);

/**
 * @brief Find next node in enumeration in order by key value.
 *
 * To get the next node in the tree, pass in the current \a node. This API
 * returns \a node pointer pointing to next node in the rbtree in order by key
 * value.
 *
 * @param node [in,out]	Pointer to current node is passed in.
 *			Pointer to next node in the tree is passed back.
 *			Function checks if this parameter is pointing to a
 *			valid node and not NULL.
 * @param root [in] Pointer to root of tree. Function checks if this parameter
 *		    is NULL. The search operation is carried out only if the
 *		    parameter is not NULL.
 */
void nvgpu_rbtree_enum_next(struct nvgpu_rbtree_node **node,
		       struct nvgpu_rbtree_node *root);

/**
 * @}
 */
#endif /* NVGPU_RBTREE_H */
