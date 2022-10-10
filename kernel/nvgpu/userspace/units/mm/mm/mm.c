/*
 * Copyright (c) 2019-2022, NVIDIA CORPORATION.  All rights reserved.
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

#include "mm.h"
#include <unit/io.h>
#include <unit/unit.h>
#include <unit/core.h>
#include <nvgpu/errata.h>

#include <nvgpu/nvgpu_init.h>
#include <nvgpu/posix/io.h>

#include "os/posix/os_posix.h"

#include "hal/mc/mc_gp10b.h"
#include "hal/mm/mm_gp10b.h"
#include "hal/mm/mm_gv11b.h"
#include "hal/mm/cache/flush_gk20a.h"
#include "hal/mm/cache/flush_gv11b.h"
#include "hal/mm/gmmu/gmmu_gm20b.h"
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

#include <nvgpu/bug.h>
#include <nvgpu/posix/dma.h>
#include <nvgpu/posix/kmem.h>
#include <nvgpu/posix/posix-fault-injection.h>

#define TEST_ADDRESS		0x10002000
#define ARBITRARY_ERROR		-42
#define ERROR_TYPE_KMEM		0
#define ERROR_TYPE_DMA		1
#define ERROR_TYPE_HAL		2

struct unit_module *current_module;
bool test_flag;
int int_empty_hal_return_error_after;

/*
 * Write callback (for all nvgpu_writel calls).
 */
static void writel_access_reg_fn(struct gk20a *g,
			     struct nvgpu_reg_access *access)
{
	if (access->addr == flush_fb_flush_r()) {
		if (access->value == flush_fb_flush_pending_busy_v()) {
			unit_info(current_module,
				"writel: setting FB_flush to not pending\n");
			access->value = 0;
		}
	} else if (access->addr == flush_l2_flush_dirty_r()) {
		if (access->value == flush_l2_flush_dirty_pending_busy_v()) {
			unit_info(current_module,
				"writel: setting L2_flush to not pending\n");
			access->value = 0;
		}
	}

	nvgpu_posix_io_writel_reg_space(g, access->addr, access->value);
	nvgpu_posix_io_record_access(g, access);
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

	/* Enable extra features to increase line coverage */
	nvgpu_set_enabled(g, NVGPU_SUPPORT_SEC2_VM, true);
	nvgpu_set_enabled(g, NVGPU_SUPPORT_GSP_VM, true);
	nvgpu_set_errata(g, NVGPU_ERRATA_MM_FORCE_128K_PMU_VM, true);
}

/*
 * Simple HAL function to exercise branches and return an arbitrary error
 * code after a given number of calls.
 */
static int int_empty_hal(struct gk20a *g)
{
	if (int_empty_hal_return_error_after > 0) {
		int_empty_hal_return_error_after--;
	}

	if (int_empty_hal_return_error_after == 0) {
		return ARBITRARY_ERROR;
	}
	return 0;
}

/* Similar HAL to mimic the bus.bar1_bind and bus.bar2_bind HALs */
static int int_empty_hal_bar_bind(struct gk20a *g, struct nvgpu_mem *bar_inst)
{
	/* Re-use int_empty_hal to leverage the error injection mechanism */
	return int_empty_hal(g);
}

/* Simple HAL with no return value */
static void void_empty_hal(struct gk20a *g)
{
	return;
}

/*
 * Helper function to factorize the testing of the many possible error cases
 * in nvgpu_init_mm_support.
 * It supports 3 types of error injection (kmalloc, dma, and empty_hal). The
 * chosen error will occur after 'count' calls. It will return 0 if the
 * expected_error occurred, and 1 otherwise.
 * The 'step' parameter is used in case of failure to more easily trace the
 * issue in logs.
 */
static int nvgpu_init_mm_support_inject_error(struct unit_module *m,
	struct gk20a *g, int error_type, int count, int expected_error,
	int step)
{
	int err;
	struct nvgpu_posix_fault_inj *dma_fi =
		nvgpu_dma_alloc_get_fault_injection();
	struct nvgpu_posix_fault_inj *kmem_fi =
		nvgpu_kmem_get_fault_injection();

	if (error_type == ERROR_TYPE_KMEM) {
		nvgpu_posix_enable_fault_injection(kmem_fi, true, count);
	} else if (error_type == ERROR_TYPE_DMA) {
		nvgpu_posix_enable_fault_injection(dma_fi, true, count);
	} else if (error_type == ERROR_TYPE_HAL) {
		int_empty_hal_return_error_after = count;
	}

	err = nvgpu_init_mm_support(g);

	nvgpu_posix_enable_fault_injection(kmem_fi, false, 0);
	nvgpu_posix_enable_fault_injection(dma_fi, false, 0);
	int_empty_hal_return_error_after = -1;

	if (err != expected_error) {
		unit_err(m, "init_mm_support didn't fail as expected step=%d err=%d\n",
			step, err);
		return 1;
	}

	return 0;
}

