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


#include <nvgpu/log.h>
#include <nvgpu/errno.h>
#include <nvgpu/timers.h>
#include <nvgpu/bitops.h>
#ifdef CONFIG_NVGPU_LS_PMU
#include <nvgpu/pmu.h>
#include <nvgpu/pmu/mutex.h>
#endif
#include <nvgpu/runlist.h>
#include <nvgpu/engines.h>
#include <nvgpu/engine_status.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/pbdma_status.h>
#include <nvgpu/power_features/pg.h>
#include <nvgpu/channel.h>
#include <nvgpu/soc.h>
#include <nvgpu/device.h>
#include <nvgpu/gr/gr_falcon.h>
#include <nvgpu/gr/gr.h>
#include <nvgpu/gr/gr_instances.h>
#include <nvgpu/fifo.h>
#include <nvgpu/static_analysis.h>
#include <nvgpu/swprofile.h>

#include <nvgpu/fifo/swprofile.h>

#define FECS_METHOD_WFI_RESTORE	0x80000U

enum nvgpu_fifo_engine nvgpu_engine_enum_from_dev(struct gk20a *g,
			const struct nvgpu_device *dev)
{
	enum nvgpu_fifo_engine ret = NVGPU_ENGINE_INVAL;

	if (nvgpu_device_is_graphics(g, dev)) {
		ret = NVGPU_ENGINE_GR;
	} else if (nvgpu_device_is_ce(g, dev)) {
		/* For now, all CE engines have separate runlists. We can
		 * identify the NVGPU_ENGINE_GRCE type CE using runlist_id
		 * comparsion logic with GR runlist_id in init_info()
		 */
		ret = NVGPU_ENGINE_ASYNC_CE;
	} else {
		ret = NVGPU_ENGINE_INVAL;
	}

	return ret;
}

const struct nvgpu_device *nvgpu_engine_get_active_eng_info(
	struct gk20a *g, u32 engine_id)
{
	struct nvgpu_fifo *f = &g->fifo;

	if (engine_id >= f->max_engines) {
		return NULL;
	}

	return f->host_engines[engine_id];
}

bool nvgpu_engine_check_valid_id(struct gk20a *g, u32 engine_id)
{
	struct nvgpu_fifo *f = &g->fifo;

	if (engine_id >= f->max_engines) {
		return false;
	}

	return f->host_engines[engine_id] != NULL;
}

u32 nvgpu_engine_get_gr_id_for_inst(struct gk20a *g, u32 inst_id)
{
	const struct nvgpu_device *dev;

	dev = nvgpu_device_get(g, NVGPU_DEVTYPE_GRAPHICS, inst_id);
	if (dev == NULL) {
		nvgpu_warn(g, "No GR devices on this GPU for inst[%u]?!",
			inst_id);
		return NVGPU_INVALID_ENG_ID;
	}

	return dev->engine_id;
}

u32 nvgpu_engine_get_gr_id(struct gk20a *g)
{
	/* Consider 1st available GR engine */
	return nvgpu_engine_get_gr_id_for_inst(g, 0U);
}

u32 nvgpu_engine_act_interrupt_mask(struct gk20a *g, u32 engine_id)
{
	const struct nvgpu_device *dev = NULL;

	dev = nvgpu_engine_get_active_eng_info(g, engine_id);
	if (dev == NULL) {
		return 0;
	}

	return BIT32(dev->intr_id);
}

u32 nvgpu_gr_engine_interrupt_mask(struct gk20a *g)
{
	const struct nvgpu_device *dev;
	u32 intr_mask = 0U;
	u32 i;

	for (i = 0U; i < g->num_gr_instances; i++) {
		dev = nvgpu_device_get(g, NVGPU_DEVTYPE_GRAPHICS,
				nvgpu_gr_get_syspipe_id(g, i));
		if (dev == NULL) {
			continue;
		}

		intr_mask |= BIT32(dev->intr_id);
	}

	return intr_mask;
}

