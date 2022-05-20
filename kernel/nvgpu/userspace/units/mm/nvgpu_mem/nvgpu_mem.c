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

#include <stdlib.h>
#include <unit/io.h>
#include <unit/unit.h>

#include <nvgpu/nvgpu_mem.h>
#include <nvgpu/nvgpu_sgt.h>
#include <nvgpu/page_allocator.h>
#include <nvgpu/pramin.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/sizes.h>
#include <nvgpu/dma.h>
#include <nvgpu/posix/io.h>
#include <nvgpu/posix/posix-fault-injection.h>
#include <os/posix/os_posix.h>

#include <hal/mm/gmmu/gmmu_gp10b.h>
#include <hal/pramin/pramin_init.h>
#include <hal/bus/bus_gk20a.h>

#include <nvgpu/hw/gk20a/hw_pram_gk20a.h>
#include <nvgpu/hw/gk20a/hw_bus_gk20a.h>

#include "nvgpu_mem.h"

/*
 * MEM_ADDRESS represents arbitrary memory start address. Init function will
 * allocate MEM_PAGES number of pages in memory.
 */
#define MEM_ADDRESS	0x00040000
#define MEM_PAGES	4U
#define MEM_SIZE	(MEM_PAGES * SZ_4K)

/* Amount of test data should be less than or equal to MEM_SIZE */
#define TEST_SIZE	(2U * SZ_4K)

#if TEST_SIZE > MEM_SIZE
#error "TEST_SIZE should be less than or equal to MEM_SIZE"
#endif

static struct nvgpu_mem *test_mem;

#ifdef CONFIG_NVGPU_DGPU
/*
 * Pramin write callback (for all nvgpu_writel calls).
 * No-op as callbacks/functions are already tested in pramin module.
 */
static void writel_access_reg_fn(struct gk20a *g,
			     struct nvgpu_reg_access *access)
{
	/* No-op */
}

/*
 * Pramin read callback, similar to the write callback above.
 * Dummy return as callbacks/functions are already tested in pramin module.
 */
static void readl_access_reg_fn(struct gk20a *g,
			    struct nvgpu_reg_access *access)
{
	access->value = 0;
}

/*
 * Define pramin callbacks to be used during the test. Typically all
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

/*
 * Populate vidmem allocations.
 * These are required for testing APERTURE_VIDMEM branches.
 */
static int init_vidmem_env(struct unit_module *m, struct gk20a *g)
{
	int err;

	nvgpu_init_pramin(&g->mm);
	nvgpu_posix_register_io(g, &pramin_callbacks);

	/* Minimum HAL init for PRAMIN */
	g->ops.bus.set_bar0_window = gk20a_bus_set_bar0_window;
	nvgpu_pramin_ops_init(g);
	unit_assert(g->ops.pramin.data032_r != NULL, return -EINVAL);

	err = nvgpu_dma_alloc_vid_at(g, TEST_SIZE, test_mem, 0);
	if (err != 0) {
		return err;
	}

	return 0;
}

/* Free vidmem allocations */
static void free_vidmem_env(struct unit_module *m, struct gk20a *g)
{
	nvgpu_dma_free(g, test_mem);
	nvgpu_posix_io_delete_reg_space(g, bus_bar0_window_r());
}

int test_nvgpu_mem_vidmem(struct unit_module *m,
					struct gk20a *g, void *args)
{
	int err;
	u32 memset_pattern = 0x0000005A;
	u32 data_size = (16U * sizeof(u32));
	u32 *data_src = (u32 *) nvgpu_kmalloc(g, data_size);
	if (data_src == NULL) {
		free_vidmem_env(m, g);
		unit_return_fail(m, "Could not allocate data_src\n");
	}
	(void) memset(data_src, memset_pattern, (data_size));

	/* Reset aperture to invalid, so that init doesn't complain */
	test_mem->aperture = APERTURE_INVALID;
	err = init_vidmem_env(m, g);
	if (err != 0) {
		nvgpu_kfree(g, data_src);
		unit_return_fail(m, "Vidmem init failed with err=%d\n", err);
	}