int test_nvgpu_init_mm(struct unit_module *m, struct gk20a *g, void *args)
{
	int err;
	int errors = 0;

	/*
	 * We need to call nvgpu_init_mm_support but first make it fail to
	 * test (numerous) error handling cases
	 */

	int_empty_hal_return_error_after = -1;

	/* Making nvgpu_alloc_sysmem_flush fail */
	errors += nvgpu_init_mm_support_inject_error(m, g, ERROR_TYPE_DMA, 0,
						     -ENOMEM, 1);

	/* Making nvgpu_init_bar1_vm fail on VM init */
	errors += nvgpu_init_mm_support_inject_error(m, g, ERROR_TYPE_KMEM, 0,
						     -ENOMEM, 2);

	/* Making nvgpu_init_bar1_vm fail on alloc_inst_block */
	errors += nvgpu_init_mm_support_inject_error(m, g, ERROR_TYPE_DMA, 2,
						     -ENOMEM, 3);

	/* Making nvgpu_init_bar2_vm fail */
	errors += nvgpu_init_mm_support_inject_error(m, g, ERROR_TYPE_DMA, 4,
						     -ENOMEM, 4);

	/* Making nvgpu_init_system_vm fail on the PMU VM init */
	errors += nvgpu_init_mm_support_inject_error(m, g, ERROR_TYPE_KMEM, 10,
						     -ENOMEM, 5);

	/* Making nvgpu_init_system_vm fail again with extra branch coverage */
	g->ops.mm.init_bar2_vm = NULL;
	errors += nvgpu_init_mm_support_inject_error(m, g, ERROR_TYPE_KMEM, 6,
						     -ENOMEM, 6);
	g->ops.mm.init_bar2_vm = gp10b_mm_init_bar2_vm;

	/* Making nvgpu_init_system_vm fail on alloc_inst_block */
	errors += nvgpu_init_mm_support_inject_error(m, g, ERROR_TYPE_DMA, 6,
						     -ENOMEM, 7);

	/* Making nvgpu_init_hwpm fail */
	errors += nvgpu_init_mm_support_inject_error(m, g, ERROR_TYPE_DMA, 7,
						     -ENOMEM, 8);

	/* Making nvgpu_init_engine_ucode_vm(sec2) fail on VM init */
	errors += nvgpu_init_mm_support_inject_error(m, g, ERROR_TYPE_KMEM, 15,
						     -ENOMEM, 9);

	/* Making nvgpu_init_engine_ucode_vm(sec2) fail on alloc_inst_block */
	errors += nvgpu_init_mm_support_inject_error(m, g, ERROR_TYPE_DMA, 9,
						     -ENOMEM, 10);

	/* Making nvgpu_init_engine_ucode_vm(gsp) fail */
	errors += nvgpu_init_mm_support_inject_error(m, g, ERROR_TYPE_DMA, 11,
						     -ENOMEM, 11);

#ifdef CONFIG_NVGPU_NON_FUSA
	/* Disable for now. */
	/* Making nvgpu_init_cde_vm fail */
	//errors += nvgpu_init_mm_support_inject_error(m, g, ERROR_TYPE_KMEM, 80,
	//					     -ENOMEM, 12);
#endif


	/* Making nvgpu_init_ce_vm fail */
	errors += nvgpu_init_mm_support_inject_error(m, g, ERROR_TYPE_KMEM, 33,
						     -ENOMEM, 12);
	/* Making nvgpu_init_mmu_debug fail on wr_mem DMA alloc */
	errors += nvgpu_init_mm_support_inject_error(m, g, ERROR_TYPE_DMA, 13,
						     -ENOMEM, 13);

	/* Making nvgpu_init_mmu_debug fail on rd_mem DMA alloc */
	errors += nvgpu_init_mm_support_inject_error(m, g, ERROR_TYPE_DMA, 14,
						     -ENOMEM, 14);

	/* Making g->ops.mm.mmu_fault.setup_sw fail */
	errors += nvgpu_init_mm_support_inject_error(m, g, ERROR_TYPE_HAL, 0,
						     ARBITRARY_ERROR, 15);

	/* Making g->ops.fb.fb_ecc_init fail */
	g->ops.fb.ecc.init = int_empty_hal;
	errors += nvgpu_init_mm_support_inject_error(m, g, ERROR_TYPE_HAL, 1,
						     ARBITRARY_ERROR, 16);
	g->ops.fb.ecc.init = NULL;

#ifdef CONFIG_NVGPU_NON_FUSA
	/*
	 * Extra cases for branch coverage: change support flags to test
	 * other branches
	 */
	nvgpu_set_enabled(g, NVGPU_SUPPORT_SEC2_VM, false);
	nvgpu_set_enabled(g, NVGPU_SUPPORT_GSP_VM, false);
	nvgpu_set_errata(g, NVGPU_ERRATA_MM_FORCE_128K_PMU_VM, false);
	g->has_cde = false;

	errors += nvgpu_init_mm_support_inject_error(m, g, ERROR_TYPE_HAL, 1,
						     ARBITRARY_ERROR, 17);

	nvgpu_set_enabled(g, NVGPU_SUPPORT_SEC2_VM, true);
	nvgpu_set_enabled(g, NVGPU_SUPPORT_GSP_VM, true);
	nvgpu_set_errata(g, NVGPU_ERRATA_MM_FORCE_128K_PMU_VM, true);
	g->has_cde = true;
#endif

	/*
	 * Extra cases for branch coverage: remove some HALs to test branches
	 * in nvgpu_init_mm_reset_enable_hw
	 */
	g->ops.mc.fb_reset = NULL;
	g->ops.fb.init_fs_state = NULL;

	errors += nvgpu_init_mm_support_inject_error(m, g, ERROR_TYPE_HAL, 1,
						     ARBITRARY_ERROR, 18);

	g->ops.mc.fb_reset = void_empty_hal;
	g->ops.fb.init_fs_state = void_empty_hal;

	if (errors != 0) {
		return UNIT_FAIL;
	}

	/* Now it should succeed */
	err = nvgpu_init_mm_support(g);
	if (err != 0) {
		unit_return_fail(m, "nvgpu_init_mm_support failed (1) err=%d\n",
				err);
	}

	/*
	 * Now running it again should succeed too but will hit some
	 * "already initialized" paths
	 */
	err = nvgpu_init_mm_support(g);
	if (err != 0) {
		unit_return_fail(m, "nvgpu_init_mm_support failed (2) err=%d\n",
				err);
	}

	/*
	 * Extra case for branch coverage: remove mmu_fault.setup_sw HALs to
	 * test branch in nvgpu_init_mm_setup_sw
	 */
	g->ops.mm.mmu_fault.setup_sw = NULL;
	g->ops.mm.setup_hw = NULL;
	g->mm.sw_ready = false;
	err = nvgpu_init_mm_support(g);
	if (err != 0) {
		unit_return_fail(m, "nvgpu_init_mm_support failed (3) err=%d\n",
				err);
	}
	g->ops.mm.mmu_fault.setup_sw = int_empty_hal;
	g->ops.mm.setup_hw = int_empty_hal;

	return UNIT_SUCCESS;
}

