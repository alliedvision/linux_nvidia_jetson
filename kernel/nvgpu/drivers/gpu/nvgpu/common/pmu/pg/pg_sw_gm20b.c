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
#include <nvgpu/gk20a.h>
#include <nvgpu/pmu/lpwr.h>
#include <nvgpu/pmu/cmd.h>
#include <nvgpu/bug.h>
#include <nvgpu/pmu/pmu_pg.h>
#include <nvgpu/engines.h>
#include <nvgpu/string.h>

#include "pg_sw_gm20b.h"
#include "pmu_pg.h"

u32 gm20b_pmu_pg_engines_list(struct gk20a *g)
{
	(void)g;
	return BIT32(PMU_PG_ELPG_ENGINE_ID_GRAPHICS);
}

u32 gm20b_pmu_pg_feature_list(struct gk20a *g, u32 pg_engine_id)
{
	(void)g;

	if (pg_engine_id == PMU_PG_ELPG_ENGINE_ID_GRAPHICS) {
		return NVGPU_PMU_GR_FEATURE_MASK_POWER_GATING;
	}

	return 0;
}

static void pmu_handle_zbc_msg(struct gk20a *g, struct pmu_msg *msg,
	void *param, u32 status)
{
	struct nvgpu_pmu *pmu = param;
	(void)msg;
	(void)status;
	nvgpu_pmu_dbg(g, "reply ZBC_TABLE_UPDATE");
	pmu->pg->zbc_save_done = true;
}

void gm20b_pmu_save_zbc(struct gk20a *g, u32 entries)
{
	struct nvgpu_pmu *pmu = g->pmu;
	struct pmu_cmd cmd;
	size_t tmp_size;
	int err = 0;

	if (!nvgpu_pmu_get_fw_ready(g, pmu) ||
		(entries == 0U) || !pmu->pg->zbc_ready) {
		return;
	}

	(void) memset(&cmd, 0, sizeof(struct pmu_cmd));
	cmd.hdr.unit_id = PMU_UNIT_PG;
	tmp_size = PMU_CMD_HDR_SIZE + sizeof(struct pmu_zbc_cmd);
	nvgpu_assert(tmp_size <= (size_t)U8_MAX);
	cmd.hdr.size = (u8)tmp_size;
	cmd.cmd.zbc.cmd_type = g->pmu_ver_cmd_id_zbc_table_update;
	cmd.cmd.zbc.entry_mask = ZBC_MASK(entries);

	pmu->pg->zbc_save_done = false;

	nvgpu_pmu_dbg(g, "cmd post ZBC_TABLE_UPDATE");
	err = nvgpu_pmu_cmd_post(g, &cmd, NULL, PMU_COMMAND_QUEUE_HPQ,
			pmu_handle_zbc_msg, pmu);
	if (err != 0) {
		nvgpu_err(g, "ZBC_TABLE_UPDATE cmd post failed");
		return;
	}
	pmu_wait_message_cond(pmu, nvgpu_get_poll_timeout(g),
			&pmu->pg->zbc_save_done, 1);
	if (!pmu->pg->zbc_save_done) {
		nvgpu_err(g, "ZBC save timeout");
	}
}

int gm20b_pmu_elpg_statistics(struct gk20a *g, u32 pg_engine_id,
		struct pmu_pg_stats_data *pg_stat_data)
{
	struct nvgpu_pmu *pmu = g->pmu;
	struct pmu_pg_stats stats;
	int err;

	err = nvgpu_falcon_copy_from_dmem(pmu->flcn,
		pmu->pg->stat_dmem_offset[pg_engine_id],
		(u8 *)&stats, (u32)sizeof(struct pmu_pg_stats), 0);
	if (err != 0) {
		nvgpu_err(g, "PMU falcon DMEM copy failed");
		return err;
	}

	pg_stat_data->ingating_time = stats.pg_ingating_time_us;
	pg_stat_data->ungating_time = stats.pg_ungating_time_us;
	pg_stat_data->gating_cnt = stats.pg_gating_cnt;
	pg_stat_data->avg_entry_latency_us = stats.pg_avg_entry_time_us;
	pg_stat_data->avg_exit_latency_us = stats.pg_avg_exit_time_us;

	return err;
}

