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

#ifndef NVGPU_PMU_PG_H
#define NVGPU_PMU_PG_H

#include <nvgpu/lock.h>
#include <nvgpu/cond.h>
#include <nvgpu/thread.h>
#include <nvgpu/nvgpu_common.h>
#include <nvgpu/flcnif_cmn.h>
#include <nvgpu/pmu/pmuif/nvgpu_cmdif.h>
#include <nvgpu/pmu/pmuif/pg.h>
#include <nvgpu/timers.h>
#include <nvgpu/nvgpu_mem.h>
#include <nvgpu/atomic.h>
#include <include/nvgpu/pmu.h>

struct nvgpu_pmu;
struct vm_gk20a;
struct pmu_pg_stats_data;
struct rpc_handler_payload;

/*PG defines used by nvpgu-pmu*/
#define PMU_PG_SEQ_BUF_SIZE		4096U

#define PMU_PG_IDLE_THRESHOLD_SIM		1000U
#define PMU_PG_POST_POWERUP_IDLE_THRESHOLD_SIM	4000000U
/* TBD: QT or else ? */
#define PMU_PG_IDLE_THRESHOLD			15000U
#define PMU_PG_POST_POWERUP_IDLE_THRESHOLD	1000000U

#define PMU_PG_LPWR_FEATURE_RPPG 0x0U
#define PMU_PG_LPWR_FEATURE_MSCG 0x1U

#define PMU_MSCG_DISABLED 0U
#define PMU_MSCG_ENABLED 1U

/* Default Sampling Period of AELPG */
#define APCTRL_SAMPLING_PERIOD_PG_DEFAULT_US                    (1000000U)

/* Default values of APCTRL parameters */
#define APCTRL_MINIMUM_IDLE_FILTER_DEFAULT_US                   (100U)
#define APCTRL_MINIMUM_TARGET_SAVING_DEFAULT_US                 (10000U)
#define APCTRL_POWER_BREAKEVEN_DEFAULT_US                       (2000U)
#define APCTRL_CYCLES_PER_SAMPLE_MAX_DEFAULT                    (200U)

/* State of golden image */
enum {
	GOLDEN_IMG_NOT_READY = 0,
	GOLDEN_IMG_SUSPEND,
	GOLDEN_IMG_READY,
};

struct nvgpu_pg_init {
	bool state_change;
	bool state_destroy;
	struct nvgpu_cond wq;
	struct nvgpu_thread state_task;
};

struct nvgpu_pmu_pg {
	u32 elpg_stat;
	u32 disallow_state;
	u32 elpg_ms_stat;
#define PMU_ELPG_ENABLE_ALLOW_DELAY_MSEC	1U /* msec */
	struct nvgpu_pg_init pg_init;
	struct nvgpu_mutex pg_mutex; /* protect pg-RPPG/MSCG enable/disable */
	struct nvgpu_mutex elpg_mutex; /* protect elpg enable/disable */
	struct nvgpu_mutex elpg_ms_mutex; /* protect elpg_ms enable/disable */
	/* disable -1, enable +1, <=0 elpg disabled, > 0 elpg enabled */
	int elpg_refcnt;
	int elpg_ms_refcnt;
	u32 aelpg_param[5];
	bool zbc_ready;
	bool zbc_save_done;
	bool buf_loaded;
	struct nvgpu_mem pg_buf;
	bool initialized;
	u32 stat_dmem_offset[PMU_PG_ELPG_ENGINE_ID_INVALID_ENGINE];
	struct nvgpu_mem seq_buf;
	nvgpu_atomic_t golden_image_initialized;
	u32 mscg_stat;
	u32 mscg_transition_state;
	int (*elpg_statistics)(struct gk20a *g, u32 pg_engine_id,
		struct pmu_pg_stats_data *pg_stat_data);
	int (*init_param)(struct gk20a *g, u32 pg_engine_id);
	int (*set_sub_feature_mask)(struct gk20a *g,
		u32 pg_engine_id);
	u32 (*supported_engines_list)(struct gk20a *g);
	u32 (*engines_feature_list)(struct gk20a *g,
		u32 pg_engine_id);
	bool (*is_lpwr_feature_supported)(struct gk20a *g,
		u32 feature_id);
	int (*lpwr_enable_pg)(struct gk20a *g, bool pstate_lock);
	int (*lpwr_disable_pg)(struct gk20a *g, bool pstate_lock);
	int (*param_post_init)(struct gk20a *g);
	void (*save_zbc)(struct gk20a *g, u32 entries);
	/* ELPG cmd post functions */
	int (*allow)(struct gk20a *g, struct nvgpu_pmu *pmu,
			u8 pg_engine_id);
	int (*disallow)(struct gk20a *g, struct nvgpu_pmu *pmu,
			u8 pg_engine_id);
	int (*init)(struct gk20a *g, struct nvgpu_pmu *pmu,
			u8 pg_engine_id);
	int (*alloc_dmem)(struct gk20a *g, struct nvgpu_pmu *pmu,
			u8 pg_engine_id);
	int (*load_buff)(struct gk20a *g, struct nvgpu_pmu *pmu);
	int (*hw_load_zbc)(struct gk20a *g, struct nvgpu_pmu *pmu);
	void (*rpc_handler)(struct gk20a *g, struct nvgpu_pmu *pmu,
			struct nv_pmu_rpc_header *rpc, struct rpc_handler_payload *rpc_payload);
	int (*init_send)(struct gk20a *g, struct nvgpu_pmu *pmu, u8 pg_engine_id);
	int (*process_pg_event)(struct gk20a *g, void *pmumsg);
};

