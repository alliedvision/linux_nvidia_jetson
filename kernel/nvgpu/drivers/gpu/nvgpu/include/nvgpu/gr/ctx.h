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

#ifndef NVGPU_GR_CTX_H
#define NVGPU_GR_CTX_H

#include <nvgpu/types.h>
#include <nvgpu/nvgpu_mem.h>
#include <nvgpu/gr/global_ctx.h>

#define NVGPU_INVALID_SM_CONFIG_ID (U32_MAX)

/** Supports only mission (default) context. */
#define NVGPU_DEFAULT_SM_DIVERSITY_CONFIG 0U

/**
 * @file
 *
 * common.gr.ctx unit interface
 */
struct gk20a;
struct vm_gk20a;
struct nvgpu_gr_ctx;
struct nvgpu_gr_global_ctx_buffer_desc;
struct nvgpu_gr_global_ctx_local_golden_image;
struct patch_desc;
struct pm_ctx_desc;
struct nvgpu_gr_ctx_desc;
#ifdef CONFIG_NVGPU_GRAPHICS
struct zcull_ctx_desc;
#endif

/*
 * allocate a minimum of 1 page (4KB) worth of patch space, this is 512 entries
 * of address and data pairs.
 */
/** Number of slots required in patch buffer per entry. */
#define PATCH_CTX_SLOTS_REQUIRED_PER_ENTRY	2U
/** Number of slots per NVGPU_CPU_PAGE_SIZE. */
#define PATCH_CTX_SLOTS_PER_PAGE \
	(NVGPU_CPU_PAGE_SIZE/(PATCH_CTX_SLOTS_REQUIRED_PER_ENTRY * (u32)sizeof(u32)))
/** Get number of entries in patch buffer given the size of buffer. */
#define PATCH_CTX_ENTRIES_FROM_SIZE(size) ((size)/sizeof(u32))

/** S/W defined value for preemption mode with no input. */
#define NVGPU_PREEMPTION_MODE_NONE		0U
/** S/W defined value for invalid preemption mode. */
#define NVGPU_PREEMPTION_MODE_INVALID		BIT32(31)

/** S/W defined value for WFI graphics preemption mode. */
#define NVGPU_PREEMPTION_MODE_GRAPHICS_WFI	BIT32(0)
#ifdef CONFIG_NVGPU_GFXP
/** S/W defined value for GFxP graphics preemption mode. */
#define NVGPU_PREEMPTION_MODE_GRAPHICS_GFXP	BIT32(1)
#endif

/** S/W defined value for WFI compute preemption mode. */
#define NVGPU_PREEMPTION_MODE_COMPUTE_WFI	BIT32(0)
/** S/W defined value for CTA compute preemption mode. */
#define NVGPU_PREEMPTION_MODE_COMPUTE_CTA	BIT32(1)
#ifdef CONFIG_NVGPU_CILP
/** S/W defined value for CILP compute preemption mode. */
#define NVGPU_PREEMPTION_MODE_COMPUTE_CILP	BIT32(2)
#endif

/** S/W defined index for Graphics context buffer. */
#define NVGPU_GR_CTX_CTX		0U
#ifdef CONFIG_NVGPU_DEBUGGER
/** S/W defined index for PM context buffer. */
#define NVGPU_GR_CTX_PM_CTX		1U
#endif
/** S/W defined index for patch context buffer. */
#define NVGPU_GR_CTX_PATCH_CTX		2U
#ifdef CONFIG_NVGPU_GFXP
/** S/W defined index for preempt ctxsw buffer. */
#define NVGPU_GR_CTX_PREEMPT_CTXSW	3U
/** S/W defined index for spill ctxsw buffer. */
#define NVGPU_GR_CTX_SPILL_CTXSW	4U
/** S/W defined index for betacb ctxsw buffer. */
#define NVGPU_GR_CTX_BETACB_CTXSW	5U
/** S/W defined index for pagepool ctxsw buffer. */
#define NVGPU_GR_CTX_PAGEPOOL_CTXSW	6U
/** S/W defined index for rtvcb ctxsw buffer. */
#define NVGPU_GR_CTX_GFXP_RTVCB_CTXSW	7U
/** Number of context buffers. */
#define NVGPU_GR_CTX_COUNT		8U
#else
/** Number of context buffers. */
#define NVGPU_GR_CTX_COUNT		3U
#endif