	nvgpu_memset(g, test_mem, 0, memset_pattern, TEST_SIZE);

	nvgpu_mem_wr(g, test_mem, 0, memset_pattern);

	nvgpu_mem_rd(g, test_mem, 0);

	nvgpu_mem_wr_n(g, test_mem, 0, data_src, data_size);

	nvgpu_mem_rd_n(g, test_mem, 0, (void *)data_src, data_size);

	nvgpu_kfree(g, data_src);
	free_vidmem_env(m, g);

	/* Reset attributes */
	test_mem->aperture = APERTURE_SYSMEM;

	return UNIT_SUCCESS;
}
#endif

int test_nvgpu_aperture_mask(struct unit_module *m,
					struct gk20a *g, void *args)
{
	u32 sysmem_mask = 1;
	u32 sysmem_coh_mask = 3;
	u32 vidmem_mask = 4;
	u32 ret_ap_mask;

#ifdef CONFIG_NVGPU_DGPU
	/* Case: APERTURE_VIDMEM */
	test_mem->aperture = APERTURE_VIDMEM;
	ret_ap_mask = nvgpu_aperture_mask(g, test_mem, sysmem_mask,
				sysmem_coh_mask, vidmem_mask);
	if (ret_ap_mask != vidmem_mask) {
		unit_return_fail(m, "Vidmem mask returned incorrect\n");
	}
#endif

	/*
	 * NVGPU_MM_HONORS_APERTURE enabled
	 */
	nvgpu_set_enabled(g, NVGPU_MM_HONORS_APERTURE, true);

	/* Case: APERTURE_SYSMEM */
	test_mem->aperture = APERTURE_SYSMEM;
	if (!nvgpu_aperture_is_sysmem(test_mem->aperture)) {
		unit_return_fail(m, "Invalid aperture enum\n");
	}
	ret_ap_mask = nvgpu_aperture_mask(g, test_mem, sysmem_mask,
				sysmem_coh_mask, vidmem_mask);
	if (ret_ap_mask != sysmem_mask) {
		unit_return_fail(m, "MM_HONORS enabled: "
			"Incorrect mask returned for sysmem\n");
	}

	/* Case: APERTURE_SYSMEM_COH */
	test_mem->aperture = APERTURE_SYSMEM_COH;
	ret_ap_mask = nvgpu_aperture_mask(g, test_mem, sysmem_mask,
				sysmem_coh_mask, vidmem_mask);
	if (ret_ap_mask != sysmem_coh_mask) {
		unit_return_fail(m, "MM_HONORS enabled: "
			"Incorrect mask returned for sysmem_coh\n");
	}

	/* Case: APERTURE_INVALID */
	test_mem->aperture = APERTURE_INVALID;
	if (!EXPECT_BUG(nvgpu_aperture_mask(g, test_mem, sysmem_mask,
				sysmem_coh_mask, vidmem_mask))) {
		unit_return_fail(m, "MM_HONORS enabled: Aperture_mask "
			"did not BUG() for APERTURE_INVALID as expected\n");
	}

	/* Case: Bad aperture value. This will cover default return value */
	test_mem->aperture = 10;
	if (!EXPECT_BUG(nvgpu_aperture_mask(g, test_mem, sysmem_mask,
				sysmem_coh_mask, vidmem_mask))) {
		unit_return_fail(m, "MM_HONORS enabled: Aperture_mask"
			"did not BUG() for junk aperture as expected\n");
	}

	/*
	 * NVGPU_MM_HONORS_APERTURE disabled
	 */
	nvgpu_set_enabled(g, NVGPU_MM_HONORS_APERTURE, false);

#ifdef CONFIG_NVGPU_DGPU
	/* Case: APERTURE_SYSMEM */
	test_mem->aperture = APERTURE_SYSMEM;
	ret_ap_mask = nvgpu_aperture_mask(g, test_mem, sysmem_mask,
				sysmem_coh_mask, vidmem_mask);
	if (ret_ap_mask != vidmem_mask) {
		unit_return_fail(m, "MM_HONORS disabled: "
			"Incorrect mask returned for sysmem\n");
	}

