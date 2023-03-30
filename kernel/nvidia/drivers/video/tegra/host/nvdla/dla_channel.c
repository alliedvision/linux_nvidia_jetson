// SPDX-License-Identifier: GPL-2.0-only
/*
 * NVDLA channel submission
 *
 * Copyright (c) 2022, NVIDIA Corporation.  All rights reserved.
 *
 */

#include <linux/platform_device.h>

#include "dla_channel.h"
#include "nvdla_debug.h"
#include "nvhost_vm.h"
#include "nvhost_channel.h"
#include "t194/hardware_t194.h"

struct platform_device *nvdla_channel_map(struct platform_device *pdev,
					  struct nvdla_queue *queue)
{
	struct nvhost_device_data *pdata = platform_get_drvdata(pdev);
	int err;

	err = nvhost_channel_map(pdata, &queue->channel, queue);
	if (err < 0)
		return NULL;

	return queue->channel->vm->pdev;
}

void nvdla_putchannel(struct nvdla_queue *queue)
{
	nvhost_putchannel(queue->channel, 1);
}

int nvdla_send_cmd_channel(struct platform_device *pdev,
			   struct nvdla_queue *queue,
			   struct nvdla_cmd_data *cmd_data,
			   struct nvdla_task *task)
{
	unsigned long timeout;
	struct nvhost_device_data *pdata = platform_get_drvdata(pdev);
	struct nvdla_device *nvdla_dev = pdata->private_data;
	uint32_t method_id = cmd_data->method_id;
	uint32_t method_data = cmd_data->method_data;
	bool wait = cmd_data->wait;
	u32 syncpt_wait_ids[MAX_NVDLA_PREFENCES_PER_TASK];
	u32 syncpt_wait_thresh[MAX_NVDLA_PREFENCES_PER_TASK];
	u32 cmdbuf[3];
	int err = 0, i;

	nvdla_dbg_info(pdev, "");
	/*
	 * enable notification for command completion or error if
	 * wait if required
	 */
	if (wait)
		method_id |= (1 << DLA_INT_ON_COMPLETE_SHIFT) |
				(1 << DLA_INT_ON_ERROR_SHIFT);

	nvdla_dev->waiting = 1;

	/* Pick up fences... */
	for (i = 0; i < task->num_prefences; i++) {
		/* ..and ensure that we have only syncpoints present */
		if (task->prefences[i].type != NVDEV_FENCE_TYPE_SYNCPT) {
			nvdla_dbg_err(pdev, "syncpt only supported");
			return -EINVAL;
		}

		nvdla_dbg_info(pdev, "presyncpt[%d] value[%d]\n",
				task->prefences[i].syncpoint_index,
				task->prefences[i].syncpoint_value);

		/* Put fences into a separate array */
		syncpt_wait_ids[i] =
				task->prefences[i].syncpoint_index;
		syncpt_wait_thresh[i] =
				task->prefences[i].syncpoint_value;
	}
	spec_bar(); /* break_spec_p#5_1 */

	cmdbuf[0] = nvhost_opcode_incr(NV_DLA_THI_METHOD_ID >> 2, 2);
	cmdbuf[1] = method_id;
	cmdbuf[2] = method_data;

	err = nvdla_queue_submit_to_host1x(queue,
					    cmdbuf,
					    ARRAY_SIZE(cmdbuf),
					    1,
					    syncpt_wait_ids,
					    syncpt_wait_thresh,
					    task->num_prefences,
					    &task->fence);
	if (err) {
		nvdla_dbg_err(pdev, "channel submit failed");
		goto done;
	}

	nvdla_dbg_info(pdev, "task submitted through channel mode");

	if (!wait)
		goto done;

	timeout = msecs_to_jiffies(CMD_TIMEOUT_MSEC);

	if (!wait_for_completion_timeout(&nvdla_dev->cmd_completion, timeout)) {
		nvdla_dbg_err(pdev, "channel mode submit timedout");
		err = -ETIMEDOUT;
		goto done;
	}

done:
	nvdla_dev->waiting = 0;
	return 0;
}
