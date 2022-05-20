/*
 * GV11B CBC
 *
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


#include <nvgpu/cbc.h>
#include <nvgpu/log.h>
#include <nvgpu/gk20a.h>

#include "cbc_gv11b.h"

void gv11b_cbc_init(struct gk20a *g, struct nvgpu_cbc *cbc)
{
	u32 max_size = g->max_comptag_mem;
	/* one tag line covers 64KB */
	u32 max_comptag_lines = max_size << 4;

	nvgpu_log_fn(g, " ");

	g->ops.fb.cbc_configure(g, cbc);

	g->ops.cbc.ctrl(g, nvgpu_cbc_op_invalidate,
			0, max_comptag_lines - 1U);

}
