/*
 * Copyright (c) 2017-2019, NVIDIA CORPORATION.  All rights reserved.
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
#include <nvgpu/engine_fb_queue.h>
#include <nvgpu/engine_queue.h>
#include <nvgpu/pmu/cmd.h>
#include <nvgpu/pmu/queue.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/pmu/super_surface.h>

/* FB queue init */
static int pmu_fb_queue_init(struct gk20a *g, struct pmu_queues *queues,
		u32 id, union pmu_init_msg_pmu *init,
		struct nvgpu_mem *super_surface_buf)
{
	struct nvgpu_pmu *pmu = g->pmu;
	struct nvgpu_engine_fb_queue_params params = {0};
	u32 oflag = 0;
	int err = 0;
	u32 tmp_id = id;

	/* init queue parameters */
	if (PMU_IS_COMMAND_QUEUE(id)) {

		/* currently PMU FBQ support SW command queue only */
		if (!PMU_IS_SW_COMMAND_QUEUE(id)) {
			queues->queue[id] = NULL;
			err = 0;
			goto exit;
		}

		/*
		 * set OFLAG_WRITE for command queue
		 * i.e, push from nvgpu &
		 * pop form falcon ucode
		 */
		oflag = OFLAG_WRITE;

		params.super_surface_mem = super_surface_buf;
		params.fbq_offset =
			nvgpu_pmu_get_ss_cmd_fbq_offset(g, pmu,
				pmu->super_surface, id);
		params.size = NV_PMU_FBQ_CMD_NUM_ELEMENTS;
		params.fbq_element_size = NV_PMU_FBQ_CMD_ELEMENT_SIZE;
	} else if (PMU_IS_MESSAGE_QUEUE(id)) {
		/*
		 * set OFLAG_READ for message queue
		 * i.e, push from falcon ucode &
		 * pop form nvgpu
		 */
		oflag = OFLAG_READ;

		params.super_surface_mem = super_surface_buf;
		params.fbq_offset =
			nvgpu_pmu_get_ss_msg_fbq_offset(g, pmu,
				pmu->super_surface);
		params.size = NV_PMU_FBQ_MSG_NUM_ELEMENTS;
		params.fbq_element_size = NV_PMU_FBQ_MSG_ELEMENT_SIZE;
	} else {
		nvgpu_err(g, "invalid queue-id %d", id);
		err = -EINVAL;
		goto exit;
	}

	params.g = g;
	params.flcn_id = FALCON_ID_PMU;
	params.id = id;
	params.oflag = oflag;
	params.queue_head = g->ops.pmu.pmu_queue_head;
	params.queue_tail = g->ops.pmu.pmu_queue_tail;

	if (tmp_id == PMU_COMMAND_QUEUE_HPQ) {
		tmp_id = PMU_QUEUE_HPQ_IDX_FOR_V3;
	} else if (tmp_id == PMU_COMMAND_QUEUE_LPQ) {
		tmp_id = PMU_QUEUE_LPQ_IDX_FOR_V3;
	} else {
		tmp_id = PMU_QUEUE_MSG_IDX_FOR_V5;
	}

	params.index = init->v5.queue_phy_id[tmp_id];

	err = nvgpu_engine_fb_queue_init(&queues->fb_queue[id], params);
	if (err != 0) {
		nvgpu_err(g, "queue-%d init failed", id);
	}

exit:
	return err;
}

