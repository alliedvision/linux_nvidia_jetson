/*
 * Copyright (c) 2014-2022, NVIDIA CORPORATION.  All rights reserved.
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
#ifndef NVGPU_TSG_H
#define NVGPU_TSG_H
/**
 * @file
 *
 * Abstract interface for TSG related functionality.
 */

#include <nvgpu/lock.h>
#include <nvgpu/kref.h>
#include <nvgpu/rwsem.h>
#include <nvgpu/list.h>
#include <nvgpu/cond.h>

/**
 * Software defined invalid TSG id value.
 */
#define NVGPU_INVALID_TSG_ID (U32_MAX)

#define NVGPU_TSG_TIMESLICE_LOW_PRIORITY_US	1300U
#define NVGPU_TSG_TIMESLICE_MEDIUM_PRIORITY_US	2600U
#define NVGPU_TSG_TIMESLICE_HIGH_PRIORITY_US	5200U
#define NVGPU_TSG_TIMESLICE_MIN_US		1000U
#define NVGPU_TSG_TIMESLICE_MAX_US		50000U
#define NVGPU_TSG_DBG_TIMESLICE_MAX_US_DEFAULT	4000000U
/**
 * Default TSG timeslice value in microseconds. Currently it is 1024 us.
 */
#define NVGPU_TSG_TIMESLICE_DEFAULT_US		(128U << 3U)

struct gk20a;
struct nvgpu_channel;
struct nvgpu_gr_ctx;
struct nvgpu_channel_hw_state;
struct nvgpu_profiler_object;
struct nvgpu_runlist;
struct nvgpu_runlist_domain;
struct nvgpu_nvs_domain;

#ifdef CONFIG_NVGPU_CHANNEL_TSG_CONTROL
enum nvgpu_event_id_type;
#endif

/** Format for reporting SM errors read from h/w registers */
struct nvgpu_tsg_sm_error_state {
	u32 hww_global_esr;
	u32 hww_warp_esr;
	u64 hww_warp_esr_pc;
	u32 hww_global_esr_report_mask;
	u32 hww_warp_esr_report_mask;
};

/**
 * Fields corresponding to TSG's s/w context.
 */
struct nvgpu_tsg {
	/** Pointer to GPU driver struct. */
	struct gk20a *g;

	/** Points to TSG's virtual memory */
	struct vm_gk20a *vm;
	/**
	 * Starting with Volta, when a Channel/TSG is set up, a recovery buffer
	 * region must be allocated in BAR2, to allow engine to save methods if
	 * it faults. Virtual memory address for this buffer is set by s/w in
	 * the channel instance block.
	 * S/w allocates memory for the #nvgpu_mem type struct for
	 * #nvgpu_fifo.num_pbdma. This is then used to alloc and map memory from
	 * BAR2 VM. Size of the actual method buffer is chip specific and
	 * calculated by s/w during TSG init.
	 */
	struct nvgpu_mem *eng_method_buffers;

	/**
	 * Pointer to graphics context buffer for this TSG. Allocated during
	 * TSG open and freed during TSG release.
	 */
	struct nvgpu_gr_ctx *gr_ctx;

	/**
	 * Mutex to prevent concurrent context initialization for channels
	 * in same TSG. All channels in one TSG share the context buffer,
	 * and only one of the channel needs to initialize the context.
	 * Rest of the channels will re-use it.
	 */
	struct nvgpu_mutex ctx_init_lock;

	/**
	 * This ref is initialized during tsg setup s/w.
	 * This is ref_get whenever a channel is bound to the TSG.
	 * This is ref_put whenever a channel is unbound from the TSG.
	 */
	struct nvgpu_ref refcount;