	/* Case: APERTURE_SYSMEM_COH */
	test_mem->aperture = APERTURE_SYSMEM_COH;
	ret_ap_mask = nvgpu_aperture_mask(g, test_mem, sysmem_mask,
				sysmem_coh_mask, vidmem_mask);
	if (ret_ap_mask != vidmem_mask) {
		unit_return_fail(m, "MM_HONORS disabled: "
			"Incorrect mask returned for sysmem_coh\n");
	}
#endif

	/* Case: APERTURE_INVALID */
	test_mem->aperture = APERTURE_INVALID;
	if (!EXPECT_BUG(nvgpu_aperture_mask(g, test_mem, sysmem_mask,
				sysmem_coh_mask, vidmem_mask))) {
		unit_return_fail(m, "MM_HONORS disabled: Aperture_mask "
			"did not BUG() for APERTURE_INVALID as expected\n");
	}

	/* Case: Bad aperture value. This will cover default return value */
	test_mem->aperture = 10;
	if (!EXPECT_BUG(nvgpu_aperture_mask(g, test_mem, sysmem_mask,
				sysmem_coh_mask, vidmem_mask))) {
		unit_return_fail(m, "MM_HONORS disabled: Aperture_mask"
			"did not BUG() for junk aperture as expected\n");
	}

	/* Reset attributes */
	test_mem->aperture = APERTURE_SYSMEM;

	return UNIT_SUCCESS;
}

static const char *aperture_name_str[APERTURE_MAX_ENUM + 1] = {
		[APERTURE_INVALID]	= "INVAL",
		[APERTURE_SYSMEM]	= "SYSTEM",
		[APERTURE_SYSMEM_COH]	= "SYSCOH",
		[APERTURE_VIDMEM]	= "VIDMEM",
		[APERTURE_MAX_ENUM]	= "UNKNOWN",
};

int test_nvgpu_aperture_str(struct unit_module *m, struct gk20a *g, void *args)
{
	enum nvgpu_aperture ap = 0;
	const char *name_str;

	while (ap <= APERTURE_MAX_ENUM) {
		name_str = nvgpu_aperture_str(ap);
		if (strcmp((name_str), aperture_name_str[ap]) != 0) {
			unit_return_fail(m,
				"Incorrect aperture str for aperture %d\n", ap);
		}
		ap += 1;
	}

	return UNIT_SUCCESS;
}

int test_nvgpu_mem_iommu_translate(struct unit_module *m,
					struct gk20a *g, void *args)
{
	u64 temp_phys;
	struct nvgpu_mem_sgl *test_sgl =
			(struct nvgpu_mem_sgl *) test_mem->phys_sgt->sgl;
	struct nvgpu_os_posix *p = nvgpu_os_posix_from_gk20a(g);

	/*
	 * Case: mm is not iommuable
	 * This is specified in nvgpu_os_posix structure.
	 */
	temp_phys = nvgpu_mem_iommu_translate(g, test_sgl->phys);

	if (temp_phys != test_sgl->phys) {
		unit_return_fail(m, "iommu_translate did not return "
			"same phys as expected\n");
	}

	/*
	 * Case: mm is not iommuable
	 * But, mm_is_iommuable = true.
	 */
	p->mm_is_iommuable = true;
	g->ops.mm.gmmu.get_iommu_bit = NULL;

	temp_phys = nvgpu_mem_iommu_translate(g, test_sgl->phys);
	if (temp_phys != test_sgl->phys) {
		unit_return_fail(m, "iommu_translate: mm_is_iommuable=true: "
			"did not return same phys as expected\n");
	}

