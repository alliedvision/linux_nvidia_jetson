/*
 * Copyright (c) 2018-2021, NVIDIA CORPORATION.  All rights reserved.
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
#ifndef NVGPU_GR_GLOBAL_CTX_H
#define NVGPU_GR_GLOBAL_CTX_H

#include <nvgpu/nvgpu_mem.h>
#include <nvgpu/static_analysis.h>

/**
 * @file
 *
 * common.gr.global_ctx unit interface
 */
struct gk20a;
struct nvgpu_mem;
struct vm_gk20a;
struct nvgpu_gr_global_ctx_buffer_desc;
struct nvgpu_gr_global_ctx_local_golden_image;

typedef void (*global_ctx_mem_destroy_fn)(struct gk20a *g,
				struct nvgpu_mem *mem);

/**
 * Size of access map buffer for register accesses via firmware
 * methods.
 */
#define NVGPU_GR_GLOBAL_CTX_PRIV_ACCESS_MAP_SIZE \
			nvgpu_safe_mult_u32(512U, 1024U)

/** S/W defined index for circular global context buffer. */
#define NVGPU_GR_GLOBAL_CTX_CIRCULAR			0U
/** S/W defined index for pagepool global context buffer. */
#define NVGPU_GR_GLOBAL_CTX_PAGEPOOL			1U
/** S/W defined index for attribute global context buffer. */
#define NVGPU_GR_GLOBAL_CTX_ATTRIBUTE			2U

#ifdef CONFIG_NVGPU_VPR
/**
 * S/W defined index for circular global context buffer allocated
 * in VPR.
 */
#define NVGPU_GR_GLOBAL_CTX_CIRCULAR_VPR		3U
/**
 * S/W defined index for pagepool global context buffer allocated
 * in VPR.
 */
#define NVGPU_GR_GLOBAL_CTX_PAGEPOOL_VPR		4U
/**
 * S/W defined index for attribute global context buffer allocated
 * in VPR.
 */
#define NVGPU_GR_GLOBAL_CTX_ATTRIBUTE_VPR		5U
#endif

/**
 * S/W defined index for register access map buffer for register
 * accesses via firmware methods.
 */
#define NVGPU_GR_GLOBAL_CTX_PRIV_ACCESS_MAP		6U

/** S/W defined index for RTV circular global context buffer. */
#define NVGPU_GR_GLOBAL_CTX_RTV_CIRCULAR_BUFFER		7U

#ifdef CONFIG_NVGPU_FECS_TRACE
/** S/W defined index for global FECS trace buffer. */
#define NVGPU_GR_GLOBAL_CTX_FECS_TRACE_BUFFER		8U
#endif
/** Number of global context buffers. */
#define NVGPU_GR_GLOBAL_CTX_COUNT			9U

/**
 * @brief Initialize global context descriptor structure.
 *
 * @param g [in]	Pointer to GPU driver struct.
 *
 * This function allocates memory for #nvgpu_gr_global_ctx_buffer_desc
 * structure.
 *
 * @return pointer to #nvgpu_gr_global_ctx_buffer_desc struct in case
 *         of success, NULL in case of failure.
 */
struct nvgpu_gr_global_ctx_buffer_desc *nvgpu_gr_global_ctx_desc_alloc(
	struct gk20a *g);

/**
 * @brief Free global context descriptor structure.
 *
 * @param g [in]	Pointer to GPU driver struct.
 * @param desc [in]	Pointer to global context descriptor struct.
 *
 * This function will free memory allocated for
 * #nvgpu_gr_global_ctx_buffer_desc structure.
 */
void nvgpu_gr_global_ctx_desc_free(struct gk20a *g,
	struct nvgpu_gr_global_ctx_buffer_desc *desc);

/**
 * @brief Set size of global context buffer with given index.
 *
 * @param desc [in]	Pointer to global context descriptor struct.
 * @param index [in]	Index of global context buffer.
 * @param size [in]	Size of buffer to be set.
 *
 * This function sets size of global context buffer with given buffer
 * index. \a index must be less than NVGPU_GR_GLOBAL_CTX_COUNT
 * otherwise an assert is raised.
 */
void nvgpu_gr_global_ctx_set_size(struct nvgpu_gr_global_ctx_buffer_desc *desc,
	u32 index, size_t size);

