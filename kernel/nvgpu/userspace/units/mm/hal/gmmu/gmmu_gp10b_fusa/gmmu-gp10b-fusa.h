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

#ifndef UNIT_MM_HAL_GMMU_GMMU_GP10B_FUSA_H
#define UNIT_MM_HAL_GMMU_GMMU_GP10B_FUSA_H

struct gk20a;
struct unit_module;

/** @addtogroup SWUTS-mm-hal-gmmu-gmmu_gp10b_fusa
 *  @{
 *
 * Software Unit Test Specification for mm.hal.gmmu.gmmu_gp10b_fusa
 */

/**
 * Test specification for: test_gp10b_mm_get_default_big_page_size
 *
 * Description: Test big page size
 *
 * Test Type: Feature
 *
 * Targets: gops_mm.gops_mm_gmmu.get_default_big_page_size,
 * nvgpu_gmmu_default_big_page_size
 *
 * Input: None
 *
 * Steps:
 * - Check big page size value and confirm that size is 64K.
 *
 * Output: Returns SUCCESS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_gp10b_mm_get_default_big_page_size(struct unit_module *m,
						struct gk20a *g, void *args);

/**
 * Test specification for: test_gp10b_mm_get_iommu_bit
 *
 * Description: Test IOMMU bit number
 *
 * Test Type: Feature
 *
 * Targets: gops_mm_gmmu.get_iommu_bit, gp10b_mm_get_iommu_bit
 *
 * Input: None
 *
 * Steps:
 * - Check iommu bit is equal to 36.
 *
 * Output: Returns SUCCESS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_gp10b_mm_get_iommu_bit(struct unit_module *m, struct gk20a *g,
								void *args);

/**
 * Test specification for: test_gp10b_get_max_page_table_levels
 *
 * Description: Test max page table levels
 *
 * Test Type: Feature
 *
 * Targets: gops_mm_gmmu.get_max_page_table_levels,
 * gp10b_get_max_page_table_levels
 *
 * Input: None
 *
 * Steps:
 * - Check max page table levels is 5.
 *
 * Output: Returns SUCCESS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_gp10b_get_max_page_table_levels(struct unit_module *m,
						struct gk20a *g, void *args);

/**
 * Test specification for: test_gp10b_mm_get_mmu_levels
 *
 * Description: Test mmu_levels structure
 *
 * Test Type: Feature
 *
 * Targets: gops_mm_gmmu.get_mmu_levels, gp10b_mm_get_mmu_levels
 *
 * Input: None
 *
 * Steps:
 * - Copy mmu_levels structure and validate struct using update_entry pointer.
 *
 * Output: Returns SUCCESS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_gp10b_mm_get_mmu_levels(struct unit_module *m,
						struct gk20a *g, void *args);

/**
 * Test specification for: test_update_gmmu_pde3_locked
 *
 * Description: Test mmu_levels update entry function
 *
 * Test Type: Feature
 *
 * Targets: update_gmmu_pde3_locked, pte_dbg_print
 *
 * Input: None
 *
 * Steps:
 * - Update gmmu pde3 for given physical address.
 * - Check if data written to memory is as expected.
 *
 * Output: Returns SUCCESS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_update_gmmu_pde3_locked(struct unit_module *m,
						struct gk20a *g, void *args);

/**
 * Test specification for: test_update_gmmu_pde0_locked
 *
 * Description: Test mmu_level 3 update entry function
 *
 * Test Type: Feature
 *
 * Targets: update_gmmu_pde0_locked, pte_dbg_print
 *
 * Input: args (value can be F_UPDATE_GMMU_PDE0_SMALL_PAGE or
 *        F_UPDATE_GMMU_PDE0_BIG_PAGE)
 *
 * Steps:
 * - Update gmmu pde3 for given physical address.
 * - For big and small page size, check data written to memory.
 *
 * Output: Returns SUCCESS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_update_gmmu_pde0_locked(struct unit_module *m,
						struct gk20a *g, void *args);

/**
 * Test specification for: test_update_gmmu_pte_locked
 *
 * Description: Test mmu_level 4 update entry function
 *
 * Test Type: Feature
 *
 * Targets: update_gmmu_pte_locked, update_pte, update_pte_sparse,
 *          gmmu_aperture_mask
 *
 * Input: args (value can be F_UPDATE_PTE_PHYS_ADDR_ZERO, F_UPDATE_PTE_DEFAULT,
 *        F_UPDATE_PTE_ATTRS_PRIV_READ_ONLY, F_UPDATE_PTE_ATTRS_VALID,
 *        F_UPDATE_PTE_ATTRS_CACHEABLE, F_UPDATE_PTE_ATTRS_VIDMEM,
 *        F_UPDATE_PTE_PLATFORM_ATOMIC or F_UPDATE_PTE_SPARSE)
 *
 * Steps:
 * - Update gmmu pte for given physical address.
 * - Check data written to pd mem for various scenarios such as cacheable GMMU
 *   mapping, priviledged mapping, read only address, etc.
 *
 * Output: Returns SUCCESS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_update_gmmu_pte_locked(struct unit_module *m,
						struct gk20a *g, void *args);

/**
 * Test specification for: test_gp10b_get_pde0_pgsz
 *
 * Description: Test GMMU level 3 page size function
 *
 * Test Type: Feature
 *
 * Targets: gp10b_get_pde0_pgsz
 *
 * Input: args (value can be F_PDE_BIG_PAGE_APERTURE_SET_ONLY,
 *        F_PDE_BIG_PAGE_APERTURE_ADDR_SET, F_PDE_SMALL_PAGE_APERTURE_SET_ONLY,
 *        F_PDE_SMALL_PAGE_APERTURE_ADDR_SET, F_PDE_SMALL_BIG_SET or
 *        F_PDE0_PGSZ_MEM_NULL)
 *
 * Steps:
 * - Check pde0 page size for given aperture values
 *
 * Output: Returns SUCCESS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_gp10b_get_pde0_pgsz(struct unit_module *m, struct gk20a *g,
								void *args);

/** @} */
#endif /* UNIT_MM_HAL_GMMU_GMMU_GP10B_FUSA_H */