	/*
	 * Case: mm is iommuable
	 * Set HAL to enable iommu_translate
	 */
	g->ops.mm.gmmu.get_iommu_bit = gp10b_mm_get_iommu_bit;

	temp_phys = nvgpu_mem_iommu_translate(g, test_sgl->phys);
	if (temp_phys == test_sgl->phys) {
		unit_return_fail(m,
			"iommu_translate did not translate address\n");
	}

	/* Reset iommuable settings */
	p->mm_is_iommuable = false;

	return UNIT_SUCCESS;
}

int test_nvgpu_memset_sysmem(struct unit_module *m,
					struct gk20a *g, void *args)
{
	u32 i;
	u32 memset_pattern = 0x0000005A;
	u32 memset_pattern_word =
		(memset_pattern << 24) | (memset_pattern << 16) |
			(memset_pattern << 8) | (memset_pattern);

	u32 memset_words = TEST_SIZE / sizeof(u32);
	u32 *test_cpu_va = (u32 *)test_mem->cpu_va;

	/* Case: APERTURE_SYSMEM */
	test_mem->aperture = APERTURE_SYSMEM;

	nvgpu_memset(g, test_mem, 0, memset_pattern, TEST_SIZE);
	for (i = 0; i < memset_words; i++) {
		if (test_cpu_va[i] != memset_pattern_word) {
			unit_return_fail(m,
				"Memset pattern not found at offset %d\n", i);
		}
	}

	/* Case: APERTURE_INVALID */
	test_mem->aperture = APERTURE_INVALID;

	if (!EXPECT_BUG(nvgpu_memset(g, test_mem, 0,
					memset_pattern, TEST_SIZE))) {
		unit_return_fail(m, "APERTURE_INVALID: "
				"nvgpu_memset did not BUG() as expected\n");
	}

	/* Reset attributes */
	test_mem->aperture = APERTURE_SYSMEM;

	return UNIT_SUCCESS;
}

int test_nvgpu_mem_wr_rd(struct unit_module *m,
					struct gk20a *g, void *args)
{
	u32 i;
	u32 data_rd, data_words = 16U;
	u32 test_offset = 0x400;
	u32 data_pattern = 0x5A5A5A5A;
	u32 *test_cpu_va = (u32 *)test_mem->cpu_va;
	u32 data_size = (data_words * sizeof(u32));
	u32 *data_src, *data_rd_buf;
	u64 data_rd_pair;

	/* Test nvgpu_mem_is_sysmem()*/

	/* Case: APERTURE_INVALID */
	test_mem->aperture = APERTURE_INVALID;

	if (nvgpu_mem_is_sysmem(test_mem) != false) {
		unit_return_fail(m, "nvgpu_mem_is_sysmem "
			"returns true for APERTURE_INVALID\n");
	}

	if (nvgpu_mem_is_valid(test_mem) != false) {
		unit_return_fail(m, "nvgpu_mem_is_valid "
			"returns true for APERTURE_INVALID\n");
	}

	/* Case: APERTURE_SYSMEM_COH */
	test_mem->aperture = APERTURE_SYSMEM_COH;
	/* Confirm nvgpu_mem is set to SYSMEM*/
	if (nvgpu_mem_is_sysmem(test_mem) != true) {
		unit_return_fail(m, "nvgpu_mem_is_sysmem "
			"returns false for APERTURE_SYSMEM_COH\n");
	}

	/* Case: APERTURE_SYSMEM */
	test_mem->aperture = APERTURE_SYSMEM;

	if (nvgpu_mem_is_sysmem(test_mem) != true) {
		unit_return_fail(m, "nvgpu_mem_is_sysmem "
			"returns false for APERTURE_SYSMEM\n");
	}

	/* Confirm nvgpu_mem is allocated*/
	if (nvgpu_mem_is_valid(test_mem) != true) {
		unit_return_fail(m, "nvgpu_mem_is_valid "
			"returns false for APERTURE_SYSMEM\n");
	}

	/* Test read and write functions */