/**
 * @brief Get size of global context buffer with given index.
 *
 * @param desc [in]	Pointer to global context descriptor struct.
 * @param index [in]	Index of global context buffer.
 *
 * This function will return size of global context buffer with given buffer
 * index.
 *
 * @return size of global context buffer.
 */
size_t nvgpu_gr_global_ctx_get_size(struct nvgpu_gr_global_ctx_buffer_desc *desc,
	u32 index);

/**
 * @brief Allocate all global context buffers.
 *
 * @param g [in]	Pointer to GPU driver struct.
 * @param desc [in]	Pointer to global context descriptor struct.
 *
 * This function allocates memory for all global context buffers.
 *
 * Sizes of all the global context buffers must be set by calling
 * #nvgpu_gr_global_ctx_set_size() function before calling this
 * function.
 *
 * @return 0 in case of success, < 0 in case of failure.
 * @retval -ENOMEM if context memory allocation fails.
 * @retval -EINVAL if context buffer size is not set in
 *         #nvgpu_gr_global_ctx_buffer_desc struct.
 */
int nvgpu_gr_global_ctx_buffer_alloc(struct gk20a *g,
	struct nvgpu_gr_global_ctx_buffer_desc *desc);

/**
 * @brief Free all global context buffers.
 *
 * @param g [in]	Pointer to GPU driver struct.
 * @param desc [in]	Pointer to global context descriptor struct.
 *
 * This function will free memory allocated for all global context
 * buffers.
 */
void nvgpu_gr_global_ctx_buffer_free(struct gk20a *g,
	struct nvgpu_gr_global_ctx_buffer_desc *desc);

/**
 * @brief Map given global context buffer.
 *
 * @param desc [in]	Pointer to global context descriptor struct.
 * @param index [in]	Index of global context buffer.
 * @param vm [in]	Pointer to virtual memory.
 * @param flags [in]	Flags used to specify mapping attributes.
 * @param priv [in]	Boolean flag to allocate privileged PTE.
 *
 * This function maps given global contex buffer with index #index into
 * given virtual memory.
 *
 * @return GPU virtual address of the global context buffer in case of success,
 *         0 in case of failure.
 */
u64 nvgpu_gr_global_ctx_buffer_map(struct nvgpu_gr_global_ctx_buffer_desc *desc,
	u32 index,
	struct vm_gk20a *vm, u32 flags, bool priv);

/**
 * @brief Unmap given global context buffer.
 *
 * @param desc [in]	Pointer to global context descriptor struct.
 * @param index [in]	Index of global context buffer.
 * @param vm [in]	Pointer to virtual memory.
 * @param gpu_va [in]	GPU virtual address to unmap.
 *
 * This function unmaps given global contex buffer with index #index from
 * given virtual memory.
 */
void nvgpu_gr_global_ctx_buffer_unmap(
	struct nvgpu_gr_global_ctx_buffer_desc *desc,
	u32 index,
	struct vm_gk20a *vm, u64 gpu_va);

/**
 * @brief Get pointer of global context buffer memory struct.
 *
 * @param desc [in]	Pointer to global context descriptor struct.
 * @param index [in]	Index of global context buffer.
 *
 * This function returns #nvgpu_mem pointer of global context buffer
 * stored in #nvgpu_gr_global_ctx_buffer_desc struct.
 *
 * @return pointer to global context buffer memory struct.
 */
struct nvgpu_mem *nvgpu_gr_global_ctx_buffer_get_mem(
	struct nvgpu_gr_global_ctx_buffer_desc *desc,
	u32 index);

/**
 * @brief Check if given global context buffer is ready.
 *
 * @param desc [in]	Pointer to global context descriptor struct.
 * @param index [in]	Index of global context buffer.
 *
 * This function checks if memory has been allocated for requested
 * global context buffer with index #index.
 *
 * @return true in case global context buffer is ready,
 *         false otherwise.
 */
bool nvgpu_gr_global_ctx_buffer_ready(
	struct nvgpu_gr_global_ctx_buffer_desc *desc,
	u32 index);

