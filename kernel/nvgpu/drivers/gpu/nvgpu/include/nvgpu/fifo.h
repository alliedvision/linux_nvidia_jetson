/*
 * FIFO common definitions.
 *
 * Copyright (c) 2011-2021, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef NVGPU_FIFO_COMMON_H
#define NVGPU_FIFO_COMMON_H

/**
 * @file
 * @page unit-fifo Unit FIFO
 *
 * Overview
 * ========
 *
 * The FIFO unit is responsible for managing xxxxx.
 * primarily of TODO types:
 *
 *   + TODO
 *   + TODO
 *
 * The FIFO code also makes sure that all of the necessary SW and HW
 * initialization for engines, pdbma, runlist, channel and tsg subsystems
 * are taken care of before the GPU begins executing work.
 *
 * Top level FIFO Unit
 * ---------------------
 *
 * The FIFO unit TODO.
 *
 *   + include/nvgpu/fifo.h
 *   + include/nvgpu/gops/fifo.h
 *
 * Runlist
 * -------
 *
 * TODO
 *
 *   + include/nvgpu/runlist.h
 *   + include/nvgpu/gops/runlist.h
 *
 * Pbdma
 * -------
 *
 * TODO
 *
 *   + include/nvgpu/pbdma.h
 *   + include/nvgpu/pbdma_status.h
 *
 * Engines
 * -------
 *
 * TODO
 *
 *   + include/nvgpu/engines.h
 *   + include/nvgpu/engine_status.h
 *   + include/nvgpu/gops/engine.h
 *
 * Preempt
 * -------
 *
 * TODO
 *
 *   + include/nvgpu/preempt.h
 *
 * Channel
 * -------
 *
 * TODO
 *
 *   + include/nvgpu/channel.h
 *   + include/nvgpu/gops/channel.h
 *
 * Tsg
 * -------
 *
 * TODO
 *
 *   + include/nvgpu/tsg.h
 *
 * RAM
 * -------
 *
 * TODO
 *
 *   + include/nvgpu/gops/ramin.h
 *   + include/nvgpu/gops/ramfc.h
 *
 * Sync
 * ----
 *
 *   + include/nvgpu/channel_sync.h
 *   + include/nvgpu/channel_sync_syncpt.h
 *   + include/nvgpu/gops/sync.h
 *
 * Usermode
 * --------
 *
 * TODO
 *
 *   + include/nvgpu/gops/usermode.h
 *
 *
 * Data Structures
 * ===============
 *
 * The major data structures exposed to users of the FIFO unit in nvgpu relate
 * to managing Engines, Runlists, Channels and Tsgs.
 * Following is a list of these structures:
 *
 *   + struct nvgpu_fifo
 *
 *       TODO
 *
 *   + struct nvgpu_runlist
 *
 *       TODO
 *
 *   + struct nvgpu_channel
 *
 *       TODO
 *
 *   + struct nvgpu_tsg
 *
 *       TODO
 *
 * Static Design
 * =============
 *
 * Details of static design.
 *
 * Resource utilization
 * --------------------
 *
 * External APIs
 * -------------
 *
 *   + TODO
 *
 *
 * Supporting Functionality
 * ========================
 *
 * There's a fair amount of supporting functionality:
 *
 *   + TODO
 *     - TODO
 *   + TODO
 *   + TODO
 *     # TODO
 *     # TODO
 *
 * Documentation for this will be filled in!
 *
 * Dependencies
 * ------------
 *
 * Dynamic Design
 * ==============
 *
 * Use case descriptions go here. Some potentials:
 *
 *   - TODO
 *   - TODO
 *   - TODO
 *
 * Open Items
 * ==========
 *
 * Any open items can go here.
 */

#include <nvgpu/types.h>
#include <nvgpu/lock.h>
#include <nvgpu/kref.h>
#include <nvgpu/list.h>
#include <nvgpu/swprofile.h>

/**
 * H/w defined value for Channel ID type
 */
#define ID_TYPE_CHANNEL			0U
/**
 * H/w defined value for Tsg ID type
 */
#define ID_TYPE_TSG			1U
/**
 * S/w defined value for Runlist ID type
 */
#define ID_TYPE_RUNLIST			2U
/**
 * S/w defined value for unknown ID type.
 */
#define ID_TYPE_UNKNOWN			(~U32(0U))
/**
 * Invalid ID.
 */
#define INVAL_ID			(~U32(0U))
/**
 * Timeout after which ctxsw timeout interrupt (if enabled by s/w) will be
 * triggered by h/w if context fails to context switch.
 */
#define CTXSW_TIMEOUT_PERIOD_MS		100U

/** Subctx id 0 */
#define CHANNEL_INFO_VEID0		0U

