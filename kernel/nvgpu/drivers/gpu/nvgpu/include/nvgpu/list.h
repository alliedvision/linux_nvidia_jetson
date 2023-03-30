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

#ifndef NVGPU_LIST_H
#define NVGPU_LIST_H
#include <nvgpu/types.h>

struct nvgpu_list_node {
	/*
	 * Pointer to the previous node.
	 */
	struct nvgpu_list_node *prev;
	/*
	 * Pointer to the next node.
	 */
	struct nvgpu_list_node *next;
};

/**
 * @brief Initialize a list node.
 *
 * Initializes a list node by setting the \a prev and \a next pointers
 * in #nvgpu_list_node \a node to \a node itself.
 *
 * @param node [in] List node to initialize. Function does not perform
 *		    any validation of the parameter.
 */
static inline void nvgpu_init_list_node(struct nvgpu_list_node *node)
{
	node->prev = node;
	node->next = node;
}

/**
 * @brief Add a new node to the list.
 *
 * Adds the node \a new_node to the head of the list pointed by \a head.
 *
 * @param node [in] Node to be added. Function does not perform any
 *		    validation of the parameter.
 * @param head [in] Head of the list. Function does not perform any
 *		    validation of the parameter.
 */
static inline void nvgpu_list_add(struct nvgpu_list_node *new_node,
					struct nvgpu_list_node *head)
{
	new_node->next = head->next;
	new_node->next->prev = new_node;
	new_node->prev = head;
	head->next = new_node;
}

/**
 * @brief Add a new node to the tail of the list.
 *
 * Adds the node \a new_node to the tail of the list pointed by \a head.
 *
 * @param new_node [in]	Node to be added. Function does not perform any
 *			validation of the parameter.
 * @param head [in] Tail node of the list where the
 *		    addition has to be done. Function does not perform any
 *		    validation of the parameter.
 *
 */
static inline void nvgpu_list_add_tail(struct nvgpu_list_node *new_node,
					struct nvgpu_list_node *head)
{
	new_node->prev = head->prev;
	new_node->prev->next = new_node;
	new_node->next = head;
	head->prev = new_node;
}

/**
 * @brief Delete a node from the list.
 *
 * Deletes the node \a node from the list and initialize the node pointers to
 * point to itself. Uses the function #nvgpu_init_list_node with \a node as
 * parameter to initialize the node after deleting it from the list.
 *
 * @param node [in] Node to be deleted. Function does not perform any
 *		    validation of the parameter.
 */
static inline void nvgpu_list_del(struct nvgpu_list_node *node)
{
	node->prev->next = node->next;
	node->next->prev = node->prev;
	nvgpu_init_list_node(node);
}

/**
 * @brief Check for empty list.
 *
 * Checks if the list pointed by \a head is empty or not.
 *
 * @param head [in] Head node of the list to be checked. Function does not
 *		    perform any validation of the parameter.
 *
 * @return Boolean value to indicate the status of the list.
 *
 * @retval TRUE if list is empty.
 * @retval FALSE if list is not empty.
 */
static inline bool nvgpu_list_empty(struct nvgpu_list_node *head)
{
	return head->next == head;
}

/**
 * @brief Move a node from the list to head.
 *
 * Moves the node pointed by \a node to the head of the list pointed
 * by \a head. Invokes the function #nvgpu_list_del with \a node as parameter
 * to delete the node from the list first and then uses the function
 * #nvgpu_list_add with \a node and \a head as parameters to add the node
 * back in the list.
 *
 * @param node [in] Node to move. Function does not perform any validation
 *		    of the parameter.
 * @param head [in] Head of the list. Function does not perform any validation
 *		    of the parameter.
 */
static inline void nvgpu_list_move(struct nvgpu_list_node *node,
					struct nvgpu_list_node *head)
{
	nvgpu_list_del(node);
	nvgpu_list_add(node, head);
}

