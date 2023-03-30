/*
 * FIFO
 *
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

#include <nvgpu/trace.h>
#include <nvgpu/dma.h>
#include <nvgpu/fifo.h>
#include <nvgpu/engines.h>
#include <nvgpu/runlist.h>
#include <nvgpu/preempt.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/channel.h>
#include <nvgpu/pbdma.h>
#include <nvgpu/tsg.h>
#include <nvgpu/vm_area.h>
#include <nvgpu/nvgpu_err.h>
#include <nvgpu/cic_mon.h>
#include <nvgpu/mc.h>
#include <nvgpu/swprofile.h>
#include <nvgpu/fifo/swprofile.h>

static const char *nvgpu_fifo_kickoff_profile_events[] = {
	NVGPU_FIFO_KICKOFF_PROFILE_EVENTS,
};

static const char *nvgpu_fifo_recovery_profile_events[] = {
	NVGPU_FIFO_RECOVERY_PROFILE_EVENTS,
};

static const char *nvgpu_fifo_engine_reset_events[] = {
	NVGPU_FIFO_ENGINE_RESET_EVENTS,
};

void nvgpu_fifo_cleanup_sw_common(struct gk20a *g)
{
	struct nvgpu_fifo *f = &g->fifo;

	nvgpu_log_fn(g, " ");

#ifdef CONFIG_NVGPU_USERD
	g->ops.userd.cleanup_sw(g);
#endif
	nvgpu_channel_cleanup_sw(g);
	nvgpu_tsg_cleanup_sw(g);
	nvgpu_runlist_cleanup_sw(g);
	nvgpu_engine_cleanup_sw(g);
	if (g->ops.pbdma.cleanup_sw != NULL) {
		g->ops.pbdma.cleanup_sw(g);
	}

#ifdef CONFIG_NVGPU_DEBUGGER
	f->deferred_reset_pending = false;
	nvgpu_mutex_destroy(&f->deferred_reset_mutex);
#endif
	nvgpu_mutex_destroy(&f->engines_reset_mutex);
	nvgpu_mutex_destroy(&f->intr.isr.mutex);

	f->sw_ready = false;
}

void nvgpu_fifo_cleanup_sw(struct gk20a *g)
{
#ifdef CONFIG_NVGPU_KERNEL_MODE_SUBMIT
	nvgpu_channel_worker_deinit(g);
#endif
	nvgpu_fifo_cleanup_sw_common(g);
}

static void nvgpu_fifo_remove_support(struct nvgpu_fifo *f)
{
	struct gk20a *g = f->g;

	g->ops.fifo.cleanup_sw(g);
}

int nvgpu_fifo_setup_sw_common(struct gk20a *g)
{
	struct nvgpu_fifo *f = &g->fifo;
	int err = 0;

	nvgpu_log_fn(g, " ");

	f->g = g;

	nvgpu_mutex_init(&f->intr.isr.mutex);
	nvgpu_mutex_init(&f->engines_reset_mutex);
#ifdef CONFIG_NVGPU_DEBUGGER
	nvgpu_mutex_init(&f->deferred_reset_mutex);
#endif

	nvgpu_swprofile_initialize(g, &f->kickoff_profiler,
				 nvgpu_fifo_kickoff_profile_events);
	nvgpu_swprofile_initialize(g, &f->recovery_profiler,
				 nvgpu_fifo_recovery_profile_events);
	nvgpu_swprofile_initialize(g, &f->eng_reset_profiler,
				 nvgpu_fifo_engine_reset_events);


	err = nvgpu_channel_setup_sw(g);
	if (err != 0) {
		nvgpu_err(g, "failed to init channel support");
		goto clean_up;
	}

	err = nvgpu_tsg_setup_sw(g);
	if (err != 0) {
		nvgpu_err(g, "failed to init tsg support");
		goto clean_up_channel;
	}

	if (g->ops.pbdma.setup_sw != NULL) {
		err = g->ops.pbdma.setup_sw(g);
		if (err != 0) {
			nvgpu_err(g, "failed to init pbdma support");
			goto clean_up_tsg;
		}
	}

	err = nvgpu_engine_setup_sw(g);
	if (err != 0) {
		nvgpu_err(g, "failed to init engine support");
		goto clean_up_pbdma;
	}

	err = nvgpu_runlist_setup_sw(g);
	if (err != 0) {
		nvgpu_err(g, "failed to init runlist support");
		goto clean_up_engine;
	}

#ifdef CONFIG_NVGPU_USERD
	err = g->ops.userd.setup_sw(g);
	if (err != 0) {
		nvgpu_err(g, "failed to init userd support");
		goto clean_up_runlist;
	}
#endif

	f->remove_support = nvgpu_fifo_remove_support;

	nvgpu_log_fn(g, "done");
	return 0;

#ifdef CONFIG_NVGPU_USERD
clean_up_runlist:
	nvgpu_runlist_cleanup_sw(g);
#endif

clean_up_engine:
	nvgpu_engine_cleanup_sw(g);

clean_up_pbdma:
	if (g->ops.pbdma.cleanup_sw != NULL) {
		g->ops.pbdma.cleanup_sw(g);
	}

clean_up_tsg:
	nvgpu_tsg_cleanup_sw(g);

clean_up_channel:
	nvgpu_channel_cleanup_sw(g);

clean_up:
	nvgpu_err(g, "init fifo support failed");
	return err;
}

int nvgpu_fifo_setup_sw(struct gk20a *g)
{
	struct nvgpu_fifo *f = &g->fifo;
	int err = 0;

	nvgpu_log_fn(g, " ");

	if (f->sw_ready) {
		nvgpu_log_fn(g, "skip init");
		return 0;
	}

	err = nvgpu_fifo_setup_sw_common(g);
	if (err != 0) {
		nvgpu_err(g, "fifo common sw setup failed, err=%d", err);
		return err;
	}

#ifdef CONFIG_NVGPU_KERNEL_MODE_SUBMIT
	err = nvgpu_channel_worker_init(g);
	if (err != 0) {
		nvgpu_err(g, "worker init fail, err=%d", err);
		goto clean_up;
	}
#endif

	f->sw_ready = true;

	nvgpu_log_fn(g, "done");
	return 0;

#ifdef CONFIG_NVGPU_KERNEL_MODE_SUBMIT
clean_up:
	nvgpu_fifo_cleanup_sw_common(g);

	return err;
#endif
}

int nvgpu_fifo_init_support(struct gk20a *g)
{
	int err;

	err = g->ops.fifo.setup_sw(g);
	if (err != 0) {
		nvgpu_err(g, "fifo sw setup failed, err=%d", err);
		return err;
	}

	if (g->ops.fifo.init_fifo_setup_hw != NULL) {
		err = g->ops.fifo.init_fifo_setup_hw(g);
		if (err != 0) {
			nvgpu_err(g, "fifo hw setup failed, err=%d", err);
			goto clean_up;
		}
	}

	return 0;

clean_up:
	nvgpu_fifo_cleanup_sw_common(g);

	return err;
}

static const char * const pbdma_ch_eng_status_str[] = {
	"invalid",
	"valid",
	"NA",
	"NA",
	"NA",
	"load",
	"save",
	"switch",
};

static const char * const not_found_str[] = {
	"NOT FOUND"
};

const char *nvgpu_fifo_decode_pbdma_ch_eng_status(u32 index)
{
	if (index >= ARRAY_SIZE(pbdma_ch_eng_status_str)) {
		return not_found_str[0];
	} else {
		return pbdma_ch_eng_status_str[index];
	}
}

static void disable_fifo_interrupts(struct gk20a *g)
{
	/** Disable fifo intr */
	g->ops.fifo.intr_0_enable(g, false);
	g->ops.fifo.intr_1_enable(g, false);

	if (g->ops.fifo.intr_top_enable == NULL) {
		nvgpu_cic_mon_intr_stall_unit_config(g, NVGPU_CIC_INTR_UNIT_FIFO,
					NVGPU_CIC_INTR_DISABLE);
#ifdef CONFIG_NVGPU_NONSTALL_INTR
		nvgpu_cic_mon_intr_nonstall_unit_config(g, NVGPU_CIC_INTR_UNIT_FIFO,
					   NVGPU_CIC_INTR_DISABLE);
#endif
	} else {
		g->ops.fifo.intr_top_enable(g, NVGPU_CIC_INTR_DISABLE);
	}
}

int nvgpu_fifo_suspend(struct gk20a *g)
{
	nvgpu_log_fn(g, " ");

	if (g->ops.mm.is_bar1_supported(g)) {
		g->ops.fifo.bar1_snooping_disable(g);
	}

	disable_fifo_interrupts(g);

	nvgpu_log_fn(g, "done");
	return 0;
}

void nvgpu_fifo_sw_quiesce(struct gk20a *g)
{
	u32 runlist_mask = U32_MAX;

	g->ops.runlist.write_state(g, runlist_mask, RUNLIST_DISABLED);

	/* Preempt all runlists */
	nvgpu_fifo_preempt_runlists_for_rc(g, runlist_mask);
}