u32 nvgpu_ce_engine_interrupt_mask(struct gk20a *g)
{
	const struct nvgpu_device *dev;
	u32 i;
	u32 mask = 0U;

	/*
	 * For old chips - pre-Pascal - we have COPY[0-2], for new chips we
	 * have some number of LCE instances. For the purpose of this code we
	 * imagine a system that could have both; in reality that'll never be
	 * the case.
	 *
	 * This can be cleaned up in the future by defining a SW type for CE and
	 * hiding this ugliness in the device management code.
	 */
	for (i = NVGPU_DEVTYPE_COPY0;  i <= NVGPU_DEVTYPE_COPY2; i++) {
		dev = nvgpu_device_get(g, i, i - NVGPU_DEVTYPE_COPY0);
		if (dev == NULL) {
			continue;
		}

		mask |= BIT32(dev->intr_id);
	}

	/*
	 * Now take care of LCEs.
	 */
	nvgpu_device_for_each(g, dev, NVGPU_DEVTYPE_LCE) {
		mask |= BIT32(dev->intr_id);
	}

	return mask;
}

#ifdef CONFIG_NVGPU_FIFO_ENGINE_ACTIVITY

static void nvgpu_engine_enable_activity(struct gk20a *g,
					 const struct nvgpu_device *dev)
{
	nvgpu_runlist_set_state(g, BIT32(dev->runlist_id), RUNLIST_ENABLED);
}

void nvgpu_engine_enable_activity_all(struct gk20a *g)
{
	u32 i;

	for (i = 0; i < g->fifo.num_engines; i++) {
		nvgpu_engine_enable_activity(g, g->fifo.active_engines[i]);
	}
}

int nvgpu_engine_disable_activity(struct gk20a *g,
				const struct nvgpu_device *dev,
				bool wait_for_idle)
{
	u32 pbdma_chid = NVGPU_INVALID_CHANNEL_ID;
	u32 engine_chid = NVGPU_INVALID_CHANNEL_ID;
#ifdef CONFIG_NVGPU_LS_PMU
	u32 token = PMU_INVALID_MUTEX_OWNER_ID;
	int mutex_ret = -EINVAL;
#endif
	int err = 0;
	struct nvgpu_channel *ch = NULL;
	struct nvgpu_engine_status_info engine_status;
	struct nvgpu_pbdma_status_info pbdma_status;
	unsigned long runlist_served_pbdmas;
	unsigned long bit;
	u32 pbdma_id;
	struct nvgpu_fifo *f = &g->fifo;

	nvgpu_log_fn(g, " ");

	g->ops.engine_status.read_engine_status_info(g, dev->engine_id,
		 &engine_status);
	if (engine_status.is_busy && !wait_for_idle) {
		return -EBUSY;
	}

#ifdef CONFIG_NVGPU_LS_PMU
	if (g->ops.pmu.is_pmu_supported(g)) {
		mutex_ret = nvgpu_pmu_lock_acquire(g, g->pmu,
						PMU_MUTEX_ID_FIFO, &token);
	}
#endif

	nvgpu_runlist_set_state(g, BIT32(dev->runlist_id),
			RUNLIST_DISABLED);

	runlist_served_pbdmas = f->runlists[dev->runlist_id]->pbdma_bitmask;

	for_each_set_bit(bit, &runlist_served_pbdmas,
			 nvgpu_get_litter_value(g, GPU_LIT_HOST_NUM_PBDMA)) {
		pbdma_id = U32(bit);
		/* chid from pbdma status */
		g->ops.pbdma_status.read_pbdma_status_info(g,
						pbdma_id,
						&pbdma_status);
		if (nvgpu_pbdma_status_is_chsw_valid(&pbdma_status) ||
			nvgpu_pbdma_status_is_chsw_save(&pbdma_status)) {
			pbdma_chid = pbdma_status.id;
		} else if (nvgpu_pbdma_status_is_chsw_load(&pbdma_status) ||
			nvgpu_pbdma_status_is_chsw_switch(&pbdma_status)) {
			pbdma_chid = pbdma_status.next_id;
		} else {
			/* Nothing to do here */
		}

		if (pbdma_chid != NVGPU_INVALID_CHANNEL_ID) {
			ch = nvgpu_channel_from_id(g, pbdma_chid);
			if (ch != NULL) {
				err = g->ops.fifo.preempt_channel(g, ch);
				nvgpu_channel_put(ch);
			}
			if (err != 0) {
				goto clean_up;
			}
		}
	}

	/* chid from engine status */
	g->ops.engine_status.read_engine_status_info(g, dev->engine_id,
		 &engine_status);
	if (nvgpu_engine_status_is_ctxsw_valid(&engine_status) ||
	    nvgpu_engine_status_is_ctxsw_save(&engine_status)) {
		engine_chid = engine_status.ctx_id;
	} else if (nvgpu_engine_status_is_ctxsw_switch(&engine_status) ||
	    nvgpu_engine_status_is_ctxsw_load(&engine_status)) {
		engine_chid = engine_status.ctx_next_id;
	} else {
		/* Nothing to do here */
	}

	if (engine_chid != NVGPU_INVALID_ENG_ID && engine_chid != pbdma_chid) {
		ch = nvgpu_channel_from_id(g, engine_chid);
		if (ch != NULL) {
			err = g->ops.fifo.preempt_channel(g, ch);
			nvgpu_channel_put(ch);
		}
		if (err != 0) {
			goto clean_up;
		}
	}

clean_up:
#ifdef CONFIG_NVGPU_LS_PMU
	if (mutex_ret == 0) {
		if (nvgpu_pmu_lock_release(g, g->pmu,
			PMU_MUTEX_ID_FIFO, &token) != 0){
			nvgpu_err(g, "failed to release PMU lock");
		}
	}
#endif
	if (err != 0) {
		nvgpu_log_fn(g, "failed");
		nvgpu_engine_enable_activity(g, dev);
	} else {
		nvgpu_log_fn(g, "done");
	}
	return err;
}

