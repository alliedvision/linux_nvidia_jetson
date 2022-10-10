/*
 * Copyright (c) 2020-2022, NVIDIA CORPORATION.  All rights reserved.
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

#include <nvgpu/gk20a.h>
#include <nvgpu/string.h>
#include <nvgpu/pmu.h>
#include <nvgpu/pmu/pmu_pg.h>
#include <nvgpu/pmu/pmuif/pg.h>
#include <nvgpu/pmu/cmd.h>

#include "common/pmu/pg/pmu_pg.h"
#include "common/pmu/pg/pg_sw_gp106.h"
#include "common/pmu/pg/pg_sw_gm20b.h"
#include "common/pmu/pg/pg_sw_gv11b.h"
#include "pg_sw_ga10b.h"

u32 ga10b_pmu_pg_engines_list(struct gk20a *g)
{
	return nvgpu_is_enabled(g, NVGPU_ELPG_MS_ENABLED) ?
		(BIT32(PMU_PG_ELPG_ENGINE_ID_GRAPHICS) |
			BIT32(PMU_PG_ELPG_ENGINE_ID_MS_LTC)) :
		(BIT32(PMU_PG_ELPG_ENGINE_ID_GRAPHICS));
}

static int ga10b_pmu_pg_pre_init(struct gk20a *g, struct nvgpu_pmu *pmu)
{
	struct pmu_rpc_struct_lpwr_loading_pre_init rpc;
	int status;
	u32 idx;

	nvgpu_log_fn(g, " ");

	(void) memset(&rpc, 0,
		sizeof(struct pmu_rpc_struct_lpwr_loading_pre_init));

	rpc.arch_sf_support_mask = NV_PMU_ARCH_FEATURE_SUPPORT_MASK;
	rpc.base_period_ms = NV_PMU_BASE_SAMPLING_PERIOD_MS;
	rpc.b_no_pstate_vbios = true;

	/* Initialize LPWR GR and MS grp data for GRAPHICS and MS_LTC engine */
	for (idx = 0; idx < NV_PMU_LPWR_GRP_CTRL_ID__COUNT; idx++)
	{
		if (idx == NV_PMU_LPWR_GRP_CTRL_ID_GR) {
			rpc.grp_ctrl_mask[idx] =
				BIT(PMU_PG_ELPG_ENGINE_ID_GRAPHICS);
		}

		if (nvgpu_is_enabled(g, NVGPU_ELPG_MS_ENABLED)) {
			if (idx == NV_PMU_LPWR_GRP_CTRL_ID_MS) {
				rpc.grp_ctrl_mask[idx] =
					BIT(PMU_PG_ELPG_ENGINE_ID_MS_LTC);
			}
		}
	}

	PMU_RPC_EXECUTE_CPB(status, pmu, PG_LOADING, PRE_INIT, &rpc, 0);
	if (status != 0) {
		nvgpu_err(g, "Failed to execute RPC status=0x%x", status);
	}

	return status;
}

static int ga10b_pmu_pg_init(struct gk20a *g, struct nvgpu_pmu *pmu,
		u8 pg_engine_id)
{
	struct pmu_rpc_struct_lpwr_loading_pg_ctrl_init rpc;
	int status;

	nvgpu_log_fn(g, " ");

	/* init ELPG */
	(void) memset(&rpc, 0,
			sizeof(struct pmu_rpc_struct_lpwr_loading_pg_ctrl_init));
	rpc.ctrl_id = (u32)pg_engine_id;
	rpc.support_mask = NV_PMU_SUB_FEATURE_SUPPORT_MASK;

	PMU_RPC_EXECUTE_CPB(status, pmu, PG_LOADING, INIT, &rpc, 0);
	if (status != 0) {
		nvgpu_err(g, "Failed to execute RPC status=0x%x",
			status);
	}

	/* Update Stats Dmem offset for reading statistics info */
	pmu->pg->stat_dmem_offset[pg_engine_id] = rpc.stats_dmem_offset;

	return status;
}

