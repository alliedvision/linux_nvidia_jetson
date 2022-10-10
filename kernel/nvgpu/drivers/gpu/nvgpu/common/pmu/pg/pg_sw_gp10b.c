/*
 * Copyright (c) 2017-2022, NVIDIA CORPORATION.  All rights reserved.
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
#include <nvgpu/log.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/bug.h>
#include <nvgpu/string.h>
#include <nvgpu/pmu/cmd.h>
#include <nvgpu/pmu/pmu_pg.h>

#include "pg_sw_gp10b.h"
#include "pg_sw_gm20b.h"

static void pmu_handle_gr_param_msg(struct gk20a *g, struct pmu_msg *msg,
				void *param, u32 status)
{
	(void)param;

	nvgpu_log_fn(g, " ");

	if (status != 0U) {
		nvgpu_err(g, "GR PARAM cmd aborted");
		return;
	}

	nvgpu_pmu_dbg(g, "GR PARAM is acknowledged from PMU %x",
		msg->msg.pg.msg_type);

	return;
}

int gp10b_pg_gr_init(struct gk20a *g, u32 pg_engine_id)
{
	struct nvgpu_pmu *pmu = g->pmu;
	struct pmu_cmd cmd;
	size_t tmp_size;

	if (pg_engine_id == PMU_PG_ELPG_ENGINE_ID_GRAPHICS) {
		(void) memset(&cmd, 0, sizeof(struct pmu_cmd));
		cmd.hdr.unit_id = PMU_UNIT_PG;
		tmp_size = PMU_CMD_HDR_SIZE +
			sizeof(struct pmu_pg_cmd_gr_init_param_v2);
		nvgpu_assert(tmp_size <= (size_t)U8_MAX);
		cmd.hdr.size = (u8)tmp_size;
		cmd.cmd.pg.gr_init_param_v2.cmd_type =
			PMU_PG_CMD_ID_PG_PARAM;
		cmd.cmd.pg.gr_init_param_v2.sub_cmd_id =
			PMU_PG_PARAM_CMD_GR_INIT_PARAM;
		cmd.cmd.pg.gr_init_param_v2.featuremask =
			NVGPU_PMU_GR_FEATURE_MASK_POWER_GATING;
		cmd.cmd.pg.gr_init_param_v2.ldiv_slowdown_factor =
			g->ldiv_slowdown_factor;

		nvgpu_pmu_dbg(g, "cmd post PMU_PG_CMD_ID_PG_PARAM ");
		nvgpu_pmu_cmd_post(g, &cmd, NULL, PMU_COMMAND_QUEUE_HPQ,
			pmu_handle_gr_param_msg, pmu);

	} else {
		return -EINVAL;
	}

	return 0;
}

int gp10b_pmu_elpg_statistics(struct gk20a *g, u32 pg_engine_id,
			struct pmu_pg_stats_data *pg_stat_data)
{
	struct nvgpu_pmu *pmu = g->pmu;
	struct pmu_pg_stats_v1 stats;
	int err;

	err = nvgpu_falcon_copy_from_dmem(pmu->flcn,
		pmu->pg->stat_dmem_offset[pg_engine_id],
			(u8 *)&stats, (u32)sizeof(struct pmu_pg_stats_v1), 0);
	if (err != 0) {
		nvgpu_err(g, "PMU falcon DMEM copy failed");
		return err;
	}

	pg_stat_data->ingating_time = stats.total_sleep_timeus;
	pg_stat_data->ungating_time = stats.total_nonsleep_timeus;
	pg_stat_data->gating_cnt = stats.entry_count;
	pg_stat_data->avg_entry_latency_us = stats.entrylatency_avgus;
	pg_stat_data->avg_exit_latency_us = stats.exitlatency_avgus;

	return err;
}

void nvgpu_gp10b_pg_sw_init(struct gk20a *g,
		struct nvgpu_pmu_pg *pg)
{
	(void)g;

	pg->elpg_statistics = gp10b_pmu_elpg_statistics;
	pg->init_param = gp10b_pg_gr_init;
	pg->supported_engines_list = gm20b_pmu_pg_engines_list;
	pg->engines_feature_list = gm20b_pmu_pg_feature_list;
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
