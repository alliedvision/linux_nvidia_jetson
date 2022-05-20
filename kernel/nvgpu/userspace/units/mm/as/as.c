/*
 * Copyright (c) 2019-2020, NVIDIA CORPORATION.  All rights reserved.
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
#include "as.h"

#include <nvgpu/posix/io.h>
#include "os/posix/os_posix.h"

#include "hal/mm/mm_gp10b.h"
#include "hal/mm/mm_gv11b.h"
#include "hal/mm/cache/flush_gk20a.h"
#include "hal/mm/cache/flush_gv11b.h"
#include "hal/mm/gmmu/gmmu_gp10b.h"
#include "hal/mm/gmmu/gmmu_gv11b.h"
#include "hal/mm/mmu_fault/mmu_fault_gv11b.h"
#include "hal/fb/fb_gm20b.h"
#include "hal/fb/fb_gp10b.h"
#include "hal/fb/fb_gv11b.h"
#include "hal/fb/fb_mmu_fault_gv11b.h"
#include "hal/fb/intr/fb_intr_gv11b.h"
#include "hal/fifo/ramin_gk20a.h"
#include "hal/fifo/ramin_gv11b.h"
#include <nvgpu/hw/gv11b/hw_fb_gv11b.h>
#include <nvgpu/hw/gv11b/hw_flush_gv11b.h>
#include <nvgpu/as.h>
#include <nvgpu/nvgpu_common.h>
#include <nvgpu/posix/posix-fault-injection.h>

/*
 * Each allocated as_share gets a unique, incrementing, global_id. Use the
 * following global static to track the global_id and ensure they are
 * correct.
 */
static int global_id_count;

/* Parameters to test standard cases of allocation */
static struct test_parameters test_64k_user_managed = {
	.big_page_size = SZ_64K,
	.small_big_split = (SZ_1G * 56ULL),
	.flags = NVGPU_AS_ALLOC_USERSPACE_MANAGED,
	.expected_error = 0
};

static struct test_parameters test_0k_user_managed = {
	.big_page_size = 0,
	.small_big_split = 0,
	.flags = NVGPU_AS_ALLOC_USERSPACE_MANAGED,
	.expected_error = 0
};

static struct test_parameters test_64k_unified_va = {
	.big_page_size = SZ_64K,
	.small_big_split = 0,
	.flags = NVGPU_AS_ALLOC_UNIFIED_VA,
	.expected_error = 0
};

static struct test_parameters test_64k_unified_va_enabled = {
	.big_page_size = SZ_64K,
	.small_big_split = 0,
	.flags = 0,
	.expected_error = 0,
	.unify_address_spaces_flag = true
};

static struct test_parameters test_einval_user_managed = {
	.big_page_size = 1,
	.small_big_split = (SZ_1G * 56ULL),
	.flags = NVGPU_AS_ALLOC_USERSPACE_MANAGED,
	.expected_error = -EINVAL
};

static struct test_parameters test_notp2_user_managed = {
	.big_page_size = SZ_64K-1,
	.small_big_split = (SZ_1G * 56ULL),
	.flags = NVGPU_AS_ALLOC_USERSPACE_MANAGED,
	.expected_error = -EINVAL
};

/* Parameters to test corner cases and error handling */
static struct test_parameters test_64k_user_managed_as_fail = {
	.big_page_size = SZ_64K,
	.small_big_split = (SZ_1G * 56ULL),
	.flags = 0,
	.expected_error = -ENOMEM,
	.special_case = SPECIAL_CASE_AS_MALLOC_FAIL
};

static struct test_parameters test_64k_user_managed_vm_fail = {
	.big_page_size = SZ_64K,
	.small_big_split = (SZ_1G * 56ULL),
	.flags = 0,
	.expected_error = -ENOMEM,
	.special_case = SPECIAL_CASE_VM_INIT_FAIL
};

static struct test_parameters test_64k_user_managed_busy_fail_1 = {
	.big_page_size = SZ_64K,
	.small_big_split = (SZ_1G * 56ULL),
	.flags = 0,
	.expected_error = -ENODEV,
	.special_case = SPECIAL_CASE_GK20A_BUSY_ALLOC
};

static struct test_parameters test_64k_user_managed_busy_fail_2 = {
	.big_page_size = SZ_64K,
	.small_big_split = (SZ_1G * 56ULL),
	.flags = 0,
	.expected_error = 0,
	.special_case = SPECIAL_CASE_GK20A_BUSY_RELEASE
};

