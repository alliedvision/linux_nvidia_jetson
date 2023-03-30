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

#ifndef NVGPU_GR_SUBCTX_H
#define NVGPU_GR_SUBCTX_H

#include <nvgpu/types.h>

/**
 * @file
 *
 * common.gr.subctx unit interface
 */
struct gk20a;
struct vm_gk20a;
struct nvgpu_gr_subctx;
struct nvgpu_mem;

/**
 * @brief Allocate graphics subcontext buffer.
 *
 * @param g [in]		Pointer to GPU driver struct.
 * @param vm [in]		Pointer to virtual memory.
 *
 * This function allocates memory for #nvgpu_gr_subctx structure
 * and subcontext header stored in #nvgpu_gr_subctx structure.
 *
 * Subcontext header memory will be mapped to given virtual
 * memory.
 *
 * @return pointer to #nvgpu_gr_subctx struct in case of success,
 *         NULL in case of failure.
 */
struct nvgpu_gr_subctx *nvgpu_gr_subctx_alloc(struct gk20a *g,
	struct vm_gk20a *vm);

/**
 * @brief Free graphics subcontext buffer.
 *
 * @param g [in]		Pointer to GPU driver struct.
 * @param subctx [in]		Pointer to graphics subcontext struct.
 * @param vm [in]		Pointer to virtual memory.
 *
 * This function will free memory allocated for subcontext header and
 * #nvgpu_gr_subctx structure.
 */
void nvgpu_gr_subctx_free(struct gk20a *g,
	struct nvgpu_gr_subctx *subctx,
	struct vm_gk20a *vm);

/**
 * @brief Initialize graphics subcontext buffer header.
 *
 * @param g [in]		Pointer to GPU driver struct.
 * @param subctx [in]		Pointer to graphics subcontext struct.
 * @param gr_ctx [in]		Pointer to graphics context struct.
 * @param gpu_va [in]		GPU virtual address of graphics context buffer.
 *
 * This function will initialize graphics subcontext buffer header
 * by reading appropriate values from #nvgpu_gr_ctx structure and
 * then writing them into subcontext buffer header.
 *
 * This function will also program GPU virtual address of graphics
 * context buffer into subcontext buffer header.
 */
void nvgpu_gr_subctx_load_ctx_header(struct gk20a *g,
	struct nvgpu_gr_subctx *subctx,
	struct nvgpu_gr_ctx *gr_ctx, u64 gpu_va);

/**
 * @brief Get pointer of subcontext header memory struct.
 *
 * @param subctx [in]		Pointer to graphics subcontext struct.
 *
 * This function returns #nvgpu_mem pointer of subcontext header stored
 * in #nvgpu_gr_subctx.
 *
 * @return pointer to subcontext header memory struct.
 */
struct nvgpu_mem *nvgpu_gr_subctx_get_ctx_header(struct nvgpu_gr_subctx *subctx);

#ifdef CONFIG_NVGPU_HAL_NON_FUSA
/**
 * @brief Set patch context buffer address in subcontext header.
 *
 * @param g [in]		Pointer to GPU driver struct.
 * @param subctx [in]		Pointer to graphics subcontext struct.
 * @param gr_ctx [in]		Pointer to graphics context struct.
 *
 * This function will program GPU virtual address of patch context buffer
 * into subcontext buffer header.
 */
void nvgpu_gr_subctx_set_patch_ctx(struct gk20a *g,
	struct nvgpu_gr_subctx *subctx, struct nvgpu_gr_ctx *gr_ctx);
#endif

#ifdef CONFIG_NVGPU_GRAPHICS
void nvgpu_gr_subctx_zcull_setup(struct gk20a *g, struct nvgpu_gr_subctx *subctx,
		struct nvgpu_gr_ctx *gr_ctx);

void nvgpu_gr_subctx_set_preemption_buffer_va(struct gk20a *g,
	struct nvgpu_gr_subctx *subctx, struct nvgpu_gr_ctx *gr_ctx);
#endif

#ifdef CONFIG_NVGPU_DEBUGGER
void nvgpu_gr_subctx_set_hwpm_mode(struct gk20a *g,
	struct nvgpu_gr_subctx *subctx, struct nvgpu_gr_ctx *gr_ctx);
#endif
#endif /* NVGPU_GR_SUBCTX_H */
