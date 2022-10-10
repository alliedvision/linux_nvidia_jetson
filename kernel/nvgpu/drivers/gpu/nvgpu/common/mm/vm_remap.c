/*
 * Copyright (c) 2017-2022, NVIDIA CORPORATION.  All rights reserved.
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

#include <nvgpu/gk20a.h>
#include <nvgpu/vm.h>
#include <nvgpu/vm_area.h>
#include <nvgpu/vm_remap.h>
#include <nvgpu/comptags.h>
#include <nvgpu/string.h>
#include <nvgpu/power_features/pg.h>
#include<nvgpu/log2.h>

/*
 * Return a pointer to the os-specific structure for the specified physical
 * memory pool.
 */
static inline struct nvgpu_vm_remap_os_buffer *nvgpu_vm_remap_mpool_handle(
	struct nvgpu_vm_remap_mpool *mpool)
{
	if (mpool == NULL) {
		return NULL;
	}

	return &mpool->remap_os_buf;
}

/*
 * Add a reference to the specified physical memory pool.
 */
static inline struct nvgpu_vm_remap_mpool *nvgpu_vm_remap_mpool_get(
	struct nvgpu_vm_remap_mpool *mpool)
{
	nvgpu_ref_get(&mpool->ref);

	return mpool;
}

/*
 * Cleanup physical memory pool resources.  This function is called when
 * the reference count for the physical memory pool goes to zero.
 */
static void nvgpu_vm_remap_mpool_release(struct nvgpu_ref *ref)
{
	struct nvgpu_vm_remap_mpool *mpool;
	struct nvgpu_vm_remap_vpool *vpool;
	struct vm_gk20a *vm;
	struct gk20a *g;

	mpool = nvgpu_vm_remap_mpool_from_ref(ref);
	vpool = mpool->vpool;
	vm = vpool->vm;
	g = gk20a_from_vm(vm);

	nvgpu_rbtree_unlink(&mpool->node, &vpool->mpools);

	/* L2 must be flushed before we destroy any SMMU mappings. */
	if (*mpool->l2_flushed == false) {
		(void) nvgpu_pg_elpg_ms_protected_call(g, g->ops.mm.cache.l2_flush(g, true));
		*mpool->l2_flushed = true;
	}

	nvgpu_vm_remap_os_buf_put(vm, &mpool->remap_os_buf);
	nvgpu_kfree(g, mpool);
}

/*
 * Release a reference to the specified physical memory pool.
 */
static inline void nvgpu_vm_remap_mpool_put(struct vm_gk20a *vm,
					struct nvgpu_vm_remap_vpool *vpool,
					struct nvgpu_vm_remap_mpool *mpool,
					bool *l2_flushed)
{
	if (mpool != NULL) {
		mpool->l2_flushed = l2_flushed;
		nvgpu_ref_put(&mpool->ref, nvgpu_vm_remap_mpool_release);
		mpool->l2_flushed = NULL;
	}
}

/*
 * Insert a physical memory pool into the rbtree of the specified virtual
 * memory pool.
 */
static inline struct nvgpu_vm_remap_mpool *nvgpu_vm_remap_mpool_add(
	struct vm_gk20a *vm,
	struct nvgpu_vm_remap_vpool *vpool,
	struct nvgpu_vm_remap_os_buffer *remap_os_buf)
{
	struct gk20a *g = gk20a_from_vm(vm);
	struct nvgpu_rbtree_node **root = &vpool->mpools;
	struct nvgpu_vm_remap_mpool *mpool;
	u64 key;

	mpool = nvgpu_kzalloc(g, sizeof(*mpool));
	if (mpool == NULL) {
		return NULL;
	}

	key = nvgpu_vm_remap_get_handle(remap_os_buf);
	mpool->node.key_start = key;
	mpool->node.key_end = key;
	nvgpu_ref_init(&mpool->ref);
	mpool->vpool = vpool;

	nvgpu_memcpy((u8 *)&mpool->remap_os_buf, (u8 *)remap_os_buf,
		sizeof(mpool->remap_os_buf));

	nvgpu_rbtree_insert(&mpool->node, root);

	return mpool;
}

/*
 * Return a pointer to the physical memory pool for the specified rbtree node.
 */
