/*
 * Copyright (c) 2021-2022, NVIDIA CORPORATION.  All rights reserved.
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
#include <nvgpu/pmu.h>
#include <nvgpu/log.h>
#include <nvgpu/gsp.h>

#include "../gsp_scheduler.h"
#include "gsp_seq.h"
#include "gsp_queue.h"
#include "gsp_cmd.h"

u8 gsp_unit_id_is_valid(u8 id)
{
	return (id < NV_GSP_UNIT_END);
}

static bool gsp_validate_cmd(struct nvgpu_gsp_sched *gsp_sched,
	struct nv_flcn_cmd_gsp *cmd, u32 queue_id)
{
	struct gk20a *g = gsp_sched->gsp->g;
	u32 queue_size;

	if (queue_id != GSP_NV_CMDQ_LOG_ID) {
		goto invalid_cmd;
	}

	if (cmd->hdr.size < PMU_CMD_HDR_SIZE) {
		goto invalid_cmd;
	}

	queue_size = nvgpu_gsp_queue_get_size(gsp_sched->queues, queue_id);

	if (cmd->hdr.size > (queue_size >> 1)) {
		goto invalid_cmd;
	}

	if (!gsp_unit_id_is_valid(cmd->hdr.unit_id)) {
		goto invalid_cmd;
	}

	return true;

invalid_cmd:
	nvgpu_err(g, "invalid gsp cmd :");
	nvgpu_err(g, "queue_id=%d, cmd_size=%d, cmd_unit_id=%d\n",
		queue_id, cmd->hdr.size, cmd->hdr.unit_id);

	return false;
}

static int gsp_write_cmd(struct nvgpu_gsp_sched *gsp_sched,
	struct nv_flcn_cmd_gsp *cmd, u32 queue_id,
	u32 timeout_ms)
{
	struct nvgpu_timeout timeout;
	struct gk20a *g = gsp_sched->gsp->g;
	struct nvgpu_gsp *gsp = gsp_sched->gsp;
	int err;

	nvgpu_log_fn(g, " ");

	nvgpu_timeout_init_cpu_timer(g, &timeout, timeout_ms);

	do {
		err = nvgpu_gsp_queue_push(gsp_sched->queues, queue_id, gsp->gsp_flcn,
					    cmd, cmd->hdr.size);
		if ((err == -EAGAIN) &&
		    (nvgpu_timeout_expired(&timeout) == 0)) {
			nvgpu_usleep_range(1000U, 2000U);
		} else {
			break;
		}
	} while (true);

	if (err != 0) {
		nvgpu_err(g, "fail to write cmd to queue %d", queue_id);
	}

	return err;
}

int nvgpu_gsp_cmd_post(struct gk20a *g, struct nv_flcn_cmd_gsp *cmd,
	u32 queue_id, gsp_callback callback,
	void *cb_param, u32 timeout)
{
	struct nvgpu_gsp_sched *gsp_sched = g->gsp_sched;
	struct gsp_sequence *seq = NULL;
	int err = 0;

	if (cmd == NULL) {
		nvgpu_err(g, "gsp cmd buffer is empty");
		err = -EINVAL;
		goto exit;
	}

	/* Sanity check the command input. */
	if (!gsp_validate_cmd(gsp_sched, cmd, queue_id)) {
		err = -EINVAL;
		goto exit;
	}

	/* Attempt to reserve a sequence for this command. */
	err = nvgpu_gsp_seq_acquire(g, gsp_sched->sequences, &seq,
			callback, cb_param);
	if (err != 0) {
		goto exit;
	}

	/* Set the sequence number in the command header. */
	cmd->hdr.seq_id = nvgpu_gsp_seq_get_id(seq);

	cmd->hdr.ctrl_flags = 0U;
	cmd->hdr.ctrl_flags = PMU_CMD_FLAGS_STATUS;

	nvgpu_gsp_seq_set_state(seq, GSP_SEQ_STATE_USED);

	err = gsp_write_cmd(gsp_sched, cmd, queue_id, timeout);
	if (err != 0) {
		gsp_seq_release(gsp_sched->sequences, seq);
	}

exit:
	return err;
}

u32 nvgpu_gsp_get_last_cmd_id(struct gk20a *g)
{
	(void)g;
	return GSP_NV_CMDQ_LOG_ID__LAST;
}