int gm20b_pmu_pg_elpg_init(struct gk20a *g, struct nvgpu_pmu *pmu,
		u8 pg_engine_id)
{
	struct pmu_cmd cmd;
	u64 tmp;

	/* init ELPG */
	(void) memset(&cmd, 0, sizeof(struct pmu_cmd));
	cmd.hdr.unit_id = PMU_UNIT_PG;
	tmp = nvgpu_safe_add_u64(PMU_CMD_HDR_SIZE,
			sizeof(struct pmu_pg_cmd_elpg_cmd));
	nvgpu_assert(tmp <= U8_MAX);
	cmd.hdr.size = nvgpu_safe_cast_u64_to_u8(tmp);
	cmd.cmd.pg.elpg_cmd.cmd_type = PMU_PG_CMD_ID_ELPG_CMD;
	cmd.cmd.pg.elpg_cmd.engine_id = pg_engine_id;
	cmd.cmd.pg.elpg_cmd.cmd = PMU_PG_ELPG_CMD_INIT;

	return nvgpu_pmu_cmd_post(g, &cmd, NULL, PMU_COMMAND_QUEUE_HPQ,
			pmu_handle_pg_elpg_msg, pmu);
}

int gm20b_pmu_pg_elpg_allow(struct gk20a *g, struct nvgpu_pmu *pmu,
		u8 pg_engine_id)
{
	struct pmu_cmd cmd;
	u64 tmp;

	(void) memset(&cmd, 0, sizeof(struct pmu_cmd));
	cmd.hdr.unit_id = PMU_UNIT_PG;
	tmp = nvgpu_safe_add_u64(PMU_CMD_HDR_SIZE,
			sizeof(struct pmu_pg_cmd_elpg_cmd));
	nvgpu_assert(tmp <= U8_MAX);
	cmd.hdr.size = nvgpu_safe_cast_u64_to_u8(tmp);
	cmd.cmd.pg.elpg_cmd.cmd_type = PMU_PG_CMD_ID_ELPG_CMD;
	cmd.cmd.pg.elpg_cmd.engine_id = pg_engine_id;
	cmd.cmd.pg.elpg_cmd.cmd = PMU_PG_ELPG_CMD_ALLOW;

	return nvgpu_pmu_cmd_post(g, &cmd, NULL,
			PMU_COMMAND_QUEUE_HPQ, pmu_handle_pg_elpg_msg,
			pmu);
}

int gm20b_pmu_pg_elpg_disallow(struct gk20a *g, struct nvgpu_pmu *pmu,
		u8 pg_engine_id)
{
	struct pmu_cmd cmd;
	u64 tmp;

	(void) memset(&cmd, 0, sizeof(struct pmu_cmd));
	cmd.hdr.unit_id = PMU_UNIT_PG;
	tmp = nvgpu_safe_add_u64(PMU_CMD_HDR_SIZE,
			sizeof(struct pmu_pg_cmd_elpg_cmd));
	nvgpu_assert(tmp <= U8_MAX);
	cmd.hdr.size = nvgpu_safe_cast_u64_to_u8(tmp);
	cmd.cmd.pg.elpg_cmd.cmd_type = PMU_PG_CMD_ID_ELPG_CMD;
	cmd.cmd.pg.elpg_cmd.engine_id = pg_engine_id;
	cmd.cmd.pg.elpg_cmd.cmd = PMU_PG_ELPG_CMD_DISALLOW;

	return nvgpu_pmu_cmd_post(g, &cmd, NULL, PMU_COMMAND_QUEUE_HPQ,
			pmu_handle_pg_elpg_msg, pmu);
}

int gm20b_pmu_pg_elpg_alloc_dmem(struct gk20a *g, struct nvgpu_pmu *pmu,
		u8 pg_engine_id)
{
	struct pmu_cmd cmd;
	u64 tmp;

	pmu->pg->stat_dmem_offset[pg_engine_id] = 0;
	(void) memset(&cmd, 0, sizeof(struct pmu_cmd));
	cmd.hdr.unit_id = PMU_UNIT_PG;
	tmp = nvgpu_safe_add_u64(PMU_CMD_HDR_SIZE,
			sizeof(struct pmu_pg_cmd_elpg_cmd));
	nvgpu_assert(tmp <= U8_MAX);
	cmd.hdr.size = nvgpu_safe_cast_u64_to_u8(tmp);
	cmd.cmd.pg.stat.cmd_type = PMU_PG_CMD_ID_PG_STAT;
	cmd.cmd.pg.stat.engine_id = pg_engine_id;
	cmd.cmd.pg.stat.sub_cmd_id = PMU_PG_STAT_CMD_ALLOC_DMEM;
	cmd.cmd.pg.stat.data = 0;

	return nvgpu_pmu_cmd_post(g, &cmd, NULL, PMU_COMMAND_QUEUE_LPQ,
		pmu_handle_pg_stat_msg, pmu);
}

