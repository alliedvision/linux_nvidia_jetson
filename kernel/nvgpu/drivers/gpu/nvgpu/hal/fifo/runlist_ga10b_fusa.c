/*
 * GA10B runlist
 *
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

#include <nvgpu/io.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/static_analysis.h>
#include <nvgpu/fifo.h>
#include <nvgpu/pbdma.h>

#include "runlist_ga10b.h"
#include "pbdma_ga10b.h"

#include <nvgpu/hw/ga10b/hw_runlist_ga10b.h>

u32 ga10b_runlist_get_runlist_id(struct gk20a *g, u32 runlist_pri_base)
{
	u32 doorbell_config = nvgpu_readl(g, nvgpu_safe_add_u32(
			runlist_pri_base, runlist_doorbell_config_r()));

	return runlist_doorbell_config_id_v(doorbell_config);
}

u32 ga10b_runlist_get_engine_id_from_rleng_id(struct gk20a *g,
					u32 rleng_id, u32 runlist_pri_base)
{
	u32 engine_status_debug = nvgpu_readl(g, nvgpu_safe_add_u32(
			runlist_pri_base,
			runlist_engine_status_debug_r(rleng_id)));

	return runlist_engine_status_debug_engine_id_v(engine_status_debug);
}

u32 ga10b_runlist_get_chram_bar0_offset(struct gk20a *g, u32 runlist_pri_base)
{
	u32 channel_config = nvgpu_readl(g, nvgpu_safe_add_u32(
			runlist_pri_base, runlist_channel_config_r()));

	return (runlist_channel_config_chram_bar0_offset_v(channel_config)
			<< runlist_channel_config_chram_bar0_offset_b());
}

/*
 * Use u32 runlist_pri_base instead of struct nvgpu_runlist *runlist
 * as input paramter, because by the time this hal is called, runlist_info
 * is not populated.
 */
void ga10b_runlist_get_pbdma_info(struct gk20a *g, u32 runlist_pri_base,
			struct nvgpu_pbdma_info *pbdma_info)
{
	u32 i, pbdma_config;

	if (runlist_pbdma_config__size_1_v() != PBDMA_PER_RUNLIST_SIZE) {
		nvgpu_warn(g, "mismatch: h/w & s/w for pbdma_per_runlist_size");
	}
	for (i = 0U; i < runlist_pbdma_config__size_1_v(); i++) {
		pbdma_config = nvgpu_readl(g, nvgpu_safe_add_u32(
				runlist_pri_base, runlist_pbdma_config_r(i)));
		if (runlist_pbdma_config_valid_v(pbdma_config) ==
				runlist_pbdma_config_valid_true_v()) {
			pbdma_info->pbdma_pri_base[i] =
			runlist_pbdma_config_pbdma_bar0_offset_v(pbdma_config);
			pbdma_info->pbdma_id[i] =
				runlist_pbdma_config_id_v(pbdma_config);
		} else {
			pbdma_info->pbdma_pri_base[i] =
						NVGPU_INVALID_PBDMA_PRI_BASE;
			pbdma_info->pbdma_id[i] = NVGPU_INVALID_PBDMA_ID;
		}
	}
}

u32 ga10b_runlist_get_engine_intr_id(struct gk20a *g, u32 runlist_pri_base,
			u32 rleng_id)
{
	u32 engine_status1;

	engine_status1  = nvgpu_readl(g, nvgpu_safe_add_u32(
			runlist_pri_base, runlist_engine_status1_r(rleng_id)));
	/**
	 * intr_id indicates the engine's default interrupt bit position in the
	 * engine_stall and engine_non_stall leaf registers within the top interrupt
	 * trees.
	 */
	return (runlist_engine_status1_intr_id_v(engine_status1));
}

u32 ga10b_runlist_get_esched_fb_thread_id(struct gk20a *g, u32 runlist_pri_base)
{
	u32 esched_fb_config = nvgpu_readl(g, nvgpu_safe_add_u32(
			runlist_pri_base, runlist_fb_config_r()));

	return runlist_fb_config_fb_thread_id_v(esched_fb_config);
}
