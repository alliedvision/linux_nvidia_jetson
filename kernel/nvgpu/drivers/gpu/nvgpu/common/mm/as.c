/*
 * GK20A Address Spaces
 *
 * Copyright (c) 2011-2021, NVIDIA CORPORATION.  All rights reserved.
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

#include <nvgpu/trace.h>
#include <nvgpu/kmem.h>
#include <nvgpu/vm.h>
#include <nvgpu/log2.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/string.h>
#include <nvgpu/nvgpu_init.h>

#define VM_NAME_PREFIX	"as_"

/* dumb allocator... */
static int generate_as_share_id(struct gk20a_as *as)
{
	struct gk20a *g = gk20a_from_as(as);

	nvgpu_log_fn(g, " ");
	as->last_share_id = nvgpu_safe_add_s32(as->last_share_id, 1);
	return as->last_share_id;
}
/* still dumb */
static void release_as_share_id(struct gk20a_as_share *as_share)
{
	struct gk20a *g = gk20a_from_as(as_share->as);

	nvgpu_log_fn(g, " ");
	return;
}

/* address space interfaces for the gk20a module */
static int gk20a_vm_alloc_share(struct gk20a_as_share *as_share,
				u32 big_page_size, u32 flags,
				u64 va_range_start, u64 va_range_end,
				u64 va_range_split)
{
	struct gk20a_as *as = as_share->as;
	struct gk20a *g = gk20a_from_as(as);
	struct mm_gk20a *mm = &g->mm;
	struct vm_gk20a *vm;
	char name[NVGPU_VM_NAME_LEN] = VM_NAME_PREFIX;
	char *p;
	u64 user_size;
	u64 kernel_size = mm->channel.kernel_size;
	u64 pde_size, pde_size_mask;
	bool big_pages;
	const bool userspace_managed =
		(flags & NVGPU_AS_ALLOC_USERSPACE_MANAGED) != 0U;
	const bool unified_va =
		nvgpu_is_enabled(g, NVGPU_MM_UNIFY_ADDRESS_SPACES) ||
		((flags & NVGPU_AS_ALLOC_UNIFIED_VA) != 0U);

	nvgpu_log_fn(g, " ");

	if (big_page_size == 0U) {
		big_pages = false;
		big_page_size = g->ops.mm.gmmu.get_default_big_page_size();
	} else {
		if (is_power_of_2(big_page_size) == false) {
			return -EINVAL;
		}

		if ((big_page_size &
		     nvgpu_mm_get_available_big_page_sizes(g)) == 0U) {
			return -EINVAL;
		}
		big_pages = true;
	}

	pde_size = BIT64(nvgpu_vm_pde_coverage_bit_count(g, big_page_size));
	pde_size_mask = nvgpu_safe_sub_u64(pde_size, U64(1));

	if ((va_range_start == 0ULL) ||
		((va_range_start & pde_size_mask) != 0ULL)) {
		return -EINVAL;
	}

	if ((va_range_end == 0ULL) ||
		((va_range_end & pde_size_mask) != 0ULL)) {
		return -EINVAL;
	}

	if (va_range_start >= va_range_end) {
		return -EINVAL;
	}

	user_size = nvgpu_safe_sub_u64(va_range_end, va_range_start);

	if (unified_va || !big_pages) {
		if (va_range_split != 0ULL) {
			return -EINVAL;
		}
	} else {
		/* non-unified VA: split required */
		if ((va_range_split == 0ULL) ||
			((va_range_split & pde_size_mask) != 0ULL)) {
			return -EINVAL;
		}

		/* non-unified VA: split range checks */
		if ((va_range_split <= va_range_start) ||
		    (va_range_split >= va_range_end)) {
			return -EINVAL;
		}
	}

	nvgpu_log_info(g,
		"vm: low_hole=0x%llx, user_size=0x%llx, kernel_size=0x%llx",
		va_range_start, user_size, kernel_size);

	p = name + strlen(name);
	(void) nvgpu_strnadd_u32(p, nvgpu_safe_cast_s32_to_u32(as_share->id),
				 sizeof(name) - sizeof(VM_NAME_PREFIX), 10U);

	vm = nvgpu_vm_init(g, big_page_size,
			   va_range_start,
			   user_size,
			   kernel_size,
			   va_range_split,
			   big_pages, userspace_managed, unified_va, name);
	if (vm == NULL) {
		return -ENOMEM;
	}

	as_share->vm = vm;
	vm->as_share = as_share;
	vm->enable_ctag = true;

	return 0;
}

int gk20a_as_alloc_share(struct gk20a *g,
			u32 big_page_size, u32 flags, u64 va_range_start,
			u64 va_range_end, u64 va_range_split,
			struct gk20a_as_share **out)
{
	struct gk20a_as_share *as_share;
	int err = 0;

	nvgpu_log_fn(g, " ");
	g = nvgpu_get(g);
	if (g == NULL) {
		return -ENODEV;
	}

	*out = NULL;
	as_share = nvgpu_kzalloc(g, sizeof(*as_share));
	if (as_share == NULL) {
		return -ENOMEM;
	}

	as_share->as = &g->as;
	as_share->id = generate_as_share_id(as_share->as);

	/* this will set as_share->vm. */
	err = gk20a_busy(g);
	if (err != 0) {
		goto failed;
	}
	err = gk20a_vm_alloc_share(as_share, big_page_size, flags,
		va_range_start, va_range_end, va_range_split);
	gk20a_idle(g);

	if (err != 0) {
		goto failed;
	}

	*out = as_share;
	return 0;

failed:
	nvgpu_kfree(g, as_share);
	return err;
}

int gk20a_vm_release_share(struct gk20a_as_share *as_share)
{
	struct vm_gk20a *vm = as_share->vm;
	struct gk20a *g = gk20a_from_vm(vm);

	nvgpu_log_fn(g, " ");

	vm->as_share = NULL;
	as_share->vm = NULL;

	nvgpu_vm_put(vm);

	return 0;
}

/*
 * channels and the device nodes call this to release.
 * once the ref_cnt hits zero the share is deleted.
 */
int gk20a_as_release_share(struct gk20a_as_share *as_share)
{
	struct gk20a *g = as_share->vm->mm->g;
	int err;

	nvgpu_log_fn(g, " ");

	err = gk20a_busy(g);

	if (err != 0) {
		goto release_fail;
	}

	err = gk20a_vm_release_share(as_share);

	gk20a_idle(g);

release_fail:
	release_as_share_id(as_share);
	nvgpu_put(g);
	nvgpu_kfree(g, as_share);

	return err;
}

struct gk20a *gk20a_from_as(struct gk20a_as *as)
{
	return (struct gk20a *)((uintptr_t)as - offsetof(struct gk20a, as));
}
