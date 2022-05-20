/*
 * Copyright (c) 2019-2021, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef NVGPU_GR_OBJ_CTX_H
#define NVGPU_GR_OBJ_CTX_H

#include <nvgpu/types.h>
#include <nvgpu/lock.h>

/**
 * @file
 *
 * common.gr.obj_ctx unit interface
 */
struct gk20a;
struct nvgpu_gr_ctx;
struct nvgpu_gr_subctx;
struct nvgpu_gr_config;
struct nvgpu_gr_ctx_desc;
struct vm_gk20a;
struct nvgpu_gr_global_ctx_buffer_desc;
struct nvgpu_mem;
struct nvgpu_channel;
struct nvgpu_gr_obj_ctx_golden_image;

#ifdef CONFIG_NVGPU_GRAPHICS
#define NVGPU_OBJ_CTX_FLAGS_SUPPORT_GFXP		BIT32(1)
#endif
#ifdef CONFIG_NVGPU_CILP
#define NVGPU_OBJ_CTX_FLAGS_SUPPORT_CILP		BIT32(2)
#endif

/**
 * @brief Commit context buffer address in instance block.
 *
 * @param g [in]		Pointer to GPU driver struct.
 * @param inst_block [in]	Pointer to channel instance block.
 * @param gpu_va [in]		GPU virtual address to be committed.
 *
 * This function will commit given GPU virtual address into given
 * channel instance block. Appropriate address is selected by
 * #nvgpu_gr_obj_ctx_commit_inst() function.
 */
void nvgpu_gr_obj_ctx_commit_inst_gpu_va(struct gk20a *g,
	struct nvgpu_mem *inst_block, u64 gpu_va);

/**
 * @brief Commit context buffer in instance block.
 *
 * @param g [in]		Pointer to GPU driver struct.
 * @param inst_block [in]	Pointer to channel instance block.
 * @param gr_ctx [in]		Pointer to graphics context buffer.
 * @param subctx [in]		Pointer to graphics subcontext buffer.
 * @param gpu_va [in]		GPU virtual address of graphics context buffer.
 *
 * If graphics subcontexts are supported, subcontext buffer GPU virtual
 * address should be committed to channel instance block. Otherwise graphics
 * context GPU virtual address should be committed.
 *
 * This function will select appropriate address and pass it to
 * #nvgpu_gr_obj_ctx_commit_inst_gpu_va() to commit into channel
 * instance block.
 */
void nvgpu_gr_obj_ctx_commit_inst(struct gk20a *g, struct nvgpu_mem *inst_block,
	struct nvgpu_gr_ctx *gr_ctx, struct nvgpu_gr_subctx *subctx,
	u64 gpu_va);

/**
 * brief Initialize preemption mode in context struct.
 *
 * @param g [in]			Pointer to GPU driver struct.
 * @param config [in]			Pointer to GR configuration struct.
 * @param gr_ctx_desc [in]		Pointer to GR context descriptor struct.
 * @param gr_ctx [in]			Pointer to graphics context.
 * @param vm [in]			Pointer to virtual memory.
 * @param class_num [in]		GR engine class.
 * @param graphics_preempt_mode		Graphics preemption mode to set.
 * @param compute_preempt_mode		Compute preemption mode to set.
 *
 * This function will validate requested class and graphics/compute
 * preemption modes. If valid modes are provided, this function will
 * initialize the preemption mode values in #nvgpu_gr_ctx struct.
 *
 * Preemption modes are finally written into context image by
 * calling #nvgpu_gr_obj_ctx_update_ctxsw_preemption_mode().
 *
 * @return 0 in case of success, < 0 in case of failure.
 * @retval -EINVAL if invalid class is provided.
 * @retval -EINVAL if invalid preemption modes are provided.
 *
 * @see nvgpu_gr_setup_set_preemption_mode.
 */
int nvgpu_gr_obj_ctx_set_ctxsw_preemption_mode(struct gk20a *g,
	struct nvgpu_gr_config *config, struct nvgpu_gr_ctx_desc *gr_ctx_desc,
	struct nvgpu_gr_ctx *gr_ctx, struct vm_gk20a *vm, u32 class_num,
	u32 graphics_preempt_mode, u32 compute_preempt_mode);