int gm20b_pmu_pg_elpg_load_buff(struct gk20a *g, struct nvgpu_pmu *pmu)
{
	struct pmu_cmd cmd;
	u64 tmp;
	u32 gr_engine_id;

	gr_engine_id = nvgpu_engine_get_gr_id(g);

	(void) memset(&cmd, 0, sizeof(struct pmu_cmd));
	tmp = nvgpu_safe_add_u64(PMU_CMD_HDR_SIZE,
		pmu->fw->ops.pg_cmd_eng_buf_load_size(&cmd.cmd.pg));
	cmd.hdr.unit_id = PMU_UNIT_PG;
	nvgpu_assert(PMU_CMD_HDR_SIZE < U32(U8_MAX));
	cmd.hdr.size = nvgpu_safe_cast_u64_to_u8(tmp);
	pmu->fw->ops.pg_cmd_eng_buf_load_set_cmd_type(&cmd.cmd.pg,
		PMU_PG_CMD_ID_ENG_BUF_LOAD);
	pmu->fw->ops.pg_cmd_eng_buf_load_set_engine_id(&cmd.cmd.pg,
		nvgpu_safe_cast_u32_to_u8(gr_engine_id));
	pmu->fw->ops.pg_cmd_eng_buf_load_set_buf_idx(&cmd.cmd.pg,
		PMU_PGENG_GR_BUFFER_IDX_FECS);
	pmu->fw->ops.pg_cmd_eng_buf_load_set_buf_size(&cmd.cmd.pg,
		nvgpu_safe_cast_u64_to_u16(pmu->pg->pg_buf.size));
	pmu->fw->ops.pg_cmd_eng_buf_load_set_dma_base(&cmd.cmd.pg,
		u64_lo32(pmu->pg->pg_buf.gpu_va));
	pmu->fw->ops.pg_cmd_eng_buf_load_set_dma_offset(&cmd.cmd.pg,
		nvgpu_safe_cast_u64_to_u8(pmu->pg->pg_buf.gpu_va & 0xFFU));
	pmu->fw->ops.pg_cmd_eng_buf_load_set_dma_idx(&cmd.cmd.pg,
		PMU_DMAIDX_VIRT);

	pmu->pg->buf_loaded = false;

	return nvgpu_pmu_cmd_post(g, &cmd, NULL, PMU_COMMAND_QUEUE_LPQ,
			pmu_handle_pg_buf_config_msg, pmu);
}

int gm20b_pmu_pg_elpg_hw_load_zbc(struct gk20a *g, struct nvgpu_pmu *pmu)
{
	struct pmu_cmd cmd;
	u64 tmp;
	u32 gr_engine_id;

	gr_engine_id = nvgpu_engine_get_gr_id(g);

	(void) memset(&cmd, 0, sizeof(struct pmu_cmd));
	tmp = nvgpu_safe_add_u64(PMU_CMD_HDR_SIZE,
		pmu->fw->ops.pg_cmd_eng_buf_load_size(&cmd.cmd.pg));
	cmd.hdr.unit_id = PMU_UNIT_PG;
	nvgpu_assert(PMU_CMD_HDR_SIZE < U32(U8_MAX));
	cmd.hdr.size = nvgpu_safe_cast_u64_to_u8(tmp);
	pmu->fw->ops.pg_cmd_eng_buf_load_set_cmd_type(&cmd.cmd.pg,
		PMU_PG_CMD_ID_ENG_BUF_LOAD);
	pmu->fw->ops.pg_cmd_eng_buf_load_set_engine_id(&cmd.cmd.pg,
		nvgpu_safe_cast_u32_to_u8(gr_engine_id));
	pmu->fw->ops.pg_cmd_eng_buf_load_set_buf_idx(&cmd.cmd.pg,
		PMU_PGENG_GR_BUFFER_IDX_ZBC);
	pmu->fw->ops.pg_cmd_eng_buf_load_set_buf_size(&cmd.cmd.pg,
		nvgpu_safe_cast_u64_to_u16(pmu->pg->seq_buf.size));
	pmu->fw->ops.pg_cmd_eng_buf_load_set_dma_base(&cmd.cmd.pg,
		u64_lo32(pmu->pg->seq_buf.gpu_va));
	pmu->fw->ops.pg_cmd_eng_buf_load_set_dma_offset(&cmd.cmd.pg,
		nvgpu_safe_cast_u64_to_u8(pmu->pg->seq_buf.gpu_va & 0xFFU));
	pmu->fw->ops.pg_cmd_eng_buf_load_set_dma_idx(&cmd.cmd.pg,
		PMU_DMAIDX_VIRT);

	pmu->pg->buf_loaded = false;

	return nvgpu_pmu_cmd_post(g, &cmd, NULL, PMU_COMMAND_QUEUE_LPQ,
			pmu_handle_pg_buf_config_msg, pmu);
}

