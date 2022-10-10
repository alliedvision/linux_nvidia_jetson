/*
 * Copyright (c) 2019-2022, NVIDIA CORPORATION.  All rights reserved.
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

#include <nvgpu/pmu/pmu_pg.h>
#include <nvgpu/pmu/pmuif/nvgpu_cmdif.h>
#include <nvgpu/bug.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/string.h>
#include <nvgpu/pmu/cmd.h>

int nvgpu_aelpg_init(struct gk20a *g)
{
	int status = 0;

	/* Remove reliance on app_ctrl field. */
	union pmu_ap_cmd ap_cmd;

	ap_cmd.init.cmd_id = PMU_AP_CMD_ID_INIT;
	ap_cmd.init.pg_sampling_period_us = g->pmu->pg->aelpg_param[0];

	status = nvgpu_pmu_ap_send_command(g, &ap_cmd, false);
	return status;
}

int nvgpu_aelpg_init_and_enable(struct gk20a *g, u8 ctrl_id)
{
	struct nvgpu_pmu *pmu = g->pmu;
	int status = 0;
	union pmu_ap_cmd ap_cmd;

	ap_cmd.init_and_enable_ctrl.cmd_id = PMU_AP_CMD_ID_INIT_AND_ENABLE_CTRL;
	ap_cmd.init_and_enable_ctrl.ctrl_id = ctrl_id;
	ap_cmd.init_and_enable_ctrl.params.min_idle_filter_us =
			pmu->pg->aelpg_param[1];
	ap_cmd.init_and_enable_ctrl.params.min_target_saving_us =
			pmu->pg->aelpg_param[2];
	ap_cmd.init_and_enable_ctrl.params.power_break_even_us =
			pmu->pg->aelpg_param[3];
	ap_cmd.init_and_enable_ctrl.params.cycles_per_sample_max =
			pmu->pg->aelpg_param[4];

	switch (ctrl_id) {
	case PMU_AP_CTRL_ID_GRAPHICS:
		break;
	default:
		nvgpu_err(g, "Invalid ctrl_id:%u for %s", ctrl_id, __func__);
		break;
	}

	status = nvgpu_pmu_ap_send_command(g, &ap_cmd, true);
	return status;
}

/* AELPG */
static void ap_callback_init_and_enable_ctrl(
		struct gk20a *g, struct pmu_msg *msg,
		void *param, u32 status)
{
	(void)param;

	WARN_ON(msg == NULL);

	if (status == 0U) {
		switch (msg->msg.pg.ap_msg.cmn.msg_id) {
		case PMU_AP_MSG_ID_INIT_ACK:
			nvgpu_pmu_dbg(g, "reply PMU_AP_CMD_ID_INIT");
			break;

		default:
			nvgpu_pmu_dbg(g, "%s: Invalid Adaptive Power Message: %x",
				__func__, msg->msg.pg.ap_msg.cmn.msg_id);
			break;
		}
	}
}

/* Send an Adaptive Power (AP) related command to PMU */
int nvgpu_pmu_ap_send_command(struct gk20a *g,
		union pmu_ap_cmd *p_ap_cmd, bool b_block)
{
	struct nvgpu_pmu *pmu = g->pmu;
	int status = 0;
	struct pmu_cmd cmd;
	pmu_callback p_callback = NULL;
	u64 tmp;

	(void)b_block;

	(void) memset(&cmd, 0, sizeof(struct pmu_cmd));

	/* Copy common members */
	cmd.hdr.unit_id = PMU_UNIT_PG;
	tmp = PMU_CMD_HDR_SIZE + sizeof(union pmu_ap_cmd);
	nvgpu_assert(tmp <= U8_MAX);
	cmd.hdr.size = (u8)tmp;

	cmd.cmd.pg.ap_cmd.cmn.cmd_type = PMU_PG_CMD_ID_AP;
	cmd.cmd.pg.ap_cmd.cmn.cmd_id = p_ap_cmd->cmn.cmd_id;

	/* Copy other members of command */
	switch (p_ap_cmd->cmn.cmd_id) {
	case PMU_AP_CMD_ID_INIT:
		nvgpu_pmu_dbg(g, "cmd post PMU_AP_CMD_ID_INIT");
		cmd.cmd.pg.ap_cmd.init.pg_sampling_period_us =
			p_ap_cmd->init.pg_sampling_period_us;
		break;

	case PMU_AP_CMD_ID_INIT_AND_ENABLE_CTRL:
		nvgpu_pmu_dbg(g, "cmd post PMU_AP_CMD_ID_INIT_AND_ENABLE_CTRL");
		cmd.cmd.pg.ap_cmd.init_and_enable_ctrl.ctrl_id =
		p_ap_cmd->init_and_enable_ctrl.ctrl_id;
		nvgpu_memcpy(
			(u8 *)&(cmd.cmd.pg.ap_cmd.init_and_enable_ctrl.params),
			(u8 *)&(p_ap_cmd->init_and_enable_ctrl.params),
			sizeof(struct pmu_ap_ctrl_init_params));

		p_callback = ap_callback_init_and_enable_ctrl;
		break;

	case PMU_AP_CMD_ID_ENABLE_CTRL:
		nvgpu_pmu_dbg(g, "cmd post PMU_AP_CMD_ID_ENABLE_CTRL");
		cmd.cmd.pg.ap_cmd.enable_ctrl.ctrl_id =
			p_ap_cmd->enable_ctrl.ctrl_id;
		break;

	case PMU_AP_CMD_ID_DISABLE_CTRL:
		nvgpu_pmu_dbg(g, "cmd post PMU_AP_CMD_ID_DISABLE_CTRL");
		cmd.cmd.pg.ap_cmd.disable_ctrl.ctrl_id =
			p_ap_cmd->disable_ctrl.ctrl_id;
		break;

	case PMU_AP_CMD_ID_KICK_CTRL:
		nvgpu_pmu_dbg(g, "cmd post PMU_AP_CMD_ID_KICK_CTRL");
		cmd.cmd.pg.ap_cmd.kick_ctrl.ctrl_id =
			p_ap_cmd->kick_ctrl.ctrl_id;
		cmd.cmd.pg.ap_cmd.kick_ctrl.skip_count =
			p_ap_cmd->kick_ctrl.skip_count;
		break;

	default:
		nvgpu_pmu_dbg(g, "%s: Invalid Adaptive Power command %d\n",
			__func__, p_ap_cmd->cmn.cmd_id);
		status = 0x2f;
		break;
	}

	if (status != 0) {
		goto err_return;
	}

	status = nvgpu_pmu_cmd_post(g, &cmd, NULL, PMU_COMMAND_QUEUE_HPQ,
			p_callback, pmu);

	if (status != 0) {
		nvgpu_pmu_dbg(g,
			"%s: Unable to submit Adaptive Power Command %d\n",
			__func__, p_ap_cmd->cmn.cmd_id);
		goto err_return;
	}

err_return:
	return status;
}
