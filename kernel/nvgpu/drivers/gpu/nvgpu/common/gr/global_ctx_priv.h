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

#ifndef NVGPU_GR_GLOBAL_CTX_PRIV_H
#define NVGPU_GR_GLOBAL_CTX_PRIV_H

/**
 * Global context buffer descriptor structure.
 *
 * This structure stores properties applicable to each global
 * context buffer.
 */
struct nvgpu_gr_global_ctx_buffer_desc {
	/**
	 * Memory to hold global context buffer.
	 */
	struct nvgpu_mem mem;

	/**
	 * Size of global context buffer.
	 */
	size_t size;

	/**
	 * Function pointer to free global context buffer.
	 */
	global_ctx_mem_destroy_fn destroy;
};

/**
 * Local Golden context image descriptor structure.
 *
 * This structure stores details of a local Golden context image.
 * Pointer to this struct is maintained in
 * #nvgpu_gr_obj_ctx_golden_image structure.
 */
struct nvgpu_gr_global_ctx_local_golden_image {
	/**
	 * Pointer to local Golden context image memory.
	 */
	u32 *context;

	/**
	 * Size of local Golden context image.
	 */
	size_t size;
};

#endif /* NVGPU_GR_GLOBAL_CTX_PRIV_H */
