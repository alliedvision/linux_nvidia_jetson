/*
 * Copyright (c) 2017-2020, NVIDIA CORPORATION.  All rights reserved.
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

#include <nvgpu/rbtree.h>
#include <nvgpu/bug.h>

/*
 * rotate node x to left
 */
static void rotate_left(struct nvgpu_rbtree_node **root,
			struct nvgpu_rbtree_node *x)
{
	struct nvgpu_rbtree_node *y = x->right;

	/* establish x->right link */
	x->right = y->left;
	if (y->left != NULL) {
		y->left->parent = x;
	}

	/* establish y->parent link */
	y->parent = x->parent;
	if (x->parent != NULL) {
		if (x == x->parent->left) {
			x->parent->left = y;
		} else {
			x->parent->right = y;
		}
	} else {
		*root = y;
	}

	/* link x and y */
	y->left = x;
	x->parent = y;
}

/*
 * rotate node x to right
 */
static void rotate_right(struct nvgpu_rbtree_node **root,
			 struct nvgpu_rbtree_node *x)
{
	struct nvgpu_rbtree_node *y = x->left;

	/* establish x->left link */
	x->left = y->right;
	if (y->right != NULL) {
		y->right->parent = x;
	}

	/* establish y->parent link */
	y->parent = x->parent;
	if (x->parent != NULL) {
		if (x == x->parent->right) {
			x->parent->right = y;
		} else {
			x->parent->left = y;
		}
	} else {
		*root = y;
	}

	/* link x and y */
	y->right = x;
	x->parent = y;
}

/*
 * maintain red-black tree balance after inserting node x
 */
static void insert_fixup(struct nvgpu_rbtree_node **root,
			 struct nvgpu_rbtree_node *x)
{
	/* check red-black properties */
	while ((x != *root) && x->parent->is_red) {
		/* we have a violation */
		if (x->parent == x->parent->parent->left) {
			struct nvgpu_rbtree_node *y = x->parent->parent->right;

			if ((y != NULL) && (y->is_red)) {
				/* uncle is RED */
				x->parent->is_red = false;
				y->is_red = false;
				x->parent->parent->is_red = true;
				x = x->parent->parent;
			} else {
				/* uncle is BLACK */
				if (x == x->parent->right) {
					/* make x a left child */
					x = x->parent;
					rotate_left(root, x);
				}

				/* recolor and rotate */
				x->parent->is_red = false;
				x->parent->parent->is_red = true;
				rotate_right(root, x->parent->parent);
			}
		} else {
			/* mirror image of above code */
			struct nvgpu_rbtree_node *y = x->parent->parent->left;

			if ((y != NULL) && (y->is_red)) {
				/* uncle is RED */
				x->parent->is_red = false;
				y->is_red = false;
				x->parent->parent->is_red = true;
				x = x->parent->parent;
			} else {
				/* uncle is BLACK */
				if (x == x->parent->left) {
					x = x->parent;
					rotate_right(root, x);
				}
				x->parent->is_red = false;
				x->parent->parent->is_red = true;
				rotate_left(root, x->parent->parent);
			}
		}
	}

	(*root)->is_red = false;
}

void nvgpu_rbtree_insert(struct nvgpu_rbtree_node *new_node,
		    struct nvgpu_rbtree_node **root)
{
	struct nvgpu_rbtree_node *curr;
	struct nvgpu_rbtree_node *parent;

	/* find future parent */
	curr = *root;
	parent = NULL;

	while (curr != NULL) {
		parent = curr;
		if (new_node->key_start < curr->key_start) {
			curr = curr->left;
		} else if (new_node->key_start > curr->key_start) {
			curr = curr->right;
		} else {
			return; /* duplicate entry */
		}
	}

	/* the caller allocated the node already, just fix the links */
	new_node->parent = parent;
	new_node->left = NULL;
	new_node->right = NULL;
	new_node->is_red = true;

	/* insert node in tree */
	if (parent != NULL) {
		if (new_node->key_start < parent->key_start) {
			parent->left = new_node;
		} else {
			parent->right = new_node;
		}
	} else {
		*root = new_node;
	}

