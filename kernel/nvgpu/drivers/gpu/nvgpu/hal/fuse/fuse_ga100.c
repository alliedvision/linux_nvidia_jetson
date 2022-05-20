/*
 * GA100 FUSE
 *
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

#include <nvgpu/gk20a.h>
#include <nvgpu/types.h>
#include <nvgpu/io.h>
#include <nvgpu/falcon.h>

#include "fuse_ga100.h"

#include <nvgpu/hw/ga100/hw_fuse_ga100.h>

u32 ga100_fuse_status_opt_l2_fbp(struct gk20a *g, u32 fbp)
{
	return nvgpu_readl(g, fuse_status_opt_rop_l2_fbp_r(fbp));
}

int ga100_fuse_read_per_device_identifier(struct gk20a *g, u64 *pdi)
{
	u32 lo = nvgpu_readl(g, fuse_opt_pdi_0_r());
	u32 hi = nvgpu_readl(g, fuse_opt_pdi_1_r());

	*pdi = ((u64)lo) | (((u64)hi) << 32);

	return 0;
}

int ga100_read_ucode_version(struct gk20a *g, u32 falcon_id, u32 *ucode_version)
{
	u32 err = 0;

	switch (falcon_id) {
	case FALCON_ID_GSPLITE:
		*ucode_version = nvgpu_readl(g, fuse_gsp_ucode1_version_r());
		break;
	case FALCON_ID_SEC2:
		*ucode_version = nvgpu_readl(g, fuse_sec2_ucode1_version_r());
		break;
	default:
		err = -EINVAL;
		nvgpu_err(g, "Invalid/Unsupported falcon ID %x", falcon_id);
		break;
	}

	return err;
}