int nvgpu_engine_disable_activity_all(struct gk20a *g,
					   bool wait_for_idle)
{
	unsigned int i;
	int err = 0, ret = 0;

	for (i = 0; i < g->fifo.num_engines; i++) {
		err = nvgpu_engine_disable_activity(g,
				g->fifo.active_engines[i],
				wait_for_idle);
		if (err != 0) {
			nvgpu_err(g, "failed to disable engine %d activity",
				  g->fifo.active_engines[i]->engine_id);
			ret = err;
			break;
		}
	}

	if (err != 0) {
		while (i-- != 0U) {
			nvgpu_engine_enable_activity(g,
					g->fifo.active_engines[i]);
		}
	}

	return ret;
}

int nvgpu_engine_wait_for_idle(struct gk20a *g)
{
	struct nvgpu_timeout timeout;
	u32 delay = POLL_DELAY_MIN_US;
	int ret = 0;
	u32 i, host_num_engines;
	struct nvgpu_engine_status_info engine_status;

	nvgpu_log_fn(g, " ");

	host_num_engines =
		 nvgpu_get_litter_value(g, GPU_LIT_HOST_NUM_ENGINES);

	nvgpu_timeout_init_cpu_timer(g, &timeout, nvgpu_get_poll_timeout(g));

	for (i = 0; i < host_num_engines; i++) {
		if (!nvgpu_engine_check_valid_id(g, i)) {
			continue;
		}

		ret = -ETIMEDOUT;
		do {
			g->ops.engine_status.read_engine_status_info(g, i,
				&engine_status);
			if (!engine_status.is_busy) {
				ret = 0;
				break;
			}

			nvgpu_usleep_range(delay, delay * 2U);
			delay = min_t(u32,
					delay << 1U, POLL_DELAY_MAX_US);
		} while (nvgpu_timeout_expired(&timeout) == 0);

		if (ret != 0) {
			/* possible causes:
			 * check register settings programmed in hal set by
			 * elcg_init_idle_filters and init_therm_setup_hw
			 */
			nvgpu_err(g, "cannot idle engine: %u "
					"engine_status: 0x%08x", i,
					engine_status.reg_data);
			break;
		}
	}

	nvgpu_log_fn(g, "done");

	return ret;
}

#endif /* CONFIG_NVGPU_FIFO_ENGINE_ACTIVITY */

int nvgpu_engine_setup_sw(struct gk20a *g)
{
	struct nvgpu_fifo *f = &g->fifo;
	int err = 0;
	size_t size;

	f->max_engines = nvgpu_get_litter_value(g, GPU_LIT_HOST_NUM_ENGINES);
	size = nvgpu_safe_mult_u64(f->max_engines,
				   sizeof(struct nvgpu_device *));

	/*
	 * Allocate the two device lists for host devices.
	 */
	f->host_engines = nvgpu_kzalloc(g, size);
	if (f->host_engines == NULL) {
		nvgpu_err(g, "OOM allocating host engine list");
		return -ENOMEM;
	}
	f->active_engines = nvgpu_kzalloc(g, size);
	if (f->active_engines == NULL) {
		nvgpu_err(g, "no mem for active engine list");
		err = -ENOMEM;
		goto clean_up_engine_info;
	}

	err = nvgpu_engine_init_info(f);
	if (err != 0) {
		nvgpu_err(g, "init engine info failed");
		goto clean_up;
	}

	return 0;

clean_up:
	nvgpu_kfree(g, f->active_engines);
	f->active_engines = NULL;

clean_up_engine_info:
	nvgpu_kfree(g, f->host_engines);
	f->host_engines = NULL;

	return err;
}