	insert_fixup(root, new_node);
}

/*
 * helper function for delete_fixup_*_child to test if node has no red
 * children
 */
static bool has_no_red_children(struct nvgpu_rbtree_node *w)
{
	return (w == NULL) ||
		(((w->left == NULL) || (!w->left->is_red)) &&
			((w->right == NULL) || (!w->right->is_red)));
}

/* delete_fixup handling if x is the left child */
static void delete_fixup_left_child(struct nvgpu_rbtree_node **root,
				    struct nvgpu_rbtree_node *parent_of_x,
				    struct nvgpu_rbtree_node **x)
{
	struct nvgpu_rbtree_node *w = parent_of_x->right;

	if ((w != NULL) && (w->is_red)) {
		w->is_red = false;
		parent_of_x->is_red = true;
		rotate_left(root, parent_of_x);
		w = parent_of_x->right;
	}

	if (has_no_red_children(w)) {
		if (w != NULL) {
			w->is_red = true;
		}
		*x = parent_of_x;
	} else {
		if ((w->right == NULL) || (!w->right->is_red)) {
			w->left->is_red = false;
			w->is_red = true;
			rotate_right(root, w);
			w = parent_of_x->right;
		}
		w->is_red = parent_of_x->is_red;
		parent_of_x->is_red = false;
		w->right->is_red = false;
		rotate_left(root, parent_of_x);
		*x = *root;
	}
}

/* delete_fixup handling if x is the right child */
static void delete_fixup_right_child(struct nvgpu_rbtree_node **root,
				     struct nvgpu_rbtree_node *parent_of_x,
				     struct nvgpu_rbtree_node **x)
{
	struct nvgpu_rbtree_node *w = parent_of_x->left;

	if ((w != NULL) && (w->is_red)) {
		w->is_red = false;
		parent_of_x->is_red = true;
		rotate_right(root, parent_of_x);
		w = parent_of_x->left;
	}

	if (has_no_red_children(w)) {
		if (w != NULL) {
			w->is_red = true;
		}
		*x = parent_of_x;
	} else {
		if ((w->left == NULL) || (!w->left->is_red)) {
			w->right->is_red = false;
			w->is_red = true;
			rotate_left(root, w);
			w = parent_of_x->left;
		}
		w->is_red = parent_of_x->is_red;
		parent_of_x->is_red = false;
		w->left->is_red = false;
		rotate_right(root, parent_of_x);
		*x = *root;
	}
}

/*
 * maintain red-black tree balance after deleting node x
 */
static void delete_fixup(struct nvgpu_rbtree_node **root,
			  struct nvgpu_rbtree_node *parent_of_x,
			  struct nvgpu_rbtree_node *x)
{
	while ((x != *root) && ((x == NULL) || (!x->is_red))) {
		/*
		 * NULL nodes are sentinel nodes. If we delete a sentinel
		 * node (x==NULL) it must have a parent node (or be the root).
		 * Hence, parent_of_x == NULL with
		 * x==NULL is never possible (tree invariant)
		 */
		if (parent_of_x == NULL) {
			nvgpu_assert(x != NULL);
			parent_of_x = x->parent;
			continue;
		}

		if (x == parent_of_x->left) {
			delete_fixup_left_child(root, parent_of_x, &x);
		} else {
			delete_fixup_right_child(root, parent_of_x, &x);
		}
		parent_of_x = x->parent;
	}

	if (x != NULL) {
		x->is_red = false;
	}
}

static void swap_in_new_child(struct nvgpu_rbtree_node *old,
			      struct nvgpu_rbtree_node *new,
			      struct nvgpu_rbtree_node **root)
{
	if (old->parent != NULL) {
		if (old == old->parent->left) {
			old->parent->left = new;
		} else {
			old->parent->right = new;
		}
	} else {
		*root = new;
	}
}

static void adopt_children(struct nvgpu_rbtree_node *old,
			   struct nvgpu_rbtree_node *new)
{
	new->left = old->left;
	if (old->left != NULL) {
		old->left->parent = new;
	}

	new->right = old->right;
	if (old->right != NULL) {
		old->right->parent = new;
	}
}