static inline struct nvgpu_vm_remap_mpool *
nvgpu_vm_remap_mpool_from_tree_entry(struct nvgpu_rbtree_node *node)
{
	return (struct nvgpu_vm_remap_mpool *)
		((uintptr_t)node - offsetof(struct nvgpu_vm_remap_mpool, node));
};

/*
 * Return a pointer to the physical memory pool for the associated physical
 * memory buffer.
 */
static inline struct nvgpu_vm_remap_mpool *
nvgpu_vm_remap_mpool_find(struct nvgpu_rbtree_node *root,
		struct nvgpu_vm_remap_os_buffer *remap_os_buf)
{
	struct nvgpu_rbtree_node *node;
	u64 key = nvgpu_vm_remap_get_handle(remap_os_buf);

	nvgpu_rbtree_search(key, &node, root);
	if (node == NULL) {
		return NULL;
	}

	return nvgpu_vm_remap_mpool_from_tree_entry(node);
}

/*
 * Validate that the specified remap operation resides within the target
 * virtual memory pool.
 */
static int nvgpu_vm_remap_validate_vpool(struct nvgpu_vm_remap_vpool *vpool,
				struct nvgpu_vm_remap_op *op)
{
	u64 first_page = op->virt_offset_in_pages;
	u64 last_page = op->virt_offset_in_pages + op->num_pages - 1ULL;
	u64 page_size = nvgpu_vm_remap_page_size(op);

	if (page_size == 0) {
		return -EINVAL;
	}

	if (first_page < vpool->base_offset_in_pages ||
		last_page >= vpool->base_offset_in_pages + vpool->num_pages ||
		last_page < first_page) {
		return -EINVAL;
	}

	return 0;
}

/*
 * Validate an unmap operation.
 */
static int nvgpu_vm_remap_validate_unmap(struct vm_gk20a *vm,
					struct nvgpu_vm_remap_vpool *vpool,
					struct nvgpu_vm_remap_op *op)
{
	return nvgpu_vm_remap_validate_vpool(vpool, op);
}

/*
 * Validate a map operation.
 */
static int nvgpu_vm_remap_validate_map(struct vm_gk20a *vm,
			struct nvgpu_vm_remap_vpool *vpool,
			struct nvgpu_vm_remap_op *op,
			struct nvgpu_vm_remap_os_buffer *remap_os_buf)
{
	u64 page_size = nvgpu_vm_remap_page_size(op);
	u64 map_offset;
	u64 map_size;
	u64 os_buf_size;

	map_offset = nvgpu_safe_mult_u64(op->mem_offset_in_pages, page_size);
	map_size = nvgpu_safe_mult_u64(op->num_pages, page_size);
	os_buf_size = nvgpu_os_buf_get_size(&remap_os_buf->os_buf);

	if ((map_size > os_buf_size) ||
		((os_buf_size - map_size) < map_offset)) {
		return -EINVAL;
	}

#ifdef CONFIG_NVGPU_COMPRESSION
	if (op->compr_kind != NVGPU_KIND_INVALID) {

		struct gk20a *g = gk20a_from_vm(vm);
		struct gk20a_comptags comptags = { 0 };

		/*
		 * Note: this is best-effort only
		 */
		gk20a_alloc_or_get_comptags(g, &remap_os_buf->os_buf,
			&g->cbc->comp_tags, &comptags);

		if (!comptags.enabled) {
			/* inform the caller that the buffer does not
			 * have compbits */
			op->compr_kind = NVGPU_KIND_INVALID;
		}

		if (comptags.needs_clear) {
			nvgpu_assert(g->ops.cbc.ctrl != NULL);
			if (gk20a_comptags_start_clear(&remap_os_buf->os_buf)) {
				int err = g->ops.cbc.ctrl(
					g, nvgpu_cbc_op_clear,
					comptags.offset,
					(comptags.offset +
					 comptags.lines - 1U));
				gk20a_comptags_finish_clear(
					&remap_os_buf->os_buf, err == 0);

				if (err) {
					nvgpu_err(
						g, "Comptags clear failed: %d",
						err);
					op->compr_kind = NVGPU_KIND_INVALID;
				}
			}
		}
	}
#endif

	return nvgpu_vm_remap_validate_vpool(vpool, op);
}

/*
 * Return a pointer to the virtual pool for the specified remap operation.
 * Note that this function must be called with the VM's GMMU update lock
 * held.
 */
