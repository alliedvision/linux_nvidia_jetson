/*
 * Copyright (c) 2019, NVIDIA CORPORATION.  All rights reserved.
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
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

#include <unit/io.h>
#include <unit/unit.h>

#include <nvgpu/types.h>
#include <nvgpu/engine_status.h>

#include "../nvgpu-fifo-common.h"
#include "nvgpu-engine-status.h"

#define assert(cond)	unit_assert(cond, goto done)

#define NUM_CTXSW_STATUS	6
#define NUM_ID_TYPES		3
#define NUM_NEXT_ID_TYPES	3

int test_engine_status(struct unit_module *m,
		struct gk20a *g, void *args)
{
	int ret = UNIT_FAIL;
	struct nvgpu_engine_status_info status;
	const u32 ctxsw_status[NUM_CTXSW_STATUS] = {
		NVGPU_CTX_STATUS_INVALID,
		NVGPU_CTX_STATUS_VALID,
		NVGPU_CTX_STATUS_CTXSW_LOAD,
		NVGPU_CTX_STATUS_CTXSW_SAVE,
		NVGPU_CTX_STATUS_CTXSW_SWITCH,
		U32(~0),
	};
	const u32 id_types[NUM_ID_TYPES] = {
		ENGINE_STATUS_CTX_ID_TYPE_CHID,
		ENGINE_STATUS_CTX_ID_TYPE_TSGID,
		ENGINE_STATUS_CTX_ID_TYPE_INVALID,
	};
	const u32 next_id_types[NUM_NEXT_ID_TYPES] = {
		ENGINE_STATUS_CTX_NEXT_ID_TYPE_CHID,
		ENGINE_STATUS_CTX_NEXT_ID_TYPE_TSGID,
		ENGINE_STATUS_CTX_NEXT_ID_TYPE_INVALID,
	};
	int i;

	for (i = 0; i < NUM_CTXSW_STATUS; i++)
	{
		status.ctxsw_status = ctxsw_status[i];
		assert(nvgpu_engine_status_is_ctxsw_switch(&status) ==
			(ctxsw_status[i] == NVGPU_CTX_STATUS_CTXSW_SWITCH));
		assert(nvgpu_engine_status_is_ctxsw_load(&status) ==
			(ctxsw_status[i] == NVGPU_CTX_STATUS_CTXSW_LOAD));
		assert(nvgpu_engine_status_is_ctxsw_save(&status) ==
			(ctxsw_status[i] == NVGPU_CTX_STATUS_CTXSW_SAVE));
		assert(nvgpu_engine_status_is_ctxsw(&status) ==
			((ctxsw_status[i] == NVGPU_CTX_STATUS_CTXSW_SWITCH) ||
			 (ctxsw_status[i] == NVGPU_CTX_STATUS_CTXSW_LOAD) ||
			 (ctxsw_status[i] == NVGPU_CTX_STATUS_CTXSW_SAVE)));
		assert(nvgpu_engine_status_is_ctxsw_invalid(&status) ==
			(ctxsw_status[i] == NVGPU_CTX_STATUS_INVALID));
		assert(nvgpu_engine_status_is_ctxsw_valid(&status) ==
			(ctxsw_status[i] == NVGPU_CTX_STATUS_VALID));
	}

	for (i = 0; i < NUM_ID_TYPES; i++)
	{
		u32 ctx_id, ctx_type;
		status.ctx_id = i;
		status.ctx_id_type = id_types[i];
		status.ctx_next_id = 0xcafe;
		status.ctx_next_id_type = 0xcafe;

		assert(nvgpu_engine_status_is_ctx_type_tsg(&status) ==
			(id_types[i] == ENGINE_STATUS_CTX_ID_TYPE_TSGID));
		nvgpu_engine_status_get_ctx_id_type(&status,
			&ctx_id, &ctx_type);
		assert(ctx_id == status.ctx_id);
		assert(ctx_type == status.ctx_id_type);
	}

	for (i = 0; i < NUM_NEXT_ID_TYPES; i++)
	{
		u32 ctx_next_id, ctx_next_type;

		status.ctx_id = 0xcafe;
		status.ctx_id_type = 0xcafe;
		status.ctx_next_id = i;
		status.ctx_next_id_type = next_id_types[i];

		assert(nvgpu_engine_status_is_next_ctx_type_tsg(&status) ==
			(next_id_types[i] ==
				ENGINE_STATUS_CTX_NEXT_ID_TYPE_TSGID));
		nvgpu_engine_status_get_next_ctx_id_type(&status,
			&ctx_next_id, &ctx_next_type);
		assert(ctx_next_id == status.ctx_next_id);
		assert(ctx_next_type == status.ctx_next_id_type);
	}

	ret = UNIT_SUCCESS;
done:
	return ret;
}

