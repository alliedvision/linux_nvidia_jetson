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

#include <nvgpu/log.h>
#include <nvgpu/gk20a.h>

#include <nvgpu/vgpu/vgpu.h>
#include <nvgpu/vgpu/os_init_hal_vgpu.h>

#include "init_hal_vgpu.h"
#include "vgpu_hal_gv11b.h"
#ifdef CONFIG_NVGPU_HAL_NON_FUSA
#include "vgpu_hal_ga10b.h"
#endif

int vgpu_init_hal(struct gk20a *g)
{
	u32 ver = g->params.gpu_arch + g->params.gpu_impl;
	int err;

	switch (ver) {
#ifdef CONFIG_NVGPU_HAL_NON_FUSA
	case NVGPU_GPUID_GV11B:
		err = vgpu_gv11b_init_hal(g);
		break;
	case NVGPU_GPUID_GA10B:
		err = vgpu_ga10b_init_hal(g);
		break;
#endif
	default:
		nvgpu_err(g, "no support for %x", ver);
		err = -ENODEV;
		break;
	}

	if (err == 0) {
		err = vgpu_init_hal_os(g);
	}

	return err;
}

void vgpu_detect_chip(struct gk20a *g)
{
	struct nvgpu_gpu_params *p = &g->params;
	struct vgpu_priv_data *priv = vgpu_get_priv_data(g);

	p->gpu_arch = priv->constants.arch;
	p->gpu_impl = priv->constants.impl;
	p->gpu_rev = priv->constants.rev;

	nvgpu_log_info(g, "arch: %x, impl: %x, rev: %x\n",
			p->gpu_arch,
			p->gpu_impl,
			p->gpu_rev);
}