/**
 * brief Update preemption mode in graphics context buffer.
 *
 * @param g [in]		Pointer to GPU driver struct.
 * @param config [in]		Pointer to GR configuration struct.
 * @param gr_ctx [in]		Pointer to graphics context.
 * @param subctx [in]		Pointer to graphics subcontext buffer.
 *
 * This function will read preemption modes stored in #nvgpu_gr_ctx
 * struct and write them into graphics context image.
 *
 * Preemption modes should be already initialized in #nvgpu_gr_ctx
 * struct by calling #nvgpu_gr_obj_ctx_set_ctxsw_preemption_mode()
 * before calling this function.
 *
 * @see nvgpu_gr_setup_set_preemption_mode.
 */
void nvgpu_gr_obj_ctx_update_ctxsw_preemption_mode(struct gk20a *g,
	struct nvgpu_gr_config *config,
	struct nvgpu_gr_ctx *gr_ctx, struct nvgpu_gr_subctx *subctx);

/**
 * brief Update global context buffer addresses in graphics context.
 *
 * @param g [in]			Pointer to GPU driver struct.
 * @param global_ctx_buffer [in]	Pointer to global context descriptor struct.
 * @param config [in]			Pointer to GR configuration struct.
 * @param gr_ctx [in]			Pointer to graphics context.
 * @param patch [in]			Boolean flag to use patch context buffer.
 *
 * This function will update GPU virtual addresses of global context
 * buffers in given graphics context image.
 *
 * If flag #patch is set, patch context image is used to update the
 * graphics context, otherwise updates are done with register writes.
 */
void nvgpu_gr_obj_ctx_commit_global_ctx_buffers(struct gk20a *g,
	struct nvgpu_gr_global_ctx_buffer_desc *global_ctx_buffer,
	struct nvgpu_gr_config *config,	struct nvgpu_gr_ctx *gr_ctx, bool patch);

/**
 * @brief Allocate golden context image.
 *
 * @param g [in]			Pointer to GPU driver struct.
 * @param golden_image [in]		Pointer to golden context image struct.
 * @param global_ctx_buffer [in]	Pointer to global context descriptor struct.
 * @param config [in]			Pointer to GR configuration struct.
 * @param gr_ctx [in]			Pointer to graphics context.
 * @param inst_block [in]		Pointer to channel instance block.
 *
 * This function allocates golden context image.
 *
 * Golden context image is typically created only once while creating
 * first ever graphics context image. Golden context image is saved
 * after explicitly initializing first graphics context image.
 *
 * All subsequent graphics context images are initialized by loading
 * golden context image instead of explicit initialization.
 *
 * Golden context is saved in a local image stored in
 * #nvgpu_gr_global_ctx_local_golden_image struct.
 *
 * This function will also capture the golden image twice to compare
 * the contents. Correct golden image is captured only if the contents
 * match from both the images. Redundant golden image is released
 * after the comparison.
 *
 * @return 0 in case of success, < 0 in case of failure.
 * @retval -ENOMEM if local golden context memory allocation fails.
 * @retval -ENOMEM if contents from two golden image captures do not
 *         match.
 *
 * @see nvgpu_gr_setup_alloc_obj_ctx.
 */
int nvgpu_gr_obj_ctx_alloc_golden_ctx_image(struct gk20a *g,
	struct nvgpu_gr_obj_ctx_golden_image *golden_image,
	struct nvgpu_gr_global_ctx_buffer_desc *global_ctx_buffer,
	struct nvgpu_gr_config *config,
	struct nvgpu_gr_ctx *gr_ctx,
	struct nvgpu_mem *inst_block);

/**
 * @brief Allocate object context.
 *
 * @param g [in]			Pointer to GPU driver struct.
 * @param golden_image [in]		Pointer to golden context image struct.
 * @param global_ctx_buffer [in]	Pointer to global context descriptor struct.
 * @param gr_ctx_desc [in]		Pointer to GR context descriptor struct.
 * @param config [in]			Pointer to GR configuration struct.
 * @param gr_ctx [in]			Pointer to graphics context.
 * @param subctx [in]			Pointer to graphics subcontext buffer.
 * @param vm [in]			Pointer to virtual memory.
 * @param inst_block [in]		Pointer to channel instance block.
 * @param class_num [in]		GR engine class.
 * @param flags [in]			Object context attribute flags.
 * @param cde [in]			Boolean flag to enable/disable CDE.
 * @param vpr [in]			Boolean flag to use global context buffers
 *                                      allocated in VPR.
 *
 * This function allocates object context for the GPU channel.
 * Allocating object context includes:
 *
 * - Allocating graphics context buffer. See #nvgpu_gr_obj_ctx_gr_ctx_alloc().
 * - Allocating patch context buffer. See #nvgpu_gr_ctx_alloc_patch_ctx().
 * - Allocating golden context image. See #nvgpu_gr_obj_ctx_alloc_golden_ctx_image().
 * - Committing global context buffers in graphics context image.
 *   See #nvgpu_gr_obj_ctx_commit_global_ctx_buffers().
 * - Initializing and updating graphics/compute preemption modes in graphics
 *   context image. See #nvgpu_gr_obj_ctx_set_ctxsw_preemption_mode() and
 *   #nvgpu_gr_obj_ctx_update_ctxsw_preemption_mode().
 * - Committing graphics context image in channel instance block.
 *   See #nvgpu_gr_obj_ctx_commit_inst().
 * - Setting golden context image status to ready in
 *   #nvgpu_gr_obj_ctx_golden_image struct if all above steps are
 *   successful.
 *
 * @return 0 in case of success, < 0 in case of failure.
 * @retval -ENOMEM if memory allocation fails during any step.
 * @retval -ENOMEM if contents from two golden image captures do not match.
 * @retval -ETIMEDOUT if GR engine idle times out.
 * @retval -EINVAL if invalid GPU class ID is provided.
 *
 * @see nvgpu_gr_setup_alloc_obj_ctx.
 */
