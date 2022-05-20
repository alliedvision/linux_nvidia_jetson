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
#include <nvgpu/nvgpu_mem.h>
#include <nvgpu/nvgpu_init.h>

#include "os/posix/os_posix.h"

#include "hal/fb/fb_gv11b.h"
#include "hal/fb/intr/fb_intr_gv11b.h"
#include "hal/fifo/ramin_gk20a.h"
#include "hal/fifo/ramin_gv11b.h"
#include "hal/mm/cache/flush_gk20a.h"
#include "hal/mm/gmmu/gmmu_gp10b.h"
#include "hal/mm/mm_gp10b.h"
#include "hal/mm/mm_gv11b.h"
#include "hal/mm/mmu_fault/mmu_fault_gv11b.h"

#include "hal/mm/cache/flush_gv11b.h"

#include <nvgpu/hw/gv11b/hw_flush_gv11b.h>

#include <nvgpu/posix/posix-fault-injection.h>
#include <nvgpu/posix/dma.h>

#include "flush-gv11b-fusa.h"

/*
 * Write callback (for all nvgpu_writel calls).
 */
#define WR_FLUSH_0	0
#define WR_FLUSH_1	1

static u32 write_specific_value;

static void writel_access_reg_fn(struct gk20a *g,
			     struct nvgpu_reg_access *access)
{
	if (access->addr == flush_l2_flush_dirty_r()) {
		nvgpu_posix_io_writel_reg_space(g, access->addr,
							write_specific_value);
	} else {
		nvgpu_posix_io_writel_reg_space(g, access->addr, access->value);
	}
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

	p->mm_is_iommuable = true;

	/* Minimum HALs for page_table */
	g->ops.mm.gmmu.get_default_big_page_size =
					nvgpu_gmmu_default_big_page_size;
	g->ops.mm.init_inst_block = gv11b_mm_init_inst_block;
	g->ops.mm.gmmu.get_mmu_levels = gp10b_mm_get_mmu_levels;
	g->ops.ramin.init_pdb = gv11b_ramin_init_pdb;
	g->ops.ramin.alloc_size = gk20a_ramin_alloc_size;
	g->ops.mm.setup_hw = nvgpu_mm_setup_hw;
	g->ops.fb.init_hw = gv11b_fb_init_hw;
	g->ops.fb.intr.enable = gv11b_fb_intr_enable;
	g->ops.mm.cache.fb_flush = gk20a_mm_fb_flush;
	g->ops.mm.mmu_fault.info_mem_destroy =
					gv11b_mm_mmu_fault_info_mem_destroy;

	nvgpu_posix_register_io(g, &mmu_faults_callbacks);

