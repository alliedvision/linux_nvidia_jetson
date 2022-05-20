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
#ifndef UNIT_NVGPU_GR_CTX_H
#define UNIT_NVGPU_GR_CTX_H

#include <nvgpu/types.h>

struct gk20a;
struct unit_module;

/** @addtogroup SWUTS-gr-ctx
 *  @{
 *
 * Software Unit Test Specification for common.gr.ctx
 */

/**
 * Test specification for: test_gr_ctx_error_injection.
 *
 * Description: Verify error handling in context allocation and mapping path.
 *
 * Test Type: Feature, Error guessing
 *
 * Targets: #nvgpu_gr_ctx_alloc,
 *          #nvgpu_gr_ctx_free,
 *          #nvgpu_gr_ctx_desc_alloc,
 *          #nvgpu_gr_ctx_desc_free,
 *          #nvgpu_alloc_gr_ctx_struct,
 *          #nvgpu_free_gr_ctx_struct,
 *          #nvgpu_gr_ctx_set_size,
 *          #nvgpu_gr_ctx_alloc_patch_ctx,
 *          #nvgpu_gr_ctx_free_patch_ctx,
 *          #nvgpu_gr_ctx_map_global_ctx_buffers,
 *          #nvgpu_gr_ctx_patch_write_begin,
 *          #nvgpu_gr_ctx_patch_write,
 *          #nvgpu_gr_ctx_patch_write_end.
 *
 * Input: gr_ctx_setup must have been executed successfully.
 *
 * Steps:
 * - Allocate context descriptor struct.
 * - Try to free gr_ctx before it is allocated, should fail.
 * - Try to allocate gr_ctx before size is set, should fail.
 * - Inject dma allocation failure and try to allocate gr_ctx, should fail.
 * - Inject kmem allocation failure and try to allocate gr_ctx, should fail.
 * - Disable error injection and allocate gr_ctx, should pass.
 * - Try to free patch_ctx before it is allocated, should fail.
 * - Inject dma allocation failure and try to allocate patch_ctx, should fail.
 * - Disable error injection and allocate patch_ctx, should pass.
 * - Setup all the global context buffers.
 * - Inject kmem allocation failures for each global context buffer mappping,
 *   should fail.
 * - Disable error injection and map, should pass.
 * - Increase data count in patch context beyond max, write should fail.
 * - Set data count to 0, write should pass.
 * - Trigger patch write with NULL context pointer. Should fail. But since
 *   we don't have any API to read contents of Patch buffer, can't be
 *   verified yet.
 * - Cleanup all the local resources.
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_gr_ctx_error_injection(struct unit_module *m,
		struct gk20a *g, void *args);

#endif /* UNIT_NVGPU_GR_CTX_H */

/**
 * @}
 */

