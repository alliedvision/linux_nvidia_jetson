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

#ifndef NVGPU_ENGINE_H
#define NVGPU_ENGINE_H
/**
 * @file
 *
 * Abstract interface for engine related functionality.
 */

#include <nvgpu/types.h>

struct gk20a;
struct nvgpu_device;

/** @cond DOXYGEN_SHOULD_SKIP_THIS */
#define ENGINE_PBDMA_INSTANCE0 0U

int nvgpu_engine_init_one_dev_extra(struct gk20a *g,
		const struct nvgpu_device *dev);
/** @endcond DOXYGEN_SHOULD_SKIP_THIS */

/**
 * Invalid engine id value.
 */
#define NVGPU_INVALID_ENG_ID		(~U32(0U))

struct gk20a;
struct nvgpu_fifo;
struct nvgpu_device;

/**
 * Engine enum types used for s/w purpose. These enum values are
 * different as compared to engine enum types defined by h/w.
 * Refer device.h header file.
 */
enum nvgpu_fifo_engine {
	/** GR engine enum */
	NVGPU_ENGINE_GR        = 0U,
	/** GR CE engine enum */
	NVGPU_ENGINE_GRCE      = 1U,
	/** Async CE engine enum */
	NVGPU_ENGINE_ASYNC_CE  = 2U,
	/** Invalid engine enum */
	NVGPU_ENGINE_INVAL     = 3U,
};

/**
 * @brief Get s/w defined engine enum type for engine enum type defined by h/w.
 *        See device.h for engine enum types defined by h/w.
 *
 * @param g [in]		The GPU driver struct.
 * @param dev [in]		Device to check.
 *
 * This is used to map engine enum type defined by h/w to engine enum type
 * defined by s/w.
 *
 * @return S/w defined valid engine enum type < #NVGPU_ENGINE_INVAL.
 * @retval #NVGPU_ENGINE_INVAL in case h/w #engine_type
 *         does not support gr (graphics) and ce (copy engine) engine enum
 *         types or if #engine_type does not match with h/w defined engine enum
 *         types for gr and/or ce engines.
 */
enum nvgpu_fifo_engine nvgpu_engine_enum_from_dev(struct gk20a *g,
					const struct nvgpu_device *dev);
/**
 * @brief Get pointer to #nvgpu_device for the h/w engine id.
 *
 * @param g [in]		The GPU driver struct.
 * @param engine_id [in]	Active (h/w) Engine id.
 *
 * If #engine_id is one of the supported h/w engine ids, get pointer to
 * #nvgpu_engine_info from an array of structures that is indexed by h/w
 * engine id.
 *
 * @return Pointer to #nvgpu_device.
 * @retval NULL if #g is NULL.
 * @retval NULL if engine_id is not less than max supported number of engines
 *         i.e. #nvgpu_fifo.max_engines or if #engine_id does not match with
 *         any engine id supported by h/w or number of available engines
 *         i.e. #nvgpu_fifo.num_engines is 0.
 */
const struct nvgpu_device *nvgpu_engine_get_active_eng_info(
		struct gk20a *g, u32 engine_id);

/**
 * @brief Check if engine id is one of the supported h/w engine ids.
 *
 * @param g [in]		The GPU driver struct.
 * @param engine_id [in]	Engine id.
 *
 * Check if #engine_id is one of the supported active engine ids.
 *
 * @return True if #engine_id is valid.
 * @retval False if engine_id is not less than maximum number of supported
 *         engines on the chip i.e. #nvgpu_fifo.max_engines or if engine id
 *         does not match with any of the engine ids supported by h/w.
 */
bool nvgpu_engine_check_valid_id(struct gk20a *g, u32 engine_id);
/**
 * @brief Get h/w engine id based on engine's instance identification number
 *        #NVGPU_ENGINE_GR engine enum type.
 *
 * @param g [in]		The GPU driver struct.
 * @param inst_id [in]		Engine's instance identification number.
 *
 * @return H/W engine id for #NVGPU_ENGINE_GR engine enum type.
 * @retval #NVGPU_INVALID_ENG_ID if #NVGPU_ENGINE_GR engine enum type could not
 *         be found in the set of available h/w engine ids.
 */
