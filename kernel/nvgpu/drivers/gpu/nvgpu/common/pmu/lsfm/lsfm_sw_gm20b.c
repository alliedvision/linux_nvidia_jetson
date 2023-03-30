/*
 * Copyright (c) 2015-2022, NVIDIA CORPORATION.  All rights reserved.
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

#include <nvgpu/timers.h>
#include <nvgpu/pmu.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/bug.h>
#include <nvgpu/string.h>
#include <nvgpu/pmu/cmd.h>
#include <nvgpu/pmu/lsfm.h>
#include <nvgpu/pmu/fw.h>
#include <nvgpu/pmu/clk/clk.h>

#include "lsfm_sw_gm20b.h"

static void lsfm_handle_acr_init_wpr_region_msg(struct gk20a *g,
	struct pmu_msg *msg, void *param, u32 status)
{
	struct nvgpu_pmu *pmu = g->pmu;

	(void)param;
	(void)status;

	nvgpu_log_fn(g, " ");

	nvgpu_pmu_dbg(g, "reply PMU_ACR_CMD_ID_INIT_WPR_REGION");

	if (msg->msg.acr.acrmsg.errorcode == PMU_ACR_SUCCESS) {
		pmu->lsfm->is_wpr_init_done = true;
	}
}

int gm20b_pmu_lsfm_init_acr_wpr_region(struct gk20a *g, struct nvgpu_pmu *pmu)
{
	struct pmu_cmd cmd;
	size_t tmp_size;

	nvgpu_log_fn(g, " ");

	/* init ACR */
	(void) memset(&cmd, 0, sizeof(struct pmu_cmd));
	cmd.hdr.unit_id = PMU_UNIT_ACR;

	tmp_size = PMU_CMD_HDR_SIZE +
		sizeof(struct pmu_acr_cmd_init_wpr_details);
	nvgpu_assert(tmp_size <= (size_t)U8_MAX);
	cmd.hdr.size = (u8)tmp_size;

	cmd.cmd.acr.init_wpr.cmd_type = PMU_ACR_CMD_ID_INIT_WPR_REGION;
	cmd.cmd.acr.init_wpr.regionid = 0x01U;
	cmd.cmd.acr.init_wpr.wproffset = 0x00U;

	nvgpu_pmu_dbg(g, "cmd post PMU_ACR_CMD_ID_INIT_WPR_REGION");

	return nvgpu_pmu_cmd_post(g, &cmd, NULL, PMU_COMMAND_QUEUE_HPQ,
		lsfm_handle_acr_init_wpr_region_msg, pmu);
}

void gm20b_pmu_lsfm_handle_bootstrap_falcon_msg(struct gk20a *g,
	struct pmu_msg *msg, void *param, u32 status)
{
	struct nvgpu_pmu *pmu = g->pmu;

	(void)param;
	(void)status;

	nvgpu_log_fn(g, " ");

	nvgpu_pmu_dbg(g, "reply PMU_ACR_CMD_ID_BOOTSTRAP_FALCON");
	nvgpu_pmu_dbg(g, "response code = %x", msg->msg.acr.acrmsg.falconid);

	pmu->lsfm->loaded_falcon_id = msg->msg.acr.acrmsg.falconid;
}

static int gm20b_pmu_lsfm_bootstrap_falcon(struct gk20a *g,
	struct nvgpu_pmu *pmu, struct nvgpu_pmu_lsfm *lsfm,
	u32 falcon_id, u32 flags)
{
	struct pmu_cmd cmd;
	size_t tmp_size;

	nvgpu_log_fn(g, " ");

	lsfm->loaded_falcon_id = 0U;

	/* send message to load FECS falcon */
	(void) memset(&cmd, 0, sizeof(struct pmu_cmd));
	cmd.hdr.unit_id = PMU_UNIT_ACR;

	tmp_size = PMU_CMD_HDR_SIZE +
		sizeof(struct pmu_acr_cmd_bootstrap_falcon);
	nvgpu_assert(tmp_size <= (size_t)U8_MAX);
	cmd.hdr.size = (u8)tmp_size;

	cmd.cmd.acr.bootstrap_falcon.cmd_type =
		PMU_ACR_CMD_ID_BOOTSTRAP_FALCON;
	cmd.cmd.acr.bootstrap_falcon.flags = flags;
	cmd.cmd.acr.bootstrap_falcon.falconid = falcon_id;
	nvgpu_pmu_dbg(g, "cmd post PMU_ACR_CMD_ID_BOOTSTRAP_FALCON: %x",
		falcon_id);

	return nvgpu_pmu_cmd_post(g, &cmd, NULL, PMU_COMMAND_QUEUE_HPQ,
			gm20b_pmu_lsfm_handle_bootstrap_falcon_msg, pmu);
}