int test_nvgpu_mm_setup_hw(struct unit_module *m, struct gk20a *g, void *args)
{
	int err;

	/*
	 * We need to call nvgpu_mm_setup_hw but first make it fail to test
	 * error handling and other corner cases
	 */
	g->ops.bus.bar1_bind = int_empty_hal_bar_bind;
	int_empty_hal_return_error_after = 1;
	err = nvgpu_mm_setup_hw(g);
	if (err != ARBITRARY_ERROR) {
		unit_return_fail(m, "nvgpu_mm_setup_hw did not fail as expected (1) err=%d\n",
				err);
	}

	g->ops.bus.bar2_bind = int_empty_hal_bar_bind;
	int_empty_hal_return_error_after = 2;
	err = nvgpu_mm_setup_hw(g);
	if (err != ARBITRARY_ERROR) {
		unit_return_fail(m, "nvgpu_mm_setup_hw did not fail as expected (2) err=%d\n",
				err);
	}
	int_empty_hal_return_error_after = -1;
	g->ops.bus.bar1_bind = NULL;
	g->ops.bus.bar2_bind = NULL;

	/* Make flush fail */
	g->ops.mm.cache.fb_flush = int_empty_hal;
	int_empty_hal_return_error_after = 1;
	err = nvgpu_mm_setup_hw(g);
	if (err != -EBUSY) {
		unit_return_fail(m, "nvgpu_mm_setup_hw did not fail as expected (3) err=%d\n",
				err);
	}
	/* Make the 2nd call to flush fail */
	int_empty_hal_return_error_after = 2;
	err = nvgpu_mm_setup_hw(g);
	if (err != -EBUSY) {
		unit_return_fail(m, "nvgpu_mm_setup_hw did not fail as expected (4) err=%d\n",
				err);
	}
	int_empty_hal_return_error_after = -1;
	g->ops.mm.cache.fb_flush = gk20a_mm_fb_flush;

	/* Success but no branch on g->ops.fb.set_mmu_page_size != NULL */
	err = nvgpu_mm_setup_hw(g);
	if (err != 0) {
		unit_return_fail(m, "nvgpu_mm_setup_hw failed (1) err=%d\n",
				err);
	}

	/* Success but branch on g->ops.fb.set_mmu_page_size != NULL */
	g->ops.fb.set_mmu_page_size = NULL;
	err = nvgpu_mm_setup_hw(g);
	if (err != 0) {
		unit_return_fail(m, "nvgpu_mm_setup_hw failed (2) err=%d\n",
				err);
	}

	/* Success but branch on error return from g->ops.bus.bar2_bind */
	g->ops.bus.bar2_bind = int_empty_hal_bar_bind;
	err = nvgpu_mm_setup_hw(g);
	if (err != 0) {
		unit_return_fail(m, "nvgpu_mm_setup_hw failed (3) err=%d\n",
				err);
	}

	/* Success but branch on g->ops.mm.mmu_fault.setup_hw != NULL */
	g->ops.mm.mmu_fault.setup_hw = NULL;
	err = nvgpu_mm_setup_hw(g);
	if (err != 0) {
		unit_return_fail(m, "nvgpu_mm_setup_hw failed (4) err=%d\n",
				err);
	}

	return UNIT_SUCCESS;
}

