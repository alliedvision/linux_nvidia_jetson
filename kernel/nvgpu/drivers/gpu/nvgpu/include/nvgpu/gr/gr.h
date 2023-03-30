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

#ifndef NVGPU_GR_H
#define NVGPU_GR_H

#include <nvgpu/types.h>

/**
 * @file
 * @page unit-gr Unit GR
 *
 * Overview
 * ========
 *
 * common.gr unit is responsible for managing the GR engine on the GPU.
 * There are two aspects of GR engine support managed by this unit:
 *
 * - GR engine h/w.
 * - GR engine s/w context images.
 *
 * GR engine h/w management
 * ------------------------
 *
 * common.gr unit has below responsibilities to manage GR engine h/w:
 *
 * - Reset and enable GR engine h/w.
 * - Allocate all necessary s/w data structures to hold GR engine
 *   configuration.
 * - Configure GR engine h/w to a known good state.
 * - Populate all s/w data structures while initializing the h/w.
 *   e.g. populate number of available GPC/TPC/SM components.
 * - Provide APIs to retrieve GR engine configuration.
 * - Enable and handle all GR engine interrupts and exceptions.
 * - Suspend GR engine while preparing GPU for poweroff.
 * - Remove GR engine s/w support as part of removing GPU support.
 *
 * GR engine s/w context image management
 * --------------------------------------
 *
 * common.gr unit has below responsibilities to manage GR engine context
 * images:
 *
 * - Manage all global context images.
 * - Manage GR engine context (per GPU Time Slice Group).
 * - Manage subcontext (per GPU channel).
 * - Allocate Golden context image.
 * - Map/unmap all global context images into GR engine context.
 *
 * Data Structures
 * ===============
 *
 * All the major data structures are defined privately in common.gr
 * unit. However common.gr unit exposes below public data structures
 * to support ucode handling in common.acr unit:
 *
 *   + struct nvgpu_ctxsw_ucode_segment
 *
 *       This struct describes single ucode segment.
 *
 *   + struct nvgpu_ctxsw_ucode_segments
 *
 *       This struct describes the ucode layout and includes description
 *       of boot/data/code segments of ucode.
 *
 * Static Design
 * =============
 *
 *   + include/nvgpu/gr/fs_state.h
 *   + include/nvgpu/gr/setup.h
 *   + include/nvgpu/gr/config.h
 *   + include/nvgpu/gr/ctx.h
 *   + include/nvgpu/gr/subctx.h
 *   + include/nvgpu/gr/global_ctx.h
 *   + include/nvgpu/gr/obj_ctx.h
 *   + include/nvgpu/gr/gr_falcon.h
 *   + include/nvgpu/gr/gr_intr.h
 *   + include/nvgpu/gr/gr_utils.h
 *
 * Resource utilization
 * --------------------
 *
 * External APIs
 * -------------
 *
 * Supporting Functionality
 * ========================
 *
 * Dependencies
 * ------------
 *
 * Dynamic Design
 * ==============
 *
 * Open Items
 * ==========
 *
 */
struct gk20a;
struct nvgpu_gr;
struct nvgpu_gr_config;
struct netlist_av_list;

/**
 * @brief Allocate memory for GR struct and initialize the minimum SW
 *        required to enable GR engine HW as phase 1 of GR engine
 *        initialization.
 *
 * @param g [in]	Pointer to GPU driver struct.
 *
 * This function allocates memory for GR struct (i.e. struct nvgpu_gr).
 * Number of GR instances are queried from #nvgpu_grmgr_get_num_gr_instances()
 * and size is allocated for each instance.
 *
 * This function executes only a subset of s/w initialization sequence
 * that is required to enable GR engine h/w in #nvgpu_gr_enable_hw().
 *
 * This initialization includes allocating memory for internal data
 * structures required to enable h/w. This function allocates memory
 * for FECS ECC error counters and GR interrupt structure.
 *
 * Note that all rest of the s/w initialization is completed in
 * #nvgpu_gr_init_support() function.
 *
 * @return 0 in case of success, < 0 in case of failure.
 * @retval -ENOMEM if memory allocation fails for GR struct.
 */