void nvgpu_engine_cleanup_sw(struct gk20a *g)
{
	struct nvgpu_fifo *f = &g->fifo;

	f->num_engines = 0;
	nvgpu_kfree(g, f->host_engines);
	f->host_engines = NULL;
	nvgpu_kfree(g, f->active_engines);
	f->active_engines = NULL;
}

#ifdef CONFIG_NVGPU_ENGINE_RESET
static void nvgpu_engine_gr_reset(struct gk20a *g)
{
	struct nvgpu_swprofiler *prof = &g->fifo.eng_reset_profiler;
	int err = 0;

	nvgpu_swprofile_snapshot(prof, PROF_ENG_RESET_PREAMBLE);

#ifdef CONFIG_NVGPU_POWER_PG
	if (nvgpu_pg_elpg_disable(g) != 0 ) {
		nvgpu_err(g, "failed to set disable elpg");
	}
#endif
	nvgpu_swprofile_snapshot(prof, PROF_ENG_RESET_ELPG_DISABLE);

#ifdef CONFIG_NVGPU_FECS_TRACE
	/*
	 * Resetting engine will alter read/write index. Need to flush
	 * circular buffer before re-enabling FECS.
	 */
	if (g->ops.gr.fecs_trace.reset != NULL) {
		if (g->ops.gr.fecs_trace.reset(g) != 0) {
			nvgpu_warn(g, "failed to reset fecs traces");
		}
	}
#endif

	nvgpu_swprofile_snapshot(prof, PROF_ENG_RESET_FECS_TRACE_RESET);

	/*
	 * HALT_PIPELINE method and gr reset during recovery is supported
	 * starting nvgpu-next simulation.
	 */
	err = g->ops.gr.falcon.ctrl_ctxsw(g,
			NVGPU_GR_FALCON_METHOD_HALT_PIPELINE, 0U, NULL);
	if (err != 0) {
		nvgpu_err(g, "failed to halt gr pipe");
	}

	nvgpu_swprofile_snapshot(prof, PROF_ENG_RESET_HALT_PIPELINE);

	/*
	 * resetting only engine is not
	 * enough, we do full init sequence
	 */
	nvgpu_log(g, gpu_dbg_rec, "resetting gr engine");

	err = nvgpu_gr_reset(g);
	if (err != 0) {
		nvgpu_err(g, "failed to reset gr engine");
	}

#ifdef CONFIG_NVGPU_POWER_PG
	if (nvgpu_pg_elpg_enable(g) != 0) {
		nvgpu_err(g, "failed to set enable elpg");
	}
	nvgpu_swprofile_snapshot(prof, PROF_ENG_RESET_ELPG_REENABLE);
#endif
}

void nvgpu_engine_reset(struct gk20a *g, u32 engine_id)
{
	struct nvgpu_swprofiler *prof = &g->fifo.eng_reset_profiler;
	const struct nvgpu_device *dev;
	int err = 0;
	u32 gr_instance_id;

	nvgpu_log_fn(g, " ");

	nvgpu_swprofile_begin_sample(prof);

	dev = nvgpu_engine_get_active_eng_info(g, engine_id);
	if (dev == NULL) {
		nvgpu_err(g, "unsupported engine_id %d", engine_id);
		return;
	}

	if (!nvgpu_device_is_ce(g, dev) &&
	    !nvgpu_device_is_graphics(g, dev)) {
		nvgpu_warn(g, "Ignoring reset for non-host engine.");
		return;
	}

	/*
	 * Simple case first: reset a copy engine.
	 */
	if (nvgpu_device_is_ce(g, dev)) {
		if (g->ops.ce.halt_engine != NULL) {
			g->ops.ce.halt_engine(g, dev);
		}
		err = nvgpu_mc_reset_dev(g, dev);
		if (g->ops.ce.request_idle != NULL) {
			/*
			 * Read CE register for CE to switch
			 * from reset to idle state.
			 */
			g->ops.ce.request_idle(g);
		}
		if (err != 0) {
			nvgpu_log_info(g, "CE engine [id:%u] reset failed",
				dev->engine_id);
		}
		return;
	}

	/*
	 * Now reset a GR engine.
	 */
	gr_instance_id =
		nvgpu_grmgr_get_gr_instance_id_for_syspipe(
			g, dev->inst_id);

	nvgpu_gr_exec_for_instance(g,
		gr_instance_id, nvgpu_engine_gr_reset(g));
}
#endif

