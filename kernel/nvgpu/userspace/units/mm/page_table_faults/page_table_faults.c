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

#include "page_table_faults.h"
#include <unit/io.h>
#include <unit/unit.h>

#include <nvgpu/io.h>
#include <nvgpu/posix/io.h>

#include <nvgpu/gk20a.h>
#include <nvgpu/types.h>
#include <nvgpu/sizes.h>
#include <nvgpu/gmmu.h>
#include <nvgpu/mm.h>
#include <nvgpu/vm.h>
#include <nvgpu/nvgpu_sgt.h>
#include <nvgpu/fifo.h>

#include "os/posix/os_posix.h"
#include "hal/fifo/channel_gv11b.h"

#include "hal/mc/mc_gp10b.h"
#include "hal/mm/mmu_fault/mmu_fault_gv11b.h"
#include "hal/mm/mm_gv11b.h"
#include "hal/mm/cache/flush_gk20a.h"
#include "hal/mm/cache/flush_gv11b.h"
#include "hal/mm/gmmu/gmmu_gp10b.h"
#include "hal/mm/gmmu/gmmu_gv11b.h"
#include "hal/mc/mc_gv11b.h"
#include "hal/fb/fb_gp10b.h"
#include "hal/fb/fb_gm20b.h"
#include "hal/fb/fb_gv11b.h"
#include "hal/fb/intr/fb_intr_gv11b.h"
#include "hal/fifo/ramin_gk20a.h"
#include "hal/fifo/ramin_gm20b.h"
#include "hal/fifo/ramin_gv11b.h"
#include "hal/fb/fb_mmu_fault_gv11b.h"

#include <nvgpu/hw/gv11b/hw_gmmu_gv11b.h>
#include <nvgpu/hw/gv11b/hw_fb_gv11b.h>

#define TEST_PA_ADDRESS 0xEFAD80000000
#define TEST_COMP_TAG 0xEF
#define TEST_INVALID_ADDRESS 0xAAC0000000

/*
 * Write callback (for all nvgpu_writel calls).
 */
static void writel_access_reg_fn(struct gk20a *g,
			     struct nvgpu_reg_access *access)
{
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
}

/*
 * Init the minimum set of HALs to use GMMU features, then call the init_mm
 * base function.
 */
