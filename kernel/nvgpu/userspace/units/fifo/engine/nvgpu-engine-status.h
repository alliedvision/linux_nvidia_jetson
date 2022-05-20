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


#ifndef UNIT_NVGPU_ENGINE_STATUS_H
#define UNIT_NVGPU_ENGINE_STATUS_H

#include <nvgpu/types.h>

struct unit_module;
struct gk20a;

/** @addtogroup SWUTS-fifo-engine
 *  @{
 *
 * Software Unit Test Specification for fifo/engine
 */

/**
 * Test specification for: test_engine_status
 *
 * Description: Engine status helper functions
 *
 * Test Type: Feature based
 *
 * Targets: nvgpu_engine_status_is_ctxsw_switch,
 *	nvgpu_engine_status_is_ctxsw_load,
 *	nvgpu_engine_status_is_ctxsw_save,
 *	nvgpu_engine_status_is_ctxsw,
 *	nvgpu_engine_status_is_ctxsw_invalid,
 *	nvgpu_engine_status_is_ctxsw_valid,
 *	nvgpu_engine_status_is_ctx_type_tsg,
 *	nvgpu_engine_status_is_next_ctx_type_tsg,
 *	nvgpu_engine_status_get_ctx_id_type,
 *	nvgpu_engine_status_get_next_ctx_id_type
 *
 * Input: None
 *
 * Steps:
 * - Initialize ctxsw_status field of nvgpu_engine_status_info structure with
 *   with NVGPU_CTX_STATUS_INVALID, NVGPU_CTX_STATUS_VALID,
 *   NVGPU_CTX_STATUS_CTXSW_LOAD, NVGPU_CTX_STATUS_CTXSW_SAVE,
 *   NVGPU_CTX_STATUS_CTXSW_SWITCH, and U32(~0).
 * - Check that nvgpu_engine_status_is_ctxsw_load,
 *   nvgpu_engine_status_is_ctxsw_save, nvgpu_engine_status_is_ctxsw,
 *   nvgpu_engine_status_is_ctxsw_invalid, nvgpu_engine_status_is_ctxsw_valid,
 *   return consistent values.
 * - Initialize ctx_id with a counter and ctx_id_types successively with
 *   ENGINE_STATUS_CTX_ID_TYPE_CHID, ENGINE_STATUS_CTX_ID_TYPE_TSGID, and
 *   ENGINE_STATUS_CTX_ID_TYPE_INVALID.
 * - Initialize next_ctx_id and next_ctx_id_types with invalid values
 *   (to make sure accessors use the right fields).
 * - Check that nvgpu_engine_status_is_ctx_type_tsg and
 *   nvgpu_engine_status_get_ctx_id_type return consitent values.
 * - Use same method to check nvgpu_engine_status_is_next_ctx_type_tsg and
 *   nvgpu_engine_status_get_next_ctx_id_type.
 *
 * Output: Returns PASS if all branches gave expected results. FAIL otherwise.
 */
int test_engine_status(struct unit_module *m,
		struct gk20a *g, void *args);

/**
 * @}
 */

#endif /* UNIT_NVGPU_ENGINE_STATUS_H */
