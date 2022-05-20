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

#include <nvgpu/trace.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/timers.h>
#include <nvgpu/log.h>
#include <nvgpu/io.h>
#include <nvgpu/debug.h>
#include <nvgpu/fifo.h>
#include <nvgpu/runlist.h>
#include <nvgpu/engines.h>
#include <nvgpu/engine_status.h>
#include <nvgpu/power_features/cg.h>
#include <nvgpu/power_features/pg.h>
#include <nvgpu/power_features/power_features.h>
#include <nvgpu/gr/fecs_trace.h>
#include <nvgpu/channel.h>
#include <nvgpu/tsg.h>

#include <hal/fifo/mmu_fault_gk20a.h>

#include <nvgpu/hw/gk20a/hw_fifo_gk20a.h>

/* fault info/descriptions */

static const char * const gk20a_fault_type_descs[] = {
	 "pde", /*fifo_intr_mmu_fault_info_type_pde_v() == 0 */
	 "pde size",
	 "pte",
	 "va limit viol",
	 "unbound inst",
	 "priv viol",
	 "ro viol",
	 "wo viol",
	 "pitch mask",
	 "work creation",
	 "bad aperture",
	 "compression failure",
	 "bad kind",
	 "region viol",
	 "dual ptes",
	 "poisoned",
};

/* engine descriptions */
static const char * const engine_subid_descs[] = {
	"gpc",
	"hub",
};

static const char * const gk20a_hub_client_descs[] = {
	"vip", "ce0", "ce1", "dniso", "fe", "fecs", "host", "host cpu",
	"host cpu nb", "iso", "mmu", "mspdec", "msppp", "msvld",
	"niso", "p2p", "pd", "perf", "pmu", "raster twod", "scc",
	"scc nb", "sec", "ssync", "gr copy", "xv", "mmu nb",
	"msenc", "d falcon", "sked", "a falcon", "n/a",
};

static const char * const gk20a_gpc_client_descs[] = {
	"l1 0", "t1 0", "pe 0",
	"l1 1", "t1 1", "pe 1",
	"l1 2", "t1 2", "pe 2",
	"l1 3", "t1 3", "pe 3",
	"rast", "gcc", "gpccs",
	"prop 0", "prop 1", "prop 2", "prop 3",
	"l1 4", "t1 4", "pe 4",
	"l1 5", "t1 5", "pe 5",
	"l1 6", "t1 6", "pe 6",
	"l1 7", "t1 7", "pe 7",
};

static const char * const does_not_exist[] = {
	"does not exist"
};

/* fill in mmu fault desc */
void gk20a_fifo_get_mmu_fault_desc(struct mmu_fault_info *mmufault)
{
	if (mmufault->fault_type >= ARRAY_SIZE(gk20a_fault_type_descs)) {
		WARN_ON(mmufault->fault_type >=
				ARRAY_SIZE(gk20a_fault_type_descs));
	} else {
		mmufault->fault_type_desc =
			 gk20a_fault_type_descs[mmufault->fault_type];
	}
}

/* fill in mmu fault client description */
void gk20a_fifo_get_mmu_fault_client_desc(struct mmu_fault_info *mmufault)
{
	if (mmufault->client_id >= ARRAY_SIZE(gk20a_hub_client_descs)) {
		WARN_ON(mmufault->client_id >=
				ARRAY_SIZE(gk20a_hub_client_descs));
	} else {
		mmufault->client_id_desc =
			 gk20a_hub_client_descs[mmufault->client_id];
	}
}

/* fill in mmu fault gpc description */
void gk20a_fifo_get_mmu_fault_gpc_desc(struct mmu_fault_info *mmufault)
{
	if (mmufault->client_id >= ARRAY_SIZE(gk20a_gpc_client_descs)) {
		WARN_ON(mmufault->client_id >=
				ARRAY_SIZE(gk20a_gpc_client_descs));
	} else {
		mmufault->client_id_desc =
			 gk20a_gpc_client_descs[mmufault->client_id];
	}
}

static void gk20a_fifo_parse_mmu_fault_info(struct gk20a *g, u32 mmu_fault_id,
	struct mmu_fault_info *mmufault)
{
	g->ops.fifo.get_mmu_fault_info(g, mmu_fault_id, mmufault);

	/* parse info */
	mmufault->fault_type_desc =  does_not_exist[0];
	if (g->ops.fifo.get_mmu_fault_desc != NULL) {
		g->ops.fifo.get_mmu_fault_desc(mmufault);
	}

