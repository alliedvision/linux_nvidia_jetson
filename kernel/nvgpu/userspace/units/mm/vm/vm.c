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

#include "vm.h"
#include <stdio.h>

#include <unit/unit.h>
#include <unit/io.h>
#include <unit/unit-requirement-ids.h>

#include <nvgpu/posix/types.h>
#include <os/posix/os_posix.h>

#include <nvgpu/gk20a.h>
#include <nvgpu/nvgpu_sgt.h>
#include <nvgpu/vm_area.h>
#include <nvgpu/pd_cache.h>

#include <hal/mm/cache/flush_gk20a.h>
#include <hal/mm/cache/flush_gv11b.h>
#include <hal/mm/gmmu/gmmu_gp10b.h>
#include <hal/mm/gmmu/gmmu_gv11b.h>
#include <hal/mm/mm_gp10b.h>
#include <hal/fb/fb_gp10b.h>
#include <hal/fb/fb_gm20b.h>

#include <nvgpu/hw/gv11b/hw_gmmu_gv11b.h>

#include <nvgpu/bug.h>
#include <nvgpu/posix/posix-fault-injection.h>

/* Random CPU physical address for the buffers we'll map */
#define BUF_CPU_PA		0xEFAD0000ULL
#define TEST_BATCH_NUM_BUFFERS	10
#define PHYS_ADDR_BITS_HIGH	0x00FFFFFFU
#define PHYS_ADDR_BITS_LOW	0xFFFFFF00U
/* Check if address is aligned at the requested boundary */
#define IS_ALIGNED(addr, align)	((addr & (align - 1U)) == 0U)

/* Define some special cases (bitfield) */
#define NO_SPECIAL_CASE			0
#define SPECIAL_CASE_DOUBLE_MAP		1
#define SPECIAL_CASE_NO_FREE		2
#define SPECIAL_CASE_NO_VM_AREA		4

/* Expected bit count from nvgpu_vm_pde_coverage_bit_count() */
#define GP10B_PDE_BIT_COUNT		21U

/*
 * Helper function used to create custom SGTs from a list of SGLs.
 * The created SGT needs to be explicitly free'd.
 */
static struct nvgpu_sgt *custom_sgt_create(struct unit_module *m,
					   struct gk20a *g,
					   struct nvgpu_mem *mem,
					   struct nvgpu_mem_sgl *sgl_list,
					   u32 nr_sgls)
{
	int ret = 0;
	struct nvgpu_sgt *sgt = NULL;

	if (mem == NULL) {
		unit_err(m, "mem is NULL\n");
		goto fail;
	}
	if (sgl_list == NULL) {
		unit_err(m, "sgl_list is NULL\n");
		goto fail;
	}

	ret = nvgpu_mem_posix_create_from_list(g, mem, sgl_list, nr_sgls);
	if (ret != 0) {
		unit_err(m, "Failed to create mem from sgl list\n");
		goto fail;
	}

	sgt = nvgpu_sgt_create_from_mem(g, mem);
	if (sgt == NULL) {
		goto fail;
	}

	return sgt;

fail:
	unit_err(m, "Failed to create sgt\n");
	return NULL;
}

/*
 * TODO: This function is copied from the gmmu/page table unit test. Instead of
 * duplicating code, share a single implementation of the function.
 */
static inline bool pte_is_valid(u32 *pte)
{
	return ((pte[0] & gmmu_new_pte_valid_true_f()) != 0U);
}

/*
 * TODO: This function is copied from the gmmu/page table unit test. Instead of
 * duplicating code, share a single implementation of the function.
 */
static u64 pte_get_phys_addr(struct unit_module *m, u32 *pte)
{
	u64 addr_bits;

	if (pte == NULL) {
		unit_err(m, "pte is NULL\n");
		unit_err(m, "Failed to get phys addr\n");
		return 0;
	}

	addr_bits = ((u64)(pte[1] & PHYS_ADDR_BITS_HIGH)) << 32;
	addr_bits |= (u64)(pte[0] & PHYS_ADDR_BITS_LOW);
	addr_bits >>= 8;
	return (addr_bits << gmmu_new_pde_address_shift_v());
}

/* Dummy HAL for mm_init_inst_block */
static void hal_mm_init_inst_block(struct nvgpu_mem *inst_block,
	struct vm_gk20a *vm, u32 big_page_size)
{
}

/* Dummy HAL for vm_as_free_share */
static void hal_vm_as_free_share(struct vm_gk20a *vm)
{
}

/* Dummy HAL for fb_tlb_invalidate that always fails */
static int hal_fb_tlb_invalidate_error(struct gk20a *g, struct nvgpu_mem *pdb)
{
	return -1;
}

/* Dummy HAL for vm_as_alloc_share that always fails */
static int hal_vm_as_alloc_share_error(struct gk20a *g, struct vm_gk20a *vm)
{
	return -1;
}

/* Dummy HAL for vm_as_alloc_share that always succeeds */
static int hal_vm_as_alloc_share_success(struct gk20a *g, struct vm_gk20a *vm)
{
	return 0;
}

/* Initialize test environment */
static int init_test_env(struct unit_module *m, struct gk20a *g)
{
	struct nvgpu_os_posix *p = nvgpu_os_posix_from_gk20a(g);
	if (p == NULL) {
		unit_err(m, "posix is NULL\n");
		unit_err(m, "Failed to initialize test environment\n");
		return UNIT_FAIL;
	}
	p->mm_is_iommuable = true;

	nvgpu_set_enabled(g, NVGPU_MM_UNIFIED_MEMORY, true);
	nvgpu_set_enabled(g, NVGPU_HAS_SYNCPOINTS, true);

#ifdef CONFIG_NVGPU_COMPRESSION
	g->ops.fb.compression_page_size = gp10b_fb_compression_page_size;
#endif
	g->ops.fb.tlb_invalidate = gm20b_fb_tlb_invalidate;

	g->ops.mm.gmmu.get_default_big_page_size =
					nvgpu_gmmu_default_big_page_size;
	g->ops.mm.gmmu.get_mmu_levels = gp10b_mm_get_mmu_levels;
	g->ops.mm.gmmu.get_max_page_table_levels = gp10b_get_max_page_table_levels;
	g->ops.mm.gmmu.map = nvgpu_gmmu_map_locked;
	g->ops.mm.gmmu.unmap = nvgpu_gmmu_unmap_locked;
	g->ops.mm.gmmu.get_iommu_bit = gp10b_mm_get_iommu_bit;
	g->ops.mm.gmmu.gpu_phys_addr = gv11b_gpu_phys_addr;
	g->ops.mm.cache.l2_flush = gv11b_mm_l2_flush;
	g->ops.mm.cache.fb_flush = gk20a_mm_fb_flush;
	g->ops.mm.get_default_va_sizes = gp10b_mm_get_default_va_sizes,
	g->ops.mm.init_inst_block = hal_mm_init_inst_block;
	g->ops.mm.vm_as_free_share = hal_vm_as_free_share;
	g->ops.mm.vm_bind_channel = nvgpu_vm_bind_channel;
	g->ops.bus.bar1_bind = NULL;

	if (nvgpu_pd_cache_init(g) != 0) {
		unit_return_fail(m, "PD cache init failed.\n");
	}

	return UNIT_SUCCESS;
}

#define NV_KIND_INVALID -1

static struct vm_gk20a *create_test_vm(struct unit_module *m, struct gk20a *g)
{
	struct vm_gk20a *vm;
	u64 low_hole = SZ_1M * 64;
	u64 kernel_reserved = 4 * SZ_1G - low_hole;
	u64 aperture_size = 128 * SZ_1G;
	u64 user_vma = aperture_size - low_hole - kernel_reserved;

	unit_info(m, "Initializing VM:\n");
	unit_info(m, "   - Low Hole Size = 0x%llx\n", low_hole);
	unit_info(m, "   - User Aperture Size = 0x%llx\n", user_vma);
	unit_info(m, "   - Kernel Reserved Size = 0x%llx\n", kernel_reserved);
	unit_info(m, "   - Total Aperture Size = 0x%llx\n", aperture_size);
	vm = nvgpu_vm_init(g,
			   g->ops.mm.gmmu.get_default_big_page_size(),
			   low_hole,
			   user_vma,
			   kernel_reserved,
			   nvgpu_gmmu_va_small_page_limit(),
			   true,
			   false,
			   true,
			   __func__);
	return vm;
}

int test_nvgpu_vm_alloc_va(struct unit_module *m, struct gk20a *g,
	void *__args)
{
	struct vm_gk20a *vm = create_test_vm(m, g);
	int ret = UNIT_FAIL;
	struct nvgpu_posix_fault_inj *kmem_fi =
		nvgpu_kmem_get_fault_injection();
	u64 addr;

	/* Error handling: invalid page size */
	addr = nvgpu_vm_alloc_va(vm, SZ_1K, GMMU_NR_PAGE_SIZES);
	if (addr != 0) {
		unit_err(m, "nvgpu_vm_alloc_va did not fail as expected (2).\n");
		ret = UNIT_FAIL;
		goto exit;
	}

	/* Error handling: unsupported page size */
	vm->big_pages = false;
	addr = nvgpu_vm_alloc_va(vm, SZ_1K, GMMU_PAGE_SIZE_BIG);
	if (addr != 0) {
		unit_err(m, "nvgpu_vm_alloc_va did not fail as expected (3).\n");
		ret = UNIT_FAIL;
		goto exit;
	}

	/* Make the PTE allocation fail */
	nvgpu_posix_enable_fault_injection(kmem_fi, true, 0);
	addr = nvgpu_vm_alloc_va(vm, SZ_1K, 0);
	nvgpu_posix_enable_fault_injection(kmem_fi, false, 0);
	if (addr != 0) {
		unit_err(m, "nvgpu_vm_alloc_va did not fail as expected (4).\n");
		ret = UNIT_FAIL;
		goto exit;
	}

	/* Now make it succeed */
	addr = nvgpu_vm_alloc_va(vm, SZ_1K, 0);
	if (addr == 0) {
		unit_err(m, "Failed to allocate a VA\n");
		ret = UNIT_FAIL;
		goto exit;
	}

	/* And now free it */
	nvgpu_vm_free_va(vm, addr, 0);

	ret = UNIT_SUCCESS;
exit:
	if (vm != NULL) {
		nvgpu_vm_put(vm);
	}

	return ret;
}

static u32 fb_tlb_invalidate_calls;
static u32 fb_tlb_invalidate_fail_mask;

static int test_fail_fb_tlb_invalidate(struct gk20a *g, struct nvgpu_mem *pdb)
{
	bool fail = (fb_tlb_invalidate_fail_mask & 1U) != 0U;

	fb_tlb_invalidate_fail_mask >>= 1U;
	fb_tlb_invalidate_calls++;

	return fail ? -ETIMEDOUT : 0;
}

