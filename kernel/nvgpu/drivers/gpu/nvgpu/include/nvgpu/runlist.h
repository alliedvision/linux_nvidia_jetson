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

#ifndef NVGPU_RUNLIST_H
#define NVGPU_RUNLIST_H

#include <nvgpu/types.h>
#include <nvgpu/nvgpu_mem.h>
#include <nvgpu/lock.h>

/**
 * @file
 *
 * Runlist interface.
 */

struct gk20a;
struct nvgpu_tsg;
struct nvgpu_fifo;
struct nvgpu_channel;
struct nvgpu_device;

/** @cond DOXYGEN_SHOULD_SKIP_THIS */
struct nvgpu_pbdma_info;

#define RLENG_PER_RUNLIST_SIZE			3

/**
 * Low interleave level for runlist entry. TSGs with this interleave level
 * typically appear only once in the runlist.
 */
#define NVGPU_FIFO_RUNLIST_INTERLEAVE_LEVEL_LOW     0U
/**
 * Medium interleave level for runlist entry. TSGs with medium or high
 * interleave levels are inserted multiple times in the runlist, so that
 * they have more opportunities to run.
 */
#define NVGPU_FIFO_RUNLIST_INTERLEAVE_LEVEL_MEDIUM  1U
/**
 * High interleave level for runlist entry.
 */
#define NVGPU_FIFO_RUNLIST_INTERLEAVE_LEVEL_HIGH    2U
/**
 * Number of interleave levels. In safety build, all TSGs are handled with
 * low interleave level.
 */
#define NVGPU_FIFO_RUNLIST_INTERLEAVE_NUM_LEVELS    3U

/** Not enough entries in runlist buffer to accommodate all channels/TSGs. */
#define RUNLIST_APPEND_FAILURE U32_MAX

/** Disable runlist. */
#define RUNLIST_DISABLED		0U
/** Enable runlist. */
#define RUNLIST_ENABLED			1U

/** Runlist identifier is invalid. */
#define NVGPU_INVALID_RUNLIST_ID		U32_MAX

/*
 * Updates to this memory are still serialized by the runlist lock.
 *
 * TODO: add a mutex when domain updates get more fine-grained. The buffers in
 * nvgpu_runlist_domain are pointer members for a reason to make this easier in
 * the future; the buffers may get juggled around.
 */
struct nvgpu_runlist_mem {
	/** Rendered runlist memory suitable for HW. */
	struct nvgpu_mem mem;

	/** Number of entries written in the buffer. */
	u32 count;
};

/*
 * Data interface to be owned by another SW unit. The heart of the domain
 * scheduler can be running outside nvgpu and as such cannot own these.
 * However, nvgpu needs to do some bookkeeping for the domain scheduler anyway;
 * this will be owned by that layer, and will be only presented to the runlist
 * HW for submitting only. The contents will be filled by another SW unit
 * responsible for only that.
 *
 * For now, we live in a transitional limbo where the domain scheduler does not
 * exist yet in its final form but managing separate runlist domains will help
 * bring it to existence.
 *
 * Intent of final interfaces ("a -> b": a uses b):
 *
 * nvgpu domain scheduler -> runlist domain
 * channels -> runlist domain
 * TSGs -> runlist domain
 * nvgpu domain scheduler -> core scheduler
 * core scheduler -> runlist HW
 * fault reset/recovery -> core scheduler
 * fault reset/recovery -> runlist HW
 *
 * Memory ownership of a runlist domain will be in the nvgpu domain scheduler.
 */
struct nvgpu_runlist_domain {
	/**
	 * Placeholder for metadata that will come in further patches.
	 */
	char name[32];
	/**
	 * All created domains are tracked in a list.
	 *
	 * The list head is nvgpu_runlist::domains
	 */
	struct nvgpu_list_node domains_list;

	/** Bitmap of active channels in the runlist domain. One bit per chid. */
	unsigned long *active_channels;
	/** Bitmap of active TSGs in the runlist domain. One bit per tsgid. */
	unsigned long *active_tsgs;

	/** Runlist buffer free to use in sw. Swapped with another mem on next load. */
	struct nvgpu_runlist_mem *mem;

	/** Currently active buffer submitted for hardware. */
	struct nvgpu_runlist_mem *mem_hw;
};

struct nvgpu_runlist {
	/** The HW has some designated RL IDs that are bound to engines. */
	u32 id;

