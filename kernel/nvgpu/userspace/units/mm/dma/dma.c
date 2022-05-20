/*
 * Copyright (c) 2019-2021, NVIDIA CORPORATION.  All rights reserved.
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

#include "dma.h"
#include <unit/io.h>
#include <unit/unit.h>

#include <nvgpu/io.h>
#include <nvgpu/posix/io.h>

#include <nvgpu/gk20a.h>
#include <nvgpu/types.h>
#include <nvgpu/sizes.h>
#include <nvgpu/mm.h>
#include <nvgpu/vm.h>
#include <nvgpu/dma.h>
#include <nvgpu/pramin.h>
#include <nvgpu/hw/gk20a/hw_pram_gk20a.h>
#include <nvgpu/hw/gk20a/hw_bus_gk20a.h>
#include "hal/bus/bus_gk20a.h"
#include "os/posix/os_posix.h"

#include "hal/mm/mm_gv11b.h"
#include "hal/mm/cache/flush_gk20a.h"
#include "hal/mm/cache/flush_gv11b.h"
#include "hal/mm/gmmu/gmmu_gp10b.h"
#include "hal/mm/gmmu/gmmu_gv11b.h"
#include "hal/fb/fb_gp10b.h"
#include "hal/fb/fb_gm20b.h"
#include "hal/fb/fb_gv11b.h"
#include "hal/fifo/ramin_gk20a.h"
#include "hal/fifo/ramin_gm20b.h"
#include "hal/fifo/ramin_gv11b.h"
#include "hal/pramin/pramin_init.h"

#include <nvgpu/posix/posix-fault-injection.h>

/* Arbitrary PA address for nvgpu_mem usage */
#define TEST_PA_ADDRESS 0xEFAD80000000

/* Create an 8MB VIDMEM area. PRAMIN has a 1MB window on this area */
#define VIDMEM_SIZE    (8*SZ_1M)

static u32 *vidmem;
static bool is_PRAM_range(struct gk20a *g, u32 addr)
{
	if ((addr >= pram_data032_r(0)) &&
	    (addr <= (pram_data032_r(0)+SZ_1M))) {
		return true;
	}
	return false;
}

static u32 PRAM_get_u32_index(struct gk20a *g, u32 addr)
{
	u32 index = addr % VIDMEM_SIZE;

	return (index)/sizeof(u32);
}

static u32 PRAM_read(struct gk20a *g, u32 addr)
{
	return vidmem[PRAM_get_u32_index(g, addr)];
}

static void PRAM_write(struct gk20a *g, u32 addr, u32 value)
{
	vidmem[PRAM_get_u32_index(g, addr)] = value;
}

/*
 * Write callback (for all nvgpu_writel calls). If address belongs to PRAM
 * range, route the call to our own handler, otherwise call the IO framework
 */
static void writel_access_reg_fn(struct gk20a *g,
			     struct nvgpu_reg_access *access)
{
	if (is_PRAM_range(g, access->addr)) {
		PRAM_write(g, access->addr - pram_data032_r(0), access->value);
	} else {
		nvgpu_posix_io_writel_reg_space(g, access->addr, access->value);
	}
	nvgpu_posix_io_record_access(g, access);
}

/*
 * Read callback, similar to the write callback above.
 */
static void readl_access_reg_fn(struct gk20a *g,
			    struct nvgpu_reg_access *access)
{
	if (is_PRAM_range(g, access->addr)) {
		access->value = PRAM_read(g, access->addr - pram_data032_r(0));
	} else {
		access->value = nvgpu_posix_io_readl_reg_space(g, access->addr);
	}
}

/*
 * Define all the callbacks to be used during the test. Typically all
 * write operations use the same callback, likewise for all read operations.
 */