static int ga10b_pmu_pg_allow(struct gk20a *g, struct nvgpu_pmu *pmu,
		u8 pg_engine_id)
{
	struct pmu_rpc_struct_lpwr_pg_ctrl_allow rpc;
	int status;

	nvgpu_log_fn(g, " ");

	(void) memset(&rpc, 0,
		sizeof(struct pmu_rpc_struct_lpwr_pg_ctrl_allow));
	rpc.ctrl_id = (u32)pg_engine_id;

	PMU_RPC_EXECUTE_CPB(status, pmu, PG, ALLOW, &rpc, 0);
	if (status != 0) {
		nvgpu_err(g, "Failed to execute RPC status=0x%x",
			status);
		return status;
	}

	return status;
}

static int ga10b_pmu_pg_disallow(struct gk20a *g, struct nvgpu_pmu *pmu,
		u8 pg_engine_id)
{
	struct pmu_rpc_struct_lpwr_pg_ctrl_disallow rpc;
	int status;

	nvgpu_log_fn(g, " ");

	(void) memset(&rpc, 0,
		sizeof(struct pmu_rpc_struct_lpwr_pg_ctrl_disallow));
	rpc.ctrl_id = (u32)pg_engine_id;

	PMU_RPC_EXECUTE_CPB(status, pmu, PG, DISALLOW, &rpc, 0);
	if (status != 0) {
		nvgpu_err(g, "Failed to execute RPC status=0x%x",
			status);
	}

	return status;
}

static int ga10b_pmu_pg_threshold_update(struct gk20a *g,
		struct nvgpu_pmu *pmu, u8 pg_engine_id)
{
	struct pmu_rpc_struct_lpwr_pg_ctrl_threshold_update rpc;
	int status;

	nvgpu_log_fn(g, " ");

	(void) memset(&rpc, 0,
		sizeof(struct pmu_rpc_struct_lpwr_pg_ctrl_threshold_update));
	rpc.ctrl_id = (u32)pg_engine_id;

#ifdef CONFIG_NVGPU_SIM
	if (nvgpu_is_enabled(g, NVGPU_IS_FMODEL)) {
		rpc.threshold_cycles.idle = PMU_PG_IDLE_THRESHOLD_SIM;
		rpc.threshold_cycles.ppu = PMU_PG_POST_POWERUP_IDLE_THRESHOLD_SIM;
	} else
#endif
	{
		rpc.threshold_cycles.idle = PMU_PG_IDLE_THRESHOLD;
		rpc.threshold_cycles.ppu = PMU_PG_POST_POWERUP_IDLE_THRESHOLD;
	}

	PMU_RPC_EXECUTE_CPB(status, pmu, PG, THRESHOLD_UPDATE, &rpc, 0);
	if (status != 0) {
		nvgpu_err(g, "Failed to execute RPC status=0x%x",
			status);
	}

	return status;
}

static int ga10b_pmu_pg_sfm_update(struct gk20a *g,
		struct nvgpu_pmu *pmu, u8 pg_engine_id)
{
	struct pmu_rpc_struct_lpwr_pg_ctrl_sfm_update rpc;
	int status;

	nvgpu_log_fn(g, " ");

	(void) memset(&rpc, 0,
		sizeof(struct pmu_rpc_struct_lpwr_pg_ctrl_sfm_update));
	rpc.ctrl_id = (u32)pg_engine_id;
	rpc.enabled_mask = NV_PMU_SUB_FEATURE_SUPPORT_MASK;

	PMU_RPC_EXECUTE_CPB(status, pmu, PG, SFM_UPDATE, &rpc, 0);
	if (status != 0) {
		nvgpu_err(g, "Failed to execute RPC status=0x%x",
			status);
	}

	return status;
}

static int ga10b_pmu_pg_post_init(struct gk20a *g,struct nvgpu_pmu *pmu)
{
	struct pmu_rpc_struct_lpwr_loading_post_init rpc;
	int status;

	nvgpu_log_fn(g, " ");

	(void) memset(&rpc, 0,
			sizeof(struct pmu_rpc_struct_lpwr_loading_post_init));

	PMU_RPC_EXECUTE_CPB(status, pmu, PG_LOADING, POST_INIT, &rpc, 0);
	if (status != 0) {
		nvgpu_err(g, "Failed to execute RPC status=0x%x", status);
	}

	return status;
}

