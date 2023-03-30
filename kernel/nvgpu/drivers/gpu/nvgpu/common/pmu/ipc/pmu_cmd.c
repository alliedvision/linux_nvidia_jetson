/*
 * Copyright (c) 2017-2022, NVIDIA CORPORATION.  All rights reserved.
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

#include <nvgpu/enabled.h>
#include <nvgpu/pmu.h>
#include <nvgpu/log.h>
#include <nvgpu/timers.h>
#include <nvgpu/bug.h>
#include <nvgpu/pmu/pmuif/nvgpu_cmdif.h>
#include <nvgpu/falcon.h>
#include <nvgpu/engine_fb_queue.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/nvgpu_init.h>
#include <nvgpu/string.h>
#include <nvgpu/pmu/seq.h>
#include <nvgpu/pmu/queue.h>
#include <nvgpu/pmu/cmd.h>
#include <nvgpu/pmu/msg.h>
#include <nvgpu/pmu/fw.h>
#include <nvgpu/pmu/allocator.h>
#include <nvgpu/pmu/seq.h>

static bool pmu_validate_in_out_payload(struct nvgpu_pmu *pmu, struct pmu_cmd *cmd,
					struct pmu_in_out_payload_desc *payload)
{
	u32 size;

	if (payload->offset != 0U && payload->buf == NULL) {
		return false;
	}

	if (payload->buf == NULL) {
		return true;
	}

	if (payload->size == 0U) {
		return false;
	}

	size = PMU_CMD_HDR_SIZE;
	size += payload->offset;
	size += pmu->fw->ops.get_allocation_struct_size(pmu);

	if (size > cmd->hdr.size) {
		return false;
	}

	return true;
}

static bool pmu_validate_rpc_payload(struct pmu_payload *payload)
{
	if (payload->rpc.prpc == NULL) {
		return true;
	}

	if (payload->rpc.size_rpc == 0U) {
		goto invalid_cmd;
	}

	return true;

invalid_cmd:

	return false;
}

static bool pmu_validate_cmd(struct nvgpu_pmu *pmu, struct pmu_cmd *cmd,
			struct pmu_payload *payload, u32 queue_id)
{
	struct gk20a *g = pmu->g;
	u32 queue_size;

	if (cmd == NULL) {
		nvgpu_err(g, "PMU cmd buffer is NULL");
		return false;
	}

	if (!PMU_IS_SW_COMMAND_QUEUE(queue_id)) {
		goto invalid_cmd;
	}

	if (cmd->hdr.size < PMU_CMD_HDR_SIZE) {
		goto invalid_cmd;
	}

	queue_size = nvgpu_pmu_queue_get_size(&pmu->queues, queue_id);

	if (cmd->hdr.size > (queue_size >> 1)) {
		goto invalid_cmd;
	}

	if (!PMU_UNIT_ID_IS_VALID(cmd->hdr.unit_id)) {
		goto invalid_cmd;
	}

	if (payload == NULL) {
		return true;
	}

	if (payload->in.buf == NULL && payload->out.buf == NULL &&
		payload->rpc.prpc == NULL) {
		goto invalid_cmd;
	}

	if (!pmu_validate_in_out_payload(pmu, cmd, &payload->in)) {
		goto invalid_cmd;
	}

	if (!pmu_validate_in_out_payload(pmu, cmd, &payload->out)) {
		goto invalid_cmd;
	}

	if (!pmu_validate_rpc_payload(payload)) {
		goto invalid_cmd;
	}

	return true;

invalid_cmd:
	nvgpu_err(g, "invalid pmu cmd :\n"
		"queue_id=%d,\n"
		"cmd_size=%d, cmd_unit_id=%d,\n"
		"payload in=%p, in_size=%d, in_offset=%d,\n"
		"payload out=%p, out_size=%d, out_offset=%d",
		queue_id, cmd->hdr.size, cmd->hdr.unit_id,
		&payload->in, payload->in.size, payload->in.offset,
		&payload->out, payload->out.size, payload->out.offset);

	return false;
}

static int pmu_write_cmd(struct nvgpu_pmu *pmu, struct pmu_cmd *cmd,
			u32 queue_id)
{
	struct gk20a *g = pmu->g;
	struct nvgpu_timeout timeout;
	int err;

	nvgpu_log_fn(g, " ");

	nvgpu_timeout_init_cpu_timer(g, &timeout, U32_MAX);

	do {
		err = nvgpu_pmu_queue_push(&pmu->queues, pmu->flcn,
					   queue_id, cmd);
		if (nvgpu_timeout_expired(&timeout) == 0 && err == -EAGAIN) {
			nvgpu_usleep_range(1000, 2000);
		} else {
			break;
		}
	} while (true);

	if (err != 0) {
		nvgpu_err(g, "fail to write cmd to queue %d", queue_id);
	} else {
		nvgpu_log_fn(g, "done");
	}

	return err;
}

static void pmu_payload_deallocate(struct gk20a *g,
				   struct falcon_payload_alloc *alloc)
{
	struct nvgpu_pmu *pmu = g->pmu;

	if (alloc->dmem_offset != 0U) {
		nvgpu_free(&pmu->dmem, alloc->dmem_offset);
	}
}

static int pmu_payload_allocate(struct gk20a *g, struct pmu_sequence *seq,
	struct falcon_payload_alloc *alloc)
{
	struct nvgpu_pmu *pmu = g->pmu;
	u16 buffer_size;
	int err = 0;
	u64 tmp;

	if (nvgpu_pmu_fb_queue_enabled(&pmu->queues)) {
		buffer_size = nvgpu_pmu_seq_get_buffer_size(seq);
		nvgpu_pmu_seq_set_fbq_out_offset(seq, buffer_size);
		/* Save target address in FBQ work buffer. */
		alloc->dmem_offset = buffer_size;
		buffer_size = (u16)(buffer_size + alloc->dmem_size);
		nvgpu_pmu_seq_set_buffer_size(seq, buffer_size);
	} else {
		tmp = nvgpu_alloc(&pmu->dmem, alloc->dmem_size);
		nvgpu_assert(tmp <= U32_MAX);
		alloc->dmem_offset = (u32)tmp;
		if (alloc->dmem_offset == 0U) {
			err = -ENOMEM;
			goto clean_up;
		}
	}