u32 nvgpu_engine_get_gr_id_for_inst(struct gk20a *g, u32 inst_id);
/**
 * @brief Get instance count and first available h/w engine id for
 *        #NVGPU_ENGINE_GR engine enum type.
 *
 * @param g [in]		The GPU driver struct.
 *
 * Call #nvgpu_engine_get_ids to get first available #NVGPU_ENGINE_GR
 * engine enum type.
 *
 * @return First available h/w engine id for #NVGPU_ENGINE_GR engine enum type.
 * @retval #NVGPU_INVALID_ENG_ID if #NVGPU_ENGINE_GR engine enum type could not
 *         be found in the set of available h/w engine ids.
 */
u32 nvgpu_engine_get_gr_id(struct gk20a *g);
/**
 * @brief Get intr mask for the GR engine supported by the chip.
 *
 * @param g[in]			The GPU driver struct.
 *
 * Return bitmask of each GR engine's interrupt bit.
 *
 * @return Interrupt mask for GR engine.
 */
u32 nvgpu_gr_engine_interrupt_mask(struct gk20a *g);
/**
 * @brief Get intr mask for the CE engines supported by the chip.
 *
 * @param g [in]		The GPU driver struct.
 *
 * Query all types of copy engine devices and OR their interrupt bits into
 * a CE interrupt mask.
 *
 * @return 0U if there is no CE support in the system.
 * @return The logical OR of all interrupt bits for all CE devices present.
 */
u32 nvgpu_ce_engine_interrupt_mask(struct gk20a *g);
/**
 * @brief Get intr mask for the device corresponding the provided engine_id.
 *
 * @param g [in]		The GPU driver struct.
 * @param engine_id [in]	HW engine_id.
 *
 * Return the interrupt mask for the host device corresponding to \a engine_id.
 *
 * @return Intr mask for the #engine_id or 0 if the engine_id does not have a
 *         corresponding device.
 */
u32 nvgpu_engine_act_interrupt_mask(struct gk20a *g, u32 engine_id);
/**
 * @brief Allocate and initialize s/w context for engine related info.
 *
 * @param g [in]		The GPU driver struct.
 *
 * - Get max number of engines supported on the chip from h/w config register.
 * - Allocate kernel memory area for storing engine info for max number of
 *   engines supported on the chip. This allocated area of memory is set to 0
 *   and then filled with value read from device info h/w registers.
 *   Also this area of memory is indexed by h/w engine id.
 * - Allocate kernel memory area for max number of engines supported on the
 *   chip to map s/w defined engine ids starting from 0 to h/w (active) engine
 *   id read from device info h/w register. This allocated area of memory is
 *   set to 0xff and then filled with engine id read from device info h/w
 *   registers. This area of memory is indexed by s/w defined engine id starting
 *   from 0.
 * - Initialize engine info related s/w context by calling hal that will read
 *   device info h/w registers and also initialize s/w variable
 *   #nvgpu_fifo.num_engines that is used to count total number of valid h/w
 *   engine ids read from device info h/w registers.
 *
 * @return 0 upon success, < 0 otherwise.
 * @retval -ENOMEM upon failure to allocate memory for engine structure.
 * @retval -EINVAL upon failure to get engine info from device info h/w
 *         registers.
 */
int nvgpu_engine_setup_sw(struct gk20a *g);
/**
 * @brief Clean up s/w context for engine related info.
 *
 * @param g [in]		The GPU driver struct.
 *
 * - Free up kernel memory area that is used for storing engine info read
 *   from device info h/w registers.
 * - Free up kernel memory area to map s/w defined engine ids (starting with 0)
 *   to active (h/w) engine ids read from device info h/w register.
 */
void nvgpu_engine_cleanup_sw(struct gk20a *g);

#ifdef CONFIG_NVGPU_FIFO_ENGINE_ACTIVITY
void nvgpu_engine_enable_activity_all(struct gk20a *g);
int nvgpu_engine_disable_activity(struct gk20a *g,
			const struct nvgpu_device *dev,
			bool wait_for_idle);
int nvgpu_engine_disable_activity_all(struct gk20a *g,
				bool wait_for_idle);

int nvgpu_engine_wait_for_idle(struct gk20a *g);
#endif
#ifdef CONFIG_NVGPU_ENGINE_RESET
/**
 * Called from recovery. This will not be part of the safety build after
 * recovery is not supported in the safety build.
 */