void nvgpu_rbtree_unlink(struct nvgpu_rbtree_node *node,
		    struct nvgpu_rbtree_node **root)
{
	struct nvgpu_rbtree_node *x;
	struct nvgpu_rbtree_node *y;
	struct nvgpu_rbtree_node *z;
	struct nvgpu_rbtree_node *parent_of_x;
	bool y_was_black;

	z = node;

	/* unlink */
	if ((z->left == NULL) || (z->right == NULL)) {
		/* y has a SENTINEL node as a child */
		y = z;
	} else {
		/* find tree successor */
		y = z->right;
		while (y->left != NULL) {
			y = y->left;
		}
	}

	/* x is y's only child */
	if (y->left != NULL) {
		x = y->left;
	} else {
		x = y->right;
	}

	/* remove y from the parent chain */
	parent_of_x = y->parent;
	if (x != NULL) {
		x->parent = parent_of_x;
	}
	/* update the parent's links */
	swap_in_new_child(y, x, root);

	y_was_black = !y->is_red;
	if (y != z) {
		/* we need to replace z with y so
		 * the memory for z can be freed
		 */
		y->parent = z->parent;
		swap_in_new_child(z, y, root);

		y->is_red = z->is_red;

		adopt_children(z, y);

		if (parent_of_x == z) {
			parent_of_x = y;
		}
	}

	if (y_was_black) {
		delete_fixup(root, parent_of_x, x);
	}
}

void nvgpu_rbtree_search(u64 key_start, struct nvgpu_rbtree_node **node,
			     struct nvgpu_rbtree_node *root)
{
	struct nvgpu_rbtree_node *curr = root;

	while (curr != NULL) {
		if (key_start < curr->key_start) {
			curr = curr->left;
		} else if (key_start > curr->key_start) {
			curr = curr->right;
		} else {
			*node = curr;
			return;
		}
	}

	*node = NULL;
}

void nvgpu_rbtree_range_search(u64 key,
			       struct nvgpu_rbtree_node **node,
			       struct nvgpu_rbtree_node *root)
{
	struct nvgpu_rbtree_node *curr = root;

	while (curr != NULL) {
		if ((key >= curr->key_start) &&
			(key < curr->key_end)) {
			*node = curr;
			return;
		} else if (key < curr->key_start) {
			curr = curr->left;
		} else {
			curr = curr->right;
		}
	}

	*node = NULL;
}

void nvgpu_rbtree_less_than_search(u64 key_start,
			       struct nvgpu_rbtree_node **node,
			       struct nvgpu_rbtree_node *root)
{
	struct nvgpu_rbtree_node *curr = root;

	while (curr != NULL) {
		if (key_start <= curr->key_start) {
			curr = curr->left;
		} else {
			*node = curr;
			curr = curr->right;
		}
	}
}

void nvgpu_rbtree_enum_start(u64 key_start, struct nvgpu_rbtree_node **node,
			struct nvgpu_rbtree_node *root)
{
	*node = NULL;

	if (root != NULL) {
		struct nvgpu_rbtree_node *curr = root;

		while (curr != NULL) {
			if (key_start < curr->key_start) {
				*node = curr;
				curr = curr->left;
			} else if (key_start > curr->key_start) {
				curr = curr->right;
			} else {
				*node = curr;
				break;
			}
		}
	}
}

void nvgpu_rbtree_enum_next(struct nvgpu_rbtree_node **node,
		       struct nvgpu_rbtree_node *root)
{
	struct nvgpu_rbtree_node *curr = NULL;

	if ((root != NULL) && (*node != NULL)) {
		/* if we don't have a right subtree return the parent */
		curr = *node;

		/* pick the leftmost node of the right subtree ? */
		if (curr->right != NULL) {
			curr = curr->right;
			while (curr->left != NULL) {
				curr = curr->left;
			}
		} else {
			/* go up until we find the right inorder node */
			for (curr = curr->parent;
			     curr != NULL;
			     curr = curr->parent) {
				if (curr->key_start > (*node)->key_start) {
					break;
				}
			}
		}
	}

	*node = curr;
}