clean_up:
	if (err != 0) {
		pmu_payload_deallocate(g, alloc);
	}

	return err;
}

static int pmu_cmd_payload_setup_rpc(struct gk20a *g, struct pmu_cmd *cmd,
	struct pmu_payload *payload, struct pmu_sequence *seq)
{
	struct nvgpu_pmu *pmu = g->pmu;
	struct pmu_fw_ver_ops *fw_ops = &g->pmu->fw->ops;
	struct nvgpu_engine_fb_queue *queue = nvgpu_pmu_seq_get_cmd_queue(seq);
	struct falcon_payload_alloc alloc;
	int err = 0;

	nvgpu_log_fn(g, " ");

	(void) memset(&alloc, 0, sizeof(struct falcon_payload_alloc));

	alloc.dmem_size = (u16)(payload->rpc.size_rpc +
		payload->rpc.size_scratch);

	err = pmu_payload_allocate(g, seq, &alloc);
	if (err != 0) {
		goto clean_up;
	}

	alloc.dmem_size = payload->rpc.size_rpc;

	if (nvgpu_pmu_fb_queue_enabled(&pmu->queues)) {
		/* copy payload to FBQ work buffer */
		nvgpu_memcpy((u8 *)
			nvgpu_engine_fb_queue_get_work_buffer(queue) +
			alloc.dmem_offset,
			(u8 *)payload->rpc.prpc, alloc.dmem_size);

		alloc.dmem_offset += nvgpu_pmu_seq_get_fbq_heap_offset(seq);

		nvgpu_pmu_seq_set_in_payload_fb_queue(seq, true);
		nvgpu_pmu_seq_set_out_payload_fb_queue(seq, true);
	} else {
		err = nvgpu_falcon_copy_to_dmem(pmu->flcn, alloc.dmem_offset,
				payload->rpc.prpc, payload->rpc.size_rpc, 0);
		if (err != 0) {
			pmu_payload_deallocate(g, &alloc);
			goto clean_up;
		}
	}

	cmd->cmd.rpc.rpc_dmem_size = payload->rpc.size_rpc;
	cmd->cmd.rpc.rpc_dmem_ptr  = alloc.dmem_offset;

	nvgpu_pmu_seq_set_out_payload(seq, payload->rpc.prpc);
	g->pmu->fw->ops.allocation_set_dmem_size(pmu,
		fw_ops->get_seq_out_alloc_ptr(seq),
		payload->rpc.size_rpc);
	g->pmu->fw->ops.allocation_set_dmem_offset(pmu,
		fw_ops->get_seq_out_alloc_ptr(seq),
		alloc.dmem_offset);

clean_up:
	if (err != 0) {
		nvgpu_log_fn(g, "fail");
	} else {
		nvgpu_log_fn(g, "done");
	}