int test_map_buffer_error_cases(struct unit_module *m, struct gk20a *g,
	void *__args)
{
	struct vm_gk20a *vm;
	int ret = UNIT_FAIL;
	struct nvgpu_os_buffer os_buf = {0};
	struct nvgpu_mem_sgl sgl_list[1];
	struct nvgpu_mem mem = {0};
	struct nvgpu_sgt *sgt = NULL;
	size_t buf_size = SZ_4K;
	struct nvgpu_mapped_buf *mapped_buf = NULL;
	struct nvgpu_posix_fault_inj *kmem_fi =
		nvgpu_kmem_get_fault_injection();

	vm = create_test_vm(m, g);

	if (vm == NULL) {
		unit_err(m, "vm is NULL\n");
		ret = UNIT_FAIL;
		goto exit;
	}

	/* Allocate a CPU buffer */
	os_buf.buf = nvgpu_kzalloc(g, buf_size);
	if (os_buf.buf == NULL) {
		unit_err(m, "Failed to allocate a CPU buffer\n");
		ret = UNIT_FAIL;
		goto free_sgt_os_buf;
	}
	os_buf.size = buf_size;

	memset(&sgl_list[0], 0, sizeof(sgl_list[0]));
	sgl_list[0].phys = BUF_CPU_PA;
	sgl_list[0].dma = 0;
	sgl_list[0].length = buf_size;

	mem.size = buf_size;
	mem.cpu_va = os_buf.buf;

	/* Create sgt */
	sgt = custom_sgt_create(m, g, &mem, sgl_list, 1);
	if (sgt == NULL) {
		ret = UNIT_FAIL;
		goto free_sgt_os_buf;
	}

	/* Non-fixed offset with userspace managed VM */
	vm->userspace_managed = true;
	ret = nvgpu_vm_map(vm,
			   &os_buf,
			   sgt,
			   0,
			   buf_size,
			   0,
			   gk20a_mem_flag_none,
			   NVGPU_VM_MAP_ACCESS_READ_WRITE,
			   NVGPU_VM_MAP_CACHEABLE,
			   NV_KIND_INVALID,
			   0,
			   NULL,
			   APERTURE_SYSMEM,
			   &mapped_buf);
	vm->userspace_managed = false;
	if (ret != -EINVAL) {
		unit_err(m, "nvgpu_vm_map did not fail as expected (1)\n");
		ret = UNIT_FAIL;
		goto free_sgt_os_buf;
	}

	/* Invalid buffer size */
	os_buf.size = 0;
	ret = nvgpu_vm_map(vm,
			   &os_buf,
			   sgt,
			   0,
			   buf_size,
			   0,
			   gk20a_mem_flag_none,
			   NVGPU_VM_MAP_ACCESS_READ_WRITE,
			   NVGPU_VM_MAP_CACHEABLE,
			   NV_KIND_INVALID,
			   0,
			   NULL,
			   APERTURE_SYSMEM,
			   &mapped_buf);
	os_buf.size = buf_size;
	if (ret != -EINVAL) {
		unit_err(m, "nvgpu_vm_map did not fail as expected (2)\n");
		ret = UNIT_FAIL;
		goto free_sgt_os_buf;
	}

	/* Make the mapped_buffer allocation fail */
	nvgpu_posix_enable_fault_injection(kmem_fi, true, 0);
	ret = nvgpu_vm_map(vm,
			   &os_buf,
			   sgt,
			   0,
			   buf_size,
			   0,
			   gk20a_mem_flag_none,
			   NVGPU_VM_MAP_ACCESS_READ_WRITE,
			   NVGPU_VM_MAP_CACHEABLE,
			   NV_KIND_INVALID,
			   0,
			   NULL,
			   APERTURE_SYSMEM,
			   &mapped_buf);
	nvgpu_posix_enable_fault_injection(kmem_fi, false, 0);
	if (ret != -ENOMEM) {
		unit_err(m, "nvgpu_vm_map did not fail as expected (3)\n");
		ret = UNIT_FAIL;
		goto free_sgt_os_buf;
	}

	/* Invalid mapping size */
	ret = nvgpu_vm_map(vm,
			   &os_buf,
			   sgt,
			   0,
			   SZ_1G,
			   0,
			   gk20a_mem_flag_none,
			   NVGPU_VM_MAP_ACCESS_READ_WRITE,
			   NVGPU_VM_MAP_CACHEABLE,
			   NV_KIND_INVALID,
			   0,
			   NULL,
			   APERTURE_SYSMEM,
			   &mapped_buf);
	if (ret != -EINVAL) {
		unit_err(m, "nvgpu_vm_map did not fail as expected (4)\n");
		ret = UNIT_FAIL;
		goto free_sgt_os_buf;
	}

#ifndef CONFIG_NVGPU_COMPRESSION
	/* Enable comptag compression (not supported) */
	ret = nvgpu_vm_map(vm,
			   &os_buf,
			   sgt,
			   0,
			   buf_size,
			   0,
			   gk20a_mem_flag_none,
			   NVGPU_VM_MAP_ACCESS_READ_WRITE,
			   NVGPU_VM_MAP_CACHEABLE,
			   NVGPU_KIND_INVALID,
			   NVGPU_KIND_INVALID,
			   NULL,
			   APERTURE_SYSMEM,
			   &mapped_buf);
	if (ret != -ENOMEM) {
		unit_err(m, "nvgpu_vm_map did not fail as expected (5)\n");
		ret = UNIT_FAIL;
		goto free_sgt_os_buf;
	}
#endif

	/* Make g->ops.mm.gmmu.map fail */
	nvgpu_posix_enable_fault_injection(kmem_fi, true, 20);
	ret = nvgpu_vm_map(vm,
			   &os_buf,
			   sgt,
			   0,
			   buf_size,
			   0,
			   gk20a_mem_flag_none,
			   NVGPU_VM_MAP_ACCESS_READ_WRITE,
			   NVGPU_VM_MAP_CACHEABLE,
			   NV_KIND_INVALID,
			   0,
			   NULL,
			   APERTURE_SYSMEM,
			   &mapped_buf);
	nvgpu_posix_enable_fault_injection(kmem_fi, false, 0);
	if (ret != -ENOMEM) {
		unit_err(m, "nvgpu_vm_map did not fail as expected (6)\n");
		ret = UNIT_FAIL;
		goto free_sgt_os_buf;
	}

	ret = UNIT_SUCCESS;

free_sgt_os_buf:
	if (sgt != NULL) {
		nvgpu_sgt_free(g, sgt);
	}
	if (os_buf.buf != NULL) {
		nvgpu_kfree(g, os_buf.buf);
	}

exit:
	if (ret == UNIT_FAIL) {
		unit_err(m, "Buffer mapping failed\n");
	}

	if (vm != NULL) {
		nvgpu_vm_put(vm);
	}

	return ret;
}

int test_map_buffer_security(struct unit_module *m, struct gk20a *g,
	void *__args)
{
	struct vm_gk20a *vm;
	int ret = UNIT_FAIL;
	struct nvgpu_os_buffer os_buf = {0};
	struct nvgpu_mem_sgl sgl_list[1];
	struct nvgpu_mem mem = {0};
	struct nvgpu_sgt *sgt = NULL;
	/*
	 * - small pages are used
	 * - four pages of page directories, one per level (0, 1, 2, 3)
	 * - 4KB/8B = 512 entries per page table chunk
	 * - a PD cache size of 64K fits 16x 4k-sized PTE pages
	 */
	size_t buf_size = SZ_4K * ((16 - 4 + 1) * (SZ_4K / 8U) + 1);
	struct nvgpu_mapped_buf *mapped_buf = NULL;
	struct nvgpu_posix_fault_inj *kmem_fi =
		nvgpu_kmem_get_fault_injection();

	vm = create_test_vm(m, g);

	if (vm == NULL) {
		unit_err(m, "vm is NULL\n");
		ret = UNIT_FAIL;
		goto exit;
	}

	/* Allocate a CPU buffer */
	os_buf.buf = nvgpu_kzalloc(g, buf_size);
	if (os_buf.buf == NULL) {
		unit_err(m, "Failed to allocate a CPU buffer\n");
		ret = UNIT_FAIL;
		goto free_sgt_os_buf;
	}
	os_buf.size = buf_size;

	memset(&sgl_list[0], 0, sizeof(sgl_list[0]));
	sgl_list[0].phys = BUF_CPU_PA;
	sgl_list[0].dma = 0;
	sgl_list[0].length = buf_size;

	mem.size = buf_size;
	mem.cpu_va = os_buf.buf;

	/* Create sgt */
	sgt = custom_sgt_create(m, g, &mem, sgl_list, 1);
	if (sgt == NULL) {
		ret = UNIT_FAIL;
		goto free_sgt_os_buf;
	}

	/*
	 * Make pentry allocation fail. Note that the PD cache size is 64K
	 * during these unit tests.
	 */
	nvgpu_posix_enable_fault_injection(kmem_fi, true, 6);

	u64 gpuva = buf_size;
	u32 pte[2];

	/*
	 * If this PTE exists now, it should be invalid; make sure for the
	 * check after the map call so we know when something changed.
	 */
	ret = nvgpu_get_pte(g, vm, gpuva, pte);
	if (ret == 0 && pte_is_valid(pte)) {
		/* This is just a big warning though; don't exit yet */
		unit_err(m, "PTE already valid before mapping anything\n");
	}

	ret = nvgpu_vm_map(vm,
			   &os_buf,
			   sgt,
			   gpuva,
			   buf_size,
			   0,
			   gk20a_mem_flag_none,
			   NVGPU_VM_MAP_ACCESS_READ_WRITE,
			   NVGPU_VM_MAP_CACHEABLE,
			   NV_KIND_INVALID,
			   0,
			   NULL,
			   APERTURE_SYSMEM,
			   &mapped_buf);

	nvgpu_posix_enable_fault_injection(kmem_fi, false, 0);

	if (ret != -ENOMEM) {
		unit_err(m, "nvgpu_vm_map did not fail as expected\n");
		ret = UNIT_FAIL;
		goto free_sgt_os_buf;
	}

	ret = nvgpu_get_pte(g, vm, gpuva, pte);
	if (ret != 0) {
		unit_err(m, "PTE lookup after map failed\n");
		ret = UNIT_FAIL;
		goto free_sgt_os_buf;
	}

	/*
	 * And now the reason this test exists: make sure the attempted address
	 * does not contain anything. Note that a simple pte_is_valid() is not
	 * sufficient here - a sparse mapping is invalid and volatile, and we
	 * don't want sparse mappings here.
	 *
	 * Only the PTE pointing at the start address is checked; we assume
	 * that if that's zero, the rest of the mapping is too, because the
	 * update code visits the entries in that order. (But if this one is
	 * errornously valid, others might be too.)
	 */
	if (pte[0] != 0U || pte[1] != 0U) {
		unit_err(m, "Mapping failed but pte is not zero (0x%x 0x%x)\n",
			 pte[0], pte[1]);
		unit_err(m, "Pte addr %llx, buf %llx\n",
			 pte_get_phys_addr(m, pte), sgl_list[0].phys);

		ret = UNIT_FAIL;
		goto free_sgt_os_buf;
	}

	ret = UNIT_SUCCESS;

free_sgt_os_buf:
	if (sgt != NULL) {
		nvgpu_sgt_free(g, sgt);
	}
	if (os_buf.buf != NULL) {
		nvgpu_kfree(g, os_buf.buf);
	}

exit:
	if (ret == UNIT_FAIL) {
		unit_err(m, "Buffer mapping failed\n");
	}

	if (vm != NULL) {
		nvgpu_vm_put(vm);
	}

	return ret;
}