u32 nvgpu_engine_get_fast_ce_runlist_id(struct gk20a *g)
{
	const struct nvgpu_device *dev;
	u32 nr_lces;
	u32 i;

	/*
	 * Obtain a runlist ID for the fastest available CE. The priority order
	 * is:
	 *
	 *   1. Last available LCE
	 *   2. Last available COPY[0-2]
	 *   3. GRAPHICS runlist as a last resort.
	 */
	nr_lces = nvgpu_device_count(g, NVGPU_DEVTYPE_LCE);
	if (nr_lces > 0U) {
		dev = nvgpu_device_get(g,
				       NVGPU_DEVTYPE_LCE,
				       nr_lces - 1U);
		nvgpu_assert(dev != NULL);

		return dev->runlist_id;
	}

	/*
	 * Note: this only works since NVGPU_DEVTYPE_GRAPHICS is 0 and the COPYx
	 * are all > 0.
	 */
	for (i = NVGPU_DEVTYPE_COPY2; i >= NVGPU_DEVTYPE_COPY0; i--) {
		dev = nvgpu_device_get(g, i, i - NVGPU_DEVTYPE_COPY0);
		if (dev != NULL) {
			return dev->runlist_id;
		}
	}

	/*
	 * Fall back to GR.
	 */
	dev = nvgpu_device_get(g, NVGPU_DEVTYPE_GRAPHICS, 0);
	nvgpu_assert(dev != NULL);

	return dev->runlist_id;
}

u32 nvgpu_engine_get_gr_runlist_id(struct gk20a *g)
{
	const struct nvgpu_device *dev;

	dev = nvgpu_device_get(g, NVGPU_DEVTYPE_GRAPHICS, 0);
	if (dev == NULL) {
		nvgpu_warn(g, "No GR device on this GPU?!");
		return NVGPU_INVALID_RUNLIST_ID;
	}

	return dev->runlist_id;
}

bool nvgpu_engine_is_valid_runlist_id(struct gk20a *g, u32 runlist_id)
{
	u32 i;
	struct nvgpu_fifo *f = &g->fifo;

	for (i = 0U; i < f->num_engines; i++) {
		const struct nvgpu_device *dev = f->active_engines[i];

		if (dev->runlist_id == runlist_id) {
			return true;
		}
	}

	return false;
}

/*
 * Link engine IDs to MMU IDs and vice versa.
 */
u32 nvgpu_engine_id_to_mmu_fault_id(struct gk20a *g, u32 engine_id)
{
	const struct nvgpu_device *dev;

	dev = nvgpu_engine_get_active_eng_info(g, engine_id);

	if (dev == NULL) {
		nvgpu_err(g,
			  "engine_id: %u is not in active list",
			  engine_id);
		return NVGPU_INVALID_ENG_ID;
	}

	return dev->fault_id;
}

u32 nvgpu_engine_mmu_fault_id_to_engine_id(struct gk20a *g, u32 fault_id)
{
	u32 i;
	const struct nvgpu_device *dev;
	struct nvgpu_fifo *f = &g->fifo;

	for (i = 0U; i < f->num_engines; i++) {
		dev = f->active_engines[i];

		if (dev->fault_id == fault_id) {
			return dev->engine_id;
		}
	}

	return NVGPU_INVALID_ENG_ID;
}

