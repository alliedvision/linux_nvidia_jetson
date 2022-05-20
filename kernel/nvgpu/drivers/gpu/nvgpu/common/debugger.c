/*
 * Tegra GK20A GPU Debugger/Profiler Driver
 *
 * Copyright (c) 2013-2019, NVIDIA CORPORATION.  All rights reserved.
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

#include <nvgpu/kmem.h>
#include <nvgpu/log.h>
#include <nvgpu/vm.h>
#include <nvgpu/atomic.h>
#include <nvgpu/mm.h>
#include <nvgpu/bug.h>
#include <nvgpu/io.h>
#include <nvgpu/utils.h>
#include <nvgpu/channel.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/nvgpu_init.h>
#include <nvgpu/debugger.h>
#include <nvgpu/profiler.h>
#include <nvgpu/power_features/power_features.h>

/*
 * API to get first channel from the list of all channels
 * bound to the debug session
 */
struct nvgpu_channel *
nvgpu_dbg_gpu_get_session_channel(struct dbg_session_gk20a *dbg_s)
{
	struct dbg_session_channel_data *ch_data;
	struct nvgpu_channel *ch;
	struct gk20a *g = dbg_s->g;

	nvgpu_mutex_acquire(&dbg_s->ch_list_lock);
	if (nvgpu_list_empty(&dbg_s->ch_list)) {
		nvgpu_mutex_release(&dbg_s->ch_list_lock);
		return NULL;
	}

	ch_data = nvgpu_list_first_entry(&dbg_s->ch_list,
				   dbg_session_channel_data,
				   ch_entry);
	ch = g->fifo.channel + ch_data->chid;

	nvgpu_mutex_release(&dbg_s->ch_list_lock);

	return ch;
}

void nvgpu_dbg_gpu_post_events(struct nvgpu_channel *ch)
{
	struct dbg_session_data *session_data;
	struct dbg_session_gk20a *dbg_s;
	struct gk20a *g = ch->g;

	nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gpu_dbg, " ");

	/* guard against the session list being modified */
	nvgpu_mutex_acquire(&ch->dbg_s_lock);

	nvgpu_list_for_each_entry(session_data, &ch->dbg_s_list,
				dbg_session_data, dbg_s_entry) {
		dbg_s = session_data->dbg_s;
		if (dbg_s->dbg_events.events_enabled) {
			nvgpu_log(g, gpu_dbg_gpu_dbg, "posting event on session id %d",
					dbg_s->id);
			nvgpu_log(g, gpu_dbg_gpu_dbg, "%d events pending",
					dbg_s->dbg_events.num_pending_events);

			dbg_s->dbg_events.num_pending_events++;

			nvgpu_dbg_session_post_event(dbg_s);
		}
	}

	nvgpu_mutex_release(&ch->dbg_s_lock);
}

bool nvgpu_dbg_gpu_broadcast_stop_trigger(struct nvgpu_channel *ch)
{
	struct dbg_session_data *session_data;
	struct dbg_session_gk20a *dbg_s;
	bool broadcast = false;
	struct gk20a *g = ch->g;

	nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gpu_dbg | gpu_dbg_intr, " ");

	/* guard against the session list being modified */
	nvgpu_mutex_acquire(&ch->dbg_s_lock);

	nvgpu_list_for_each_entry(session_data, &ch->dbg_s_list,
				dbg_session_data, dbg_s_entry) {
		dbg_s = session_data->dbg_s;
		if (dbg_s->broadcast_stop_trigger) {
			nvgpu_log(g, gpu_dbg_gpu_dbg | gpu_dbg_fn | gpu_dbg_intr,
					"stop trigger broadcast enabled");
			broadcast = true;
			break;
		}
	}

	nvgpu_mutex_release(&ch->dbg_s_lock);

	return broadcast;
}

