/*
 * Copyright (c) 2011-2022, NVIDIA CORPORATION.  All rights reserved.
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

#include <nvgpu/gk20a.h>
#include <nvgpu/types.h>
#include <nvgpu/timers.h>
#include <nvgpu/soc.h>
#include <nvgpu/io.h>
#include <nvgpu/utils.h>
#include <nvgpu/fifo.h>
#include <nvgpu/rc.h>
#include <nvgpu/runlist.h>
#include <nvgpu/channel.h>
#include <nvgpu/tsg.h>
#include <nvgpu/preempt.h>
#ifdef CONFIG_NVGPU_LS_PMU
#include <nvgpu/pmu/mutex.h>
#endif

#include "preempt_gk20a.h"

#include <nvgpu/hw/gk20a/hw_fifo_gk20a.h>

void gk20a_fifo_preempt_trigger(struct gk20a *g, u32 id, unsigned int id_type)
{
	if (id_type == ID_TYPE_TSG) {
		nvgpu_writel(g, fifo_preempt_r(),
			fifo_preempt_id_f(id) |
			fifo_preempt_type_tsg_f());
	} else {
		nvgpu_writel(g, fifo_preempt_r(),
			fifo_preempt_chid_f(id) |
			fifo_preempt_type_channel_f());
	}
}

static int gk20a_fifo_preempt_locked(struct gk20a *g, u32 id,
		unsigned int id_type)
{
	nvgpu_log_fn(g, "id: %d id_type: %d", id, id_type);

	/* issue preempt */
	g->ops.fifo.preempt_trigger(g, id, id_type);

	/* wait for preempt */
	return g->ops.fifo.is_preempt_pending(g, id, id_type, false);
}

int gk20a_fifo_is_preempt_pending(struct gk20a *g, u32 id,
		unsigned int id_type, bool preempt_retries_left)
{
	struct nvgpu_timeout timeout;
	u32 delay = POLL_DELAY_MIN_US;
	int ret;

	(void)preempt_retries_left;

	nvgpu_timeout_init_cpu_timer(g, &timeout, nvgpu_preempt_get_timeout(g));

	ret = -EBUSY;
	do {
		if ((nvgpu_readl(g, fifo_preempt_r()) &
				fifo_preempt_pending_true_f()) == 0U) {
			ret = 0;
			break;
		}

		nvgpu_usleep_range(delay, delay * 2U);
		delay = min_t(u32, delay << 1, POLL_DELAY_MAX_US);
	} while (nvgpu_timeout_expired(&timeout) == 0);

	if (ret != 0) {
		nvgpu_err(g, "preempt timeout: id: %u id_type: %d ",
			id, id_type);
	}
	return ret;
}

int gk20a_fifo_preempt_channel(struct gk20a *g, struct nvgpu_channel *ch)
{
	int ret = 0;
#ifdef CONFIG_NVGPU_LS_PMU
	u32 token = PMU_INVALID_MUTEX_OWNER_ID;
	int mutex_ret = 0;
#endif
	nvgpu_log_fn(g, "preempt chid: %d", ch->chid);

	/* we have no idea which runlist we are using. lock all */
	nvgpu_runlist_lock_active_runlists(g);
#ifdef CONFIG_NVGPU_LS_PMU
	mutex_ret = nvgpu_pmu_lock_acquire(g, g->pmu,
		PMU_MUTEX_ID_FIFO, &token);
#endif
	ret = gk20a_fifo_preempt_locked(g, ch->chid, ID_TYPE_CHANNEL);
#ifdef CONFIG_NVGPU_LS_PMU
	if (mutex_ret == 0) {
		if (nvgpu_pmu_lock_release(g, g->pmu,
				PMU_MUTEX_ID_FIFO, &token) != 0) {
			nvgpu_err(g, "failed to release PMU lock");
		}
	}
#endif
	nvgpu_runlist_unlock_active_runlists(g);

	if (ret != 0) {
		if (nvgpu_platform_is_silicon(g)) {
			nvgpu_err(g, "preempt timed out for chid: %u, "
			"ctxsw timeout will trigger recovery if needed",
			ch->chid);
		} else {
			struct nvgpu_tsg *tsg;

			nvgpu_err(g, "preempt channel %d timeout", ch->chid);
			tsg = nvgpu_tsg_from_ch(ch);
			if (tsg != NULL) {
				nvgpu_rc_preempt_timeout(g, tsg);
			} else {
				nvgpu_err(g, "chid: %d is not bound to tsg",
					ch->chid);
			}

		}
	}

	return ret;
}

int gk20a_fifo_preempt_tsg(struct gk20a *g, struct nvgpu_tsg *tsg)
{
	int ret = 0;
#ifdef CONFIG_NVGPU_LS_PMU
	u32 token = PMU_INVALID_MUTEX_OWNER_ID;
	int mutex_ret = 0;
#endif
	nvgpu_log_fn(g, "tsgid: %d", tsg->tsgid);

	/* we have no idea which runlist we are using. lock all */
	nvgpu_runlist_lock_active_runlists(g);
#ifdef CONFIG_NVGPU_LS_PMU
	mutex_ret = nvgpu_pmu_lock_acquire(g, g->pmu,
			PMU_MUTEX_ID_FIFO, &token);
#endif
	ret = gk20a_fifo_preempt_locked(g, tsg->tsgid, ID_TYPE_TSG);
#ifdef CONFIG_NVGPU_LS_PMU
	if (mutex_ret == 0) {
		if (nvgpu_pmu_lock_release(g, g->pmu,
				PMU_MUTEX_ID_FIFO, &token) != 0) {
			nvgpu_err(g, "failed to release PMU lock");
		}
	}
#endif
	nvgpu_runlist_unlock_active_runlists(g);

	if (ret != 0) {
		if (nvgpu_platform_is_silicon(g)) {
			nvgpu_err(g, "preempt timed out for tsgid: %u, "
			"ctxsw timeout will trigger recovery if needed",
			tsg->tsgid);
		} else {
			nvgpu_err(g, "preempt TSG %d timeout", tsg->tsgid);
			nvgpu_rc_preempt_timeout(g, tsg);
		}
	}

	return ret;
}