/*PG defines used by nvpgu-pmu*/
struct pmu_pg_stats_data {
	u32 gating_cnt;
	u32 ingating_time;
	u32 ungating_time;
	u32 avg_entry_latency_us;
	u32 avg_exit_latency_us;
};

/* PG init*/
int nvgpu_pmu_pg_init(struct gk20a *g, struct nvgpu_pmu *pmu,
	struct nvgpu_pmu_pg **pg);
void nvgpu_pmu_pg_deinit(struct gk20a *g, struct nvgpu_pmu *pmu,
	struct nvgpu_pmu_pg *pg);
int nvgpu_pmu_pg_sw_setup(struct gk20a *g, struct nvgpu_pmu *pmu,
	struct nvgpu_pmu_pg *pg);
void nvgpu_pmu_pg_destroy(struct gk20a *g, struct nvgpu_pmu *pmu,
	struct nvgpu_pmu_pg *pg);
int nvgpu_pmu_restore_golden_img_state(struct gk20a *g);

/* PG enable/disable */
int nvgpu_pmu_reenable_elpg(struct gk20a *g);
int nvgpu_pmu_enable_elpg(struct gk20a *g);
int nvgpu_pmu_disable_elpg(struct gk20a *g);
int nvgpu_pmu_enable_elpg_ms(struct gk20a *g);
int nvgpu_pmu_disable_elpg_ms(struct gk20a *g);

int nvgpu_pmu_pg_global_enable(struct gk20a *g, bool enable_pg);

int nvgpu_pmu_get_pg_stats(struct gk20a *g, u32 pg_engine_id,
	struct pmu_pg_stats_data *pg_stat_data);

/* AELPG */
int nvgpu_aelpg_init(struct gk20a *g);
int nvgpu_aelpg_init_and_enable(struct gk20a *g, u8 ctrl_id);
int nvgpu_pmu_ap_send_command(struct gk20a *g,
		union pmu_ap_cmd *p_ap_cmd, bool b_block);

void nvgpu_pmu_set_golden_image_initialized(struct gk20a *g, u8 state);

/* PG ops*/
int nvgpu_pmu_elpg_statistics(struct gk20a *g, u32 pg_engine_id,
		struct pmu_pg_stats_data *pg_stat_data);
void nvgpu_pmu_save_zbc(struct gk20a *g, u32 entries);
bool nvgpu_pmu_is_lpwr_feature_supported(struct gk20a *g, u32 feature_id);
int nvgpu_pmu_pg_buf_alloc(struct gk20a *g, struct nvgpu_pmu *pmu, u32 size);
u64 nvgpu_pmu_pg_buf_get_gpu_va(struct gk20a *g, struct nvgpu_pmu *pmu);

#endif /* NVGPU_PMU_PG_H */