static struct nvgpu_vm_remap_vpool *nvgpu_vm_remap_get_vpool_locked(
	struct vm_gk20a *vm, struct nvgpu_vm_remap_op *op)
{
	struct gk20a *g = gk20a_from_vm(vm);
	u64 page_size = nvgpu_vm_remap_page_size(op);
	u64 offset;
	struct nvgpu_vm_area *vm_area;

	if (page_size == 0) {
		return NULL;
	}

	offset = nvgpu_safe_mult_u64(op->virt_offset_in_pages, page_size);
	vm_area = nvgpu_vm_area_find(vm, offset);

	if ((vm_area == NULL) || (vm_area->vpool == NULL) ||
		(vm->gmmu_page_sizes[vm_area->pgsz_idx] != page_size)) {
		return NULL;
	}

	/* allocate per-page tracking if first time vpool is used */
	if ((vm_area->vpool->mpool_by_page == NULL) &&
		(vm_area->vpool->num_pages != 0)) {
		struct nvgpu_vm_remap_mpool **mp;

		mp = nvgpu_kzalloc(g, vm_area->vpool->num_pages *
				sizeof(struct nvgpu_vm_remap_mpool *));
		if (mp == NULL) {
			return NULL;
		}
		vm_area->vpool->mpool_by_page = mp;
	}

	return vm_area->vpool;
}

/*
 * Update physical memory pool reference counts for the specified virtual
 * pool range of pages.
 */
static void nvgpu_vm_remap_update_pool_refcounts(
	struct vm_gk20a *vm,
	struct nvgpu_vm_remap_vpool *vpool,
	u64 first_page,
	u64 num_pages,
	struct nvgpu_vm_remap_mpool *new_pool,
	bool *l2_flushed)
{
	u64 pgnum;

	if (vpool->num_pages < first_page + num_pages) {
		nvgpu_err(gk20a_from_vm(vm), "bad vpool range; update skipped");
		return;
	}

	for (pgnum = first_page; pgnum < first_page + num_pages; pgnum++) {

		/*
		 * Decrement refcount of the physical pool that was previously
		 * mapped to this page.
		 */
		nvgpu_vm_remap_mpool_put(vm, vpool,
				vpool->mpool_by_page[pgnum], l2_flushed);

		/*
		 * And record the fact that the current page now refers to
		 * "new_pool".
		 */
		vpool->mpool_by_page[pgnum] = new_pool;

		/*
		 * Increment refcount of the new physical pool.
		 */
		if (new_pool != NULL) {
			(void) nvgpu_vm_remap_mpool_get(new_pool);
		}
	}
}

/*
 * Return the ctag offset (if applicable) for the specified map operation.
 */
static u64 nvgpu_vm_remap_get_ctag_offset(struct vm_gk20a *vm,
					struct nvgpu_vm_remap_op *op,
					struct nvgpu_os_buffer *os_buf,
					s16 *kind)
{
#ifdef CONFIG_NVGPU_COMPRESSION
	struct gk20a *g = gk20a_from_vm(vm);
	struct gk20a_comptags comptags;
	u64 ctag = 0;
	u64 ctag_offset = 0;
	u64 page_size = nvgpu_vm_remap_page_size(op);
	u64 phys_offset = nvgpu_safe_mult_u64(op->mem_offset_in_pages,
					page_size);
	u64 compression_page_size;

	gk20a_get_comptags(os_buf, &comptags);

	if (comptags.lines != 0) {
		ctag = (u64)comptags.offset;
	}

	if (op->compr_kind != NVGPU_KIND_INVALID) {
		*kind = op->compr_kind;
		compression_page_size = g->ops.fb.compression_page_size(g);

		nvgpu_assert(compression_page_size > 0ULL);

		if (ctag != 0) {
			ctag_offset = ctag + (phys_offset >>
				nvgpu_ilog2(compression_page_size));
		} else {
			ctag_offset = 0;
		}
	} else {
		*kind = op->incompr_kind;
		ctag_offset = 0;
	}

	return ctag_offset;
#else
	return 0;
#endif
}

static u32 nvgpu_vm_remap_get_map_flags(struct nvgpu_vm_remap_op *op)
{
	u32 flags = 0;

	if ((op->flags & NVGPU_VM_REMAP_OP_FLAGS_CACHEABLE) != 0U) {
		flags = NVGPU_VM_MAP_CACHEABLE;
	}

	return flags;
}