static struct nvgpu_posix_io_callbacks pramin_callbacks = {
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
 * Init the minimum set of HALs to use DMA amd GMMU features, then call the
 * init_mm base function.
 */
static int init_mm(struct unit_module *m, struct gk20a *g)
{
	u64 low_hole, aperture_size;
	struct nvgpu_os_posix *p = nvgpu_os_posix_from_gk20a(g);
	struct mm_gk20a *mm = &g->mm;

	p->mm_is_iommuable = true;

	if (!nvgpu_iommuable(g)) {
		unit_return_fail(m, "Mismatch on nvgpu_iommuable\n");
	}

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

	if (g->ops.mm.is_bar1_supported(g)) {
		unit_return_fail(m, "BAR1 is not supported on Volta+\n");
	}

	/*
	 * Initialize one VM space for system memory to be used throughout this
	 * unit module.
	 * Values below are similar to those used in nvgpu_init_system_vm()
	 */
	low_hole = SZ_4K * 16UL;
	aperture_size = GK20A_PMU_VA_SIZE;
	mm->pmu.aperture_size = GK20A_PMU_VA_SIZE;
	g->ops.mm.get_default_va_sizes(NULL, &mm->channel.user_size,
		&mm->channel.kernel_size);

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
		unit_return_fail(m, "nvgpu_vm_init failed\n");
	}

	mm->bar1.aperture_size = bar1_aperture_size_mb_gk20a() << 20;
	mm->bar1.vm = nvgpu_vm_init(g,
			g->ops.mm.gmmu.get_default_big_page_size(),
			low_hole,
			0ULL,
			nvgpu_safe_sub_u64(mm->bar1.aperture_size, low_hole),
			0ULL,
			true, false, false,
			"bar1");
	if (mm->bar1.vm == NULL) {
		unit_return_fail(m, "nvgpu_vm_init failed\n");
	}

	if (nvgpu_pd_cache_init(g) != 0) {
		unit_return_fail(m, "pd cache initialization failed\n");
	}

	return UNIT_SUCCESS;
}

int test_mm_dma_init(struct unit_module *m, struct gk20a *g, void *args)
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

#ifdef CONFIG_NVGPU_DGPU
	nvgpu_init_pramin(&g->mm);
#endif

	/* Create the VIDMEM */
	vidmem = (u32 *) malloc(VIDMEM_SIZE);
	if (vidmem == NULL) {
		return UNIT_FAIL;
	}

	nvgpu_posix_register_io(g, &pramin_callbacks);

#ifdef CONFIG_NVGPU_DGPU
	/* Minimum HAL init for PRAMIN */
	g->ops.bus.set_bar0_window = gk20a_bus_set_bar0_window;
	nvgpu_pramin_ops_init(g);
	unit_assert(g->ops.pramin.data032_r != NULL, return UNIT_FAIL);
#endif

	/* Register space: BUS_BAR0 */
	if (nvgpu_posix_io_add_reg_space(g, bus_bar0_window_r(), 0x100) != 0) {
		free(vidmem);
		return UNIT_FAIL;
	}

	if (init_mm(m, g) != 0) {
		unit_return_fail(m, "nvgpu_init_mm_support failed\n");
	}

	return UNIT_SUCCESS;
}

/*
 * Helper function to create a nvgpu_mem for use throughout this unit.
 */
static struct nvgpu_mem *create_test_mem(void)
{
	struct nvgpu_mem *mem = malloc(sizeof(struct nvgpu_mem));

	memset(mem, 0, sizeof(struct nvgpu_mem));
	mem->size = SZ_4K;
	mem->cpu_va = (void *) TEST_PA_ADDRESS;
	return mem;
}