/*
 * either ATTRIBUTE or ATTRIBUTE_VPR maps to NVGPU_GR_CTX_ATTRIBUTE_VA.
*/
/** S/W defined index for circular context buffer virtual address. */
#define NVGPU_GR_CTX_CIRCULAR_VA		0U
/** S/W defined index for pagepool context buffer virtual address. */
#define NVGPU_GR_CTX_PAGEPOOL_VA		1U
/** S/W defined index for attribute context buffer virtual address. */
#define NVGPU_GR_CTX_ATTRIBUTE_VA		2U
/** S/W defined index for access map buffer virtual address. */
#define NVGPU_GR_CTX_PRIV_ACCESS_MAP_VA		3U
/** S/W defined index for RTV circular context buffer virtual address. */
#define NVGPU_GR_CTX_RTV_CIRCULAR_BUFFER_VA	4U
#ifdef CONFIG_NVGPU_FECS_TRACE
/** S/W defined index for fecs trace buffer virtual address. */
#define NVGPU_GR_CTX_FECS_TRACE_BUFFER_VA	5U
#endif
/** Number of context buffer virtual addresses. */
#define NVGPU_GR_CTX_VA_COUNT			6U

#ifdef CONFIG_NVGPU_DEBUGGER
/* PM Context Switch Modes */
/** This mode says that the pms are not to be context switched. */
#define NVGPU_GR_CTX_HWPM_CTXSW_MODE_NO_CTXSW               (0x00000000U)
/** This mode says that the pms in Mode-B are to be context switched. */
#define NVGPU_GR_CTX_HWPM_CTXSW_MODE_CTXSW                  (0x00000001U)
/** This mode says that the pms in Mode-E (stream out) are to be context switched. */
#define NVGPU_GR_CTX_HWPM_CTXSW_MODE_STREAM_OUT_CTXSW       (0x00000002U)
#endif

/**
 * @brief Initialize GR context descriptor structure.
 *
 * @param g [in]		Pointer to GPU driver struct.
 *
 * This function allocates memory for #nvgpu_gr_ctx_desc structure.
 *
 * @return pointer to #nvgpu_gr_ctx_desc struct in case of success,
 *         NULL in case of failure.
 */
struct nvgpu_gr_ctx_desc *nvgpu_gr_ctx_desc_alloc(struct gk20a *g);

/**
 * @brief Free GR context descriptor structure.
 *
 * @param g [in]		Pointer to GPU driver struct.
 * @param desc [in]		Pointer to context descriptor struct.
 *
 * This function will free memory allocated for #nvgpu_gr_ctx_desc structure.
 */
void nvgpu_gr_ctx_desc_free(struct gk20a *g,
	struct nvgpu_gr_ctx_desc *desc);

/**
 * @brief Set size of GR context buffer with given index.
 *
 * @param desc [in]		Pointer to context descriptor struct.
 * @param index [in]		Index of GR context buffer.
 * @param size [in]		Size of buffer to be set.
 *
 * This function sets size of GR context buffer with given buffer
 * index. \a index must be less than NVGPU_GR_CTX_COUNT otherwise
 * an assert is raised.
 */
void nvgpu_gr_ctx_set_size(struct nvgpu_gr_ctx_desc *gr_ctx_desc,
	u32 index, u32 size);

