/*
 * Copyright (c) 2019-2021, NVIDIA CORPORATION.  All rights reserved.
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
#include <unit/core.h>

#include <nvgpu/rbtree.h>
#include <stdlib.h>

#include "rbtree.h"

struct nvgpu_rbtree_node *elements[INITIAL_ELEMENTS];

/*
 * Helper function to ensure a given tree satisfies all the properties to be
 * considered a red-black binary tree. That is:
 * 1. Every node is either red or black: implied since color is a bool with only
 *    two possible values.
 * 2. The root is black: checked by the function below.
 * 3. Every leaf is black: implied since all leaves are NULL.
 * 4. If a node is red, then both its children have to be black: checked by the
 *    function below.
 * 5. All simple paths from a node to its descendant leaves must contain the
 *    same number of black nodes: checked by the function below.
 *
 * So only properties 2, 4 and 5 need to be checked.
 *
 * Returns either:
 * - a negative value in case of error
 * - the number of black nodes to leaves (which is the black height of the tree
 *   when ran from the root).
 */
static int check_rbtree(struct unit_module *m, struct nvgpu_rbtree_node *node)
{
	int left_black_count, right_black_count;
	int black_count = 0;

	if (node == NULL) {
		/* This is a leaf, so black count is 1 */
		return 1;
	}

	/* Check property 2 (root is black) */
	if ((node->parent == NULL) && (node->is_red)) {
		unit_err(m, "check_rbtree: root is red\n");
		return -1;
	}

	/* Check property 4 (if red node, children must be black) */
	if (node->is_red) {
		/*
		 * If left or right is NULL then it is a leaf which is
		 * implicitly black
		 */
		if ((node->left != NULL) && (node->left->is_red)) {
			unit_err(m,
			 "check_rbtree: l_child of red parent is also red\n");
			return -1;
		}
		if ((node->right != NULL) && (node->right->is_red)) {
			unit_err(m,
			 "check_rbtree: r_child of red parent is also red\n");
			return -1;
		}
	}

	/* Count black nodes */
	if (!node->is_red) {
		black_count = 1;
	}

	/*
	 * Check property 5 (descendant leaves must have the same number of
	 * black nodes)
	 * Start by recursively checking the height of the left and right
	 * sub-trees.
	 */
	left_black_count = check_rbtree(m, node->left);
	right_black_count = check_rbtree(m, node->right);

	if ((left_black_count == -1) || (right_black_count == -1)) {
		/* There was an error in one of the subtrees, propagate it */
		return -1;
	}

	if (left_black_count != right_black_count) {
		unit_err(m, "check_rbtree: mismatch between left and right\n");
		return -1;
	}

	return left_black_count + black_count;
}

/*
 * Helper function to insert elements into a tree using the initial_key_start
 * values.
 */
static int fill_test_tree(struct unit_module *m,
				struct nvgpu_rbtree_node **root)
{
	int i;

	for (i = 0; i < INITIAL_ELEMENTS; i++) {
		elements[i] = (struct nvgpu_rbtree_node *)
				malloc(sizeof(struct nvgpu_rbtree_node));
		elements[i]->key_start = initial_key_start[i];
		elements[i]->key_end = initial_key_start[i]+RANGE_SIZE;
		nvgpu_rbtree_insert(elements[i], root);
	}

	return UNIT_SUCCESS;
}

/*
 * Helper function to free the test nodes of the tree.
 */
static void free_test_tree(struct unit_module *m,
				struct nvgpu_rbtree_node *root)
{
	int i;

	for (i = 0; i < INITIAL_ELEMENTS; i++) {
		free(elements[i]);
	}
	/* No need to explicitly free the root as it was one of the elements */
}

