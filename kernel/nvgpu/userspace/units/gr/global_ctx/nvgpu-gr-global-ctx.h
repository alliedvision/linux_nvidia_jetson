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
#ifndef UNIT_NVGPU_GR_GLOBAL_CTX_H
#define UNIT_NVGPU_GR_GLOBAL_CTX_H

#include <nvgpu/types.h>

struct gk20a;
struct unit_module;

/** @addtogroup SWUTS-gr-global-ctx
 *  @{
 *
 * Software Unit Test Specification for common.gr.global_ctx
 */

/**
 * Test specification for: test_gr_global_ctx_alloc_error_injection.
 *
 * Description: Verify error handling in global context allocation path.
 *
 * Test Type: Feature, Error guessing
 *
 * Targets: nvgpu_gr_global_ctx_buffer_alloc,
 *          nvgpu_gr_global_ctx_buffer_free,
 *          nvgpu_gr_global_ctx_desc_alloc,
 *          nvgpu_gr_global_ctx_desc_free,
 *          nvgpu_gr_global_ctx_set_size,
 *          nvgpu_gr_global_ctx_buffer_map,
 *          nvgpu_gr_global_ctx_buffer_unmap,
 *          nvgpu_gr_global_ctx_buffer_get_mem,
 *          nvgpu_gr_global_ctx_buffer_ready
 *
 * Input: gr_global_ctx_setup must have been executed successfully.
 *
 * Steps:
 * - Trigger nvgpu_gr_global_ctx_buffer_alloc() to allocate global context
 *   buffers before sizes of all the buffers are set. This step should fail.
 * - Trigger map/unmap calls for global context buffers before buffers are
 *   allocated. This step should fail.
 * - Check if valid memory handle is returned or "ready" status is returned
 *   before context buffers are allocated. This step should fail.
 * - Inject dma allocation errors for each context buffer and ensure
 *   nvgpu_gr_global_ctx_buffer_alloc() returns error in each case.
 * - Ensure nvgpu_gr_global_ctx_buffer_alloc() is successful after all sizes
 *   are set and no error is injected.
 * - Call nvgpu_gr_global_ctx_buffer_alloc() one more time to ensure API
 *   does not fail for double allocation.
 * - Cleanup all the local resources.
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_gr_global_ctx_alloc_error_injection(struct unit_module *m,
		struct gk20a *g, void *args);

/**
 * Test specification for: test_gr_global_ctx_local_ctx_error_injection.
 *
 * Description: Verify error handling in local golden context image
 *              creation and comparison.
 *
 * Test Type: Feature, Error guessing
 *
 * Targets: nvgpu_gr_global_ctx_alloc_local_golden_image,
 *          nvgpu_gr_global_ctx_init_local_golden_image,
 *          nvgpu_gr_global_ctx_load_local_golden_image,
 *          nvgpu_gr_global_ctx_compare_golden_images,
 *          nvgpu_gr_global_ctx_deinit_local_golden_image
 *
 * Input: gr_global_ctx_setup must have been executed successfully.
 *
 * Steps:
 * - Allocate a dummy buffer for local use.
 * - Inject memory allocation failures and ensure
 *   #nvgpu_gr_global_ctx_init_local_golden_image returns error in
 *   each case.
 * - Trigger #nvgpu_gr_global_ctx_init_local_golden_image without any
 *   error injection and ensure it returns success.
 * - Trigger memory flush errors and execute
 *   #nvgpu_gr_global_ctx_load_local_golden_image to cover error
 *   handling code.
 * - Allocate another dummy local golden context image and compare
 *   the contents. This step should pass.
 * - Trigger the comparison with vidmem flag set to true, this step
 *   should fail.
 * - Change the contents of dummy local context image and compare.
 *   This step should fail.
 * - Cleanup all the local resources.
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_gr_global_ctx_local_ctx_error_injection(struct unit_module *m,
		struct gk20a *g, void *args);

#endif /* UNIT_NVGPU_GR_GLOBAL_CTX_H */

/**
 * @}
 */