	/* The currently active scheduling domain. */
	struct nvgpu_runlist_domain *domain;
	/*
	 * All scheduling domains of this RL, see nvgpu_runlist_domain::domain_node.
	 *
	 * Design note: the runlist hardware unit should not own the actual
	 * domain memory; this arrangement is temporary to aid in transition
	 * for a domain scheduler where a scheduling domain will own
	 * domain-related runlist data (nvgpu_runlist_domain). See the
	 * documentation of nvgpu_runlist_domain.
	 */
	struct nvgpu_list_node domains;

	/** Bitmask of PBDMAs supported for this runlist. */
	u32  pbdma_bitmask;
	/** Bitmask of engines using this runlist. */
	u32  eng_bitmask;
	/** Bitmask of engines to be reset during recovery. */
	u32  reset_eng_bitmask;
	/** Protect ch/tsg/runlist preempt & runlist update. */
	struct nvgpu_mutex runlist_lock;

	/** @cond DOXYGEN_SHOULD_SKIP_THIS */
	/* Ampere+ runlist info additions */

	/** Runlist pri base - offset into device's runlist space */
	u32 runlist_pri_base;
	/** Channel ram address in bar0 pri space */
	u32 chram_bar0_offset;
	/** Pointer to pbdma info stored in engine_info*/
	const struct nvgpu_pbdma_info *pbdma_info;
	/** Pointer to engine info for per runlist engine id */
	const struct nvgpu_device *rl_dev_list[RLENG_PER_RUNLIST_SIZE];
	/** @endcond DOXYGEN_SHOULD_SKIP_THIS */
};

int nvgpu_rl_domain_alloc(struct gk20a *g, const char *name);
int nvgpu_rl_domain_delete(struct gk20a *g, const char *name);
struct nvgpu_runlist_domain *nvgpu_rl_domain_get(struct gk20a *g, u32 runlist_id,
						 const char *name);

static inline struct nvgpu_runlist_domain *
nvgpu_runlist_domain_from_domains_list(struct nvgpu_list_node *node)
{
	return (struct nvgpu_runlist_domain *)
	((uintptr_t)node - offsetof(struct nvgpu_runlist_domain, domains_list));
}

void nvgpu_runlist_tick(struct gk20a *g);

/**
 * @brief Rebuild runlist
 *
 * @param f [in]		The FIFO context using this runlist.
 * @param runlist [in]		Runlist context.
 * @param buf_id [in]		Indicates which runlist buffer to use.
 * @param max_entries [in]	Max number of entries in runlist buffer.
 *
 * Walks through all active TSGs in #runlist, and constructs runlist
 * buffer #buf_id. This buffer can afterwards be submitted to H/W
 * to be used for scheduling.
 *
 * Note: Caller must hold runlist_lock before invoking this function.
 *
 * @return Number of entries in the runlist.
 * @retval #RUNLIST_APPEND_FAILURE in case there is not enough entries in
 * runlist buffer to describe all active channels and TSGs.
 */
u32 nvgpu_runlist_construct_locked(struct nvgpu_fifo *f,
		struct nvgpu_runlist_domain *domain,
		u32 max_entries);

/**
 * @brief Add/remove channel to/from runlist (locked)
 *
 * @param g [in]		The GPU driver struct owning this runlist.
 * @param runlist [in]		Runlist object to be modified.
 * @param ch [in]		Channel to be added/removed or NULL to update all.
 * @param add [in]		True to add a channel, false to remove it.
 * @param wait_for_finish [in]	True to wait for runlist update completion.
 *
 * When #ch is NULL, this function has same behavior as #nvgpu_runlist_reload.
 * When #ch is non NULL, this function has same behavior as
 * #nvgpu_runlist_update.
 *
 * The only difference with #nvgpu_runlist_reload is that the caller already
 * holds the runlist_lock before calling this function.
 *
 * @return 0 in case of success, < 0 in case of failure.
 * @retval -E2BIG in case there are not enough entries in runlist buffer to
 *          describe all active channels and TSGs.
 */
int nvgpu_runlist_update_locked(struct gk20a *g, struct nvgpu_runlist *rl,
		struct nvgpu_runlist_domain *domain,
		struct nvgpu_channel *ch, bool add, bool wait_for_finish);

#ifdef CONFIG_NVGPU_CHANNEL_TSG_SCHEDULING
int nvgpu_runlist_reschedule(struct nvgpu_channel *ch, bool preempt_next,
		bool wait_preempt);
#endif