	if (mmufault->client_type >= ARRAY_SIZE(engine_subid_descs)) {
		WARN_ON(mmufault->client_type >=
				ARRAY_SIZE(engine_subid_descs));
		mmufault->client_type_desc = does_not_exist[0];
	} else {
		mmufault->client_type_desc =
				 engine_subid_descs[mmufault->client_type];
	}

	mmufault->client_id_desc = does_not_exist[0];
	if ((mmufault->client_type ==
		fifo_intr_mmu_fault_info_engine_subid_hub_v())
		&& (g->ops.fifo.get_mmu_fault_client_desc != NULL)) {
		g->ops.fifo.get_mmu_fault_client_desc(mmufault);
	} else if ((mmufault->client_type ==
			fifo_intr_mmu_fault_info_engine_subid_gpc_v())
			&& (g->ops.fifo.get_mmu_fault_gpc_desc != NULL)) {
		g->ops.fifo.get_mmu_fault_gpc_desc(mmufault);
	} else {
		mmufault->client_id_desc = does_not_exist[0];
	}
}

/* reads info from hardware and fills in mmu fault info record */
void gk20a_fifo_get_mmu_fault_info(struct gk20a *g, u32 mmu_fault_id,
	struct mmu_fault_info *mmufault)
{
	u32 fault_info;
	u32 addr_lo, addr_hi;

	nvgpu_log_fn(g, "mmu_fault_id %d", mmu_fault_id);

	(void) memset(mmufault, 0, sizeof(*mmufault));

	fault_info = nvgpu_readl(g,
		fifo_intr_mmu_fault_info_r(mmu_fault_id));
	mmufault->fault_type =
		fifo_intr_mmu_fault_info_type_v(fault_info);
	mmufault->access_type =
		fifo_intr_mmu_fault_info_write_v(fault_info);
	mmufault->client_type =
		fifo_intr_mmu_fault_info_engine_subid_v(fault_info);
	mmufault->client_id =
		fifo_intr_mmu_fault_info_client_v(fault_info);

	addr_lo = nvgpu_readl(g, fifo_intr_mmu_fault_lo_r(mmu_fault_id));
	addr_hi = nvgpu_readl(g, fifo_intr_mmu_fault_hi_r(mmu_fault_id));
	mmufault->fault_addr = hi32_lo32_to_u64(addr_hi, addr_lo);
	/* note:ignoring aperture on gk20a... */
	mmufault->inst_ptr = fifo_intr_mmu_fault_inst_ptr_v(
		 nvgpu_readl(g, fifo_intr_mmu_fault_inst_r(mmu_fault_id)));
	/* note: inst_ptr is a 40b phys addr.  */
	mmufault->inst_ptr <<= fifo_intr_mmu_fault_inst_ptr_align_shift_v();
}

void gk20a_fifo_mmu_fault_info_dump(struct gk20a *g, u32 engine_id,
	u32 mmu_fault_id, bool fake_fault, struct mmu_fault_info *mmufault)
{
	gk20a_fifo_parse_mmu_fault_info(g, mmu_fault_id, mmufault);

#ifdef CONFIG_NVGPU_TRACE
	trace_gk20a_mmu_fault(mmufault->fault_addr,
			      mmufault->fault_type,
			      mmufault->access_type,
			      mmufault->inst_ptr,
			      engine_id,
			      mmufault->client_type_desc,
			      mmufault->client_id_desc,
			      mmufault->fault_type_desc);
#endif
	nvgpu_err(g, "MMU fault @ address: 0x%llx %s",
		  mmufault->fault_addr,
		  fake_fault ? "[FAKE]" : "");
	nvgpu_err(g, "  Engine: %d  subid: %d (%s)",
		  (int)engine_id,
		  mmufault->client_type, mmufault->client_type_desc);
	nvgpu_err(g, "  Cient %d (%s), ",
		  mmufault->client_id, mmufault->client_id_desc);
	nvgpu_err(g, "  Tpe %d (%s); access_type 0x%08x; inst_ptr 0x%llx",
		  mmufault->fault_type,
		  mmufault->fault_type_desc,
		  mmufault->access_type, mmufault->inst_ptr);
}

void gk20a_fifo_handle_dropped_mmu_fault(struct gk20a *g)
{
	u32 fault_id = nvgpu_readl(g, fifo_intr_mmu_fault_id_r());

	nvgpu_err(g, "dropped mmu fault (0x%08x)", fault_id);
}