int test_insert(struct unit_module *m, struct gk20a *g, void *args)
{
	struct nvgpu_rbtree_node *root = NULL;
	struct nvgpu_rbtree_node *node1, *node2 = NULL;
	int status = UNIT_FAIL;

	fill_test_tree(m, &root);
	if (check_rbtree(m, root) < 0) {
		goto free_tree;
	}

	node1 = (struct nvgpu_rbtree_node *)
				malloc(sizeof(struct nvgpu_rbtree_node));
	node1->key_start = RED_BLACK_VIOLATION_1;
	node1->key_end = RED_BLACK_VIOLATION_1+RANGE_SIZE;
	nvgpu_rbtree_insert(node1, &root);

	node2 = (struct nvgpu_rbtree_node *)
				malloc(sizeof(struct nvgpu_rbtree_node));
	node2->key_start = RED_BLACK_VIOLATION_2;
	node2->key_end = RED_BLACK_VIOLATION_2+RANGE_SIZE;
	nvgpu_rbtree_insert(node2, &root);

	if (check_rbtree(m, root) < 0) {
		goto free_nodes;
	}

	status = UNIT_SUCCESS;
free_nodes:
	free(node1);
	free(node2);
free_tree:
	free_test_tree(m, root);
	return status;
}

int test_unlink(struct unit_module *m, struct gk20a *g, void *args)
{
	struct nvgpu_rbtree_node *root = NULL;
	struct nvgpu_rbtree_node *result = NULL;
	int status = UNIT_FAIL;
	bool duplicate_handled = false;
	u64 key_start_search;
	int i;

	fill_test_tree(m, &root);

	for (i = 0; i < INITIAL_ELEMENTS; i++) {
		/*
		 * Search for a node from values in the initial_key_start table.
		 */
		key_start_search = initial_key_start[i];
		if ((key_start_search == DUPLICATE_VALUE) &&
			(!duplicate_handled)) {
				duplicate_handled = true;
				continue;
		}

		nvgpu_rbtree_search(key_start_search, &result, root);
		if (result == NULL) {
			unit_err(m, "Search failed for key_start=%lld\n",
					key_start_search);
			goto cleanup;
		} else {
			if (verbose_lvl(m) > 0) {
				unit_info(m, "Found node with key_start=%lld\n",
						result->key_start);
			}
		}

		/*
		 * Unlink will simply remove the node from the tree. It will not
		 * free the resources. It will be done at the end of this
		 * function.
		 */
		nvgpu_rbtree_unlink(result, &root);

		/* Make sure the node was actually removed */
		nvgpu_rbtree_search(key_start_search, &result, root);
		if (result != NULL) {
			unit_err(m, "Unlink failed, node still exists\n");
			goto cleanup;
		} else {
			if (verbose_lvl(m) > 0) {
				unit_info(m, "Node was removed as expected\n");
			}
		}
	}

	status = UNIT_SUCCESS;
cleanup:
	free_test_tree(m, root);
	return status;
}

int test_search(struct unit_module *m, struct gk20a *g, void *args)
{
	struct nvgpu_rbtree_node *root = NULL;
	struct nvgpu_rbtree_node *result1, *result2 = NULL;
	int status = UNIT_FAIL;
	u64 key_start_search = SEARCH_KEY;

	fill_test_tree(m, &root);

	/* Search with a NULL root should not crash and keep result as NULL */
	nvgpu_rbtree_search(key_start_search, &result1, NULL);
	if (result1 != NULL) {
		unit_err(m, "Search did not fail as expected\n");
		goto cleanup;
	}

	/* Same thing with the range_search operation */
	nvgpu_rbtree_range_search(key_start_search, &result2, NULL);
	if (result1 != NULL) {
		unit_err(m, "Range search did not fail as expected\n");
		goto cleanup;
	}

	/* Now search for a real value */
	if (verbose_lvl(m) > 0) {
		unit_info(m, "Searching for key_start=%lld\n",
			key_start_search);
	}
	nvgpu_rbtree_search(key_start_search, &result1, root);
	if (result1 == NULL) {
		unit_err(m, "Search failed\n");
		goto cleanup;
	} else if (verbose_lvl(m) > 0) {
		unit_info(m, "Found node with key_start=%lld key_end=%lld\n",
				result1->key_start, result1->key_end);
	}

	/*
	 * Now do a range search by just incrementing key_start_search by 1
	 * which should yield the exact same result as the previous search
	 * (since it will fall in the same range)
	 */
	key_start_search++;
	if (verbose_lvl(m) > 0) {
		unit_info(m, "Range searching for key=%lld\n",
			key_start_search);
	}
	nvgpu_rbtree_range_search(key_start_search, &result2, root);
	if (result2 == NULL) {
		unit_err(m, "Range search failed\n");
		goto cleanup;
	} else if (result1 != result2) {
		unit_err(m, "Range search did not find the expected result\n");
		goto cleanup;
	} else if (verbose_lvl(m) > 0) {
		unit_info(m, "Found node with key_start=%lld key_end=%lld\n",
				result1->key_start, result1->key_end);
	}

	status = UNIT_SUCCESS;
cleanup:
	free_test_tree(m, root);
	return status;
}