/*
 * Init the minimum set of HALs to use DMA amd GMMU features, then call the
 * init_mm base function.
 */
int test_init_mm(struct unit_module *m, struct gk20a *g, void *args)
{
	int err;
	struct nvgpu_os_posix *p = nvgpu_os_posix_from_gk20a(g);

	p->mm_is_iommuable = true;

	g->ops.mm.gmmu.get_default_big_page_size =
		nvgpu_gmmu_default_big_page_size;
	g->ops.mm.gmmu.get_mmu_levels = gp10b_mm_get_mmu_levels;
	g->ops.mm.gmmu.get_max_page_table_levels = gp10b_get_max_page_table_levels;
	g->ops.mm.init_inst_block = gv11b_mm_init_inst_block;
	g->ops.mm.gmmu.map = nvgpu_gmmu_map_locked;
	g->ops.mm.gmmu.unmap = nvgpu_gmmu_unmap_locked;
	g->ops.mm.gmmu.get_iommu_bit = gp10b_mm_get_iommu_bit;
	g->ops.mm.gmmu.gpu_phys_addr = gv11b_gpu_phys_addr;
	g->ops.mm.is_bar1_supported = gv11b_mm_is_bar1_supported;
	g->ops.mm.cache.l2_flush = gv11b_mm_l2_flush;
	g->ops.mm.cache.fb_flush = gk20a_mm_fb_flush;
#ifdef CONFIG_NVGPU_COMPRESSION
	g->ops.fb.compression_page_size = gp10b_fb_compression_page_size;
#endif
	g->ops.fb.tlb_invalidate = gm20b_fb_tlb_invalidate;
	g->ops.ramin.init_pdb = gv11b_ramin_init_pdb;
	g->ops.ramin.alloc_size = gk20a_ramin_alloc_size;
	g->ops.fb.is_fault_buf_enabled = gv11b_fb_is_fault_buf_enabled;
	g->ops.fb.read_mmu_fault_buffer_size =
		gv11b_fb_read_mmu_fault_buffer_size;
	g->ops.fb.init_hw = gv11b_fb_init_hw;
	g->ops.fb.intr.enable = gv11b_fb_intr_enable;
	g->ops.fb.ecc.init = NULL;

	err = nvgpu_pd_cache_init(g);
	if (err != 0) {
		unit_return_fail(m, "pd cache initialization failed\n");
	}

	err = nvgpu_init_mm_support(g);
	if (err != 0) {
		unit_return_fail(m, "nvgpu_init_mm_support failed err=%d\n",
				err);
	}

	/*
	 * Before ref_init calls to gk20a_as_alloc_share should immediately
	 * fail.
	 */
	err = gk20a_as_alloc_share(g, 0, 0, 0, 0, 0, NULL);
	if (err != -ENODEV) {
		unit_return_fail(m, "gk20a_as_alloc_share did not fail as expected err=%d\n",
				err);
	}

	nvgpu_ref_init(&g->refcount);

	return UNIT_SUCCESS;
}