int nvgpu_gr_alloc(struct gk20a *g);

/**
 * @brief Free GR struct.
 *
 * @param g [in]	Pointer to GPU driver struct.
 *
 * This function ensures that memory allocated for GR struct is released
 * during deinitialization.
 */
void nvgpu_gr_free(struct gk20a *g);

/**
 * @brief Reset and enable GR engine HW as phase 2 of GR engine
 *        initialization.
 *
 * @param g [in]	Pointer to GPU driver struct.
 *
 * This function enables GR engine h/w. This includes:
 * - Resetting GR engine in MC.
 * - Load PROD register values.
 * - Disable elcg until GR engine gets enabled.
 * - Enable fifo access.
 * - Enabling GR engine interrupts.
 * - Load non_context init.
 * - Ensure falcon memory is scrubbed.
 * - Wait till GR engine status busy bit gets cleared.
 *
 * @return 0 in case of success, < 0 in case of failure.
 * @retval -ETIMEDOUT if falcon mem scrubbing times out.
 * @retval -EAGAIN if GR engine idle wait times out.
 */
int nvgpu_gr_enable_hw(struct gk20a *g);

/**
 * @brief Initialize CTXSW, GR engine HW, GR engine interrupts, and
 *        necessary SW context support as phase 3 (final phase) of GR
 *        engine initialization.
 *
 * @param g [in]	Pointer to GPU driver struct.
 *
 * This function initializes all the GR engine support and
 * functionality. This includes:
 * - Initializing context switch ucode. In case of failure, report
 *   it to qnx.sdl using #gr_intr_report_ctxsw_error().
 * - Reading Golden context image size from FECS micro controller.
 * - Allocating memory for all internal data structures such as GR ctx desc
 *   and ECC.
 * - Allocating global context buffers.
 * - Initializing GR engine h/w registers to known good values.
 * - Enabling GR engine stalling and non-stalling interrupts and exceptions
 *   at GR unit and MC level.
 * - Reading GR engine configuration (like number of GPC/TPC/SM etc)
 *   after considering floorsweeping.
 *
 * This function must be called in this sequence:
 * - nvgpu_gr_alloc()
 * - nvgpu_gr_enable_hw()
 * - nvgpu_gr_init_support()
 *
 * @return 0 in case of success, < 0 in case of failure.
 * @retval -ENOENT if context switch ucode is not found.
 * @retval -ETIMEDOUT if context switch ucode times out.
 * @retval -ETIMEDOUT if reading golden context size times out.
 * @retval -ENOMEM if memory allocation fails for any internal data
 *         structure.
 * @retval -ENOMEM if mapping to address space fails for any internal data
 *         structure.
 * @retval -ETIMEDOUT if falcon method opcode submission returns
 *         ucode wait status of \a WAIT_UCODE_TIMEOUT.
 * @retval -EINVAL if falcon method opcode submission returns
 *         ucode wait status of \a WAIT_UCODE_ERROR.
 */
int nvgpu_gr_init_support(struct gk20a *g);

/**
 * @brief Set GR s/w ready status.
 *
 * @param g [in]	Pointer to GPU driver struct.
 * @param enable [in]	Boolean flag.
 *
 * This function sets/unsets GR s/w ready status in struct nvgpu_gr.
 * Setting of flag is typically needed during initialization of GR s/w.
 * Unsetting of flag is needed while preparing for poweroff.
 */
void nvgpu_gr_sw_ready(struct gk20a *g, bool enable);

/**
 * @brief Get number of SMs in GR engine.
 *
 * @param g [in]	Pointer to GPU driver struct.
 *
 * This function returns number of SMs available in GR engine.
 * Note that this count is initialized only after GR engine is
 * completely initialized through #nvgpu_gr_init_support().
 *
 * @return number of available SMs in GR engine.
 */
u32 nvgpu_gr_get_no_of_sm(struct gk20a *g);

/**
 * @brief Suspend GR engine.
 *
 * @param g [in]	Pointer to GPU driver struct.
 *
 * This function is typically called while preparing for GPU power off.
 * This function makes sure that GR engine is idle before power off.
 * It will also disable all GR engine stalling and non-stalling
 * interrupts and exceptions at GR unit level and GR engine
 * stalling and non-stalling interrupts at MC level.
 *
 * @return 0 in case of success, < 0 in case of failure.
 * @retval -EAGAIN if GR engine idle wait times out.
 */