u32 nvgpu_engine_get_mask_on_id(struct gk20a *g, u32 id, bool is_tsg)
{
	unsigned int i;
	u32 engines = 0;
	struct nvgpu_engine_status_info engine_status;
	u32 ctx_id;
	u32 type;
	bool busy;

	for (i = 0; i < g->fifo.num_engines; i++) {
		const struct nvgpu_device *dev = g->fifo.active_engines[i];

		g->ops.engine_status.read_engine_status_info(g,
			dev->engine_id, &engine_status);

		if (nvgpu_engine_status_is_ctxsw_load(
			&engine_status)) {
			nvgpu_engine_status_get_next_ctx_id_type(
				&engine_status, &ctx_id, &type);
		} else {
			nvgpu_engine_status_get_ctx_id_type(
				&engine_status, &ctx_id, &type);
		}

		busy = engine_status.is_busy;

		if (!busy || !(ctx_id == id)) {
			continue;
		}

		if ((is_tsg  && (type == ENGINE_STATUS_CTX_ID_TYPE_TSGID)) ||
		    (!is_tsg && (type == ENGINE_STATUS_CTX_ID_TYPE_CHID))) {
			engines |= BIT32(dev->engine_id);
		}
	}

	return engines;
}


int nvgpu_engine_init_one_dev_extra(struct gk20a *g,
		const struct nvgpu_device *dev)
{
	struct nvgpu_device *dev_rw = (struct nvgpu_device *)dev;

	/*
	 * Bail out on pre-ga10b platforms.
	 */
	if (g->ops.runlist.get_engine_id_from_rleng_id == NULL) {
		return 0;
	}

	/*
	 * Init PBDMA info for this device; needs FIFO to be alive to do this.
	 * SW expects at least pbdma instance0 to be valid.
	 *
	 * See JIRA NVGPU-4980 for multiple pbdma support.
	 */
	g->ops.runlist.get_pbdma_info(g,
				      dev->rl_pri_base,
				      &dev_rw->pbdma_info);
	if (dev->pbdma_info.pbdma_id[ENGINE_PBDMA_INSTANCE0] ==
	    NVGPU_INVALID_PBDMA_ID) {
		nvgpu_err(g, "busted pbdma info: no pbdma for engine id:%d",
			  dev->engine_id);
		return -EINVAL;
	}

	dev_rw->pbdma_id = dev->pbdma_info.pbdma_id[ENGINE_PBDMA_INSTANCE0];

	nvgpu_log(g, gpu_dbg_device, "Parsed engine: ID: %u", dev->engine_id);
	nvgpu_log(g, gpu_dbg_device, "  inst_id %u,  runlist_id: %u,  fault id %u",
                  dev->inst_id, dev->runlist_id, dev->fault_id);
	nvgpu_log(g, gpu_dbg_device, "  intr_id %u,  reset_id %u",
                  dev->intr_id, dev->reset_id);
	nvgpu_log(g, gpu_dbg_device, "  engine_type %u",
                  dev->type);
	nvgpu_log(g, gpu_dbg_device, "  reset_id 0x%08x, rleng_id 0x%x",
                  dev->reset_id, dev->rleng_id);
	nvgpu_log(g, gpu_dbg_device, "  runlist_pri_base 0x%x",
		  dev->rl_pri_base);

	return 0;
}

static int nvgpu_engine_init_one_dev(struct nvgpu_fifo *f,
				     const struct nvgpu_device *dev)
{
	bool found;
	struct nvgpu_device *dev_rw;
	struct gk20a *g = f->g;

	dev_rw = (struct nvgpu_device *)dev;

	/*
	 * Populate the PBDMA info for this device; ideally it'd be done
	 * during device init, but the FIFO unit is not out of reset that
	 * early in the nvgpu_finalize_poweron() sequence.
	 *
	 * We only need to do this for native; vGPU already has pbdma_id
	 * populated during device initialization.
	 */
	if (g->ops.fifo.find_pbdma_for_runlist != NULL) {
		found = g->ops.fifo.find_pbdma_for_runlist(g,
							   dev->runlist_id,
							   &dev_rw->pbdma_id);
		if (!found) {
			nvgpu_err(g, "busted pbdma map");
			return -EINVAL;
		}
	}

	{
		/*
		 * Fill Ampere+ device fields.
		 */
		int err = nvgpu_engine_init_one_dev_extra(g, dev);
		if (err != 0) {
			return err;
		}
	}

	f->host_engines[dev->engine_id] = dev;
	f->active_engines[f->num_engines] = dev;
	++f->num_engines;

	return 0;
}

