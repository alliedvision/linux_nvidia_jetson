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
#include <nvgpu/kmem.h>
#include <nvgpu/log.h>
#include <nvgpu/fbp.h>
#include <nvgpu/static_analysis.h>

#include "fbp_priv.h"

int nvgpu_fbp_init_support(struct gk20a *g)
{
	struct nvgpu_fbp *fbp;
	u32 fbp_en_mask;
#ifdef CONFIG_NVGPU_NON_FUSA
	u32 max_ltc_per_fbp;
	u32 l2_all_en_mask;
	unsigned long i;
	unsigned long fbp_en_mask_tmp;
	u32 tmp;
#endif

	if (g->fbp != NULL) {
		return 0;
	}

	fbp = nvgpu_kzalloc(g, sizeof(*fbp));
	if (fbp == NULL) {
		return -ENOMEM;
	}

#ifdef CONFIG_NVGPU_NON_FUSA
	fbp->num_fbps = g->ops.priv_ring.get_fbp_count(g);
	nvgpu_log_info(g, "fbps: %d", fbp->num_fbps);
#endif

	fbp->max_fbps_count = g->ops.top.get_max_fbps_count(g);
	nvgpu_log_info(g, "max_fbps_count: %d", fbp->max_fbps_count);

	/*
	 * Read active fbp mask from fuse
	 * Note that 0:enable and 1:disable in value read from fuse so we've to
	 * flip the bits.
	 * Also set unused bits to zero
	 */
	fbp_en_mask = g->ops.fuse.fuse_status_opt_fbp(g);
	fbp_en_mask = ~fbp_en_mask;
	fbp_en_mask = fbp_en_mask &
		nvgpu_safe_sub_u32(BIT32(fbp->max_fbps_count), 1U);
	fbp->fbp_en_mask = fbp_en_mask;

#ifdef CONFIG_NVGPU_NON_FUSA
	fbp->fbp_l2_en_mask =
		nvgpu_kzalloc(g,
			nvgpu_safe_mult_u64(fbp->max_fbps_count, sizeof(u32)));
	if (fbp->fbp_l2_en_mask == NULL) {
		nvgpu_kfree(g, fbp);
		return -ENOMEM;
	}

	fbp_en_mask_tmp = fbp_en_mask;
	max_ltc_per_fbp = g->ops.top.get_max_ltc_per_fbp(g);
	l2_all_en_mask = nvgpu_safe_sub_u32(BIT32(max_ltc_per_fbp), 1U);

	/* get active L2 mask per FBP */
	for_each_set_bit(i, &fbp_en_mask_tmp, fbp->max_fbps_count) {
		tmp = g->ops.fuse.fuse_status_opt_l2_fbp(g, (u32)i);
		fbp->fbp_l2_en_mask[i] = l2_all_en_mask ^ tmp;
	}
#endif

	g->fbp = fbp;

	return 0;
}

void nvgpu_fbp_remove_support(struct gk20a *g)
{
	struct nvgpu_fbp *fbp = g->fbp;

	if (fbp != NULL) {
		nvgpu_kfree(g, fbp->fbp_l2_en_mask);
		nvgpu_kfree(g, fbp);
	}

	g->fbp = NULL;
}

u32 nvgpu_fbp_get_max_fbps_count(struct nvgpu_fbp *fbp)
{
	return fbp->max_fbps_count;
}

u32 nvgpu_fbp_get_fbp_en_mask(struct nvgpu_fbp *fbp)
{
	return fbp->fbp_en_mask;
}

#ifdef CONFIG_NVGPU_NON_FUSA
u32 nvgpu_fbp_get_num_fbps(struct nvgpu_fbp *fbp)
{
	return fbp->num_fbps;
}

u32 *nvgpu_fbp_get_l2_en_mask(struct nvgpu_fbp *fbp)
{
	return fbp->fbp_l2_en_mask;
}
#endif