/**
 * @brief Allocate graphics context buffer.
 *
 * @param g [in]		Pointer to GPU driver struct.
 * @param gr_ctx [in]		Pointer to graphics context struct.
 * @param gr_ctx_desc [in]	Pointer to context descriptor struct.
 * @param vm [in]		Pointer to virtual memory.
 *
 * This function allocates memory for graphics context buffer and also
 * maps it to given virtual memory.
 *
 * @return 0 in case of success, < 0 in case of failure.
 * @retval -ENOMEM if context memory allocation fails.
 * @retval -EINVAL if context buffer size is not set in
 *         #nvgpu_gr_ctx_desc struct.
 */
int nvgpu_gr_ctx_alloc(struct gk20a *g,
	struct nvgpu_gr_ctx *gr_ctx,
	struct nvgpu_gr_ctx_desc *gr_ctx_desc,
	struct vm_gk20a *vm);

/**
 * @brief Free graphics context buffer.
 *
 * @param g [in]		Pointer to GPU driver struct.
 * @param gr_ctx [in]		Pointer to graphics context struct.
 * @param global_ctx_buffer [in]Pointer to global context descriptor struct.
 * @param vm [in]		Pointer to virtual memory.
 *
 * This function will free memory allocated for graphics context buffer,
 * patch context buffer, and all the ctxsw buffers.
 */
void nvgpu_gr_ctx_free(struct gk20a *g,
	struct nvgpu_gr_ctx *gr_ctx,
	struct nvgpu_gr_global_ctx_buffer_desc *global_ctx_buffer,
	struct vm_gk20a *vm);

/**
 * @brief Allocate patch context buffer.
 *
 * @param g [in]		Pointer to GPU driver struct.
 * @param gr_ctx [in]		Pointer to graphics context struct.
 * @param gr_ctx_desc [in]	Pointer to context descriptor struct.
 * @param vm [in]		Pointer to virtual memory.
 *
 * This function allocates memory for patch context buffer and also
 * maps it to given virtual memory.
 *
 * @return 0 in case of success, < 0 in case of failure.
 * @retval -ENOMEM if context memory allocation fails.
 */
int nvgpu_gr_ctx_alloc_patch_ctx(struct gk20a *g,
	struct nvgpu_gr_ctx *gr_ctx,
	struct nvgpu_gr_ctx_desc *gr_ctx_desc,
	struct vm_gk20a *vm);

/**
 * @brief Free patch context buffer.
 *
 * @param g [in]		Pointer to GPU driver struct.
 * @param vm [in]		Pointer to virtual memory.
 * @param gr_ctx [in]		Pointer to graphics context struct.
 *
 * This function will free memory allocated for patch context buffer.
 */
void nvgpu_gr_ctx_free_patch_ctx(struct gk20a *g, struct vm_gk20a *vm,
	struct nvgpu_gr_ctx *gr_ctx);

/**
 * @brief Map global context buffers.
 *
 * @param g [in]		Pointer to GPU driver struct.
 * @param gr_ctx [in]		Pointer to graphics context struct.
 * @param global_ctx_buffer [in]Pointer to global context descriptor struct.
 * @param vm [in]		Pointer to virtual memory.
 * @param vpr [in]		Boolean flag to use buffers in VPR.
 *
 * This function maps all global context buffers into given
 * virtual memory and stores each virtual address into given
 * #nvgpu_gr_ctx struct.
 *
 * @return 0 in case of success, < 0 in case of failure.
 * @retval -ENOMEM if memory mapping fails for any context buffer.
 */
int nvgpu_gr_ctx_map_global_ctx_buffers(struct gk20a *g,
	struct nvgpu_gr_ctx *gr_ctx,
	struct nvgpu_gr_global_ctx_buffer_desc *global_ctx_buffer,
	struct vm_gk20a *vm, bool vpr);

/**
 * @brief Get global context buffer virtual address.
 *
 * @param gr_ctx [in]		Pointer to graphics context struct.
 * @param index [in]		Index of global context buffer.
 *
 * This function returns virtual address of global context buffer
 * with given index stored in #nvgpu_gr_ctx struct.
 *
 * @return virtual address of global context buffer.
 */