void nvgpu_engine_reset(struct gk20a *g, u32 engine_id);
#endif
/**
 * @brief Get runlist id for the last available #NVGPU_ENGINE_ASYNC_CE
 *         engine enum type.
 *
 * @param g [in]		The GPU driver struct.
 *
 * - Call #nvgpu_engine_get_gr_runlist_id to get runlist id for the first
 *   available #NVGPU_ENGINE_GR engine type.
 * - Get #nvgpu_engine_info.runlist_id for last available #NVGPU_ENGINE_ASYNC_CE
 *   engine enum type.
 *   - Get pointer to #nvgpu_engine_info for each of #nvgpu_fifo.num_engines.
 *   - Get #nvgpu_engine_info.runlist_id for the #nvgpu_engine_info.engine_enum
 *     type matching with #NVGPU_ENGINE_ASYNC_CE.
 *
 * @return #nvgpu_engine_info.runlist_id for the last available
 *         #NVGPU_ENGINE_ASYNC_CE engine enum type.
 * @retval Return value of #nvgpu_engine_get_gr_runlist_id if #g is NULL.
 * @retval Return value of #nvgpu_engine_get_gr_runlist_id if
 *         #NVGPU_ENGINE_ASYNC_CE engine enum type is not available.
 * @retval Return value of #nvgpu_engine_get_gr_runlist_id if
 *         #nvgpu_fifo.num_engines is 0.
 */
u32 nvgpu_engine_get_fast_ce_runlist_id(struct gk20a *g);
/**
 * @brief Get runlist id for the first available #NVGPU_ENGINE_GR engine enum
 *        type.
 *
 * @param g [in]		The GPU driver struct.
 *
 * - Get h/w engine id for the first available #NVGPU_ENGINE_GR engine enum
 *   type.
 * -- Get #nvgpu_engine_info for the first available gr engine id.
 * -- Get #nvgpu_engine_info.runlist_id for first available gr engine id.
 *
 * @return #nvgpu_engine_info.runlist_id for the first available gr engine id.
 * @retval U32_MAX if #NVGPU_ENGINE_GR engine enum type is not available.
 * @retval U32_MAX if pointer to #nvgpu_engine_info for the first available
 *         gr h/w engine id is NULL.
 */
u32 nvgpu_engine_get_gr_runlist_id(struct gk20a *g);
/**
 * @brief Check if runlist id corresponds to runlist id of one of the
 *        engine ids supported by h/w.
 *
 * @param g [in]		The GPU driver struct.
 * @param runlist_id [in]	Runlist id.
 *
 * Check if #runlist_id corresponds to runlist id of one of the engine
 * ids supported by h/w by checking #nvgpu_engine_info for each of
 * #nvgpu_fifo.num_engines engines.
 *
 * @return True if #runlist_id is valid.
 * @return False if #nvgpu_engine_info is NULL for all the engine ids starting
 *         with 0 upto #nvgpu_fifo.num_engines or #runlist_id did not match with
 *         any of the runlist ids of engine ids supported by h/w.
 */
bool nvgpu_engine_is_valid_runlist_id(struct gk20a *g, u32 runlist_id);
/**
 * @brief Get mmu fault id for the engine id.
 *
 * @param g [in]		The GPU driver struct.
 * @param engine_id [in]	Engine id.
 *
 * Get pointer to #nvgpu_engine_info for the #engine_id. Use this pointer to
 * get mmu fault id for the #engine_id.
 *
 * @return Mmu fault id for #engine_id.
 * @retval Invalid mmu fault id, #NVGPU_INVALID_ENG_ID.
 */
u32 nvgpu_engine_id_to_mmu_fault_id(struct gk20a *g, u32 engine_id);
/**
 * @brief Get engine id from mmu fault id.
 *
 * @param g [in]		The GPU driver struct.
 * @param fault_id [in]		Mmu fault id.
 *
 * Check if #fault_id corresponds to fault id of one of the active engine
 * ids supported by h/w by checking #nvgpu_engine_info for each of
 * #nvgpu_fifo.num_engines engines.
 *
 * @return Valid engine id corresponding to #fault_id.
 * @retval Invalid engine id, #NVGPU_INVALID_ENG_ID if pointer to
 *         #nvgpu_engine_info is NULL for the  engine ids starting with
 *         0 upto #nvgpu_fifo.num_engines or
 *         #fault_id did not match with any of the fault ids of h/w engine ids.
 */
u32 nvgpu_engine_mmu_fault_id_to_engine_id(struct gk20a *g, u32 fault_id);
/**
 * Called from recovery. This will not be part of the safety build after
 * recovery is not supported in the safety build.
 */