struct gk20a;
struct nvgpu_runlist;
struct nvgpu_channel;
struct nvgpu_tsg;
struct nvgpu_swprofiler;

struct nvgpu_fifo {
	/** Pointer to GPU driver struct. */
	struct gk20a *g;

	/** Number of channels supported by the h/w. */
	unsigned int num_channels;

	/** Runlist entry size in bytes as supported by h/w. */
	unsigned int runlist_entry_size;

	/** Number of runlist entries per runlist as supported by the h/w. */
	unsigned int num_runlist_entries;

	/**
	 * Array of pointers to the engines that host controls. The size is
	 * based on the GPU litter value HOST_NUM_ENGINES. This is indexed by
	 * engine ID. That is to say, if you want to get a device that
	 * corresponds to engine ID, E, then host_engines[E] will give you a
	 * pointer to that device.
	 *
	 * If a given element is NULL, that means that there is no engine for
	 * the given engine ID. This is expected for chips that do not populate
	 * the full set of possible engines for a given family of chips. E.g
	 * a GV100 has a lot more engines than a gv11b.
	 */
	const struct nvgpu_device **host_engines;

	/**
	 * Total number of engines supported by the chip family. See
	 * #host_engines above.
	 */
	u32 max_engines;

	/**
	 * The list of active engines; it can be (and often is) smaller than
	 * #host_engines. This list will have exactly #num_engines engines;
	 * use #num_engines to iterate over the list of devices with a for-loop.
	 */
	const struct nvgpu_device **active_engines;

	/**
	 * Length of the #active_engines array.
	 */
	u32 num_engines;

	/**
	 * Pointers to runlists, indexed by real hw runlist_id.
	 * If a runlist is active, then runlists[runlist_id] points
	 * to one entry in active_runlist_info. Otherwise, it is NULL.
	 */
	struct nvgpu_runlist **runlists;
	/** Number of runlists supported by the h/w. */
	u32 max_runlists;

	/** Array of actual HW runlists that are present on the GPU. */
	struct nvgpu_runlist *active_runlists;
	/** Number of active runlists. */
	u32 num_runlists;

	struct nvgpu_swprofiler kickoff_profiler;
	struct nvgpu_swprofiler recovery_profiler;
	struct nvgpu_swprofiler eng_reset_profiler;

#ifdef CONFIG_NVGPU_USERD
	struct nvgpu_mutex userd_mutex;
	struct nvgpu_mem *userd_slabs;
	u32 num_userd_slabs;
	u32 num_channels_per_slab;
	u64 userd_gpu_va;
#endif

	/**
	 * Number of channels in use. This is incremented by one when a
	 * channel is opened and decremented by one when a channel is closed by
	 * userspace.
	 */
	unsigned int used_channels;
	/**
	 * This is the zero initialized area of memory allocated by kernel for
	 * storing channel specific data i.e. #nvgpu_channel struct info for
	 * #num_channels number of channels.
	 */
	struct nvgpu_channel *channel;
	/** List of channels available for allocation */
	struct nvgpu_list_node free_chs;
	/**
	 * Lock used to read and update #free_chs list. Channel entry is
	 * removed when a channel is opened and added back to the #free_ch list
	 * when channel is closed by userspace.
	 * This lock is also used to protect #used_channels.
	 */
	struct nvgpu_mutex free_chs_mutex;

	/** Lock used to prevent multiple recoveries. */
	struct nvgpu_mutex engines_reset_mutex;

	/** Lock used to update h/w runlist registers for submitting runlist. */
	struct nvgpu_spinlock runlist_submit_lock;

	/**
	 * This is the zero initialized area of memory allocated by kernel for
	 * storing TSG specific data i.e. #nvgpu_tsg struct info for
	 * #num_channels number of TSG.
	 */
	struct nvgpu_tsg *tsg;
	/**
	 * Lock used to read and update #nvgpu_tsg.in_use. TSG entry is
	 * in use when a TSG is opened and not in use when TSG is closed
	 * by userspace. Refer #nvgpu_tsg.in_use in tsg.h.
	 */
	struct nvgpu_mutex tsg_inuse_mutex;

	/**
	 * Pointer to a function that will be executed when FIFO support
	 * is requested to be removed. This is supposed to clean up
	 * all s/w resources used by FIFO module e.g. Channel, TSG, PBDMA,
	 * Runlist, Engines and USERD.
	 */
	void (*remove_support)(struct nvgpu_fifo *f);

	/**
	 * nvgpu_fifo_setup_sw is skipped if this flag is set to true.
	 * This gets set to true after successful completion of
	 * nvgpu_fifo_setup_sw.
	 */
	bool sw_ready;