int test_map_buffer_security_error_cases(struct unit_module *m, struct gk20a *g,
	void *__args)
{
	struct vm_gk20a *vm;
	int ret = UNIT_FAIL;
	struct nvgpu_os_buffer os_buf = {0};
	struct nvgpu_mem_sgl sgl_list[1];
	struct nvgpu_mem mem = {0};
	struct nvgpu_sgt *sgt = NULL;
	/*
	 * - small pages are used
	 * - four pages of page directories, one per level (0, 1, 2, 3)
	 * - 4KB/8B = 512 entries per page table chunk
	 * - a PD cache size of 64K fits 16x 4k-sized PTE pages
	 */
	size_t buf_size = SZ_4K * ((16 - 4 + 1) * (SZ_4K / 8U) + 1);
	struct nvgpu_mapped_buf *mapped_buf = NULL;
	struct nvgpu_posix_fault_inj *kmem_fi =
		nvgpu_kmem_get_fault_injection();

	vm = create_test_vm(m, g);

	if (vm == NULL) {
		unit_err(m, "vm is NULL\n");
		ret = UNIT_FAIL;
		goto exit;
	}

	int (*old_fb_tlb_invalidate)(struct gk20a *, struct nvgpu_mem *) = g->ops.fb.tlb_invalidate;

	/* Allocate a CPU buffer */
	os_buf.buf = nvgpu_kzalloc(g, buf_size);
	if (os_buf.buf == NULL) {
		unit_err(m, "Failed to allocate a CPU buffer\n");
		ret = UNIT_FAIL;
		goto free_sgt_os_buf;
	}
	os_buf.size = buf_size;

	memset(&sgl_list[0], 0, sizeof(sgl_list[0]));
	sgl_list[0].phys = BUF_CPU_PA;
	sgl_list[0].dma = 0;
	sgl_list[0].length = buf_size;

	mem.size = buf_size;
	mem.cpu_va = os_buf.buf;

	/* Create sgt */
	sgt = custom_sgt_create(m, g, &mem, sgl_list, 1);
	if (sgt == NULL) {
		ret = UNIT_FAIL;
		goto free_sgt_os_buf;
	}

	u64 gpuva = buf_size;
	u32 pte[2];

	/* control nvgpu_gmmu_cache_maint_unmap and nvgpu_gmmu_cache_maint_map failures */
	g->ops.fb.tlb_invalidate = test_fail_fb_tlb_invalidate;

	/* Make nvgpu_gmmu_update_page_table fail; see test_map_buffer_security */
	nvgpu_posix_enable_fault_injection(kmem_fi, true, 6);

	/* Make the unmap cache maint fail too */
	fb_tlb_invalidate_fail_mask = 1U;
	fb_tlb_invalidate_calls = 0U;
	ret = nvgpu_vm_map(vm,
			   &os_buf,
			   sgt,
			   gpuva,
			   buf_size,
			   0,
			   gk20a_mem_flag_none,
			   NVGPU_VM_MAP_ACCESS_READ_WRITE,
			   NVGPU_VM_MAP_CACHEABLE,
			   NV_KIND_INVALID,
			   0,
			   NULL,
			   APERTURE_SYSMEM,
			   &mapped_buf);

	nvgpu_posix_enable_fault_injection(kmem_fi, false, 0);

	if (ret != -ENOMEM) {
		unit_err(m, "nvgpu_vm_map did not fail as expected (7)\n");
		ret = UNIT_FAIL;
		goto free_sgt_os_buf;
	}

	if (fb_tlb_invalidate_calls != 1U) {
		unit_err(m, "tlb invalidate called %u, not as expected 1\n",
				fb_tlb_invalidate_calls);
		ret = UNIT_FAIL;
		goto free_sgt_os_buf;
	}
	fb_tlb_invalidate_calls = 0U;

	/* Successful map but failed cache maintenance once */
	fb_tlb_invalidate_fail_mask = 1U;
	fb_tlb_invalidate_calls = 0U;
	ret = nvgpu_vm_map(vm,
			   &os_buf,
			   sgt,
			   0,
			   buf_size,
			   0,
			   gk20a_mem_flag_none,
			   NVGPU_VM_MAP_ACCESS_READ_WRITE,
			   NVGPU_VM_MAP_CACHEABLE,
			   NV_KIND_INVALID,
			   0,
			   NULL,
			   APERTURE_SYSMEM,
			   &mapped_buf);

	if (ret != -ENOMEM) {
		unit_err(m, "nvgpu_vm_map did not fail as expected (8)\n");
		ret = UNIT_FAIL;
		goto free_sgt_os_buf;
	}

	if (fb_tlb_invalidate_calls != 2U) {
		unit_err(m, "tlb invalidate called %u, not as expected 2\n",
				fb_tlb_invalidate_calls);
		ret = UNIT_FAIL;
		goto free_sgt_os_buf;
	}

	ret = nvgpu_get_pte(g, vm, gpuva, pte);
	if (ret != 0) {
		unit_err(m, "PTE lookup after map failed\n");
		ret = UNIT_FAIL;
		goto free_sgt_os_buf;
	}

	if (pte[0] != 0U || pte[1] != 0U) {
		unit_err(m, "Mapping failed but pte is not zero (0x%x 0x%x)\n",
			 pte[0], pte[1]);
		unit_err(m, "Pte addr %llx, buf %llx\n",
			 pte_get_phys_addr(m, pte), sgl_list[0].phys);

		ret = UNIT_FAIL;
		goto free_sgt_os_buf;
	}

	/* Successful map but failed cache maintenance twice */
	fb_tlb_invalidate_fail_mask = 3U;
	fb_tlb_invalidate_calls = 0U;
	ret = nvgpu_vm_map(vm,
			   &os_buf,
			   sgt,
			   0,
			   buf_size,
			   0,
			   gk20a_mem_flag_none,
			   NVGPU_VM_MAP_ACCESS_READ_WRITE,
			   NVGPU_VM_MAP_CACHEABLE,
			   NV_KIND_INVALID,
			   0,
			   NULL,
			   APERTURE_SYSMEM,
			   &mapped_buf);

	if (ret != -ENOMEM) {
		unit_err(m, "nvgpu_vm_map did not fail as expected (9)\n");
		ret = UNIT_FAIL;
		goto free_sgt_os_buf;
	}

	if (fb_tlb_invalidate_calls != 2U) {
		unit_err(m, "tlb invalidate called %u, not as expected 2\n",
				fb_tlb_invalidate_calls);
		ret = UNIT_FAIL;
		goto free_sgt_os_buf;
	}

	ret = nvgpu_get_pte(g, vm, gpuva, pte);
	if (ret != 0) {
		unit_err(m, "PTE lookup after map failed (2)\n");
		ret = UNIT_FAIL;
		goto free_sgt_os_buf;
	}

	if (pte[0] != 0U || pte[1] != 0U) {
		unit_err(m, "Mapping (2) failed but pte is not zero (0x%x 0x%x)\n",
			 pte[0], pte[1]);
		unit_err(m, "Pte addr %llx, buf %llx\n",
			 pte_get_phys_addr(m, pte), sgl_list[0].phys);

		ret = UNIT_FAIL;
		goto free_sgt_os_buf;
	}

	ret = UNIT_SUCCESS;

free_sgt_os_buf:
	g->ops.fb.tlb_invalidate = old_fb_tlb_invalidate;
	if (sgt != NULL) {
		nvgpu_sgt_free(g, sgt);
	}
	if (os_buf.buf != NULL) {
		nvgpu_kfree(g, os_buf.buf);
	}

exit:
	if (ret == UNIT_FAIL) {
		unit_err(m, "Buffer mapping failed\n");
	}

	if (vm != NULL) {
		nvgpu_vm_put(vm);
	}

	return ret;
}

/*
 * Try mapping a buffer into the GPU virtual address space:
 *    - Allocate a new CPU buffer
 *    - If a specific GPU VA was requested, allocate a VM area for a fixed GPU
 *      VA mapping
 *    - Map buffer into the GPU virtual address space
 *    - Verify that the buffer was mapped correctly
 *    - Unmap buffer
 */
