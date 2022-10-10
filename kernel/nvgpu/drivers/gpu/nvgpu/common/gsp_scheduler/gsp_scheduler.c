/*
 * Copyright (c) 2022, NVIDIA CORPORATION.  All rights reserved.
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
#include <nvgpu/string.h>
#include <nvgpu/kmem.h>
#include <nvgpu/log.h>
#include <nvgpu/errno.h>
#include <nvgpu/gsp.h>
#include <nvgpu/gsp_sched.h>

#include "gsp_scheduler.h"
#include "ipc/gsp_seq.h"
#include "ipc/gsp_queue.h"
#include "gsp_runlist.h"

static void gsp_sched_get_file_names(struct gk20a *g, struct gsp_fw *gsp_ucode)
{
	nvgpu_log_fn(g, " ");

	gsp_ucode->code_name = GSP_DBG_RISCV_FW_CODE;
	gsp_ucode->data_name = GSP_DBG_RISCV_FW_DATA;
	gsp_ucode->manifest_name = GSP_DBG_RISCV_FW_MANIFEST;
}

void nvgpu_gsp_sched_suspend(struct gk20a *g, struct nvgpu_gsp_sched *gsp_sched)
{
	struct nvgpu_gsp *gsp = gsp_sched->gsp;

	nvgpu_log_fn(g, " ");

	if (gsp == NULL) {
		nvgpu_info(g, "GSP not initialized");
		return;
	}

	gsp_sched->gsp_ready = false;
	nvgpu_gsp_suspend(g, gsp);
}

static void gsp_sched_deinit(struct gk20a *g, struct nvgpu_gsp_sched *gsp_sched)
{
	gsp_sched->gsp_ready = false;

	nvgpu_kfree(g, gsp_sched);
	gsp_sched = NULL;
}

void nvgpu_gsp_sched_sw_deinit(struct gk20a *g)
{
	struct nvgpu_gsp_sched *gsp_sched = g->gsp_sched;

	nvgpu_log_fn(g, " ");

	if (gsp_sched == NULL) {
		return;
	}

	if (gsp_sched->gsp != NULL) {
		nvgpu_gsp_sw_deinit(g, gsp_sched->gsp);
	}

	if (gsp_sched->sequences != NULL) {
		nvgpu_gsp_sequences_free(g, gsp_sched->sequences);
	}

	nvgpu_gsp_queues_free(g, gsp_sched->queues);

	if (gsp_sched != NULL) {
		gsp_sched_deinit(g, gsp_sched);
	}
}

static int gsp_sched_wait_for_init(struct gk20a *g,
	struct nvgpu_gsp_sched *gsp_sched, signed int timeoutms)
{
	nvgpu_log_fn(g, " ");

	do {
		if (gsp_sched->gsp_ready) {
			break;
		}

		if (timeoutms <= 0) {
			nvgpu_err(g, "gsp wait for init timedout");
			return -1;
		}

		nvgpu_msleep(10);
		timeoutms -= 10;
	} while (true);

	return 0;
}

int nvgpu_gsp_sched_sw_init(struct gk20a *g)
{
	int err = 0;
	struct nvgpu_gsp_sched *gsp_sched;
	struct nvgpu_gsp *gsp;

	nvgpu_log_fn(g, " ");

	if (g->gsp_sched != NULL) {
		/*
		 * Recovery/unrailgate case, we do not need to do gsp_sched init as
		 * gsp_sched is set during cold boot & doesn't execute gsp_sched clean
		 * up as part of power off sequence, so reuse to perform faster boot.
		 */
		return err;
	}

	/* Init struct holding the gsp sched software state */
	g->gsp_sched = (struct nvgpu_gsp_sched *)
					nvgpu_kzalloc(g, sizeof(struct nvgpu_gsp_sched));
	if (g->gsp_sched == NULL) {
		err = -ENOMEM;
		goto de_init;
	}

	/* Init struct holding the gsp software state */
	g->gsp_sched->gsp = (struct nvgpu_gsp *)
					nvgpu_kzalloc(g, sizeof(struct nvgpu_gsp));
	if (g->gsp_sched->gsp == NULL) {
		err = -ENOMEM;
		goto de_init;
	}

	gsp_sched = g->gsp_sched;
	gsp = g->gsp_sched->gsp;

	/* gsp falcon software state */
	gsp->gsp_flcn = &g->gsp_flcn;
	gsp->g = g;

	/* Init isr mutex */
	nvgpu_mutex_init(&gsp->isr_mutex);

	err = nvgpu_gsp_sequences_init(g, gsp_sched);
	if (err != 0) {
		nvgpu_err(g, "GSP sequences init failed");
		goto de_init;
	}

	nvgpu_log_fn(g, " Done ");
	return err;
de_init:
	nvgpu_gsp_sched_sw_deinit(g);
	return err;
}

int nvgpu_gsp_sched_bootstrap_ns(struct gk20a *g)
{
	struct nvgpu_gsp_sched *gsp_sched = g->gsp_sched;
	int status = 0;

#ifdef CONFIG_NVGPU_FALCON_DEBUG
	status = nvgpu_gsp_debug_buf_init(g, GSP_SCHED_DEBUG_BUFFER_QUEUE,
			GSP_SCHED_DMESG_BUFFER_SIZE);
	if (status != 0) {
		nvgpu_err(g, "GSP sched debug buf init failed");
		goto de_init;
	}
#endif

	/* Get ucode file names */
	gsp_sched_get_file_names(g, &gsp_sched->gsp->gsp_ucode);

	status = nvgpu_gsp_bootstrap_ns(g, gsp_sched->gsp);
	if (status != 0) {
		nvgpu_err(g, " GSP sched bootstrap failed ");
		goto de_init;
	}

	status = nvgpu_gsp_wait_for_priv_lockdown_release(gsp_sched->gsp,
				GSP_WAIT_TIME_MS);
	if (status != 0) {
		nvgpu_err(g, "gsp PRIV lockdown release wait failed ");
		goto de_init;
	}

	status = gsp_sched_wait_for_init(g, gsp_sched, GSP_WAIT_TIME_MS);
	if (status != 0) {
		nvgpu_err(g, "gsp wait for basic init failed ");
		goto de_init;
	}

	status = nvgpu_gsp_send_devices_info(g);
	if (status != 0) {
		nvgpu_err(g, "gsp send device info failed");
	}

	return status;
de_init:
	nvgpu_gsp_sched_sw_deinit(g);
	return status;
}

void nvgpu_gsp_sched_isr(struct gk20a *g)
{
	struct nvgpu_gsp *gsp = g->gsp_sched->gsp;

	g->ops.gsp.gsp_isr(g, gsp);
}