u32 nvgpu_engine_get_mask_on_id(struct gk20a *g, u32 id, bool is_tsg);
/**
 * @brief Read device info h/w registers to get engine info.
 *
 * @param f [in]	Pointer to #nvgpu_fifo struct.
 *
 * - Get device info related info for h/w engine enum type,
 *   #NVGPU_DEVTYPE_GRAPHICS.
 * - Get PBDMA id serving the runlist id of the h/w engine enum type,
 *   #NVGPU_DEVTYPE_GRAPHICS.
 * - Get s/w defined engine enum type for the h/w engine enum type read
 *   from device info registers.
 * - Initialize #nvgpu_fifo.engine_info and #nvgpu_fifo.active_engines_list
 *   with the data from local device info struct and also initialize s/w
 *   variable.
 *   #nvgpu_fifo.num_engines that is used to count total number of valid h/w
 *   engine ids read from device info h/w registers.
 * - Call function to initialize CE engine info.
 *
 * @return 0 upon success.
 * @retval -EINVAL if call to function to get device info related info for
 *         h/w engine enum type, #NVGPU_DEVTYPE_GRAPHICS returned failure.
 * @retval -EINVAL if call to function to get pbdma id for runlist id of
 *         h/w engine enum type, #NVGPU_DEVTYPE_GRAPHICS returned failure.
 * @retval Return value of function called to initialize CE engine info.
 */
int nvgpu_engine_init_info(struct nvgpu_fifo *f);

/**
 * Called from recovery handling for architectures before volta. This will
 * not be part of safety build after recovery is not supported in the safety
 * build.
 */
void nvgpu_engine_get_id_and_type(struct gk20a *g, u32 engine_id,
					  u32 *id, u32 *type);
/**
 * Called from ctxsw timeout intr handling. This function will not be part
 * of safety build after recovery is not supported in the safety build.
 */
u32 nvgpu_engine_find_busy_doing_ctxsw(struct gk20a *g,
		u32 *id_ptr, bool *is_tsg_ptr);
/**
 * Called from runlist update timeout handling. This function will not be part
 * of safety build after recovery is not supported in safety build.
 */
u32 nvgpu_engine_get_runlist_busy_engines(struct gk20a *g, u32 runlist_id);

#ifdef CONFIG_NVGPU_DEBUGGER
bool nvgpu_engine_should_defer_reset(struct gk20a *g, u32 engine_id,
			u32 engine_subid, bool fake_fault);
#endif
/**
 * @brief Get veid from mmu fault id.
 *
 * @param g [in]		The GPU driver struct.
 * @param mmu_fault_id [in]	Mmu fault id.
 * @param gr_eng_fault_id [in]	GR engine's mmu fault id.
 *
 * Get valid veid by subtracting #gr_eng_fault_id from #mmu_fault_id,
 * if #mmu_fault_id is greater than or equal to #gr_eng_fault_id and less
 * than #gr_eng_fault_id + #nvgpu_fifo.max_subctx_count.
 *
 * @return Veid.
 * @retval Invalid veid, #INVAL_ID.
 */
u32 nvgpu_engine_mmu_fault_id_to_veid(struct gk20a *g, u32 mmu_fault_id,
			u32 gr_eng_fault_id);
/**
 * @brief Get engine id, veid and pbdma id from mmu fault id.
 *
 * @param g [in]		The GPU driver struct.
 * @param mmu_fault_id [in]	Mmu fault id.
 * @param engine_id [in,out]	Pointer to store active engine id.
 * @param veid [in,out]		Pointer to store veid.
 * @param pbdma_id [in,out]	Pointer to store pbdma id.
 *
 * Calls function to get h/w engine id and veid for given #mmu_fault_id.
 * If h/w (active) engine id is not #INVAL_ID, call function to get pbdma id for
 * the engine having fault id as #mmu_fault_id.
 *
 * @return Updated #engine_id, #veid and #pbdma_id pointers
 */
void nvgpu_engine_mmu_fault_id_to_eng_ve_pbdma_id(struct gk20a *g,
	u32 mmu_fault_id, u32 *engine_id, u32 *veid, u32 *pbdma_id);
/**
 * @brief Remove a device entry from engine list.
 *
 * @param g [in]		The GPU driver struct.
 * @param dev [in]		A device.
 *
 * Remove the device entry \a dev from fifo->host_engines, fifo->active_engines.
 * The device entry is retained in g->devs->devlist_heads list to ensure device
 * reset.
 */
void nvgpu_engine_remove_one_dev(struct nvgpu_fifo *f,
		const struct nvgpu_device *dev);
#endif /*NVGPU_ENGINE_H*/