static int map_buffer(struct unit_module *m,
		      struct gk20a *g,
		      struct vm_gk20a *vm,
		      struct vm_gk20a_mapping_batch *batch,
		      u64 cpu_pa,
		      u64 gpu_va,
		      size_t buf_size,
		      size_t page_size,
		      size_t alignment,
		      int subcase)
{
	int ret = UNIT_SUCCESS;
	u32 flags = NVGPU_VM_MAP_CACHEABLE;
	struct nvgpu_mapped_buf *mapped_buf = NULL;
	struct nvgpu_mapped_buf *mapped_buf_check = NULL;
	struct nvgpu_os_buffer os_buf = {0};
	struct nvgpu_mem_sgl sgl_list[1];
	struct nvgpu_mem mem = {0};
	struct nvgpu_sgt *sgt = NULL;
	bool fixed_gpu_va = (gpu_va != 0);
	s16 compr_kind;
	u32 pte[2];
	struct nvgpu_mapped_buf **mapped_buffers = NULL;
	u32 num_mapped_buffers = 0;
	struct nvgpu_posix_fault_inj *kmem_fi =
		nvgpu_kmem_get_fault_injection();

	if (vm == NULL) {
		unit_err(m, "vm is NULL\n");
		ret = UNIT_FAIL;
		goto exit;
	}

	/* Allocate a CPU buffer */
	os_buf.buf = nvgpu_kzalloc(g, buf_size);
	if (os_buf.buf == NULL) {
		unit_err(m, "Failed to allocate a CPU buffer\n");
		ret = UNIT_FAIL;
		goto free_sgt_os_buf;
	}
	os_buf.size = buf_size;

	memset(&sgl_list[0], 0, sizeof(sgl_list[0]));
	sgl_list[0].phys = cpu_pa;
	sgl_list[0].dma = 0;
	sgl_list[0].length = buf_size;

	mem.size = buf_size;
	mem.cpu_va = os_buf.buf;

	/* Create sgt */
	sgt = custom_sgt_create(m, g, &mem, sgl_list, 1);
	if (sgt == NULL) {
		ret = UNIT_FAIL;
		goto free_sgt_os_buf;
	}

	if (fixed_gpu_va) {
		flags |= NVGPU_VM_MAP_FIXED_OFFSET;

		if (!(subcase & SPECIAL_CASE_NO_VM_AREA)) {
			size_t num_pages = DIV_ROUND_UP(buf_size, page_size);
			u64 gpu_va_copy = gpu_va;

			unit_info(m, "Allocating VM Area for fixed GPU VA mapping\n");
			ret = nvgpu_vm_area_alloc(vm,
					num_pages,
					page_size,
					&gpu_va_copy,
					NVGPU_VM_AREA_ALLOC_FIXED_OFFSET);
			if (ret != 0) {
				unit_err(m, "Failed to allocate a VM area\n");
				ret = UNIT_FAIL;
				goto free_sgt_os_buf;
			}
			if (gpu_va_copy != gpu_va) {
				unit_err(m, "VM area created at the wrong GPU VA\n");
				ret = UNIT_FAIL;
				goto free_vm_area;
			}
			if (nvgpu_vm_area_find(vm, gpu_va) == NULL) {
				unit_err(m, "VM area not found\n");
				ret = UNIT_FAIL;
				goto free_vm_area;
			}
			/* For branch coverage */
			if (nvgpu_vm_area_find(vm, 0) != NULL) {
				unit_err(m, "nvgpu_vm_area_find did not fail as expected\n");
				ret = UNIT_FAIL;
				goto free_vm_area;
			}
		}
	}

#ifdef CONFIG_NVGPU_COMPRESSION
	compr_kind = 0;
#else
	compr_kind = NV_KIND_INVALID;
#endif

	ret = nvgpu_vm_map(vm,
			   &os_buf,
			   sgt,
			   gpu_va,
			   buf_size,
			   0,
			   gk20a_mem_flag_none,
			   NVGPU_VM_MAP_ACCESS_READ_WRITE,
			   flags,
			   compr_kind,
			   0,
			   batch,
			   APERTURE_SYSMEM,
			   &mapped_buf);
	if (ret != 0) {
		unit_err(m, "Failed to map buffer into the GPU virtual address"
			    " space\n");
		ret = UNIT_FAIL;
		goto free_vm_area;
	}

	/*
	 * Make nvgpu_vm_find_mapping return non-NULL to prevent the actual
	 * mapping, thus simulating the fact that the buffer is already mapped.
	 */
	if (subcase & SPECIAL_CASE_DOUBLE_MAP) {
		ret = nvgpu_vm_map(vm,
				&os_buf,
				sgt,
				gpu_va,
				buf_size,
				0,
				gk20a_mem_flag_none,
				NVGPU_VM_MAP_ACCESS_READ_WRITE,
				flags,
				compr_kind,
				0,
				batch,
				APERTURE_SYSMEM,
				&mapped_buf);
		if (ret != 0) {
			unit_err(m, "Failed to map buffer into the GPU virtual"
				" address space (second mapping)\n");
			ret = UNIT_FAIL;
			goto free_mapped_buf;
		}
	}

	/* Check if we can find the mapped buffer */
	mapped_buf_check = nvgpu_vm_find_mapped_buf(vm, mapped_buf->addr);
	if (mapped_buf_check == NULL) {
		unit_err(m, "Can't find mapped buffer\n");
		ret = UNIT_FAIL;
		goto free_mapped_buf;
	}
	if (mapped_buf_check->addr != mapped_buf->addr) {
		unit_err(m, "Invalid buffer GPU VA\n");
		ret = UNIT_FAIL;
		goto free_mapped_buf;
	}

	/* Check if we can find the mapped buffer via a range search */
	mapped_buf_check = nvgpu_vm_find_mapped_buf_range(vm,
		mapped_buf->addr + buf_size/2);
	if (mapped_buf_check == NULL) {
		unit_err(m, "Can't find mapped buffer via range search\n");
		ret = UNIT_FAIL;
		goto free_mapped_buf;
	}

	/* Check if we can find the mapped buffer via "less than" search */
	mapped_buf_check = nvgpu_vm_find_mapped_buf_less_than(vm,
		mapped_buf->addr + buf_size/2);
	if (mapped_buf_check == NULL) {
		unit_err(m, "Can't find mapped buffer via less-than search\n");
		ret = UNIT_FAIL;
		goto free_mapped_buf;
	}

	/* Check if we can find the mapped buffer via nvgpu_vm_find_mapping */
	if (fixed_gpu_va) {
		mapped_buf_check = nvgpu_vm_find_mapping(vm, &os_buf, gpu_va,
			flags, compr_kind);
		if (mapped_buf_check == NULL) {
			unit_err(m, "Can't find buf nvgpu_vm_find_mapping\n");
			ret = UNIT_FAIL;
			goto free_mapped_buf;
		}
	}

	/*
	 * For code coverage, ensure that an invalid address does not return
	 * a buffer.
	 */
	mapped_buf_check = nvgpu_vm_find_mapped_buf_range(vm, 0);
	if (mapped_buf_check != NULL) {
		unit_err(m, "Found inexistant mapped buffer\n");
		ret = UNIT_FAIL;
		goto free_mapped_buf;
	}

	/*
	 * Based on the virtual address returned, lookup the corresponding PTE
	 */
	ret = nvgpu_get_pte(g, vm, mapped_buf->addr, pte);
	if (ret != 0) {
		unit_err(m, "PTE lookup failed\n");
		ret = UNIT_FAIL;
		goto free_mapped_buf;
	}

	/* Check if PTE is valid */
	if (!pte_is_valid(pte)) {
		unit_err(m, "Invalid PTE!\n");
		ret = UNIT_FAIL;
		goto free_mapped_buf;
	}

	/* Check if PTE corresponds to the physical address we requested */
	if (pte_get_phys_addr(m, pte) != cpu_pa) {
		unit_err(m, "Unexpected physical address in PTE\n");
		ret = UNIT_FAIL;
		goto free_mapped_buf;
	}

	/* Check if the buffer's GPU VA is aligned correctly */
	if (!IS_ALIGNED(mapped_buf->addr, alignment)) {
		unit_err(m, "Incorrect buffer GPU VA alignment\n");
		ret = UNIT_FAIL;
		goto free_mapped_buf;
	}

	/*
	 * If a specific GPU VA was requested, check that the buffer's GPU VA
	 * matches the requested GPU VA
	 */
	if (fixed_gpu_va && (mapped_buf->addr != gpu_va)) {
		unit_err(m, "Mapped buffer's GPU VA does not match requested"
			    " GPU VA\n");
		ret = UNIT_FAIL;
		goto free_mapped_buf;
	}


	/*
	 * Test the nvgpu_vm_get_buffers logic and ensure code coverage.
	 * First use error injection to make it fail.
	 */
	nvgpu_posix_enable_fault_injection(kmem_fi, true, 0);
	ret = nvgpu_vm_get_buffers(vm, &mapped_buffers, &num_mapped_buffers);
	nvgpu_posix_enable_fault_injection(kmem_fi, false, 0);
	if (ret != -ENOMEM) {
		unit_err(m, "nvgpu_vm_get_buffers did not fail as expected.\n");
		ret = UNIT_FAIL;
		goto free_mapped_buf;
	}

	/* Second, make it succeed and check the result. */
	nvgpu_vm_get_buffers(vm, &mapped_buffers, &num_mapped_buffers);
	nvgpu_vm_put_buffers(vm, mapped_buffers, 0);
	nvgpu_vm_put_buffers(vm, mapped_buffers, num_mapped_buffers);
	if (num_mapped_buffers == 0) {
		unit_err(m, "Invalid number of mapped buffers\n");
		ret = UNIT_FAIL;
		goto free_mapped_buf;
	}

	/*
	 * If VM is userspace managed, there should not be any accessible
	 * buffers.
	 */
	vm->userspace_managed = true;
	nvgpu_vm_get_buffers(vm, &mapped_buffers, &num_mapped_buffers);
	vm->userspace_managed = false;
	if (num_mapped_buffers != 0) {
		unit_err(m, "Found accessible buffers in userspace managed VM\n");
		ret = UNIT_FAIL;
		goto free_mapped_buf;
	}

	ret = UNIT_SUCCESS;

free_mapped_buf:
	if ((mapped_buf != NULL) && !(subcase & SPECIAL_CASE_NO_FREE)) {
		/*
		 * A second unmap will be attempted; the first one will free
		 * mapped_buf, so get the address before that happens.
		 */
		u64 buf_addr = mapped_buf->addr;

		nvgpu_vm_unmap(vm, buf_addr, batch);
		mapped_buf = NULL;
		/*
		 * Unmapping an already unmapped buffer should not cause any
		 * errors.
		 */
		nvgpu_vm_unmap(vm, buf_addr, batch);
	}
free_vm_area:
	if (fixed_gpu_va && !(subcase & SPECIAL_CASE_NO_FREE)) {
		ret = nvgpu_vm_area_free(vm, gpu_va);
		if (ret != 0) {
			unit_err(m, "Failed to free vm area\n");
			ret = UNIT_FAIL;
		}
	}
free_sgt_os_buf:
	if (sgt != NULL) {
		nvgpu_sgt_free(g, sgt);
	}
	if (os_buf.buf != NULL) {
		nvgpu_kfree(g, os_buf.buf);
	}

exit:
	if (ret == UNIT_FAIL) {
		unit_err(m, "Buffer mapping failed\n");
	}
	return ret;
}

int test_vm_bind(struct unit_module *m, struct gk20a *g, void *__args)
{
	int ret = UNIT_FAIL;
	struct vm_gk20a *vm = NULL;

	struct nvgpu_channel *channel = (struct nvgpu_channel *)
		malloc(sizeof(struct nvgpu_channel));
	channel->g = g;

	vm = create_test_vm(m, g);

	/* Error testing */
	g->ops.mm.vm_bind_channel(vm, NULL);
	if (channel->vm == vm) {
		ret = UNIT_FAIL;
		unit_err(m, "nvgpu_vm_bind_channel did not fail as expected.\n");
		goto exit;
	}

	/* Succesfull call */
	g->ops.mm.vm_bind_channel(vm, channel);

	if (channel->vm != vm) {
		ret = UNIT_FAIL;
		unit_err(m, "nvgpu_vm_bind_channel failed to bind the vm.\n");
		goto exit;
	}

	ret = UNIT_SUCCESS;
exit:
	g->fifo.channel = NULL;
	free(channel);
	nvgpu_vm_put(vm);
	return ret;
}