int nvgpu_gr_suspend(struct gk20a *g);

/**
 * @brief Remove GR engine s/w support.
 *
 * @param g [in]	Pointer to GPU driver struct.
 *
 * This is typically called while removing entire GPU driver.
 * This function will ensure that all memory and other system resources
 * allocated during GR s/w initialization are released appropriately.
 */
void nvgpu_gr_remove_support(struct gk20a *g);

/**
 * @brief Get base register offset of a given GPC.
 *
 * @param g [in]	Pointer to GPU driver struct.
 * @param gpc [in]	Valid GPC index.
 *
 * This function calculates and returns base register offset of a given
 * GPC. GPC index must be less than value returned by
 * #nvgpu_gr_config_get_gpc_count(), otherwise an assert is raised.
 *
 * @return base register offset of a given GPC.
 */
u32 nvgpu_gr_gpc_offset(struct gk20a *g, u32 gpc);

/**
 * @brief Get base register offset of a given TPC in a GPC.
 *
 * @param g [in]	Pointer to GPU driver struct.
 * @param tpc [in]	Valid TPC index.
 *
 * This function calculates and returns base register offset of a given
 * TPC within a GPC. TPC index must be less than value returned by
 * #nvgpu_gr_config_get_max_tpc_per_gpc_count(), otherwise an assert
 * is raised.
 *
 * @return base register offset of a given TPC.
 */
u32 nvgpu_gr_tpc_offset(struct gk20a *g, u32 tpc);

/**
 * @brief Get base register offset of a given SM in a GPC/TPC.
 *
 * @param g [in]	Pointer to GPU driver struct.
 * @param sm [in]	Valid SM index.
 *
 * This function calculates and returns base register offset of a given
 * SM within a GPC/TPC pair. SM index must be less than value returned by
 * #nvgpu_gr_config_get_sm_count_per_tpc(), otherwise an assert
 * is raised.
 *
 * @return base register offset of a given SM.
 */
u32 nvgpu_gr_sm_offset(struct gk20a *g, u32 sm);

/**
 * @brief Get pri base register offset of a given ROP instance within a GPC
 *
 * @param g [in]	Pointer to GPU driver struct.
 * @param rop [in]	ROP index.
 *
 * This function calculates and returns base register offset of a given
 * ROP within a GPC.
 *
 * @return base register offset of a given ROP.
 */
u32 nvgpu_gr_rop_offset(struct gk20a *g, u32 rop);

u32 nvgpu_gr_get_syspipe_id(struct gk20a *g, u32 gr_instance_id);

#ifdef CONFIG_NVGPU_HAL_NON_FUSA
/**
 * @brief Wait for GR engine to be initialized
 *
 * @param g [in]	Pointer to GPU driver struct.
 *
 * Calling this function ensures that GR engine initialization i.e.
 * nvgpu_gr_init_support() function call is complete.
 */
void nvgpu_gr_wait_initialized(struct gk20a *g);
#endif
bool nvgpu_gr_is_tpc_addr(struct gk20a *g, u32 addr);
u32 nvgpu_gr_get_tpc_num(struct gk20a *g, u32 addr);

#if defined(CONFIG_NVGPU_RECOVERY) || defined(CONFIG_NVGPU_DEBUGGER)
int nvgpu_gr_disable_ctxsw(struct gk20a *g);
int nvgpu_gr_enable_ctxsw(struct gk20a *g);
#endif
#ifdef CONFIG_NVGPU_ENGINE_RESET
int nvgpu_gr_reset(struct gk20a *g);
#endif

/** @cond DOXYGEN_SHOULD_SKIP_THIS */
void nvgpu_gr_init_reset_enable_hw_non_ctx_local(struct gk20a *g);
void nvgpu_gr_init_reset_enable_hw_non_ctx_global(struct gk20a *g);
/** @endcond DOXYGEN_SHOULD_SKIP_THIS */

#endif /* NVGPU_GR_H */
