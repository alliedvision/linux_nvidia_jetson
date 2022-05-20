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

#include <nvgpu/hw/gv100/hw_fifo_gv100.h>

#include "engine_status_gm20b.h"
#include "engine_status_gv100.h"

void gv100_read_engine_status_info(struct gk20a *g, u32 engine_id,
		struct nvgpu_engine_status_info *status)
{
	u32 engine_reg_data;

	gm20b_read_engine_status_info(g, engine_id, status);
	engine_reg_data = status->reg_data;
	/* populate the engine reload status */
	status->in_reload_status =
		fifo_engine_status_eng_reload_v(engine_reg_data) != 0U;

	return;
}

void gv100_dump_engine_status(struct gk20a *g, struct nvgpu_debug_context *o)
{
	u32 i, host_num_engines;
	struct nvgpu_engine_status_info engine_status;

	host_num_engines = nvgpu_get_litter_value(g, GPU_LIT_HOST_NUM_ENGINES);

	for (i = 0; i < host_num_engines; i++) {
		if (!nvgpu_engine_check_valid_id(g, i)) {
			/* Skip invalid engines */
			continue;
		}

		g->ops.engine_status.read_engine_status_info(g, i, &engine_status);

		gk20a_debug_output(o, "%s eng %d: ", g->name, i);
		gk20a_debug_output(o,
			"id: %d (%s), next_id: %d (%s), ctx status: %s ",
			engine_status.ctx_id,
			nvgpu_engine_status_is_ctx_type_tsg(
				&engine_status) ?
				"tsg" : "channel",
			engine_status.ctx_next_id,
			nvgpu_engine_status_is_next_ctx_type_tsg(
				&engine_status) ?
				"tsg" : "channel",
			nvgpu_fifo_decode_pbdma_ch_eng_status(
				engine_status.ctxsw_state));

		if (engine_status.in_reload_status) {
			gk20a_debug_output(o, "ctx_reload ");
		}
		if (engine_status.is_faulted) {
			gk20a_debug_output(o, "faulted ");
		}
		if (engine_status.is_busy) {
			gk20a_debug_output(o, "busy ");
		}
		gk20a_debug_output(o, " ");
	}
	gk20a_debug_output(o, " ");
}
