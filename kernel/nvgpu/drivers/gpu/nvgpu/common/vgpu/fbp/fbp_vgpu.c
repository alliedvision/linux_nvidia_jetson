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

#include <nvgpu/vgpu/vgpu.h>
#include <nvgpu/vgpu/tegra_vgpu.h>
#include <nvgpu/gk20a.h>

#include "fbp_vgpu.h"
#include "common/fbp/fbp_priv.h"

int vgpu_fbp_init_support(struct gk20a *g)
{
	struct vgpu_priv_data *priv = vgpu_get_priv_data(g);
	struct nvgpu_fbp *fbp;
	u32 i;

	if (g->fbp != NULL) {
		return 0;
	}

	fbp = nvgpu_kzalloc(g, sizeof(*fbp));
	if (fbp == NULL) {
		return -ENOMEM;
	}

	fbp->num_fbps = priv->constants.num_fbps;
	fbp->max_fbps_count = priv->constants.num_fbps;
	fbp->fbp_en_mask = priv->constants.fbp_en_mask;

	fbp->fbp_l2_en_mask =
		nvgpu_kzalloc(g, fbp->max_fbps_count * sizeof(u32));
	if (fbp->fbp_l2_en_mask == NULL) {
		nvgpu_kfree(g, fbp);
		return -ENOMEM;
	}

	for (i = 0U; i < fbp->max_fbps_count; i++) {
		fbp->fbp_l2_en_mask[i] = priv->constants.l2_en_mask[i];
	}

	g->fbp = fbp;

	return 0;
}