static enum gk20a_mem_rw_flag nvgpu_vm_remap_get_map_rw_flag(
	struct nvgpu_vm_remap_op *op)
{
	enum gk20a_mem_rw_flag rw_flag = gk20a_mem_flag_none;

	if ((op->flags & NVGPU_VM_REMAP_OP_FLAGS_ACCESS_NO_WRITE) != 0U) {
		rw_flag = gk20a_mem_flag_read_only;
	}

	return rw_flag;
}

/*
 * Execute remap operations in sequence.
 * All remap operations must succeed for this routine to return success.
 */
static int nvgpu_vm_remap_execute_remaps(struct vm_gk20a *vm,
			struct nvgpu_vm_remap_vpool *vpool,
			struct nvgpu_vm_remap_mpool **mpools,
			struct nvgpu_vm_remap_op *ops, u32 *num_ops,
			bool *l2_flushed)
{
	struct gk20a *g = gk20a_from_vm(vm);
	struct nvgpu_vm_remap_op *op;
	struct nvgpu_vm_remap_os_buffer *remap_os_buf;
	u32 pgsz_idx = vpool->vm_area->pgsz_idx;
	u64 page_size = vm->gmmu_page_sizes[pgsz_idx];
	u64 map_addr = 0;
	u64 phys_offset = 0;
	u64 map_size;
	u64 ctag_offset = 0;
	enum gk20a_mem_rw_flag rw_flag;
	u32 flags;
	struct nvgpu_vm_remap_os_buffer *curr_remap_os_buf = NULL;
	struct vm_gk20a_mapping_batch batch;
	s16 curr_kind = 0;
	u32 i;
	int err = 0;

	nvgpu_vm_mapping_batch_start(&batch);

	/* Update GPU page tables. */
	for (i = 0; i < *num_ops; i++) {
		op = &ops[i];
		remap_os_buf = nvgpu_vm_remap_mpool_handle(mpools[i]);

		map_size = nvgpu_safe_mult_u64(op->num_pages, page_size);
		map_addr = nvgpu_safe_mult_u64(op->virt_offset_in_pages,
					page_size);
		phys_offset = nvgpu_safe_mult_u64(op->mem_offset_in_pages,
						page_size);

		if (remap_os_buf == NULL) {
			/*
			 * Unmap range.
			 */
			(void) g->ops.mm.gmmu.unmap(vm,
						map_addr,
						map_size,
						pgsz_idx,
						false, /* va_allocated */
						gk20a_mem_flag_none,
						true, /* sparse */
						&batch);
		} else {
			/*
			 * If physical pool changed from the previous ops,
			 * update curr_pool* variables.
			 */
			if (remap_os_buf != curr_remap_os_buf) {
				curr_remap_os_buf = remap_os_buf;
			}

			ctag_offset = nvgpu_vm_remap_get_ctag_offset(vm, op,
						&curr_remap_os_buf->os_buf,
						&curr_kind);

			flags = nvgpu_vm_remap_get_map_flags(op);
			rw_flag = nvgpu_vm_remap_get_map_rw_flag(op);

			/*
			 * Remap range.
			 */
			map_addr = g->ops.mm.gmmu.map(vm,
						map_addr,
						remap_os_buf->nv_sgt,
						phys_offset,
						map_size,
						pgsz_idx,
						(u8)curr_kind,
						ctag_offset,
						flags,
						rw_flag,
						false, /* ctags clear */
						true,
						false,
						&batch,
						remap_os_buf->aperture);

			if (map_addr == 0ULL) {
				nvgpu_err(g, "map addr is zero");
				err = -ENOMEM;
				break;
			}
		}
	}

	/*
	 * Handle possible error condition by updating num_ops to reflect
	 * the number of remap ops that actually succeeded.
	 */
	if (i != *num_ops) {
		*num_ops = i;
	}

	nvgpu_vm_mapping_batch_finish_locked(vm, &batch);

	/*
	 * Release references to the previously mapped pages of all
	 * remap operations that succeeded.
	 */
	for (i = 0; i < *num_ops; i++) {
		op = &ops[i];
		nvgpu_vm_remap_update_pool_refcounts(
			vm,
			vpool,
			nvgpu_safe_sub_u64(op->virt_offset_in_pages,
					vpool->base_offset_in_pages),
			op->num_pages,
			mpools[i],
			l2_flushed);
	}

	return err;
}

