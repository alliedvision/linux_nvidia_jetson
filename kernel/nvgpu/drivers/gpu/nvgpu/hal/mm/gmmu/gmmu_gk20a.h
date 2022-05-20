/*
 * Copyright (c) 2019, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef HAL_MM_GMMU_GMMU_GK20A_H
#define HAL_MM_GMMU_GMMU_GK20A_H

#include <nvgpu/types.h>
#include <nvgpu/gmmu.h>

struct gk20a;

#ifdef CONFIG_NVGPU_HAL_NON_FUSA

const struct gk20a_mmu_level *gk20a_mm_get_mmu_levels(struct gk20a *g,
						      u64 big_page_size);
u32 gk20a_get_max_page_table_levels(struct gk20a *g);
u32 gk20a_mm_get_iommu_bit(struct gk20a *g);
#endif

u32 gk20a_get_pde_pgsz(struct gk20a *g, const struct gk20a_mmu_level *l,
				struct nvgpu_gmmu_pd *pd, u32 pd_idx);
u32 gk20a_get_pte_pgsz(struct gk20a *g, const struct gk20a_mmu_level *l,
				struct nvgpu_gmmu_pd *pd, u32 pd_idx);

#endif
