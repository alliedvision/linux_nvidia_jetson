/*
 * Copyright (c) 2019-2021, NVIDIA CORPORATION.  All rights reserved.
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

#include <nvgpu/hw/gv11b/hw_flush_gv11b.h>

#include "flush_gk20a.h"
#include "flush_gv11b.h"

int gv11b_mm_l2_flush(struct gk20a *g, bool invalidate)
{
	int err = 0;

	nvgpu_log(g, gpu_dbg_mm, "gv11b_mm_l2_flush");

	err = g->ops.mm.cache.fb_flush(g);
	if (err != 0) {
		nvgpu_err(g, "mm.cache.fb_flush()[1] failed err=%d", err);
		return err;
	}
	err = gk20a_mm_l2_flush(g, invalidate);
	if (err != 0) {
		nvgpu_err(g, "gk20a_mm_l2_flush failed");
		return err;
	}
	if (g->ops.bus.bar1_bind != NULL) {
		err = g->ops.fb.tlb_invalidate(g, g->mm.bar1.vm->pdb.mem);
		if (err != 0) {
			nvgpu_err(g, "fb.tlb_invalidate() failed err=%d", err);
			return err;
		}
	} else {
		err = g->ops.mm.cache.fb_flush(g);
		if (err != 0) {
			nvgpu_err(g, "mm.cache.fb_flush()[2] failed err=%d",
				  err);
			return err;
		}
	}

	return err;
}