int test_enum(struct unit_module *m, struct gk20a *g, void *args)
{
	struct nvgpu_rbtree_node *root = NULL;
	struct nvgpu_rbtree_node *node = NULL;
	int status = UNIT_FAIL;
	u64 key_start;
	int i;

	fill_test_tree(m, &root);

	/* Enum with a NULL root should not crash and keep result as NULL */
	nvgpu_rbtree_enum_start(0, &node, NULL);
	if (node != NULL) {
		unit_err(m, "Enum did not fail as expected (NULL root)\n");
		goto cleanup;
	}

	/* Enum all the nodes we know are in the tree */
	for (i = 0; i < INITIAL_ELEMENTS; i++) {
		key_start = initial_key_start[i];
		nvgpu_rbtree_enum_start(key_start, &node, root);
		if (node->key_start != key_start) {
			unit_err(m, "Enum mismatch\n");
			goto cleanup;
		}
	}

	/* If the key_start does not exist, enum should return a NULL node */
	nvgpu_rbtree_enum_start(INVALID_KEY_START, &node, root);
	if (node != NULL) {
		unit_err(m, "Enum did not fail as expected: wrong key_start\n");
		goto cleanup;
	}

	status = UNIT_SUCCESS;
cleanup:
	free_test_tree(m, root);
	return status;
}

int test_enum_next(struct unit_module *m, struct gk20a *g, void *args)
{
	struct nvgpu_rbtree_node *root = NULL;
	struct nvgpu_rbtree_node *node = NULL;
	int status = UNIT_FAIL;
	u64 prev_key_start;

	fill_test_tree(m, &root);

	/* Enum with a NULL root should not crash and keep result as NULL */
	nvgpu_rbtree_enum_next(&node, NULL);
	if (node != NULL) {
		unit_err(m, "Enum_next did not fail as expected (NULL root)\n");
		goto cleanup;
	}

	/*
	 * The tree is balanced and we know there are INITIAL_ELEMENTS inside.
	 * Enumerate the next key_start value from root.
	 */
	node = root;
	prev_key_start = node->key_start;
	while (node != NULL) {
		nvgpu_rbtree_enum_next(&node, root);
		if (node != NULL) {
			if (verbose_lvl(m) > 0) {
				unit_info(m, "Node has key_start=%lld\n",
					node->key_start);
			}
			if (node->key_start < prev_key_start) {
				unit_err(m, "Enum_next returned a low value\n");
				goto cleanup;
			}
			prev_key_start = node->key_start;
		}
	}

	/* For branch coverage, test some error handling. */
	node = NULL;
	nvgpu_rbtree_enum_next(&node, root);
	nvgpu_rbtree_enum_next(&node, NULL);

	status = UNIT_SUCCESS;
cleanup:
	free_test_tree(m, root);
	return status;
}