	/** List of channels bound to this TSG. */
	struct nvgpu_list_node ch_list;
#ifdef CONFIG_NVGPU_CHANNEL_TSG_CONTROL
	/**
	 * Ioctls using this field are not supported in the safety build.
	 */
	struct nvgpu_list_node event_id_list;
	/**
	 * Mutex used to access/modify #event_id_list.
	 * Ioctls using this field are not supported in the safety build.
	 */
	struct nvgpu_mutex event_id_list_lock;
#endif
	/**
	 * Read write type of semaphore lock used for accessing/modifying
	 * #ch_list.
	 */
	struct nvgpu_rwsem ch_list_lock;

	/**
	 * Total number of channels that are bound to a TSG. This count is
	 * incremented when a channel is bound to TSG, and decremented when
	 * channel is unbound from TSG.
	 */
	u32 ch_count;

	/**
	 * Total number of active channels that are bound to a TSG. This count
	 * is incremented when a channel bound to TSG is added into the runlist
	 * under the same TSG header. Count is decremented when channel bound
	 * to TSG is removed from the runlist. This count is specifically
	 * tracked for runlist construction of TSG entry.
	 */
	u32 num_active_channels;

	/**
	 * This is for timeout amount for the TSG's timslice.
	 * All channels in a TSG share the same runlist timeslice
	 * which specifies how long a single context runs on an engine
	 * or PBDMA before being swapped for a different context.
	 * The timeslice period is set in the TSG header of runlist entry
	 * defined by h/w.
	 * The timeslice period should normally not be set to zero. A timeslice
	 * of zero will be treated as a timeslice period of 1 ns (Bug 1652173).
	 * The runlist timeslice period begins after the context has been
	 * loaded on a PBDMA but is paused while the channel has an outstanding
	 * context load to an engine.  Time spent switching a context into an
	 * engine is not part of the runlist timeslice.
	 */
	unsigned int timeslice_us;

	/**
	 * Interleave level decides the number of entries of this TSG in the
	 * runlist.
	 * Refer #NVGPU_FIFO_RUNLIST_INTERLEAVE_LEVEL_LOW.
	 */
	u32 interleave_level;
	/** TSG identifier, ranges from 0 to #nvgpu_fifo.num_channels. */
	u32 tsgid;

	/**
	 * Runlist this TSG will be assigned to.
	 */
	struct nvgpu_runlist *runlist;

	/**
	 * Runlist domain this TSG is bound to. Bound with an ioctl, initially the default domain.
	 */
	struct nvgpu_runlist_domain *rl_domain;

	/*
	 * A TSG keeps a ref to its scheduling domain so that active domains can't be deleted.
	 */
	struct nvgpu_nvs_domain *nvs_domain;

	/** tgid (OS specific) of the process that openend the TSG. */

	/**
	 * Thread Group identifier (OS specific) of the process that openend
	 * the TSG.
	 */
	pid_t tgid;
	/**
	 * Set to true if tsgid is acquired else set to false.
	 * This is protected by #nvgpu_fifo.tsg_inuse_mutex. Acquire/Release
	 * the mutex to check if tsgid is already acquired or not.
	 */
	bool in_use;
	/**
	 * This will indicate if TSG can be aborted. Non abortable TSG is for
	 * vidmem clear.
	 */
	bool abortable;

	/** MMU debug mode enabled if mmu_debug_mode_refcnt > 0 */
	u32  mmu_debug_mode_refcnt;

	/**
	 * Pointer to store SM errors read from h/w registers.
	 * Check #nvgpu_tsg_sm_error_state.
	 */
	struct nvgpu_tsg_sm_error_state *sm_error_states;

#ifdef CONFIG_NVGPU_DEBUGGER
#define NVGPU_SM_EXCEPTION_TYPE_MASK_NONE		(0x0U)
#define NVGPU_SM_EXCEPTION_TYPE_MASK_FATAL		(0x1U << 0)
	u32 sm_exception_mask_type;
	struct nvgpu_mutex sm_exception_mask_lock;
#endif

#ifdef CONFIG_NVGPU_PROFILER
	/** Pointer of profiler object to which this TSG is bound */
	struct nvgpu_profiler_object *prof;
#endif
};