static int ga10b_pmu_pg_init_send(struct gk20a *g,
		struct nvgpu_pmu *pmu, u8 pg_engine_id)
{
	int status;

	nvgpu_log_fn(g, " ");

	status = ga10b_pmu_pg_pre_init(g, pmu);
	if (status != 0) {
		nvgpu_err(g, "Failed to execute PG_PRE_INIT RPC");
		return status;
	}

	status = ga10b_pmu_pg_init(g, pmu, pg_engine_id);
	if (status != 0) {
		nvgpu_err(g, "Failed to execute PG_INIT RPC");
		return status;
	}

	status = ga10b_pmu_pg_threshold_update(g, pmu, pg_engine_id);
	if (status != 0) {
		nvgpu_err(g, "Failed to execute PG_THRESHOLD_UPDATE RPC");
		return status;
	}

	status = ga10b_pmu_pg_sfm_update(g, pmu, pg_engine_id);
	if (status != 0) {
		nvgpu_err(g, "Failed to execute PG_SFM_UPDATE RPC");
		return status;
	}

	status = ga10b_pmu_pg_post_init(g, pmu);
	if (status != 0) {
		nvgpu_err(g, "Failed to execute PG_POST_INIT RPC");
		return status;
	}

	return status;
}

static int ga10b_pmu_pg_load_buff(struct gk20a *g, struct nvgpu_pmu *pmu)
{
	struct pmu_rpc_struct_lpwr_loading_pg_ctrl_buf_load rpc;
	u32 gr_engine_id;
	int status;

	nvgpu_log_fn(g, " ");

	gr_engine_id = nvgpu_engine_get_gr_id(g);

	(void) memset(&rpc, 0,
		sizeof(struct pmu_rpc_struct_lpwr_loading_pg_ctrl_buf_load));
	rpc.ctrl_id = nvgpu_safe_cast_u32_to_u8(gr_engine_id);
	rpc.buf_idx = PMU_PGENG_GR_BUFFER_IDX_FECS;
	rpc.dma_desc.params = (pmu->pg->pg_buf.size & 0xFFFFFFU);
	rpc.dma_desc.params |= (U32(PMU_DMAIDX_VIRT) << U32(24));
	rpc.dma_desc.address.lo = u64_lo32(pmu->pg->pg_buf.gpu_va);
	rpc.dma_desc.address.hi = u64_hi32(pmu->pg->pg_buf.gpu_va);

	pmu->pg->buf_loaded = false;

	PMU_RPC_EXECUTE_CPB(status, pmu, PG_LOADING, BUF_LOAD, &rpc, 0);
	if (status != 0) {
		nvgpu_err(g, "Failed to execute RPC status=0x%x",
			status);
		return status;
	}

	return status;
}

static void ga10b_pg_rpc_handler(struct gk20a *g, struct nvgpu_pmu *pmu,
		struct nv_pmu_rpc_header *rpc, struct rpc_handler_payload *rpc_payload)
{
	struct pmu_rpc_struct_lpwr_pg_ctrl_allow *rpc_allow;
	struct pmu_rpc_struct_lpwr_pg_ctrl_disallow *rpc_disallow;

