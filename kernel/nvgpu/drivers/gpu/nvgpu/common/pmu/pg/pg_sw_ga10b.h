/*
 * Copyright (c) 2020-2021, NVIDIA CORPORATION.  All rights reserved.
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
#ifndef NVGPU_PMU_PG_SW_GA10B_H
#define NVGPU_PMU_PG_SW_GA10B_H

#include <nvgpu/types.h>
#include <nvgpu/pmu/pmuif/pg.h>

struct gk20a;

#define NV_PMU_SUB_FEATURE_SUPPORT_MASK		0xf84
#define NV_PMU_ARCH_FEATURE_SUPPORT_MASK	0x1B3
#define NV_PMU_BASE_SAMPLING_PERIOD_MS		0xFFFF

/*
* Brief Identifier for each Lpwr Group Ctrl ids
*/
enum
{
	NV_PMU_LPWR_GRP_CTRL_ID_GR = 0,
	NV_PMU_LPWR_GRP_CTRL_ID_MS,
	NV_PMU_LPWR_GRP_CTRL_ID_EI,
	NV_PMU_LPWR_GRP_CTRL_ID__COUNT,
};

/*
* Defines the structure that holds data used to execute PRE_INIT RPC.
*/
struct pmu_rpc_struct_lpwr_loading_pre_init
{
	/*
	 * [IN/OUT] Must be first field in RPC structure.
	 */
	struct nv_pmu_rpc_header hdr;
	/*
	 * [IN] Lpwr group data
	 */
	u32 grp_ctrl_mask[NV_PMU_LPWR_GRP_CTRL_ID__COUNT];
	/*
	 * [IN] Mask of NV_PMU_SUBFEATURE_ID_ARCH_xyz
	 */
	u32 arch_sf_support_mask;
	/*
	 * [IN] Base sampling period for centralised LPWR callback
	 */
	u16 base_period_ms;
	/*
	 * [IN] Indicates if it is a no pstate vbios
	 */
	bool b_no_pstate_vbios;
	/*
	 * [NONE] Must be last field in RPC structure.
	 */
	u32 scratch[1];
};

/*
* Defines the structure that holds data used to execute POST_INIT RPC.
*/
struct pmu_rpc_struct_lpwr_loading_post_init
{
	/*
	 * [IN/OUT] Must be first field in RPC structure
	 */
	struct nv_pmu_rpc_header hdr;
	/*
	 * Voltage rail data in LPWR
	 */
	struct pmu_pg_volt_rail pg_volt_rail[PG_VOLT_RAIL_IDX_MAX];
	/*
	 * [IN] Dummy array to match with pmu struct
	 */
	bool dummy;
	/*
	 * Must be last field in RPC structure.
	 */
	u32 scratch[1];
 };

struct pmu_rpc_struct_lpwr_loading_pg_ctrl_init
{
	/*[IN/OUT] Must be first field in RPC structure */
	struct nv_pmu_rpc_header hdr;
	/*
	* [OUT] stats dmem offset
	*/
	u32 stats_dmem_offset;
	/*
	* [OUT] Engines hold off Mask
	*/
	u32 eng_hold_off_Mask;
	/*
	* [OUT] HW FSM index
	*/
	u8 hw_eng_idx;
	/*
	* [OUT] Indicates if wakeup reason type is cumulative or normal
	*/
	bool b_cumulative_wakeup_mask;
	/*
	* [IN/OUT] Sub-feature support mask
	*/
	u32 support_mask;
	/*
	* [IN] Controller ID - NV_PMU_PG_ELPG_ENGINE_ID_xyz
	*/
	u32 ctrl_id;
	/*
	* [IN] Dummy array to match with pmu struct
	*/
	u8 dummy[8];
	/*
	* [NONE] Must be last field in RPC structure.
	*/
	u32 scratch[1];
};

/*
* Defines the structure that holds data used to execute PG_CTRL_ALLOW RPC.
*/
struct pmu_rpc_struct_lpwr_pg_ctrl_allow
{
	/*
	 * [IN/OUT] Must be first field in RPC structure.
	 */
	struct nv_pmu_rpc_header hdr;
	/*
	 * [IN] Controller ID - NV_PMU_PG_ELPG_ENGINE_ID_xyz
	 */
	u32 ctrl_id;
	/*
	 * [NONE] Must be last field in RPC structure.
	 */
	u32 scratch[1];
};

/*
* Defines the structure that holds data used to execute PG_CTRL_DISALLOW RPC.
*/
struct pmu_rpc_struct_lpwr_pg_ctrl_disallow
{
	/*
	 * [IN/OUT] Must be first field in RPC structure.
	 */
	struct nv_pmu_rpc_header hdr;
	/*
	 * [IN] Controller ID - NV_PMU_PG_ELPG_ENGINE_ID_xyz
	 */
	u32 ctrl_id;
	/*
	 * [NONE] Must be last field in RPC structure.
	 */
	u32 scratch[1];
};

/*
* Brief Structure defining PG Ctrl thresholds
*/
struct pg_ctrl_threshold
{
	/*
	 *Idle threshold. HW FSM raises entry interrupt after expiration
	 *of idle threshold.
	 */
	u32 idle;
	/*
	 * Post power up threshold. This helps to avoid immediate entry
	 * after exit. PPU threshold is used for HOST based wake-up.
	 */
	u32 ppu;
	/* Minimum value of Idle threshold supported */
	u32 min_idle;
	/* Maximum value of Idle threshold supported */
	u32 max_idle;
};

