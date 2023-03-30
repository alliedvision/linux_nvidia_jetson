/*
 * Copyright (c) 2020-2021, NVIDIA CORPORATION.  All rights reserved.
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

#include <nvgpu/bitops.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/pbdma_status.h>
#include <nvgpu/nvgpu_err.h>
#include <nvgpu/device.h>
#include <nvgpu/runlist.h>
#include <nvgpu/io.h>

#include "pbdma_ga10b.h"
#include "pbdma_ga100.h"

#include <nvgpu/hw/ga100/hw_pbdma_ga100.h>

u32 ga100_pbdma_set_clear_intr_offsets(struct gk20a *g,
			u32 set_clear_size)
{
	u32 ret = 0U;
	switch(set_clear_size) {
		case INTR_SIZE:
			ret = pbdma_intr_0__size_1_v();
			break;
		case INTR_SET_SIZE:
			ret = pbdma_intr_0_en_set_tree__size_1_v();
			break;
		case INTR_CLEAR_SIZE:
			ret = pbdma_intr_0_en_clear_tree__size_1_v();
			break;
		default:
			nvgpu_err(g, "Invalid input for set_clear_intr_offset");
			break;
	}

	return ret;
}

u32 ga100_pbdma_get_fc_target(const struct nvgpu_device *dev)
{
	return (pbdma_target_engine_f(dev->rleng_id) |
			pbdma_target_eng_ctx_valid_true_f() |
			pbdma_target_ce_ctx_valid_true_f());
}

static void ga100_pbdma_force_ce_split_set(struct gk20a *g,
		struct nvgpu_runlist *runlist)
{
	u32 reg;
	u32 i;
	u32 pbdma_id;
	const struct nvgpu_pbdma_info *pbdma_info = NULL;

	pbdma_info = runlist->pbdma_info;
	for (i = 0U; i < PBDMA_PER_RUNLIST_SIZE; i++) {
		pbdma_id = pbdma_info->pbdma_id[i];
		if (pbdma_id == U32_MAX) {
			continue;
		}

		reg = nvgpu_readl(g, pbdma_secure_config_r(pbdma_id));
		reg = set_field(reg, pbdma_secure_config_force_ce_split_m(),
			 pbdma_secure_config_force_ce_split_true_f());
		nvgpu_writel(g, pbdma_secure_config_r(pbdma_id), reg);
	}
}

void ga100_pbdma_force_ce_split(struct gk20a *g)
{
	struct nvgpu_runlist *runlist = NULL;
	u32 i;

	for (i = 0U; i < g->fifo.num_runlists; i++) {
		runlist = g->fifo.runlists[i];
		ga100_pbdma_force_ce_split_set(g, runlist);
	}
}

u32 ga100_pbdma_read_data(struct gk20a *g, u32 pbdma_id)
{
	return nvgpu_readl(g, pbdma_hdr_shadow_r(pbdma_id));
}

u32 ga100_pbdma_get_num_of_pbdmas(void)
{
	return pbdma_cfg0__size_1_v();
}