int test_mm_init_hal(struct unit_module *m, struct gk20a *g, void *args)
{
	g->log_mask = 0;
	if (verbose_lvl(m) >= 1) {
		g->log_mask = gpu_dbg_map;
	}
	if (verbose_lvl(m) >= 2) {
		g->log_mask |= gpu_dbg_map_v;
	}
	if (verbose_lvl(m) >= 3) {
		g->log_mask |= gpu_dbg_pte;
	}

	current_module = m;

	init_platform(m, g, true);

	struct nvgpu_os_posix *p = nvgpu_os_posix_from_gk20a(g);

	p->mm_is_iommuable = true;
#ifdef CONFIG_NVGPU_NON_FUSA
	g->has_cde = true;
#endif

	g->ops.mc.intr_stall_unit_config = mc_gp10b_intr_stall_unit_config;
	g->ops.mc.intr_nonstall_unit_config =
					mc_gp10b_intr_nonstall_unit_config;

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

	/* Add bar2 to have more init/cleanup logic */
	g->ops.mm.init_bar2_vm = gp10b_mm_init_bar2_vm;
	g->ops.mm.init_bar2_vm(g);

	/*
	 * For extra coverage. Note: the goal of this unit test is to validate
	 * the mm.mm unit, not the underlying HALs.
	 */
	g->ops.fb.init_fs_state = void_empty_hal;
	g->ops.fb.set_mmu_page_size = void_empty_hal;
	g->ops.mc.fb_reset = void_empty_hal;
	g->ops.mm.mmu_fault.setup_hw = void_empty_hal;
	g->ops.mm.mmu_fault.setup_sw = int_empty_hal;
	g->ops.mm.setup_hw = int_empty_hal;

	nvgpu_posix_register_io(g, &mmu_faults_callbacks);

	/* Register space: FB_MMU */
	if (nvgpu_posix_io_add_reg_space(g, fb_niso_intr_r(), 0x800) != 0) {
		unit_return_fail(m, "nvgpu_posix_io_add_reg_space failed\n");
	}

	/* Register space: HW_FLUSH */
	if (nvgpu_posix_io_add_reg_space(g, flush_fb_flush_r(), 0x20) != 0) {
		unit_return_fail(m, "nvgpu_posix_io_add_reg_space failed\n");
	}

	if (g->ops.mm.is_bar1_supported(g)) {
		unit_return_fail(m, "BAR1 is not supported on Volta+\n");
	}

	return UNIT_SUCCESS;
}

static int stub_mm_l2_flush(struct gk20a *g, bool invalidate)
{
	return -ETIMEDOUT;
}

