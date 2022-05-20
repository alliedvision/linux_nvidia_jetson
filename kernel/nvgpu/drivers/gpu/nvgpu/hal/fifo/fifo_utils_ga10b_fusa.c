/*
 * Copyright (c) 2020-2021, NVIDIA CORPORATION.  All rights reserved.
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

#include <nvgpu/bug.h>
#include <nvgpu/io.h>

#include <nvgpu/gk20a.h>
#include <nvgpu/runlist.h>

#include "fifo_utils_ga10b.h"

u32 nvgpu_runlist_readl(struct gk20a *g, struct nvgpu_runlist *runlist,
			u32 r)
{
	u32 runlist_pri_base = 0U;

	nvgpu_assert(runlist != NULL);
	runlist_pri_base = runlist->runlist_pri_base;
	nvgpu_assert(runlist_pri_base != 0U);

	return nvgpu_readl(g, nvgpu_safe_add_u32(runlist_pri_base, r));
}

void nvgpu_runlist_writel(struct gk20a *g, struct nvgpu_runlist *runlist,
			u32 r, u32 v)
{
	u32 runlist_pri_base = 0U;

	nvgpu_assert(runlist != NULL);
	runlist_pri_base = runlist->runlist_pri_base;
	nvgpu_assert(runlist_pri_base != 0U);

	nvgpu_writel(g, nvgpu_safe_add_u32(runlist_pri_base, r), v);
}

u32 nvgpu_chram_bar0_readl(struct gk20a *g, struct nvgpu_runlist *runlist,
			u32 r)
{
	u32 chram_bar0_offset = 0U;

	nvgpu_assert(runlist != NULL);
	chram_bar0_offset = runlist->chram_bar0_offset;
	nvgpu_assert(chram_bar0_offset != 0U);

	return nvgpu_readl(g, nvgpu_safe_add_u32(chram_bar0_offset, r));
}

void nvgpu_chram_bar0_writel(struct gk20a *g,
			struct nvgpu_runlist *runlist, u32 r, u32 v)
{
	u32 chram_bar0_offset = 0U;

	nvgpu_assert(runlist != NULL);
	chram_bar0_offset = runlist->chram_bar0_offset;
	nvgpu_assert(chram_bar0_offset != 0U);

	nvgpu_writel(g, nvgpu_safe_add_u32(chram_bar0_offset, r), v);
}