int test_vm_aspace_id(struct unit_module *m, struct gk20a *g, void *__args)
{
	int ret = UNIT_FAIL;
	struct vm_gk20a *vm = NULL;
	struct gk20a_as_share as_share;

	vm = create_test_vm(m, g);

	if (vm_aspace_id(vm) != -1) {
		ret = UNIT_FAIL;
		unit_err(m, "create_test_vm did not return an expected value (1).\n");
		goto exit;
	}

	as_share.id = 0;
	vm->as_share = &as_share;

	if (vm_aspace_id(vm) != 0) {
		ret = UNIT_FAIL;
		unit_err(m, "create_test_vm did not return an expected value (2).\n");
		goto exit;
	}

	ret = UNIT_SUCCESS;
exit:
	nvgpu_vm_put(vm);
	return ret;
}

int test_init_error_paths(struct unit_module *m, struct gk20a *g, void *__args)
{
	int ret = UNIT_SUCCESS;
	struct vm_gk20a *vm = NULL;
	u64 low_hole = 0;
	u64 kernel_reserved = 0;
	u64 aperture_size = 0;
	u64 user_vma = 0;
	bool big_pages = true;
	struct nvgpu_posix_fault_inj *kmem_fi =
		nvgpu_kmem_get_fault_injection();
	u64 default_aperture_size;

	/* Initialize test environment */
	ret = init_test_env(m, g);
	if (ret != UNIT_SUCCESS) {
		goto exit;
	}

	/* Set VM parameters */
	g->ops.mm.get_default_va_sizes(&default_aperture_size, NULL, NULL);
	big_pages = true;
	low_hole = SZ_1M * 64;
	aperture_size = 128 * SZ_1G;
	kernel_reserved = 4 * SZ_1G - low_hole;
	user_vma = aperture_size - low_hole - kernel_reserved;

	/* Error injection to make the allocation for struct vm_gk20a to fail */
	nvgpu_posix_enable_fault_injection(kmem_fi, true, 0);
	vm = nvgpu_vm_init(g,
			   g->ops.mm.gmmu.get_default_big_page_size(),
			   low_hole,
			   user_vma,
			   kernel_reserved,
			   nvgpu_gmmu_va_small_page_limit(),
			   big_pages,
			   false,
			   true,
			   __func__);
	nvgpu_posix_enable_fault_injection(kmem_fi, false, 0);
	if (vm != NULL) {
		unit_err(m, "Init VM did not fail as expected. (1)\n");
		ret = UNIT_FAIL;
		goto exit;
	}

	/*
	 * Cause the nvgpu_vm_do_init function to assert by setting an invalid
	 * aperture size
	 */
	if (!EXPECT_BUG(
		nvgpu_vm_init(g,
			g->ops.mm.gmmu.get_default_big_page_size(),
			low_hole,
			user_vma,
			default_aperture_size, /* invalid aperture size */
			nvgpu_gmmu_va_small_page_limit(),
			big_pages,
			false,
			true,
			__func__)
	)) {
		unit_err(m, "BUG() was not called but it was expected (2).\n");
		ret = UNIT_FAIL;
		goto exit;
	}

	/* Make nvgpu_vm_do_init fail with invalid parameters */
	vm = nvgpu_kzalloc(g, sizeof(*vm));

	/* vGPU with userspace managed */
	g->is_virtual = true;
	ret = nvgpu_vm_do_init(&g->mm, vm,
				g->ops.mm.gmmu.get_default_big_page_size(),
				low_hole, user_vma, kernel_reserved,
				nvgpu_gmmu_va_small_page_limit(),
				big_pages, true, true, __func__);
	g->is_virtual = false;
	if (ret != -ENOSYS) {
		unit_err(m, "nvgpu_vm_do_init did not fail as expected (4).\n");
		ret = UNIT_FAIL;
		goto exit;
	}

	/* Define a mock HAL that will report a failure */
	g->ops.mm.vm_as_alloc_share = hal_vm_as_alloc_share_error;
	ret = nvgpu_vm_do_init(&g->mm, vm,
				g->ops.mm.gmmu.get_default_big_page_size(),
				low_hole, user_vma, kernel_reserved,
				nvgpu_gmmu_va_small_page_limit(),
				big_pages, true, true, __func__);
	g->ops.mm.vm_as_alloc_share = hal_vm_as_alloc_share_success;
	if (ret != -1) {
		unit_err(m, "nvgpu_vm_do_init did not fail as expected (5).\n");
		ret = UNIT_FAIL;
		goto exit;
	}

	/* Invalid VM configuration - This scenario is not feasible */
	low_hole = SZ_1M * 64;

	/* Cause nvgpu_gmmu_init_page_table to fail */
	nvgpu_posix_enable_fault_injection(kmem_fi, true, 0);
	ret = nvgpu_vm_do_init(&g->mm, vm,
				g->ops.mm.gmmu.get_default_big_page_size(),
				low_hole, user_vma, kernel_reserved,
				nvgpu_gmmu_va_small_page_limit(),
				big_pages, false, true, __func__);
	nvgpu_posix_enable_fault_injection(kmem_fi, false, 0);
	if (ret != -ENOMEM) {
		unit_err(m, "nvgpu_vm_do_init did not fail as expected (7).\n");
		ret = UNIT_FAIL;
		goto exit;
	}

	/* Cause nvgpu_allocator_init(BUDDY) to fail for user VMA */
	nvgpu_posix_enable_fault_injection(kmem_fi, true, 5);
	ret = nvgpu_vm_do_init(&g->mm, vm,
				g->ops.mm.gmmu.get_default_big_page_size(),
				low_hole, user_vma, kernel_reserved,
				nvgpu_gmmu_va_small_page_limit(),
				big_pages, false, true, __func__);
	nvgpu_posix_enable_fault_injection(kmem_fi, false, 0);
	if (ret != -ENOMEM) {
		unit_err(m, "nvgpu_vm_do_init did not fail as expected (8).\n");
		ret = UNIT_FAIL;
		goto exit;
	}

	/* Cause nvgpu_allocator_init(BUDDY) to fail for user_lp VMA */
	nvgpu_posix_enable_fault_injection(kmem_fi, true, 12);
	ret = nvgpu_vm_do_init(&g->mm, vm,
				g->ops.mm.gmmu.get_default_big_page_size(),
				low_hole, user_vma, kernel_reserved,
				nvgpu_gmmu_va_small_page_limit(),
				big_pages, false, false, __func__);
	nvgpu_posix_enable_fault_injection(kmem_fi, false, 0);
	if (ret != -ENOMEM) {
		unit_err(m, "nvgpu_vm_do_init didn't fail as expected (9).\n");
		ret = UNIT_FAIL;
		goto exit;
	}

	/* Cause nvgpu_allocator_init(BUDDY) to fail for kernel VMA */
	nvgpu_posix_enable_fault_injection(kmem_fi, true, 17);
	ret = nvgpu_vm_do_init(&g->mm, vm,
				g->ops.mm.gmmu.get_default_big_page_size(),
				low_hole, user_vma, kernel_reserved,
				nvgpu_gmmu_va_small_page_limit(),
				big_pages, false, false, __func__);
	nvgpu_posix_enable_fault_injection(kmem_fi, false, 0);
	if (ret != -ENOMEM) {
		unit_err(m, "nvgpu_vm_do_init didn't fail as expected (10).\n");
		ret = UNIT_FAIL;
		goto exit;
	}

	/* Cause nvgpu_vm_init_vma_allocators to fail for long vm name */
	ret = nvgpu_vm_do_init(&g->mm, vm,
				g->ops.mm.gmmu.get_default_big_page_size(),
				low_hole, user_vma, kernel_reserved,
				nvgpu_gmmu_va_small_page_limit(),
				big_pages, false, false,
				"very_long_vm_name_to_fail_vm_init");
	if (ret != -EINVAL) {
		unit_err(m, "nvgpu_vm_do_init didn't fail as expected (12).\n");
		ret = UNIT_FAIL;
		goto exit;
	}

	/* Success with big pages and not unified VA */
	ret = nvgpu_vm_do_init(&g->mm, vm,
				g->ops.mm.gmmu.get_default_big_page_size(),
				low_hole, user_vma, kernel_reserved,
				nvgpu_gmmu_va_small_page_limit(),
				big_pages, false, false, __func__);
	if (ret != 0) {
		unit_err(m, "nvgpu_vm_do_init did not succeed as expected (B).\n");
		ret = UNIT_FAIL;
		goto exit;
	}

	/* Success with big pages disabled */
	ret = nvgpu_vm_do_init(&g->mm, vm,
				g->ops.mm.gmmu.get_default_big_page_size(),
				low_hole, user_vma, kernel_reserved,
				nvgpu_gmmu_va_small_page_limit(),
				false, false, false, __func__);
	if (ret != 0) {
		unit_err(m, "nvgpu_vm_do_init did not succeed as expected (C).\n");
		ret = UNIT_FAIL;
		goto exit;
	}

	/* No user VMA, so use kernel allocators */
	ret = nvgpu_vm_do_init(&g->mm, vm,
				g->ops.mm.gmmu.get_default_big_page_size(),
				nvgpu_gmmu_va_small_page_limit(),
				0ULL, kernel_reserved,
				nvgpu_gmmu_va_small_page_limit(), big_pages,
				false, false, __func__);
	if (ret != 0) {
		unit_err(m, "nvgpu_vm_do_init did not succeed as expected (D).\n");
		ret = UNIT_FAIL;
		goto exit;
	}

	/* Ref count */
	if (vm->ref.refcount.v != 1U) {
		unit_err(m, "Invalid ref count. (1)\n");
		ret = UNIT_FAIL;
		goto exit;
	}
	nvgpu_vm_get(vm);
	if (vm->ref.refcount.v != 2U) {
		unit_err(m, "Invalid ref count. (2)\n");
		ret = UNIT_FAIL;
		goto exit;
	}
	nvgpu_vm_put(vm);
	if (vm->ref.refcount.v != 1U) {
		unit_err(m, "Invalid ref count. (3)\n");
		ret = UNIT_FAIL;
		goto exit;
	}

	ret = UNIT_SUCCESS;

exit:
	if (vm != NULL) {
		nvgpu_vm_put(vm);
	}

	return ret;
}

