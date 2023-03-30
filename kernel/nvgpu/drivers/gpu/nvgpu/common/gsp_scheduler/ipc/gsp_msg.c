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

#include <nvgpu/engine_queue.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/gsp.h>

#include "../gsp_scheduler.h"
#include "gsp_queue.h"
#include "gsp_msg.h"
#include "gsp_seq.h"

/* Message/Event request handlers */
static int gsp_response_handle(struct nvgpu_gsp_sched *gsp_sched,
	struct nv_flcn_msg_gsp *msg)
{
	struct gk20a *g = gsp_sched->gsp->g;

	return nvgpu_gsp_seq_response_handle(g, gsp_sched->sequences,
				msg, msg->hdr.seq_id);
}

static int gsp_handle_event(struct nvgpu_gsp_sched *gsp_sched,
	struct nv_flcn_msg_gsp *msg)
{
	int err = 0;

	switch (msg->hdr.unit_id) {
	default:
		break;
	}

	(void)gsp_sched;
	return err;
}

static bool gsp_read_message(struct nvgpu_gsp_sched *gsp_sched,
	u32 queue_id, struct nv_flcn_msg_gsp *msg, int *status)
{
	struct gk20a *g = gsp_sched->gsp->g;
	u32 read_size;
	int err;

	*status = 0U;

	if (nvgpu_gsp_queue_is_empty(gsp_sched->queues, queue_id)) {
		return false;
	}

	if (!nvgpu_gsp_queue_read(g, gsp_sched->queues, queue_id,
			gsp_sched->gsp->gsp_flcn,  &msg->hdr,
				GSP_MSG_HDR_SIZE, status)) {
		nvgpu_err(g, "fail to read msg from queue %d", queue_id);
		goto clean_up;
	}

	if (msg->hdr.unit_id == NV_GSP_UNIT_REWIND) {
		err = nvgpu_gsp_queue_rewind(gsp_sched->gsp->gsp_flcn,
				gsp_sched->queues, queue_id);
		if (err != 0) {
			nvgpu_err(g, "fail to rewind queue %d", queue_id);
			*status = err;
			goto clean_up;
		}

		/* read again after rewind */
		if (!nvgpu_gsp_queue_read(g, gsp_sched->queues, queue_id,
				gsp_sched->gsp->gsp_flcn, &msg->hdr,
				GSP_MSG_HDR_SIZE, status)) {
			nvgpu_err(g, "fail to read msg from queue %d",
				queue_id);
			goto clean_up;
		}
	}

	if (!gsp_unit_id_is_valid(msg->hdr.unit_id)) {
		nvgpu_err(g, "read invalid unit_id %d from queue %d",
			msg->hdr.unit_id, queue_id);
			*status = -EINVAL;
			goto clean_up;
	}

	if (msg->hdr.size > GSP_MSG_HDR_SIZE) {
		read_size = msg->hdr.size - GSP_MSG_HDR_SIZE;
		if (!nvgpu_gsp_queue_read(g, gsp_sched->queues, queue_id,
				gsp_sched->gsp->gsp_flcn, &msg->msg,
				read_size, status)) {
			nvgpu_err(g, "fail to read msg from queue %d",
				queue_id);
			goto clean_up;
		}
	}

	return true;

clean_up:
	return false;
}

static int gsp_process_init_msg(struct nvgpu_gsp_sched *gsp_sched,
	struct nv_flcn_msg_gsp *msg)
{
	struct gk20a *g = gsp_sched->gsp->g;
	struct nvgpu_gsp *gsp = gsp_sched->gsp;
	struct gsp_init_msg_gsp_init *gsp_init;
	u32 tail = 0;
	int err = 0;

	g->ops.gsp.msgq_tail(g, gsp, &tail, QUEUE_GET);

	err = nvgpu_falcon_copy_from_emem(gsp_sched->gsp->gsp_flcn, tail,
		(u8 *)&msg->hdr, GSP_MSG_HDR_SIZE, 0U);
	if (err != 0) {
		goto exit;
	}

	if (msg->hdr.unit_id != NV_GSP_UNIT_INIT) {
		nvgpu_err(g, "expecting init msg");
		err = -EINVAL;
		goto exit;
	}

	err = nvgpu_falcon_copy_from_emem(gsp_sched->gsp->gsp_flcn, tail + GSP_MSG_HDR_SIZE,
		(u8 *)&msg->msg, msg->hdr.size - GSP_MSG_HDR_SIZE, 0U);
	if (err != 0) {
		goto exit;
	}

	if (msg->msg.init.msg_type != NV_GSP_INIT_MSG_ID_GSP_INIT) {
		nvgpu_err(g, "expecting init msg");
		err = -EINVAL;
		goto exit;
	}

	tail += NVGPU_ALIGN(U32(msg->hdr.size), GSP_DMEM_ALIGNMENT);
	g->ops.gsp.msgq_tail(g, gsp, &tail, QUEUE_SET);

	gsp_init = &msg->msg.init.gsp_init;

	err = nvgpu_gsp_queues_init(g, gsp_sched->queues, gsp_init);
	if (err != 0) {
		return err;
	}

	gsp_sched->gsp_ready = true;

exit:
	return err;
}

int nvgpu_gsp_process_message(struct gk20a *g)
{
	struct nvgpu_gsp_sched *gsp_sched = g->gsp_sched;
	struct nv_flcn_msg_gsp msg;
	bool read_msg;
	int status = 0;

	nvgpu_log_fn(g, " ");

	if (unlikely(!gsp_sched->gsp_ready)) {
		status = gsp_process_init_msg(gsp_sched, &msg);
		goto exit;
	}

	do {
		read_msg = gsp_read_message(gsp_sched,
				GSP_NV_MSGQ_LOG_ID, &msg, &status);
		if (read_msg == false) {
			break;
		}

		nvgpu_info(g, "read msg hdr: ");
		nvgpu_info(g, "unit_id = 0x%08x, size = 0x%08x",
			msg.hdr.unit_id, msg.hdr.size);
		nvgpu_info(g, "ctrl_flags = 0x%08x, seq_id = 0x%08x",
			msg.hdr.ctrl_flags, msg.hdr.seq_id);

		msg.hdr.ctrl_flags &= (u8)~GSP_CMD_FLAGS_MASK;

		if (msg.hdr.ctrl_flags == GSP_CMD_FLAGS_EVENT) {
			gsp_handle_event(gsp_sched, &msg);
		} else {
			gsp_response_handle(gsp_sched, &msg);
		}

		if (!nvgpu_gsp_queue_is_empty(gsp_sched->queues,
				GSP_NV_MSGQ_LOG_ID)) {
			g->ops.gsp.set_msg_intr(g);
		}
	} while (read_msg);

exit:
	return status;
}

int nvgpu_gsp_wait_message_cond(struct gk20a *g, u32 timeout_ms,
		void *var, u8 val)
{
	struct nvgpu_timeout timeout;
	u32 delay = POLL_DELAY_MIN_US;

	nvgpu_timeout_init_cpu_timer(g, &timeout, timeout_ms);

	do {
		if (*(u8 *)var == val) {
			return 0;
		}

		nvgpu_usleep_range(delay, delay * 2U);
		delay = min_t(u32, delay << 1U, POLL_DELAY_MAX_US);
	} while (nvgpu_timeout_expired(&timeout) == 0);

	return -ETIMEDOUT;
}