/**
 * @brief Initialize given TSG
 *
 * @param g [in]		The GPU driver struct.
 * @param pid [in]		The PID of the process.
 *
 * - Set s/w context of the given TSG.
 * - Update given TSG struct with init pointers.
 *
 * @return 0 in case of success, < 0 in case of failure.
 * @retval -EINVAL if there is no SM.
 * @retval -ENOMEM if not enough memory is available.
 */
int nvgpu_tsg_open_common(struct gk20a *g, struct nvgpu_tsg *tsg, pid_t pid);
/**
 * @brief Open and initialize unused TSG
 *
 * @param g [in]		The GPU driver struct.
 * @param pid [in]		The PID of the process.
 *
 * - Acquire unused TSG.
 * - Set s/w context of the acquired TSG.
 *
 * @return Pointer to  TSG struct. See #nvgpu_tsg .
 * @retval NULL if there is no un-used TSG.
 * @retval NULL if setting s/w context for opened TSG failed. If setting s/w
 *         context failed, release acquired TSG back to the pool of unused
 *         TSGs.
 */
struct nvgpu_tsg *nvgpu_tsg_open(struct gk20a *g, pid_t pid);

/**
 * @brief Clean up resources used by tsg. This is needed for releasing TSG.
 *
 * @param g [in]		The GPU driver struct.
 * @param tsg [in]		Pointer to TSG struct.
 *
 * - Call non-NULL HAL to release tsg. This HAL is non-NULL for vgpu only.
 * - Call nvgpu_free_gr_ctx_struct to free #nvgpu_tsg.gr_ctx.
 * - Set #nvgpu_tsg.gr_ctx to NULL.
 * - If #nvgpu_tsg.vm is non-NULL, do #nvgpu_vm_put for this vm and set
 *   it to NULL (Unhook TSG from VM).
 * - If #nvgpu_tsg.sm_error_states is non-NULL, free allocated memory and set
 *   it to NULL.
 */
void nvgpu_tsg_release_common(struct gk20a *g, struct nvgpu_tsg *tsg);

/**
 * @brief Release TSG to the pool of free TSGs.
 *
 * @param ref [in]	Pointer to #nvgpu_tsg.refcount.
 *
 * - Get pointer to the #nvgpu_tsg using #ref.
 * - Call HAL to free #nvgpu_tsg.gr_ctx if this memory pointer is non-NULL
 *   and valid and also #nvgpu_tsg.vm is non-NULL.
 * - Unhook all events created on the TSG being released.
 * -- Acquire #nvgpu_tsg.event_id_list_lock.
 * -- While #nvgpu_tsg.event_id_list is non-empty,
 * ---  Delete #nvgpu_tsg.event_id_list.next.
 * -- Release #nvgpu_tsg.event_id_list_lock.
 * - Call #nvgpu_tsg_release_common.
 * - Set #nvgpu_tsg.in_use to false so that tsg can be made available
 *   to the pool of unused tsgs.
 */
void nvgpu_tsg_release(struct nvgpu_ref *ref);

/**
 * @brief Initialize s/w context for TSGs.
 *
 * @param g [in]		The GPU driver struct.
 *
 * - Allocate zero initialized kernel memory area for #nvgpu_fifo.num_channels
 *     number of #nvgpu_fifo.tsg struct. This area of memory is indexed by
 *     tsgid starting from 0 to #nvgpu_fifo.num_channels.
 * - Upon successful allocation of memory, initialize memory area assigned to
 *     each TSG with s/w defaults.
 *
 * @return 0 for successful init, < 0 for failure.
 * @retval -ENOMEM if kernel memory could not be allocated to support TSG
 *         s/w context.
 */
int nvgpu_tsg_setup_sw(struct gk20a *g);

/**
 * @brief De-initialize s/w context for TSGs.
 *
 * @param g [in]		The GPU driver struct.
 *
 * - Destroy s/w context for all tsgid starting from 0 to
 *     #nvgpu_fifo.num_channels.
 * - De-allocate kernel memory area allocated to support s/w context of
 *     #nvgpu_fifo.num_channels number of TSGs.
 */
