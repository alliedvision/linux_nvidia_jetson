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

#include <nvgpu/log.h>
#include <nvgpu/io.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/gr/subctx.h>
#include <nvgpu/gr/ctx.h>
#include <nvgpu/gr/zcull.h>
#include <nvgpu/gr/config.h>

#include "zcull_priv.h"

int nvgpu_gr_zcull_init(struct gk20a *g, struct nvgpu_gr_zcull **gr_zcull,
			u32 size, struct nvgpu_gr_config *config)
{
	struct nvgpu_gr_zcull *zcull;
	int err = 0;

	nvgpu_log(g, gpu_dbg_gr, "size = %u", size);

	zcull = nvgpu_kzalloc(g, sizeof(*zcull));
	if (zcull == NULL) {
		err = -ENOMEM;
		goto exit;
	}

	zcull->g = g;

	zcull->zcull_ctxsw_image_size = size;

	zcull->aliquot_width = nvgpu_gr_config_get_tpc_count(config) * 16U;
	zcull->aliquot_height = 16;

	zcull->width_align_pixels =
		nvgpu_gr_config_get_tpc_count(config) * 16U;
	zcull->height_align_pixels = 32;

	zcull->aliquot_size =
		zcull->aliquot_width * zcull->aliquot_height;

	/* assume no floor sweeping since we only have 1 tpc in 1 gpc */
	zcull->pixel_squares_by_aliquots =
		nvgpu_gr_config_get_zcb_count(config) * 16U * 16U *
		nvgpu_gr_config_get_tpc_count(config) /
		(nvgpu_gr_config_get_gpc_count(config) *
		 nvgpu_gr_config_get_gpc_tpc_count(config, 0U));

exit:
	*gr_zcull = zcull;
	return err;
}

void nvgpu_gr_zcull_deinit(struct gk20a *g, struct nvgpu_gr_zcull *gr_zcull)
{
	if (gr_zcull == NULL) {
		return;
	}

	nvgpu_kfree(g, gr_zcull);
}

u32 nvgpu_gr_get_ctxsw_zcull_size(struct gk20a *g,
				struct nvgpu_gr_zcull *gr_zcull)
{
	(void)g;

	/* assuming zcull has already been initialized */
	return gr_zcull->zcull_ctxsw_image_size;
}

int nvgpu_gr_zcull_init_hw(struct gk20a *g,
			struct nvgpu_gr_zcull *gr_zcull,
			struct nvgpu_gr_config *gr_config)
{
	u32 *zcull_map_tiles, *zcull_bank_counters;
	u32 map_counter;
	u32 num_gpcs = nvgpu_get_litter_value(g, GPU_LIT_NUM_GPCS);
	u32 num_tpc_per_gpc = nvgpu_get_litter_value(g,
						GPU_LIT_NUM_TPC_PER_GPC);
	u32 zcull_alloc_num = num_gpcs * num_tpc_per_gpc;
	u32 map_tile_count;
	int ret = 0;

	nvgpu_log(g, gpu_dbg_gr, " ");

	if (nvgpu_gr_config_get_map_tiles(gr_config) == NULL) {
		return -1;
	}

	if (zcull_alloc_num % 8U != 0U) {
		/* Total 8 fields per map reg i.e. tile_0 to tile_7*/
		zcull_alloc_num += (zcull_alloc_num % 8U);
	}
	zcull_map_tiles = nvgpu_kzalloc(g, zcull_alloc_num * sizeof(u32));

	if (zcull_map_tiles == NULL) {
		nvgpu_err(g,
			"failed to allocate zcull map titles");
		return -ENOMEM;
	}

	zcull_bank_counters = nvgpu_kzalloc(g, zcull_alloc_num * sizeof(u32));

	if (zcull_bank_counters == NULL) {
		nvgpu_err(g,
			"failed to allocate zcull bank counters");
		nvgpu_kfree(g, zcull_map_tiles);
		return -ENOMEM;
	}

	for (map_counter = 0;
	     map_counter < nvgpu_gr_config_get_tpc_count(gr_config);
	     map_counter++) {
		map_tile_count =
			nvgpu_gr_config_get_map_tile_count(gr_config,
							map_counter);
		zcull_map_tiles[map_counter] =
			zcull_bank_counters[map_tile_count];
		zcull_bank_counters[map_tile_count]++;
	}

	if (g->ops.gr.zcull.program_zcull_mapping != NULL) {
		g->ops.gr.zcull.program_zcull_mapping(g, zcull_alloc_num,
						zcull_map_tiles);
	}

	nvgpu_kfree(g, zcull_map_tiles);
	nvgpu_kfree(g, zcull_bank_counters);

	if (g->ops.gr.zcull.init_zcull_hw != NULL) {
		ret = g->ops.gr.zcull.init_zcull_hw(g, gr_zcull, gr_config);
		if (ret != 0) {
			nvgpu_err(g, "failed to init zcull hw. err:%d", ret);
			return ret;
		}
	}

	nvgpu_log(g, gpu_dbg_gr, "done");
	return 0;
}

int nvgpu_gr_zcull_ctx_setup(struct gk20a *g, struct nvgpu_gr_subctx *subctx,
		struct nvgpu_gr_ctx *gr_ctx)
{
	int ret = 0;

	if (subctx != NULL) {
		ret = nvgpu_gr_ctx_zcull_setup(g, gr_ctx, false);
		if (ret == 0) {
			nvgpu_gr_subctx_zcull_setup(g, subctx, gr_ctx);
		}
	} else {
		ret = nvgpu_gr_ctx_zcull_setup(g, gr_ctx, true);
	}

	return ret;
}