u64 nvgpu_gr_ctx_get_global_ctx_va(struct nvgpu_gr_ctx *gr_ctx,
	u32 index);

/**
 * @brief Get pointer of patch context buffer memory struct.
 *
 * @param gr_ctx [in]		Pointer to graphics context struct.
 *
 * This function returns #nvgpu_mem pointer of patch context buffer stored
 * in #nvgpu_gr_ctx struct.
 *
 * @return pointer to patch context buffer memory struct.
 */
struct nvgpu_mem *nvgpu_gr_ctx_get_patch_ctx_mem(struct nvgpu_gr_ctx *gr_ctx);

/**
 * @brief Set data count in patch context buffer.
 *
 * @param gr_ctx [in]		Pointer to graphics context struct.
 * @param data_count [in]	Value to be set.
 *
 * This function sets data count of patch context buffer. Data count
 * indicates how many entries have been written into patch context.
 */
void nvgpu_gr_ctx_set_patch_ctx_data_count(struct nvgpu_gr_ctx *gr_ctx,
	u32 data_count);

/**
 * @brief Get sm diversity config of the given graphics context.
 *
 * @param gr_ctx [in]		Pointer to graphics context struct.
 *
 * This function returns #sm_diversity_config of graphics context struct.
 *
 * @return sm diversity config of the given graphics context.
 */
struct nvgpu_mem *nvgpu_gr_ctx_get_ctx_mem(struct nvgpu_gr_ctx *gr_ctx);

#ifdef CONFIG_NVGPU_SM_DIVERSITY
/**
 * @brief Set sm diversity config in the given graphics context struct.
 *
 * @param gr_ctx [in]		Pointer to graphics context struct.
 * @param sm_diversity_config [in]	Value to be set.
 *
 * This function sets sm diversity config of the given graphics context struct.
 */
void nvgpu_gr_ctx_set_sm_diversity_config(struct nvgpu_gr_ctx *gr_ctx,
	u32 sm_diversity_config);

/**
 * @brief Get pointer of graphics context buffer memory struct.
 *
 * @param gr_ctx [in]		Pointer to graphics context struct.
 *
 * @return sm diversity config of the given graphics context struct.
 */
u32 nvgpu_gr_ctx_get_sm_diversity_config(struct nvgpu_gr_ctx *gr_ctx);
#endif

/**
 * @brief Load local golden image into given graphics context buffer.
 *
 * @param g [in]			Pointer to GPU driver struct.
 * @param gr_ctx [in]			Pointer to graphics context struct.
 * @param local_golden_image [in]	Pointer to local golden image struct.
 * @param cde [in]			Boolean flag to enable/disable CDE.
 *
 * This function loads local golden image copy into given new graphics
 * context buffer.
 *
 * Local golden image copy is saved while creating first graphics context
 * buffer. Subsequent graphics contexts can be initialized by loading
 * golden image into new context with this function.
 */
void nvgpu_gr_ctx_load_golden_ctx_image(struct gk20a *g,
	struct nvgpu_gr_ctx *gr_ctx,
	struct nvgpu_gr_global_ctx_local_golden_image *local_golden_image,
	bool cde);

/**
 * @brief Prepare patch context buffer for writes.
 *
 * @param g [in]			Pointer to GPU driver struct.
 * @param gr_ctx [in]			Pointer to graphics context struct.
 * @param update_patch_count [in]	Boolean flag to update data count
 *                                      in patch context.
 *
 * This function will prepare patch context buffer for writes. This
 * function must be called before calling #nvgpu_gr_ctx_patch_write()
 * to add entries into the buffer.
 */
void nvgpu_gr_ctx_patch_write_begin(struct gk20a *g,
	struct nvgpu_gr_ctx *gr_ctx,
	bool update_patch_count);

