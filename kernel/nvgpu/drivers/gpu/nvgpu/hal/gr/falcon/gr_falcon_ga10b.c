/*
 * Copyright (c) 2020, NVIDIA CORPORATION.  All rights reserved.
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
#include <nvgpu/io.h>

#include "gr_falcon_ga10b.h"

#include <nvgpu/hw/ga10b/hw_gr_ga10b.h>

#ifdef CONFIG_NVGPU_GR_FALCON_NON_SECURE_BOOT
void ga10b_gr_falcon_gpccs_dmemc_write(struct gk20a *g, u32 port, u32 offs,
	u32 blk, u32 ainc)
{
	nvgpu_writel(g, gr_gpccs_dmemc_r(port),
			gr_gpccs_dmemc_offs_f(offs) |
			gr_gpccs_dmemc_blk_f(blk) |
			gr_gpccs_dmemc_aincw_f(ainc));
}

void ga10b_gr_falcon_gpccs_imemc_write(struct gk20a *g, u32 port, u32 offs,
	u32 blk, u32 ainc)
{
	nvgpu_writel(g, gr_gpccs_imemc_r(port),
			gr_gpccs_imemc_offs_f(offs) |
			gr_gpccs_imemc_blk_f(blk) |
			gr_gpccs_imemc_aincw_f(ainc));
}

void ga10b_gr_falcon_fecs_imemc_write(struct gk20a *g, u32 port, u32 offs,
	u32 blk, u32 ainc)
{
	nvgpu_writel(g, gr_fecs_imemc_r(port),
			gr_fecs_imemc_offs_f(offs) |
			gr_fecs_imemc_blk_f(blk) |
			gr_fecs_imemc_aincw_f(ainc));
}

#endif