	/* Case: APERTURE_SYSMEM */

	nvgpu_mem_wr(g, test_mem, test_offset, data_pattern);
	if (test_cpu_va[(test_offset / (u32)sizeof(u32))] != data_pattern) {
		unit_err(m,
			"mem_wr incorrect write at offset %d\n", test_offset);
		goto return_fail;
	}

	data_rd = nvgpu_mem_rd(g, test_mem, test_offset);
	if (data_rd != data_pattern) {
		unit_err(m,
			"mem_rd data at offset %d incorrect\n", test_offset);
		goto return_fail;
	}

	data_src = (u32 *) nvgpu_kmalloc(g, data_size);
	if (data_src == NULL) {
		unit_err(m, "Could not allocate data_src\n");
		goto return_fail;
	}
	(void) memset(data_src, data_pattern, (data_size));

	nvgpu_mem_wr_n(g, test_mem, 0, data_src, data_size);
	for (i = 0; i < data_words; i++) {
		if (test_cpu_va[i] != data_src[i]) {
			unit_err(m,
				"mem_wr_n incorrect write at offset %d\n", i);
			goto free_data_src;
		}
	}

	data_rd_buf = (u32 *) nvgpu_kzalloc(g, data_size);
	if (data_rd_buf == NULL) {
		unit_err(m, "Could not allocate data_rd_buf\n");
		goto free_data_src;
	}

	nvgpu_mem_rd_n(g, test_mem, 0, (void *)data_rd_buf, data_size);
	for (i = 0; i < data_words; i++) {
		if (data_rd_buf[i] != data_src[i]) {
			unit_err(m,
				"mem_rd_n data at offset %d incorrect\n", i);
			goto free_buffers;
		}
	}

	data_rd_pair = nvgpu_mem_rd32_pair(g, test_mem, 0, 1);
	if (data_rd_pair != ((u64)data_pattern |
					((u64)data_pattern << 32ULL))) {
		unit_err(m, "nvgpu_mem_rd32_pair pattern incorrect\n");
		goto free_buffers;
	}

	/* Case: APERTURE_INVALID */
	test_mem->aperture = APERTURE_INVALID;

	if (!EXPECT_BUG(nvgpu_mem_wr(g, test_mem, test_offset, data_pattern))) {
		unit_err(m,
			"APERTURE_INVALID: mem_wr did not BUG() as expected\n");
		goto free_buffers;
	}

	if (!EXPECT_BUG(nvgpu_mem_rd(g, test_mem, test_offset))) {
		unit_err(m, "APERTURE_INVALID: "
			"mem_rd did not BUG() as expected\n");
		goto free_buffers;
	}

	if (!EXPECT_BUG(nvgpu_mem_wr_n(g, test_mem, 0, data_src, data_size))) {
		unit_err(m, "APERTURE_INVALID: "
			"mem_wr_n did not BUG() as expected\n");
		goto free_buffers;
	}

	if (!EXPECT_BUG(nvgpu_mem_rd_n(g, test_mem, 0,
				(void *)data_rd_buf, data_size))) {
		unit_err(m, "APERTURE_INVALID: "
				"mem_rd_n did not BUG() as expected\n");
		goto free_buffers;
	}

	nvgpu_kfree(g, data_src);
	data_src = NULL;

	nvgpu_kfree(g, data_rd_buf);
	data_rd_buf = NULL;

	/* Reset attribute */
	test_mem->aperture = APERTURE_SYSMEM;

	return UNIT_SUCCESS;

free_buffers:
	nvgpu_kfree(g, data_rd_buf);
free_data_src:
	nvgpu_kfree(g, data_src);
return_fail:
	return UNIT_FAIL;
}

/*
 * Test: test_nvgpu_mem_phys_ops
 */
int test_nvgpu_mem_phys_ops(struct unit_module *m,
					struct gk20a *g, void *args)
{
	u64 ret;
	struct nvgpu_gmmu_attrs *attrs = NULL;
	struct nvgpu_sgt *test_sgt = test_mem->phys_sgt;
	void *test_sgl = test_sgt->sgl;

