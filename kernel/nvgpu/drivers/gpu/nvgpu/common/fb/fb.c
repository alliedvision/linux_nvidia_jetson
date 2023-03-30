/*
 * Copyright (c) 2020-2022, NVIDIA CORPORATION.  All rights reserved.
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
#include <nvgpu/power_features/cg.h>
#include <nvgpu/fb.h>

int nvgpu_init_fb_support(struct gk20a *g)
{
	if (g->ops.mc.fb_reset != NULL) {
		g->ops.mc.fb_reset(g);
	}

	nvgpu_cg_slcg_fb_load_enable(g);

	nvgpu_cg_blcg_fb_load_enable(g);

	if (g->ops.fb.init_fs_state != NULL) {
		g->ops.fb.init_fs_state(g);
	}
	return 0;
}

#if defined(CONFIG_NVGPU_NON_FUSA) && defined(CONFIG_NVGPU_HAL_NON_FUSA)
int nvgpu_fb_vab_init_hal(struct gk20a *g)
{
	int err = 0;

	if (g->ops.fb.vab.init != NULL) {
		err = g->ops.fb.vab.init(g);
	}
	return err;
}

int nvgpu_fb_vab_teardown_hal(struct gk20a *g)
{
	int err = 0;

	if (g->ops.fb.vab.teardown != NULL)  {
		err = g->ops.fb.vab.teardown(g);
	}
	return err;
}
#endif