int test_mm_dma_alloc_flags(struct unit_module *m, struct gk20a *g, void *args)
{
	int err;
	struct nvgpu_os_posix *p = nvgpu_os_posix_from_gk20a(g);
	int result = UNIT_FAIL;
	struct nvgpu_mem *mem = create_test_mem();

	p->mm_is_iommuable = false;
	p->mm_sgt_is_iommuable = false;

	/* Force allocation in SYSMEM and READ_ONLY */
	err = nvgpu_dma_alloc_flags_sys(g, NVGPU_DMA_READ_ONLY, SZ_4K, mem);
	if (err != 0) {
		unit_return_fail(m, "alloc failed, err=%d\n", err);
	}
	if (mem->aperture != APERTURE_SYSMEM) {
		unit_err(m, "allocation not in SYSMEM\n");
		goto end;
	}
	nvgpu_dma_free(g, mem);

	/* Force allocation in SYSMEM and NVGPU_DMA_PHYSICALLY_ADDRESSED */
	err = nvgpu_dma_alloc_flags_sys(g, NVGPU_DMA_PHYSICALLY_ADDRESSED,
					SZ_4K, mem);
	if (err != 0) {
		unit_return_fail(m, "alloc failed, err=%d\n", err);
	}
	if (mem->aperture != APERTURE_SYSMEM) {
		unit_err(m, "allocation not in SYSMEM\n");
		goto end;
	}
	nvgpu_dma_free_sys(g, mem);

	/* Force allocation in VIDMEM and READ_ONLY */
#ifdef CONFIG_NVGPU_DGPU
	unit_info(m, "alloc_vid with READ_ONLY will cause a WARNING.");
	err = nvgpu_dma_alloc_flags_vid(g, NVGPU_DMA_READ_ONLY, SZ_4K, mem);
	if (err != 0) {
		unit_return_fail(m, "alloc failed, err=%d\n", err);
	}
	if (mem->aperture != APERTURE_VIDMEM) {
		unit_err(m, "allocation not in VIDMEM\n");
		goto end;
	}
	nvgpu_dma_free(g, mem);

	/* Force allocation in VIDMEM and NVGPU_DMA_PHYSICALLY_ADDRESSED */
	unit_info(m, "alloc_vid PHYSICALLY_ADDRESSED will cause a WARNING.");
	err = nvgpu_dma_alloc_flags_vid(g, NVGPU_DMA_PHYSICALLY_ADDRESSED,
					SZ_4K, mem);
	if (err != 0) {
		unit_return_fail(m, "alloc failed, err=%d\n", err);
	}
	if (mem->aperture != APERTURE_VIDMEM) {
		unit_err(m, "allocation not in VIDMEM\n");
		goto end;
	}
	nvgpu_dma_free(g, mem);
#endif

	result = UNIT_SUCCESS;
end:
	nvgpu_dma_free(g, mem);
	free(mem);

	return result;
}