	void *temp_sgl = test_sgt->ops->sgl_next(test_sgl);

	if (temp_sgl != NULL) {
		unit_return_fail(m,
			"nvgpu_mem_phys_sgl_next not NULL as expected\n");
	}

	ret = test_sgt->ops->sgl_dma(test_sgl);
	if (ret != MEM_ADDRESS) {
		unit_return_fail(m,
		"nvgpu_mem_phys_sgl_dma not equal to phys as expected\n");
	}

	ret = test_sgt->ops->sgl_phys(g, test_sgl);
	if (ret != MEM_ADDRESS) {
		unit_return_fail(m,
		"nvgpu_mem_phys_sgl_phys not equal to phys as expected\n");
	}

	ret = test_sgt->ops->sgl_ipa(g, test_sgl);
	if (ret != MEM_ADDRESS) {
		unit_return_fail(m, "nvgpu_mem_phys_sgl_ipa incorrect\n");
	}

	ret = test_sgt->ops->sgl_ipa_to_pa(g, test_sgl, 0ULL, NULL);
	if (ret != 0ULL) {
		unit_return_fail(m,
			"nvgpu_mem_phys_sgl_ipa_to_pa not zero as expected\n");
	}

	ret = test_sgt->ops->sgl_length(test_sgl);
	if (ret != MEM_SIZE) {
		unit_return_fail(m, "nvgpu_mem_phys_sgl_length incorrect\n");
	}

	ret = test_sgt->ops->sgl_gpu_addr(g, test_sgl, attrs);
	if (ret != MEM_ADDRESS) {
		unit_return_fail(m, "nvgpu_mem_phys_sgl_gpu_addr incorrect\n");
	}

	if (test_sgt->ops->sgt_iommuable != NULL) {
		unit_return_fail(m, "physical nvgpu_mems is not IOMMU'able\n");
	}

	/*
	 * Test nvgpu_mem_phys_sgt_free - No-op
	 */
	test_sgt->ops->sgt_free(g, test_sgt);

	return UNIT_SUCCESS;
}

int test_nvgpu_mem_create_from_phys(struct unit_module *m,
					struct gk20a *g, void *args)
{
	int err;
	struct nvgpu_posix_fault_inj *kmem_fi =
		nvgpu_kmem_get_fault_injection();

	test_mem = (struct nvgpu_mem *)
			nvgpu_kzalloc(g, sizeof(struct nvgpu_mem));
	if (test_mem == NULL) {
		unit_return_fail(m, "Couldn't allocate memory for nvgpu_mem\n");
	}

	/*
	 * Test 1 - Enable SW fault injection and check that init function
	 * fails with -ENOMEM.
	 */
	nvgpu_posix_enable_fault_injection(kmem_fi, true, 0);

	err = nvgpu_mem_create_from_phys(g, test_mem, MEM_ADDRESS, MEM_PAGES);
	if (err != -ENOMEM) {
		unit_return_fail(m,
			"nvgpu_mem_create_from_phys didn't fail as expected\n");
	}

	nvgpu_posix_enable_fault_injection(kmem_fi, false, 0);

	/*
	 * Test 2 - Enable SW fault injection for second allocation and
	 * check that init function fails with -ENOMEM.
	 */
	nvgpu_posix_enable_fault_injection(kmem_fi, true, 1);

	err = nvgpu_mem_create_from_phys(g, test_mem, MEM_ADDRESS, MEM_PAGES);
	if (err != -ENOMEM) {
		unit_return_fail(m,
			"nvgpu_mem_create_from_phys didn't fail as expected\n");
	}

	nvgpu_posix_enable_fault_injection(kmem_fi, false, 0);

	/*
	 * Test 3 - Check that physical memory is inited successfully
	 * Use this allocated memory for next tests in module.
	 */
	err = nvgpu_mem_create_from_phys(g, test_mem, MEM_ADDRESS, MEM_PAGES);