	/* Register space: FB_MMU */
	if (nvgpu_posix_io_add_reg_space(g, flush_fb_flush_r(), 0x800) != 0) {
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

	/* BAR1 memory space */
	mm->bar1.aperture_size = U32(16) << 20U;
	mm->bar1.vm = nvgpu_vm_init(g,
		g->ops.mm.gmmu.get_default_big_page_size(),
		SZ_4K, 0ULL, nvgpu_safe_sub_u64(mm->bar1.aperture_size, SZ_4K),
		0ULL, false, false, false, "bar1");
	if (mm->bar1.vm == NULL) {
		unit_return_fail(m, "'bar1' nvgpu_vm_init failed\n");
	}

	/*
	 * This initialization will make sure that correct aperture mask
	 * is returned */
	g->mm.mmu_wr_mem.aperture = APERTURE_SYSMEM;
	g->mm.mmu_rd_mem.aperture = APERTURE_SYSMEM;

	return UNIT_SUCCESS;
}

int test_env_init_flush_gv11b_fusa(struct unit_module *m, struct gk20a *g,
								void *args)
{
	g->log_mask = 0;

	init_platform(m, g, true);

	if (init_mm(m, g) != 0) {
		unit_return_fail(m, "nvgpu_init_mm_support failed\n");
	}

	write_specific_value = 0;

	return UNIT_SUCCESS;
}

#define F_GV11B_L2_FLUSH_PASS_BAR1_BIND_NOT_NULL	0
#define F_GV11B_L2_FLUSH_PASS_BAR1_BIND_NULL		1
#define F_GV11B_L2_FLUSH_FB_FLUSH_FAIL			2
#define F_GV11B_L2_FLUSH_L2_FLUSH_FAIL			3
#define F_GV11B_L2_FLUSH_TLB_INVALIDATE_FAIL		4
#define F_GV11B_L2_FLUSH_FB_FLUSH2_FAIL			5

const char *m_gv11b_mm_l2_flush_str[] = {
	"pass_bar1_bind_not_null",
	"pass_bar1_bind_null",
	"fb_flush_fail",
	"l2_flush_fail",
	"tlb_invalidate_fail",
	"fb_flush_2_fail",
};

static u32 stub_fb_flush_fail;
static bool stub_tlb_invalidate_fail;

static int stub_mm_fb_flush(struct gk20a *g)
{
	if (stub_fb_flush_fail == 0) {
		return -EBUSY;
	}

	stub_fb_flush_fail--;
	return 0;
}

static int stub_bus_bar1_bind(struct gk20a *g, struct nvgpu_mem *bar1_inst)
{
	return 0;
}

static int stub_fb_tlb_invalidate(struct gk20a *g, struct nvgpu_mem *pdb)
{
	if (stub_tlb_invalidate_fail) {
		return -ETIMEDOUT;
	}

	return 0;
}

int test_gv11b_mm_l2_flush(struct unit_module *m, struct gk20a *g, void *args)
{
	struct gpu_ops gops = g->ops;
	int err;
	int ret = UNIT_FAIL;
	u64 branch = (u64)args;

	nvgpu_set_power_state(g, NVGPU_STATE_POWERED_ON);
	g->ops.mm.cache.fb_flush = stub_mm_fb_flush;
	g->ops.fb.tlb_invalidate = stub_fb_tlb_invalidate;

	stub_fb_flush_fail = (branch == F_GV11B_L2_FLUSH_FB_FLUSH_FAIL) ?
		0U : (branch == F_GV11B_L2_FLUSH_FB_FLUSH2_FAIL ? 1U : 2U);

	/* Write data to flush dirty addr will control l2_flush() output */
	write_specific_value = branch == F_GV11B_L2_FLUSH_L2_FLUSH_FAIL ?
			WR_FLUSH_1 : WR_FLUSH_0;

	g->ops.bus.bar1_bind =
		((branch == F_GV11B_L2_FLUSH_PASS_BAR1_BIND_NULL) ||
				(branch == F_GV11B_L2_FLUSH_FB_FLUSH2_FAIL)) ?
		NULL : stub_bus_bar1_bind;

	stub_tlb_invalidate_fail =
		branch == F_GV11B_L2_FLUSH_TLB_INVALIDATE_FAIL ? true : false;

	err = gv11b_mm_l2_flush(g, false);

	unit_info(m, "%p\n", g->mm.bar1.vm->pdb.mem);

	if ((branch == F_GV11B_L2_FLUSH_PASS_BAR1_BIND_NOT_NULL) ||
			(branch == F_GV11B_L2_FLUSH_PASS_BAR1_BIND_NULL)) {
		unit_assert(err == 0, goto done);
	} else {
		unit_assert(err != 0, goto done);
	}

	ret = UNIT_SUCCESS;
done:
	if (ret != UNIT_SUCCESS) {
		unit_err(m, "%s: failed at %s\n", __func__,
					m_gv11b_mm_l2_flush_str[branch]);
	}
	nvgpu_set_power_state(g, NVGPU_STATE_POWERED_OFF);
	g->ops = gops;
	return ret;
}

int test_env_clean_flush_gv11b_fusa(struct unit_module *m, struct gk20a *g,
								void *args)
{
	g->log_mask = 0;

	nvgpu_vm_put(g->mm.pmu.vm);
	nvgpu_vm_put(g->mm.bar1.vm);

	return UNIT_SUCCESS;
}

struct unit_module_test mm_flush_gv11b_fusa_tests[] = {
	UNIT_TEST(env_init, test_env_init_flush_gv11b_fusa, NULL, 0),
	UNIT_TEST(mm_l2_flush_s0, test_gv11b_mm_l2_flush, (void *)F_GV11B_L2_FLUSH_PASS_BAR1_BIND_NOT_NULL, 0),
	UNIT_TEST(mm_l2_flush_s1, test_gv11b_mm_l2_flush, (void *)F_GV11B_L2_FLUSH_PASS_BAR1_BIND_NULL, 0),
	UNIT_TEST(mm_l2_flush_s2, test_gv11b_mm_l2_flush, (void *)F_GV11B_L2_FLUSH_FB_FLUSH_FAIL, 0),
	UNIT_TEST(mm_l2_flush_s3, test_gv11b_mm_l2_flush, (void *)F_GV11B_L2_FLUSH_L2_FLUSH_FAIL, 0),
	UNIT_TEST(mm_l2_flush_s4, test_gv11b_mm_l2_flush, (void *)F_GV11B_L2_FLUSH_TLB_INVALIDATE_FAIL, 0),
	UNIT_TEST(mm_l2_flush_s5, test_gv11b_mm_l2_flush, (void *)F_GV11B_L2_FLUSH_FB_FLUSH2_FAIL, 0),
	UNIT_TEST(env_clean, test_env_clean_flush_gv11b_fusa, NULL, 0),
};

UNIT_MODULE(flush_gv11b_fusa, mm_flush_gv11b_fusa_tests, UNIT_PRIO_NVGPU_TEST);