	nvgpu_log_fn(g, " ");
	switch (rpc->function) {
	case NV_PMU_RPC_ID_PG_LOADING_PRE_INIT:
		nvgpu_pmu_dbg(g, "Reply to PG_PRE_INIT");
		break;
	case NV_PMU_RPC_ID_PG_LOADING_POST_INIT:
		nvgpu_pmu_dbg(g, "Reply to PG_POST_INIT");
		break;
	case NV_PMU_RPC_ID_PG_LOADING_INIT:
		nvgpu_pmu_dbg(g, "Reply to PG_INIT");
		break;
	case NV_PMU_RPC_ID_PG_THRESHOLD_UPDATE:
		nvgpu_pmu_dbg(g, "Reply to PG_THRESHOLD_UPDATE");
		break;
	case NV_PMU_RPC_ID_PG_SFM_UPDATE:
		nvgpu_pmu_dbg(g, "Reply to PG_SFM_UPDATE");
		nvgpu_pmu_fw_state_change(g, pmu, PMU_FW_STATE_ELPG_BOOTED, true);
		break;
	case NV_PMU_RPC_ID_PG_LOADING_BUF_LOAD:
		nvgpu_pmu_dbg(g, "Reply to PG_LOADING_BUF_LOAD");
		pmu->pg->buf_loaded = true;
		nvgpu_pmu_fw_state_change(g, pmu, PMU_FW_STATE_LOADING_ZBC, true);
		break;
	case NV_PMU_RPC_ID_PG_ALLOW:
		nvgpu_pmu_dbg(g, "Reply to PG_ALLOW");
		rpc_allow = (struct pmu_rpc_struct_lpwr_pg_ctrl_allow *)rpc_payload->rpc_buff;
		if (rpc_allow->ctrl_id == PMU_PG_ELPG_ENGINE_ID_GRAPHICS) {
			pmu->pg->elpg_stat = PMU_ELPG_STAT_ON;
		} else if (rpc_allow->ctrl_id == PMU_PG_ELPG_ENGINE_ID_MS_LTC) {
			pmu->pg->elpg_ms_stat = PMU_ELPG_MS_STAT_ON;
		} else {
			nvgpu_err(g, "Invalid pg_engine_id");
		}
		break;
	case NV_PMU_RPC_ID_PG_DISALLOW:
		nvgpu_pmu_dbg(g, "Reply to PG_DISALLOW");
		rpc_disallow = (struct pmu_rpc_struct_lpwr_pg_ctrl_disallow *)(void *)rpc_payload->rpc_buff;
		if (rpc_disallow->ctrl_id == PMU_PG_ELPG_ENGINE_ID_GRAPHICS) {
			pmu->pg->elpg_stat = PMU_ELPG_STAT_OFF;
		} else if (rpc_disallow->ctrl_id == PMU_PG_ELPG_ENGINE_ID_MS_LTC) {
			pmu->pg->elpg_ms_stat = PMU_ELPG_MS_STAT_OFF;
		} else {
			nvgpu_err(g, "Invalid pg_engine_id");
		}
		break;
	case NV_PMU_RPC_ID_PG_PG_CTRL_STATS_GET:
		nvgpu_pmu_dbg(g, "Reply to PG_STATS_GET");
		break;
	default:
		nvgpu_err(g,
			"unsupported PG rpc function : 0x%x", rpc->function);
		break;
	}
}

static int ga10b_pmu_elpg_statistics(struct gk20a *g, u32 pg_engine_id,
		struct pmu_pg_stats_data *pg_stat_data)
{
	struct pmu_rpc_struct_lpwr_pg_ctrl_stats_get rpc;
	int status = 0;

	(void) memset(&rpc, 0,
			sizeof(struct pmu_rpc_struct_lpwr_pg_ctrl_stats_get));

	rpc.ctrl_id = (u8)pg_engine_id;
	PMU_RPC_EXECUTE_CPB(status, g->pmu, PG, PG_CTRL_STATS_GET, &rpc, 0);

	if (status != 0) {
		nvgpu_err(g, "Failed to execute RPC status=0x%x", status);
		return status;
	}

	pg_stat_data->ingating_time = rpc.stats.total_sleep_time_us;
	pg_stat_data->ungating_time = rpc.stats.total_non_sleep_time_us;
	pg_stat_data->gating_cnt = rpc.stats.entry_count;
	pg_stat_data->avg_entry_latency_us = rpc.stats.entry_latency_avg_us;
	pg_stat_data->avg_exit_latency_us = rpc.stats.exit_latency_avg_us;

	return status;
}

static int ga10b_pmu_pg_handle_async_cmd_resp(struct gk20a *g, u32 ctrl_id,
					u32 msg_id)
{
	int err = 0;

	switch (msg_id) {
	case PMU_PG_MSG_ASYNC_CMD_DISALLOW:
		if (ctrl_id == PMU_PG_ELPG_ENGINE_ID_GRAPHICS) {
			g->pmu->pg->disallow_state = PMU_ELPG_STAT_OFF;
		} else if (ctrl_id == PMU_PG_ELPG_ENGINE_ID_MS_LTC) {
			/* To-do for MS_LTC */
		} else {
			nvgpu_err(g, "Invalid engine id");
			err = -EINVAL;
		}
		break;
	default:
		nvgpu_err(g, "Invalid message id: %d", msg_id);
		err = -EINVAL;
		break;
	}
	return err;
}

