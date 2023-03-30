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

#include <nvgpu/io.h>
#include <nvgpu/engine_status.h>

bool nvgpu_engine_status_is_ctxsw_switch(struct nvgpu_engine_status_info
		*engine_status)
{
	return engine_status->ctxsw_status == NVGPU_CTX_STATUS_CTXSW_SWITCH;
}

bool nvgpu_engine_status_is_ctxsw_load(struct nvgpu_engine_status_info
		*engine_status)
{
	return engine_status->ctxsw_status == NVGPU_CTX_STATUS_CTXSW_LOAD;
}

bool nvgpu_engine_status_is_ctxsw_save(struct nvgpu_engine_status_info
		*engine_status)
{
	return	engine_status->ctxsw_status == NVGPU_CTX_STATUS_CTXSW_SAVE;
}

bool nvgpu_engine_status_is_ctxsw(struct nvgpu_engine_status_info
		*engine_status)
{
	return (nvgpu_engine_status_is_ctxsw_switch(engine_status) ||
		nvgpu_engine_status_is_ctxsw_load(engine_status) ||
		nvgpu_engine_status_is_ctxsw_save(engine_status));
}

bool nvgpu_engine_status_is_ctxsw_invalid(struct nvgpu_engine_status_info
		*engine_status)
{
	return engine_status->ctxsw_status == NVGPU_CTX_STATUS_INVALID;
}

bool nvgpu_engine_status_is_ctxsw_valid(struct nvgpu_engine_status_info
		*engine_status)
{
	return engine_status->ctxsw_status == NVGPU_CTX_STATUS_VALID;
}
bool nvgpu_engine_status_is_ctx_type_tsg(struct nvgpu_engine_status_info
		*engine_status)
{
	return engine_status->ctx_id_type == ENGINE_STATUS_CTX_ID_TYPE_TSGID;
}
bool nvgpu_engine_status_is_next_ctx_type_tsg(struct nvgpu_engine_status_info
		*engine_status)
{
	return engine_status->ctx_next_id_type ==
		ENGINE_STATUS_CTX_NEXT_ID_TYPE_TSGID;
}

void nvgpu_engine_status_get_ctx_id_type(struct nvgpu_engine_status_info
		*engine_status, u32 *ctx_id, u32 *ctx_type)
{
	*ctx_id = engine_status->ctx_id;
	*ctx_type = engine_status->ctx_id_type;
}

void nvgpu_engine_status_get_next_ctx_id_type(struct nvgpu_engine_status_info
		*engine_status, u32 *ctx_next_id,
		u32 *ctx_next_type)
{
	*ctx_next_id = engine_status->ctx_next_id;
	*ctx_next_type = engine_status->ctx_next_id_type;
}
