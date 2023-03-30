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

#include "priv_ring_gv11b.h"

#include <nvgpu/hw/gv11b/hw_pri_ringstation_sys_gv11b.h>
#include <nvgpu/hw/gv11b/hw_pri_ringstation_gpc_gv11b.h>
#include <nvgpu/hw/gv11b/hw_pri_ringstation_fbp_gv11b.h>

void gv11b_priv_ring_read_pri_fence(struct gk20a *g)
{
	/* Read back to ensure all writes to all chiplets are complete. */
	nvgpu_readl(g, pri_ringstation_sys_pri_fence_r());
	nvgpu_readl(g, pri_ringstation_gpc_pri_fence_r());
	nvgpu_readl(g, pri_ringstation_fbp_pri_fence_r());
}