int test_map_buf(struct unit_module *m, struct gk20a *g, void *__args)
{
	int ret = UNIT_SUCCESS;
	struct vm_gk20a *vm = NULL;
	u64 low_hole = 0;
	u64 user_vma = 0;
	u64 kernel_reserved = 0;
	u64 aperture_size = 0;
	bool big_pages = true;
	size_t buf_size = 0;
	size_t page_size = 0;
	size_t alignment = 0;
	struct nvgpu_mapped_buf **mapped_buffers = NULL;
	u32 num_mapped_buffers = 0;
	struct nvgpu_os_posix *p = nvgpu_os_posix_from_gk20a(g);

	if (m == NULL) {
		ret = UNIT_FAIL;
		goto exit;
	}
	if (g == NULL) {
		unit_err(m, "gk20a is NULL\n");
		ret = UNIT_FAIL;
		goto exit;
	}

	/* Initialize test environment */
	ret = init_test_env(m, g);
	if (ret != UNIT_SUCCESS) {
		goto exit;
	}

	/* Initialize VM */
	big_pages = true;
	low_hole = SZ_1M * 64;
	aperture_size = 128 * SZ_1G;
	kernel_reserved = 4 * SZ_1G - low_hole;
	user_vma = aperture_size - low_hole - kernel_reserved;
	unit_info(m, "Initializing VM:\n");
	unit_info(m, "   - Low Hole Size = 0x%llx\n", low_hole);
	unit_info(m, "   - User Aperture Size = 0x%llx\n", user_vma);
	unit_info(m, "   - Kernel Reserved Size = 0x%llx\n", kernel_reserved);
	unit_info(m, "   - Total Aperture Size = 0x%llx\n", aperture_size);
	vm = nvgpu_vm_init(g,
			   g->ops.mm.gmmu.get_default_big_page_size(),
			   low_hole,
			   user_vma,
			   kernel_reserved,
			   nvgpu_gmmu_va_small_page_limit(),
			   big_pages,
			   false,
			   true,
			   __func__);
	if (vm == NULL) {
		unit_err(m, "Failed to init VM\n");
		ret = UNIT_FAIL;
		goto exit;
	}

	/*
	 * There shouldn't be any mapped buffers at this point.
	 */
	nvgpu_vm_get_buffers(vm, &mapped_buffers, &num_mapped_buffers);
	if (num_mapped_buffers != 0) {
		unit_err(m, "Found mapped buffers in a new VM\n");
		ret = UNIT_FAIL;
		goto exit;
	}

	/* Big pages should be possible */
	if (!nvgpu_big_pages_possible(vm, low_hole,
		nvgpu_gmmu_va_small_page_limit())) {
		unit_err(m, "Big pages unexpectedly not possible\n");
		ret = UNIT_FAIL;
		goto exit;
	}

	/*
	 * Error handling: use invalid values to cover
	 * nvgpu_big_pages_possible()
	 */
	if (nvgpu_big_pages_possible(vm, 0, 1)) {
		unit_err(m, "Big pages unexpectedly possible (1)\n");
		ret = UNIT_FAIL;
		goto exit;
	}
	if (nvgpu_big_pages_possible(vm, 1, 0)) {
		unit_err(m, "Big pages unexpectedly possible (2)\n");
		ret = UNIT_FAIL;
		goto exit;
	}

	/* Map 4KB buffer */
	buf_size = SZ_4K;
	page_size = SZ_4K;
	alignment = SZ_4K;
	unit_info(m, "Mapping Buffer:\n");
	unit_info(m, "   - CPU PA = 0x%llx\n", BUF_CPU_PA);
	unit_info(m, "   - Buffer Size = 0x%lx\n", buf_size);
	unit_info(m, "   - Page Size = 0x%lx\n", page_size);
	unit_info(m, "   - Alignment = 0x%lx\n", alignment);
	ret = map_buffer(m,
			 g,
			 vm,
			 NULL,
			 BUF_CPU_PA,
			 0,
			 buf_size,
			 page_size,
			 alignment,
			 NO_SPECIAL_CASE);
	if (ret != UNIT_SUCCESS) {
		unit_err(m, "4KB buffer mapping failed\n");
		goto exit;
	}

	/* Map 64KB buffer */
	buf_size = SZ_64K;
	page_size = SZ_64K;
	alignment = SZ_64K;
	unit_info(m, "Mapping Buffer:\n");
	unit_info(m, "   - CPU PA = 0x%llx\n", BUF_CPU_PA);
	unit_info(m, "   - Buffer Size = 0x%lx\n", buf_size);
	unit_info(m, "   - Page Size = 0x%lx\n", page_size);
	unit_info(m, "   - Alignment = 0x%lx\n", alignment);
	ret = map_buffer(m,
			 g,
			 vm,
			 NULL,
			 BUF_CPU_PA,
			 0,
			 buf_size,
			 page_size,
			 alignment,
			 NO_SPECIAL_CASE);
	if (ret != UNIT_SUCCESS) {
		unit_err(m, "64KB buffer mapping failed\n");
		goto exit;
	}

	/* Corner case: big pages explicitly disabled at gk20a level */
	g->mm.disable_bigpage = true;
	ret = map_buffer(m,
			 g,
			 vm,
			 NULL,
			 BUF_CPU_PA,
			 0,
			 buf_size,
			 page_size,
			 alignment,
			 NO_SPECIAL_CASE);
	g->mm.disable_bigpage = false;
	if (ret != UNIT_SUCCESS) {
		unit_err(m, "Mapping failed (big pages disabled gk20a)\n");
		goto exit;
	}

	/* Corner case: big pages explicitly disabled at vm level */
	vm->big_pages = false;
	ret = map_buffer(m,
			 g,
			 vm,
			 NULL,
			 BUF_CPU_PA,
			 0,
			 buf_size,
			 page_size,
			 alignment,
			 NO_SPECIAL_CASE);
	vm->big_pages = true;
	if (ret != UNIT_SUCCESS) {
		unit_err(m, "Mapping failed (big pages disabled VM)\n");
		ret = UNIT_FAIL;
		goto exit;
	}

	/* Corner case: VA is not unified */
	vm->unified_va = false;
	ret = map_buffer(m,
			 g,
			 vm,
			 NULL,
			 BUF_CPU_PA,
			 0,
			 buf_size,
			 page_size,
			 alignment,
			 NO_SPECIAL_CASE);
	vm->unified_va = true;
	if (ret != UNIT_SUCCESS) {
		unit_err(m, "Mapping failed (non-unified VA)\n");
		ret = UNIT_FAIL;
		goto exit;
	}

	/* Corner case: disable IOMMU */
	p->mm_is_iommuable = false;
	ret = map_buffer(m,
			 g,
			 vm,
			 NULL,
			 BUF_CPU_PA,
			 0,
			 buf_size,
			 page_size,
			 alignment,
			 NO_SPECIAL_CASE);
	p->mm_is_iommuable = false;
	if (ret != UNIT_SUCCESS) {
		unit_err(m, "Non IOMMUable mapping failed\n");
		goto exit;
	}

	/* Corner case: smaller than vm->gmmu_page_sizes[GMMU_PAGE_SIZE_BIG] */
	buf_size = SZ_4K;
	page_size = SZ_4K;
	alignment = SZ_4K;
	vm->unified_va = false;
	ret = map_buffer(m,
			 g,
			 vm,
			 NULL,
			 BUF_CPU_PA,
			 0,
			 buf_size,
			 page_size,
			 alignment,
			 NO_SPECIAL_CASE);
	vm->unified_va = true;
	if (ret != UNIT_SUCCESS) {
		unit_err(m, "4KB buffer mapping failed\n");
		goto exit;
	}

	ret = UNIT_SUCCESS;

exit:
	if (vm != NULL) {
		nvgpu_vm_put(vm);
	}

	return ret;
}

