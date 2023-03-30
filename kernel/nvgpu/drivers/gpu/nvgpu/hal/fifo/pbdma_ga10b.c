/*
 * Copyright (c) 2020, NVIDIA CORPORATION.  All rights reserved.
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
#include <nvgpu/log2.h>
#include <nvgpu/utils.h>
#include <nvgpu/io.h>
#include <nvgpu/bitops.h>
#include <nvgpu/bug.h>
#include <nvgpu/debug.h>
#include <nvgpu/fifo.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/pbdma_status.h>

#include <nvgpu/hw/ga10b/hw_pbdma_ga10b.h>

#include "pbdma_ga10b.h"

void ga10b_pbdma_dump_status(struct gk20a *g, struct nvgpu_debug_context *o)
{
	u32 i, host_num_pbdma;
	struct nvgpu_pbdma_status_info pbdma_status;

	host_num_pbdma = nvgpu_get_litter_value(g, GPU_LIT_HOST_NUM_PBDMA);

	gk20a_debug_output(o, "PBDMA Status - chip %-5s", g->name);
	gk20a_debug_output(o, "-------------------------");

	for (i = 0U; i < host_num_pbdma; i++) {
		g->ops.pbdma_status.read_pbdma_status_info(g, i,
			&pbdma_status);

		gk20a_debug_output(o, "pbdma %d:", i);
		gk20a_debug_output(o,
			"  id: %d - %-9s next_id: - %d %-9s | status: %s",
			pbdma_status.id,
			nvgpu_pbdma_status_is_id_type_tsg(&pbdma_status) ?
				   "[tsg]" : "[channel]",
			pbdma_status.next_id,
			nvgpu_pbdma_status_is_next_id_type_tsg(
				&pbdma_status) ?
				   "[tsg]" : "[channel]",
			nvgpu_fifo_decode_pbdma_ch_eng_status(
				pbdma_status.pbdma_channel_status));
		gk20a_debug_output(o,
			"  PBDMA_PUT %016llx PBDMA_GET %016llx",
			(u64)nvgpu_readl(g, pbdma_put_r(i)) +
			((u64)nvgpu_readl(g, pbdma_put_hi_r(i)) << 32ULL),
			(u64)nvgpu_readl(g, pbdma_get_r(i)) +
			((u64)nvgpu_readl(g, pbdma_get_hi_r(i)) << 32ULL));
		gk20a_debug_output(o,
			"  GP_PUT    %08x  GP_GET  %08x  "
			"FETCH   %08x HEADER %08x",
			nvgpu_readl(g, pbdma_gp_put_r(i)),
			nvgpu_readl(g, pbdma_gp_get_r(i)),
			nvgpu_readl(g, pbdma_gp_fetch_r(i)),
			nvgpu_readl(g, pbdma_pb_header_r(i)));
		gk20a_debug_output(o,
			"  HDR       %08x  SHADOW0 %08x  SHADOW1 %08x",
			g->ops.pbdma.read_data(g, i),
			nvgpu_readl(g, pbdma_gp_shadow_0_r(i)),
			nvgpu_readl(g, pbdma_gp_shadow_1_r(i)));
	}

	gk20a_debug_output(o, " ");
}