void gk20a_fifo_handle_mmu_fault_locked(
	struct gk20a *g,
	u32 mmu_fault_engines, /* queried from HW if 0 */
	u32 hw_id, /* queried from HW if ~(u32)0 OR mmu_fault_engines == 0*/
	bool id_is_tsg)
{
	bool fake_fault;
	unsigned long fault_id;
	unsigned long engine_mmu_fault_id;
	struct nvgpu_engine_status_info engine_status;
	bool deferred_reset_pending = false;

	nvgpu_log_fn(g, " ");

	if (nvgpu_cg_pg_disable(g) != 0) {
		nvgpu_warn(g, "fail to disable power mgmt");
	}

	/* Disable fifo access */
	g->ops.gr.init.fifo_access(g, false);

	if (mmu_fault_engines != 0U) {
		fault_id = mmu_fault_engines;
		fake_fault = true;
	} else {
		fault_id = nvgpu_readl(g, fifo_intr_mmu_fault_id_r());
		fake_fault = false;
	}
#ifdef CONFIG_NVGPU_DEBUGGER
	nvgpu_mutex_acquire(&g->fifo.deferred_reset_mutex);
	g->fifo.deferred_reset_pending = false;
	nvgpu_mutex_release(&g->fifo.deferred_reset_mutex);
#endif

	/* go through all faulted engines */
	for_each_set_bit(engine_mmu_fault_id, &fault_id, 32U) {
		/*
		 * bits in fifo_intr_mmu_fault_id_r do not correspond 1:1 to
		 * engines. Convert engine_mmu_id to engine_id
		 */
		u32 engine_id = nvgpu_engine_mmu_fault_id_to_engine_id(g,
					(u32)engine_mmu_fault_id);
		struct mmu_fault_info mmfault_info;
		struct nvgpu_channel *ch = NULL;
		struct nvgpu_tsg *tsg = NULL;
		struct nvgpu_channel *refch = NULL;
		bool ctxsw;

		/* read and parse engine status */
		g->ops.engine_status.read_engine_status_info(g, engine_id,
			&engine_status);

		ctxsw = nvgpu_engine_status_is_ctxsw(&engine_status);

		gk20a_fifo_mmu_fault_info_dump(g, engine_id,
				(u32)engine_mmu_fault_id,
				fake_fault, &mmfault_info);

		if (ctxsw) {
			g->ops.gr.falcon.dump_stats(g);
#ifdef CONFIG_NVGPU_DEBUGGER
			nvgpu_err(g, "  gr_status_r: 0x%x",
				  g->ops.gr.get_gr_status(g));
#endif
		}

		/* get the channel/TSG */
		if (fake_fault) {
			/* use next_id if context load is failing */
			u32 id, type;

			if (hw_id == ~(u32)0) {
				if (nvgpu_engine_status_is_ctxsw_load(
					&engine_status)) {
					nvgpu_engine_status_get_next_ctx_id_type(
						&engine_status, &id, &type);
				} else {
					nvgpu_engine_status_get_ctx_id_type(
						&engine_status, &id, &type);
				}
			} else {
				id = hw_id;
				type = id_is_tsg ?
					ENGINE_STATUS_CTX_ID_TYPE_TSGID :
					ENGINE_STATUS_CTX_ID_TYPE_CHID;
			}

			if (type == ENGINE_STATUS_CTX_ID_TYPE_TSGID) {
				tsg = nvgpu_tsg_get_from_id(g, id);
			} else if (type == ENGINE_STATUS_CTX_ID_TYPE_CHID) {
				ch = &g->fifo.channel[id];
				refch = nvgpu_channel_get(ch);
				if (refch != NULL) {
					tsg = nvgpu_tsg_from_ch(refch);
				}
			} else {
				nvgpu_err(g, "ctx_id_type is not chid/tsgid");
			}
		} else {
			/* Look up channel from the inst block pointer. */
			ch = nvgpu_channel_refch_from_inst_ptr(g,
					mmfault_info.inst_ptr);
			refch = ch;
			if (refch != NULL) {
				tsg = nvgpu_tsg_from_ch(refch);
			}
		}

		/* Set unserviceable flag right at start of recovery to reduce
		 * the window of race between job submit and recovery on same
		 * TSG.
		 * The unserviceable flag is checked during job submit and
		 * prevent new jobs from being submitted to TSG which is headed
		 * for teardown.
		 */
		if (tsg != NULL) {
			/* Set error notifier before letting userspace
			 * know about faulty channel.
			 * The unserviceable flag is moved early to
			 * disallow submits on the broken channel. If
			 * userspace checks the notifier code when a
			 * submit fails, we need it set to convey to
			 * userspace that channel is no longer usable.
			 */
			if (!fake_fault) {
				/*
				 * If a debugger is attached and debugging is
				 * enabled, then do not set error notifier as
				 * it will cause the application to teardown the
				 * channels and debugger will not be able to
				 * collect any data.
				 */
#ifdef CONFIG_NVGPU_DEBUGGER
				if (!nvgpu_engine_should_defer_reset(g,
					mmfault_info.faulted_engine,
					mmfault_info.client_type, false)) {
#endif
					nvgpu_tsg_set_ctx_mmu_error(g, tsg);
#ifdef CONFIG_NVGPU_DEBUGGER
				}
#endif
			}
			nvgpu_tsg_set_unserviceable(g, tsg);
		}

		/* check if engine reset should be deferred */
		if (engine_id != NVGPU_INVALID_ENG_ID) {
#ifdef CONFIG_NVGPU_DEBUGGER
			bool defer = nvgpu_engine_should_defer_reset(g,
					engine_id, mmfault_info.client_type,
					fake_fault);
			if (((ch != NULL) || (tsg != NULL)) && defer) {
				g->fifo.deferred_fault_engines |= BIT(engine_id);

				/* handled during channel free */
				nvgpu_mutex_acquire(&g->fifo.deferred_reset_mutex);
				g->fifo.deferred_reset_pending = true;
				nvgpu_mutex_release(&g->fifo.deferred_reset_mutex);

				deferred_reset_pending = true;

				nvgpu_log(g, gpu_dbg_intr | gpu_dbg_gpu_dbg,
					   "sm debugger attached,"
					   " deferring channel recovery to channel free");
			} else {
#endif
#ifdef CONFIG_NVGPU_ENGINE_RESET
				nvgpu_engine_reset(g, engine_id);
#endif
#ifdef CONFIG_NVGPU_DEBUGGER
			}
#endif
		}

#ifdef CONFIG_NVGPU_FECS_TRACE
		if (tsg != NULL) {
			nvgpu_gr_fecs_trace_add_tsg_reset(g, tsg);
		}
#endif
		/*
		 * Disable the channel/TSG from hw and increment syncpoints.
		 */
		if (tsg != NULL) {
			if (deferred_reset_pending) {
				g->ops.tsg.disable(tsg);
			} else {
				nvgpu_tsg_wakeup_wqs(g, tsg);
				nvgpu_tsg_abort(g, tsg, false);
			}

			/* put back the ref taken early above */
			if (refch != NULL) {
				nvgpu_channel_put(ch);
			}
		} else if (refch != NULL) {
			nvgpu_err(g, "mmu error in unbound channel %d",
					  ch->chid);
			nvgpu_channel_put(ch);
		} else if (mmfault_info.inst_ptr ==
				nvgpu_inst_block_addr(g,
					&g->mm.bar1.inst_block)) {
			nvgpu_err(g, "mmu fault from bar1");
		} else if (mmfault_info.inst_ptr ==
				nvgpu_inst_block_addr(g,
					&g->mm.pmu.inst_block)) {
			nvgpu_err(g, "mmu fault from pmu");
		} else {
			nvgpu_err(g, "couldn't locate channel for mmu fault");
		}
	}

	if (!fake_fault) {
		gk20a_debug_dump(g);
	}

	/* clear interrupt */
	nvgpu_writel(g, fifo_intr_mmu_fault_id_r(), (u32)fault_id);

	/* resume scheduler */
	nvgpu_writel(g, fifo_error_sched_disable_r(),
		     nvgpu_readl(g, fifo_error_sched_disable_r()));

	/* Re-enable fifo access */
	g->ops.gr.init.fifo_access(g, true);

	if (nvgpu_cg_pg_enable(g) != 0) {
		nvgpu_warn(g, "fail to enable power mgmt");
	}
}

void gk20a_fifo_handle_mmu_fault(
	struct gk20a *g,
	u32 mmu_fault_engines, /* queried from HW if 0 */
	u32 hw_id, /* queried from HW if ~(u32)0 OR mmu_fault_engines == 0*/
	bool id_is_tsg)
{
	nvgpu_log_fn(g, " ");

	nvgpu_log_info(g, "acquire engines_reset_mutex");
	nvgpu_mutex_acquire(&g->fifo.engines_reset_mutex);

	nvgpu_runlist_lock_active_runlists(g);

	gk20a_fifo_handle_mmu_fault_locked(g, mmu_fault_engines,
			hw_id, id_is_tsg);

	nvgpu_runlist_unlock_active_runlists(g);

	nvgpu_log_info(g, "release engines_reset_mutex");
	nvgpu_mutex_release(&g->fifo.engines_reset_mutex);
}