void nvgpu_tsg_cleanup_sw(struct gk20a *g);

/**
 * @brief Get pointer to #nvgpu_tsg for the tsgid of the given Channel.
 *
 * @param ch [in]		Pointer to Channel struct.
 *
 * Validate tsgid of the given channel. If tsgid is not equal to
 * #NVGPU_INVALID_TSG_ID, get pointer to area of memory, reserved for s/w
 * context of TSG and indexed by tsgid.
 *
 * @note   This does not check if tsgid is < num_channels.
 * @return Pointer to #nvgpu_tsg struct.
 * @retval NULL if tsgid of the given channel is #NVGPU_INVALID_TSG_ID.
 */
struct nvgpu_tsg *nvgpu_tsg_from_ch(struct nvgpu_channel *ch);

/**
 * @brief Disable all the channels bound to a TSG.
 *
 * @param tsg [in]		Pointer to TSG struct.
 *
 * Disable all the channels bound to a TSG so that h/w scheduler does not
 * schedule these channels.
 */
void nvgpu_tsg_disable(struct nvgpu_tsg *tsg);

/**
 * @brief Bind a channel to the TSG.
 *
 * @param tsg [in]		Pointer to TSG struct.
 * @param ch [in]		Pointer to Channel struct.
 *
 * - Make sure channel is not already bound to a TSG.
 * - Make sure channel is not part of any runlists.
 * - If channel had ASYNC subctx id, then set runqueue selector to 1.
 * - Set runlist id of TSG to channel's runlist_id if runlist_id of TSG
 *   is set to #NVGPU_INVALID_TSG_ID.
 * - Call HAL to bind channel to TSG.
 * - Add channel to TSG's list of channels. See #nvgpu_tsg.ch_list
 * - Set #nvgpu_channel.tsgid to #nvgpu_tsg.tsgid.
 * - Set #nvgpu_channel.unserviceable to false to mark that channel is
 *   serviceable.
 * - Bind Engine Method Buffers (This may not be required for all the chips).
 * - Get #nvgpu_tsg.refcount to prevent TSG from being freed till channel/s are
 *   bound to this TSG.
 *
 * @return 0 for successful bind, < 0 for failure.
 * @retval -EINVAL if channel is already bound to a TSG.
 * @retval -EINVAL if channel is already active. This is done by checking if
 *          bit corresponding to chid is set in the
 *          #nvgpu_runlist_info.active_channels of any of the supported
 *          #nvgpu_fifo.num_runlists.
 * @retval -EINVAL if runlist_id of the channel and tsg do not match.
 */
int nvgpu_tsg_bind_channel(struct nvgpu_tsg *tsg,
			struct nvgpu_channel *ch);

#ifdef CONFIG_NVS_PRESENT
/**
 * @brief Bind a TSG to a domain.
 *
 * @param tsg [in]		Pointer to TSG struct.
 * @param nnvs_domain [in]	Pointer to nvgpu nvs domain.
 *
 * Make this TSG participate in the given domain, such that it can only be
 * seen by runlist HW when the domain has been scheduled in.
 *
 * The TSG must have no channels at this point.
 *
 * @return 0 for successful bind, < 0 for failure.
 */
int nvgpu_tsg_bind_domain(struct nvgpu_tsg *tsg, struct nvgpu_nvs_domain *nnvs_domain);
#endif

/**
 * @brief Get pointer to #nvgpu_tsg for the tsgid.
 *
 * @param g [in]		The GPU driver struct.
 * @param tsgid [in]		Id of the TSG.
 *
 * Get pointer to area of memory, reserved for s/w context of TSG
 * and indexed by tsgid.
 *
 * @return Pointer to #nvgpu_tsg struct.
 */
struct nvgpu_tsg *nvgpu_tsg_get_from_id(struct gk20a *g, u32 tsgid);