	/** FIFO interrupt related fields. */
	struct nvgpu_fifo_intr {
		/** Share info between isr and non-isr code. */
		struct nvgpu_fifo_intr_isr {
			/** Lock for fifo isr. */
			struct nvgpu_mutex mutex;
		} isr;
		/** PBDMA interrupt specific data. */
		struct nvgpu_fifo_intr_pbdma {
			/** H/w specific unrecoverable PBDMA interrupts. */
			u32 device_fatal_0;
			/**
			 * H/w specific recoverable PBDMA interrupts that are
			 * limited to channels. Fixing and clearing the
			 * interrupt will allow PBDMA to continue.
			 */
			u32 channel_fatal_0;
			/** H/w specific recoverable PBDMA interrupts. */
			u32 restartable_0;
		} pbdma;
	} intr;

#ifdef CONFIG_NVGPU_DEBUGGER
	unsigned long deferred_fault_engines;
	bool deferred_reset_pending;
	struct nvgpu_mutex deferred_reset_mutex;
#endif

	/** Max number of sub context i.e. veid supported by the h/w. */
	u32 max_subctx_count;
	/** Used for vgpu. */
	u32 channel_base;
};

static inline const char *nvgpu_id_type_to_str(unsigned int id_type)
{
	const char *str = NULL;

	switch (id_type) {
	case ID_TYPE_CHANNEL:
		str = "Channel";
		break;
	case ID_TYPE_TSG:
		str = "TSG";
		break;
	case ID_TYPE_RUNLIST:
		str = "Runlist";
		break;
	default:
		str = "Unknown";
		break;
	}

	return str;
}

/**
 * @brief Initialize FIFO unit.
 *
 * @param g [in]	The GPU driver struct.
 *                      - The function does not perform g parameter validation.
 *
 * - Invoke \ref gops_fifo.setup_sw(g) to initialize nvgpu_fifo variables
 * and sub-modules. In case of failure, return and propagate the resulting
 * error code.
 * - Check if \ref gops_fifo.init_fifo_setup_hw(g) is initialized.
 * If yes, invoke \ref gops_fifo.init_fifo_setup_hw(g) to handle FIFO unit
 * h/w setup. In case of failure, use \ref nvgpu_fifo_cleanup_sw_common(g)
 * to clear FIFO s/w metadata.
 *
 * @retval 0		in case of success.
 * @retval -ENOMEM	in case there is not enough memory available.
 * @retval <0		other unspecified errors.
 */
int nvgpu_fifo_init_support(struct gk20a *g);

/**
 * @brief Initialize FIFO software metadata and mark it ready to be used.
 *
 * @param g [in]	The GPU driver struct.
 *                      - The function does not perform g parameter validation.
 *
 * - Check if #nvgpu_fifo.sw_ready is set to true i.e. s/w setup is already done
 * (pointer to nvgpu_fifo is obtained using g->fifo). In case setup is ready,
 * return 0, else continue to setup.
 * - Invoke \ref nvgpu_fifo_setup_sw_common(g) to perform sw setup.
 * - Mark FIFO sw setup ready by setting #nvgpu_fifo.sw_ready to true.
 *
 * @retval 0		in case of success.
 * @retval -ENOMEM	in case there is not enough memory available.
 * @retval <0		other unspecified errors.
 */
int nvgpu_fifo_setup_sw(struct gk20a *g);

/**
 * @brief Initialize FIFO software metadata sequentially for sub-units channel,
 *        tsg, pbdma, engine, runlist and userd.
 *
 * @param g [in]	The GPU driver struct.
 *                      - The function does not perform g parameter validation.
 *
 * - Obtain #nvgpu_fifo pointer from GPU pointer as g->fifo.
 * - Initialize mutexes of type nvgpu_mutex needed by the FIFO module using
 * \ref nvgpu_mutex_init(&nvgpu_fifo.intr.isr.mutex) and
 * \ref nvgpu_mutex_init(&nvgpu_fifo.engines_reset_mutex).
 * - Use \ref nvgpu_channel_setup_sw(g) to setup channel data structures. In case
 * of failure, print error and return with error.
 * - Use \ref nvgpu_tsg_setup_sw(g) to setup tsg data structures. In case
 * of failure, clean up channel structures using \ref nvgpu_channel_cleanup_sw(g)
 * and return with error.
 * - Check if \ref gops_pbdma.setup_sw(g) is set. If yes, invoke
 * \ref gops_pbdma.setup_sw(g) to setup pbdma data structures. In case of
 * failure, clean up tsg using \ref nvgpu_tsg_cleanup_sw() and channel
 * structures, then return with error.
 * - Use nvgpu_engine_setup_sw(g) to setup engine data structures. In case
 * of failure, if \ref gops_pbdma.cleanup_sw(g) is set, clean up pbdma
 * using \ref gops_pbdma.cleanup_sw(g). Further clean up tsg and channel
 * structures then return with error.
 * - Use \ref nvgpu_runlist_setup_sw(g) to setup runlist data structures.
 * In case of failure, clean up engine using \ref nvgpu_engine_cleanup_sw(g) and
 * pbdma, tsg, channel structures then return with error.
 * - Invoke \ref gops_userd.setup_sw(g)" to setup userd data structures.
 * In case of failure, clean up runlist using \ref nvgpu_runlist_cleanup_sw(g)
 * and engine, pbdma, tsg, channel structures then return with error.
 * - Initialize \ref nvgpu_fifo.remove_support(g) function pointer.
 *
 * @note In case of failure, cleanup sw metadata for sub-units that are
 *       initialized.
 *
 * @retval 0		in case of success.
 * @retval -ENOMEM	in case there is not enough memory available.
 * @retval <0		other unspecified errors.
 */
