/*
 * Copyright (c) 2018, NVIDIA CORPORATION.  All rights reserved.
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

#include <nvgpu/io.h>
#include <nvgpu/io_usermode.h>
#include <nvgpu/bug.h>

/*
 * For now none of these make sense to execute in userspace. Eventually we
 * may want to use these to verify certain register read/write sequences
 * but for now, just hang.
 */

void nvgpu_writel(struct gk20a *g, u32 r, u32 v)
{
	BUG();
}

u32 nvgpu_readl(struct gk20a *g, u32 r)
{
	BUG();

	return 0;
}

u32 __nvgpu_readl(struct gk20a *g, u32 r)
{
	BUG();

	return 0;
}

void nvgpu_writel_check(struct gk20a *g, u32 r, u32 v)
{
	BUG();
}

void nvgpu_bar1_writel(struct gk20a *g, u32 b, u32 v)
{
	BUG();
}

u32 nvgpu_bar1_readl(struct gk20a *g, u32 b)
{
	BUG();

	return 0;
}

bool nvgpu_io_exists(struct gk20a *g)
{
	return false;
}

bool nvgpu_io_valid_reg(struct gk20a *g, u32 r)
{
	return false;
}

void nvgpu_usermode_writel(struct gk20a *g, u32 r, u32 v)
{
	BUG();
}
