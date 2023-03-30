/*
 * Copyright (c) 2019-2022, NVIDIA CORPORATION.  All rights reserved.
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
#include <nvgpu/log.h>
#include <nvgpu/io.h>
#include <nvgpu/gr/gr_falcon.h>
#include <nvgpu/power_features/pg.h>

#include "fecs_trace_gm20b.h"

#include <nvgpu/hw/gm20b/hw_gr_gm20b.h>

int gm20b_fecs_trace_flush(struct gk20a *g)
{
	int err;

	nvgpu_log(g, gpu_dbg_fn|gpu_dbg_ctxsw, " ");

	err = nvgpu_pg_elpg_protected_call(g,
		g->ops.gr.falcon.ctrl_ctxsw(g,
			NVGPU_GR_FALCON_METHOD_FECS_TRACE_FLUSH, 0U, NULL));
	if (err != 0)
		nvgpu_err(g, "write timestamp record failed");

	return err;
}

int gm20b_fecs_trace_get_read_index(struct gk20a *g)
{
	return nvgpu_pg_elpg_protected_call(g,
			(int)nvgpu_readl(g, gr_fecs_mailbox1_r()));
}

int gm20b_fecs_trace_get_write_index(struct gk20a *g)
{
	return nvgpu_pg_elpg_protected_call(g,
			(int)nvgpu_readl(g, gr_fecs_mailbox0_r()));
}

int gm20b_fecs_trace_set_read_index(struct gk20a *g, int index)
{
	nvgpu_log(g, gpu_dbg_ctxsw, "set read=%d", index);
	return nvgpu_pg_elpg_protected_call(g,
			(nvgpu_writel(g, gr_fecs_mailbox1_r(), (u32)index), 0));
}

u32 gm20b_fecs_trace_get_buffer_full_mailbox_val(void)
{
	return 0x26;
}
