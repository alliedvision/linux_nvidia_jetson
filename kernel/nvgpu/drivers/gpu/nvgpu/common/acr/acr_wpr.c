/*
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

#include <nvgpu/gk20a.h>
#include <nvgpu/types.h>
#include <nvgpu/dma.h>

#include "acr_wpr.h"

/* Both size and address of WPR need to be 128K-aligned */
#define DGPU_WPR_SIZE 0x200000U

void nvgpu_acr_wpr_info_sys(struct gk20a *g, struct wpr_carveout_info *inf)
{
	g->ops.fb.read_wpr_info(g, &inf->wpr_base, &inf->size);
}
#ifdef CONFIG_NVGPU_DGPU
void nvgpu_acr_wpr_info_vid(struct gk20a *g, struct wpr_carveout_info *inf)
{
	inf->wpr_base = g->mm.vidmem.bootstrap_base;
	inf->nonwpr_base = inf->wpr_base + DGPU_WPR_SIZE;
	inf->size = DGPU_WPR_SIZE;
}
#endif
