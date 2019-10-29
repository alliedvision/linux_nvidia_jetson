/*
 * NVIDIA GPU HAL interface.
 *
 * Copyright (c) 2014-2018, NVIDIA CORPORATION.  All rights reserved.
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

#include "gk20a.h"
#include "hal.h"
#include "gm20b/hal_gm20b.h"
#include "gp10b/hal_gp10b.h"
#include "gp106/hal_gp106.h"
#include "gv100/hal_gv100.h"
#include "gv11b/hal_gv11b.h"
#if defined(CONFIG_TEGRA_GPU_NEXT)
#include "nvgpu_gpuid_next.h"
#endif

#include <nvgpu/log.h>

int gpu_init_hal(struct gk20a *g)
{
	u32 ver = g->params.gpu_arch + g->params.gpu_impl;
	switch (ver) {
	case GK20A_GPUID_GM20B:
	case GK20A_GPUID_GM20B_B:
		nvgpu_log_info(g, "gm20b detected");
		if (gm20b_init_hal(g))
			return -ENODEV;
		break;
	case NVGPU_GPUID_GP10B:
		if (gp10b_init_hal(g))
			return -ENODEV;
		break;
	case NVGPU_GPUID_GP104:
	case NVGPU_GPUID_GP106:
		if (gp106_init_hal(g))
			return -ENODEV;
		break;
	case NVGPU_GPUID_GV11B:
		if (gv11b_init_hal(g))
			return -ENODEV;
		break;
	case NVGPU_GPUID_GV100:
		if (gv100_init_hal(g))
			return -ENODEV;
		break;
#if defined(CONFIG_TEGRA_GPU_NEXT)
	case NVGPU_GPUID_NEXT:
		if (NVGPU_NEXT_INIT_HAL(g))
			return -ENODEV;
		break;
	case NVGPU_GPUID_NEXT_2:
		if (NVGPU_NEXT_2_INIT_HAL(g))
			return -ENODEV;
		break;
#endif

	default:
		nvgpu_err(g, "no support for %x", ver);
		return -ENODEV;
	}

	return 0;
}