void nvgpu_engine_remove_one_dev(struct nvgpu_fifo *f,
		const struct nvgpu_device *dev)
{
	u32 i, j;
	struct gk20a *g = f->g;

	/*
	 * First remove the engine from fifo->host_engines list, for this, it
	 * suffices to set the entry corresponding to the dev->engine_id to
	 * NULL, this should prevent the entry from being used.
	 */
	f->host_engines[dev->engine_id] = NULL;
#if defined(CONFIG_NVGPU_NON_FUSA)
	/*
	 * Remove the device from the runlist device list.
	 */
	f->runlists[dev->runlist_id]->rl_dev_list[dev->rleng_id] = NULL;

	/*
	 * Remove the engine id from runlist->eng_bitmask
	 */
	f->runlists[dev->runlist_id]->eng_bitmask &= (~BIT32(dev->engine_id));
#endif
	/*
	 * For fifo->active_engines, we have to figure out the index of the
	 * device to be removed and shift the remaining elements up to that
	 * index.
	 */
	for (i = 0U; i < f->num_engines; i++) {
		if (f->active_engines[i] == dev) {
			nvgpu_log(g, gpu_dbg_device, "deleting device with"
				" engine_id(%d) from active_engines list",
				dev->engine_id);
			for (j = i; j < nvgpu_safe_sub_u32(f->num_engines, 1U);
					j++) {
				f->active_engines[j] = f->active_engines[
					nvgpu_safe_add_u32(j, 1U)];
			}
			break;
		}
	}
	/*
	 * Update f->num_engines if a device was removed from f->active_engines
	 * list.
	 */
	f->num_engines = (i < f->num_engines) ?
		nvgpu_safe_sub_u32(f->num_engines, 1U) : f->num_engines;
}

int nvgpu_engine_init_info(struct nvgpu_fifo *f)
{
	int err;
	struct gk20a *g = f->g;
	const struct nvgpu_device *dev;

	f->num_engines = 0;

	nvgpu_log(g, gpu_dbg_device, "Loading host engines from device list");
	nvgpu_log(g, gpu_dbg_device, "  GFX devices: %u",
		  nvgpu_device_count(g, NVGPU_DEVTYPE_GRAPHICS));

	nvgpu_device_for_each(g, dev, NVGPU_DEVTYPE_GRAPHICS) {
		err = nvgpu_engine_init_one_dev(f, dev);
		if (err != 0) {
			return err;
		}
	}

	return g->ops.engine.init_ce_info(f);
}

void nvgpu_engine_get_id_and_type(struct gk20a *g, u32 engine_id,
					  u32 *id, u32 *type)
{
	struct nvgpu_engine_status_info engine_status;

	g->ops.engine_status.read_engine_status_info(g, engine_id,
		&engine_status);

	/* use next_id if context load is failing */
	if (nvgpu_engine_status_is_ctxsw_load(
		&engine_status)) {
		nvgpu_engine_status_get_next_ctx_id_type(
			&engine_status, id, type);
	} else {
		nvgpu_engine_status_get_ctx_id_type(
			&engine_status, id, type);
	}
}

u32 nvgpu_engine_find_busy_doing_ctxsw(struct gk20a *g,
			u32 *id_ptr, bool *is_tsg_ptr)
{
	u32 i;
	u32 id = U32_MAX;
	bool is_tsg = false;
	u32 mailbox2;
	struct nvgpu_engine_status_info engine_status;
	const struct nvgpu_device *dev = NULL;

	for (i = 0U; i < g->fifo.num_engines; i++) {
		dev = g->fifo.active_engines[i];

		g->ops.engine_status.read_engine_status_info(g, dev->engine_id,
			&engine_status);

		/*
		 * we are interested in busy engines that
		 * are doing context switch
		 */
		if (!engine_status.is_busy ||
		    !nvgpu_engine_status_is_ctxsw(&engine_status)) {
			continue;
		}

		if (nvgpu_engine_status_is_ctxsw_load(&engine_status)) {
			id = engine_status.ctx_next_id;
			is_tsg = nvgpu_engine_status_is_next_ctx_type_tsg(
					&engine_status);
		} else if (nvgpu_engine_status_is_ctxsw_switch(&engine_status)) {
			mailbox2 = g->ops.gr.falcon.read_fecs_ctxsw_mailbox(g,
					NVGPU_GR_FALCON_FECS_CTXSW_MAILBOX2);
			if ((mailbox2 & FECS_METHOD_WFI_RESTORE) != 0U) {
				id = engine_status.ctx_next_id;
				is_tsg = nvgpu_engine_status_is_next_ctx_type_tsg(
						&engine_status);
			} else {
				id = engine_status.ctx_id;
				is_tsg = nvgpu_engine_status_is_ctx_type_tsg(
						&engine_status);
			}
		} else {
			id = engine_status.ctx_id;
			is_tsg = nvgpu_engine_status_is_ctx_type_tsg(
					&engine_status);
		}
		break;
	}

	*id_ptr = id;
	*is_tsg_ptr = is_tsg;

	return dev->engine_id;
}