void nvgpu_dbg_gpu_clear_broadcast_stop_trigger(struct nvgpu_channel *ch)
{
	struct dbg_session_data *session_data;
	struct dbg_session_gk20a *dbg_s;
	struct gk20a *g = ch->g;

	nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gpu_dbg | gpu_dbg_intr, " ");

	/* guard against the session list being modified */
	nvgpu_mutex_acquire(&ch->dbg_s_lock);

	nvgpu_list_for_each_entry(session_data, &ch->dbg_s_list,
				dbg_session_data, dbg_s_entry) {
		dbg_s = session_data->dbg_s;
		if (dbg_s->broadcast_stop_trigger) {
			nvgpu_log(g, gpu_dbg_gpu_dbg | gpu_dbg_fn | gpu_dbg_intr,
					"stop trigger broadcast disabled");
			dbg_s->broadcast_stop_trigger = false;
		}
	}

	nvgpu_mutex_release(&ch->dbg_s_lock);
}

u32 nvgpu_set_powergate_locked(struct dbg_session_gk20a *dbg_s,
				bool mode)
{
	u32 err = 0U;
	struct gk20a *g = dbg_s->g;

	if (dbg_s->is_pg_disabled != mode) {
		if (mode == false) {
			g->dbg_powergating_disabled_refcount--;
		}

		/*
		 * Allow powergate disable or enable only if
		 * the global pg disabled refcount is zero
		 */
		if (g->dbg_powergating_disabled_refcount == 0) {
			err = g->ops.debugger.dbg_set_powergate(dbg_s,
									mode);
		}

		if (mode) {
			g->dbg_powergating_disabled_refcount++;
		}

		dbg_s->is_pg_disabled = mode;
	}

	return err;
}

int nvgpu_dbg_set_powergate(struct dbg_session_gk20a *dbg_s, bool disable_powergate)
{
	int err = 0;
	struct gk20a *g = dbg_s->g;

	 /* This function must be called with g->dbg_sessions_lock held */

	nvgpu_log(g, gpu_dbg_fn|gpu_dbg_gpu_dbg, "%s powergate mode = %s",
		   g->name, disable_powergate ? "disable" : "enable");

	/*
	 * Powergate mode here refers to railgate+powergate+clockgate
	 * so in case slcg/blcg/elcg are disabled and railgating is enabled,
	 * disable railgating and then set is_pg_disabled = true
	 * Similarly re-enable railgating and not other features if they are not
	 * enabled when powermode=MODE_ENABLE
	 */
	if (disable_powergate) {
		/* save off current powergate, clk state.
		 * set gpu module's can_powergate = 0.
		 * set gpu module's clk to max.
		 * while *a* debug session is active there will be no power or
		 * clocking state changes allowed from mainline code (but they
		 * should be saved).
		 */

		nvgpu_log(g, gpu_dbg_gpu_dbg | gpu_dbg_fn,
						"module busy");
		err = gk20a_busy(g);
		if (err != 0) {
			return err;
		}

#ifdef CONFIG_NVGPU_NON_FUSA
		err = nvgpu_cg_pg_disable(g);
#endif
		if (err == 0) {
			dbg_s->is_pg_disabled = true;
			nvgpu_log(g, gpu_dbg_gpu_dbg | gpu_dbg_fn,
					"pg disabled");
		}
	} else {
		/* restore (can) powergate, clk state */
		/* release pending exceptions to fault/be handled as usual */
		/*TBD: ordering of these? */

#ifdef CONFIG_NVGPU_NON_FUSA
		err = nvgpu_cg_pg_enable(g);
#endif
		if (err == 0) {
			dbg_s->is_pg_disabled = false;
			nvgpu_log(g, gpu_dbg_gpu_dbg | gpu_dbg_fn,
					"pg enabled");
		}

		nvgpu_log(g, gpu_dbg_gpu_dbg | gpu_dbg_fn, "module idle");

		gk20a_idle(g);
	}

	nvgpu_log(g, gpu_dbg_fn|gpu_dbg_gpu_dbg, "%s powergate mode = %s done",
		   g->name, disable_powergate ? "disable" : "enable");
	return err;
}