/* DMEM queue init */
static int pmu_dmem_queue_init(struct gk20a *g, struct pmu_queues *queues,
		u32 id, union pmu_init_msg_pmu *init)
{
	struct nvgpu_engine_mem_queue_params params = {0};
	u32 oflag = 0;
	int err = 0;

	if (PMU_IS_COMMAND_QUEUE(id)) {
		/*
		 * set OFLAG_WRITE for command queue
		 * i.e, push from nvgpu &
		 * pop form falcon ucode
		 */
		oflag = OFLAG_WRITE;
	} else if (PMU_IS_MESSAGE_QUEUE(id)) {
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
	params.g = g;
	params.flcn_id = FALCON_ID_PMU;
	params.id = id;
	params.oflag = oflag;
	params.queue_head = g->ops.pmu.pmu_queue_head;
	params.queue_tail = g->ops.pmu.pmu_queue_tail;
	params.queue_type = QUEUE_TYPE_DMEM;
	g->pmu->fw->ops.get_init_msg_queue_params(id, init,
							 &params.index,
							 &params.offset,
							 &params.size);
	err = nvgpu_engine_mem_queue_init(&queues->queue[id], params);
	if (err != 0) {
		nvgpu_err(g, "queue-%d init failed", id);
	}

exit:
	return err;
}

static void pmu_queue_free(struct gk20a *g, struct pmu_queues *queues, u32 id)
{
	if (!PMU_IS_COMMAND_QUEUE(id) && !PMU_IS_MESSAGE_QUEUE(id)) {
		nvgpu_err(g, "invalid queue-id %d", id);
		goto exit;
	}

	if (queues->queue_type == QUEUE_TYPE_FB) {
		if (queues->fb_queue[id] == NULL) {
			goto exit;
		}

		nvgpu_engine_fb_queue_free(&queues->fb_queue[id]);
	} else {
		if (queues->queue[id] == NULL) {
			goto exit;
		}

		nvgpu_engine_mem_queue_free(&queues->queue[id]);
	}

exit:
	return;
}

int nvgpu_pmu_queues_init(struct gk20a *g,
			  union pmu_init_msg_pmu *init,
			  struct pmu_queues *queues,
			  struct nvgpu_mem *super_surface_buf)
{
	u32 i = 0U;
	u32 j = 0U;
	int err;

	if (nvgpu_is_enabled(g, NVGPU_SUPPORT_PMU_RTOS_FBQ)) {
		queues->queue_type = QUEUE_TYPE_FB;
		for (i = 0; i < PMU_QUEUE_COUNT; i++) {
			err = pmu_fb_queue_init(g, queues, i, init,
						super_surface_buf);
			if (err != 0) {
				for (j = 0; j < i; j++) {
					pmu_queue_free(g, queues, j);
				}
				nvgpu_err(g, "PMU queue init failed");
				return err;
			}
		}
	} else {
		queues->queue_type = QUEUE_TYPE_DMEM;
		for (i = 0; i < PMU_QUEUE_COUNT; i++) {
			err = pmu_dmem_queue_init(g, queues, i, init);
			if (err != 0) {
				for (j = 0; j < i; j++) {
					pmu_queue_free(g, queues, j);
				}
				nvgpu_err(g, "PMU queue init failed");
				return err;
			}
		}
	}

	return 0;
}

void nvgpu_pmu_queues_free(struct gk20a *g, struct pmu_queues *queues)
{
	u32 i = 0U;

	for (i = 0U; i < PMU_QUEUE_COUNT; i++) {
		pmu_queue_free(g, queues, i);
	}
}

u32 nvgpu_pmu_queue_get_size(struct pmu_queues *queues, u32 queue_id)
{
	struct nvgpu_engine_fb_queue *fb_queue = NULL;
	struct nvgpu_engine_mem_queue *queue = NULL;
	u32 queue_size;

	if (queues->queue_type == QUEUE_TYPE_FB) {
		fb_queue = queues->fb_queue[queue_id];
		queue_size = nvgpu_engine_fb_queue_get_element_size(fb_queue);
	} else {
		queue = queues->queue[queue_id];
		queue_size = nvgpu_engine_mem_queue_get_size(queue);
	}

	return queue_size;
}

int nvgpu_pmu_queue_push(struct pmu_queues *queues, struct nvgpu_falcon *flcn,
			 u32 queue_id, struct pmu_cmd *cmd)
{
	struct nvgpu_engine_fb_queue *fb_queue = NULL;
	struct nvgpu_engine_mem_queue *queue = NULL;
	int err;

	if (queues->queue_type == QUEUE_TYPE_FB) {
		fb_queue = queues->fb_queue[queue_id];
		err = nvgpu_engine_fb_queue_push(fb_queue,
						 cmd, cmd->hdr.size);
	} else {
		queue = queues->queue[queue_id];
		err = nvgpu_engine_mem_queue_push(flcn, queue,
						  cmd, cmd->hdr.size);
	}

	return err;
}

int nvgpu_pmu_queue_pop(struct pmu_queues *queues, struct nvgpu_falcon *flcn,
			u32 queue_id, void *data, u32 bytes_to_read,
			u32 *bytes_read)
{
	struct nvgpu_engine_fb_queue *fb_queue = NULL;
	struct nvgpu_engine_mem_queue *queue = NULL;
	int err;

	if (queues->queue_type == QUEUE_TYPE_FB) {
		fb_queue = queues->fb_queue[queue_id];
		err = nvgpu_engine_fb_queue_pop(fb_queue, data,
				bytes_to_read, bytes_read);
	} else {
		queue = queues->queue[queue_id];
		err = nvgpu_engine_mem_queue_pop(flcn, queue, data,
				bytes_to_read, bytes_read);
	}

	return err;
}

bool nvgpu_pmu_queue_is_empty(struct pmu_queues *queues, u32 queue_id)
{
	struct nvgpu_engine_mem_queue *queue = NULL;
	struct nvgpu_engine_fb_queue *fb_queue = NULL;
	bool empty;

	if (queues->queue_type == QUEUE_TYPE_FB) {
		fb_queue = queues->fb_queue[queue_id];
		empty = nvgpu_engine_fb_queue_is_empty(fb_queue);
	} else {
		queue = queues->queue[queue_id];
		empty = nvgpu_engine_mem_queue_is_empty(queue);
	}

	return empty;
}

bool nvgpu_pmu_fb_queue_enabled(struct pmu_queues *queues)
{
	return queues->queue_type == QUEUE_TYPE_FB;
}

struct nvgpu_engine_fb_queue *nvgpu_pmu_fb_queue(struct pmu_queues *queues,
						 u32 queue_id)
{
	return queues->fb_queue[queue_id];
}

int nvgpu_pmu_queue_rewind(struct pmu_queues *queues, u32 queue_id,
			   struct nvgpu_falcon *flcn)
{
	struct nvgpu_engine_mem_queue *queue = queues->queue[queue_id];

	if (queues->queue_type == QUEUE_TYPE_FB) {
		return -EINVAL;
	}

	return nvgpu_engine_mem_queue_rewind(flcn, queue);
}