int test_mm_suspend(struct unit_module *m, struct gk20a *g, void *args)
{
	int err;
	int (*save_func)(struct gk20a *g, bool inv);

	/* Allow l2_flush failure by stubbing the call. */
	save_func = g->ops.mm.cache.l2_flush;
	g->ops.mm.cache.l2_flush = stub_mm_l2_flush;

	nvgpu_set_power_state(g, NVGPU_STATE_POWERED_OFF);
	err = nvgpu_mm_suspend(g);
	if (err != -ETIMEDOUT) {
		unit_return_fail(m, "suspend did not fail as expected err=%d\n",
				err);
	}

	/* restore original l2_flush method */
	g->ops.mm.cache.l2_flush = save_func;

	nvgpu_set_power_state(g, NVGPU_STATE_POWERED_ON);
	err = nvgpu_mm_suspend(g);
	if (err != 0) {
		unit_return_fail(m, "suspend fail err=%d\n", err);
	}

	/*
	 * Some optional HALs are executed if not NULL in nvgpu_mm_suspend.
	 * Calls above went through branches where these HAL pointers were NULL,
	 * now define them and run again for complete coverage.
	 */
	g->ops.fb.intr.disable = gv11b_fb_intr_disable;
	g->ops.mm.mmu_fault.disable_hw = gv11b_mm_mmu_fault_disable_hw;
	nvgpu_set_power_state(g, NVGPU_STATE_POWERED_ON);
	err = nvgpu_mm_suspend(g);
	if (err != 0) {
		unit_return_fail(m, "suspend fail err=%d\n", err);
	}

	return UNIT_SUCCESS;
}

int test_mm_remove_mm_support(struct unit_module *m, struct gk20a *g,
				void *args)
{
	int err;

	/*
	 * Since the last step of the removal is to call nvgpu_pd_cache_fini,
	 * g->mm.pd_cache = NULL indicates that the removal completed
	 * successfully.
	 */

	err = nvgpu_pd_cache_init(g);
	if (err != 0) {
		unit_return_fail(m, "nvgpu_pd_cache_init failed ??\n");
	}

	g->ops.mm.mmu_fault.info_mem_destroy = NULL;
	g->mm.remove_support(&g->mm);

	if (g->mm.pd_cache != NULL) {
		unit_return_fail(m, "mm removal did not complete\n");
	}

	/* Add extra HALs to cover some branches */
	g->ops.mm.mmu_fault.info_mem_destroy =
		gv11b_mm_mmu_fault_info_mem_destroy;
	g->ops.mm.remove_bar2_vm = gp10b_mm_remove_bar2_vm;
	g->mm.remove_support(&g->mm);

	/* Reset this to NULL to avoid trying to destroy the mutex again */
	g->ops.mm.mmu_fault.info_mem_destroy = NULL;

#ifdef CONFIG_NVGPU_NON_FUSA
	/* Extra cases for branch coverage */
	nvgpu_set_enabled(g, NVGPU_SUPPORT_SEC2_VM, false);
	nvgpu_set_enabled(g, NVGPU_SUPPORT_GSP_VM, false);
	g->has_cde = false;

	g->mm.remove_support(&g->mm);

	nvgpu_set_enabled(g, NVGPU_SUPPORT_SEC2_VM, true);
	nvgpu_set_enabled(g, NVGPU_SUPPORT_GSP_VM, true);
	g->has_cde = true;
#endif

	return UNIT_SUCCESS;
}

/*
 * Test: test_mm_page_sizes
 * Test a couple of page_size related functions
 */
int test_mm_page_sizes(struct unit_module *m, struct gk20a *g,
				void *args)
{
	g->ops.mm.gmmu.get_big_page_sizes = NULL;

	if (nvgpu_mm_get_default_big_page_size(g) != SZ_64K) {
		unit_return_fail(m, "unexpected big page size (1)\n");
	}
	if (nvgpu_mm_get_available_big_page_sizes(g) != SZ_64K) {
		unit_return_fail(m, "unexpected big page size (2)\n");
	}

	/* For branch/line coverage */
	g->mm.disable_bigpage = true;
	if (nvgpu_mm_get_available_big_page_sizes(g) != 0) {
		unit_return_fail(m, "unexpected big page size (3)\n");
	}
	if (nvgpu_mm_get_default_big_page_size(g) != 0) {
		unit_return_fail(m, "unexpected big page size (4)\n");
	}
	g->mm.disable_bigpage = false;

	/* Case of non NULL g->ops.mm.gmmu.get_big_page_sizes */
	g->ops.mm.gmmu.get_big_page_sizes = gm20b_mm_get_big_page_sizes;
	if (nvgpu_mm_get_available_big_page_sizes(g) != (SZ_64K | SZ_128K)) {
		unit_return_fail(m, "unexpected big page size (5)\n");
	}
	g->ops.mm.gmmu.get_big_page_sizes = NULL;

	return UNIT_SUCCESS;
}