int test_search_less(struct unit_module *m, struct gk20a *g, void *args)
{
	struct nvgpu_rbtree_node *root = NULL;
	struct nvgpu_rbtree_node *result;
	int status = UNIT_FAIL;
	u64 key_start_search;

	fill_test_tree(m, &root);

	/*
	 * The tree is balanced, so the range in the root should be in the
	 * middle of the values, so searching for that value will guarantee
	 * a result.
	 */
	key_start_search = root->key_start;

	nvgpu_rbtree_less_than_search(key_start_search, &result, root);
	if (result == NULL) {
		unit_err(m, "less_than_search unexpectedly failed\n");
		goto cleanup;
	}

	if (result->key_start >= key_start_search) {
		unit_err(m, "less_than_search returned a wrong result\n");
		goto cleanup;
	}

	status = UNIT_SUCCESS;
cleanup:
	free_test_tree(m, root);
	return status;
}

int test_unlink_corner_cases(struct unit_module *m, struct gk20a *g, void *args)
{
	struct nvgpu_rbtree_node *root = NULL;
	u64 more_key_start[] = {0x1000, 0x61000, 0x79000, 0x7d000, 0x7f000,
		0x80000, 0x91000, 0x81000, 0x71000, 0x99000, 0x9d000, 0xa0000,
		0x500, 0x600, 0x700, 0x800, 0x900, 0xa000, 0xb000, 0xc000,
		0xd000, 0xe000, 0xf000};
	u64 num_elems = sizeof(more_key_start) / sizeof(u64);
	struct nvgpu_rbtree_node **more_elements =
		malloc(sizeof(struct nvgpu_rbtree_node *)*num_elems);
	u64 i;

	fill_test_tree(m, &root);

	/*
	 * Add extra nodes to create a much more complicated tree that will
	 * allow targeting specific conditions when unlinking those nodes.
	 * Even though the unlinking of some of those nodes have no direct
	 * impact on line or branch coverage, their presence is needed to create
	 * the corner cases we need.
	 */

	for (i = 0; i < num_elems; i++) {
		more_elements[i] = (struct nvgpu_rbtree_node *)
				malloc(sizeof(struct nvgpu_rbtree_node));
		more_elements[i]->key_start = more_key_start[i];
		more_elements[i]->key_end = more_key_start[i]+RANGE_SIZE;
		nvgpu_rbtree_insert(more_elements[i], &root);
	}

	/* No impact on coverage */
	nvgpu_rbtree_unlink(more_elements[0], &root);

	/*
	 * Targets some conditions when removing a node on the left and the
	 * right needs to become red.
	 */
	nvgpu_rbtree_unlink(more_elements[1], &root);

	/* No impact on coverage */
	nvgpu_rbtree_unlink(more_elements[2], &root);

	/*
	 * Targets some conditions when removing a node on the left and the
	 * right is NULL.
	 */
	nvgpu_rbtree_unlink(more_elements[3], &root);

	/* No impact on coverage */
	nvgpu_rbtree_unlink(more_elements[4], &root);

	/*
	 * Targets some conditions when removing a node on the left and the
	 * right node is red. This requires rotating the tree to the left.
	 */
	nvgpu_rbtree_unlink(more_elements[5], &root);

	/* No impact on coverage */
	nvgpu_rbtree_unlink(more_elements[6], &root);
	nvgpu_rbtree_unlink(more_elements[7], &root);

	/*
	 * Targets statements in the link rebuilding of the rotate_left
	 * function. Also targets some conditions when removing a node on the
	 * right and the left needs to become black.
	 */
	nvgpu_rbtree_unlink(more_elements[8], &root);

	/*
	 * Targets statements in the link rebuilding of nvgpu_rbtree_unlink
	 */
	nvgpu_rbtree_unlink(more_elements[9], &root);

	/* No impact on coverage */
	nvgpu_rbtree_unlink(more_elements[10], &root);
	nvgpu_rbtree_unlink(more_elements[11], &root);
	nvgpu_rbtree_unlink(more_elements[12], &root);
	nvgpu_rbtree_unlink(more_elements[13], &root);
	nvgpu_rbtree_unlink(more_elements[14], &root);
	nvgpu_rbtree_unlink(more_elements[15], &root);
	nvgpu_rbtree_unlink(more_elements[16], &root);
	nvgpu_rbtree_unlink(more_elements[17], &root);

	/*
	 * Targets statements in the link rebuilding of delete_fixup (right
	 * sibling of deleted node is black or has a right sentinel)
	 */
	nvgpu_rbtree_unlink(more_elements[18], &root);

	/*
	 * Targets statement in the rotate_left function (the rotated node
	 * becomes the root of the tree)
	 */
	nvgpu_rbtree_unlink(more_elements[19], &root);

	/* No impact on coverage */
	nvgpu_rbtree_unlink(more_elements[20], &root);
	nvgpu_rbtree_unlink(more_elements[21], &root);
	nvgpu_rbtree_unlink(more_elements[22], &root);

	free_test_tree(m, root);
	for (i = 0; i < num_elems; i++) {
		free(more_elements[i]);
	}
	free(more_elements);

	return UNIT_SUCCESS;
}