/**
 * @brief Add/remove channel to/from runlist
 *
 * @param g [in]		The GPU driver struct owning this runlist.
 * @param runlist_id [in]	Runlist identifier
 * @param ch [in]		Channel to be added/removed (must be non NULL)
 * @param add [in]		True to add channel to runlist
 * @param wait_for_finish [in]	True to wait for completion
 *
 * When #add is true, adds #ch to active channels of runlist #runlist_id.
 * When #add is false, removes #ch from active channels of runlist #runlist_id.
 * A new runlist is then constructed for active channels/TSGs, and submitted
 * to H/W.
 *
 * When transitioning from prior runlist to the new runlist, H/W may have
 * to preempt current TSG. When #wait_for_finish is true, the function polls
 * H/W until it is done transitionning to the new runlist. In this case
 * the function may fail with -ETIMEDOUT if transition to the new runlist takes
 * too long.
 *
 * Note: function asserts that #ch is not NULL.
 *
 * @return 0 in case of success, < 0 in case of failure.
 * @retval -E2BIG in case there are not enough entries in runlist buffer to
 *         accommodate all active channels/TSGs.
 */
int nvgpu_runlist_update(struct gk20a *g, struct nvgpu_runlist *rl,
		struct nvgpu_channel *ch, bool add, bool wait_for_finish);

/**
 * @brief Reload runlist
 *
 * @param g [in]		The GPU driver struct owning this runlist.
 * @param runlist_id [in]	Runlist identifier.
 * @param add [in]		True to submit a runlist buffer with all active
 *				channels. False to submit an empty runlist
 *				buffer.
 * @param wait_for_finish [in]	True to wait for runlist update completion.
 *
 * When #add is true, all entries are updated for the runlist. A runlist buffer
 * is built with all active channels/TSGs for the runlist and submitted to H/W.
 * When #add is false, an empty runlist buffer is submitted to H/W. Submitting
 * a NULL runlist results in Host expiring the current timeslices and
 * effectively disabling scheduling for that runlist processor until the next
 * runlist is submitted.
 *
 * @return 0 in case of success, < 0 in case of failure.
 * @retval -ETIMEDOUT if transition to the new runlist takes too long, and
 *         #wait_for_finish was requested.
 * @retval -E2BIG in case there are not enough entries in the runlist buffer
 *         to accommodate all active channels/TSGs.
 */
int nvgpu_runlist_reload(struct gk20a *g, struct nvgpu_runlist *rl,
		struct nvgpu_runlist_domain *domain,
		bool add, bool wait_for_finish);

/**
 * @brief Reload a set of runlists
 *
 * @param g [in]		The GPU driver struct owning the runlists.
 * @param runlist_ids [in]	Bitmask of runlists, one bit per runlist_id.
 * @param add [in]		True to submit a runlist buffer with all active
 *				channels. False to submit an empty runlist
 *				buffer.
 *
 * This function is similar to nvgpu_runlist_reload, but takes a set of
 * runlists as a parameter. It also always waits for runlist update completion.
 *
 * @return 0 in case of success, < 0 in case of failure.
 * @retval -ETIMEDOUT if runlist update takes too long for one of the runlists.
 * @retval -E2BIG in case there are not enough entries in one runlist buffer
 *         to accommodate all active channels/TSGs.
 */
int nvgpu_runlist_reload_ids(struct gk20a *g, u32 runlist_ids, bool add);

/**
 * @brief Interleave level name.
 *
 * @param interleave_level	Interleave level.
 *				(e.g. #NVGPU_FIFO_RUNLIST_INTERLEAVE_LEVEL_LOW)
 *
 * @return String representing the name of runlist interleave level.
 */
const char *nvgpu_runlist_interleave_level_name(u32 interleave_level);

/**
 * @brief Enable/disable a set of runlists
 *
 * @param g [in]		The GPU driver struct owning the runlists.
 * @param runlist_mask [in]	Bitmask of runlist, one bit per runlist_id.
 * @param runlist_state [in]	#RUNLIST_ENABLE or #RUNLIST_DISABLE.
 *
 * If scheduling of a runlist is disabled, no new channels will be scheduled
 * to run from that runlist. It does not stop the scheduler from finishing
 * parsing a TSG that was in flight at the point scheduling was disabled,
 * but no channels will be scheduled from that TSG.  The currently running
 * channel will continue to run, and any scheduler events for this runlist
 * will continue to be handled. In particular, the PBDMA unit will continue
 * processing methods for the channel, and the downstream engine will continue
 * processing methods.
 */
void nvgpu_runlist_set_state(struct gk20a *g, u32 runlists_mask,
		 u32 runlist_state);

