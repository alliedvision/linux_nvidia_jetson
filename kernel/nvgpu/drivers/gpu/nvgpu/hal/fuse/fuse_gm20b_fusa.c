/*
 * GM20B FUSE
 *
 * Copyright (c) 2017-2021, NVIDIA CORPORATION.  All rights reserved.
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

#include <nvgpu/types.h>
#include <nvgpu/fuse.h>
#include <nvgpu/enabled.h>
#include <nvgpu/io.h>
#include <nvgpu/gk20a.h>

#include "fuse_gm20b.h"

#include <nvgpu/hw/gm20b/hw_fuse_gm20b.h>

u32 gm20b_fuse_status_opt_fbio(struct gk20a *g)
{
	return nvgpu_readl(g, fuse_status_opt_fbio_r());
}

u32 gm20b_fuse_status_opt_fbp(struct gk20a *g)
{
	return nvgpu_readl(g, fuse_status_opt_fbp_r());
}

u32 gm20b_fuse_status_opt_l2_fbp(struct gk20a *g, u32 fbp)
{
	return nvgpu_readl(g, fuse_status_opt_rop_l2_fbp_r(fbp));
}

u32 gm20b_fuse_status_opt_tpc_gpc(struct gk20a *g, u32 gpc)
{
	u32 max_gpc_count = g->ops.top.get_max_gpc_count(g);
	if (gpc >= max_gpc_count) {
		BUG();
	}

	return nvgpu_readl(g, fuse_status_opt_tpc_gpc_r(gpc));
}

void gm20b_fuse_ctrl_opt_tpc_gpc(struct gk20a *g, u32 gpc, u32 val)
{
	nvgpu_writel(g, fuse_ctrl_opt_tpc_gpc_r(gpc), val);
}

u32 gm20b_fuse_opt_sec_debug_en(struct gk20a *g)
{
	return nvgpu_readl(g, fuse_opt_sec_debug_en_r());
}

u32 gm20b_fuse_opt_priv_sec_en(struct gk20a *g)
{
	return nvgpu_readl(g, fuse_opt_priv_sec_en_r());
}