static int gm20b_pmu_lsfm_bootstrap_ls_falcon(struct gk20a *g,
	struct nvgpu_pmu *pmu, struct nvgpu_pmu_lsfm *lsfm, u32 falcon_id_mask)
{
	int  err = 0;
	u32 flags = PMU_ACR_CMD_BOOTSTRAP_FALCON_FLAGS_RESET_YES;

	/* GM20B PMU supports loading FECS only */
	if (!(falcon_id_mask == BIT32(FALCON_ID_FECS))) {
		return -EINVAL;
	}

	/* load FECS */
	nvgpu_falcon_mailbox_write(&g->fecs_flcn, FALCON_MAILBOX_0, ~U32(0x0U));

	err = gm20b_pmu_lsfm_bootstrap_falcon(g, pmu, lsfm,
		FALCON_ID_FECS, flags);
	if (err != 0) {
		return err;
	}

	nvgpu_assert(falcon_id_mask <= U8_MAX);
	pmu_wait_message_cond(g->pmu, nvgpu_get_poll_timeout(g),
		&lsfm->loaded_falcon_id, (u8)FALCON_ID_FECS);
	if (lsfm->loaded_falcon_id != FALCON_ID_FECS) {
		err = -ETIMEDOUT;
	}

	return err;
}

int gm20b_pmu_lsfm_pmu_cmd_line_args_copy(struct gk20a *g,
	struct nvgpu_pmu *pmu)
{
	u32 cmd_line_args_offset = 0U;
	u32 dmem_size = 0U;
	int err = 0;

	err = nvgpu_falcon_get_mem_size(pmu->flcn, MEM_DMEM, &dmem_size);
	if (err != 0) {
		nvgpu_err(g, "dmem size request failed");
		return -EINVAL;
	}

	cmd_line_args_offset = dmem_size -
		pmu->fw->ops.get_cmd_line_args_size(pmu);

	/* Copying pmu cmdline args */
	pmu->fw->ops.set_cmd_line_args_cpu_freq(pmu,
		(u32)g->ops.clk.get_rate(g, CTRL_CLK_DOMAIN_PWRCLK));
	pmu->fw->ops.set_cmd_line_args_secure_mode(pmu, 1U);
	pmu->fw->ops.set_cmd_line_args_trace_size(
		pmu, PMU_RTOS_TRACE_BUFSIZE);
	pmu->fw->ops.set_cmd_line_args_trace_dma_base(pmu);
	pmu->fw->ops.set_cmd_line_args_trace_dma_idx(
		pmu, GK20A_PMU_DMAIDX_VIRT);

	return nvgpu_falcon_copy_to_dmem(pmu->flcn, cmd_line_args_offset,
		(u8 *)(pmu->fw->ops.get_cmd_line_args_ptr(pmu)),
		pmu->fw->ops.get_cmd_line_args_size(pmu), 0U);
}

void nvgpu_gm20b_lsfm_sw_init(struct gk20a *g, struct nvgpu_pmu_lsfm *lsfm)
{
	nvgpu_log_fn(g, " ");

	lsfm->is_wpr_init_done = false;
	lsfm->loaded_falcon_id = 0U;

	lsfm->init_wpr_region = gm20b_pmu_lsfm_init_acr_wpr_region;
	lsfm->bootstrap_ls_falcon = gm20b_pmu_lsfm_bootstrap_ls_falcon;
	lsfm->ls_pmu_cmdline_args_copy = gm20b_pmu_lsfm_pmu_cmd_line_args_copy;
}