static int ga10b_pmu_pg_handle_idle_snap_rpc(struct gk20a *g,
		struct pmu_nv_rpc_struct_lpwr_pg_idle_snap *idle_snap_rpc)
{
	int err = 0;

	nvgpu_err(g, "IDLE SNAP RPC received");
	nvgpu_err(g, "IDLE SNAP ctrl_id:%d", idle_snap_rpc->ctrl_id);
	nvgpu_err(g, "IDLE SNAP reason:0x%x", idle_snap_rpc->reason);

	switch (idle_snap_rpc->reason) {
	case PG_IDLE_SNAP_REASON_ERR_IDLE_FLIP_POWERING_DOWN:
		nvgpu_err(g, "IDLE_SNAP reason:ERR_IDLE_FLIP_POWERING_DOWN");
		break;
	case PG_IDLE_SNAP_REASON_ERR_IDLE_FLIP_PWR_OFF:
		nvgpu_err(g, "IDLE_SNAP reason:ERR_IDLE_PWR_OFF");
		break;
	default:
		nvgpu_err(g, "IDLE_SNAP reason unknown");
		err = -EINVAL;
		break;
	}

	nvgpu_err(g, "IDLE SNAP idle_status: 0x%x",
				idle_snap_rpc->idle_status);
	nvgpu_err(g, "IDLE SNAP idle_status1: 0x%x",
				idle_snap_rpc->idle_status1);
	nvgpu_err(g, "IDLE SNAP idle_status2: 0x%x",
				idle_snap_rpc->idle_status2);
	return err;
}

static int ga10b_pmu_pg_process_pg_event(struct gk20a *g, void *pmumsg)
{
	int err = 0;
	struct pmu_nv_rpc_struct_lpwr_pg_async_cmd_resp *async_cmd;
	struct pmu_nv_rpc_struct_lpwr_pg_idle_snap *idle_snap_rpc;
	struct pmu_nvgpu_rpc_pg_event *msg =
		(struct pmu_nvgpu_rpc_pg_event *)pmumsg;

	switch (msg->rpc_hdr.function) {
	case PMU_NV_RPC_ID_LPWR_PG_ASYNC_CMD_RESP:
		async_cmd =
		(struct pmu_nv_rpc_struct_lpwr_pg_async_cmd_resp *)
			(void *)(&msg->rpc_hdr);
		err = ga10b_pmu_pg_handle_async_cmd_resp(g, async_cmd->ctrl_id,
					async_cmd->msg_id);
		break;
	case PMU_NV_RPC_ID_LPWR_PG_IDLE_SNAP:
		idle_snap_rpc =
		(struct pmu_nv_rpc_struct_lpwr_pg_idle_snap *)
			(void *)(&msg->rpc_hdr);
		err = ga10b_pmu_pg_handle_idle_snap_rpc(g, idle_snap_rpc);
		break;

	default:
		nvgpu_err(g, "Invalid PMU RPC: 0x%x", msg->rpc_hdr.function);
		err = -EINVAL;
		break;
	}
	return err;
}

void nvgpu_ga10b_pg_sw_init(struct gk20a *g,
		struct nvgpu_pmu_pg *pg)
{
	nvgpu_log_fn(g, " ");

	pg->elpg_statistics = ga10b_pmu_elpg_statistics;
	pg->init_param = NULL;
	pg->supported_engines_list = ga10b_pmu_pg_engines_list;
	pg->engines_feature_list = NULL;
	pg->set_sub_feature_mask = NULL;
	pg->save_zbc = gm20b_pmu_save_zbc;
	pg->allow = ga10b_pmu_pg_allow;
	pg->disallow = ga10b_pmu_pg_disallow;
	pg->init = ga10b_pmu_pg_init;
	pg->alloc_dmem = NULL;
	pg->load_buff = ga10b_pmu_pg_load_buff;
	pg->hw_load_zbc = NULL;
	pg->rpc_handler = ga10b_pg_rpc_handler;
	pg->init_send = ga10b_pmu_pg_init_send;
	pg->process_pg_event = ga10b_pmu_pg_process_pg_event;
}
