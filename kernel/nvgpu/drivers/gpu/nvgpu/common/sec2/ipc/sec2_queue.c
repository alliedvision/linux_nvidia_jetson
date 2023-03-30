/*
 * Copyright (c) 2018-2019, NVIDIA CORPORATION.  All rights reserved.
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

#include <nvgpu/engine_mem_queue.h>
#include <nvgpu/engine_queue.h>
#include <nvgpu/sec2/queue.h>
#include <nvgpu/sec2/msg.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/log.h>

/* sec2 falcon queue init */
static int sec2_queue_init(struct gk20a *g,
			   struct nvgpu_engine_mem_queue **queues, u32 id,
			   struct sec2_init_msg_sec2_init *init)
{
	struct nvgpu_engine_mem_queue_params params = {0};
	u32 queue_log_id = 0;
	u32 oflag = 0;
	int err = 0;

	if (id == SEC2_NV_CMDQ_LOG_ID) {
		/*
		 * set OFLAG_WRITE for command queue
		 * i.e, push from nvgpu &
		 * pop form falcon ucode
		 */
		oflag = OFLAG_WRITE;
	} else if (id == SEC2_NV_MSGQ_LOG_ID) {
		/*
		 * set OFLAG_READ for message queue
		 * i.e, push from falcon ucode &
		 * pop form nvgpu
		 */
		oflag = OFLAG_READ;
	} else {
		nvgpu_err(g, "invalid queue-id %d", id);
		err = -EINVAL;
		goto exit;
	}

	/* init queue parameters */
	queue_log_id = init->q_info[id].queue_log_id;

	params.g = g;
	params.flcn_id = FALCON_ID_SEC2;
	params.id = queue_log_id;
	params.index = init->q_info[id].queue_phy_id;
	params.offset = init->q_info[id].queue_offset;
	params.position = init->q_info[id].queue_offset;
	params.size = init->q_info[id].queue_size;
	params.oflag = oflag;
	params.queue_head = g->ops.sec2.sec2_queue_head;
	params.queue_tail = g->ops.sec2.sec2_queue_tail;
	params.queue_type = QUEUE_TYPE_EMEM;

	err = nvgpu_engine_mem_queue_init(&queues[queue_log_id],
				      params);
	if (err != 0) {
		nvgpu_err(g, "queue-%d init failed", queue_log_id);
	}

exit:
	return err;
}

static void sec2_queue_free(struct gk20a *g,
			    struct nvgpu_engine_mem_queue **queues, u32 id)
{
	if (!(id == SEC2_NV_CMDQ_LOG_ID) && !(id == SEC2_NV_MSGQ_LOG_ID)) {
		nvgpu_err(g, "invalid queue-id %d", id);
		goto exit;
	}

	if (queues[id] == NULL) {
		goto exit;
	}

	nvgpu_engine_mem_queue_free(&queues[id]);
exit:
	return;
}

int nvgpu_sec2_queues_init(struct gk20a *g,
			   struct nvgpu_engine_mem_queue **queues,
			   struct sec2_init_msg_sec2_init *init)
{
	u32 i, j;
	int err;

	for (i = 0; i < SEC2_QUEUE_NUM; i++) {
		err = sec2_queue_init(g, queues, i, init);
		if (err != 0) {
			for (j = 0; j < i; j++) {
				sec2_queue_free(g, queues, j);
			}
			nvgpu_err(g, "SEC2 queue init failed");
			return err;
		}
	}

	return 0;
}

void nvgpu_sec2_queues_free(struct gk20a *g,
			    struct nvgpu_engine_mem_queue **queues)
{
	u32 i;

	for (i = 0; i < SEC2_QUEUE_NUM; i++) {
		sec2_queue_free(g, queues, i);
	}
}

u32 nvgpu_sec2_queue_get_size(struct nvgpu_engine_mem_queue **queues,
			      u32 queue_id)
{
	return nvgpu_engine_mem_queue_get_size(queues[queue_id]);
}

int nvgpu_sec2_queue_push(struct nvgpu_engine_mem_queue **queues,
			  u32 queue_id, struct nvgpu_falcon *flcn,
			  struct nv_flcn_cmd_sec2 *cmd, u32 size)
{
	struct nvgpu_engine_mem_queue *queue;

	queue = queues[queue_id];
	return nvgpu_engine_mem_queue_push(flcn, queue, cmd, size);
}

bool nvgpu_sec2_queue_is_empty(struct nvgpu_engine_mem_queue **queues,
			       u32 queue_id)
{
	struct nvgpu_engine_mem_queue *queue = queues[queue_id];

	return nvgpu_engine_mem_queue_is_empty(queue);
}

bool nvgpu_sec2_queue_read(struct gk20a *g,
			   struct nvgpu_engine_mem_queue **queues,
			   u32 queue_id, struct nvgpu_falcon *flcn, void *data,
			   u32 bytes_to_read, int *status)
{
	struct nvgpu_engine_mem_queue *queue = queues[queue_id];
	u32 bytes_read;
	int err;

	err = nvgpu_engine_mem_queue_pop(flcn, queue, data,
			bytes_to_read, &bytes_read);
	if (err != 0) {
		nvgpu_err(g, "fail to read msg: err %d", err);
		*status = err;
		return false;
	}
	if (bytes_read != bytes_to_read) {
		nvgpu_err(g, "fail to read requested bytes: 0x%x != 0x%x",
			bytes_to_read, bytes_read);
		*status = -EINVAL;
		return false;
	}

	return true;
}

int nvgpu_sec2_queue_rewind(struct nvgpu_falcon *flcn,
			    struct nvgpu_engine_mem_queue **queues,
			    u32 queue_id)
{
	struct nvgpu_engine_mem_queue *queue = queues[queue_id];

	return nvgpu_engine_mem_queue_rewind(flcn, queue);
}