/**
 * @brief Validate tsgid and get pointer to #nvgpu_tsg for this tsgid.
 *
 * @param g [in]		The GPU driver struct.
 * @param tsgid [in]		Id of the TSG.
 *
 * If tsgid is not equal to #NVGPU_INVALID_TSG_ID, get pointer to area of
 * memory, reserved for s/w context of TSG and indexed by tsgid.
 *
 * @return Pointer to #nvgpu_tsg struct.
 * @retval NULL if tsgid is #NVGPU_INVALID_TSG_ID.
 */
struct nvgpu_tsg *nvgpu_tsg_check_and_get_from_id(struct gk20a *g, u32 tsgid);

/**
 * @brief Unbind a channel from the TSG it is bound to.
 *
 * @param tsg [in]			Pointer to TSG struct.
 * @param ch [in]			Pointer to Channel struct.
 * @param force [in]			If set, unbind proceeds if the channel
 *					is busy.
 *
 * Unbind channel from TSG:
 *  - Check if channel being unbound has become unserviceable.
 *  - Disable TSG.
 *  - Preempt TSG.
 *  - Check hw state of the channel.
 *  - If NEXT bit is set and force is set to true, perform error
 *    handling steps given next.
 *  - If NEXT bit is set and force is set to false, caller will
 *    have to retry unbind.
 *  - Remove channel from its runlist.
 *  - Remove channel from TSG's channel list.
 *  - Set tsgid of the channel to #NVGPU_INVALID_TSG_ID.
 *  - Disable channel so that it is not picked up by h/w scheduler.
 *  - Enable TSG if it is still serviceable. TSG becomes unserviceable
 *    if channel being unbound has become unserviceable.
 *  - Do clean up for aborting channel.
 * If an error occurred during previous steps:
 *  - Call #nvgpu_tsg_abort to abort the tsg.
 *  - Call #nvgpu_channel_update_runlist to remove the channel from the runlist.
 *  - Acquire #nvgpu_tsg.ch_list_lock of the tsg and delete channel from
 *    #nvgpu_tsg.ch_list.
 *  - Set #nvgpu_channel.tsgid to #NVGPU_INVALID_TSG_ID
 *  - Release #nvgpu_tsg.ch_list_lock of the tsg.
 * Call non NULL HAL to unbind channel from the tsg. This HAL is vgpu specific
 * and does not apply for non-vgpu.
 * Release #nvgpu_tsg.refcount and call #nvgpu_tsg_release if refcount
 * becomes 0.
 *
 * @return 0
 * @note Caller of this function must make sure that channel requested to be
 *       unbound from the TSG is bound to the TSG.
 */
int nvgpu_tsg_unbind_channel(struct nvgpu_tsg *tsg, struct nvgpu_channel *ch,
			     bool force);

/**
 * @brief Check h/w channel status before unbinding Channel.
 *
 * @param tsg [in]		Pointer to TSG struct.
 * @param ch [in]		Pointer to Channel struct.
 *
 * - Call HAL to read chip specific h/w channel status register into hw_state
 *   local variable.
 * - If next bit is not set in hw_state,
 * -- Call HAL (if supported for the chip) to check ctx_reload bit in hw_state.
 * --- If set, move ctx_reload h/w
 *     state to some other channel's h/w status register. New channel id should
 *     be different than the channel requested to be unbound.
 * -- Call HAL (if supported for the chip) to check eng_faulted bit in hw_state.
 * --- If set, clear the CE method buffer in #ASYNC_CE_RUNQUE index of
 *     #nvgpu_tsg.eng_method_buffers of the tsg that the channel being unbound
 *     is bound to.
 *
 * @return 0 in case of success and < 0 in case of failure.
 * @retval -EINVAL if next bit is set in hw_state.
 */
int nvgpu_tsg_unbind_channel_check_hw_state(struct nvgpu_tsg *tsg,
		struct nvgpu_channel *ch);