static int init_mm(struct unit_module *m, struct gk20a *g)
{
	u64 low_hole, aperture_size;
	struct nvgpu_os_posix *p = nvgpu_os_posix_from_gk20a(g);
	struct mm_gk20a *mm = &g->mm;
	int err;

	p->mm_is_iommuable = true;

	g->ops.mc.intr_stall_unit_config = mc_gp10b_intr_stall_unit_config;
	g->ops.mc.intr_nonstall_unit_config =
					mc_gp10b_intr_nonstall_unit_config;

	/* Minimum HALs for page_table */
	memset(&g->ops.bus, 0, sizeof(g->ops.bus));
	memset(&g->ops.fb, 0, sizeof(g->ops.fb));
#ifdef CONFIG_NVGPU_COMPRESSION
	g->ops.fb.compression_page_size = gp10b_fb_compression_page_size;
#endif
	g->ops.fb.tlb_invalidate = gm20b_fb_tlb_invalidate;

	g->ops.ramin.init_pdb = gv11b_ramin_init_pdb;
	g->ops.ramin.alloc_size = gk20a_ramin_alloc_size;

	g->ops.mm.init_inst_block = gv11b_mm_init_inst_block;
	g->ops.mm.gmmu.get_default_big_page_size =
		nvgpu_gmmu_default_big_page_size;
	g->ops.mm.gmmu.get_mmu_levels = gp10b_mm_get_mmu_levels;
	g->ops.mm.gmmu.get_max_page_table_levels = gp10b_get_max_page_table_levels;
	g->ops.mm.gmmu.map = nvgpu_gmmu_map_locked;
	g->ops.mm.gmmu.unmap = nvgpu_gmmu_unmap_locked;
	g->ops.mm.gmmu.get_iommu_bit = gp10b_mm_get_iommu_bit;
	g->ops.mm.gmmu.gpu_phys_addr = gv11b_gpu_phys_addr;

	/* New HALs for fault testing */
	g->ops.mc.is_mmu_fault_pending = gv11b_mc_is_mmu_fault_pending;
	g->ops.mm.mmu_fault.info_mem_destroy =
		gv11b_mm_mmu_fault_info_mem_destroy;
	g->ops.mm.mmu_fault.disable_hw = gv11b_mm_mmu_fault_disable_hw;
	g->ops.mm.setup_hw = nvgpu_mm_setup_hw;
	g->ops.mm.cache.l2_flush = gv11b_mm_l2_flush;
	g->ops.mm.cache.fb_flush = gk20a_mm_fb_flush;
	g->ops.fb.init_hw = gv11b_fb_init_hw;
	g->ops.fb.intr.enable = gv11b_fb_intr_enable;
	g->ops.fb.fault_buf_configure_hw = gv11b_fb_fault_buf_configure_hw;
	g->ops.fb.read_mmu_fault_buffer_size =
		gv11b_fb_read_mmu_fault_buffer_size;
	g->ops.fb.write_mmu_fault_buffer_size =
		gv11b_fb_write_mmu_fault_buffer_size;
	g->ops.fb.read_mmu_fault_status = gv11b_fb_read_mmu_fault_status;
	g->ops.fb.write_mmu_fault_buffer_lo_hi =
		gv11b_fb_write_mmu_fault_buffer_lo_hi;
	g->ops.fb.intr.is_mmu_fault_pending =
		gv11b_fb_intr_is_mmu_fault_pending;
	g->ops.fb.is_fault_buf_enabled = gv11b_fb_is_fault_buf_enabled;
	g->ops.fb.fault_buf_set_state_hw = gv11b_fb_fault_buf_set_state_hw;
	g->ops.ramin.set_big_page_size = gm20b_ramin_set_big_page_size;
	g->ops.channel.count = gv11b_channel_count;

	/*
	 * Define some arbitrary addresses for test purposes.
	 * Note: no need to malloc any memory as this unit only needs to trigger
	 * MMU faults via register mocking. No other memory accesses are done.
	 */
	g->mm.sysmem_flush.cpu_va = (void *) 0x10000000;
	g->mm.mmu_wr_mem.cpu_va = (void *) 0x20000000;
	g->mm.mmu_rd_mem.cpu_va = (void *) 0x30000000;

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

	mm->bar1.aperture_size = U32(16) << 20U;
	mm->bar1.vm = nvgpu_vm_init(g,
			g->ops.mm.gmmu.get_default_big_page_size(),
			SZ_64K,
			0ULL,
			nvgpu_safe_sub_u64(mm->bar1.aperture_size, SZ_64K),
			0ULL,
			true, false, false,
			"bar1");
	if (mm->bar1.vm == NULL) {
		return -ENOMEM;
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

	/*
	 * Call the init function again to cover branches checking for already
	 * initialized structures
	 */
	err = g->ops.mm.setup_hw(g);
	if (err != 0) {
		unit_return_fail(m, "init_mm_setup_hw/2 failed code=%d\n", err);
	}

	return UNIT_SUCCESS;
}

int test_page_faults_init(struct unit_module *m, struct gk20a *g, void *args)
{
	u64 debug_level = (u64)args;

	g->log_mask = 0;
	if (debug_level >= 1) {
		g->log_mask = gpu_dbg_map;
	}
	if (debug_level >= 2) {
		g->log_mask |= gpu_dbg_map_v;
	}
	if (debug_level >= 3) {
		g->log_mask |= gpu_dbg_pte;
	}

	init_platform(m, g, true);

	if (init_mm(m, g) != 0) {
		unit_return_fail(m, "nvgpu_init_mm_support failed\n");
	}

	return UNIT_SUCCESS;
}

/* Helper function to write an error in the status register */
static void write_error(struct unit_module *m, struct gk20a *g, u32 error)
{
	nvgpu_posix_io_writel_reg_space(g, fb_niso_intr_r(), error);
}

int test_page_faults_pending(struct unit_module *m, struct gk20a *g,
					void *args)
{
	if (g->ops.mc.is_mmu_fault_pending(g)) {
		unit_return_fail(m, "MMU fault already pending at init.\n");
	}

	/* Write a fault in the pending register */
	write_error(m, g, fb_niso_intr_mmu_other_fault_notify_m());

	if (!g->ops.mc.is_mmu_fault_pending(g)) {
		unit_return_fail(m, "MMU fault not pending as expected.\n");
	}

	return UNIT_SUCCESS;
}

int test_page_faults_disable_hw(struct unit_module *m, struct gk20a *g,
					void *args)
{
	g->ops.mm.mmu_fault.disable_hw(g);
	if (g->ops.fb.is_fault_buf_enabled(g,
		NVGPU_MMU_FAULT_NONREPLAY_REG_INDX)) {
			unit_return_fail(m, "Non-replay buf still enabled\n");
	}
	if (g->ops.fb.is_fault_buf_enabled(g,
		NVGPU_MMU_FAULT_REPLAY_REG_INDX)) {
			unit_return_fail(m, "Non-replay buf still enabled\n");
	}

	/* Call disable again to test some branches */
	g->ops.mm.mmu_fault.disable_hw(g);

	return UNIT_SUCCESS;
}

int test_page_faults_inst_block(struct unit_module *m, struct gk20a *g,
					void *args)
{
	struct nvgpu_mem inst_blk_desc;
	u64 scenario = (u64)args;
	u32 big_page_size = 0;

	/* Handle some corner cases */
	if (scenario == 1) {
		/* Init inst_block with large page size */
		big_page_size = g->ops.mm.gmmu.get_default_big_page_size();
	} else if (scenario == 2) {
		/* Handle branch case in gv11b_init_inst_block() */
		big_page_size = g->ops.mm.gmmu.get_default_big_page_size();
		g->ops.ramin.set_big_page_size = NULL;
	}

	if (nvgpu_alloc_inst_block(g, &inst_blk_desc) != 0) {
		unit_return_fail(m, "nvgpu_alloc_inst_block failed\n");
	}

	g->ops.mm.init_inst_block(&inst_blk_desc, g->mm.bar2.vm, big_page_size);

	return UNIT_SUCCESS;
}

int test_page_faults_clean(struct unit_module *m, struct gk20a *g, void *args)
{
	g->log_mask = 0;
	g->ops.mm.mmu_fault.info_mem_destroy(g);
	nvgpu_vm_put(g->mm.pmu.vm);
	nvgpu_vm_put(g->mm.bar2.vm);
	nvgpu_vm_put(g->mm.bar1.vm);

	return UNIT_SUCCESS;
}

struct unit_module_test nvgpu_gmmu_faults_tests[] = {
	UNIT_TEST(init, test_page_faults_init, (void *)0, 0),
	UNIT_TEST(pending, test_page_faults_pending, NULL, 0),
	UNIT_TEST(disable_hw, test_page_faults_disable_hw, NULL, 0),
	UNIT_TEST(inst_block_s0, test_page_faults_inst_block, (void *)0, 0),
	UNIT_TEST(inst_block_s1, test_page_faults_inst_block, (void *)1, 0),
	UNIT_TEST(inst_block_s2, test_page_faults_inst_block, (void *)2, 0),
	UNIT_TEST(clean, test_page_faults_clean, NULL, 0),
};

UNIT_MODULE(page_table_faults, nvgpu_gmmu_faults_tests, UNIT_PRIO_NVGPU_TEST);