int test_search_bvec(struct unit_module *m, struct gk20a *g, void *args)
{
	struct nvgpu_rbtree_node *root = NULL;
	struct nvgpu_rbtree_node *result1 = NULL;
	struct nvgpu_rbtree_node *result2 = NULL;
	struct nvgpu_rbtree_node *result3 = NULL;
	struct nvgpu_rbtree_node *node1 = NULL;
	struct nvgpu_rbtree_node *node2 = NULL;
	u64 key_start_search = RED_BLACK_BVEC_KEY_MIN;
	int status = UNIT_FAIL;

	fill_test_tree(m, &root);
	if (check_rbtree(m, root) < 0) {
		goto free_tree;
	}

	node1 = (struct nvgpu_rbtree_node *)
				malloc(sizeof(struct nvgpu_rbtree_node));
	node1->key_start = RED_BLACK_BVEC_KEY_MIN;
	node1->key_end = RED_BLACK_BVEC_KEY_MIN + RANGE_SIZE;
	nvgpu_rbtree_insert(node1, &root);

	node2 = (struct nvgpu_rbtree_node *)
				malloc(sizeof(struct nvgpu_rbtree_node));
	node2->key_start = RED_BLACK_BVEC_KEY_MAX;
	node2->key_end = RED_BLACK_BVEC_KEY_MAX;
	nvgpu_rbtree_insert(node2, &root);

	if (check_rbtree(m, root) < 0) {
		goto free_nodes;
	}

	nvgpu_rbtree_search(key_start_search, &result1, root);
	if (result1 == NULL) {
		unit_err(m, "BVEC search failed for min value\n");
		goto free_nodes;
	}

	nvgpu_rbtree_range_search(key_start_search, &result2, root);
	if (result2 == NULL) {
		unit_err(m, "BVEC range search failed\n");
		goto free_nodes;
	} else if (result1 != result2) {
		unit_err(m,
			"BVEC range search did not find the expected result\n");
		goto free_nodes;
	}

	nvgpu_rbtree_less_than_search(key_start_search, &result3, root);
	if (result3 != NULL) {
		unit_err(m, "BVEC less than search failed\n");
		goto free_nodes;
	}

	result1 = NULL;
	result2 = NULL;
	result3 = NULL;
	key_start_search = RED_BLACK_BVEC_KEY_MAX;

	nvgpu_rbtree_search(key_start_search, &result1, root);
	if (result1 == NULL) {
		unit_err(m, "BVEC search failed for max value\n");
		goto free_nodes;
	}

	nvgpu_rbtree_range_search(key_start_search, &result2, root);
	if (result2 != NULL) {
		unit_err(m, "BVEC range search failed for max value\n");
		goto free_nodes;
	}

	nvgpu_rbtree_less_than_search(key_start_search, &result3, root);
	if (result3 == NULL) {
		unit_err(m, "BVEC less than search failed for max value\n");
		goto free_nodes;
	}

	result1 = NULL;
	result2 = NULL;
	result3 = NULL;
	key_start_search = SEARCH_KEY;

	nvgpu_rbtree_search(key_start_search, &result1, root);
	if (result1 == NULL) {
		unit_err(m, "BVEC search failed for valid value\n");
		goto free_nodes;
	}

	nvgpu_rbtree_range_search(key_start_search, &result2, root);
	if (result2 == NULL) {
		unit_err(m, "BVEC range search failed for valid value\n");
		goto free_nodes;
	}

	nvgpu_rbtree_less_than_search(key_start_search, &result3, root);
	if (result3 == NULL) {
		unit_err(m, "BVEC less than search failed for valid value\n");
		goto free_nodes;
	}

	status = UNIT_SUCCESS;

free_nodes:
	free(node1);
	free(node2);
free_tree:
	free_test_tree(m, root);

	return status;
}

