/*
 * Copyright (c) 2017-2020, NVIDIA CORPORATION.  All rights reserved.
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
#include <nvgpu/bug.h>
#include <nvgpu/pmu/cmd.h>
#include <nvgpu/pmu/pmu_pg.h>

#include "pg_sw_gv11b.h"
#include "pg_sw_gp106.h"
#include "pg_sw_gm20b.h"

static void pmu_handle_pg_sub_feature_msg(struct gk20a *g, struct pmu_msg *msg,
			void *param, u32 status)
{
	nvgpu_log_fn(g, " ");

	if (status != 0U) {
		nvgpu_err(g, "Sub-feature mask update cmd aborted");
		return;
	}

	nvgpu_pmu_dbg(g, "sub-feature mask update is acknowledged from PMU %x",
		msg->msg.pg.msg_type);
}

static void pmu_handle_pg_param_msg(struct gk20a *g, struct pmu_msg *msg,
			void *param, u32 status)
{
	nvgpu_log_fn(g, " ");

	if (status != 0U) {
		nvgpu_err(g, "GR PARAM cmd aborted");
		return;
	}

	nvgpu_pmu_dbg(g, "GR PARAM is acknowledged from PMU %x",
		msg->msg.pg.msg_type);
}

int gv11b_pg_gr_init(struct gk20a *g, u32 pg_engine_id)
{
	struct nvgpu_pmu *pmu = g->pmu;
	struct pmu_cmd cmd;
	size_t tmp_size;

	if (pg_engine_id == PMU_PG_ELPG_ENGINE_ID_GRAPHICS) {
		(void) memset(&cmd, 0, sizeof(struct pmu_cmd));
		cmd.hdr.unit_id = PMU_UNIT_PG;
		tmp_size = PMU_CMD_HDR_SIZE +
				sizeof(struct pmu_pg_cmd_gr_init_param_v1);
		nvgpu_assert(tmp_size <= (size_t)U8_MAX);
		cmd.hdr.size = (u8)tmp_size;
		cmd.cmd.pg.gr_init_param_v1.cmd_type =
				PMU_PG_CMD_ID_PG_PARAM;
		cmd.cmd.pg.gr_init_param_v1.sub_cmd_id =
				PMU_PG_PARAM_CMD_GR_INIT_PARAM;
		cmd.cmd.pg.gr_init_param_v1.featuremask =
				NVGPU_PMU_GR_FEATURE_MASK_ALL;

		nvgpu_pmu_dbg(g, "cmd post PMU_PG_CMD_ID_PG_PARAM_INIT");
		nvgpu_pmu_cmd_post(g, &cmd, NULL, PMU_COMMAND_QUEUE_HPQ,
				pmu_handle_pg_param_msg, pmu);

	} else {
		return -EINVAL;
	}

	return 0;
}

int gv11b_pg_set_subfeature_mask(struct gk20a *g, u32 pg_engine_id)
{
	struct nvgpu_pmu *pmu = g->pmu;
	struct pmu_cmd cmd;
	size_t tmp_size;

	if (pg_engine_id == PMU_PG_ELPG_ENGINE_ID_GRAPHICS) {
		(void) memset(&cmd, 0, sizeof(struct pmu_cmd));
		cmd.hdr.unit_id = PMU_UNIT_PG;
		tmp_size = PMU_CMD_HDR_SIZE +
			sizeof(struct pmu_pg_cmd_sub_feature_mask_update);
		nvgpu_assert(tmp_size <= (size_t)U8_MAX);
		cmd.hdr.size = (u8)tmp_size;
		cmd.cmd.pg.sf_mask_update.cmd_type =
				PMU_PG_CMD_ID_PG_PARAM;
		cmd.cmd.pg.sf_mask_update.sub_cmd_id =
				PMU_PG_PARAM_CMD_SUB_FEATURE_MASK_UPDATE;
		cmd.cmd.pg.sf_mask_update.ctrl_id =
				PMU_PG_ELPG_ENGINE_ID_GRAPHICS;
		cmd.cmd.pg.sf_mask_update.enabled_mask =
				NVGPU_PMU_GR_FEATURE_MASK_POWER_GATING |
				NVGPU_PMU_GR_FEATURE_MASK_PRIV_RING |
				NVGPU_PMU_GR_FEATURE_MASK_UNBIND |
				NVGPU_PMU_GR_FEATURE_MASK_SAVE_GLOBAL_STATE |
				NVGPU_PMU_GR_FEATURE_MASK_RESET_ENTRY |
				NVGPU_PMU_GR_FEATURE_MASK_HW_SEQUENCE |
				NVGPU_PMU_GR_FEATURE_MASK_ELPG_SRAM |
				NVGPU_PMU_GR_FEATURE_MASK_ELPG_LOGIC |
				NVGPU_PMU_GR_FEATURE_MASK_ELPG_L2RPPG;

		nvgpu_pmu_dbg(g, "cmd post PMU_PG_CMD_SUB_FEATURE_MASK_UPDATE");
		nvgpu_pmu_cmd_post(g, &cmd, NULL, PMU_COMMAND_QUEUE_HPQ,
				pmu_handle_pg_sub_feature_msg, pmu);
	} else {
		return -EINVAL;
	}

	return 0;
}

void nvgpu_gv11b_pg_sw_init(struct gk20a *g,
		struct nvgpu_pmu_pg *pg)
{
	pg->elpg_statistics = gp106_pmu_elpg_statistics;
	pg->init_param = gv11b_pg_gr_init;
	pg->supported_engines_list = gm20b_pmu_pg_engines_list;
	pg->engines_feature_list = gm20b_pmu_pg_feature_list;
	pg->set_sub_feature_mask = gv11b_pg_set_subfeature_mask;
	pg->save_zbc = gm20b_pmu_save_zbc;
	pg->allow = gm20b_pmu_pg_elpg_allow;
	pg->disallow = gm20b_pmu_pg_elpg_disallow;
	pg->init = gm20b_pmu_pg_elpg_init;
	pg->alloc_dmem = gm20b_pmu_pg_elpg_alloc_dmem;
	pg->load_buff = gm20b_pmu_pg_elpg_load_buff;
	pg->hw_load_zbc = gm20b_pmu_pg_elpg_hw_load_zbc;
	pg->rpc_handler = NULL;
	pg->init_send = gm20b_pmu_pg_init_send;
}
