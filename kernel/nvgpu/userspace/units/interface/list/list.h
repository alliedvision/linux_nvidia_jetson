/*
 * Copyright (c) 2020, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef UNIT_LIST_H
#define UNIT_LIST_H

struct gk20a;
struct unit_module;

/** @addtogroup SWUTS-interface-list
 *  @{
 *
 * Software Unit Test Specification for interface.list
 */

/**
 * Test specification for: test_list_add
 *
 * Description: Test case to verify that elements get added and stay in the
 * right order.
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_init_list_node, nvgpu_list_empty, nvgpu_list_add_tail,
 * nvgpu_list_add, nvgpu_list_del
 *
 * Input: __args is a boolean to indicate if adding to the head (false) or to
 * the tail (true)
 *
 * Steps:
 * - Create a test list with a known number of elements of consecutive values.
 * - For each element in the list, ensure it is consecutive with the previous
 *   one (ascending if adding to head, descending if adding to tail).
 * - Delete all known elements from the list and ensure the resulting list
 *   is empty.
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_list_add(struct unit_module *m, struct gk20a *g, void *__args);

/**
 * Test specification for: test_list_move
 *
 * Description: Test case to verify that elements get added and stay in the
 * right order.
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_init_list_node, nvgpu_list_empty, nvgpu_list_add_tail,
 * nvgpu_list_add, nvgpu_list_move, nvgpu_list_del
 *
 * Input: None
 *
 * Steps:
 * - Create a test list with a known number of elements of consecutive values.
 * - Add an extra element to the tail so that the list is not ordered anymore.
 * - Move the last element to the head.
 * - Ensure the list is now ordered.
 * - Delete all known elements from the list and ensure the resulting list
 *   is empty.
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_list_move(struct unit_module *m, struct gk20a *g, void *__args);

/**
 * Test specification for: test_list_replace
 *
 * Description: Test case to test the replace operation by replacing the last
 * node by a new one.
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_init_list_node, nvgpu_list_empty, nvgpu_list_add_tail,
 * nvgpu_list_add, nvgpu_list_replace_init, nvgpu_list_del
 *
 * Input: None
 *
 * Steps:
 * - Create a test list with a known number of elements of consecutive values.
 * - Create a new node.
 * - Get the last element of the list and replace it by the new node.
 * - Ensure the last element of the list is indeed the new node.
 * - Delete all known elements from the list and ensure the resulting list
 *   is empty.
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_list_replace(struct unit_module *m, struct gk20a *g, void *__args);

/** }@ */
#endif /* UNIT_LIST_H */