	if (err != 0) {
		unit_return_fail(m, "nvgpu_mem_create_from_phys init failed\n");
	}

	if (nvgpu_mem_get_phys_addr(g, test_mem) != ((u64) test_mem->cpu_va)) {
		unit_return_fail(m, "invalid physical address\n");
	}

	if (nvgpu_mem_get_addr(g, test_mem) != ((u64) test_mem->cpu_va)) {
		unit_return_fail(m, "invalid nvgpu_mem_get_addr address\n");
	}

	/* Allocate cpu_va for later tests */
	test_mem->cpu_va = nvgpu_kzalloc(g, MEM_SIZE);
	if (test_mem->cpu_va == NULL) {
		nvgpu_kfree(g, test_mem);
		unit_return_fail(m, "Could not allocate memory for cpu_va\n");
	}

	return UNIT_SUCCESS;
}

int test_nvgpu_mem_create_from_mem(struct unit_module *m, struct gk20a *g,
			void *args)
{
	struct nvgpu_mem dest_mem;

	nvgpu_mem_create_from_mem(g, &dest_mem, test_mem, 0, 2);

	unit_assert(dest_mem.cpu_va == test_mem->cpu_va, goto done);
	unit_assert(dest_mem.size == (2 * NVGPU_CPU_PAGE_SIZE), goto done);
	unit_assert((dest_mem.mem_flags & NVGPU_MEM_FLAG_SHADOW_COPY) == true,
			goto done);
	unit_assert(dest_mem.aperture == APERTURE_SYSMEM, goto done);

	return UNIT_SUCCESS;
done:
	unit_return_fail(m, "%s: failed!\n", __func__);
}

int test_free_nvgpu_mem(struct unit_module *m, struct gk20a *g, void *args)
{
	test_mem->aperture = APERTURE_SYSMEM;
	nvgpu_dma_free(g, test_mem);
	nvgpu_kfree(g, test_mem);

	return UNIT_SUCCESS;
}


struct unit_module_test nvgpu_mem_tests[] = {
	/*
	 * Init test should run first in order to use allocated memory.
	 */
	UNIT_TEST(mem_create_from_phys,	test_nvgpu_mem_create_from_phys, NULL, 0),
	/*
	 * Tests for SYSMEM
	 */
	UNIT_TEST(nvgpu_mem_phys_ops,	test_nvgpu_mem_phys_ops,	NULL, 2),
	UNIT_TEST(nvgpu_memset_sysmem,	test_nvgpu_memset_sysmem,	NULL, 0),
	UNIT_TEST(nvgpu_mem_wr_rd,	test_nvgpu_mem_wr_rd,		NULL, 0),
	UNIT_TEST(mem_iommu_translate,	test_nvgpu_mem_iommu_translate,	NULL, 2),

	/*
	 * Tests covering VIDMEM branches
	 */
	UNIT_TEST(nvgpu_aperture_mask,	test_nvgpu_aperture_mask,	NULL, 0),
	UNIT_TEST(nvgpu_aperture_name,	test_nvgpu_aperture_str,	NULL, 0),
	UNIT_TEST(create_mem_from_mem,	test_nvgpu_mem_create_from_mem,	NULL, 0),
#ifdef CONFIG_NVGPU_DGPU
	UNIT_TEST(nvgpu_mem_vidmem,	test_nvgpu_mem_vidmem,		NULL, 2),
#endif
	/*
	 * Free test should be executed at the end to free allocated memory.
	 * As nvgpu_mem doesn't not have an explicit free function for sysmem,
	 * this test doesn't cover any nvgpu_mem code.
	 */
	UNIT_TEST(test_free_nvgpu_mem,	test_free_nvgpu_mem,		NULL, 0),
};

UNIT_MODULE(nvgpu_mem, nvgpu_mem_tests, UNIT_PRIO_NVGPU_TEST);
