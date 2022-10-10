/*
 * Copyright (c) 2016-2022, NVIDIA CORPORATION.  All rights reserved.
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

#include <nvgpu/pmu.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/pmu/lpwr.h>
#include <nvgpu/bug.h>
#include <nvgpu/string.h>
#include <nvgpu/pmu/cmd.h>
#include <nvgpu/pmu/pmu_pstate.h>


static void pmu_handle_rppg_init_msg(struct gk20a *g, struct pmu_msg *msg,
	void *param, u32 status)
{
	u32 *success = param;

	if (status == 0U) {
		switch (msg->msg.pg.rppg_msg.cmn.msg_id) {
		case NV_PMU_RPPG_MSG_ID_INIT_CTRL_ACK:
			*success = 1;
			nvgpu_pmu_dbg(g, "RPPG is acknowledged from PMU %x",
				msg->msg.pg.msg_type);
			break;
		default:
			*success = 0;
			nvgpu_err(g, "Invalid message ID:%u",
				msg->msg.pg.rppg_msg.cmn.msg_id);
			break;
		}
	}
}

static int rppg_send_cmd(struct gk20a *g, struct nv_pmu_rppg_cmd *prppg_cmd)
{
	struct pmu_cmd cmd;
	int status = 0;
	u32 success = 0;
	size_t tmp_size = PMU_CMD_HDR_SIZE + sizeof(struct nv_pmu_rppg_cmd);

	(void) memset(&cmd, 0, sizeof(struct pmu_cmd));
	cmd.hdr.unit_id = PMU_UNIT_PG;
	nvgpu_assert(tmp_size <= U8_MAX);
	cmd.hdr.size = (u8)tmp_size;

	cmd.cmd.pg.rppg_cmd.cmn.cmd_type = PMU_PMU_PG_CMD_ID_RPPG;
	cmd.cmd.pg.rppg_cmd.cmn.cmd_id   = prppg_cmd->cmn.cmd_id;

	switch (prppg_cmd->cmn.cmd_id) {
	case NV_PMU_RPPG_CMD_ID_INIT:
		break;
	case NV_PMU_RPPG_CMD_ID_INIT_CTRL:
		cmd.cmd.pg.rppg_cmd.init_ctrl.ctrl_id =
			prppg_cmd->init_ctrl.ctrl_id;
		cmd.cmd.pg.rppg_cmd.init_ctrl.domain_id =
			prppg_cmd->init_ctrl.domain_id;
		break;
	case NV_PMU_RPPG_CMD_ID_STATS_RESET:
		cmd.cmd.pg.rppg_cmd.stats_reset.ctrl_id =
			prppg_cmd->stats_reset.ctrl_id;
		break;
	default:
		nvgpu_err(g, "Invalid RPPG command %d",
			prppg_cmd->cmn.cmd_id);
		status = -1;
		break;
	}

	if (status != 0) {
		goto exit;
	}

	status = nvgpu_pmu_cmd_post(g, &cmd, NULL, PMU_COMMAND_QUEUE_HPQ,
			pmu_handle_rppg_init_msg, &success);
	if (status != 0) {
		nvgpu_err(g, "Unable to submit parameter command %d",
			prppg_cmd->cmn.cmd_id);
		goto exit;
	}

	if (prppg_cmd->cmn.cmd_id == NV_PMU_RPPG_CMD_ID_INIT_CTRL) {
		pmu_wait_message_cond(g->pmu, nvgpu_get_poll_timeout(g),
			&success, 1);
		if (success == 0U) {
			status = -EINVAL;
			nvgpu_err(g, "Ack for the parameter command %x",
				prppg_cmd->cmn.cmd_id);
		}
	}

exit:
	return status;
}

static int rppg_init(struct gk20a *g)
{
	struct nv_pmu_rppg_cmd rppg_cmd;

	rppg_cmd.init.cmd_id = NV_PMU_RPPG_CMD_ID_INIT;

	return rppg_send_cmd(g, &rppg_cmd);
}

static int rppg_ctrl_init(struct gk20a *g, u8 ctrl_id)
{
	struct nv_pmu_rppg_cmd rppg_cmd;

	rppg_cmd.init_ctrl.cmd_id  = NV_PMU_RPPG_CMD_ID_INIT_CTRL;
	rppg_cmd.init_ctrl.ctrl_id = ctrl_id;

	switch (ctrl_id) {
	case NV_PMU_RPPG_CTRL_ID_GR:
		rppg_cmd.init_ctrl.domain_id = NV_PMU_RPPG_DOMAIN_ID_GFX;
		break;
	case NV_PMU_RPPG_CTRL_ID_MS:
		rppg_cmd.init_ctrl.domain_id = NV_PMU_RPPG_DOMAIN_ID_GFX;
		break;
	default:
		nvgpu_err(g, "Invalid ctrl_id %u for %s", ctrl_id, __func__);
		break;
	}

	return rppg_send_cmd(g, &rppg_cmd);
}

int init_rppg(struct gk20a *g)
{
	int status;

	status = rppg_init(g);
	if (status != 0) {
		nvgpu_err(g,
			"Failed to initialize RPPG in PMU: 0x%08x", status);
		return status;
	}


	status = rppg_ctrl_init(g, NV_PMU_RPPG_CTRL_ID_GR);
	if (status != 0) {
		nvgpu_err(g,
			"Failed to initialize RPPG_CTRL: GR in PMU: 0x%08x",
			status);
		return status;
	}

	status = rppg_ctrl_init(g, NV_PMU_RPPG_CTRL_ID_MS);
	if (status != 0) {
		nvgpu_err(g,
			"Failed to initialize RPPG_CTRL: MS in PMU: 0x%08x",
			status);
		return status;
	}

	return status;
}