int test_mm_dma_alloc(struct unit_module *m, struct gk20a *g, void *args)
{
	int err;
	struct nvgpu_os_posix *p = nvgpu_os_posix_from_gk20a(g);
	int result = UNIT_FAIL;
	struct nvgpu_mem *mem = create_test_mem();

	p->mm_is_iommuable = false;
	p->mm_sgt_is_iommuable = false;

	/* iGPU mode so SYSMEM allocations by default */
	init_platform(m, g, true);
	err = nvgpu_dma_alloc(g, SZ_4K, mem);
	if (err != 0) {
		unit_return_fail(m, "alloc failed, err=%d\n", err);
	}
	if (mem->aperture != APERTURE_SYSMEM) {
		unit_err(m, "allocation not in SYSMEM\n");
		goto end;
	}
	nvgpu_dma_free(g, mem);

#ifdef CONFIG_NVGPU_DGPU
	/* dGPU mode */
	init_platform(m, g, false);
	err = nvgpu_dma_alloc(g, SZ_4K, mem);
	if (err != 0) {
		unit_return_fail(m, "alloc failed, err=%d\n", err);
	}
	if (mem->aperture != APERTURE_VIDMEM) {
		unit_err(m, "allocation not in VIDMEM\n");
		goto end;
	}
	nvgpu_dma_free(g, mem);
#endif

	/* Force allocation in SYSMEM */
	err = nvgpu_dma_alloc_sys(g, SZ_4K, mem);
	if (err != 0) {
		unit_return_fail(m, "alloc failed, err=%d\n", err);
	}
	if (mem->aperture != APERTURE_SYSMEM) {
		unit_err(m, "allocation not in SYSMEM\n");
		goto end;
	}
	nvgpu_dma_free(g, mem);

#ifdef CONFIG_NVGPU_DGPU
	/* Force allocation in VIDMEM */
	init_platform(m, g, true);
	err = nvgpu_dma_alloc_vid(g, SZ_4K, mem);
	if (err != 0) {
		unit_return_fail(m, "alloc failed, err=%d\n", err);
	}
	if (mem->aperture != APERTURE_VIDMEM) {
		unit_err(m, "allocation not in VIDMEM\n");
		goto end;
	}
	nvgpu_dma_free(g, mem);
#endif

#ifdef CONFIG_NVGPU_DGPU
	/* Allocation at fixed address in VIDMEM */
	init_platform(m, g, true);
	err = nvgpu_dma_alloc_vid_at(g, SZ_4K, mem, 0x1000);
	if (err != -ENOMEM) {
		unit_err(m, "allocation did not fail as expected: %d\n", err);
		goto end;
	}
	nvgpu_dma_free(g, mem);
#endif

	result = UNIT_SUCCESS;
end:
	nvgpu_dma_free(g, mem);
	free(mem);
	return result;
}

/*
 * Test to target nvgpu_dma_alloc_map_* functions, testing allocations and GMMU
 * mappings in SYSMEM or VIDMEM.
 */
int test_mm_dma_alloc_map(struct unit_module *m, struct gk20a *g, void *args)
{
	int err;
	struct nvgpu_os_posix *p = nvgpu_os_posix_from_gk20a(g);
	int result = UNIT_FAIL;
	struct nvgpu_mem *mem = create_test_mem();

	p->mm_is_iommuable = false;
	p->mm_sgt_is_iommuable = false;

	/* iGPU mode so SYSMEM allocations by default */
	init_platform(m, g, true);
	err = nvgpu_dma_alloc_map(g->mm.pmu.vm, SZ_4K, mem);
	if (err != 0) {
		unit_return_fail(m, "alloc failed, err=%d\n", err);
	}
	if (mem->aperture != APERTURE_SYSMEM) {
		unit_err(m, "allocation not in SYSMEM\n");
		goto end;
	}
	nvgpu_dma_unmap_free(g->mm.pmu.vm, mem);

#ifdef CONFIG_NVGPU_DGPU
	/* dGPU mode */
	mem->size = SZ_4K;
	mem->cpu_va = (void *) TEST_PA_ADDRESS;
	init_platform(m, g, false);
	err = nvgpu_dma_alloc_map(g->mm.pmu.vm, SZ_4K, mem);
	if (err != 0) {
		unit_return_fail(m, "alloc failed, err=%d\n", err);
	}
	if (mem->aperture != APERTURE_VIDMEM) {
		unit_err(m, "allocation not in VIDMEM\n");
		goto end;
	}
	/*
	 * Mark SGT as freed since page_table takes care of that in VIDMEM
	 * case
	 */
	mem->priv.sgt = NULL;
	nvgpu_dma_unmap_free(g->mm.pmu.vm, mem);
#endif

	/* Force allocation in SYSMEM */
	mem->size = SZ_4K;
	mem->cpu_va = (void *) TEST_PA_ADDRESS;
	err = nvgpu_dma_alloc_map_sys(g->mm.pmu.vm, SZ_4K, mem);
	if (err != 0) {
		unit_return_fail(m, "alloc failed, err=%d\n", err);
	}
	if (mem->aperture != APERTURE_SYSMEM) {
		unit_err(m, "allocation not in SYSMEM\n");
		goto end;
	}
	mem->priv.sgt = NULL;
	nvgpu_dma_unmap_free(g->mm.pmu.vm, mem);

#ifdef CONFIG_NVGPU_DGPU
	/* Force allocation in VIDMEM */
	mem->size = SZ_4K;
	mem->cpu_va = (void *) TEST_PA_ADDRESS;
	init_platform(m, g, true);
	err = nvgpu_dma_alloc_map_vid(g->mm.pmu.vm, SZ_4K, mem);
	if (err != 0) {
		unit_return_fail(m, "alloc failed, err=%d\n", err);
	}
	if (mem->aperture != APERTURE_VIDMEM) {
		unit_err(m, "allocation not in VIDMEM\n");
		goto end;
	}
	mem->priv.sgt = NULL;
	nvgpu_dma_unmap_free(g->mm.pmu.vm, mem);
#endif

	result = UNIT_SUCCESS;
end:
	nvgpu_dma_unmap_free(g->mm.pmu.vm, mem);
	free(mem);

	return result;
}