int nvgpu_gr_obj_ctx_alloc(struct gk20a *g,
	struct nvgpu_gr_obj_ctx_golden_image *golden_image,
	struct nvgpu_gr_global_ctx_buffer_desc *global_ctx_buffer,
	struct nvgpu_gr_ctx_desc *gr_ctx_desc,
	struct nvgpu_gr_config *config,
	struct nvgpu_gr_ctx *gr_ctx,
	struct nvgpu_gr_subctx *subctx,
	struct vm_gk20a *vm,
	struct nvgpu_mem *inst_block,
	u32 class_num, u32 flags,
	bool cde, bool vpr);

/**
 * @brief Set golden context image size.
 *
 * @param golden_image [in]	Pointer to golden context image struct.
 * @param size [in]		Size to be set.
 *
 * This function sets given size in #nvgpu_gr_obj_ctx_golden_image
 * struct.
 */
void nvgpu_gr_obj_ctx_set_golden_image_size(
		struct nvgpu_gr_obj_ctx_golden_image *golden_image,
		size_t size);

/**
 * @brief Get golden context image size.
 *
 * @param golden_image [in]	Pointer to golden context image struct.
 *
 * This function returns size of golden context image stored in
 * #nvgpu_gr_obj_ctx_golden_image struct.
 *
 * @return Size of golden context image.
 */
size_t nvgpu_gr_obj_ctx_get_golden_image_size(
		struct nvgpu_gr_obj_ctx_golden_image *golden_image);

/**
 * @brief Check if golden context image is ready.
 *
 * @param golden_image [in]	Pointer to golden context image struct.
 *
 * This function checks if golden context image has been allocated
 * and initialized.
 *
 * A flag to indicate golden context image readiness is set in
 * #nvgpu_gr_obj_ctx_golden_image struct if it is created successfully.
 * See #nvgpu_gr_obj_ctx_alloc().
 *
 * @return true if golden context image is ready, false otherwise.
 */
bool nvgpu_gr_obj_ctx_is_golden_image_ready(
	struct nvgpu_gr_obj_ctx_golden_image *golden_image);

/**
 * @brief Initialize object context.
 *
 * @param gr_golden_image [in]	Pointer to golden context image struct.
 * @param size [in]		Size to be set.
 *
 * This function allocates memory for #nvgpu_gr_obj_ctx_golden_image
 * struct and sets given size in it.
 *
 * @return 0 in case of success, < 0 in case of failure.
 * @retval -ENOMEM if struct memory allocation fails.
 */
int nvgpu_gr_obj_ctx_init(struct gk20a *g,
	struct nvgpu_gr_obj_ctx_golden_image **gr_golden_image, u32 size);

/**
 * @brief Deinitialize object context.
 *
 * @param golden_image [in]	Pointer to golden context image struct.
 *
 * This function will free memory allocated for local golden context
 * image and also for #nvgpu_gr_obj_ctx_golden_image struct.
 */
void nvgpu_gr_obj_ctx_deinit(struct gk20a *g,
	struct nvgpu_gr_obj_ctx_golden_image *golden_image);

#ifdef CONFIG_NVGPU_DEBUGGER
u32 *nvgpu_gr_obj_ctx_get_local_golden_image_ptr(
	struct nvgpu_gr_obj_ctx_golden_image *golden_image);
#endif

#endif /* NVGPU_GR_OBJ_CTX_H */
