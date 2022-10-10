/*
 * Copyright (c) 2011-2022, NVIDIA CORPORATION.  All rights reserved.
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
#include <nvgpu/pbdma.h>

static void nvgpu_pbdma_init_intr_descs(struct gk20a *g)
{
	struct nvgpu_fifo *f = &g->fifo;

	if (g->ops.pbdma.device_fatal_0_intr_descs != NULL) {
		f->intr.pbdma.device_fatal_0 =
			g->ops.pbdma.device_fatal_0_intr_descs();
	}

	if (g->ops.pbdma.channel_fatal_0_intr_descs != NULL) {
		f->intr.pbdma.channel_fatal_0 =
			g->ops.pbdma.channel_fatal_0_intr_descs();
	}
	if (g->ops.pbdma.restartable_0_intr_descs != NULL) {
		f->intr.pbdma.restartable_0 =
			g->ops.pbdma.restartable_0_intr_descs();
	}
}

int nvgpu_pbdma_setup_sw(struct gk20a *g)
{
	nvgpu_pbdma_init_intr_descs(g);

	return 0;
}

void nvgpu_pbdma_cleanup_sw(struct gk20a *g)
{
	(void)g;
	return;
}
