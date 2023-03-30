/*
 * Copyright (c) 2017-2022, NVIDIA CORPORATION.  All rights reserved.
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
#include <nvgpu/falcon.h>
#include <nvgpu/string.h>

#include "falcon_gk20a.h"

#include <nvgpu/hw/gm20b/hw_falcon_gm20b.h>

bool gk20a_falcon_clear_halt_interrupt_status(struct nvgpu_falcon *flcn)
{
	struct gk20a *g = flcn->g;
	u32 base_addr = flcn->flcn_base;
	u32 data = 0;
	bool status = false;

	gk20a_writel(g, base_addr + falcon_falcon_irqsclr_r(),
		gk20a_readl(g, base_addr + falcon_falcon_irqsclr_r()) |
		0x10U);
	data = gk20a_readl(g, (base_addr + falcon_falcon_irqstat_r()));

	if ((data & falcon_falcon_irqstat_halt_true_f()) !=
		falcon_falcon_irqstat_halt_true_f()) {
		/*halt irq is clear*/
		status = true;
	}

	return status;
}


int gk20a_falcon_copy_from_imem(struct nvgpu_falcon *flcn, u32 src,
	u8 *dst, u32 size, u8 port)
{
	struct gk20a *g = flcn->g;
	u32 base_addr = flcn->flcn_base;
	u32 *dst_u32 = (u32 *)dst;
	u32 words = 0;
	u32 bytes = 0;
	u32 data = 0;
	u32 blk = 0;
	u32 i = 0;

	nvgpu_log_info(g, "download %d bytes from 0x%x", size, src);

	words = size >> 2U;
	bytes = size & 0x3U;
	blk = src >> 8;

	nvgpu_log_info(g, "download %d words from 0x%x block %d",
			words, src, blk);

	nvgpu_writel(g, base_addr + falcon_falcon_imemc_r(port),
		falcon_falcon_imemc_offs_f(src >> 2) |
		g->ops.falcon.imemc_blk_field(blk) |
		falcon_falcon_dmemc_aincr_f(1));

	for (i = 0; i < words; i++) {
		dst_u32[i] = nvgpu_readl(g,
			base_addr + falcon_falcon_imemd_r(port));
	}

	if (bytes > 0U) {
		data = nvgpu_readl(g, base_addr + falcon_falcon_imemd_r(port));
		nvgpu_memcpy(&dst[words << 2U], (u8 *)&data, bytes);
	}

	return 0;
}

void gk20a_falcon_get_ctls(struct nvgpu_falcon *flcn, u32 *sctl,
				  u32 *cpuctl)
{
	*sctl = gk20a_readl(flcn->g, flcn->flcn_base + falcon_falcon_sctl_r());
	*cpuctl = gk20a_readl(flcn->g, flcn->flcn_base +
					falcon_falcon_cpuctl_r());
}