int nvgpu_fifo_setup_sw_common(struct gk20a *g);

/**
 * @brief Clean up FIFO software metadata.
 *
 * @param g [in]	The GPU driver struct.
 *                      - The function does not perform g parameter validation.
 *
 * Use \ref nvgpu_fifo_cleanup_sw_common(g) to free FIFO software metadata.
 *
 * @return	None
 */
void nvgpu_fifo_cleanup_sw(struct gk20a *g);

/**
 * @brief Clean up FIFO sub-unit metadata.
 *
 * @param g [in]	The GPU driver struct.
 *                      - The function does not perform g parameter validation.
 *
 * - Invoke \ref gops_userd.cleanup_sw(g) to free userd data structures.
 * - Use \ref nvgpu_channel_cleanup_sw(g) to free channel data structures.
 * - Use \ref nvgpu_tsg_cleanup_sw(g) to free tsg data structures.
 * - Use \ref nvgpu_runlist_cleanup_sw(g) to free runlist data structures.
 * - Use \ref nvgpu_engine_cleanup_sw(g) to free engine data structures.
 * - Check if \ref gops_pbdma.cleanup_sw(g) is set. If yes, invoke
 * \ref gops_pbdma.cleanup_sw(g) to free pbdma data structures.
 * - Destroy mutexes of type #nvgpu_mutex needed by the FIFO module using
 * \ref nvgpu_mutex_destroy(&nvgpu_fifo.intr.isr.mutex) and
 * \ref nvgpu_mutex_destroy(&nvgpu_fifo.engines_reset_mutex).
 * Mark FIFO s/w clean up complete by setting #nvgpu_fifo.sw_ready to false.
 *
 * @return	None
 */
void nvgpu_fifo_cleanup_sw_common(struct gk20a *g);

/**
 * @brief Decode PBDMA channel status and Engine status read from h/w register.
 *
 * @param index [in]	Status value used to index into the constant array of
 *			constant characters.
 *                      - The function validates that \a index is not greater
 *                        than #pbdma_ch_eng_status_str array size.
 *
 * Check \a index is within #pbdma_ch_eng_status_str array size. If \a index
 * is valid, return string at \a index of #pbdma_ch_eng_status_str array. Else
 * return "not found" string.
 *
 * @return PBDMA and channel status as a string
 */
const char *nvgpu_fifo_decode_pbdma_ch_eng_status(u32 index);

/**
 * @brief Suspend FIFO support while preparing GPU for poweroff.
 *
 * @param g [in]	The GPU driver struct.
 *                      - The function does not perform g parameter validation.
 *
 * - Check if \ref gops_mm.is_bar1_supported(g) is set. If yes, invoke
 * \ref gops_mm.is_bar1_supported(g) to disable BAR1 snooping.
 * - Invoke \ref disable_fifo_interrupts(g) to disable FIFO stalling and
 * non-stalling interrupts at FIFO unit and MC level.
 *
 * @retval 0 is always returned.
 */
int nvgpu_fifo_suspend(struct gk20a *g);

/**
 * @brief Emergency quiescing of FIFO.
 *
 * @param g [in]	The GPU driver struct.
 *                      - The function does not perform g parameter validation.
 *
 * Put FIFO into a non-functioning state to ensure that no corrupted
 * work is completed because of the fault. This is because the freedom
 * from interference may not always be shown between the faulted and
 * the non-faulted TSG contexts.
 * - Set runlist_mask = U32_MAX, to indicate all runlists
 * - Invoke \ref gops_runlist.write_state(g, runlist_mask, RUNLIST_DISABLED)
 * to disable all runlists.
 * - Invoke \ref gops_fifo.preempt_runlists_for_rc(g, runlist_mask)
 * to preempt all runlists.
 *
 * @return	None
 */
void nvgpu_fifo_sw_quiesce(struct gk20a *g);

#endif /* NVGPU_FIFO_COMMON_H */