	return err;
}

static int pmu_cmd_in_payload_setup(struct gk20a *g, struct pmu_cmd *cmd,
	struct pmu_payload *payload, struct pmu_sequence *seq)
{
	struct nvgpu_engine_fb_queue *fb_queue =
				nvgpu_pmu_seq_get_cmd_queue(seq);
	struct pmu_fw_ver_ops *fw_ops = &g->pmu->fw->ops;
	struct falcon_payload_alloc alloc;
	struct nvgpu_pmu *pmu = g->pmu;
	void *in = NULL;
	int err = 0;
	u32 offset;

	(void) memset(&alloc, 0, sizeof(struct falcon_payload_alloc));

	if (payload != NULL && payload->in.offset != 0U) {
		fw_ops->set_allocation_ptr(pmu, &in,
		((u8 *)&cmd->cmd + payload->in.offset));

		if (payload->in.buf != payload->out.buf) {
			fw_ops->allocation_set_dmem_size(pmu, in,
			(u16)payload->in.size);
		} else {
			fw_ops->allocation_set_dmem_size(pmu, in,
			(u16)max(payload->in.size, payload->out.size));
		}

		alloc.dmem_size = fw_ops->allocation_get_dmem_size(pmu, in);

		err = pmu_payload_allocate(g, seq, &alloc);
		if (err != 0) {
			return err;
		}

		*(fw_ops->allocation_get_dmem_offset_addr(pmu, in)) =
			alloc.dmem_offset;

		if (nvgpu_pmu_fb_queue_enabled(&pmu->queues)) {
			/* copy payload to FBQ work buffer */
			nvgpu_memcpy((u8 *)
				nvgpu_engine_fb_queue_get_work_buffer(
						fb_queue) +
				alloc.dmem_offset,
				(u8 *)payload->in.buf,
				payload->in.size);

			alloc.dmem_offset +=
				nvgpu_pmu_seq_get_fbq_heap_offset(seq);
			*(fw_ops->allocation_get_dmem_offset_addr(pmu,
				in)) = alloc.dmem_offset;

			nvgpu_pmu_seq_set_in_payload_fb_queue(seq, true);
		} else {
			offset =
				fw_ops->allocation_get_dmem_offset(pmu,
					in);
			err = nvgpu_falcon_copy_to_dmem(pmu->flcn,
					offset, payload->in.buf,
					payload->in.size, 0);
			if (err != 0) {
				pmu_payload_deallocate(g, &alloc);
				return err;
			}
		}

		fw_ops->allocation_set_dmem_size(pmu,
			fw_ops->get_seq_in_alloc_ptr(seq),
			fw_ops->allocation_get_dmem_size(pmu, in));
		fw_ops->allocation_set_dmem_offset(pmu,
			fw_ops->get_seq_in_alloc_ptr(seq),
			fw_ops->allocation_get_dmem_offset(pmu, in));
	}

	return 0;
}

static int pmu_cmd_out_payload_setup(struct gk20a *g, struct pmu_cmd *cmd,
	struct pmu_payload *payload, struct pmu_sequence *seq)
{
	struct pmu_fw_ver_ops *fw_ops = &g->pmu->fw->ops;
	struct falcon_payload_alloc alloc;
	struct nvgpu_pmu *pmu = g->pmu;
	void *in = NULL, *out = NULL;
	int err = 0;

	(void) memset(&alloc, 0, sizeof(struct falcon_payload_alloc));

