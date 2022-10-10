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

#include <nvgpu/io.h>
#include <nvgpu/debug.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/engine_status.h>
#include <nvgpu/engines.h>
#include <nvgpu/fifo.h>
#include <nvgpu/string.h>

#include <nvgpu/hw/gm20b/hw_fifo_gm20b.h>

#include "engine_status_gm20b.h"

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
	bool id_type_tsg;
	u32 engine_status = status_info->reg_data;

	status_info->ctx_id =
		fifo_engine_status_id_v(status_info->reg_data);
	id_type_tsg = fifo_engine_status_id_type_v(engine_status) ==
			fifo_engine_status_id_type_tsgid_v();
	status_info->ctx_id_type =
		id_type_tsg ? ENGINE_STATUS_CTX_ID_TYPE_TSGID :
			ENGINE_STATUS_CTX_ID_TYPE_CHID;
	status_info->ctx_next_id =
		ENGINE_STATUS_CTX_NEXT_ID_INVALID;
	status_info->ctx_next_id_type = ENGINE_STATUS_CTX_NEXT_ID_TYPE_INVALID;
	status_info->ctxsw_status = NVGPU_CTX_STATUS_VALID;
}

static void populate_load_ctxsw_status_info(
		struct nvgpu_engine_status_info *status_info)
{
	bool next_id_type_tsg;
	u32 engine_status = status_info->reg_data;

	status_info->ctx_id = ENGINE_STATUS_CTX_ID_INVALID;
	status_info->ctx_id_type = ENGINE_STATUS_CTX_ID_TYPE_INVALID;
	status_info->ctx_next_id =
		fifo_engine_status_next_id_v(
			status_info->reg_data);
	next_id_type_tsg = fifo_engine_status_next_id_type_v(engine_status) ==
			fifo_engine_status_next_id_type_tsgid_v();
	status_info->ctx_next_id_type =
		next_id_type_tsg ? ENGINE_STATUS_CTX_NEXT_ID_TYPE_TSGID :
			ENGINE_STATUS_CTX_NEXT_ID_TYPE_CHID;
	status_info->ctxsw_status = NVGPU_CTX_STATUS_CTXSW_LOAD;
}

static void populate_save_ctxsw_status_info(
		struct nvgpu_engine_status_info *status_info)
{
	bool id_type_tsg;
	u32 engine_status = status_info->reg_data;

	status_info->ctx_id =
		fifo_engine_status_id_v(status_info->reg_data);
	id_type_tsg = fifo_engine_status_id_type_v(engine_status) ==
			fifo_engine_status_id_type_tsgid_v();
	status_info->ctx_id_type =
		id_type_tsg ? ENGINE_STATUS_CTX_ID_TYPE_TSGID :
			ENGINE_STATUS_CTX_ID_TYPE_CHID;
	status_info->ctx_next_id =
		ENGINE_STATUS_CTX_NEXT_ID_INVALID;
	status_info->ctx_next_id_type = ENGINE_STATUS_CTX_NEXT_ID_TYPE_INVALID;
	status_info->ctxsw_status = NVGPU_CTX_STATUS_CTXSW_SAVE;
}

static void populate_switch_ctxsw_status_info(
		struct nvgpu_engine_status_info *status_info)
{
	bool id_type_tsg;
	bool next_id_type_tsg;
	u32 engine_status = status_info->reg_data;

	status_info->ctx_id =
		fifo_engine_status_id_v(status_info->reg_data);
	id_type_tsg = fifo_engine_status_id_type_v(engine_status) ==
			fifo_engine_status_id_type_tsgid_v();
	status_info->ctx_id_type =
		id_type_tsg ? ENGINE_STATUS_CTX_ID_TYPE_TSGID :
			ENGINE_STATUS_CTX_ID_TYPE_CHID;
	status_info->ctx_next_id =
		fifo_engine_status_next_id_v(
			status_info->reg_data);
	next_id_type_tsg = fifo_engine_status_next_id_type_v(engine_status) ==
			fifo_engine_status_next_id_type_tsgid_v();
	status_info->ctx_next_id_type =
		next_id_type_tsg ? ENGINE_STATUS_CTX_NEXT_ID_TYPE_TSGID :
			ENGINE_STATUS_CTX_NEXT_ID_TYPE_CHID;
	status_info->ctxsw_status = NVGPU_CTX_STATUS_CTXSW_SWITCH;
}

void gm20b_read_engine_status_info(struct gk20a *g, u32 engine_id,
		struct nvgpu_engine_status_info *status)
{
	u32 engine_reg_data;
	u32 ctxsw_state;

	(void) memset(status, 0, sizeof(*status));

	if (engine_id == NVGPU_INVALID_ENG_ID) {
		/* just return NULL info */
		return;
	}
	engine_reg_data = nvgpu_readl(g, fifo_engine_status_r(engine_id));

	status->reg_data = engine_reg_data;

	/* populate the engine_state enum */
	status->is_busy = fifo_engine_status_engine_v(engine_reg_data) ==
			fifo_engine_status_engine_busy_v();

	/* populate the engine_faulted_state enum */
	status->is_faulted = fifo_engine_status_faulted_v(engine_reg_data) ==
			fifo_engine_status_faulted_true_v();

	/* populate the ctxsw_in_progress_state */
	status->ctxsw_in_progress = ((engine_reg_data &
			fifo_engine_status_ctxsw_in_progress_f()) != 0U);

	/* populate the ctxsw related info */
	ctxsw_state = fifo_engine_status_ctx_status_v(engine_reg_data);

	status->ctxsw_state = ctxsw_state;

	if (ctxsw_state == fifo_engine_status_ctx_status_valid_v()) {
		populate_valid_ctxsw_status_info(status);
	} else if (ctxsw_state ==
			fifo_engine_status_ctx_status_ctxsw_load_v()) {
		populate_load_ctxsw_status_info(status);
	} else if (ctxsw_state ==
			fifo_engine_status_ctx_status_ctxsw_save_v()) {
		populate_save_ctxsw_status_info(status);
	} else if (ctxsw_state ==
			fifo_engine_status_ctx_status_ctxsw_switch_v()) {
		populate_switch_ctxsw_status_info(status);
	} else {
		populate_invalid_ctxsw_status_info(status);
	}
}
