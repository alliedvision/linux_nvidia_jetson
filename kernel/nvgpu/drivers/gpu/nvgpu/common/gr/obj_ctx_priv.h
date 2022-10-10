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

#ifndef NVGPU_GR_OBJ_CTX_PRIV_H
#define NVGPU_GR_OBJ_CTX_PRIV_H

#include <nvgpu/types.h>
#include <nvgpu/lock.h>

struct nvgpu_gr_global_ctx_local_golden_image;

/**
 * Graphics specific context register values structure.
 *
 * This structure stores init values for some of the registers that need to be
 * configured differently for Graphics contexts.
 */
struct nvgpu_gr_obj_ctx_gfx_regs {
	u32 reg_sm_disp_ctrl;
	u32 reg_gpcs_setup_debug;
	u32 reg_tex_lod_dbg;
	u32 reg_hww_warp_esr_report_mask;
};

/**
 * Golden context image descriptor structure.
 *
 * This structure stores details of the Golden context image.
 */
struct nvgpu_gr_obj_ctx_golden_image {
	/**
	 * Flag to indicate if Golden context image is ready or not.
	 */
	bool ready;

	/**
	 * Mutex to hold for accesses to Golden context image.
	 */
	struct nvgpu_mutex ctx_mutex;

	/**
	 * Size of Golden context image.
	 */
	size_t size;

	/**
	 * Pointer to local Golden context image struct.
	 */
	struct nvgpu_gr_global_ctx_local_golden_image *local_golden_image;

	/**
	 * Init values for graphics specific registers.
	 */
	struct nvgpu_gr_obj_ctx_gfx_regs gfx_regs;

#ifdef CONFIG_NVGPU_GR_GOLDEN_CTX_VERIFICATION
	/**
	 * Pointer to local Golden context image struct used for Golden
	 * context verification.
	 */
	struct nvgpu_gr_global_ctx_local_golden_image *local_golden_image_copy;
#endif
};

#endif /* NVGPU_GR_OBJ_CTX_PRIV_H */