/*
* Defines the structure that holds data used to execute PG_CTRL_THRESHOLD_UPDATE RPC.
*/
struct pmu_rpc_struct_lpwr_pg_ctrl_threshold_update
{
	/*
	 * [IN/OUT] Must be first field in RPC structure.
	 */
	struct nv_pmu_rpc_header hdr;
	/*
	 * [IN] Controller ID - NV_PMU_PG_ELPG_ENGINE_ID_xyz
	 */
	u32 ctrl_id;
	/*
	 * [IN] PgCtrl thresholds
	 */
	struct pg_ctrl_threshold threshold_cycles;
	/*
	 * [NONE] Must be last field in RPC structure.
	 */
	u32 scratch[1];
};

/*
* Defines the structure that holds data used to execute PG_CTRL_SFM_UPDATE RPC.
*/
struct pmu_rpc_struct_lpwr_pg_ctrl_sfm_update
{
	/*
	 * [IN/OUT] Must be first field in RPC structure.
	 */
	struct nv_pmu_rpc_header hdr;
	/*
	 * [IN] Updated enabled mask - NV_PMU_PG_ELPG_ENGINE_ID_xyz
	*/
	u32 enabled_mask;
	/*
	 * [IN] Controller ID - NV_PMU_PG_ELPG_ENGINE_ID_xyz
	 */
	u32 ctrl_id;
	/*
	 * [NONE] Must be last field in RPC structure.
	 */
	u32 scratch[1];
};

/*
* Defines the structure that holds data used to execute PG_CTRL_BUF_LOAD RPC.
*/
struct pmu_rpc_struct_lpwr_loading_pg_ctrl_buf_load
{
	/*
	 * [IN/OUT] Must be first field in RPC structure.
	 */
	struct nv_pmu_rpc_header hdr;
	/*
	 * [IN] DMA buffer descriptor
	 */
	struct flcn_mem_desc_v0 dma_desc;
	/*
	 * [IN] PgCtrl ID
	 */
	u8 ctrl_id;
	/*
	 * [IN] Engine Buffer Index
	 */
	u8 buf_idx;
	/*
	 * [NONE] Must be last field in RPC structure.
	 */
	u32 scratch[1];
};

/*!
 * Defines the structure that holds data used to execute PG_ASYNC_CMD_RESP RPC.
 */
struct pmu_nv_rpc_struct_lpwr_pg_async_cmd_resp {
	/*!
	 *  Must be first field in RPC structure.
	 */
	struct nv_pmu_rpc_header hdr;
	/*!
	 *  Control ID of the Async PG Command.
	 */
	u8 ctrl_id;
	/*!
	 *  Message ID of the Async PG Command.
	 */
	u8 msg_id;
};

/*!
 * Defines the structure that holds data used to execute PG_IDLE_SNAP RPC.
 */
struct pmu_nv_rpc_struct_lpwr_pg_idle_snap {
	/*!
	 *  Must be first field in RPC structure.
	 */
	struct nv_pmu_rpc_header hdr;
	/*!
	 *  PgCtrl ID.
	 */
	u8 ctrl_id;
	/*!
	 *  Idle Snap reason.
	 */
	u8 reason;
	/*!
	 *  Primary status from Idle Snap.
	 */
	u32 idle_status;
	/*!
	 *  Additional status from Idle Snap.
	 */
	u32 idle_status1;
	/*!
	 *  Additional status from Idle Snap.
	 */
	u32 idle_status2;
};

/*
* Brief Statistics structure for PG features
*/
struct pmu_pg_stats_v3
{
	/* Number of time PMU successfully engaged sleep state */
	u32 entry_count;
	/* Number of time PMU exit sleep state */
	u32 exit_count;
	/* Number of time PMU aborted in entry sequence */
	u32 abort_count;
	/* Number of time task thrashing/starvation detected by Task MGMT feature */
	u32 detection_count;
	/*
	* Time for which GPU was neither in Sleep state not
	* executing sleep sequence.
	*/
	u32 powered_up_time_us;
	/* Entry and exit latency of current sleep cycle */
	u32 entry_latency_us;
	u32 exit_latency_us;
	/* Resident time for current sleep cycle. */
	u32 resident_time_us;
	/* Rolling average entry and exit latencies */
	u32 entry_latency_avg_us;
	u32 exit_latency_avg_us;
	/* Max entry and exit latencies */
	u32 entry_latency_max_us;
	u32 exit_latency_max_us;
	/* Total time spent in sleep and non-sleep state */
	u32 total_sleep_time_us;
	u32 total_non_sleep_time_us;
	/* Wakeup Type - Saves events that caused a power-up. */
	u32 wake_up_events;
	/* Abort Reason - Saves reason that caused an abort */
	u32 abort_reason;
	u32 sw_disallow_reason_mask;
	u32 hw_disallow_reason_mask;
};

/*
 * Defines the structure that holds data used to execute PG_CTRL_STATS_GET RPC.
 */
struct pmu_rpc_struct_lpwr_pg_ctrl_stats_get {
	/*!
	 * Must be first field in RPC structure.
	 */
	struct nv_pmu_rpc_header hdr;
	/*!
	 * PgCtrl statistics
	 */
	struct pmu_pg_stats_v3 stats;
	/*!
	 * Control ID
	 */
	u8 ctrl_id;
	/*!
	 * Must be last field in RPC structure.
	 * Used as variable size scrach space on
	 * RM managed DMEM heap for this RPC.
	 */
	u32 scratch[1];
};


void nvgpu_ga10b_pg_sw_init(struct gk20a *g, struct nvgpu_pmu_pg *pg);
u32 ga10b_pmu_pg_engines_list(struct gk20a *g);

#endif /* NVGPU_PMU_PG_SW_GA10B_H */
