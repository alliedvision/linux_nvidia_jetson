/*
 * Copyright (c) 2019-2020, NVIDIA CORPORATION.  All rights reserved.
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
#include <nvgpu/debug.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/engine_status.h>
#include <nvgpu/engines.h>
#include <nvgpu/fifo.h>

#include <nvgpu/hw/gm20b/hw_fifo_gm20b.h>

#include "engine_status_gm20b.h"

void gm20b_dump_engine_status(struct gk20a *g, struct nvgpu_debug_context *o)
{
	u32 i, host_num_engines;
	struct nvgpu_engine_status_info engine_status;

	host_num_engines = nvgpu_get_litter_value(g, GPU_LIT_HOST_NUM_ENGINES);

	gk20a_debug_output(o, "Engine status - chip %-5s", g->name);
	gk20a_debug_output(o, "--------------------------");

	for (i = 0; i < host_num_engines; i++) {
		if (!nvgpu_engine_check_valid_id(g, i)) {
			/* Skip invalid engines */
			continue;
		}

		g->ops.engine_status.read_engine_status_info(g, i, &engine_status);

		gk20a_debug_output(o,
			"Engine %d | "
			"ID: %d - %-9s next_id: %d %-9s | status: %s",
			i,
			engine_status.ctx_id,
			nvgpu_engine_status_is_ctx_type_tsg(
				&engine_status) ?
				"[tsg]" : "[channel]",
			engine_status.ctx_next_id,
			nvgpu_engine_status_is_next_ctx_type_tsg(
				&engine_status) ?
				"[tsg]" : "[channel]",
			nvgpu_fifo_decode_pbdma_ch_eng_status(
				engine_status.ctxsw_state));

		if (engine_status.is_faulted) {
			gk20a_debug_output(o, "  State: faulted");
		}
		if (engine_status.is_busy) {
			gk20a_debug_output(o, "  State: busy");
		}
	}
	gk20a_debug_output(o, " ");
}
