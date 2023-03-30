/*
 * Copyright (c) 2018-2021, NVIDIA CORPORATION.  All rights reserved.
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

#include <nvgpu/sec2/allocator.h>
#include <nvgpu/engine_queue.h>
#include <nvgpu/sec2/queue.h>
#include <nvgpu/flcnif_cmn.h>
#include <nvgpu/sec2/sec2.h>
#include <nvgpu/sec2/msg.h>
#include <nvgpu/gk20a.h>

/* Message/Event request handlers */
static int sec2_response_handle(struct nvgpu_sec2 *sec2,
	struct nv_flcn_msg_sec2 *msg)
{
	struct gk20a *g = sec2->g;

	return nvgpu_sec2_seq_response_handle(g, &sec2->sequences,
					      msg, msg->hdr.seq_id);
}

static int sec2_handle_event(struct nvgpu_sec2 *sec2,
	struct nv_flcn_msg_sec2 *msg)
{
	int err = 0;

	switch (msg->hdr.unit_id) {
	default:
		break;
	}

	return err;
}

static bool sec2_read_message(struct nvgpu_sec2 *sec2,
	u32 queue_id, struct nv_flcn_msg_sec2 *msg, int *status)
{
	struct gk20a *g = sec2->g;
	u32 read_size;
	int err;

	*status = 0U;

	if (nvgpu_sec2_queue_is_empty(sec2->queues, queue_id)) {
		return false;
	}

	if (!nvgpu_sec2_queue_read(g, sec2->queues, queue_id,
				   &sec2->flcn,  &msg->hdr,
				   PMU_MSG_HDR_SIZE, status)) {
		nvgpu_err(g, "fail to read msg from queue %d", queue_id);
		goto clean_up;
	}

	if (msg->hdr.unit_id == NV_SEC2_UNIT_REWIND) {
		err = nvgpu_sec2_queue_rewind(&sec2->flcn,
					      sec2->queues, queue_id);
		if (err != 0) {
			nvgpu_err(g, "fail to rewind queue %d", queue_id);
			*status = err;
			goto clean_up;
		}

		/* read again after rewind */
		if (!nvgpu_sec2_queue_read(g, sec2->queues, queue_id,
					   &sec2->flcn, &msg->hdr,
					   PMU_MSG_HDR_SIZE, status)) {
			nvgpu_err(g, "fail to read msg from queue %d",
				queue_id);
			goto clean_up;
		}
	}

	if (!NV_SEC2_UNITID_IS_VALID(msg->hdr.unit_id)) {
		nvgpu_err(g, "read invalid unit_id %d from queue %d",
			msg->hdr.unit_id, queue_id);
			*status = -EINVAL;
			goto clean_up;
	}

	if (msg->hdr.size > PMU_MSG_HDR_SIZE) {
		read_size = msg->hdr.size - PMU_MSG_HDR_SIZE;
		if (!nvgpu_sec2_queue_read(g, sec2->queues, queue_id,
					   &sec2->flcn, &msg->msg,
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

static int sec2_process_init_msg(struct nvgpu_sec2 *sec2,
	struct nv_flcn_msg_sec2 *msg)
{
	struct gk20a *g = sec2->g;
	struct sec2_init_msg_sec2_init *sec2_init;
	u32 tail = 0;
	int err = 0;

	g->ops.sec2.msgq_tail(g, sec2, &tail, QUEUE_GET);

	err = nvgpu_falcon_copy_from_emem(&sec2->flcn, tail,
		(u8 *)&msg->hdr, PMU_MSG_HDR_SIZE, 0U);
	if (err != 0) {
		goto exit;
	}

	if (msg->hdr.unit_id != NV_SEC2_UNIT_INIT) {
		nvgpu_err(g, "expecting init msg");
		err = -EINVAL;
		goto exit;
	}

	err = nvgpu_falcon_copy_from_emem(&sec2->flcn, tail + PMU_MSG_HDR_SIZE,
		(u8 *)&msg->msg, msg->hdr.size - PMU_MSG_HDR_SIZE, 0U);
	if (err != 0) {
		goto exit;
	}

	if (msg->msg.init.msg_type != NV_SEC2_INIT_MSG_ID_SEC2_INIT) {
		nvgpu_err(g, "expecting init msg");
		err = -EINVAL;
		goto exit;
	}

	tail += NVGPU_ALIGN(U32(msg->hdr.size), PMU_DMEM_ALIGNMENT);
	g->ops.sec2.msgq_tail(g, sec2, &tail, QUEUE_SET);

	sec2_init = &msg->msg.init.sec2_init;

	err = nvgpu_sec2_queues_init(g, sec2->queues, sec2_init);
	if (err != 0) {
		return err;
	}

	err = nvgpu_sec2_dmem_allocator_init(g, &sec2->dmem, sec2_init);
	if (err != 0) {
		nvgpu_sec2_queues_free(g, sec2->queues);
		return err;
	}

	sec2->sec2_ready = true;

exit:
	return err;
}

int nvgpu_sec2_process_message(struct nvgpu_sec2 *sec2)
{
	struct gk20a *g = sec2->g;
	struct nv_flcn_msg_sec2 msg;
	int status = 0;

	if (unlikely(!sec2->sec2_ready)) {
		status = sec2_process_init_msg(sec2, &msg);
		goto exit;
	}

	while (sec2_read_message(sec2,
		SEC2_NV_MSGQ_LOG_ID, &msg, &status)) {

		nvgpu_sec2_dbg(g, "read msg hdr: ");
		nvgpu_sec2_dbg(g, "unit_id = 0x%08x, size = 0x%08x",
			msg.hdr.unit_id, msg.hdr.size);
		nvgpu_sec2_dbg(g, "ctrl_flags = 0x%08x, seq_id = 0x%08x",
			msg.hdr.ctrl_flags, msg.hdr.seq_id);

		msg.hdr.ctrl_flags &= ~PMU_CMD_FLAGS_PMU_MASK;

		if (msg.hdr.ctrl_flags == PMU_CMD_FLAGS_EVENT) {
			sec2_handle_event(sec2, &msg);
		} else {
			sec2_response_handle(sec2, &msg);
		}
	}

exit:
	return status;
}

static void sec2_isr(struct gk20a *g, struct nvgpu_sec2 *sec2)
{
	bool recheck = false;
	u32 intr;

	if (!g->ops.sec2.is_interrupted(sec2)) {
		return;
	}

	nvgpu_mutex_acquire(&sec2->isr_mutex);
	if (!sec2->isr_enabled) {
		goto exit;
	}

	intr = g->ops.sec2.get_intr(g);
	if (intr == 0U) {
		goto exit;
	}

	/*
	 * Handle swgen0 interrupt to process received messages from SEC2.
	 * If any other interrupt is to be handled with some software
	 * action expected, then it should be handled here.
	 * g->ops.sec2.isr call below will handle other hardware interrupts
	 * that are not expected to be handled in software.
	 */
	if (g->ops.sec2.msg_intr_received(g)) {
		if (nvgpu_sec2_process_message(sec2) != 0) {
			g->ops.sec2.clr_intr(g, intr);
			goto exit;
		}
		recheck = true;
	}

	g->ops.sec2.process_intr(g, sec2);
	g->ops.sec2.clr_intr(g, intr);

	if (recheck) {
		if (!nvgpu_sec2_queue_is_empty(sec2->queues,
					       SEC2_NV_MSGQ_LOG_ID)) {
			g->ops.sec2.set_msg_intr(g);
		}
	}

exit:
	nvgpu_mutex_release(&sec2->isr_mutex);
}

int nvgpu_sec2_wait_message_cond(struct nvgpu_sec2 *sec2, u32 timeout_ms,
	void *var, u8 val)
{
	struct gk20a *g = sec2->g;
	struct nvgpu_timeout timeout;
	u32 delay = POLL_DELAY_MIN_US;

	nvgpu_timeout_init_cpu_timer(g, &timeout, timeout_ms);

	do {
		if (*(u8 *)var == val) {
			return 0;
		}

		sec2_isr(g, sec2);

		nvgpu_usleep_range(delay, delay * 2U);
		delay = min_t(u32, delay << 1U, POLL_DELAY_MAX_US);
	} while (nvgpu_timeout_expired(&timeout) == 0);

	return -ETIMEDOUT;
}
