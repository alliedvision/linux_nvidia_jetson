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

#include "pramin_init.h"
#include "pramin_gp10b.h"
#ifdef CONFIG_NVGPU_DGPU
#include "pramin_gv100.h"
#include "pramin_tu104.h"
#endif

void nvgpu_pramin_ops_init(struct gk20a *g)
{
	u32 ver = g->params.gpu_arch + g->params.gpu_impl;

	switch (ver) {
	case NVGPU_GPUID_GP10B:
		g->ops.pramin.data032_r = gp10b_pramin_data032_r;
		break;
#ifdef CONFIG_NVGPU_DGPU
	case NVGPU_GPUID_GV100:
		g->ops.pramin.data032_r = gv100_pramin_data032_r;
		break;
	case NVGPU_GPUID_TU104:
#ifdef CONFIG_NVGPU_HAL_NON_FUSA
	case NVGPU_GPUID_GA100:
#endif
		g->ops.pramin.data032_r = tu104_pramin_data032_r;
		break;
#endif
	default:
		g->ops.pramin.data032_r = NULL;
		break;
	}
}