	if (payload != NULL && payload->out.offset != 0U) {
		fw_ops->set_allocation_ptr(pmu, &out,
			((u8 *)&cmd->cmd + payload->out.offset));
		fw_ops->allocation_set_dmem_size(pmu, out,
			(u16)payload->out.size);

		if (payload->in.buf != payload->out.buf) {
			alloc.dmem_size =
				fw_ops->allocation_get_dmem_size(pmu, out);

			err = pmu_payload_allocate(g, seq, &alloc);
			if (err != 0) {
				return err;
			}

			*(fw_ops->allocation_get_dmem_offset_addr(pmu,
				out)) = alloc.dmem_offset;
		} else {
			WARN_ON(payload->in.offset == 0U);

			fw_ops->set_allocation_ptr(pmu, &in,
				((u8 *)&cmd->cmd + payload->in.offset));

			fw_ops->allocation_set_dmem_offset(pmu, out,
				fw_ops->allocation_get_dmem_offset(pmu,
					in));
		}

		if (nvgpu_pmu_fb_queue_enabled(&pmu->queues)) {
			if (payload->in.buf != payload->out.buf) {
				*(fw_ops->allocation_get_dmem_offset_addr(pmu,
					out)) +=
					nvgpu_pmu_seq_get_fbq_heap_offset(seq);
			}

			nvgpu_pmu_seq_set_out_payload_fb_queue(seq, true);
		}

		fw_ops->allocation_set_dmem_size(pmu,
			fw_ops->get_seq_out_alloc_ptr(seq),
			fw_ops->allocation_get_dmem_size(pmu, out));
		fw_ops->allocation_set_dmem_offset(pmu,
			fw_ops->get_seq_out_alloc_ptr(seq),
			fw_ops->allocation_get_dmem_offset(pmu, out));
	}

	return 0;
}

static int pmu_cmd_payload_setup(struct gk20a *g, struct pmu_cmd *cmd,
	struct pmu_payload *payload, struct pmu_sequence *seq)
{
	struct pmu_fw_ver_ops *fw_ops = &g->pmu->fw->ops;
	struct nvgpu_pmu *pmu = g->pmu;
	void *in = NULL;
	int err = 0;

	nvgpu_log_fn(g, " ");

	if (payload != NULL) {
		nvgpu_pmu_seq_set_out_payload(seq, payload->out.buf);
	}

	err = pmu_cmd_in_payload_setup(g, cmd, payload, seq);
	if (err != 0) {
		goto exit;
	}

	err = pmu_cmd_out_payload_setup(g, cmd, payload, seq);
	if (err != 0) {
		goto clean_up;
	}

	goto exit;

clean_up:
	if (payload->in.offset != 0U) {
		fw_ops->set_allocation_ptr(pmu, &in,
			((u8 *)&cmd->cmd + payload->in.offset));

		nvgpu_free(&pmu->dmem,
			fw_ops->allocation_get_dmem_offset(pmu,
				in));
	}

exit:
	if (err != 0) {
		nvgpu_log_fn(g, "fail");
	} else {
		nvgpu_log_fn(g, "done");
	}

	return err;
}

static int pmu_fbq_cmd_setup(struct gk20a *g, struct pmu_cmd *cmd,
	struct nvgpu_engine_fb_queue *queue, struct pmu_payload *payload,
	struct pmu_sequence *seq)
{
	struct nvgpu_pmu *pmu = g->pmu;
	struct nv_falcon_fbq_hdr *fbq_hdr = NULL;
	struct pmu_cmd *flcn_cmd = NULL;
	u32 fbq_size_needed = 0;
	u16 heap_offset = 0;
	u64 tmp;
	int err = 0;

	fbq_hdr = (struct nv_falcon_fbq_hdr *)
		nvgpu_engine_fb_queue_get_work_buffer(queue);

	flcn_cmd = (struct pmu_cmd *)
		(nvgpu_engine_fb_queue_get_work_buffer(queue) +
		sizeof(struct nv_falcon_fbq_hdr));

	if (cmd->cmd.rpc.cmd_type == NV_PMU_RPC_CMD_ID) {
		if (payload != NULL) {
			fbq_size_needed = (u32)payload->rpc.size_rpc +
					(u32)payload->rpc.size_scratch;
		}
	} else {
		err = -EINVAL;
		goto exit;
	}

	tmp = fbq_size_needed +
		sizeof(struct nv_falcon_fbq_hdr) +
		cmd->hdr.size;
	nvgpu_assert(tmp <= (size_t)U32_MAX);
	fbq_size_needed = (u32)tmp;

	fbq_size_needed = ALIGN_UP(fbq_size_needed, 4U);

	/* Check for allocator pointer and proceed */
	if (pmu->dmem.priv != NULL) {
		tmp = nvgpu_alloc(&pmu->dmem, fbq_size_needed);
	}
	nvgpu_assert(tmp <= U32_MAX);
	heap_offset = (u16) tmp;
	if (heap_offset == 0U) {
		err = -ENOMEM;
		goto exit;
	}

	/* clear work queue buffer */
	(void) memset(nvgpu_engine_fb_queue_get_work_buffer(queue), 0,
		nvgpu_engine_fb_queue_get_element_size(queue));