/**
 * @brief Find another channel in the TSG and force ctx reload if
 *        h/w channel status of the channel is set to ctx_reload.
 *
 * @param tsg [in]		Pointer to TSG struct.
 * @param ch [in]		Pointer to Channel struct.
 * @param hw_state [in]		Pointer to nvgpu_channel_hw_state struct.
 *
 * Find another channel in the TSG and force ctx reload.
 *
 * @note If there is only one channel in this TSG then function will not find
 *       another channel to force ctx reload.
 */
void nvgpu_tsg_unbind_channel_check_ctx_reload(struct nvgpu_tsg *tsg,
		struct nvgpu_channel *ch,
		struct nvgpu_channel_hw_state *hw_state);

#ifdef CONFIG_NVGPU_CHANNEL_TSG_CONTROL
int nvgpu_tsg_force_reset_ch(struct nvgpu_channel *ch,
				u32 err_code, bool verbose);
void nvgpu_tsg_post_event_id(struct nvgpu_tsg *tsg,
			     enum nvgpu_event_id_type event_id);
#endif
/**
 * @brief Set mmu fault error notifier for all the channels bound to a TSG.
 *
 * @param g [in]		The GPU driver struct.
 * @param tsg [in]		Pointer to TSG struct.
 *
 * Set mmu fault error notifier for all the channels bound to the TSG.
 */
void nvgpu_tsg_set_ctx_mmu_error(struct gk20a *g,
		struct nvgpu_tsg *tsg);

/**
 * @brief Mark error for all the referenceable channels of tsg's channel list.
 *
 * @param g [in]		The GPU driver struct.
 * @param tsg [in]		Pointer to TSG struct.
 *
 * - Set verbose local variable to false.
 * - Acquire #nvgpu_tsg.ch_list_lock of the tsg.
 * - For each entry of the channels in #nvgpu_tsg.ch_list of the tsg,
 * -- Get reference to the channel.
 * -- If channel is referenceable,
 * --- Call #nvgpu_channel_mark_error and set verbose local variable to true
 *     if return value of this function is true.
 * --- Put reference to the channel.
 * - Release #nvgpu_tsg.ch_list_lock of the tsg.
 *
 * @return verbose bool variable. This is used to decide if driver needs to dump
 *         debug info. This can be either true or false.
 */
bool nvgpu_tsg_mark_error(struct gk20a *g, struct nvgpu_tsg *tsg);

#ifdef CONFIG_NVGPU_KERNEL_MODE_SUBMIT
bool nvgpu_tsg_check_ctxsw_timeout(struct nvgpu_tsg *tsg,
		bool *debug_dump, u32 *ms);
#endif
#ifdef CONFIG_NVGPU_CHANNEL_TSG_SCHEDULING
int nvgpu_tsg_set_timeslice(struct nvgpu_tsg *tsg, u32 timeslice_us);
u32 nvgpu_tsg_get_timeslice(struct nvgpu_tsg *tsg);
int nvgpu_tsg_set_long_timeslice(struct nvgpu_tsg *tsg, u32 timeslice_us);
int nvgpu_tsg_set_priority(struct gk20a *g, struct nvgpu_tsg *tsg,
				u32 priority);
int nvgpu_tsg_set_interleave(struct nvgpu_tsg *tsg, u32 level);
#endif
/**
 * @brief Get default TSG timeslice in us as defined by nvgpu driver.
 *
 * @param g [in]		The GPU driver struct.
 *
 * This function returns TSG timeslice value. This is the default timeslice
 * value in us as defined by s/w.
 *
 * @return S/w defined default TSG timeslice value in us.
 */
u32 nvgpu_tsg_default_timeslice_us(struct gk20a *g);

/**
 * @brief Allocate zero initialized memory to store SM errors.
 *
 * @param g [in]		The GPU driver struct.
 * @param tsg [in]		Pointer to the TSG struct.
 * @param num_sm [in]		Total number of SMs supported by h/w.
 *
 * Allocate zero initialized memory to #nvgpu_tsg_sm_error_state, which stores
 * SM errors for all the SMs supported by h/w.
 *
 * @return 0 in case of success, < 0 in case of failure.
 * @retval -EINVAL if memory is already allocated to store
 *         SM error states.
 * @retval -ENOMEM if memory could not be allocated to store
 *         SM error states.
 */