int test_mm_dma_alloc_map_fault_injection(struct unit_module *m,
						struct gk20a *g, void *args)
{
	int err;
	struct nvgpu_posix_fault_inj *dma_fi;
	struct nvgpu_posix_fault_inj *kmem_fi;
	struct nvgpu_os_posix *p = nvgpu_os_posix_from_gk20a(g);
	int result = UNIT_FAIL;
	struct nvgpu_mem *mem = create_test_mem();

	p->mm_is_iommuable = false;
	p->mm_sgt_is_iommuable = false;

	/* iGPU mode so SYSMEM allocations by default */
	init_platform(m, g, true);

	/* Enable fault injection(0) to make nvgpu_dma_alloc_flags_sys fail */
	dma_fi = nvgpu_dma_alloc_get_fault_injection();
	nvgpu_posix_enable_fault_injection(dma_fi, true, 0);

	err = nvgpu_dma_alloc_map(g->mm.pmu.vm, SZ_4K, mem);
	if (err == 0) {
		unit_err(m, "alloc did not fail as expected (1)\n");
		nvgpu_dma_unmap_free(g->mm.pmu.vm, mem);
		goto end_dma;
	}
	nvgpu_posix_enable_fault_injection(dma_fi, false, 0);

	/*
	 * Enable fault injection(5) to make nvgpu_gmmu_map fail inside of the
	 * nvgpu_dma_alloc_flags_sys function
	 */
	kmem_fi = nvgpu_kmem_get_fault_injection();
	nvgpu_posix_enable_fault_injection(kmem_fi, true, 1);

	err = nvgpu_dma_alloc_map(g->mm.pmu.vm, SZ_4K, mem);
	if (err == 0) {
		unit_err(m, "alloc did not fail as expected (2)\n");
		nvgpu_dma_unmap_free(g->mm.pmu.vm, mem);
		goto end_kmem;
	}

	nvgpu_posix_enable_fault_injection(kmem_fi, false, 0);

	result = UNIT_SUCCESS;

end_kmem:
	nvgpu_posix_enable_fault_injection(kmem_fi, false, 0);
end_dma:
	nvgpu_posix_enable_fault_injection(dma_fi, false, 0);
	free(mem);

	return result;
}

struct unit_module_test nvgpu_mm_dma_tests[] = {
	UNIT_TEST(init, test_mm_dma_init, (void *)0, 0),
	UNIT_TEST(alloc, test_mm_dma_alloc, NULL, 0),
	UNIT_TEST(alloc_flags, test_mm_dma_alloc_flags, NULL, 0),
	UNIT_TEST(alloc_map, test_mm_dma_alloc_map, NULL, 0),
	UNIT_TEST(alloc_map_fault_inj, test_mm_dma_alloc_map_fault_injection,
		NULL, 0),
};

UNIT_MODULE(mm.dma, nvgpu_mm_dma_tests, UNIT_PRIO_NVGPU_TEST);
