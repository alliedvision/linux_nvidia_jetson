/*
 * Copyright (c) 2018-2020, NVIDIA CORPORATION.  All rights reserved.
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

#include <nvgpu/io.h>
#include <nvgpu/posix/io.h>

#include <nvgpu/gk20a.h>
#include <nvgpu/types.h>
#include <nvgpu/vm.h>

#include "os/posix/os_posix.h"

#include "hal/fb/fb_gv11b.h"
#include "hal/fb/intr/fb_intr_gv11b.h"
#include "hal/fifo/ramin_gk20a.h"
#include "hal/fifo/ramin_gv11b.h"
#include "hal/mm/cache/flush_gk20a.h"
#include "hal/mm/gmmu/gmmu_gp10b.h"
#include "hal/mm/mm_gv11b.h"
#include "hal/mm/mmu_fault/mmu_fault_gv11b.h"
#include "hal/mc/mc_gp10b.h"

#include <nvgpu/hw/gv11b/hw_fb_gv11b.h>

#include "mm-gv11b-fusa.h"

/*
 * Write callback (for all nvgpu_writel calls).
 */
static void writel_access_reg_fn(struct gk20a *g,
			     struct nvgpu_reg_access *access)
{
	nvgpu_posix_io_writel_reg_space(g, access->addr, access->value);
}

/*
 * Read callback, similar to the write callback above.
 */
static void readl_access_reg_fn(struct gk20a *g,
			    struct nvgpu_reg_access *access)
{
	access->value = nvgpu_posix_io_readl_reg_space(g, access->addr);
}

/*
 * Define all the callbacks to be used during the test. Typically all
 * write operations use the same callback, likewise for all read operations.
 */
static struct nvgpu_posix_io_callbacks mmu_faults_callbacks = {
	/* Write APIs all can use the same accessor. */
	.writel          = writel_access_reg_fn,
	.writel_check    = writel_access_reg_fn,
	.bar1_writel     = writel_access_reg_fn,
	.usermode_writel = writel_access_reg_fn,

	/* Likewise for the read APIs. */
	.__readl         = readl_access_reg_fn,
	.readl           = readl_access_reg_fn,
	.bar1_readl      = readl_access_reg_fn,
};

static void init_platform(struct unit_module *m, struct gk20a *g, bool is_iGPU)
{
	if (is_iGPU) {
		nvgpu_set_enabled(g, NVGPU_MM_UNIFIED_MEMORY, true);
	} else {
		nvgpu_set_enabled(g, NVGPU_MM_UNIFIED_MEMORY, false);
	}
}

static int init_mm(struct unit_module *m, struct gk20a *g)
{
	u64 low_hole, aperture_size;
	struct nvgpu_os_posix *p = nvgpu_os_posix_from_gk20a(g);
	struct mm_gk20a *mm = &g->mm;
	int err;

	p->mm_is_iommuable = true;

	/* Minimum HALs for page_table */
	memset(&g->ops.bus, 0, sizeof(g->ops.bus));
	memset(&g->ops.fb, 0, sizeof(g->ops.fb));
	g->ops.fb.init_hw = gv11b_fb_init_hw;
	g->ops.fb.intr.enable = gv11b_fb_intr_enable;
	g->ops.ramin.init_pdb = gv11b_ramin_init_pdb;
	g->ops.ramin.alloc_size = gk20a_ramin_alloc_size;
	g->ops.mc.intr_stall_unit_config = mc_gp10b_intr_stall_unit_config;
	g->ops.mm.gmmu.get_default_big_page_size =
					nvgpu_gmmu_default_big_page_size;
	g->ops.mm.gmmu.get_mmu_levels = gp10b_mm_get_mmu_levels;
	g->ops.mm.setup_hw = nvgpu_mm_setup_hw;
	g->ops.mm.cache.fb_flush = gk20a_mm_fb_flush;
	g->ops.mm.mmu_fault.info_mem_destroy =
					gv11b_mm_mmu_fault_info_mem_destroy;

	nvgpu_posix_register_io(g, &mmu_faults_callbacks);

	/* Register space: FB_MMU */
	if (nvgpu_posix_io_add_reg_space(g, fb_niso_intr_r(), 0x800) != 0) {
		unit_return_fail(m, "nvgpu_posix_io_add_reg_space failed\n");
	}

	/*
	 * Initialize VM space for system memory to be used throughout this
	 * unit module.
	 * Values below are similar to those used in nvgpu_init_system_vm()
	 */
	low_hole = SZ_4K * 16UL;
	aperture_size = GK20A_PMU_VA_SIZE;
	mm->pmu.aperture_size = GK20A_PMU_VA_SIZE;

	mm->pmu.vm = nvgpu_vm_init(g,
				   g->ops.mm.gmmu.get_default_big_page_size(),
				   low_hole,
				   0ULL,
				   nvgpu_safe_sub_u64(aperture_size, low_hole),
				   0ULL,
				   true,
				   false,
				   false,
				   "system");
	if (mm->pmu.vm == NULL) {
		unit_return_fail(m, "'system' nvgpu_vm_init failed\n");
	}

	/* BAR2 memory space */
	mm->bar2.aperture_size = U32(32) << 20U;
	mm->bar2.vm = nvgpu_vm_init(g,
		g->ops.mm.gmmu.get_default_big_page_size(),
		SZ_4K, 0ULL, nvgpu_safe_sub_u64(mm->bar2.aperture_size, SZ_4K),
		0ULL, false, false, false, "bar2");
	if (mm->bar2.vm == NULL) {
		unit_return_fail(m, "'bar2' nvgpu_vm_init failed\n");
	}

	/*
	 * This initialization will make sure that correct aperture mask
	 * is returned */
	g->mm.mmu_wr_mem.aperture = APERTURE_SYSMEM;
	g->mm.mmu_rd_mem.aperture = APERTURE_SYSMEM;

	/* Init MM H/W */
	err = g->ops.mm.setup_hw(g);
	if (err != 0) {
		unit_return_fail(m, "init_mm_setup_hw failed code=%d\n", err);
	}

	return UNIT_SUCCESS;
}

