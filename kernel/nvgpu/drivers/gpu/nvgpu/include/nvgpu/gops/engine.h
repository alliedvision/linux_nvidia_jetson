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
#ifndef NVGPU_GOPS_ENGINE_H
#define NVGPU_GOPS_ENGINE_H

#include <nvgpu/types.h>

/**
 * @file
 *
 * Engine HAL interface.
 */
struct gk20a;
struct nvgpu_engine_status_info;
struct nvgpu_debug_context;

/**
 * Engine status HAL operations.
 *
 * @see gpu_ops
 */
struct gops_engine_status {
	/**
	 * @brief Read engine status info.
	 *
	 * @param g [in]		Pointer to GPU driver struct.
	 * @param engine_id [in]	H/w engine id.
	 * @param status [in,out]	Pointer to struct abstracting engine
	 * 				status.
	 *
	 * Read engine status information from GPU h/w, and:
	 * - Determine if engine is busy.
	 * - Determine if engine if faulted.
	 * - Determine current context status
	 *   (see #nvgpu_engine_status_ctx_status).
	 * - Determine current context id and type. Valid when status is
	 *   #NVGPU_CTX_STATUS_VALID, #NVGPU_CTX_STATUS_CTXSW_SAVE or
	 *   #NVGPU_CTX_STATUS_CTXSW_SWITCH.
	 * - Determine next context id and type. Valid when status is
	 *   #NVGPU_CTX_STATUS_CTXSW_LOAD or #NVGPU_CTX_STATUS_CTXSW_SWITCH.
	 *
	 * @see nvgpu_engine_status_info
	 * @see nvgpu_engine_status_ctx_status
	 * @see nvgpu_engine_get_ids
	 */
	void (*read_engine_status_info)(struct gk20a *g,
		u32 engine_id, struct nvgpu_engine_status_info *status);

/** @cond DOXYGEN_SHOULD_SKIP_THIS */
	void (*dump_engine_status)(struct gk20a *g,
			struct nvgpu_debug_context *o);
/** @endcond DOXYGEN_SHOULD_SKIP_THIS */
};

/** @cond DOXYGEN_SHOULD_SKIP_THIS */
struct gops_engine {

	bool (*is_fault_engine_subid_gpc)(struct gk20a *g,
				 u32 engine_subid);
	int (*init_ce_info)(struct nvgpu_fifo *f);
};
/** @endcond DOXYGEN_SHOULD_SKIP_THIS */

#endif