int test_map_buf_gpu_va(struct unit_module *m,
			       struct gk20a *g,
			       void *__args)
{
	int ret = UNIT_SUCCESS;
	struct vm_gk20a *vm = NULL;
	u64 low_hole = 0;
	u64 user_vma = 0;
	u64 user_vma_limit = 0;
	u64 kernel_reserved = 0;
	u64 aperture_size = 0;
	u64 gpu_va = 0;
	bool big_pages = true;
	size_t buf_size = 0;
	size_t page_size = 0;
	size_t alignment = 0;

	if (m == NULL) {
		ret = UNIT_FAIL;
		goto exit;
	}
	if (g == NULL) {
		unit_err(m, "gk20a is NULL\n");
		ret = UNIT_FAIL;
		goto exit;
	}

	/* Initialize test environment */
	ret = init_test_env(m, g);
	if (ret != UNIT_SUCCESS) {
		goto exit;
	}

	/* Initialize VM */
	big_pages = true;
	low_hole = SZ_1M * 64;
	aperture_size = 128 * SZ_1G;
	kernel_reserved = 4 * SZ_1G - low_hole;
	user_vma = aperture_size - low_hole - kernel_reserved;
	unit_info(m, "Initializing VM:\n");
	unit_info(m, "   - Low Hole Size = 0x%llx\n", low_hole);
	unit_info(m, "   - User Aperture Size = 0x%llx\n", user_vma);
	unit_info(m, "   - Kernel Reserved Size = 0x%llx\n", kernel_reserved);
	unit_info(m, "   - Total Aperture Size = 0x%llx\n", aperture_size);
	vm = nvgpu_vm_init(g,
			   g->ops.mm.gmmu.get_default_big_page_size(),
			   low_hole,
			   user_vma,
			   kernel_reserved,
			   nvgpu_gmmu_va_small_page_limit(),
			   big_pages,
			   false,
			   true,
			   __func__);
	if (vm == NULL) {
		unit_err(m, "Failed to init VM\n");
		ret = UNIT_FAIL;
		goto exit;
	}

	/* Map 4KB buffer */
	buf_size = SZ_4K;
	page_size = SZ_4K;
	alignment = SZ_4K;
	/*
	 * Calculate a valid base GPU VA for the buffer. We're multiplying
	 * buf_size by 10 just to be on the safe side.
	 */
	user_vma_limit = nvgpu_alloc_end(&vm->user);
	gpu_va = user_vma_limit - buf_size*10;
	unit_info(m, "   - user_vma_limit = 0x%llx\n", user_vma_limit);
	unit_info(m, "Mapping Buffer:\n");
	unit_info(m, "   - CPU PA = 0x%llx\n", BUF_CPU_PA);
	unit_info(m, "   - GPU VA = 0x%llx\n", gpu_va);
	unit_info(m, "   - Buffer Size = 0x%lx\n", buf_size);
	unit_info(m, "   - Page Size = 0x%lx\n", page_size);
	unit_info(m, "   - Alignment = 0x%lx\n", alignment);
	ret = map_buffer(m,
			 g,
			 vm,
			 NULL,
			 BUF_CPU_PA,
			 gpu_va,
			 buf_size,
			 page_size,
			 alignment,
			 NO_SPECIAL_CASE);
	if (ret != UNIT_SUCCESS) {
		unit_err(m, "4KB buffer mapping failed\n");
		goto exit;
	}

	/*
	 * Corner case: if already mapped, map_buffer should still report
	 * success.
	 */
	ret = map_buffer(m,
			 g,
			 vm,
			 NULL,
			 BUF_CPU_PA,
			 gpu_va,
			 buf_size,
			 page_size,
			 alignment,
			 SPECIAL_CASE_DOUBLE_MAP);
	if (ret != UNIT_SUCCESS) {
		unit_err(m, "Mapping failed (already mapped case)\n");
		goto exit;
	}

	/* Map 64KB buffer */
	buf_size = SZ_64K;
	page_size = SZ_64K;
	alignment = SZ_64K;
	/*
	 * Calculate a valid base GPU VA for the buffer. We're multiplying
	 * buf_size by 10 just to be on the safe side.
	 */
	gpu_va = user_vma_limit - buf_size*10;
	unit_info(m, "Mapping Buffer:\n");
	unit_info(m, "   - CPU PA = 0x%llx\n", BUF_CPU_PA);
	unit_info(m, "   - GPU VA = 0x%llx\n", gpu_va);
	unit_info(m, "   - Buffer Size = 0x%lx\n", buf_size);
	unit_info(m, "   - Page Size = 0x%lx\n", page_size);
	unit_info(m, "   - Alignment = 0x%lx\n", alignment);
	ret = map_buffer(m,
			 g,
			 vm,
			 NULL,
			 BUF_CPU_PA,
			 gpu_va,
			 buf_size,
			 page_size,
			 alignment,
			 NO_SPECIAL_CASE);
	if (ret != UNIT_SUCCESS) {
		unit_err(m, "64KB buffer mapping failed\n");
		goto exit;
	}

	/*
	 * Corner case: VA is not unified, GPU_VA fixed above
	 * nvgpu_gmmu_va_small_page_limit()
	 */
	vm->unified_va = false;
	ret = map_buffer(m,
			 g,
			 vm,
			 NULL,
			 BUF_CPU_PA,
			 gpu_va,
			 buf_size,
			 page_size,
			 alignment,
			 NO_SPECIAL_CASE);
	vm->unified_va = true;
	if (ret != UNIT_SUCCESS) {
		unit_err(m, "Mapping failed (non-unified VA, fixed GPU VA)\n");
		ret = UNIT_FAIL;
		goto exit;
	}

	/*
	 * Corner case: VA is not unified, GPU_VA fixed below
	 * nvgpu_gmmu_va_small_page_limit()
	 */
	vm->unified_va = false;
	gpu_va = nvgpu_gmmu_va_small_page_limit() - buf_size*10;
	ret = map_buffer(m,
			 g,
			 vm,
			 NULL,
			 BUF_CPU_PA,
			 gpu_va,
			 buf_size,
			 page_size,
			 alignment,
			 NO_SPECIAL_CASE);
	vm->unified_va = true;
	if (ret != UNIT_SUCCESS) {
		unit_err(m, "Mapping failed (non-unified VA, fixed GPU VA)\n");
		ret = UNIT_FAIL;
		goto exit;
	}

	/*
	 * Corner case: do not allocate a VM area which will force an allocation
	 * with small pages.
	 */
	ret = map_buffer(m,
			 g,
			 vm,
			 NULL,
			 BUF_CPU_PA,
			 gpu_va,
			 buf_size,
			 page_size,
			 alignment,
			 SPECIAL_CASE_NO_VM_AREA);
	if (ret != UNIT_SUCCESS) {
		unit_err(m, "Mapping failed (SPECIAL_CASE_NO_FREE)\n");
		ret = UNIT_FAIL;
		goto exit;
	}

	/*
	 * Corner case: do not unmap the buffer so that nvgpu_vm_put can take
	 * care of the cleanup of both the mapping and the VM area.
	 */
	ret = map_buffer(m,
			 g,
			 vm,
			 NULL,
			 BUF_CPU_PA,
			 gpu_va,
			 buf_size,
			 page_size,
			 alignment,
			 SPECIAL_CASE_NO_FREE);
	if (ret != UNIT_SUCCESS) {
		unit_err(m, "Mapping failed (SPECIAL_CASE_NO_FREE)\n");
		ret = UNIT_FAIL;
		goto exit;
	}

	ret = UNIT_SUCCESS;

exit:
	if (vm != NULL) {
		nvgpu_vm_put(vm);
	}

	return ret;
}

/*
 * Dummy cache flush ops for counting number of cache flushes
 */
static unsigned int test_batch_tlb_inval_cnt = 0;
static int test_batch_fb_tlb_invalidate(struct gk20a *g, struct nvgpu_mem *pdb)
{
	test_batch_tlb_inval_cnt++;
	return 0;
}

static unsigned int test_batch_l2_flush_cnt = 0;
static int test_batch_mm_l2_flush(struct gk20a *g, bool invalidate)
{
	test_batch_l2_flush_cnt++;
	return 0;
}

int test_batch(struct unit_module *m, struct gk20a *g, void *__args)
{
	int ret = UNIT_SUCCESS;
	struct vm_gk20a *vm = NULL;
	u64 low_hole = 0;
	u64 user_vma = 0;
	u64 kernel_reserved = 0;
	u64 aperture_size = 0;
	bool big_pages = true;
	int i = 0;
	u64 buf_cpu_pa = 0;
	size_t buf_size = 0;
	size_t page_size = 0;
	size_t alignment = 0;
	struct vm_gk20a_mapping_batch batch;

	if (m == NULL) {
		ret = UNIT_FAIL;
		goto exit;
	}
	if (g == NULL) {
		unit_err(m, "gk20a is NULL\n");
		ret = UNIT_FAIL;
		goto exit;
	}

	/* Initialize test environment */
	ret = init_test_env(m, g);
	if (ret != UNIT_SUCCESS) {
		goto exit;
	}
	/* Set custom cache flush ops */
	g->ops.fb.tlb_invalidate = test_batch_fb_tlb_invalidate;
	g->ops.mm.cache.l2_flush = test_batch_mm_l2_flush;

	/* Initialize VM */
	big_pages = true;
	low_hole = SZ_1M * 64;
	aperture_size = 128 * SZ_1G;
	kernel_reserved = 4 * SZ_1G - low_hole;
	user_vma = aperture_size - low_hole - kernel_reserved;
	unit_info(m, "Initializing VM:\n");
	unit_info(m, "   - Low Hole Size = 0x%llx\n", low_hole);
	unit_info(m, "   - User Aperture Size = 0x%llx\n", user_vma);
	unit_info(m, "   - Kernel Reserved Size = 0x%llx\n", kernel_reserved);
	unit_info(m, "   - Total Aperture Size = 0x%llx\n", aperture_size);
	vm = nvgpu_vm_init(g,
			   g->ops.mm.gmmu.get_default_big_page_size(),
			   low_hole,
			   user_vma,
			   kernel_reserved,
			   nvgpu_gmmu_va_small_page_limit(),
			   big_pages,
			   false,
			   true,
			   __func__);
	if (vm == NULL) {
		unit_err(m, "Failed to init VM\n");
		ret = UNIT_FAIL;
		goto exit;
	}

	nvgpu_vm_mapping_batch_start(&batch);

	/* Map buffers */
	buf_cpu_pa = BUF_CPU_PA;
	buf_size = SZ_4K;
	page_size = SZ_4K;
	alignment = SZ_4K;
	for (i = 0; i < TEST_BATCH_NUM_BUFFERS; i++) {
		unit_info(m, "Mapping Buffer #%d:\n", i);
		unit_info(m, "   - CPU PA = 0x%llx\n", buf_cpu_pa);
		unit_info(m, "   - Buffer Size = 0x%lx\n", buf_size);
		unit_info(m, "   - Page Size = 0x%lx\n", page_size);
		unit_info(m, "   - Alignment = 0x%lx\n", alignment);
		ret = map_buffer(m,
				 g,
				 vm,
				 &batch,
				 buf_cpu_pa,
				 0,
				 buf_size,
				 page_size,
				 alignment,
				 NO_SPECIAL_CASE);
		if (ret != UNIT_SUCCESS) {
			unit_err(m, "Buffer mapping failed\n");
			goto clean_up;
		}

		buf_cpu_pa += buf_size;
	}

	ret = UNIT_SUCCESS;

clean_up:
	nvgpu_vm_mapping_batch_finish(vm, &batch);
	/* Verify cache flush counts */
	if (ret == UNIT_SUCCESS) {
		if (!batch.need_tlb_invalidate ||
		    !batch.gpu_l2_flushed) {
			unit_err(m, "batch struct is invalid\n");
			ret = UNIT_FAIL;
		}
		if (test_batch_tlb_inval_cnt != 1) {
			unit_err(m, "Incorrect number of TLB invalidates\n");
			ret = UNIT_FAIL;
		}
		if (test_batch_l2_flush_cnt != 1) {
			unit_err(m, "Incorrect number of L2 flushes\n");
			ret = UNIT_FAIL;
		}

		/*
		* Cause an error in tlb_invalidate for code coverage of
		* nvgpu_vm_mapping_batch_finish
		*/
		g->ops.fb.tlb_invalidate = hal_fb_tlb_invalidate_error;
		nvgpu_vm_mapping_batch_finish(vm, &batch);
		g->ops.fb.tlb_invalidate = gm20b_fb_tlb_invalidate;
	}

	nvgpu_vm_put(vm);

exit:
	return ret;
}

