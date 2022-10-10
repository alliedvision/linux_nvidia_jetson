/*
 * Copyright (c) 2022, NVIDIA CORPORATION.  All rights reserved.
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
#include <nvgpu/static_analysis.h>
#include <nvgpu/gr/config.h>

#include "gr_config_ga10b.h"

#include <nvgpu/hw/ga10b/hw_gr_ga10b.h>

u32 ga10b_gr_config_get_gpc_rop_mask(struct gk20a *g,
	struct nvgpu_gr_config *config, u32 gpc_index)
{
	u32 val;
	u32 rop_cnt = nvgpu_gr_config_get_max_rop_per_gpc_count(config);

	/*
	 * Toggle the bits of NV_FUSE_STATUS_OPT_ROP_GPC to get the non-FSed
	 * ROP mask.
	 */
	val = g->ops.fuse.fuse_status_opt_rop_gpc(g, gpc_index);

	return (~val) & nvgpu_safe_sub_u32(BIT32(rop_cnt), 1U);
}