static int nvgpu_vm_remap_get_mpool(struct vm_gk20a *vm,
				struct nvgpu_vm_remap_vpool *vpool,
				struct nvgpu_vm_remap_op *op,
				struct nvgpu_vm_remap_mpool **curr_mpool,
				u32 *curr_mem_handle)
{
	struct nvgpu_vm_remap_os_buffer remap_os_buf;
	int err = 0;

	if (op->mem_handle == *curr_mem_handle) {
		/*
		 * Physical pool didn't change from the previous op, so we
		 * can skip validation and just use the curr_pool pointer
		 * again.  Just take one extra ref.
		 */
		*curr_mpool = nvgpu_vm_remap_mpool_get(*curr_mpool);
	} else {
		/*
		 * Move to the next memory handle (validate access + acquire
		 * reference).
		 */
		err = nvgpu_vm_remap_os_buf_get(vm, op,	&remap_os_buf);
		if (err != 0) {
			goto done;
		}

		/*
		 * Make sure the new memory handle is included in (and
		 * referenced by) the list of memory handles mapped in the
		 * virtual pool.
		 */
		*curr_mpool = nvgpu_vm_remap_mpool_find(vpool->mpools,
						&remap_os_buf);
		if (*curr_mpool != NULL) {
			/*
			 * This memory handle was already mapped to the virtual
			 * pool, so we don't need to keep the extra reference
			 * to the nvmap handle.
			 */
			nvgpu_vm_remap_os_buf_put(vm, &remap_os_buf);
			*curr_mpool = nvgpu_vm_remap_mpool_get(*curr_mpool);
		} else {
			/*
			 * Add the physical memory to the list of mapped
			 * handles.
			 */
			*curr_mpool = nvgpu_vm_remap_mpool_add(vm, vpool,
							&remap_os_buf);
			if (*curr_mpool == NULL) {
				nvgpu_vm_remap_os_buf_put(vm, &remap_os_buf);
				err = -ENOMEM;
				goto done;
			}
		}

		*curr_mem_handle = op->mem_handle;
	}

done:
	return err;
}

/*
 * Prepare to execute remap operations.  Allocate an array to track the
 * associated physical memory pool for each specified operation and
 * then validate the parameters for each operation.  Note that this function
 * must be called with the VM's GMMU update lock held.
 */
static int nvgpu_vm_remap_locked(struct vm_gk20a *vm,
				struct nvgpu_vm_remap_vpool *vpool,
				struct nvgpu_vm_remap_op *ops, u32 *num_ops)
{
	struct gk20a *g = gk20a_from_vm(vm);
	bool l2_flushed = false;
	struct nvgpu_vm_remap_mpool **mpools;
	struct nvgpu_vm_remap_mpool *curr_mpool = NULL;
	u32 curr_mem_handle = 0U;
	u32 num_ops_validated = 0U;
	u32 i;
	int err = 0;

	if (*num_ops == 0U) {
		return 0;
	}

	if (vpool == NULL) {
		return -EINVAL;
	}

	mpools = nvgpu_kzalloc(g, *num_ops * sizeof(mpools[0]));
	if (mpools == NULL) {
		return -ENOMEM;
	}

	/* We cache the validated memory handle across ops to avoid
	 * revalidation in the common case where the physical pool doesn't
	 * change between ops.
	 */
	for (i = 0; i < *num_ops; i++) {
		struct nvgpu_vm_remap_op *op = &ops[i];

		if (op->mem_handle == 0U) {
			err = nvgpu_vm_remap_validate_unmap(vm, vpool, op);
			if (err != 0) {
				nvgpu_err(g, "validate_unmap failed: %d", err);
				break;
			}
		} else {
			err = nvgpu_vm_remap_get_mpool(vm, vpool, op,
						&curr_mpool, &curr_mem_handle);
			if (err != 0) {
				nvgpu_err(g, "get_mpool failed: %d", err);
				break;
			}

			/*
			 * Validate that the mapping request is valid.
			 * This may demote the kind from compressed to
			 * uncompressed if we have run out of compbits.
			 */
			err = nvgpu_vm_remap_validate_map(vm, vpool, op,
				nvgpu_vm_remap_mpool_handle(curr_mpool));
			if (err != 0) {
				nvgpu_err(g, "validate_map failed: %d", err);
				nvgpu_vm_remap_mpool_put(vm, vpool,
					curr_mpool, &l2_flushed);
				break;
			}

			mpools[i] = curr_mpool;
		}
	}