int test_vm_area_error_cases(struct unit_module *m, struct gk20a *g,
	void *__args)
{
	int ret;
	struct vm_gk20a *vm = create_test_vm(m, g);
	struct nvgpu_vm_area *pvm_area = NULL;
	u64 map_addr = 0;
	u64 map_size = 0;
	u32 pgsz_idx = 0;
	struct nvgpu_posix_fault_inj *kmem_fi =
		nvgpu_kmem_get_fault_injection();
	/* Arbitrary address in the range of the VM created by create_test_vm */
	u64 gpu_va = 0x4100000;

	/*
	 * Failure: "fixed offset mapping with invalid map_size"
	 * The mapped size is 0.
	 */
	ret = nvgpu_vm_area_validate_buffer(vm, map_addr, map_size, pgsz_idx,
		&pvm_area);
	if (ret != -EINVAL) {
		unit_err(m, "area_validate_buffer did not fail as expected (1).\n");
		ret = UNIT_FAIL;
		goto exit;
	}

	/*
	 * Failure: "map offset must be buffer page size aligned"
	 * The mapped address is not aligned to the page size.
	 */
	map_addr = 0x121;
	map_size = SZ_1M;
	ret = nvgpu_vm_area_validate_buffer(vm, map_addr, map_size, pgsz_idx,
		&pvm_area);
	if (ret != -EINVAL) {
		unit_err(m, "area_validate_buffer did not fail as expected (2).\n");
		ret = UNIT_FAIL;
		goto exit;
	}

	/*
	 * Failure: "fixed offset mapping without space allocation"
	 * The VM has no VM area.
	 */
	map_addr = gpu_va;
	map_size = SZ_4K;
	ret = nvgpu_vm_area_validate_buffer(vm, map_addr, map_size, pgsz_idx,
		&pvm_area);
	if (ret != -EINVAL) {
		unit_err(m, "area_validate_buffer did not fail as expected (3).\n");
		ret = UNIT_FAIL;
		goto exit;
	}

	/*
	 * To continue testing nvgpu_vm_area_validate_buffer, we now need
	 * a VM area. First target error cases for nvgpu_vm_area_alloc and then
	 * create a 10-page VM_AREA and assign it to the VM and enable sparse
	 * support to cover extra corner cases.
	 */
	/* Failure: invalid page size (SZ_1G) */
	ret = nvgpu_vm_area_alloc(vm, 10, SZ_1G, &gpu_va, 0);
	if (ret != -EINVAL) {
		unit_err(m, "nvgpu_vm_area_alloc did not fail as expected (4).\n");
		goto exit;
	}

	/* Failure: big page size in a VM that does not support it */
	vm->big_pages = false;
	ret = nvgpu_vm_area_alloc(vm, 10, SZ_64K, &gpu_va, 0);
	vm->big_pages = true;
	if (ret != -EINVAL) {
		unit_err(m, "nvgpu_vm_area_alloc did not fail as expected (4).\n");
		goto exit;
	}

	/* Failure: Dynamic allocation of vm_area fails */
	nvgpu_posix_enable_fault_injection(kmem_fi, true, 0);
	ret = nvgpu_vm_area_alloc(vm, 10, SZ_4K, &gpu_va, 0);
	nvgpu_posix_enable_fault_injection(kmem_fi, false, 0);
	if (ret != -ENOMEM) {
		unit_err(m, "nvgpu_vm_area_alloc did not fail as expected (5).\n");
		goto exit;
	}

	/* Failure: Dynamic allocation in nvgpu_vm_area_alloc_memory fails */
	nvgpu_posix_enable_fault_injection(kmem_fi, true, 1);
	ret = nvgpu_vm_area_alloc(vm, 10, SZ_4K, &gpu_va, 0);
	nvgpu_posix_enable_fault_injection(kmem_fi, false, 0);
	if (ret != -ENOMEM) {
		unit_err(m, "nvgpu_vm_area_alloc did not fail as expected (5).\n");
		goto exit;
	}

	/* Failure: Dynamic allocation in nvgpu_vm_area_alloc_gmmu_map fails */
	nvgpu_posix_enable_fault_injection(kmem_fi, true, 25);
	ret = nvgpu_vm_area_alloc(vm, 10, SZ_4K, &gpu_va,
		NVGPU_VM_AREA_ALLOC_SPARSE);
	nvgpu_posix_enable_fault_injection(kmem_fi, false, 0);
	if (ret != -ENOMEM) {
		unit_err(m, "nvgpu_vm_area_alloc did not fail as expected (5).\n");
		goto exit;
	}

	/*
	 * Now make nvgpu_vm_area_alloc succeed to be able to continue testing
	 * failures within nvgpu_vm_area_validate_buffer.
	 */
	ret = nvgpu_vm_area_alloc(vm, 10, SZ_4K, &gpu_va,
		NVGPU_VM_AREA_ALLOC_SPARSE);
	if (ret != 0) {
		unit_err(m, "nvgpu_vm_area_alloc failed.\n");
		goto exit;
	}

	/*
	 * Failure: "fixed offset mapping size overflows va node"
	 * Make the mapped size bigger than the VA space.
	 */
	map_addr = gpu_va;
	map_size = SZ_4K + 128*SZ_1G;
	ret = nvgpu_vm_area_validate_buffer(vm, map_addr, map_size, pgsz_idx,
		&pvm_area);
	if (ret != -EINVAL) {
		unit_err(m, "area_validate_buffer did not fail as expected (5).\n");
		ret = UNIT_FAIL;
		goto exit;
	}

	/*
	 * Failure: "overlapping buffer map requested"
	 * Map the buffer, then try to validate the same buffer again.
	 */
	map_addr = gpu_va + SZ_4K;
	map_size = SZ_4K;
	ret = map_buffer(m,
			 g,
			 vm,
			 NULL,
			 map_addr,
			 map_addr,
			 map_size,
			 SZ_4K,
			 SZ_4K,
			 SPECIAL_CASE_NO_VM_AREA | SPECIAL_CASE_NO_FREE);
	if (ret != UNIT_SUCCESS) {
		unit_err(m, "4KB buffer mapping failed\n");
		goto exit;
	}
	ret = nvgpu_vm_area_validate_buffer(vm, map_addr, map_size, pgsz_idx,
		&pvm_area);

	if (ret != -EINVAL) {
		unit_err(m, "area_validate_buffer did not fail as expected (5).\n");
		ret = UNIT_FAIL;
		goto exit;
	}

	ret = UNIT_SUCCESS;

exit:
	/*
	 * The mapped buffer is not explicitly freed because it will be taken
	 * care of by nvgpu_vm_area_free, thus increasing code coverage.
	 */
	nvgpu_vm_area_free(vm, gpu_va);
	nvgpu_vm_put(vm);

	return ret;
}

int test_gk20a_from_vm(struct unit_module *m, struct gk20a *g, void *args)
{
	struct vm_gk20a *vm = create_test_vm(m, g);
	int ret = UNIT_FAIL;

	if (g != gk20a_from_vm(vm)) {
		unit_err(m, "ptr mismatch in gk20a_from_vm\n");
		goto exit;
	}

	ret = UNIT_SUCCESS;

exit:
	nvgpu_vm_put(vm);

	return ret;
}

static bool is_overlapping_mapping(struct nvgpu_rbtree_node *root, u64 addr,
	u64 size)
{
	struct nvgpu_rbtree_node *node = NULL;
	struct nvgpu_mapped_buf *buffer;

	nvgpu_rbtree_search(addr, &node, root);
	if (!node)
		return false;

	buffer = mapped_buffer_from_rbtree_node(node);
	if (addr + size > buffer->addr)
		return true;

	return false;
}


int test_nvgpu_insert_mapped_buf(struct unit_module *m, struct gk20a *g,
	void *args)
{
	int ret = UNIT_FAIL;
	struct vm_gk20a *vm = create_test_vm(m, g);
	struct nvgpu_mapped_buf *mapped_buffer = NULL;
	u64 map_addr = BUF_CPU_PA;
	u64 size = SZ_64K;

	if (is_overlapping_mapping(vm->mapped_buffers, map_addr, size)) {
		unit_err(m, "addr already mapped");
		ret = UNIT_FAIL;
		goto done;
	}

	mapped_buffer = malloc(sizeof(*mapped_buffer));
	if (!mapped_buffer) {
		ret = UNIT_FAIL;
		goto done;
	}

	mapped_buffer->addr	= map_addr;
	mapped_buffer->size	= size;
	mapped_buffer->pgsz_idx	= GMMU_PAGE_SIZE_BIG;
	mapped_buffer->vm	= vm;
	nvgpu_init_list_node(&mapped_buffer->buffer_list);
	nvgpu_ref_init(&mapped_buffer->ref);

	nvgpu_insert_mapped_buf(vm, mapped_buffer);

	if (!is_overlapping_mapping(vm->mapped_buffers, map_addr, size)) {
		unit_err(m, "addr NOT already mapped");
		ret = UNIT_FAIL;
		goto done;
	}

	ret = UNIT_SUCCESS;

done:
	nvgpu_vm_free_va(vm, map_addr, 0);

	return ret;
}

int test_vm_pde_coverage_bit_count(struct unit_module *m, struct gk20a *g,
	void *args)
{
	u32 bit_count;
	int ret = UNIT_FAIL;
	struct vm_gk20a *vm = create_test_vm(m, g);

	bit_count = nvgpu_vm_pde_coverage_bit_count(g, vm->big_page_size);

	if (bit_count != GP10B_PDE_BIT_COUNT) {
		unit_err(m, "invalid PDE bit count\n");
		goto done;
	}

	ret = UNIT_SUCCESS;

done:
	nvgpu_vm_put(vm);

	return ret;
}

struct unit_module_test vm_tests[] = {
	/*
	 * Requirement verification tests
	 */
	UNIT_TEST_REQ("NVGPU-RQCD-45.C1",
		      VM_REQ1_UID,
		      "V5",
		      map_buf,
		      test_map_buf,
		      NULL,
		      0),
	UNIT_TEST(init_error_paths, test_init_error_paths, NULL, 0),
	UNIT_TEST(map_buffer_error_cases, test_map_buffer_error_cases, NULL, 0),
	UNIT_TEST(map_buffer_security, test_map_buffer_security, NULL, 0),
	UNIT_TEST(map_buffer_security_error_cases, test_map_buffer_security_error_cases, NULL, 0),
	UNIT_TEST(nvgpu_vm_alloc_va, test_nvgpu_vm_alloc_va, NULL, 0),
	UNIT_TEST(vm_bind, test_vm_bind, NULL, 2),
	UNIT_TEST(vm_aspace_id, test_vm_aspace_id, NULL, 0),
	UNIT_TEST(vm_area_error_cases, test_vm_area_error_cases, NULL, 0),
	UNIT_TEST_REQ("NVGPU-RQCD-45.C2",
		      VM_REQ1_UID,
		      "V5",
		      map_buf_gpu_va,
		      test_map_buf_gpu_va,
		      NULL,
		      0),

	/*
	 * Feature tests
	 */
	UNIT_TEST(batch,
		  test_batch,
		  NULL,
		  0),
	UNIT_TEST(gk20a_from_vm, test_gk20a_from_vm, NULL, 0),
	UNIT_TEST(nvgpu_insert_mapped_buf, test_nvgpu_insert_mapped_buf, NULL,
		0),
	UNIT_TEST(vm_pde_coverage_bit_count, test_vm_pde_coverage_bit_count,
		NULL, 0),
};

UNIT_MODULE(vm, vm_tests, UNIT_PRIO_NVGPU_TEST);