/**
 * @brief Finish patch context buffer writes.
 *
 * @param g [in]			Pointer to GPU driver struct.
 * @param gr_ctx [in]			Pointer to graphics context struct.
 * @param update_patch_count [in]	Boolean flag to update data count
 *                                      in patch context.
 *
 * This function will write final data count into patch context image
 * after all entries have been written by calling
 * #nvgpu_gr_ctx_patch_write().
 */
void nvgpu_gr_ctx_patch_write_end(struct gk20a *g,
	struct nvgpu_gr_ctx *gr_ctx,
	bool update_patch_count);

/**
 * @brief Edit the context state.
 *
 * @param g [in]		Pointer to GPU driver struct.
 * @param gr_ctx [in]		Pointer to graphics context struct.
 * @param addr [in]		Address entry to be added.
 * @param data [in]		Data entry to be added.
 * @param patch [in]		Boolean flag.
 *
 * This function will write given addr and data entries into patch
 * context buffer if boolean flag patch is set. In this case, the
 * context state will be updated on next context restore to h/w.
 *
 * If patch is not set, context is resident on h/w already and this
 * function will write data value into h/w register with address addr
 * directly.
 *
 * This function must be preceded by #nvgpu_gr_ctx_patch_write_begin()
 * and succeeded by #nvgpu_gr_ctx_patch_write_end().
 */
void nvgpu_gr_ctx_patch_write(struct gk20a *g,
	struct nvgpu_gr_ctx *gr_ctx,
	u32 addr, u32 data, bool patch);

/**
 * @brief Set given compute preemption mode in graphics context struct.
 *
 * @param gr_ctx [in]			Pointer to graphics context struct.
 * @param compute_preempt_mode [in]	Compute preemption mode to be set.
 *
 * This function will set given compute preemption mode into #nvgpu_gr_ctx
 * structure.
 *
 * @see nvgpu_gr_ctx_check_valid_preemption_mode for valid compute preemption
 *      modes.
 */
void nvgpu_gr_ctx_init_compute_preemption_mode(struct nvgpu_gr_ctx *gr_ctx,
	u32 compute_preempt_mode);

/**
 * @brief Get compute preemption mode stored in graphics context struct.
 *
 * @param gr_ctx [in]		Pointer to graphics context struct.
 *
 * This function returns compute preemption mode stored in #nvgpu_gr_ctx
 * struct.
 *
 * @return compute preemption mode.
 */
u32 nvgpu_gr_ctx_get_compute_preemption_mode(struct nvgpu_gr_ctx *gr_ctx);

/**
 * @brief Check if given preemption modes are valid.
 *
 * @param g [in]			Pointer to GPU driver struct.
 * @param gr_ctx [in]			Pointer to graphics context struct.
 * @param graphics_preempt_mode		Requested graphics preemption mode.
 * @param compute_preempt_mode		Requested compute preemption mode.
 *
 * This function checks if requested graphics/compute preemption modes are
 * valid or not.
 *
 * The function supports NVGPU_PREEMTION_MODE_GRAPHICS_WFI graphics
 * preemption mode and NVGPU_PREEMTION_MODE_COMPUTE_WFI,
 * NVGPU_PREEMTION_MODE_COMPUTE_CTA compute preemption modes as
 * valid preemption modes. Any other preemption mode is invalid.
 *
 * Requesting both CILP compute and GFxP graphics preemption modes at
 * the same time is not allowed.
 *
 * It is also not allowed to downgrade the preemption modes to lower
 * type.
 *
 * @return true if requested preemption modes are valid, false otherwise.
 */
bool nvgpu_gr_ctx_check_valid_preemption_mode(struct gk20a *g,
	struct nvgpu_gr_ctx *gr_ctx,
	u32 graphics_preempt_mode, u32 compute_preempt_mode);