	/* Need to save room for both FBQ hdr, and the CMD */
	tmp = sizeof(struct nv_falcon_fbq_hdr) +
		   cmd->hdr.size;
	nvgpu_assert(tmp <= (size_t)U16_MAX);
	nvgpu_pmu_seq_set_buffer_size(seq, (u16)tmp);

	/* copy cmd into the work buffer */
	nvgpu_memcpy((u8 *)flcn_cmd, (u8 *)cmd, cmd->hdr.size);

	/* Fill in FBQ hdr, and offset in seq structure */
	nvgpu_assert(fbq_size_needed < U16_MAX);
	fbq_hdr->heap_size = (u16)fbq_size_needed;
	fbq_hdr->heap_offset = heap_offset;
	nvgpu_pmu_seq_set_fbq_heap_offset(seq, heap_offset);

	/*
	 * save queue index in seq structure
	 * so can free queue element when response is received
	 */
	nvgpu_pmu_seq_set_fbq_element_index(seq,
				nvgpu_engine_fb_queue_get_position(queue));

exit:
	return err;
}

int nvgpu_pmu_cmd_post(struct gk20a *g, struct pmu_cmd *cmd,
		struct pmu_payload *payload,
		u32 queue_id, pmu_callback callback, void *cb_param)
{
	struct nvgpu_pmu *pmu = g->pmu;
	struct pmu_sequence *seq = NULL;
	struct nvgpu_engine_fb_queue *fb_queue = NULL;
	int err;

	nvgpu_log_fn(g, " ");

	if (!nvgpu_pmu_get_fw_ready(g, pmu)) {
		nvgpu_warn(g, "PMU is not ready");
		return -EINVAL;
	}

	if (!pmu_validate_cmd(pmu, cmd, payload, queue_id)) {
		return -EINVAL;
	}

	err = nvgpu_pmu_seq_acquire(g, pmu->sequences, &seq, callback,
				    cb_param);
	if (err != 0) {
		return err;
	}

	cmd->hdr.seq_id = nvgpu_pmu_seq_get_id(seq);

	cmd->hdr.ctrl_flags = 0;
	cmd->hdr.ctrl_flags |= PMU_CMD_FLAGS_STATUS;
	cmd->hdr.ctrl_flags |= PMU_CMD_FLAGS_INTR;

	if (nvgpu_pmu_fb_queue_enabled(&pmu->queues)) {
		fb_queue = nvgpu_pmu_fb_queue(&pmu->queues, queue_id);
		/* Save the queue in the seq structure. */
		nvgpu_pmu_seq_set_cmd_queue(seq, fb_queue);

		/* Lock the FBQ work buffer */
		nvgpu_engine_fb_queue_lock_work_buffer(fb_queue);

		/* Create FBQ work buffer & copy cmd to FBQ work buffer */
		err = pmu_fbq_cmd_setup(g, cmd, fb_queue, payload, seq);
		if (err != 0) {
			nvgpu_err(g, "FBQ cmd setup failed");
			nvgpu_pmu_seq_release(g, pmu->sequences, seq);
			goto exit;
		}

		/*
		 * change cmd pointer to point to FBQ work
		 * buffer as cmd copied to FBQ work buffer
		 * in call pmu_fbq_cmd_setup()
		 */
		cmd = (struct pmu_cmd *)
			(nvgpu_engine_fb_queue_get_work_buffer(fb_queue) +
			sizeof(struct nv_falcon_fbq_hdr));
	}

	if (cmd->cmd.rpc.cmd_type == NV_PMU_RPC_CMD_ID) {
		err = pmu_cmd_payload_setup_rpc(g, cmd, payload, seq);
	} else {
		err = pmu_cmd_payload_setup(g, cmd, payload, seq);
	}

	if (err != 0) {
		nvgpu_err(g, "payload setup failed");
		pmu->fw->ops.allocation_set_dmem_size(pmu,
			pmu->fw->ops.get_seq_in_alloc_ptr(seq), 0);
		pmu->fw->ops.allocation_set_dmem_size(pmu,
			pmu->fw->ops.get_seq_out_alloc_ptr(seq), 0);

		nvgpu_pmu_seq_release(g, pmu->sequences, seq);
		goto exit;
	}

	nvgpu_pmu_seq_set_state(seq, PMU_SEQ_STATE_USED);

	err = pmu_write_cmd(pmu, cmd, queue_id);
	if (err != 0) {
		nvgpu_pmu_seq_set_state(seq, PMU_SEQ_STATE_PENDING);
	}

exit:
	if (nvgpu_pmu_fb_queue_enabled(&pmu->queues)) {
		/* Unlock the FBQ work buffer */
		nvgpu_engine_fb_queue_unlock_work_buffer(fb_queue);
	}

	nvgpu_log_fn(g, "Done, err %x", err);
	return err;
}