int test_env_init_mm_gv11b_fusa(struct unit_module *m, struct gk20a *g,
								void *args)
{
	g->log_mask = 0;

	init_platform(m, g, true);

	if (init_mm(m, g) != 0) {
		unit_return_fail(m, "nvgpu_init_mm_support failed\n");
	}

	return UNIT_SUCCESS;
}

#define F_INIT_INST_BLOCK_SET_BIG_PAGE_ZERO		1ULL
#define F_INIT_INST_BLOCK_SET_BIG_PAGE_SIZE_NULL	2ULL
#define F_INIT_INST_BLOCK_INIT_SUBCTX_PDB_NULL		3ULL

int test_gv11b_mm_init_inst_block(struct unit_module *m, struct gk20a *g,
					void *args)
{
	struct nvgpu_mem inst_block;
	struct gpu_ops gops = g->ops;
	int err;
	u64 branch = (u64)args;
	u32 big_page_size;

	big_page_size = branch == F_INIT_INST_BLOCK_SET_BIG_PAGE_ZERO ?
			0U : g->ops.mm.gmmu.get_default_big_page_size();

	g->ops.ramin.set_big_page_size =
		branch == F_INIT_INST_BLOCK_SET_BIG_PAGE_SIZE_NULL ?
		NULL : gops.ramin.set_big_page_size;

	g->ops.ramin.init_subctx_pdb =
		branch == F_INIT_INST_BLOCK_INIT_SUBCTX_PDB_NULL ?
		NULL : gops.ramin.init_subctx_pdb;

	err = nvgpu_alloc_inst_block(g, &inst_block);
	if (err != 0) {
		unit_return_fail(m, "could not alloc inst block\n");
	}

	gv11b_mm_init_inst_block(&inst_block, g->mm.bar2.vm, big_page_size);

	return UNIT_SUCCESS;
}

int test_gv11b_mm_is_bar1_supported(struct unit_module *m, struct gk20a *g,
					void *args)
{
	bool ret = gv11b_mm_is_bar1_supported(g);
	if (ret != false) {
		unit_return_fail(m, "FAIL! bar1 is supported on Volta\n");
	}

	return UNIT_SUCCESS;
}

int test_env_clean_mm_gv11b_fusa(struct unit_module *m, struct gk20a *g,
								void *args)
{
	g->log_mask = 0;
	g->ops.mm.mmu_fault.info_mem_destroy(g);
	nvgpu_vm_put(g->mm.pmu.vm);
	nvgpu_vm_put(g->mm.bar2.vm);

	return UNIT_SUCCESS;
}

struct unit_module_test mm_gv11b_fusa_tests[] = {
	UNIT_TEST(env_init, test_env_init_mm_gv11b_fusa, (void *)0, 0),
	UNIT_TEST(inst_block_s0, test_gv11b_mm_init_inst_block, (void *)0U, 0),
	UNIT_TEST(inst_block_s1, test_gv11b_mm_init_inst_block, (void *)1U, 0),
	UNIT_TEST(inst_block_s2, test_gv11b_mm_init_inst_block, (void *)2U, 0),
	UNIT_TEST(inst_block_s3, test_gv11b_mm_init_inst_block, (void *)3U, 0),
	UNIT_TEST(is_bar1_supported, test_gv11b_mm_is_bar1_supported, NULL, 0),
	UNIT_TEST(env_clean, test_env_clean_mm_gv11b_fusa, NULL, 0),
};

UNIT_MODULE(mm_gv11b_fusa, mm_gv11b_fusa_tests, UNIT_PRIO_NVGPU_TEST);