int test_enum_bvec(struct unit_module *m, struct gk20a *g, void *args)
{
	struct nvgpu_rbtree_node *root = NULL;
	struct nvgpu_rbtree_node *node = NULL;
	struct nvgpu_rbtree_node *node1 = NULL;
	struct nvgpu_rbtree_node *node2 = NULL;
	u64 key_start;
	int status = UNIT_FAIL;

	fill_test_tree(m, &root);
	if (check_rbtree(m, root) < 0) {
		goto free_tree;
	}

	key_start = initial_key_start[0];
	nvgpu_rbtree_enum_start(key_start, &node, root);
	if (node == NULL) {
		unit_err(m, "Enum for valid key returned NULL\n");
		goto free_nodes;
	}
	if (node->key_start != key_start) {
		unit_err(m, "Enum mismatch for initial value\n");
		goto free_tree;
	}

	node1 = (struct nvgpu_rbtree_node *)
				malloc(sizeof(struct nvgpu_rbtree_node));
	node1->key_start = RED_BLACK_BVEC_KEY_MIN;
	node1->key_end = RED_BLACK_BVEC_KEY_MIN + RANGE_SIZE;
	nvgpu_rbtree_insert(node1, &root);

	node2 = (struct nvgpu_rbtree_node *)
				malloc(sizeof(struct nvgpu_rbtree_node));
	node2->key_start = RED_BLACK_BVEC_KEY_MAX;
	node2->key_end = RED_BLACK_BVEC_KEY_MAX;
	nvgpu_rbtree_insert(node2, &root);

	if (check_rbtree(m, root) < 0) {
		goto free_nodes;
	}

	node = NULL;
	key_start = RED_BLACK_BVEC_KEY_MIN;
	nvgpu_rbtree_enum_start(key_start, &node, root);
	if (node == NULL) {
		unit_err(m, "Enum for min key returned NULL\n");
		goto free_nodes;
	}
	if (node->key_start != key_start) {
		unit_err(m, "Enum mismatch for min value\n");
		goto free_nodes;
	}

	node = NULL;
	key_start = RED_BLACK_BVEC_KEY_MAX;
	nvgpu_rbtree_enum_start(key_start, &node, root);
	if (node == NULL) {
		unit_err(m, "Enum for max key returned NULL\n");
		goto free_nodes;
	}
	if (node->key_start != key_start) {
		unit_err(m, "Enum mismatch for max value\n");
		goto free_nodes;
	}

	status = UNIT_SUCCESS;

free_nodes:
	free(node1);
	free(node2);
free_tree:
	free_test_tree(m, root);

	return status;
}

struct unit_module_test interface_rbtree_tests[] = {
	UNIT_TEST(insert, test_insert, NULL, 0),
	UNIT_TEST(search, test_search, NULL, 0),
	UNIT_TEST(unlink, test_unlink, NULL, 0),
	UNIT_TEST(enum, test_enum, NULL, 0),
	UNIT_TEST(enum_next, test_enum_next, NULL, 0),
	UNIT_TEST(search_less_than, test_search_less, NULL, 0),
	UNIT_TEST(unlink_corner_cases, test_unlink_corner_cases, NULL, 0),
	UNIT_TEST(search_bvec, test_search_bvec, NULL, 0),
	UNIT_TEST(search_bvec, test_enum_bvec, NULL, 0),
};

UNIT_MODULE(interface_rbtree, interface_rbtree_tests, UNIT_PRIO_NVGPU_TEST);
