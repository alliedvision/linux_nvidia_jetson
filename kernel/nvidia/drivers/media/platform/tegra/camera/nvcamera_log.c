/*
 * nvcamera_log.c - general tracing function for vi and isp API calls
 *
 * Copyright (c) 2018-2020, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include "nvcamera_log.h"
#include <linux/nvhost.h>
#include <linux/platform_device.h>
#include <uapi/linux/nvhost_events.h>

/*
 * Set to 1 to enable additional kernel API traces
 */
#define NVCAM_ENABLE_EXTRA_TRACES	0

#if defined(CONFIG_EVENTLIB)
#include <linux/keventlib.h>

/*
 * Camera "task submission" event enabled by default
 */
void nv_camera_log_submit(struct platform_device *pdev,
		u32 syncpt_id,
		u32 syncpt_thresh,
		u32 channel_id,
		u64 timestamp)
{
	struct nvhost_device_data *pdata = platform_get_drvdata(pdev);
	struct nvhost_task_submit task_submit;

	if (!pdata->eventlib_id)
		return;

	/*
	 * Write task submit event
	 */
	task_submit.syncpt_id = syncpt_id;
	task_submit.syncpt_thresh = syncpt_thresh;
	task_submit.channel_id = channel_id;
	task_submit.class_id = pdata->class;

	/*
	 * Eventlib events are meant to be matched with their userspace
	 * analogues. Instead of the PID as (this) thread's ID use the
	 * inherited thread group ID. For the reported TID use this thread's
	 * ID (i.e. PID).
	 */
	task_submit.tid = current->pid;
	task_submit.pid = current->tgid;

	keventlib_write(pdata->eventlib_id,
			&task_submit,
			sizeof(task_submit),
			NVHOST_TASK_SUBMIT,
			timestamp);
}

#else

void nv_camera_log_submit(struct platform_device *pdev,
		u32 syncpt_id,
		u32 syncpt_thresh,
		u32 channel_id,
		u64 timestamp)
{
}

#endif

#if defined(CONFIG_EVENTLIB) && NVCAM_ENABLE_EXTRA_TRACES
#include <linux/keventlib.h>

/*
 * Additional camera traces disabled by default
 */
void nv_camera_log(struct platform_device *pdev,
		u64 timestamp,
		u32 type)
{
	struct nvhost_device_data *pdata = platform_get_drvdata(pdev);
	struct nv_camera_task_log task_log;

	if (!pdata->eventlib_id)
		return;

	/*
	 * Write task log event
	 */
	task_log.class_id = pdata->class;

	/*
	 * Eventlib events are meant to be matched with their userspace
	 * analogues. Instead of the PID as (this) thread's ID use the
	 * inherited thread group ID. For the reported TID use this thread's
	 * ID (i.e. PID).
	 */
	task_log.tid = current->pid;
	task_log.pid = current->tgid;

	keventlib_write(pdata->eventlib_id,
			&task_log,
			sizeof(task_log),
			type,
			timestamp);
}

#else

void nv_camera_log(struct platform_device *pdev,
		u64 timestamp,
		u32 type)
{
}

#endif
