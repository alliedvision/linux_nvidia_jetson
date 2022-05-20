/*
 * Copyright (c) 2018-2019, NVIDIA CORPORATION.  All rights reserved.
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

#include <stdlib.h>

#include <unit/io.h>
#include <unit/unit.h>

#include <nvgpu/list.h>
#include "list.h"

#define ELEMENTS 10

/* Simple list holding a single int value */
struct integer_list {
	int value;
	struct nvgpu_list_node list;
};

static inline struct integer_list *
integer_list_from_list(struct nvgpu_list_node *node)
{
	return (struct integer_list *)
		((uintptr_t)node - offsetof(struct integer_list, list));
};

/*
 * Count the number of elements inside an integer_list
 */
static int list_count(struct nvgpu_list_node *head)
{
	int count = 0;
	struct integer_list *pos;

	nvgpu_list_for_each_entry(pos, head, integer_list, list) {
		count++;
	}

	return count;
}

/*
 * Add "count" consecutive elements to the list. Depending on the value of
 * "add_to_tail", the elements will be added to the head or the tail.
 */
static int add_consecutive_elements(struct nvgpu_list_node *head, int count,
		bool add_to_tail)
{
	int i;
	int list_elements = list_count(head);

	for (i = 0; i < count; i++) {
		struct integer_list *node = malloc(sizeof(struct integer_list));

		if (node == NULL) {
			return -ENOMEM;
		}

		node->value = list_elements+i+1;

		if (add_to_tail) {
			nvgpu_list_add_tail(&node->list, head);
		} else {
			nvgpu_list_add(&node->list, head);
		}
	}

	return 0;
}

/*
 * Delete "count" consecutive elements starting from head.
 */
static int del_consecutive_elements(struct nvgpu_list_node *head, int count)
{
	int i;
	struct integer_list *pos;

	for (i = 0; i < count; i++) {
		if (nvgpu_list_empty(head)) {
			return -ENOMEM;
		}
		pos = nvgpu_list_first_entry(head, integer_list, list);
		nvgpu_list_del(&pos->list);
		free(pos);
	}

	return 0;
}

/* Initialize the list and add some elements to it. */
static int init_list_elements(struct unit_module *m,
		struct nvgpu_list_node *head, int count, bool add_to_tail)
{
	nvgpu_init_list_node(head);

	if (!nvgpu_list_empty(head)) {
		unit_return_fail(m, "List should be empty");
	}

	if (add_consecutive_elements(head, count, add_to_tail) != 0) {
		unit_return_fail(m, "Error adding elements");
	}

	if (nvgpu_list_empty(head)) {
		unit_return_fail(m, "List should not be empty");
	}

	return 0;
}

int test_list_add(struct unit_module *m, struct gk20a *g, void *__args)
{
	struct integer_list *pos;
	int i;
	struct nvgpu_list_node head;
	bool tail_case = (bool) __args;

	if (init_list_elements(m, &head, ELEMENTS, tail_case) != 0) {
		return UNIT_FAIL;
	}

	i = tail_case ? 1 : ELEMENTS;

	nvgpu_list_for_each_entry(pos, &head, integer_list, list) {
		if (i != pos->value) {
			unit_return_fail(m, "Incorrect value in list: %d/%d",
				i, pos->value);
		}

		tail_case ? i++ : i--;
	}

	if (del_consecutive_elements(&head, ELEMENTS) != 0) {
		unit_return_fail(m, "Could not delete all elements");
	}

	return UNIT_SUCCESS;
}

int test_list_move(struct unit_module *m, struct gk20a *g, void *__args)
{
	struct integer_list *pos;
	int i;
	struct nvgpu_list_node head;

	if (init_list_elements(m, &head, ELEMENTS, false) != 0) {
		return UNIT_FAIL;
	}

	/*
	 * Add an extra element, but at the tail. Now the list is not ordered
	 * anymore
	 */
	if (add_consecutive_elements(&head, 1, true) != 0) {
		unit_return_fail(m, "Error adding elements");
	}

	/*
	 * Move the extra element from the tail to the head so that the list
	 * is now ordered.
	 */
	pos = nvgpu_list_last_entry(&head, integer_list, list);
	nvgpu_list_move(&pos->list, &head);

	i = ELEMENTS + 1;

	/* Now the list should be ordered */
	nvgpu_list_for_each_entry(pos, &head, integer_list, list) {
		if (i != pos->value) {
			unit_return_fail(m, "Incorrect value in list: %d/%d",
				i, pos->value);
		}

		i--;
	}

	if (del_consecutive_elements(&head, ELEMENTS) != 0) {
		unit_return_fail(m, "Could not delete all elements");
	}

	return UNIT_SUCCESS;
}

int test_list_replace(struct unit_module *m, struct gk20a *g, void *__args)
{
	struct integer_list *pos, *new_elem;
	struct nvgpu_list_node head;

	if (init_list_elements(m, &head, ELEMENTS, false) != 0) {
		return UNIT_FAIL;
	}

	pos = nvgpu_list_last_entry(&head, integer_list, list);
	new_elem = malloc(sizeof(struct integer_list));
	if (new_elem == NULL) {
		unit_return_fail(m, "Error creating new element");
	}

	new_elem->value = ELEMENTS * 2; /* Value is irrelevant */
	nvgpu_list_replace_init(&pos->list, &new_elem->list);

	/* The old element is out of the list and can now be freed */
	free(pos);

	pos = nvgpu_list_last_entry(&head, integer_list, list);
	if (pos != new_elem) {
		unit_return_fail(m, "Replace operation failed.");
	}

	if (del_consecutive_elements(&head, ELEMENTS) != 0) {
		unit_return_fail(m, "Could not delete all elements");
	}

	return UNIT_SUCCESS;
}

struct unit_module_test list_tests[] = {
	UNIT_TEST(list_all_head, test_list_add, (void *) false, 0),
	UNIT_TEST(list_all_tail, test_list_add, (void *) true, 0),
	UNIT_TEST(list_move, test_list_move, NULL, 0),
	UNIT_TEST(list_replace, test_list_replace, NULL, 0),
};

UNIT_MODULE(interface_list, list_tests, UNIT_PRIO_NVGPU_TEST);
