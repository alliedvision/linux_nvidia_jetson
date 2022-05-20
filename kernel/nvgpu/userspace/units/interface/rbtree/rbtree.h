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

#ifndef UNIT_RBTREE_H
#define UNIT_RBTREE_H

struct gk20a;
struct unit_module;

/** @addtogroup SWUTS-interface-rbtree
 *  @{
 *
 * Software Unit Test Specification for interface.rbtree
 *
 * To make testing easier, most tests will use the same rbtree that is built
 * according to:
 * - The tree will contain 9 nodes (10 insertions, but one rejected as
 *   duplicate).
 * - The values in the tree express a range. All nodes have the same range.
 * - The values and the order in which they are inserted is carefully chosen
 *   to maximize code coverage by ensuring that all corner cases are hit.
 *
 * Refer to #initial_key_start for the definition of the test tree.
 */

/**
 * Number of initial elements in the test tree.
 */
#define INITIAL_ELEMENTS 10

/**
 * Range of each element in the test tree.
 */
#define RANGE_SIZE 10U

/**
 * This value is to be used twice in the test tree to create a duplicate.
 */
#define DUPLICATE_VALUE 300

/**
 * Sample tree used throughout this unit. Node values below are key_start.
 *
 *             130 (Black)
 *            /   \
 *           /     \
 *         50       200  (Red)
 *        /  \     /   \
 *       30  80   170   300  (Black)
 *      /        /
 *     10      120  (Red)
 *
 * NOTE: There is a duplicate entry that will be ignored during insertion.
 */

u64 initial_key_start[] = {50, 30, 80, 100, 170, 10, 200, DUPLICATE_VALUE,
	DUPLICATE_VALUE, 120};

/**
 * The following key value should not exist or cover a range from the keys
 * above.
 */
#define INVALID_KEY_START 2000

/**
 * The following key will be used to search and range_search in the tree. It is
 * chosen so that paths taken will involve both left and right branches.
 */
#define SEARCH_KEY 120

/**
 * The values below will cause the red-black properties to be violated upon
 * insertion into the tree defined above. As a result, these will trigger
 * specific cases during the tree rebalancing procedure.
 */
#define RED_BLACK_VIOLATION_1 20
#define RED_BLACK_VIOLATION_2 320


#define RED_BLACK_BVEC_KEY_MIN 0
#define RED_BLACK_BVEC_KEY_MAX UINT64_MAX

