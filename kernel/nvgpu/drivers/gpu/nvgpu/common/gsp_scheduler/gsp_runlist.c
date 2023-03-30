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

#include "gsp_runlist.h"

#include <nvgpu/gk20a.h>
#include <nvgpu/log.h>
#include <nvgpu/gsp.h>
#include <nvgpu/io.h>
#include <nvgpu/device.h>
#include <nvgpu/nvgpu_mem.h>
#include <nvgpu/runlist.h>
#include <nvgpu/string.h>

#include "ipc/gsp_cmd.h"
#include "ipc/gsp_msg.h"

static void gsp_handle_cmd_ack(struct gk20a *g, struct nv_flcn_msg_gsp *msg,
	void *param, u32 status)
{
	bool *command_ack = param;

	nvgpu_log_fn(g, " ");

	switch (msg->hdr.unit_id) {
	case NV_GSP_UNIT_NULL:
		nvgpu_info(g, "Reply to NV_GSP_UNIT_NULL");
		*command_ack = true;
		break;
	case NV_GSP_UNIT_SUBMIT_RUNLIST:
		nvgpu_info(g, "Reply to NV_GSP_UNIT_RUNLIST_INFO");
		*command_ack = true;
		break;
	case NV_GSP_UNIT_DEVICES_INFO:
		nvgpu_info(g, "Reply to NV_GSP_UNIT_DEVICES_INFO");
		*command_ack = true;
		break;
	default:
		nvgpu_err(g, "Un-handled response from GSP");
		*command_ack = false;
		break;
	}

	(void)status;
}

static void gsp_get_runlist_info(struct gk20a *g,
		struct nvgpu_gsp_runlist_info *rl_info, struct nvgpu_runlist *runlist)
{
	u64 runlist_iova;
	u32 aperture, num_entries;

	runlist_iova = nvgpu_mem_get_addr(g, &runlist->domain->mem_hw->mem);

	num_entries = runlist->domain->mem_hw->count;

	aperture = g->ops.runlist.get_runlist_aperture(g, runlist);

	rl_info->runlist_base_lo = u64_lo32(runlist_iova);
	rl_info->runlist_base_hi = u64_hi32(runlist_iova);
	rl_info->aperture = aperture;
	rl_info->num_entries = num_entries;
	rl_info->runlist_id = runlist->id;
}

int nvgpu_gsp_runlist_submit(struct gk20a *g, struct nvgpu_runlist *runlist)
{
	struct nv_flcn_cmd_gsp cmd;
	bool command_ack = false;
	int err = 0;
	size_t tmp_size;

	nvgpu_log_fn(g, " ");

	(void) memset(&cmd, 0, sizeof(struct nv_flcn_cmd_gsp));
	cmd.hdr.unit_id = NV_GSP_UNIT_SUBMIT_RUNLIST;
	tmp_size = GSP_CMD_HDR_SIZE + sizeof(struct nvgpu_gsp_runlist_info);
	nvgpu_assert(tmp_size <= U64(U8_MAX));
	cmd.hdr.size = (u8)tmp_size;

	/* copy domain info into cmd buffer */
	gsp_get_runlist_info(g, &cmd.cmd.runlist, runlist);

	err = nvgpu_gsp_cmd_post(g, &cmd, GSP_NV_CMDQ_LOG_ID,
			gsp_handle_cmd_ack, &command_ack, U32_MAX);
	if (err != 0) {
		nvgpu_err(g, "command post failed");
		goto exit;
	}

	err = nvgpu_gsp_wait_message_cond(g, nvgpu_get_poll_timeout(g),
			&command_ack, U8(true));
	if (err != 0) {
		nvgpu_err(g, "command ack receive failed");
	}

exit:
	return err;
}

static void gsp_get_device_info(struct gk20a *g,
		struct nvgpu_gsp_device_info *dev_info)
{
	const struct nvgpu_device *device;

	/* Only GRAPHICS 0-instance is supported by  GSP scheduler.
	 * In future, more devices can be looped through and send it to the GSP.
	 */
	device = nvgpu_device_get(g, NVGPU_DEVTYPE_GRAPHICS, 0);

	/* copy domain info into cmd buffer */
	dev_info->device_id = NVGPU_DEVTYPE_GRAPHICS;
	dev_info->is_engine = true;
	dev_info->engine_type = device->type;
	dev_info->engine_id = device->engine_id;
	dev_info->instance_id = device->inst_id;
	dev_info->rl_engine_id = device->rleng_id;
	dev_info->dev_pri_base = device->pri_base;
	dev_info->runlist_pri_base = device->rl_pri_base;
}

int nvgpu_gsp_send_devices_info(struct gk20a *g)
{
	struct nv_flcn_cmd_gsp cmd;
	bool command_ack = false;
	int err = 0;
	size_t tmp_size;

	nvgpu_log_fn(g, " ");

	(void) memset(&cmd, 0, sizeof(struct nv_flcn_cmd_gsp));
	cmd.hdr.unit_id = NV_GSP_UNIT_DEVICES_INFO;
	tmp_size = GSP_CMD_HDR_SIZE + sizeof(struct nvgpu_gsp_device_info);
	nvgpu_assert(tmp_size <= U64(U8_MAX));
	cmd.hdr.size = (u8)tmp_size;

	/* copy domain info into cmd buffer */
	gsp_get_device_info(g, &cmd.cmd.device);

	err = nvgpu_gsp_cmd_post(g, &cmd, GSP_NV_CMDQ_LOG_ID,
			gsp_handle_cmd_ack, &command_ack, U32_MAX);
	if (err != 0) {
		nvgpu_err(g, "command post failed");
		goto exit;
	}

	err = nvgpu_gsp_wait_message_cond(g, nvgpu_get_poll_timeout(g),
			&command_ack, U8(true));
	if (err != 0) {
		nvgpu_err(g, "command ack receive failed");
	}

exit:
	return err;
}
