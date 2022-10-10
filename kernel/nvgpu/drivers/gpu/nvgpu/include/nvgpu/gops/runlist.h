/*
 * Copyright (c) 2019-2022, NVIDIA CORPORATION.  All rights reserved.
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
#ifndef NVGPU_GOPS_RUNLIST_H
#define NVGPU_GOPS_RUNLIST_H

#include <nvgpu/types.h>
#include <nvgpu/pbdma.h>

/**
 * @file
 *
 * Runlist HAL interface.
 */
struct gk20a;
struct nvgpu_channel;
struct nvgpu_runlist;
struct nvgpu_runlist_domain;

/**
 * Runlist HAL operations.
 *
 * @see gpu_ops
 */
struct gops_runlist {
	/**
	 * @brief Reload runlist.
	 *
	 * @param g [in]		The GPU driver struct.
	 * @param runlist_id [in]	Runlist identifier.
	 * @param add [in]		True to submit a runlist buffer with
	 * 				all active channels. False to submit
	 * 				an empty runlist buffer.
	 * @param wait_for_finish [in]	True to wait for runlist update
	 * 				completion.
	 *
	 * When \a add is true, all entries are updated for the runlist.
	 * A runlist buffer is built with all active channels/TSGs for the
	 * runlist and submitted to H/W.
	 *
	 * When \a add is false, an empty runlist buffer is submitted to H/W.
	 * Submitting a NULL runlist results in Host expiring the current
	 * timeslices and effectively disabling scheduling for that runlist
	 * processor until the next runlist is submitted.
	 *
	 * @return 0 in case of success, < 0 in case of failure.
	 * @retval -ETIMEDOUT if transition to the new runlist takes too long,
	 *         and \a wait_for_finish was requested.
	 * @retval -E2BIG in case there are not enough entries in the runlist
	 *         buffer to accommodate all active channels/TSGs.
	 */
	int (*reload)(struct gk20a *g, struct nvgpu_runlist *rl,
			struct nvgpu_runlist_domain *domain,
			bool add, bool wait_for_finish);

	/**
	 * @brief Get maximum number of channels supported per TSG entry
	 *        in runlist.
	 *
	 * @param none.
	 *
	 * @return maximum number of channels supported per TSG in runlist.
	 */
	u32 (*get_max_channels_per_tsg)(void);

	/** @cond DOXYGEN_SHOULD_SKIP_THIS */

	int (*update)(struct gk20a *g, struct nvgpu_runlist *rl,
			struct nvgpu_channel *ch, bool add,
			bool wait_for_finish);
	u32 (*count_max)(struct gk20a *g);
	u32 (*entry_size)(struct gk20a *g);
	u32 (*length_max)(struct gk20a *g);
	void (*get_tsg_entry)(struct nvgpu_tsg *tsg,
			u32 *runlist, u32 timeslice);
	void (*get_ch_entry)(struct nvgpu_channel *ch, u32 *runlist);
	void (*hw_submit)(struct gk20a *g, struct nvgpu_runlist *runlist);
	int (*wait_pending)(struct gk20a *g, struct nvgpu_runlist *runlist);
	void (*write_state)(struct gk20a *g, u32 runlists_mask,
			u32 runlist_state);
	int (*reschedule)(struct nvgpu_channel *ch, bool preempt_next);
	int (*reschedule_preempt_next_locked)(struct nvgpu_channel *ch,
			bool wait_preempt);
	void (*init_enginfo)(struct gk20a *g, struct nvgpu_fifo *f);
	u32 (*get_tsg_max_timeslice)(void);
	u32 (*get_runlist_id)(struct gk20a *g, u32 runlist_pri_base);
	u32 (*get_runlist_aperture)(struct gk20a *g, struct nvgpu_runlist *runlist);
	u32 (*get_engine_id_from_rleng_id)(struct gk20a *g,
				u32 rleng_id, u32 runlist_pri_base);
	u32 (*get_chram_bar0_offset)(struct gk20a *g, u32 runlist_pri_base);
	void (*get_pbdma_info)(struct gk20a *g, u32 runlist_pri_base,
				struct nvgpu_pbdma_info *pbdma_info);
	u32 (*get_engine_intr_id)(struct gk20a *g, u32 runlist_pri_base,
			u32 rleng_id);
	u32 (*get_esched_fb_thread_id)(struct gk20a *g, u32 runlist_pri_base);

	/** @endcond DOXYGEN_SHOULD_SKIP_THIS */
};

#endif
