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

#include <nvgpu/io.h>
#include <nvgpu/debug.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/engine_status.h>
#include <nvgpu/engines.h>
#include <nvgpu/device.h>
#include <nvgpu/fifo.h>
#include <nvgpu/string.h>

#include "engine_status_ga10b.h"
#include <nvgpu/hw/ga10b/hw_runlist_ga10b.h>

static void populate_invalid_ctxsw_status_info(
		struct nvgpu_engine_status_info *status_info)
{
	status_info->ctx_id = ENGINE_STATUS_CTX_ID_INVALID;
	status_info->ctx_id_type = ENGINE_STATUS_CTX_NEXT_ID_TYPE_INVALID;
	status_info->ctx_next_id =
		ENGINE_STATUS_CTX_NEXT_ID_INVALID;
	status_info->ctx_next_id_type = ENGINE_STATUS_CTX_NEXT_ID_TYPE_INVALID;
	status_info->ctxsw_status = NVGPU_CTX_STATUS_INVALID;
}

static void populate_valid_ctxsw_status_info(
		struct nvgpu_engine_status_info *status_info)
{
	status_info->ctx_id =
		runlist_engine_status0_tsgid_v(status_info->reg_data);
	status_info->ctx_id_type = ENGINE_STATUS_CTX_ID_TYPE_TSGID;
	status_info->ctx_next_id =
		ENGINE_STATUS_CTX_NEXT_ID_INVALID;
	status_info->ctx_next_id_type = ENGINE_STATUS_CTX_NEXT_ID_TYPE_INVALID;
	status_info->ctxsw_status = NVGPU_CTX_STATUS_VALID;
}

static void populate_load_ctxsw_status_info(
		struct nvgpu_engine_status_info *status_info)
{
	status_info->ctx_id = ENGINE_STATUS_CTX_ID_INVALID;
	status_info->ctx_id_type = ENGINE_STATUS_CTX_ID_TYPE_INVALID;
	status_info->ctx_next_id =
		runlist_engine_status0_next_tsgid_v(status_info->reg_data);
	status_info->ctx_next_id_type = ENGINE_STATUS_CTX_NEXT_ID_TYPE_TSGID;
	status_info->ctxsw_status = NVGPU_CTX_STATUS_CTXSW_LOAD;
}

static void populate_save_ctxsw_status_info(
		struct nvgpu_engine_status_info *status_info)
{
	status_info->ctx_id =
		runlist_engine_status0_tsgid_v(status_info->reg_data);
	status_info->ctx_id_type = ENGINE_STATUS_CTX_ID_TYPE_TSGID;
	status_info->ctx_next_id =
		ENGINE_STATUS_CTX_NEXT_ID_INVALID;
	status_info->ctx_next_id_type = ENGINE_STATUS_CTX_NEXT_ID_TYPE_INVALID;
	status_info->ctxsw_status = NVGPU_CTX_STATUS_CTXSW_SAVE;
}

static void populate_switch_ctxsw_status_info(
		struct nvgpu_engine_status_info *status_info)
{
	status_info->ctx_id =
		runlist_engine_status0_tsgid_v(status_info->reg_data);
	status_info->ctx_id_type = ENGINE_STATUS_CTX_ID_TYPE_TSGID;
	status_info->ctx_next_id =
		runlist_engine_status0_next_tsgid_v(status_info->reg_data);
	status_info->ctx_next_id_type = ENGINE_STATUS_CTX_NEXT_ID_TYPE_TSGID;
	status_info->ctxsw_status = NVGPU_CTX_STATUS_CTXSW_SWITCH;
}

void ga10b_read_engine_status_info(struct gk20a *g, u32 engine_id,
		struct nvgpu_engine_status_info *status)
{
	u32 engine_reg0_data;
	u32 engine_reg1_data;
	u32 ctxsw_state;
	const struct nvgpu_device *dev;

	(void) memset(status, 0U, sizeof(*status));

	if (!nvgpu_engine_check_valid_id(g, engine_id)) {
		/* just return NULL info */
		return;
	}

	dev = g->fifo.host_engines[engine_id];

	engine_reg0_data = nvgpu_readl(g, nvgpu_safe_add_u32(
		dev->rl_pri_base,
		runlist_engine_status0_r(dev->rleng_id)));

	engine_reg1_data = nvgpu_readl(g, nvgpu_safe_add_u32(
		dev->rl_pri_base,
		runlist_engine_status1_r(dev->rleng_id)));

	status->reg_data = engine_reg0_data;
	status->reg1_data = engine_reg1_data;

	/* populate the engine_state enum */
	status->is_busy = runlist_engine_status0_engine_v(engine_reg0_data) ==
			runlist_engine_status0_engine_busy_v();

	/* populate the engine_faulted_state enum */
	status->is_faulted =
			runlist_engine_status0_faulted_v(engine_reg0_data) ==
			runlist_engine_status0_faulted_true_v();

	/* populate the ctxsw_in_progress_state */
	status->ctxsw_in_progress = ((engine_reg0_data &
			runlist_engine_status0_ctxsw_in_progress_f()) != 0U);

	/* populate the ctxsw related info */
	ctxsw_state = runlist_engine_status0_ctx_status_v(engine_reg0_data);

	status->ctxsw_state = ctxsw_state;

	/* check for ctx_status switch/load/save before valid */
	if (ctxsw_state ==
			runlist_engine_status0_ctx_status_switch_v()) {
		populate_switch_ctxsw_status_info(status);
	} else if (ctxsw_state ==
			runlist_engine_status0_ctx_status_load_v()) {
		populate_load_ctxsw_status_info(status);
	} else if (ctxsw_state ==
			runlist_engine_status0_ctx_status_save_v()) {
		populate_save_ctxsw_status_info(status);
	} else if (ctxsw_state == runlist_engine_status0_ctx_status_valid_v()) {
		populate_valid_ctxsw_status_info(status);
	} else {
		populate_invalid_ctxsw_status_info(status);
	}
}
