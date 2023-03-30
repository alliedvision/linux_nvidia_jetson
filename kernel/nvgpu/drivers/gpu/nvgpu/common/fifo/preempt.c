/*
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

#include <nvgpu/soc.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/errata.h>
#include <nvgpu/runlist.h>
#include <nvgpu/types.h>
#include <nvgpu/channel.h>
#include <nvgpu/tsg.h>
#include <nvgpu/preempt.h>
#include <nvgpu/nvgpu_err.h>
#include <nvgpu/cic_rm.h>
#include <nvgpu/rc.h>
#ifdef CONFIG_NVGPU_LS_PMU
#include <nvgpu/pmu/mutex.h>
#endif

u32 nvgpu_preempt_get_timeout(struct gk20a *g)
{
	return g->ctxsw_timeout_period_ms;
}

int nvgpu_fifo_preempt_tsg(struct gk20a *g, struct nvgpu_tsg *tsg)
{
	int ret = 0;
	u32 preempt_retry_count = 10U;
	u32 preempt_retry_timeout =
			nvgpu_preempt_get_timeout(g) / preempt_retry_count;
#ifdef CONFIG_NVGPU_LS_PMU
	u32 token = PMU_INVALID_MUTEX_OWNER_ID;
	int mutex_ret = 0;
#endif

	nvgpu_log_fn(g, "tsgid: %d", tsg->tsgid);

	if (tsg->runlist == NULL) {
		return 0;
	}

	do {
		nvgpu_mutex_acquire(&tsg->runlist->runlist_lock);

		if (nvgpu_is_errata_present(g, NVGPU_ERRATA_2016608)) {
			nvgpu_runlist_set_state(g, BIT32(tsg->runlist->id),
						RUNLIST_DISABLED);
		}

#ifdef CONFIG_NVGPU_LS_PMU
		mutex_ret = nvgpu_pmu_lock_acquire(g, g->pmu,
						   PMU_MUTEX_ID_FIFO, &token);
#endif
		g->ops.fifo.preempt_trigger(g, tsg->tsgid, ID_TYPE_TSG);

		/*
		 * Poll for preempt done. if stalling interrupts are pending
		 * while preempt is in progress we poll for stalling interrupts
		 * to finish based on return value from this function and
		 * retry preempt again.
		 * If HW is hung, on the last retry instance we try to identify
		 * the engines hung and set the runlist reset_eng_bitmask
		 * and mark preemption completion.
		 */
		ret = g->ops.fifo.is_preempt_pending(g, tsg->tsgid,
					ID_TYPE_TSG, preempt_retry_count > 1U);

#ifdef CONFIG_NVGPU_LS_PMU
		if (mutex_ret == 0) {
			int err = nvgpu_pmu_lock_release(g, g->pmu,
						PMU_MUTEX_ID_FIFO, &token);
			if (err != 0) {
				nvgpu_err(g, "PMU_MUTEX_ID_FIFO not released err=%d", err);
			}
		}
#endif
		if (nvgpu_is_errata_present(g, NVGPU_ERRATA_2016608)) {
			nvgpu_runlist_set_state(g, BIT32(tsg->runlist->id),
						RUNLIST_ENABLED);
		}

		nvgpu_mutex_release(&tsg->runlist->runlist_lock);

		if (ret != -EAGAIN) {
			break;
		}

		ret = nvgpu_cic_rm_wait_for_stall_interrupts(g, preempt_retry_timeout);
		if (ret != 0) {
			nvgpu_log_info(g, "wait for stall interrupts failed %d", ret);
		}
	} while (--preempt_retry_count != 0U);

	if (ret != 0) {
		if (nvgpu_platform_is_silicon(g)) {
			nvgpu_err(g, "preempt timed out for tsgid: %u, "
			"ctxsw timeout will trigger recovery if needed",
			tsg->tsgid);
		} else {
			nvgpu_rc_preempt_timeout(g, tsg);
		}
	}
	return ret;
}

int nvgpu_preempt_channel(struct gk20a *g, struct nvgpu_channel *ch)
{
	int err;
	struct nvgpu_tsg *tsg = nvgpu_tsg_from_ch(ch);

	if (tsg != NULL) {
		err = g->ops.fifo.preempt_tsg(ch->g, tsg);
	} else {
		err = g->ops.fifo.preempt_channel(ch->g, ch);
	}

	return err;
}

/* called from rc */
int nvgpu_preempt_poll_tsg_on_pbdma(struct gk20a *g,
		struct nvgpu_tsg *tsg)
{
	unsigned long runlist_served_pbdmas;
	unsigned long pbdma_id_bit;
	u32 tsgid, pbdma_id;

	if (g->ops.fifo.preempt_poll_pbdma == NULL) {
		return 0;
	}

	tsgid = tsg->tsgid;
	runlist_served_pbdmas = tsg->runlist->pbdma_bitmask;

	for_each_set_bit(pbdma_id_bit, &runlist_served_pbdmas,
			 nvgpu_get_litter_value(g, GPU_LIT_HOST_NUM_PBDMA)) {
		pbdma_id = U32(pbdma_id_bit);
		/*
		 * If pbdma preempt fails the only option is to reset
		 * GPU. Any sort of hang indicates the entire GPU’s
		 * memory system would be blocked.
		 */
		if (g->ops.fifo.preempt_poll_pbdma(g, tsgid, pbdma_id) != 0) {
			nvgpu_err(g, "PBDMA preempt failed");
			return -EBUSY;
		}
	}
	return 0;
}

/*
 * This should be called with runlist_lock held for all the
 * runlists set in runlists_mask
 */
void nvgpu_fifo_preempt_runlists_for_rc(struct gk20a *g, u32 runlists_bitmask)
{
	struct nvgpu_fifo *f = &g->fifo;
	u32 i;
#ifdef CONFIG_NVGPU_LS_PMU
	u32 token = PMU_INVALID_MUTEX_OWNER_ID;
	int mutex_ret = 0;
#endif

	/* runlist_lock are locked by teardown and sched are disabled too */
	nvgpu_log_fn(g, "preempt runlists_bitmask:0x%08x", runlists_bitmask);
#ifdef CONFIG_NVGPU_LS_PMU
	mutex_ret = nvgpu_pmu_lock_acquire(g, g->pmu,
			PMU_MUTEX_ID_FIFO, &token);
#endif

	for (i = 0U; i < f->num_runlists; i++) {
		struct nvgpu_runlist *runlist;

		runlist = &f->active_runlists[i];

		if ((BIT32(runlist->id) & runlists_bitmask) == 0U) {
			continue;
		}
		/* issue runlist preempt */
		g->ops.fifo.preempt_trigger(g, runlist->id,
					ID_TYPE_RUNLIST);
#ifdef CONFIG_NVGPU_RECOVERY
		/*
		 * Preemption will never complete in RC due to some
		 * fatal condition. Do not poll for preemption to
		 * complete. Reset engines served by runlists.
		 */
		runlist->reset_eng_bitmask = runlist->eng_bitmask;
#endif
	}

#ifdef CONFIG_NVGPU_LS_PMU
	if (mutex_ret == 0) {
		int err = nvgpu_pmu_lock_release(g, g->pmu, PMU_MUTEX_ID_FIFO,
				&token);
		if (err != 0) {
			nvgpu_err(g, "PMU_MUTEX_ID_FIFO not released err=%d",
					err);
		}
	}
#endif
}