	num_ops_validated = i;

	if (err == 0) {
		/*
		 * The validation stage completed without errors so
		 * execute all map and unmap operations sequentially.
		 */
		err = nvgpu_vm_remap_execute_remaps(vm, vpool, mpools,
					ops, num_ops, &l2_flushed);
	} else {
		/*
		 * Validation failed so set successful num_ops count to zero.
		 */
		*num_ops = 0;
	}

	/* Release references acquired during ops validation. */
	for (i = 0; i < num_ops_validated; i++) {
		nvgpu_vm_remap_mpool_put(vm, vpool,
					mpools[i], &l2_flushed);
	}

	nvgpu_kfree(g, mpools);

	return err;
}

/*
 * Top-level remap handler.  This function is used by the os-specific REMAP
 * API handler to execute remap operations.
 */
int nvgpu_vm_remap(struct vm_gk20a *vm,
		struct nvgpu_vm_remap_op *ops,
		u32 *num_ops)
{
	int ret = 0;
	struct nvgpu_vm_remap_vpool *vpool;

	nvgpu_mutex_acquire(&vm->update_gmmu_lock);

	vpool = nvgpu_vm_remap_get_vpool_locked(vm, &ops[0]);

	if (vpool != NULL) {
		ret = nvgpu_vm_remap_locked(vm, vpool, ops, num_ops);
	} else {
		*num_ops = 0;
		ret = -EINVAL;
	}

	nvgpu_mutex_release(&vm->update_gmmu_lock);

	return ret;
}

/*
 * Create a virtual memory pool.
 */
int nvgpu_vm_remap_vpool_create(struct vm_gk20a *vm,
				struct nvgpu_vm_area *vm_area,
				u64 num_pages)
{
	struct gk20a *g = gk20a_from_vm(vm);
	struct nvgpu_vm_remap_vpool *vp;
	u64 start_page_nr = 0;
	u32 gmmu_page_size;

	if ((num_pages == 0ULL) ||
		((vm_area->flags & NVGPU_VM_AREA_ALLOC_SPARSE) == 0U)) {
		return -EINVAL;
	}

	vp = nvgpu_kzalloc(g, sizeof(struct nvgpu_vm_remap_vpool));
	if (vp == NULL) {
		return -ENOMEM;
	}
	gmmu_page_size = vm->gmmu_page_sizes[vm_area->pgsz_idx];
	nvgpu_assert(gmmu_page_size > 0U);

	start_page_nr = vm_area->addr >>
		nvgpu_ilog2(gmmu_page_size);

	vp->base_offset_in_pages = start_page_nr;
	vp->num_pages = num_pages;
	vp->vm = vm;

	vp->vm_area = vm_area;
	vm_area->vpool = vp;

	return 0;
}

/*
 * Destroy a virtual memory pool.
 */
void nvgpu_vm_remap_vpool_destroy(struct vm_gk20a *vm,
				struct nvgpu_vm_area *vm_area)
{
	struct gk20a *g = gk20a_from_vm(vm);
	struct nvgpu_vm_remap_vpool *vpool = vm_area->vpool;

	if (vpool == NULL) {
		return;
	}

	if (vpool->mpools != NULL) {
		struct nvgpu_vm_remap_op op = { 0 };
		u32 num_ops = 1;
		int err;

		op.flags = nvgpu_vm_remap_page_size_flag(
			vm->gmmu_page_sizes[vm_area->pgsz_idx]);
		op.virt_offset_in_pages = vpool->base_offset_in_pages;
		op.num_pages = vpool->num_pages;

		err = nvgpu_vm_remap_locked(vm, vpool, &op, &num_ops);
		nvgpu_assert(err == 0);
	}

	nvgpu_assert(vpool->mpools == NULL);

	if (vpool->mpool_by_page != NULL) {
		nvgpu_kfree(g, vpool->mpool_by_page);
	}
	nvgpu_kfree(g, vpool);

	vm_area->vpool = NULL;

	return;
}