/**
 * @brief Replace a node in the list.
 *
 * Replaces the node pointed by \a old_node with the node pointed
 * by \a new_node. Uses the function #nvgpu_init_list_node with \a old_node
 * as parameter to initialize the node after replacing it with the \a new_node.
 *
 * @param old_node [in]	Node to replace. Function does not perform any
 *			validation of the parameter.
 * @param new_node [in]	Node to replace with. Function does not perform any
 *			validation of the parameter.
 */
static inline void nvgpu_list_replace_init(struct nvgpu_list_node *old_node,
					struct nvgpu_list_node *new_node)
{
	new_node->next = old_node->next;
	new_node->next->prev = new_node;
	new_node->prev = old_node->prev;
	new_node->prev->next = new_node;
	nvgpu_init_list_node(old_node);
}

/**
 * @brief Entry from the list
 *
 * Uses token pasting operator to invoke the type and member specific
 * function implementation.
 *
 * @param ptr [in]         Ptr to the list.
 * @param type [in]        Type of the entry.
 * @param member [in]      Name of the list entry in \a type.
 */
#define nvgpu_list_entry(ptr, type, member)	\
	type ## _from_ ## member(ptr)

/**
 * @brief Next entry from the list.
 *
 * Fetches the next entry from the list.
 *
 * @param pos [in]	Position in the list to get the next entry from.
 * @param type [in]	Type of the entry.
 * @param member [in]	Name of the list entry in \a type.
 *
 * @return The next entry from the list.
 */
#define nvgpu_list_next_entry(pos, type, member)	\
	nvgpu_list_entry((pos)->member.next, type, member)

/**
 * @brief First entry from the list.
 *
 * Fetches the first entry from the list. Does not perform any validation of
 * the macro parameters.
 *
 * @param ptr [in]	Pointer to the head.
 * @param type [in]	Type of the entry.
 * @param member [in]	Name of the list entry in \a type.
 *
 * @return The first entry from the list.
 */
#define nvgpu_list_first_entry(ptr, type, member)	\
	nvgpu_list_entry((ptr)->next, type, member)

/**
 * @brief Last entry from the list.
 *
 * Fetches the last entry from the list. Does not perform any validation of
 * the macro parameters.
 *
 * @param ptr [in]	Pointer to list.
 * @param type [in]	Type of the entry.
 * @param member [in]	Name of the list entry in \a type.
 *
 * @return The last entry from the list.
 */
#define nvgpu_list_last_entry(ptr, type, member)	\
	nvgpu_list_entry((ptr)->prev, type, member)

/**
 * @brief Loop through each entry in the list.
 *
 * Loops through each entry in the list. Does not perform any validation of
 * the macro parameters.
 *
 * @param pos [in, out] Entry node in the list.
 * @param head [in]	Pointer to the list.
 * @param type [in]	Type of the entry.
 * @param member [in]	Name of the list entry in \a type.
 */
#define nvgpu_list_for_each_entry(pos, head, type, member)	\
	for ((pos) = nvgpu_list_first_entry(head, type, member);	\
		&(pos)->member != (head);				\
		(pos) = nvgpu_list_next_entry(pos, type, member))

/**
 * @brief Safe loop through each entry in the list.
 *
 * Loops through each entry in the list and safe against removal of the list
 * entry. Does not perform any validation of the macro parameters.
 *
 * @param pos [in, out]	Entry node in the list.
 * @param n [in, out]	Next node in the list.
 * @param head [in]	Pointer to the list.
 * @param type [in]	Type of the entry.
 * @param member [in]	Name of the list entry in \a type.
 */
#define nvgpu_list_for_each_entry_safe(pos, n, head, type, member)	\
	for ((pos) = nvgpu_list_first_entry(head, type, member),	\
			(n) = nvgpu_list_next_entry(pos, type, member);	\
		&(pos)->member != (head);				\
		(pos) = (n), (n) = nvgpu_list_next_entry(n, type, member))

#endif /* NVGPU_LIST_H */