int test_mm_inst_block(struct unit_module *m, struct gk20a *g,
				void *args)
{
	u32 addr;
	struct nvgpu_mem *block = malloc(sizeof(struct nvgpu_mem));
	int ret = UNIT_FAIL;

	memset(block, 0, sizeof(*block));
	block->aperture = APERTURE_SYSMEM;
	block->cpu_va = (void *) TEST_ADDRESS;

	g->ops.ramin.base_shift = gk20a_ramin_base_shift;
	addr = nvgpu_inst_block_ptr(g, block);

	if (addr != ((u32) TEST_ADDRESS >> g->ops.ramin.base_shift())) {
		unit_err(m, "invalid inst_block_ptr address (1)\n");
		goto cleanup;
	}

	/* Run again with NVLINK support for code coverage */
	nvgpu_set_enabled(g, NVGPU_SUPPORT_NVLINK, true);
	addr = nvgpu_inst_block_ptr(g, block);
	nvgpu_set_enabled(g, NVGPU_SUPPORT_NVLINK, false);

	if (addr != ((u32) TEST_ADDRESS >> g->ops.ramin.base_shift())) {
		unit_err(m, "invalid inst_block_ptr address (2)\n");
		goto cleanup;
	}

	ret = UNIT_SUCCESS;
cleanup:
	free(block);
	return ret;
}

int test_mm_alloc_inst_block(struct unit_module *m, struct gk20a *g,
				void *args)
{
	struct nvgpu_mem *mem = malloc(sizeof(struct nvgpu_mem));
	struct nvgpu_posix_fault_inj *dma_fi =
		nvgpu_dma_alloc_get_fault_injection();
	int result = UNIT_FAIL;

	if (nvgpu_alloc_inst_block(g, mem) != 0) {
		unit_err(m, "alloc_inst failed unexpectedly\n");
		goto cleanup;
	}

	nvgpu_posix_enable_fault_injection(dma_fi, true, 0);

	if (nvgpu_alloc_inst_block(g, mem) == 0) {
		unit_err(m, "alloc_inst did not fail as expected\n");
		goto cleanup;
	}

	result = UNIT_SUCCESS;

cleanup:
	nvgpu_posix_enable_fault_injection(dma_fi, false, 0);
	free(mem);

	return result;
}

int test_gk20a_from_mm(struct unit_module *m, struct gk20a *g, void *args)
{
	if (g != gk20a_from_mm(&(g->mm))) {
		unit_return_fail(m, "ptr mismatch in gk20a_from_mm\n");
	}

	return UNIT_SUCCESS;
}

int test_bar1_aperture_size_mb_gk20a(struct unit_module *m, struct gk20a *g,
	void *args)
{
	if (g->mm.bar1.aperture_size != (bar1_aperture_size_mb_gk20a() << 20)) {
		unit_return_fail(m, "mismatch in bar1_aperture_size\n");
	}

	return UNIT_SUCCESS;
}

struct unit_module_test nvgpu_mm_mm_tests[] = {
	UNIT_TEST(init_hal, test_mm_init_hal, NULL, 0),
	UNIT_TEST(init_mm, test_nvgpu_init_mm, NULL, 0),
	UNIT_TEST(init_mm_hw, test_nvgpu_mm_setup_hw, NULL, 0),
	UNIT_TEST(suspend, test_mm_suspend, NULL, 0),
	UNIT_TEST(remove_support, test_mm_remove_mm_support, NULL, 0),
	UNIT_TEST(page_sizes, test_mm_page_sizes, NULL, 0),
	UNIT_TEST(inst_block, test_mm_inst_block, NULL, 0),
	UNIT_TEST(alloc_inst_block, test_mm_alloc_inst_block, NULL, 0),
	UNIT_TEST(gk20a_from_mm, test_gk20a_from_mm, NULL, 0),
	UNIT_TEST(bar1_aperture_size, test_bar1_aperture_size_mb_gk20a, NULL,
		0),
};

UNIT_MODULE(mm.mm, nvgpu_mm_mm_tests, UNIT_PRIO_NVGPU_TEST);