/**
 * @brief Write preemption modes in graphics context image.
 *
 * @param g [in]		Pointer to GPU driver struct.
 * @param gr_ctx [in]		Pointer to graphics context struct.
 *
 * This function writes graphics/compute preemption modes into graphics
 * context image. Preemption mode values are taken from #nvgpu_gr_ctx
 * structure.
 */
void nvgpu_gr_ctx_set_preemption_modes(struct gk20a *g,
	struct nvgpu_gr_ctx *gr_ctx);

/**
 * @brief Initialize GR context structure.
 *
 * @param g [in]		Pointer to GPU driver struct.
 *
 * This function allocates memory for #nvgpu_gr_ctx structure.
 *
 * @return pointer to #nvgpu_gr_ctx struct in case of success,
 *         NULL in case of failure.
 */
struct nvgpu_gr_ctx *nvgpu_alloc_gr_ctx_struct(struct gk20a *g);

/**
 * @brief Free GR context structure.
 *
 * @param g [in]		Pointer to GPU driver struct.
 * @param gr_ctx [in]		Pointer to graphics context struct.
 *
 * This function will free memory allocated for #nvgpu_gr_ctx structure.
 */
void nvgpu_free_gr_ctx_struct(struct gk20a *g, struct nvgpu_gr_ctx *gr_ctx);

/**
 * @brief Set TSG id in graphics context structure.
 *
 * @param gr_ctx [in]		Pointer to graphics context struct.
 * @param tsgid [in]		TSG identifier.
 *
 * This function will set given TSG identifier into #nvgpu_gr_ctx
 * structure.
 */
void nvgpu_gr_ctx_set_tsgid(struct nvgpu_gr_ctx *gr_ctx, u32 tsgid);

/**
 * @brief Get TSG id stored in graphics context structure.
 *
 * @param gr_ctx [in]		Pointer to graphics context struct.
 *
 * This function will return TSG identifier stored in #nvgpu_gr_ctx
 * structure.
 *
 * @return TSG identifier.
 */
u32 nvgpu_gr_ctx_get_tsgid(struct nvgpu_gr_ctx *gr_ctx);

#ifdef CONFIG_NVGPU_CILP
bool nvgpu_gr_ctx_get_cilp_preempt_pending(struct nvgpu_gr_ctx *gr_ctx);

void nvgpu_gr_ctx_set_cilp_preempt_pending(struct nvgpu_gr_ctx *gr_ctx,
	bool cilp_preempt_pending);

bool nvgpu_gr_ctx_desc_force_preemption_cilp(
		struct nvgpu_gr_ctx_desc *gr_ctx_desc);
#endif /* CONFIG_NVGPU_CILP */

#ifdef CONFIG_NVGPU_GFXP
int nvgpu_gr_ctx_alloc_ctxsw_buffers(struct gk20a *g,
	struct nvgpu_gr_ctx *gr_ctx,
	struct nvgpu_gr_ctx_desc *gr_ctx_desc,
	struct vm_gk20a *vm);

struct nvgpu_mem *nvgpu_gr_ctx_get_spill_ctxsw_buffer(
	struct nvgpu_gr_ctx *gr_ctx);

struct nvgpu_mem *nvgpu_gr_ctx_get_betacb_ctxsw_buffer(
	struct nvgpu_gr_ctx *gr_ctx);

struct nvgpu_mem *nvgpu_gr_ctx_get_pagepool_ctxsw_buffer(
	struct nvgpu_gr_ctx *gr_ctx);

struct nvgpu_mem *nvgpu_gr_ctx_get_preempt_ctxsw_buffer(
	struct nvgpu_gr_ctx *gr_ctx);

struct nvgpu_mem *nvgpu_gr_ctx_get_gfxp_rtvcb_ctxsw_buffer(
	struct nvgpu_gr_ctx *gr_ctx);

void nvgpu_gr_ctx_set_preemption_buffer_va(struct gk20a *g,
	struct nvgpu_gr_ctx *gr_ctx);