int test_as_alloc_share(struct unit_module *m, struct gk20a *g, void *args)
{
	struct gk20a_as_share *out;
	int err;
	struct nvgpu_posix_fault_inj *kmem_fi =
		nvgpu_kmem_get_fault_injection();
	struct nvgpu_posix_fault_inj *nvgpu_fi =
		nvgpu_nvgpu_get_fault_injection();
	struct test_parameters *params = (struct test_parameters *) args;

	global_id_count++;

	if (params->unify_address_spaces_flag) {
		nvgpu_set_enabled(g, NVGPU_MM_UNIFY_ADDRESS_SPACES, true);
	}

	if (params->special_case == SPECIAL_CASE_AS_MALLOC_FAIL) {
		nvgpu_posix_enable_fault_injection(kmem_fi, true, 0);
	}

	if (params->special_case == SPECIAL_CASE_VM_INIT_FAIL) {
		nvgpu_posix_enable_fault_injection(kmem_fi, true, 1);
	}

	if (params->special_case == SPECIAL_CASE_GK20A_BUSY_ALLOC) {
		nvgpu_posix_enable_fault_injection(nvgpu_fi, true, 0);
	}

	err = gk20a_as_alloc_share(g, params->big_page_size,
			params->flags, (SZ_64K << 10), (1ULL << 37),
			params->small_big_split, &out);

	if (params->unify_address_spaces_flag) {
		nvgpu_set_enabled(g, NVGPU_MM_UNIFY_ADDRESS_SPACES, false);
	}

	if (params->special_case == SPECIAL_CASE_AS_MALLOC_FAIL) {
		/* The failure will cause the global_id not to be incremented */
		global_id_count--;
	}

	nvgpu_posix_enable_fault_injection(kmem_fi, false, 0);
	nvgpu_posix_enable_fault_injection(nvgpu_fi, false, 0);

	if (err != params->expected_error) {
		unit_return_fail(m, "gk20a_as_alloc_share failed err=%d\n",
			err);
	} else if (err != 0) {
		/* We got the expected error, no cleanup needed */
		return UNIT_SUCCESS;
	}

	if (out->id != global_id_count) {
		unit_return_fail(m, "unexpected out->id (%d)\n", out->id);
	}

	if (params->special_case == SPECIAL_CASE_GK20A_BUSY_RELEASE) {
		nvgpu_posix_enable_fault_injection(nvgpu_fi, true, 0);
	}

	err = gk20a_as_release_share(out);

	if (params->special_case == SPECIAL_CASE_GK20A_BUSY_RELEASE) {
		nvgpu_posix_enable_fault_injection(nvgpu_fi, false, 0);
		if (err != -ENODEV) {
			unit_return_fail(m, "gk20a_as_release_share did not fail as expected err=%d\n", err);
		}
	} else if (err != 0) {
		unit_return_fail(m, "gk20a_as_release_share failed err=%d\n",
			err);
	}
	return UNIT_SUCCESS;
}

int test_gk20a_from_as(struct unit_module *m, struct gk20a *g, void *args)
{
	int ret = UNIT_FAIL;
	struct gk20a_as_share *out;
	int err;

	err = gk20a_as_alloc_share(g, SZ_64K, NVGPU_AS_ALLOC_USERSPACE_MANAGED,
			(SZ_64K << 10), (1ULL << 37),
			nvgpu_gmmu_va_small_page_limit(), &out);
	if (err != 0) {
		unit_return_fail(m, "gk20a_as_alloc_share failed err=%d\n",
			err);
	}

	if (g != gk20a_from_as(out->as)) {
		unit_err(m, "ptr mismatch in gk20a_from_as\n");
		goto exit;
	}

	ret = UNIT_SUCCESS;

exit:
	gk20a_as_release_share(out);

	return ret;
}

struct unit_module_test nvgpu_mm_as_tests[] = {
	UNIT_TEST(init, test_init_mm, NULL, 0),
	UNIT_TEST(as_alloc_share_64k_um_as_fail, test_as_alloc_share,
		(void *) &test_64k_user_managed_as_fail, 0),
	UNIT_TEST(as_alloc_share_64k_um_vm_fail, test_as_alloc_share,
		(void *) &test_64k_user_managed_vm_fail, 0),
	UNIT_TEST(as_alloc_share_64k_um_busy_fail_1, test_as_alloc_share,
		(void *) &test_64k_user_managed_busy_fail_1, 0),
	UNIT_TEST(as_alloc_share_64k_um_busy_fail_2, test_as_alloc_share,
		(void *) &test_64k_user_managed_busy_fail_2, 0),
	UNIT_TEST(as_alloc_share_64k_um, test_as_alloc_share,
		(void *) &test_64k_user_managed, 0),
	UNIT_TEST(as_alloc_share_0k_um, test_as_alloc_share,
		(void *) &test_0k_user_managed, 0),
	UNIT_TEST(as_alloc_share_einval_um, test_as_alloc_share,
		(void *) &test_einval_user_managed, 0),
	UNIT_TEST(as_alloc_share_notp2_um, test_as_alloc_share,
		(void *) &test_notp2_user_managed, 0),
	UNIT_TEST(as_alloc_share_uva, test_as_alloc_share,
		(void *) &test_64k_unified_va, 0),
	UNIT_TEST(as_alloc_share_uva_enabled, test_as_alloc_share,
		(void *) &test_64k_unified_va_enabled, 0),
	UNIT_TEST(gk20a_from_as, test_gk20a_from_as, NULL, 0),
};

UNIT_MODULE(mm.as, nvgpu_mm_as_tests, UNIT_PRIO_NVGPU_TEST);