/**
 * Test specification for: test_insert
 *
 * Description: Test to check the nvgpu_rbtree_insert operation.
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_rbtree_insert
 *
 * Input: None
 *
 * Steps:
 * - Create a test tree with known values.
 * - Perform all the checks to ensure the resulting tree has all the properties
 *   of a red-black tree.
 * - Insert 2 well known values defined by RED_BLACK_VIOLATION_1 and
 *   RED_BLACK_VIOLATION_2 to cause red-black violations upon insertion.
 * - Check the red-black correctness again to ensure that the insertion
 *   algorithm rebalanced the tree after the 2 insertions.
 * - Free the test tree.
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_insert(struct unit_module *m, struct gk20a *g, void *args);

/**
 * Test specification for: test_unlink
 *
 * Description: Test to check the nvgpu_rbtree_unlink operation by removing
 * every node from a test tree
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_rbtree_search, nvgpu_rbtree_unlink
 *
 * Input: None
 *
 * Steps:
 * - Create a test tree with known values.
 * - For each of the known values used to create the tree"
 *   - Use nvgpu_rbtree_search to search for the node and ensure it exists in
 *     the tree.
 *   - Use nvgpu_rbtree_unlink to unlink the node.
 *   - Run search again to ensure the node is not in the tree anymore.
 * - Free the test tree.
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_unlink(struct unit_module *m, struct gk20a *g, void *args);

/**
 * Test specification for: test_search
 *
 * Description: Test to check the nvgpu_rbtree_search and
 * nvgpu_rbtree_range_search routines and go over some error handling.
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_rbtree_search, nvgpu_rbtree_range_search
 *
 * Input: None
 *
 * Steps:
 * - Create a test tree with known values.
 * - Ensure that searching with a NULL root returns NULL.
 * - Ensure that range searching with a NULL root returns NULL.
 * - Ensure that searching for a known value returns a valid result.
 * - Perform a range search on a value that falls within a known existing range
 *   and ensure it returns the correct result.
 * - Free the test tree.
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_search(struct unit_module *m, struct gk20a *g, void *args);

/**
 * Test specification for: test_enum
 *
 * Description: Test to check the nvgpu_rbtree_enum_start routine and go over
 * some error handling.
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_rbtree_enum_start
 *
 * Input: None
 *
 * Steps:
 * - Create a test tree with known values.
 * - Ensure that enumerating with a NULL root returns NULL.
 * - For each known value of the tree, start an enumeration with the value
 *   itself and ensure that the resulting node's key_start is the same.
 * - Start an enumeration of a key that is know to not be in the tree and ensure
 *   that the returned value is NULL.
 * - Free the test tree.
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_enum(struct unit_module *m, struct gk20a *g, void *args);

/**
 * Test specification for: test_enum_next
 *
 * Description: Test to check the nvgpu_rbtree_enum_next routine and go over
 * some error handling. nvgpu_rbtree_enum_next will find the next node whose
 * key_start value is greater than the one in the provided node.
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_rbtree_enum_next
 *
 * Input: None
 *
 * Steps:
 * - Create a test tree with known values.
 * - Ensure that enum_next with a NULL root returns NULL.
 * - Set a node pointer to point to the root of the tree.
 * - While the node pointer is not NULL, do the following:
 *   - Perform an enum_next operation from the node pointer. The result is
 *     stored in the node pointer.
 *   - Ensure that the node pointer's key_start is not lower than the key_start
 *     value of the previous node.
 *   - Continue until there are no more results and enum_next changes the node
 *     pointer to NULL.
 * - Free the test tree.
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_enum_next(struct unit_module *m, struct gk20a *g, void *args);

/**
 * Test specification for: test_search_less
 *
 * Description: Test to check the nvgpu_rbtree_less_than_search routine: given
 * a key_start value, find a node with a lower key_start value.
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_rbtree_less_than_search
 *
 * Input: None
 *
 * Steps:
 * - Create a test tree with known values.
 * - Considering that after creating the tree is balanced, this implies that the
 *   key_start value of root is somewhere in the middle of the key_start values
 *   of the other nodes of the tree. So root->key_start is used for the
 *   less_than_search operation.
 * - Perform the less_than_search operation and ensure that:
 *   - It yields a non-NULL result.
 *   - The key_start value of the resulting node is lower than root->key_start.
 * - Free the test tree.
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_search_less(struct unit_module *m, struct gk20a *g, void *args);

/**
 * Test specification for: test_unlink_corner_cases
 *
 * Description: Test corner cases in nvgpu_rbtree_unlink (and delete_fixup) to
 * increase branch and line coverage.
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_rbtree_insert, nvgpu_rbtree_unlink
 *
 * Input: None
 *
 * Steps:
 * - Create a test tree with known values.
 * - Unlink well chosen values to create conditions where nodes are removed on
 *   the left and subsequently the right side needs to become red.
 * - Unlink well chosen values to create conditions where nodes are removed on
 *   the left and the right is NULL.
 * - Unlink well chosen values to create conditions where nodes are removed on
 *   the left and the right side is red, requiring a tree rotation to the left.
 * - Unlink well chosen values to create conditions where nodes are removed on
 *   the right and the left side needs to become black.
 * - Unlink well chosen values to create conditions where right sibling of
 *   deleted node is black or has a right sentinel.
 * - Unlink well chosen values to create conditions where the left rotation
 *   will create a new root.
 * - Free the test tree.
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_unlink_corner_cases(struct unit_module *m, struct gk20a *g,
	void *args);

/**
 * Test specification for: test_search_bvec
 *
 * Description: Test to check the boundary values for nvgpu_rbtree_search,
 * nvgpu_rbtree_range_search and nvgpu_rbtree_less_than_search.
 *
 * Test Type: Boundary value
 *
 * Targets: nvgpu_rbtree_search, nvgpu_rbtree_range_search,
 *          nvgpu_rbtree_less_than_search.
 *
 * Input: None
 *
 * Equivalence classes:
 * Variable: key_start
 * - Valid : { 0 - UINT64_MAX }
 * Variable: key
 * - Valid : { 0 - UINT64_MAX }
 * Steps:
 * - Create a test tree with known values.
 * - Insert two new nodes with boundary key values.
 * - Ensure that searching for min boundary value returns a valid result.
 * - Ensure that range searching for min value returns a valid result.
 * - Ensure that less than search for min value returns NULL.
 * - Ensure that searching for max boundary value returns a valid result.
 * - Ensure that range searching for max value returns NULL.
 * - Ensure that less than search for max value returns a valid result.
 * - Ensure that searching for a valid value in between min and max values
 *   returns a valid result.
 * - Ensure that range searching for a valid value in between min and max
 *   returns a valid result.
 * - Ensure that less than search for a valid value in between min and max
 *   returns a valid result.
 * - Free the extra nodes allocated.
 * - Free the test tree.
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_search_bvec(struct unit_module *m, struct gk20a *g, void *args);

/**
 * Test specification for: test_enum_bvec
 *
 * Description: Test to check the boundary values for nvgpu_rbtree_enum_start
 *
 * Test Type: Boundary value
 *
 * Targets: nvgpu_rbtree_enum_start
 *
 * Input: None
 *
 * Equivalence classes:
 * Variable: key_start
 * - Valid : { 0 - UINT64_MAX }
 *
 * Steps:
 * - Create a test tree with known values.
 * - Invoke nvgpu_rbtree_enum_start for a known key value in the tree. The API
 *   should return a valid node and the key_start value of the node should
 *   match the requested key value.
 * - Insert two new nodes with boundary key values.
 * - Invoke nvgpu_rbtree_enum_start for BVEC min key value in the tree. The API
 *   should return a valid node and the key_start value of the node should
 *   match the requested key value.
 * - Invoke nvgpu_rbtree_enum_start for BVEC max key value in the tree. The API
 *   should return a valid node and the key_start value of the node should
 *   match the requested key value.
 * - Free the extra nodes allocated.
 * - Free the test tree.
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_enum_bvec(struct unit_module *m, struct gk20a *g, void *args);

/** }@ */
#endif /* UNIT_RBTREE_H */