int nvgpu_tsg_alloc_sm_error_states_mem(struct gk20a *g,
					struct nvgpu_tsg *tsg,
					u32 num_sm);

/**
 * @brief Mark for all the referenceable channels of tsg's channel
 *        list as unserviceable.
 *
 * @param g [in]		The GPU driver struct.
 * @param tsg [in]		Pointer to TSG struct.
 *
 * The channels are set unserviceable to convey that an uncorrectable
 * error has occurred on the channel. It is not possible to take more
 * references on this channel and only available option for userspace
 * is to close the channel fd.
 * - Acquire #nvgpu_tsg.ch_list_lock of the tsg.
 * - For each entry of the channels in #nvgpu_tsg.ch_list of the tsg,
 * -- Get reference to the channel.
 * -- If channel is referenceable,
 * --- Call #nvgpu_channel_set_unserviceable.
 * --- Put reference to the channel.
 * - Release #nvgpu_tsg.ch_list_lock of the tsg.
 *
 */
void nvgpu_tsg_set_unserviceable(struct gk20a *g, struct nvgpu_tsg *tsg);

/**
 * @brief Release waiters for all the referenceable channels of tsg's
 *        channel list.
 *
 * @param g [in]		The GPU driver struct.
 * @param tsg [in]		Pointer to TSG struct.
 *
 * Unblock pending waits on this channel (semaphore and error
 * notifier wait queues)
 * - Acquire #nvgpu_tsg.ch_list_lock of the tsg.
 * - For each entry of the channels in #nvgpu_tsg.ch_list of the tsg,
 * -- Get reference to the channel.
 * -- If channel is referenceable,
 * --- Call #nvgpu_channel_wakeup_wqs.
 * --- Put reference to the channel.
 * - Release #nvgpu_tsg.ch_list_lock of the tsg.
 *
 */
void nvgpu_tsg_wakeup_wqs(struct gk20a *g, struct nvgpu_tsg *tsg);

/** @brief store error info for SM error state
 *
 * @param tsg [in]		Pointer to the TSG struct.
 * @param sm_id [in]	index of SM
 * @param hww_global_esr [in]	hww_global_esr reg value
 * @param hww_warp_esr [in]	hww_warp_esr register value
 * @param hww_warp_esr_pc [in]	PC value of hww_warp_esr
 * @param hww_global_esr_report_mask [in]	hww_global_esr_report_mask
 * @param hww_warp_esr_report_mask [in]	hww_warp_esr_report_mask
 *
 * Allocate zero initialized memory to #nvgpu_tsg_sm_error_state, which stores
 * SM errors for all the SMs supported by h/w.
 *
 * @return 0 in case of success, < 0 in case of failure.
 * @retval -EINVAL if memory is already allocated to store
 *         SM error states.
 * @retval -ENOMEM if memory could not be allocated to store
 *         SM error states.
 */
int nvgpu_tsg_store_sm_error_state(struct nvgpu_tsg *tsg, u32 sm_id,
		u32 hww_global_esr, u32 hww_warp_esr, u64 hww_warp_esr_pc,
		u32 hww_global_esr_report_mask, u32 hww_warp_esr_report_mask);

/**
 * @brief retrieve pointer to nvgpu_tsg_get_sm_error_state
 *
 * @param tsg [in]		Pointer to the TSG struct.
 * @param sm_id [in]	index of SM
 *
 * Retrieve a pointer to a struct nvgpu_tsg_get_sm_error_state for
 * the index sm_id.
 *
 * @retval NULL if sm_id is incorrect or no memory was allocated for storing
 *         SM error states.
 * @retval pointer to a constant struct nvgpu_tsg_sm_error_state
 */