u32 nvgpu_engine_get_runlist_busy_engines(struct gk20a *g, u32 runlist_id)
{
	struct nvgpu_fifo *f = &g->fifo;
	u32 i, eng_bitmask = 0U;
	struct nvgpu_engine_status_info engine_status;

	for (i = 0U; i < f->num_engines; i++) {
		const struct nvgpu_device *dev = f->active_engines[i];

		g->ops.engine_status.read_engine_status_info(g, dev->engine_id,
			&engine_status);

		if (engine_status.is_busy && (dev->runlist_id == runlist_id)) {
			eng_bitmask |= BIT32(dev->engine_id);
		}
	}

	return eng_bitmask;
}

#ifdef CONFIG_NVGPU_DEBUGGER
bool nvgpu_engine_should_defer_reset(struct gk20a *g, u32 engine_id,
		u32 engine_subid, bool fake_fault)
{
	const struct nvgpu_device *dev;

	dev = nvgpu_engine_get_active_eng_info(g, engine_id);
	if (dev == NULL) {
		return false;
	}

	/*
	 * channel recovery is only deferred if an sm debugger
	 * is attached and has MMU debug mode is enabled
	 */
	if (!g->ops.gr.sm_debugger_attached(g) ||
	    !g->ops.fb.is_debug_mode_enabled(g)) {
		return false;
	}

	/* if this fault is fake (due to RC recovery), don't defer recovery */
	if (fake_fault) {
		return false;
	}

	if (dev->type != NVGPU_DEVTYPE_GRAPHICS) {
		return false;
	}

	return g->ops.engine.is_fault_engine_subid_gpc(g, engine_subid);
}
#endif

u32 nvgpu_engine_mmu_fault_id_to_veid(struct gk20a *g, u32 mmu_fault_id,
			u32 gr_eng_fault_id)
{
	struct nvgpu_fifo *f = &g->fifo;
	u32 num_subctx;
	u32 veid = INVAL_ID;

	num_subctx = f->max_subctx_count;

	if ((mmu_fault_id >= gr_eng_fault_id) &&
		(mmu_fault_id < nvgpu_safe_add_u32(gr_eng_fault_id,
						num_subctx))) {
		veid = mmu_fault_id - gr_eng_fault_id;
	}

	return veid;
}

static u32 nvgpu_engine_mmu_fault_id_to_eng_id_and_veid(struct gk20a *g,
			 u32 mmu_fault_id, u32 *veid)
{
	u32 i;
	u32 engine_id = INVAL_ID;
	const struct nvgpu_device *dev;
	struct nvgpu_fifo *f = &g->fifo;

	for (i = 0U; i < f->num_engines; i++) {
		dev = f->active_engines[i];

		if (dev->type == NVGPU_DEVTYPE_GRAPHICS) {
			*veid = nvgpu_engine_mmu_fault_id_to_veid(g,
					mmu_fault_id, dev->fault_id);
			if (*veid != INVAL_ID) {
				engine_id = dev->engine_id;
				break;
			}
		} else {
			if (dev->fault_id == mmu_fault_id) {
				engine_id = dev->engine_id;
				*veid = INVAL_ID;
				break;
			}
		}
	}
	return engine_id;
}

void nvgpu_engine_mmu_fault_id_to_eng_ve_pbdma_id(struct gk20a *g,
	u32 mmu_fault_id, u32 *engine_id, u32 *veid, u32 *pbdma_id)
{
	*engine_id = nvgpu_engine_mmu_fault_id_to_eng_id_and_veid(g,
				 mmu_fault_id, veid);

	if (*engine_id == INVAL_ID) {
		*pbdma_id = g->ops.fifo.mmu_fault_id_to_pbdma_id(g,
				mmu_fault_id);
	} else {
		*pbdma_id = INVAL_ID;
	}
}