int gm20b_pmu_pg_init_send(struct gk20a *g, struct nvgpu_pmu *pmu,
		u8 pg_engine_id)
{
	int err = 0;

	nvgpu_log_fn(g, " ");

	g->ops.pmu.pmu_pg_idle_counter_config(g, pg_engine_id);

	if (pmu->pg->init_param != NULL) {
		err = pmu->pg->init_param(g, pg_engine_id);
		if (err != 0) {
			nvgpu_err(g, "init_param failed err=%d", err);
			return err;
		}
	}

	nvgpu_pmu_dbg(g, "cmd post PMU_PG_ELPG_CMD_INIT");
	if (pmu->pg->init == NULL) {
		nvgpu_err(g, "PG init function not assigned");
		return -EINVAL;
	}
	err = pmu->pg->init(g, pmu, pg_engine_id);
	if (err != 0) {
		nvgpu_err(g, "PMU_PG_ELPG_CMD_INIT cmd failed\n");
		return err;
	}

	/* alloc dmem for powergating state log */
	nvgpu_pmu_dbg(g, "cmd post PMU_PG_STAT_CMD_ALLOC_DMEM");
	if (pmu->pg->alloc_dmem == NULL) {
		nvgpu_err(g, "PG alloc dmem function not assigned");
		return -EINVAL;
	}
	err = pmu->pg->alloc_dmem(g, pmu, pg_engine_id);
	if (err != 0) {
		nvgpu_err(g, "PMU_PG_STAT_CMD_ALLOC_DMEM cmd failed\n");
		return err;
	}

	/* disallow ELPG initially
	 * PMU ucode requires a disallow cmd before allow cmd
	 * set for wait_event PMU_ELPG_STAT_OFF */
	if (pg_engine_id == PMU_PG_ELPG_ENGINE_ID_GRAPHICS) {
		pmu->pg->elpg_stat = PMU_ELPG_STAT_OFF;
	} else if (pg_engine_id == PMU_PG_ELPG_ENGINE_ID_MS) {
		pmu->pg->mscg_transition_state = PMU_ELPG_STAT_OFF;
	}

	nvgpu_pmu_dbg(g, "cmd post PMU_PG_ELPG_CMD_DISALLOW");
	if (pmu->pg->disallow == NULL) {
		nvgpu_err(g, "PG disallow function not assigned");
		return -EINVAL;
	}
	err = pmu->pg->disallow(g, pmu, pg_engine_id);
	if (err != 0) {
		nvgpu_err(g, "PMU_PG_ELPG_CMD_DISALLOW cmd failed\n");
		return err;
	}

	if (pmu->pg->set_sub_feature_mask != NULL) {
		err = pmu->pg->set_sub_feature_mask(g, pg_engine_id);
		if (err != 0) {
			nvgpu_err(g, "set_sub_feature_mask failed err=%d",
				err);
			return err;
		}
	}

	return err;
}

void nvgpu_gm20b_pg_sw_init(struct gk20a *g,
		struct nvgpu_pmu_pg *pg)
{
	(void)g;

	pg->elpg_statistics = gm20b_pmu_elpg_statistics;
	pg->init_param = NULL;
	pg->supported_engines_list = gm20b_pmu_pg_engines_list;
	pg->engines_feature_list = gm20b_pmu_pg_feature_list;
	pg->is_lpwr_feature_supported = NULL;
	pg->lpwr_enable_pg = NULL;
	pg->lpwr_disable_pg = NULL;
	pg->param_post_init = NULL;
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