bool nvgpu_gr_ctx_desc_force_preemption_gfxp(
		struct nvgpu_gr_ctx_desc *gr_ctx_desc);
#endif /* CONFIG_NVGPU_GFXP */

#ifdef CONFIG_NVGPU_GRAPHICS
void nvgpu_gr_ctx_init_graphics_preemption_mode(struct nvgpu_gr_ctx *gr_ctx,
	u32 graphics_preempt_mode);
u32 nvgpu_gr_ctx_get_graphics_preemption_mode(struct nvgpu_gr_ctx *gr_ctx);

void nvgpu_gr_ctx_set_zcull_ctx(struct gk20a *g, struct nvgpu_gr_ctx *gr_ctx,
	u32 mode, u64 gpu_va);

u64 nvgpu_gr_ctx_get_zcull_ctx_va(struct nvgpu_gr_ctx *gr_ctx);

int nvgpu_gr_ctx_init_zcull(struct gk20a *g, struct nvgpu_gr_ctx *gr_ctx);

int nvgpu_gr_ctx_zcull_setup(struct gk20a *g, struct nvgpu_gr_ctx *gr_ctx,
	bool set_zcull_ptr);
#endif /* CONFIG_NVGPU_GRAPHICS */

#ifdef CONFIG_NVGPU_DEBUGGER
int nvgpu_gr_ctx_alloc_pm_ctx(struct gk20a *g,
	struct nvgpu_gr_ctx *gr_ctx,
	struct nvgpu_gr_ctx_desc *gr_ctx_desc,
	struct vm_gk20a *vm);
void nvgpu_gr_ctx_free_pm_ctx(struct gk20a *g, struct vm_gk20a *vm,
	struct nvgpu_gr_ctx *gr_ctx);

void nvgpu_gr_ctx_reset_patch_count(struct gk20a *g,
	struct nvgpu_gr_ctx *gr_ctx);
void nvgpu_gr_ctx_set_patch_ctx(struct gk20a *g, struct nvgpu_gr_ctx *gr_ctx,
	bool set_patch_addr);

u32 nvgpu_gr_ctx_get_ctx_id(struct gk20a *g, struct nvgpu_gr_ctx *gr_ctx);
u32 nvgpu_gr_ctx_read_ctx_id(struct nvgpu_gr_ctx *gr_ctx);

struct nvgpu_mem *nvgpu_gr_ctx_get_pm_ctx_mem(struct nvgpu_gr_ctx *gr_ctx);

void nvgpu_gr_ctx_set_pm_ctx_pm_mode(struct nvgpu_gr_ctx *gr_ctx, u32 pm_mode);
u32 nvgpu_gr_ctx_get_pm_ctx_pm_mode(struct nvgpu_gr_ctx *gr_ctx);

int nvgpu_gr_ctx_set_smpc_mode(struct gk20a *g, struct nvgpu_gr_ctx *gr_ctx,
	bool enable);

int nvgpu_gr_ctx_prepare_hwpm_mode(struct gk20a *g, struct nvgpu_gr_ctx *gr_ctx,
	u32 mode, bool *skip_update);
int nvgpu_gr_ctx_set_hwpm_mode(struct gk20a *g, struct nvgpu_gr_ctx *gr_ctx,
	bool set_pm_ptr);

#ifdef CONFIG_NVGPU_CHANNEL_TSG_SCHEDULING
void nvgpu_gr_ctx_set_boosted_ctx(struct nvgpu_gr_ctx *gr_ctx, bool boost);
bool nvgpu_gr_ctx_get_boosted_ctx(struct nvgpu_gr_ctx *gr_ctx);
#endif
#endif /* CONFIG_NVGPU_DEBUGGER */

#ifdef CONFIG_DEBUG_FS
bool nvgpu_gr_ctx_desc_dump_ctxsw_stats_on_channel_close(
		struct nvgpu_gr_ctx_desc *gr_ctx_desc);
#endif

#endif /* NVGPU_GR_CTX_H */
