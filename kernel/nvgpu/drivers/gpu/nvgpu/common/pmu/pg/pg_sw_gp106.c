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
#include <nvgpu/pmu/lpwr.h>
#include <nvgpu/bug.h>
#include <nvgpu/pmu/cmd.h>
#include <nvgpu/clk_arb.h>
#include <nvgpu/pmu/pmu_pg.h>

#include "pg_sw_gp106.h"

static void pmu_handle_param_msg(struct gk20a *g, struct pmu_msg *msg,
			void *param, u32 status)
{
	nvgpu_log_fn(g, " ");

	if (status != 0U) {
		nvgpu_err(g, "PG PARAM cmd aborted");
		return;
	}

	nvgpu_pmu_dbg(g, "PG PARAM is acknowledged from PMU %x",
		msg->msg.pg.msg_type);
}

int gp106_pg_param_init(struct gk20a *g, u32 pg_engine_id)
{
	struct nvgpu_pmu *pmu = g->pmu;
	struct pmu_cmd cmd;
	int status;
	u64 tmp_size;

	(void) memset(&cmd, 0, sizeof(struct pmu_cmd));
	if (pg_engine_id == PMU_PG_ELPG_ENGINE_ID_GRAPHICS) {

		status = init_rppg(g);
		if (status != 0) {
			nvgpu_err(g, "RPPG init Failed");
			return -1;
		}

		cmd.hdr.unit_id = PMU_UNIT_PG;
		tmp_size = PMU_CMD_HDR_SIZE +
			sizeof(struct pmu_pg_cmd_gr_init_param);
		nvgpu_assert(tmp_size <= U64(U8_MAX));
		cmd.hdr.size = U8(tmp_size);
		cmd.cmd.pg.gr_init_param.cmd_type =
			PMU_PG_CMD_ID_PG_PARAM;
		cmd.cmd.pg.gr_init_param.sub_cmd_id =
			PMU_PG_PARAM_CMD_GR_INIT_PARAM;
		cmd.cmd.pg.gr_init_param.featuremask =
			NVGPU_PMU_GR_FEATURE_MASK_RPPG;

		nvgpu_pmu_dbg(g, "cmd post GR PMU_PG_CMD_ID_PG_PARAM");
		nvgpu_pmu_cmd_post(g, &cmd, NULL, PMU_COMMAND_QUEUE_HPQ,
			pmu_handle_param_msg, pmu);
	} else if (pg_engine_id == PMU_PG_ELPG_ENGINE_ID_MS) {
		cmd.hdr.unit_id = PMU_UNIT_PG;
		tmp_size = PMU_CMD_HDR_SIZE +
			sizeof(struct pmu_pg_cmd_ms_init_param);
		nvgpu_assert(tmp_size <= U64(U8_MAX));
		cmd.hdr.size = U8(tmp_size);
		cmd.cmd.pg.ms_init_param.cmd_type =
			PMU_PG_CMD_ID_PG_PARAM;
		cmd.cmd.pg.ms_init_param.cmd_id =
			PMU_PG_PARAM_CMD_MS_INIT_PARAM;
		cmd.cmd.pg.ms_init_param.support_mask =
			NVGPU_PMU_MS_FEATURE_MASK_CLOCK_GATING |
			NVGPU_PMU_MS_FEATURE_MASK_SW_ASR |
			NVGPU_PMU_MS_FEATURE_MASK_RPPG |
			NVGPU_PMU_MS_FEATURE_MASK_FB_TRAINING;

		nvgpu_pmu_dbg(g, "cmd post MS PMU_PG_CMD_ID_PG_PARAM");
		nvgpu_pmu_cmd_post(g, &cmd, NULL, PMU_COMMAND_QUEUE_HPQ,
				pmu_handle_param_msg, pmu);
	}

	return 0;
}

int gp106_pmu_elpg_statistics(struct gk20a *g, u32 pg_engine_id,
		struct pmu_pg_stats_data *pg_stat_data)
{
	struct nvgpu_pmu *pmu = g->pmu;
	struct pmu_pg_stats_v2 stats;
	int err;

	err = nvgpu_falcon_copy_from_dmem(pmu->flcn,
		pmu->pg->stat_dmem_offset[pg_engine_id],
			(u8 *)&stats, (u32)sizeof(struct pmu_pg_stats_v2), 0);
	if (err != 0) {
		nvgpu_err(g, "PMU falcon DMEM copy failed");
		return err;
	}

	pg_stat_data->ingating_time = stats.total_sleep_time_us;
	pg_stat_data->ungating_time = stats.total_non_sleep_time_us;
	pg_stat_data->gating_cnt = stats.entry_count;
	pg_stat_data->avg_entry_latency_us = stats.entry_latency_avg_us;
	pg_stat_data->avg_exit_latency_us = stats.exit_latency_avg_us;

	return err;
}

u32 gp106_pmu_pg_engines_list(struct gk20a *g)
{
	return BIT32(PMU_PG_ELPG_ENGINE_ID_GRAPHICS) |
		BIT32(PMU_PG_ELPG_ENGINE_ID_MS);
}

u32 gp106_pmu_pg_feature_list(struct gk20a *g, u32 pg_engine_id)
{
	if (pg_engine_id == PMU_PG_ELPG_ENGINE_ID_GRAPHICS) {
		return NVGPU_PMU_GR_FEATURE_MASK_RPPG;
	}

	if (pg_engine_id == PMU_PG_ELPG_ENGINE_ID_MS) {
		return NVGPU_PMU_MS_FEATURE_MASK_ALL;
	}

	return 0;
}

bool gp106_pmu_is_lpwr_feature_supported(struct gk20a *g, u32 feature_id)
{
	return false;
}