const struct nvgpu_tsg_sm_error_state *nvgpu_tsg_get_sm_error_state(
		struct nvgpu_tsg *tsg, u32 sm_id);

#ifdef CONFIG_NVGPU_DEBUGGER
int nvgpu_tsg_set_sm_exception_type_mask(struct nvgpu_channel *ch,
		u32 exception_mask);
#endif

#ifdef CONFIG_NVGPU_CHANNEL_TSG_CONTROL
struct gk20a_event_id_data {
	struct gk20a *g;

	int id; /* ch or tsg */
	int pid;
	u32 event_id;

	bool event_posted;

	struct nvgpu_cond event_id_wq;
	struct nvgpu_mutex lock;
	struct nvgpu_list_node event_id_node;
};

static inline struct gk20a_event_id_data *
gk20a_event_id_data_from_event_id_node(struct nvgpu_list_node *node)
{
	return (struct gk20a_event_id_data *)
		((uintptr_t)node - offsetof(struct gk20a_event_id_data, event_id_node));
};
#endif

/**
 * @brief Set error notifier for all the channels bound to a TSG.
 *
 * @param g [in]		The GPU driver struct.
 * @param tsg [in]		Pointer to TSG struct.
 * @param error_notifier [in]	Error notifier defined by s/w.
 *
 * For each channel bound to given TSG, set given error notifier.
 * See include/nvgpu/error_notifier.h.
 */
void nvgpu_tsg_set_error_notifier(struct gk20a *g, struct nvgpu_tsg *tsg,
		u32 error_notifier);

#ifdef CONFIG_NVGPU_KERNEL_MODE_SUBMIT
bool nvgpu_tsg_ctxsw_timeout_debug_dump_state(struct nvgpu_tsg *tsg);
void nvgpu_tsg_set_ctxsw_timeout_accumulated_ms(struct nvgpu_tsg *tsg, u32 ms);
#endif

/**
 * @brief Abort all the channels bound to the TSG.
 *
 * @param g [in]		The GPU driver struct.
 * @param tsg [in]		Pointer to TSG struct.
 * @param preempt [in]		Flag to ask for preempting TSG.
 *
 * - Disable all the channels bound to the #tsg so that h/w does not schedule
 *   them.
 * - Preempt #tsg if #preempt flag is set. This is to offload all the channels
 *   bound to the #tsg from the pbdma/engines.
 * - Set #nvgpu_channel.unserviceable of all the channels bound to the #tsg
 *   to let s/w know of bad state of channels.
 * - Do s/w formalities so that channels bound to the #tsg are ready to be
 *   closed by userspace.
 */
void nvgpu_tsg_abort(struct gk20a *g, struct nvgpu_tsg *tsg, bool preempt);

/**
 * @brief Clear h/w bits PBDMA_FAULTED and ENG_FAULTED in CCSR channel h/w
 *        register for all the channels bound to the TSG.
 *
 * @param g [in]		The GPU driver struct.
 * @param tsg [in]		Pointer to TSG struct.
 * @param eng [in]		Flag to ask for clearing ENG_FAULTED h/w bit.
 * @param pbdma [in]		Flag to ask for clearing PBDMA_FAULTED h/w bit.
 *
 * If chip supports the h/w bits PBDMA_FAULTED and ENG_FAULTED and tsg
 * is non-NULL, clear PBDMA_FAULTED bit in CCSR channel h/w register if #pbdma
 * is set, clear ENG_FAULTED bit in CCSR channel h/w register if #eng is set.
 * For chips that do not support these h/w bits, just return. Also return if
 * #tsg input param is NULL.
 */
void nvgpu_tsg_reset_faulted_eng_pbdma(struct gk20a *g, struct nvgpu_tsg *tsg,
		bool eng, bool pbdma);
#ifdef CONFIG_NVGPU_DEBUGGER
int nvgpu_tsg_set_mmu_debug_mode(struct nvgpu_channel *ch, bool enable);
#endif
#endif /* NVGPU_TSG_H */