/**
 * @brief Allocate memory for local golden context image.
 *
 * @param g [in]			Pointer to GPU driver struct.
 * @param local_golden_image [in]	Pointer to local golden context image struct.
 * @param size [in]			Size of local golden context image.
 *
 * This function allocates memory to store local golden context image
 * and also for #nvgpu_gr_global_ctx_local_golden_image structure.
 *
 * @return 0 in case of success, < 0 in case of failure.
 * @retval -ENOMEM if local golden image memory allocation fails.
 */
int nvgpu_gr_global_ctx_alloc_local_golden_image(struct gk20a *g,
		struct nvgpu_gr_global_ctx_local_golden_image **img,
		size_t size);

/**
 * @brief Initialize local golden context image.
 *
 * @param g [in]			Pointer to GPU driver struct.
 * @param local_golden_image [in]	Pointer to local golden image to be initialized.
 * @param source_mem [in]		Pointer to source memory.
 * @param size [in]			Size of local golden context image.
 *
 * This function will then initialize local golden context image by
 * copying contents of #source_mem into newly created image.
 *
 * This operation is typically needed only once while creating first
 * ever graphics context image for any channel. Subsequent graphics
 * context allocations will re-use this local golden image to
 * initialize. See #nvgpu_gr_global_ctx_load_local_golden_image.
 */
void nvgpu_gr_global_ctx_init_local_golden_image(struct gk20a *g,
	struct nvgpu_gr_global_ctx_local_golden_image *local_golden_image,
	struct nvgpu_mem *source_mem, size_t size);

/**
 * @brief Load local golden context image into target memory.
 *
 * @param g [in]			Pointer to GPU driver struct.
 * @param local_golden_image [in]	Pointer to local golden context image struct.
 * @param target_mem [in]		Pointer to target memory.
 *
 * This function copies contents of local golden context image to
 * given target memory. Target memory is usually a new graphics context
 * image that needs to be initialized using local golden image.
 */
void nvgpu_gr_global_ctx_load_local_golden_image(struct gk20a *g,
	struct nvgpu_gr_global_ctx_local_golden_image *local_golden_image,
	struct nvgpu_mem *target_mem);

/**
 * @brief Deinit local golden context image.
 *
 * @param local_golden_image [in]	Pointer to local golden context image struct.
 *
 * This function will free memory allocated for local golden context
 * image and for #nvgpu_gr_global_ctx_local_golden_image struct.
 */
void nvgpu_gr_global_ctx_deinit_local_golden_image(struct gk20a *g,
	struct nvgpu_gr_global_ctx_local_golden_image *local_golden_image);

#ifdef CONFIG_NVGPU_GR_GOLDEN_CTX_VERIFICATION
/**
 * @brief Compare two local golden images.
 *
 * @param g [in]			Pointer to GPU driver struct.
 * @param is_sysmem [in]		Boolean flag to indicate images are in sysmem.
 * @param local_golden_image1[in]	Pointer to first local golden context image struct.
 * @param local_golden_image2[in]	Pointer to second local golden context image struct.
 * @param size [in]			Size of local golden context image.
 *
 * This function compares contents of two local golden images and
 * returns true only if they match.
 *
 * This is typically needed to verify if local golden context image
 * has been captured correctly or not. To verify, common.gr unit
 * will capture the golden image twice and compare.
 *
 * @return true in case both the golden images match, false otherwise.
 */
bool nvgpu_gr_global_ctx_compare_golden_images(struct gk20a *g,
	bool is_sysmem,
	struct nvgpu_gr_global_ctx_local_golden_image *local_golden_image1,
	struct nvgpu_gr_global_ctx_local_golden_image *local_golden_image2,
	size_t size);
#endif

#ifdef NVGPU_UNITTEST_FAULT_INJECTION_ENABLEMENT
struct nvgpu_posix_fault_inj *nvgpu_golden_ctx_verif_get_fault_injection(void);
struct nvgpu_posix_fault_inj *nvgpu_local_golden_image_get_fault_injection(void);
#endif

#ifdef CONFIG_NVGPU_DEBUGGER
u32 *nvgpu_gr_global_ctx_get_local_golden_image_ptr(
	struct nvgpu_gr_global_ctx_local_golden_image *local_golden_image);
#endif

#endif /* NVGPU_GR_GLOBAL_CTX_H */
