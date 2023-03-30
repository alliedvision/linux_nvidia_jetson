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

#include <nvgpu/gk20a.h>
#include <nvgpu/types.h>
#include <nvgpu/gmmu.h>

#include "hal/mm/gmmu/gmmu_gp10b.h"
#include "hal/mm/gmmu/gmmu_gv11b.h"

#include "gmmu-gv11b-fusa.h"

#define F_GV11B_GPU_PHYS_ADDR_GMMU_ATTRS_NULL	0
#define F_GV11B_GPU_PHYS_ADDR_L3_ALLOC_FALSE	1
#define F_GV11B_GPU_PHYS_ADDR_L3_ALLOC_TRUE	2

int test_gv11b_gpu_phys_addr(struct unit_module *m, struct gk20a *g, void *args)
{
	struct gpu_ops gops = g->ops;
	struct nvgpu_gmmu_attrs attrs = {0};
	struct nvgpu_gmmu_attrs *attrs_ptr;
	u64 phys = BIT(10);
	u64 ret_phys;
	u64 branch = (u64)args;
	int ret = UNIT_FAIL;

	g->ops.mm.gmmu.get_iommu_bit = gp10b_mm_get_iommu_bit;

	attrs_ptr = branch == F_GV11B_GPU_PHYS_ADDR_GMMU_ATTRS_NULL ?
			NULL : &attrs;

	attrs.l3_alloc = branch == F_GV11B_GPU_PHYS_ADDR_L3_ALLOC_FALSE ?
			false : true;

	ret_phys = gv11b_gpu_phys_addr(g, attrs_ptr, phys);

	if (branch == F_GV11B_GPU_PHYS_ADDR_L3_ALLOC_TRUE) {
		unit_assert(ret_phys == (phys |
			BIT64(g->ops.mm.gmmu.get_iommu_bit(g))),
			goto done);
	} else {
		unit_assert(ret_phys == phys, goto done);
	}

	ret = UNIT_SUCCESS;

done:
	if (ret != UNIT_SUCCESS) {
		unit_err(m, "%s pde_pgsz != GMMU_PAGE_SIZE_SMALL as expected\n",
				__func__);
	}

	g->ops = gops;
	return ret;
}

struct unit_module_test mm_gmmu_gv11b_fusa_tests[] = {
	UNIT_TEST(gpu_phys_addr_s0, test_gv11b_gpu_phys_addr, (void *)F_GV11B_GPU_PHYS_ADDR_GMMU_ATTRS_NULL, 0),
	UNIT_TEST(gpu_phys_addr_s1, test_gv11b_gpu_phys_addr, (void *)F_GV11B_GPU_PHYS_ADDR_L3_ALLOC_FALSE, 0),
	UNIT_TEST(gpu_phys_addr_s2, test_gv11b_gpu_phys_addr, (void *)F_GV11B_GPU_PHYS_ADDR_L3_ALLOC_TRUE, 0),
};

UNIT_MODULE(gmmu_gv11b_fusa, mm_gmmu_gv11b_fusa_tests, UNIT_PRIO_NVGPU_TEST);
