/*
 * Copyright (c) 2018-2021, NVIDIA CORPORATION.  All rights reserved.
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

#include "hal/mm/cache/flush_gk20a.h"

#include <nvgpu/hw/gv11b/hw_flush_gv11b.h>

#include <nvgpu/posix/posix-fault-injection.h>
#include <nvgpu/posix/dma.h>

#include "flush-gk20a-fusa.h"

/*
 * Write callback (for all nvgpu_writel calls).
 */
#define WR_FLUSH_0	0
#define WR_FLUSH_1	1
#define WR_FLUSH_2	2
#define WR_FLUSH_3	3
#define WR_FLUSH_ACTUAL				0
#define WR_FLUSH_TEST_FB_FLUSH_ADDR		1
#define WR_FLUSH_TEST_L2_FLUSH_DIRTY_ADDR	2
#define WR_FLUSH_TEST_L2_SYSTEM_INVALIDATE	3

static u32 write_specific_value;
static u32 write_specific_addr;

static void writel_access_reg_fn(struct gk20a *g,
			     struct nvgpu_reg_access *access)
{
	if (((write_specific_addr == WR_FLUSH_TEST_FB_FLUSH_ADDR) &&
			(access->addr == flush_fb_flush_r())) ||
		((write_specific_addr == WR_FLUSH_TEST_L2_FLUSH_DIRTY_ADDR) &&
			(access->addr == flush_l2_flush_dirty_r())) ||
		((write_specific_addr == WR_FLUSH_TEST_L2_SYSTEM_INVALIDATE) &&
			(access->addr == flush_l2_system_invalidate_r()))) {
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

	/*
	 * This initialization will make sure that correct aperture mask
	 * is returned */
	g->mm.mmu_wr_mem.aperture = APERTURE_SYSMEM;
	g->mm.mmu_rd_mem.aperture = APERTURE_SYSMEM;

	return UNIT_SUCCESS;
}

int test_env_init_flush_gk20a_fusa(struct unit_module *m, struct gk20a *g,
								void *args)
{
	g->log_mask = 0;

	init_platform(m, g, true);

	if (init_mm(m, g) != 0) {
		unit_return_fail(m, "nvgpu_init_mm_support failed\n");
	}

	write_specific_value = 0;
	write_specific_addr = 0;

	return UNIT_SUCCESS;
}

#define F_GK20A_FB_FLUSH_DEFAULT_INPUT			0
#define F_GK20A_FB_FLUSH_GET_RETRIES			1
#define F_GK20A_FB_FLUSH_PENDING_TRUE			2
#define F_GK20A_FB_FLUSH_OUTSTANDING_TRUE		3
#define F_GK20A_FB_FLUSH_OUTSTANDING_PENDING_TRUE	4
#define F_GK20A_FB_FLUSH_DUMP_VPR_WPR_INFO		5
#define F_GK20A_FB_FLUSH_NVGPU_POWERED_OFF		6

const char *m_gk20a_mm_fb_flush_str[] = {
	"default_input",
	"get_flush_retries",
	"fb_flush_pending_true",
	"fb_flush_outstanding_true",
	"fb_flush_outstanding_pending_true",
	"nvgpu_powered_off",
};

static u32 stub_mm_get_flush_retries(struct gk20a *g, enum nvgpu_flush_op op)
{
	return 100U;
}

static void stub_fb_dump_vpr_info(struct gk20a *g)
{
}

static void stub_fb_dump_wpr_info(struct gk20a *g)
{
}

int test_gk20a_mm_fb_flush(struct unit_module *m, struct gk20a *g, void *args)
{
	int err;
	int ret = UNIT_FAIL;
	u64 branch = (u64)args;

	nvgpu_set_power_state(g, NVGPU_STATE_POWERED_ON);
	write_specific_addr = WR_FLUSH_TEST_FB_FLUSH_ADDR;

	switch (branch) {
	case F_GK20A_FB_FLUSH_PENDING_TRUE:
		write_specific_value = WR_FLUSH_1;
		break;
	case F_GK20A_FB_FLUSH_OUTSTANDING_TRUE:
		write_specific_value = WR_FLUSH_2;
		break;
	case F_GK20A_FB_FLUSH_OUTSTANDING_PENDING_TRUE:
		write_specific_value = WR_FLUSH_3;
		break;
	case F_GK20A_FB_FLUSH_DUMP_VPR_WPR_INFO:
		write_specific_value = WR_FLUSH_1;
		break;
	default:
		write_specific_value = WR_FLUSH_0;
		break;
	}

	g->ops.mm.get_flush_retries = branch == F_GK20A_FB_FLUSH_GET_RETRIES ?
			stub_mm_get_flush_retries : NULL;

	g->ops.fb.dump_vpr_info = branch == F_GK20A_FB_FLUSH_DUMP_VPR_WPR_INFO ?
			stub_fb_dump_vpr_info : NULL;

	g->ops.fb.dump_wpr_info = branch == F_GK20A_FB_FLUSH_DUMP_VPR_WPR_INFO ?
			stub_fb_dump_wpr_info : NULL;

	if (branch == F_GK20A_FB_FLUSH_NVGPU_POWERED_OFF) {
		nvgpu_set_power_state(g, NVGPU_STATE_POWERED_OFF);
	}

	err = gk20a_mm_fb_flush(g);

	if ((branch == F_GK20A_FB_FLUSH_PENDING_TRUE) ||
		(branch == F_GK20A_FB_FLUSH_OUTSTANDING_TRUE) ||
		(branch == F_GK20A_FB_FLUSH_OUTSTANDING_PENDING_TRUE) ||
		(branch == F_GK20A_FB_FLUSH_DUMP_VPR_WPR_INFO)) {
		unit_assert(err != 0, goto done);
	} else {
		unit_assert(err == 0, goto done);
	}

	ret = UNIT_SUCCESS;
done:
	if (ret != UNIT_SUCCESS) {
		unit_err(m, "%s: failed at %s\n", __func__,
					m_gk20a_mm_fb_flush_str[branch]);
	}
	write_specific_addr = WR_FLUSH_ACTUAL;
	return ret;
}

#define F_GK20A_L2_FLUSH_DEFAULT_INPUT			0
#define F_GK20A_L2_FLUSH_GET_RETRIES			1
#define F_GK20A_L2_FLUSH_PENDING_TRUE			2
#define F_GK20A_L2_FLUSH_OUTSTANDING_TRUE		3
#define F_GK20A_L2_FLUSH_INVALIDATE			4
#define F_GK20A_L2_FLUSH_NVGPU_POWERED_OFF		5

const char *m_gk20a_mm_l2_flush_str[] = {
	"default_input",
	"get_flush_retries",
	"l2_flush_pending_true",
	"l2_flush_outstanding_true",
	"l2_flush_invalidate",
	"nvgpu_powered_off",
};

int test_gk20a_mm_l2_flush(struct unit_module *m, struct gk20a *g, void *args)
{
	int err;
	int ret = UNIT_FAIL;
	u64 branch = (u64)args;
	bool invalidate;

	nvgpu_set_power_state(g, NVGPU_STATE_POWERED_ON);
	write_specific_addr = WR_FLUSH_TEST_L2_FLUSH_DIRTY_ADDR;

	switch (branch) {
	case F_GK20A_L2_FLUSH_PENDING_TRUE:
		write_specific_value = WR_FLUSH_1;
		break;
	case F_GK20A_L2_FLUSH_OUTSTANDING_TRUE:
		write_specific_value = WR_FLUSH_2;
		break;
	default:
		write_specific_value = WR_FLUSH_0;
		break;
	}

	g->ops.mm.get_flush_retries = (branch == F_GK20A_L2_FLUSH_GET_RETRIES) ?
			stub_mm_get_flush_retries : NULL;

	invalidate = (branch == F_GK20A_L2_FLUSH_INVALIDATE) ? true : false;

	if (branch == F_GK20A_L2_FLUSH_NVGPU_POWERED_OFF) {
		nvgpu_set_power_state(g, NVGPU_STATE_POWERED_OFF);
	}

	err = gk20a_mm_l2_flush(g, invalidate);

	if ((branch == F_GK20A_L2_FLUSH_PENDING_TRUE) ||
		(branch == F_GK20A_L2_FLUSH_OUTSTANDING_TRUE)) {
		unit_assert(err != 0, goto done);
	} else {
		unit_assert(err == 0, goto done);
	}

	ret = UNIT_SUCCESS;
done:
	if (ret != UNIT_SUCCESS) {
		unit_err(m, "%s: failed at %s\n", __func__,
					m_gk20a_mm_l2_flush_str[branch]);
	}
	write_specific_addr = WR_FLUSH_ACTUAL;
	return ret;
}

#define F_GK20A_L2_INVALIDATE_DEFAULT_INPUT	0
#define F_GK20A_L2_INVALIDATE_PENDING_TRUE	1
#define F_GK20A_L2_INVALIDATE_OUTSTANDING_TRUE	2
#define F_GK20A_L2_INVALIDATE_GET_RETRIES_NULL	3
#define F_GK20A_L2_INVALIDATE_NVGPU_POWERED_OFF	4


const char *m_gk20a_mm_l2_invalidate_str[] = {
	"invalidate_default_input",
	"invalidate_l2_pending_true",
	"invalidate_l2_outstanding_true",
	"invalidate_get_flush_retries_null",
};

static u32 global_count = 100;
static u32 count;

static u32 stub_mm_get_flush_retries_count(struct gk20a *g,
							enum nvgpu_flush_op op)
{
	count = global_count++;
	return 100U;
}

int test_gk20a_mm_l2_invalidate(struct unit_module *m, struct gk20a *g,
								void *args)
{
	int ret = UNIT_FAIL;
	u64 branch = (u64)args;

	nvgpu_set_power_state(g, NVGPU_STATE_POWERED_ON);
	write_specific_addr = WR_FLUSH_TEST_L2_SYSTEM_INVALIDATE;

	switch (branch) {
	case F_GK20A_L2_INVALIDATE_PENDING_TRUE:
		write_specific_value = WR_FLUSH_1;
		break;
	case F_GK20A_L2_INVALIDATE_OUTSTANDING_TRUE:
		write_specific_value = WR_FLUSH_2;
		break;
	default:
		write_specific_value = WR_FLUSH_0;
		break;
	}

	g->ops.mm.get_flush_retries =
			(branch == F_GK20A_L2_INVALIDATE_GET_RETRIES_NULL) ?
			NULL : stub_mm_get_flush_retries_count;

	if (branch == F_GK20A_L2_INVALIDATE_NVGPU_POWERED_OFF) {
		nvgpu_set_power_state(g, NVGPU_STATE_POWERED_OFF);
	}

	gk20a_mm_l2_invalidate(g);

	if (branch != F_GK20A_L2_INVALIDATE_GET_RETRIES_NULL) {
		unit_assert(count == (global_count - 1U), goto done);
	}

	ret = UNIT_SUCCESS;
done:
	if (ret != UNIT_SUCCESS) {
		unit_err(m, "%s: failed at %s\n", __func__,
					m_gk20a_mm_l2_invalidate_str[branch]);
	}
	write_specific_addr = WR_FLUSH_ACTUAL;
	return ret;
}

int test_env_clean_flush_gk20a_fusa(struct unit_module *m, struct gk20a *g,
								void *args)
{
	g->log_mask = 0;

	nvgpu_vm_put(g->mm.pmu.vm);

	return UNIT_SUCCESS;
}

struct unit_module_test mm_flush_gk20a_fusa_tests[] = {
	UNIT_TEST(env_init, test_env_init_flush_gk20a_fusa, NULL, 0),
	UNIT_TEST(mm_fb_flush_s0, test_gk20a_mm_fb_flush, (void *)F_GK20A_FB_FLUSH_DEFAULT_INPUT, 0),
	UNIT_TEST(mm_fb_flush_s1, test_gk20a_mm_fb_flush, (void *)F_GK20A_FB_FLUSH_GET_RETRIES, 0),
	UNIT_TEST(mm_fb_flush_s2, test_gk20a_mm_fb_flush, (void *)F_GK20A_FB_FLUSH_PENDING_TRUE, 0),
	UNIT_TEST(mm_fb_flush_s3, test_gk20a_mm_fb_flush, (void *)F_GK20A_FB_FLUSH_OUTSTANDING_TRUE, 0),
	UNIT_TEST(mm_fb_flush_s4, test_gk20a_mm_fb_flush, (void *)F_GK20A_FB_FLUSH_OUTSTANDING_PENDING_TRUE, 0),
	UNIT_TEST(mm_fb_flush_s5, test_gk20a_mm_fb_flush, (void *)F_GK20A_FB_FLUSH_DUMP_VPR_WPR_INFO, 0),
	UNIT_TEST(mm_fb_flush_s6, test_gk20a_mm_fb_flush, (void *)F_GK20A_FB_FLUSH_NVGPU_POWERED_OFF, 0),
	UNIT_TEST(mm_l2_flush_s0, test_gk20a_mm_l2_flush, (void *)F_GK20A_L2_FLUSH_DEFAULT_INPUT, 0),
	UNIT_TEST(mm_l2_flush_s1, test_gk20a_mm_l2_flush, (void *)F_GK20A_L2_FLUSH_GET_RETRIES, 0),
	UNIT_TEST(mm_l2_flush_s2, test_gk20a_mm_l2_flush, (void *)F_GK20A_L2_FLUSH_PENDING_TRUE, 0),
	UNIT_TEST(mm_l2_flush_s3, test_gk20a_mm_l2_flush, (void *)F_GK20A_L2_FLUSH_OUTSTANDING_TRUE, 0),
	UNIT_TEST(mm_l2_flush_s4, test_gk20a_mm_l2_flush, (void *)F_GK20A_L2_FLUSH_INVALIDATE, 0),
	UNIT_TEST(mm_l2_flush_s5, test_gk20a_mm_l2_flush, (void *)F_GK20A_L2_FLUSH_NVGPU_POWERED_OFF, 0),
	UNIT_TEST(mm_l2_invalidate_s0, test_gk20a_mm_l2_invalidate, (void *)F_GK20A_L2_INVALIDATE_DEFAULT_INPUT, 0),
	UNIT_TEST(mm_l2_invalidate_s1, test_gk20a_mm_l2_invalidate, (void *)F_GK20A_L2_INVALIDATE_PENDING_TRUE, 0),
	UNIT_TEST(mm_l2_invalidate_s2, test_gk20a_mm_l2_invalidate, (void *)F_GK20A_L2_INVALIDATE_OUTSTANDING_TRUE, 0),
	UNIT_TEST(mm_l2_invalidate_s3, test_gk20a_mm_l2_invalidate, (void *)F_GK20A_L2_INVALIDATE_GET_RETRIES_NULL, 0),
	UNIT_TEST(mm_l2_invalidate_s4, test_gk20a_mm_l2_invalidate, (void *)F_GK20A_L2_INVALIDATE_NVGPU_POWERED_OFF, 0),
	UNIT_TEST(env_clean, test_env_clean_flush_gk20a_fusa, NULL, 0),
};

UNIT_MODULE(flush_gk20a_fusa, mm_flush_gk20a_fusa_tests, UNIT_PRIO_NVGPU_TEST);
