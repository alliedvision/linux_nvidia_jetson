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

#include <nvgpu/gk20a.h>
#include <nvgpu/gmmu.h>

#include "gmmu_gv11b.h"

/*
 * On Volta the GPU determines whether to do L3 allocation for a mapping by
 * checking the l3 alloc bit (bit number depends on soc) of the physical address.
 * So if a mapping should allocate lines in the L3 this bit must be set.
 */
u64 gv11b_gpu_phys_addr(struct gk20a *g,
			struct nvgpu_gmmu_attrs *attrs, u64 phys)
{
	if ((attrs != NULL) && attrs->l3_alloc &&
			(g->ops.mm.gmmu.get_iommu_bit != NULL)) {
		return phys | BIT64(g->ops.mm.gmmu.get_iommu_bit(g));
	}

	return phys;
}