/**
 * @brief Initialize runlist context for current GPU
 *
 * @param g [in]		The GPU driver struct owning the runlists.
 *
 * - Determine number of runlists and max entries per runlists.
 * - Determine active runlists, i.e. runlists that are mapped to one engine.
 * - For each active runlist,
 *   - Build mapping between runlist_id (H/W) and runlist info.
 *   - Allocate runlist buffers.
 *   - Allocate bitmaps to track active channels and TSGs.
 *   - Determine bitmask of engines serviced by this runlist.
 *   - Determine bitmask of PBDMAs servicing this runlist.
 *
 * @return 0 in case of success, < 0 in case of failure.
 * @retval -ENOMEM in case insufficient memory is available.
 */
int nvgpu_runlist_setup_sw(struct gk20a *g);

/**
 * @brief De-initialize runlist context
 *
 * @param g [in]		The GPU driver struct owning the runlists.
 *
 * Cleans up runlist context for current GPU:
 * - Free runlist buffers.
 * - Free bitmaps to track active channels and TSGs.
 * - Free runlists.
 */
void nvgpu_runlist_cleanup_sw(struct gk20a *g);

/**
 * @brief Acquire lock for active runlists
 *
 * @param g [in]		The GPU driver struct owning the runlists.
 *
 * Walk through runlist ids, and acquire runlist lock for runlists that are
 * actually in use (i.e. mapped to one engine).
 */
void nvgpu_runlist_lock_active_runlists(struct gk20a *g);

/**
 * @brief Release lock for active runlists
 *
 * @param g [in]		The GPU driver struct owning the runlists.
 *
 * Walk through runlist ids, and release runlist lock for runlists that are
 * actually in use (i.e. mapped to one engine).
 */
void nvgpu_runlist_unlock_active_runlists(struct gk20a *g);

/**
 * @brief Release lock for a set of runlists
 *
 * @param g [in]		The GPU driver struct owning the runlists.
 * @param runlists_mask [in]	Set of runlists to release lock for. One bit
 *				per runlist_id.
 *
 * Walk through runlist ids, and release runlist lock for runlists that are
 * actually in use (i.e. mapped to one engine).
 */
void nvgpu_runlist_unlock_runlists(struct gk20a *g, u32 runlists_mask);

/**
 * @brief Get list of runlists per engine/PBDMA/TSG
 *
 * @param g [in]		The GPU driver struct owning the runlists.
 * @param id [in]		TSG or Channel Identifier (see #id_type).
 * @param id_type [in]		Identifier Type (#ID_TYPE_CHANNEL, #ID_TYPE_TSG
 *				or #ID_TYPE_UNKNOWN).
 * @param act_eng_bitmask [in]	Bitmask of active engines, one bit per
 *				engine id.
 * @param pbdma_bitmask [in]	Bitmask of PBDMAs, one bit per PBDMA id.
 *
 * If engines or PBDMAs are known (i.e. non-zero #act_eng_bitmask and/or
 * #pbdma_bitmask), the function looks up for all runlists servicing those
 * engines and/or PBDMAs.
 *
 * If #id_type is known (i.e. ID_TYPE_CHANNEL or ID_TYPE_TSG), the function
 * looks up for the runlist servicing related channel/TSG.
 *
 * @return A bitmask of runlists servicing specified engines/PBDMAs/channel/TSG.
 * @retval Runlist_mask of runlist servicing \a id or engine/PBDMA, if both
 *         \a id_type and engine/PBDMAs are known.
 * @retval Runlist_mask of all active runlists, if both \a id_type and
 *         engines/PBDMAs are unknown (i.e. #ID_TYPE_UNKNOWN and both
 *         \a act_eng_bitmask and #pbdma_bitmask are equal to 0).
 */
u32 nvgpu_runlist_get_runlists_mask(struct gk20a *g, u32 id,
	unsigned int id_type, u32 act_eng_bitmask, u32 pbdma_bitmask);

/** @cond DOXYGEN_SHOULD_SKIP_THIS */
/**
 * @brief Initialize runlists with engine info
 *
 * @param g [in]		The GPU driver struct owning the runlists.
 * @param f [in]		The FIFO context using this runlist.
 *
 * Walks through all active engines info, and initialize runlist info.
 */
void nvgpu_runlist_init_enginfo(struct gk20a *g, struct nvgpu_fifo *f);

/** @endcond DOXYGEN_SHOULD_SKIP_THIS */

#define rl_dbg(g, fmt, arg...)			\
	nvgpu_log(g, gpu_dbg_runlists, "RL | " fmt, ##arg)

#endif /* NVGPU_RUNLIST_H */