int nvgpu_pmu_rpc_execute(struct nvgpu_pmu *pmu, struct nv_pmu_rpc_header *rpc,
	u16 size_rpc, u16 size_scratch, pmu_callback caller_cb,
	void *caller_cb_param, bool is_copy_back)
{
	struct gk20a *g = pmu->g;
	struct pmu_cmd cmd;
	struct pmu_payload payload;
	struct rpc_handler_payload *rpc_payload = NULL;
	pmu_callback callback = NULL;
	void *rpc_buff = NULL;
	int status = 0;

	if (nvgpu_can_busy(g) == 0) {
		return 0;
	}

	if (!nvgpu_pmu_get_fw_ready(g, pmu)) {
		nvgpu_warn(g, "PMU is not ready to process RPC");
		status = EINVAL;
		goto exit;
	}

	if (caller_cb == NULL) {
		rpc_payload = nvgpu_kzalloc(g,
			sizeof(struct rpc_handler_payload) + size_rpc);
		if (rpc_payload == NULL) {
			status = ENOMEM;
			goto exit;
		}

		rpc_payload->rpc_buff = (u8 *)rpc_payload +
			sizeof(struct rpc_handler_payload);
		rpc_payload->is_mem_free_set =
			is_copy_back ? false : true;

		/* assign default RPC handler*/
		callback = nvgpu_pmu_rpc_handler;
	} else {
		if (caller_cb_param == NULL) {
			nvgpu_err(g, "Invalid cb param addr");
			status = EINVAL;
			goto exit;
		}
		rpc_payload = nvgpu_kzalloc(g,
			sizeof(struct rpc_handler_payload));
		if (rpc_payload == NULL) {
			status = ENOMEM;
			goto exit;
		}
		rpc_payload->rpc_buff = caller_cb_param;
		rpc_payload->is_mem_free_set = true;
		callback = caller_cb;
		WARN_ON(is_copy_back);
	}

	rpc_buff = rpc_payload->rpc_buff;
	(void) memset(&cmd, 0, sizeof(struct pmu_cmd));
	(void) memset(&payload, 0, sizeof(struct pmu_payload));

	cmd.hdr.unit_id = rpc->unit_id;
	cmd.hdr.size = (u8)(PMU_CMD_HDR_SIZE + sizeof(struct nv_pmu_rpc_cmd));
	cmd.cmd.rpc.cmd_type = NV_PMU_RPC_CMD_ID;
	cmd.cmd.rpc.flags = rpc->flags;

	nvgpu_memcpy((u8 *)rpc_buff, (u8 *)rpc, size_rpc);
	payload.rpc.prpc = rpc_buff;
	payload.rpc.size_rpc = size_rpc;
	payload.rpc.size_scratch = size_scratch;

	status = nvgpu_pmu_cmd_post(g, &cmd, &payload,
			PMU_COMMAND_QUEUE_LPQ, callback,
			rpc_payload);
	if (status != 0) {
		nvgpu_err(g, "Failed to execute RPC status=0x%x, func=0x%x",
				status, rpc->function);
		goto cleanup;
	}

	/*
	 * Option act like blocking call, which waits till RPC request
	 * executes on PMU & copy back processed data to rpc_buff
	 * to read data back in nvgpu
	 */
	if (is_copy_back) {
		/* wait till RPC execute in PMU & ACK */
		if (nvgpu_pmu_wait_fw_ack_status(g, pmu,
				nvgpu_get_poll_timeout(g),
				&rpc_payload->complete, 1U) != 0) {
			nvgpu_err(g, "PMU wait timeout expired.");
			status = -ETIMEDOUT;
			goto cleanup;
		}
		/* copy back data to caller */
		nvgpu_memcpy((u8 *)rpc, (u8 *)rpc_buff, size_rpc);
		/* free allocated memory */
		nvgpu_kfree(g, rpc_payload);
	}

	return 0;

cleanup:
	nvgpu_kfree(g, rpc_payload);
exit:
	return status;
}
