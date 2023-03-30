/*
 * Copyright (c) 2011-2022, NVIDIA CORPORATION.  All rights reserved.
 *
 * GK20A Graphics
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
#ifndef GK20A_H
#define GK20A_H

/**
 * @mainpage
 *
 * NVGPU Design Documentation
 * ==========================
 *
 * Welcome to the nvgpu unit design documentation. The following pages document
 * the major top level units within nvgpu-common:
 *
 *   - @ref unit-ce
 *   - @ref unit-mm
 *   - @ref unit-common-bus
 *   - @ref unit-fifo
 *   - @ref unit-gr
 *   - @ref unit-fb
 *   - @ref unit-devctl
 *   - @ref unit-sdl
 *   - @ref unit-init
 *   - @ref unit-qnx_init
 *   - @ref unit-falcon
 *   - @ref unit-os_utils
 *   - @ref unit-acr
 *   - @ref unit-cg
 *   - @ref unit-pmu
 *   - @ref unit-common-priv-ring
 *   - @ref unit-common-nvgpu
 *   - @ref unit-common-ltc
 *   - @ref unit-common-utils
 *   - @ref unit-common-netlist
 *   - @ref unit-mc
 *   - Etc, etc.
 *
 * NVGPU Software Unit Design Documentation
 * ========================================
 *
 * For each top level unit, a corresponding Unit Test Specification is
 * available in the @ref NVGPU-SWUTS
 *
 * nvgpu-driver Level Requirements Table
 * =====================================
 *
 * ...
 */

struct gk20a;
struct nvgpu_acr;
struct nvgpu_fifo;
struct nvgpu_channel;
struct nvgpu_gr;
struct nvgpu_fbp;
#ifdef CONFIG_NVGPU_SIM
struct sim_nvgpu;
#endif
#ifdef CONFIG_NVGPU_DGPU
struct nvgpu_ce_app;
#endif
#ifdef CONFIG_NVGPU_FECS_TRACE
struct gk20a_ctxsw_trace;
#endif
#ifdef CONFIG_NVGPU_TRACK_MEM_USAGE
struct nvgpu_mem_alloc_tracker;
#endif
struct nvgpu_profiler_object;
#ifdef CONFIG_NVGPU_DEBUGGER
struct dbg_profiler_object_data;
struct nvgpu_debug_context;
#endif
#ifdef CONFIG_NVGPU_HAL_NON_FUSA
struct nvgpu_clk_pll_debug_data;
#endif
struct nvgpu_nvhost_dev;
struct nvgpu_netlist_vars;
#ifdef CONFIG_NVGPU_FECS_TRACE
struct nvgpu_gr_fecs_trace;
#endif
#ifdef CONFIG_NVGPU_CLK_ARB
struct nvgpu_clk_arb;
#endif
struct nvgpu_setup_bind_args;
struct nvgpu_mem;
#ifdef CONFIG_NVGPU_CYCLESTATS
struct gk20a_cs_snapshot_client;
struct gk20a_cs_snapshot;
#endif
#ifdef CONFIG_NVGPU_DEBUGGER
struct dbg_session_gk20a;
struct nvgpu_dbg_reg_op;
#endif
#ifdef CONFIG_NVGPU_KERNEL_MODE_SUBMIT
struct _resmgr_context;
struct nvgpu_gpfifo_entry;
#endif
#ifdef CONFIG_NVGPU_HAL_NON_FUSA
struct clk_domains_mon_status_params;
#endif
struct nvgpu_cic_mon;
struct nvgpu_cic_rm;
#ifdef CONFIG_NVGPU_GSP_SCHEDULER
struct nvgpu_gsp_sched;
#endif
#ifdef CONFIG_NVGPU_GSP_STRESS_TEST
struct nvgpu_gsp_test;
#endif

#ifdef CONFIG_NVGPU_DGPU
enum nvgpu_nvlink_minion_dlcmd;
#endif
enum nvgpu_profiler_pm_resource_type;
enum nvgpu_profiler_pm_reservation_scope;

#include <nvgpu/lock.h>
#include <nvgpu/thread.h>
#include <nvgpu/utils.h>

#include <nvgpu/mm.h>
#include <nvgpu/as.h>
#include <nvgpu/log.h>
#include <nvgpu/kref.h>
#include <nvgpu/pmu.h>
#include <nvgpu/atomic.h>
#include <nvgpu/barrier.h>
#include <nvgpu/rwsem.h>
#ifdef CONFIG_NVGPU_DGPU
#include <nvgpu/nvlink.h>
#include <nvgpu/nvlink_link_mode_transitions.h>
#endif
#include <nvgpu/ecc.h>
#include <nvgpu/channel.h>
#include <nvgpu/tsg.h>
#include <nvgpu/sec2/sec2.h>
#include <nvgpu/cbc.h>
#include <nvgpu/ltc.h>
#include <nvgpu/worker.h>
#ifdef CONFIG_NVGPU_DGPU
#include <nvgpu/bios.h>
#endif
#include <nvgpu/semaphore.h>
#include <nvgpu/fifo.h>
#include <nvgpu/sched.h>
#include <nvgpu/ipa_pa_cache.h>
#include <nvgpu/mig.h>

#include <nvgpu/gpu_ops.h>

#ifdef CONFIG_NVGPU_NON_FUSA
#include "hal/clk/clk_gk20a.h"
#endif

/**
 * @addtogroup unit-common-nvgpu
 * @{
 */

#ifdef CONFIG_DEBUG_FS
struct railgate_stats {
	unsigned long last_rail_gate_start;
	unsigned long last_rail_gate_complete;
	unsigned long last_rail_ungate_start;
	unsigned long last_rail_ungate_complete;
	unsigned long total_rail_gate_time_ms;
	unsigned long total_rail_ungate_time_ms;
	unsigned long railgating_cycle_count;
};
#endif

/**
 * @defgroup NVGPU_COMMON_NVGPU_DEFINES
 *
 * GPU litters defines which corresponds to various chip specific values related
 * to h/w units.
 */

/**
 * @ingroup NVGPU_COMMON_NVGPU_DEFINES
 * @{
 */

/** Number of gpcs. */
#define GPU_LIT_NUM_GPCS	0
/** Number of pes per gpc. */
#define GPU_LIT_NUM_PES_PER_GPC 1
/** Number of zcull banks. */
#define GPU_LIT_NUM_ZCULL_BANKS 2
/** Number of tpcs per gpc. */
#define GPU_LIT_NUM_TPC_PER_GPC 3
/** Number of SMs per tpc. */
#define GPU_LIT_NUM_SM_PER_TPC  4
/** Number of fbps. */
#define GPU_LIT_NUM_FBPS	5
/** Gpc base address (in bytes). */
#define GPU_LIT_GPC_BASE	6
/** Gpc stride (in bytes). */
#define GPU_LIT_GPC_STRIDE	7
/** Gpc shared base offset (in bytes). */
#define GPU_LIT_GPC_SHARED_BASE 8
/** Tpc's base offset in gpc (in bytes). */
#define GPU_LIT_TPC_IN_GPC_BASE 9
/** Tpc's stride in gpc (in bytes). */
#define GPU_LIT_TPC_IN_GPC_STRIDE 10
/** Tpc's shared base offset in gpc (in bytes). */
#define GPU_LIT_TPC_IN_GPC_SHARED_BASE 11
/** Ppc's base offset in gpc (in bytes). */
#define GPU_LIT_PPC_IN_GPC_BASE	12
/** Ppc's stride in gpc (in bytes). */
#define GPU_LIT_PPC_IN_GPC_STRIDE 13
/** Ppc's shared base offset in gpc (in bytes). */
#define GPU_LIT_PPC_IN_GPC_SHARED_BASE 14
/** Rop base offset (in bytes). */
#define GPU_LIT_ROP_BASE	15
/** Rop stride (in bytes). */
#define GPU_LIT_ROP_STRIDE	16
/** Rop shared base offset (in bytes). */
#define GPU_LIT_ROP_SHARED_BASE 17
/** Number of host engines. */
#define GPU_LIT_HOST_NUM_ENGINES 18
/** Number of host pbdma. */
#define GPU_LIT_HOST_NUM_PBDMA	19
/** LTC stride (in bytes). */
#define GPU_LIT_LTC_STRIDE	20
/** LTS stride (in bytes). */
#define GPU_LIT_LTS_STRIDE	21
/** Number of fbpas. */
#define GPU_LIT_NUM_FBPAS	22
/** Fbpa stride (in bytes). */
#define GPU_LIT_FBPA_STRIDE	23
/** Fbpa base offset (in bytes). */
#define GPU_LIT_FBPA_BASE	24
/** Fbpa shared base offset (in bytes). */
#define GPU_LIT_FBPA_SHARED_BASE 25
/** Sm pri stride (in bytes). */
#define GPU_LIT_SM_PRI_STRIDE	26
/** Smpc pri base offset (in bytes). */
#define GPU_LIT_SMPC_PRI_BASE		27
/** Smpc pri shared base offset (in bytes). */
#define GPU_LIT_SMPC_PRI_SHARED_BASE	28
/** Smpc pri unique base offset (in bytes). */
#define GPU_LIT_SMPC_PRI_UNIQUE_BASE	29
/** Smpc pri stride (in bytes). */
#define GPU_LIT_SMPC_PRI_STRIDE		30
/** Twod class. */
#define GPU_LIT_TWOD_CLASS	31
/** Threed class. */
#define GPU_LIT_THREED_CLASS	32
/** Compute class. */
#define GPU_LIT_COMPUTE_CLASS	33
/** Gpfifo class. */
#define GPU_LIT_GPFIFO_CLASS	34
/** I2m class. */
#define GPU_LIT_I2M_CLASS	35
/** Dma copy class. */
#define GPU_LIT_DMA_COPY_CLASS	36
/** Gpc priv stride (in bytes). */
#define GPU_LIT_GPC_PRIV_STRIDE	37
#ifdef CONFIG_NVGPU_DEBUGGER
#define GPU_LIT_PERFMON_PMMGPCTPCA_DOMAIN_START 38
#define GPU_LIT_PERFMON_PMMGPCTPCB_DOMAIN_START 39
#define GPU_LIT_PERFMON_PMMGPCTPC_DOMAIN_COUNT  40
#define GPU_LIT_PERFMON_PMMFBP_LTC_DOMAIN_START 41
#define GPU_LIT_PERFMON_PMMFBP_LTC_DOMAIN_COUNT 42
#define GPU_LIT_PERFMON_PMMFBP_ROP_DOMAIN_START 43
#define GPU_LIT_PERFMON_PMMFBP_ROP_DOMAIN_COUNT 44
#endif
#define GPU_LIT_SM_UNIQUE_BASE			45
#define GPU_LIT_SM_SHARED_BASE			46
#define GPU_LIT_GPC_ADDR_WIDTH			47
#define GPU_LIT_TPC_ADDR_WIDTH			48
#define GPU_LIT_MAX_RUNLISTS_SUPPORTED		49
#define GPU_LIT_NUM_LTC_LTS_SETS		50
#define GPU_LIT_NUM_LTC_LTS_WAYS		51
#define GPU_LIT_ROP_IN_GPC_BASE			52
#define GPU_LIT_ROP_IN_GPC_SHARED_BASE		53
#define GPU_LIT_ROP_IN_GPC_PRI_SHARED_IDX	54
#define GPU_LIT_ROP_IN_GPC_STRIDE		55
#define GPU_LIT_PERFMON_PMMGPC_ROP_DOMAIN_START	56
#define GPU_LIT_PERFMON_PMMGPC_ROP_DOMAIN_COUNT	57

/** Macro to get litter values corresponding to the litter defines. */
#define nvgpu_get_litter_value(g, v) ((g)->ops.get_litter_value((g), v))

/**
 * @}
 */
/** @cond DOXYGEN_SHOULD_SKIP_THIS */
#ifdef CONFIG_NVGPU_STATIC_POWERGATE
#define MAX_PG_GPC		2
#define MAX_TPC_PER_GPC		4
#define PG_GPC0			0
#define PG_GPC1			1
/*
 * MAX_PG_TPC_CONFIGS describes the maximum number of
 * valid configurations we can have for the TPC mask.
 */
#define MAX_PG_TPC_CONFIGS	(0x1 << MAX_TPC_PER_GPC)
/*
 * MAX_PG_GPC_FBP_CONFIGS describes the maximum number of
 * valid configurations we can have for the GPC and FBP mask.
 */
#define MAX_PG_GPC_FBP_CONFIGS	((0x1 << MAX_PG_GPC) - 1)

#endif

#ifdef CONFIG_NVGPU_KERNEL_MODE_SUBMIT
struct nvgpu_gpfifo_userdata {
	struct nvgpu_gpfifo_entry nvgpu_user *entries;
	struct _resmgr_context *context;
};
#endif

#ifdef CONFIG_NVGPU_CHANNEL_TSG_CONTROL
enum nvgpu_event_id_type {
	NVGPU_EVENT_ID_BPT_INT = 0,
	NVGPU_EVENT_ID_BPT_PAUSE = 1,
	NVGPU_EVENT_ID_BLOCKING_SYNC = 2,
	NVGPU_EVENT_ID_CILP_PREEMPTION_STARTED = 3,
	NVGPU_EVENT_ID_CILP_PREEMPTION_COMPLETE = 4,
	NVGPU_EVENT_ID_GR_SEMAPHORE_WRITE_AWAKEN = 5,
	NVGPU_EVENT_ID_MAX = 6,
};
#endif

/**
 * @brief HW version info read from the HW.
 */
struct nvgpu_gpu_params {
	/** GPU architecture ID */
	u32 gpu_arch;
	/** GPU implementation ID */
	u32 gpu_impl;
	/** GPU revision ID */
	u32 gpu_rev;
	/** sm version */
	u32 sm_arch_sm_version;
	/** sm instruction set */
	u32 sm_arch_spa_version;
	/** total number of physical warps possible on an SM. */
	u32 sm_arch_warp_count;
};

/**
 * @brief The GPU superstructure.
 *
 * This structure describes the GPU. There is a unique \a gk20a struct for each
 * GPU in the system. This structure includes many state variables used
 * throughout the driver. It also contains the #gpu_ops HALs.
 *
 * Whenever possible, units should keep their data within their own sub-struct
 * and not in the main gk20a struct.
 */
struct gk20a {
#ifdef CONFIG_NVGPU_NON_FUSA
	/**
	 * @brief Free data in the struct allocated during its creation.
	 *
	 * @param g [in]	The GPU superstructure
	 *
	 * This does not free all of the memory in the structure as many of the
	 * units allocate private data, and those units are responsible for
	 * freeing that data. \a gfree should be called after all of the units
	 * have had the opportunity to free their private data.
	 */
	void (*gfree)(struct gk20a *g);
#endif

	/** Starting virtual address of mapped bar0 io region. */
	uintptr_t regs;
	u64 regs_size;
	u64 regs_bus_addr;

	/** Starting virtual address of mapped bar1 io region. */
	uintptr_t bar1;

	/** Starting virtual address of usermode registers io region. */
	uintptr_t usermode_regs;
	u64 usermode_regs_bus_addr;

	uintptr_t regs_saved;
	uintptr_t bar1_saved;
	uintptr_t usermode_regs_saved;

	/**
	 * Handle to access nvhost APIs.
	 */
	struct nvgpu_nvhost_dev *nvhost;

	/**
	 * Used by <nvgpu/errata.h>. Do not access directly!
	 */
	unsigned long *errata_flags;

	/**
	 * Used by <nvgpu/enabled.h>. Do not access directly!
	 */
	unsigned long *enabled_flags;

#ifdef CONFIG_NVGPU_NON_FUSA
	/** Used by Linux module to keep track of driver usage */
	nvgpu_atomic_t usage_count;
#endif

	/** Used by common.init unit to track users of the driver */
	struct nvgpu_ref refcount;

	/** Name of the gpu. */
	const char *name;

	/**
	 * Is the GPU ready to be used? Access to this field is protected by
	 * lock \ref gk20a "gk20a.power_spinlock".
	 */
	u32 power_on_state;

	/** Is the GPU probe complete? */
	bool probe_done;

#ifdef CONFIG_NVGPU_DGPU
	bool gpu_reset_done;
#endif
#ifdef CONFIG_PM
	bool suspended;
#endif
#ifdef CONFIG_NVGPU_NON_FUSA
	bool sw_ready;
#endif

	/** Flag to indicate that quiesce framework is initialized. */
	bool sw_quiesce_init_done;
	/** Flag to indicate that system is transitioning to quiesce state. */
	bool sw_quiesce_pending;
	/** Condition variable on which quiesce thread waits. */
	struct nvgpu_cond sw_quiesce_cond;
	/** Quiesce thread id. */
	struct nvgpu_thread sw_quiesce_thread;
	/**
	 * Struct having callback and it's arguments. The callback gets called
	 * when \ref BUG is hit by the code.
	 */
	struct nvgpu_bug_cb sw_quiesce_bug_cb;

	/** An entry into list of callbacks to be called when BUG() is hit. */
	struct nvgpu_list_node bug_node;

	/** Controls which messages are logged */
	u64 log_mask;
#ifdef CONFIG_NVGPU_NON_FUSA
	u32 log_trace;
#endif

#ifdef CONFIG_NVGPU_STATIC_POWERGATE
	struct nvgpu_mutex static_pg_lock;
#endif

	/** Stored HW version info */
	struct nvgpu_gpu_params params;

#ifdef CONFIG_NVGPU_DETERMINISTIC_CHANNELS
	/**
	 * Guards access to hardware when usual gk20a_{busy,idle} are skipped
	 * for submits and held for channel lifetime but dropped for an ongoing
	 * gk20a_do_idle().
	 */
	struct nvgpu_rwsem deterministic_busy;
#endif
	/** Pointer to struct containing netlist data of ucodes. */
	struct nvgpu_netlist_vars *netlist_vars;
	/** Flag to indicate initialization status of netlists. */
	bool netlist_valid;

	/** Struct holding the pmu falcon software state. */
	struct nvgpu_falcon pmu_flcn;
	/** Struct holding the fecs falcon software state. */
	struct nvgpu_falcon fecs_flcn;
	/** Struct holding the gpccs falcon software state. */
	struct nvgpu_falcon gpccs_flcn;
#ifdef CONFIG_NVGPU_DGPU
	struct nvgpu_falcon nvdec_flcn;
	struct nvgpu_falcon minion_flcn;
	struct clk_gk20a clk;
#endif
	struct nvgpu_falcon gsp_flcn;
	/** Top level struct maintaining fifo unit's software state. */
	struct nvgpu_fifo fifo;
#ifdef CONFIG_NVGPU_DGPU
	struct nvgpu_nvlink_dev nvlink;
#endif
	/** Pointer to struct maintaining multiple GR instance's software state. */
	struct nvgpu_gr *gr;
	u32 num_gr_instances;
	/** Pointer to struct maintaining fbp unit's software state. */
	struct nvgpu_fbp *fbp;
#ifdef CONFIG_NVGPU_SIM
	struct sim_nvgpu *sim;
#endif
	struct nvgpu_device_list *devs;
	/** Top level struct maintaining MM unit's software state. */
	struct mm_gk20a mm;
	/** Pointer to struct maintaining PMU unit's software state. */
	struct nvgpu_pmu *pmu;
	/** Pointer to struct maintaining ACR unit's software state. */
	struct nvgpu_acr *acr;
#ifdef CONFIG_NVGPU_GSP_SCHEDULER
	/** Pointer to struct maintaining GSP unit's software state. */
	struct nvgpu_gsp_sched *gsp_sched;
#endif
#ifdef CONFIG_NVGPU_GSP_STRESS_TEST
	struct nvgpu_gsp_test *gsp_stest;
#endif
	/** Top level struct maintaining ECC unit's software state. */
	struct nvgpu_ecc ecc;
#ifdef CONFIG_NVGPU_DGPU
	struct pmgr_pmupstate *pmgr_pmu;
	struct nvgpu_sec2 sec2;
#endif
#ifdef CONFIG_NVGPU_CHANNEL_TSG_SCHEDULING
	struct nvgpu_sched_ctrl sched_ctrl;
#endif

#ifdef CONFIG_DEBUG_FS
	struct railgate_stats pstats;
#endif
	/** Global default timeout for use throughout driver */
	u32 poll_timeout_default;
	/** User disabled timeouts */
	bool timeouts_disabled_by_user;

#ifdef CONFIG_NVGPU_CHANNEL_WDT
	unsigned int ch_wdt_init_limit_ms;
	u32 ctxsw_wdt_period_us;
#endif
	/**
	 * Timeout after which ctxsw timeout interrupt (if enabled by s/w) will
	 * be triggered by h/w if context fails to context switch.
	 */
	u32 ctxsw_timeout_period_ms;

#ifdef CONFIG_NVGPU_NON_FUSA
	struct nvgpu_mutex power_lock;
#endif

	/** Lock to protect accessing \ref gk20a "gk20a.power_on_state". */
	struct nvgpu_spinlock power_spinlock;

#ifdef CONFIG_NVGPU_CHANNEL_TSG_SCHEDULING
	/** Channel priorities */
	u32 tsg_timeslice_low_priority_us;
	u32 tsg_timeslice_medium_priority_us;
	u32 tsg_timeslice_high_priority_us;
	u32 tsg_timeslice_min_us;
	u32 tsg_timeslice_max_us;
#endif
	u32 tsg_dbg_timeslice_max_us;
	/**
	 * Flag to indicate if runlist interleaving is supported or not. Set to
	 * true for safety.
	 */
	bool runlist_interleave;

	/** Lock serializing CG an PG programming for various units */
	struct nvgpu_mutex cg_pg_lock;
	/** SLCG setting read from the platform data */
	bool slcg_enabled;
	/** BLCG setting read from the platform data */
	bool blcg_enabled;
	/** ELCG setting read from the platform data */
	bool elcg_enabled;
#ifdef CONFIG_NVGPU_LS_PMU
	bool elpg_enabled;
	bool elpg_ms_enabled;
	bool aelpg_enabled;
	bool can_elpg;
#endif
#ifdef CONFIG_NVGPU_NON_FUSA
	bool mscg_enabled;
	bool forced_idle;
	bool forced_reset;
#endif
	/** Allow priv register access to all. */
	bool allow_all;

	/** Ptimer source frequency. */
	u32 ptimer_src_freq;

#ifdef CONFIG_NVGPU_NON_FUSA
	int railgate_delay;
	u8 ldiv_slowdown_factor;
#endif
#ifdef CONFIG_NVGPU_KERNEL_MODE_SUBMIT
	unsigned int aggressive_sync_destroy_thresh;
	bool aggressive_sync_destroy;
#endif

	/** Is LS PMU supported? */
	bool support_ls_pmu;

	/** Is this a virtual GPU? */
	bool is_virtual;

#ifdef CONFIG_NVGPU_NON_FUSA
	/* Whether cde engine is supported or not. */
	bool has_cde;

	u32 emc3d_ratio;
#endif

#ifdef CONFIG_NVGPU_SW_SEMAPHORE
	/**
	 * A group of semaphore pools. One for each channel.
	 */
	struct nvgpu_semaphore_sea *sema_sea;
#endif

#ifdef CONFIG_NVGPU_DEBUGGER
	/* held while manipulating # of debug/profiler sessions present */
	/* also prevents debug sessions from attaching until released */
	struct nvgpu_mutex dbg_sessions_lock;
	int dbg_powergating_disabled_refcount; /*refcount for pg disable */
	/*refcount for timeout disable */
	nvgpu_atomic_t timeouts_disabled_refcount;

	/* must have dbg_sessions_lock before use */
	struct nvgpu_dbg_reg_op *dbg_regops_tmp_buf;
	u32 dbg_regops_tmp_buf_ops;

	/* For perfbuf mapping */
	struct {
		struct dbg_session_gk20a *owner;
		u64 offset;
	} perfbuf;

	bool mmu_debug_ctrl;
	u32 mmu_debug_mode_refcnt;
#endif /* CONFIG_NVGPU_DEBUGGER */

#ifdef CONFIG_NVGPU_PROFILER
	struct nvgpu_list_node profiler_objects;
	struct nvgpu_pm_resource_reservations *pm_reservations;
	nvgpu_atomic_t hwpm_refcount;

	u32 num_sys_perfmon;
	u32 num_gpc_perfmon;
	u32 num_fbp_perfmon;
#endif

#ifdef CONFIG_NVGPU_FECS_TRACE
	struct gk20a_ctxsw_trace *ctxsw_trace;
	struct nvgpu_gr_fecs_trace *fecs_trace;
#endif

#ifdef CONFIG_NVGPU_CYCLESTATS
	struct nvgpu_mutex		cs_lock;
	struct gk20a_cs_snapshot	*cs_data;
#endif

#ifdef CONFIG_NVGPU_NON_FUSA
	/* Called after all references to driver are gone. Unused in safety */
	void (*remove_support)(struct gk20a *g);
#endif
#ifdef CONFIG_NVGPU_POWER_PG
	u64 pg_ingating_time_us;
	u64 pg_ungating_time_us;
	u32 pg_gating_cnt;
	u32 pg_ms_gating_cnt;
#endif

	/** GPU address-space identifier. */
	struct gk20a_as as;

	/** The HAL function pointers */
	struct gpu_ops ops;

#ifdef CONFIG_NVGPU_LS_PMU
	/*used for change of enum zbc update cmd id from ver 0 to ver1*/
	u8 pmu_ver_cmd_id_zbc_table_update;
#endif

	/** Top level struct managing interrupt handling. */
	struct nvgpu_mc mc;

#ifdef CONFIG_NVGPU_COMPRESSION
	/*
	 * The deductible memory size for max_comptag_mem (in MBytes)
	 * Usually close to memory size that running system is taking
	*/
	u32 comptag_mem_deduct;

	u32 max_comptag_mem; /* max memory size (MB) for comptag */
	struct nvgpu_cbc *cbc;
#endif

#ifdef CONFIG_NVGPU_NON_FUSA
	u32 ltc_streamid;
#endif
	/** ltc unit's meta data handle. */
	struct nvgpu_ltc *ltc;

#ifdef CONFIG_NVGPU_KERNEL_MODE_SUBMIT
	struct nvgpu_channel_worker {
		struct nvgpu_worker worker;

#ifdef CONFIG_NVGPU_CHANNEL_WDT
		u32 watchdog_interval;
		struct nvgpu_timeout timeout;
#endif
	} channel_worker;
#endif

#ifdef CONFIG_NVGPU_CLK_ARB
	struct nvgpu_clk_arb_worker {
		struct nvgpu_worker worker;
	} clk_arb_worker;
#endif

	struct {
#ifdef CONFIG_NVGPU_KERNEL_MODE_SUBMIT
		void (*open)(struct nvgpu_channel *ch);
#endif
		/** Os specific callback called at channel closure. */
		void (*close)(struct nvgpu_channel *ch, bool force);
#ifdef CONFIG_NVGPU_KERNEL_MODE_SUBMIT
		void (*work_completion_signal)(struct nvgpu_channel *ch);
		void (*work_completion_cancel_sync)(struct nvgpu_channel *ch);
		bool (*os_fence_framework_inst_exists)(struct nvgpu_channel *ch);
		int (*init_os_fence_framework)(
			struct nvgpu_channel *ch, const char *fmt, ...);
		void (*signal_os_fence_framework)(struct nvgpu_channel *ch,
				struct nvgpu_fence_type *fence);
		void (*destroy_os_fence_framework)(struct nvgpu_channel *ch);
		int (*copy_user_gpfifo)(struct nvgpu_gpfifo_entry *dest,
				struct nvgpu_gpfifo_userdata userdata,
				u32 start, u32 length);
#endif
		/** Os specific callback to allocate usermode buffers. */
		int (*alloc_usermode_buffers)(struct nvgpu_channel *c,
			struct nvgpu_setup_bind_args *args);
		/** Os specific callback to free usermode buffers. */
		void (*free_usermode_buffers)(struct nvgpu_channel *c);
	} os_channel;

#ifdef CONFIG_NVGPU_NON_FUSA
	/* Used by Linux OS Layer */
	struct gk20a_scale_profile *scale_profile;
	unsigned long last_freq;

	u32 tpc_fs_mask_user;
	u32 fecs_feature_override_ecc_val;

	/** VAB struct */
	struct nvgpu_vab vab;

#endif

#ifdef CONFIG_NVGPU_STATIC_POWERGATE
	/** @cond DOXYGEN_SHOULD_SKIP_THIS */
	/* tpc pg mask array for available GPCs */
	u32 tpc_pg_mask[MAX_PG_GPC];
	u32 fbp_pg_mask;
	u32 gpc_pg_mask;
	bool can_tpc_pg;
	bool can_fbp_pg;
	bool can_gpc_pg;
#endif

#ifdef CONFIG_NVGPU_DGPU
	struct nvgpu_bios *bios;
	bool bios_is_init;
#endif

#ifdef CONFIG_NVGPU_CLK_ARB
	struct nvgpu_clk_arb *clk_arb;

	struct nvgpu_mutex clk_arb_enable_lock;

	nvgpu_atomic_t clk_arb_global_nr;
#endif

#ifdef CONFIG_NVGPU_DGPU
	struct nvgpu_ce_app *ce_app;
#endif

#ifdef CONFIG_NVGPU_NON_FUSA
	/** Flag to control enabling/disabling of illegal compstat intr. */
	bool ltc_intr_en_illegal_compstat;
#endif

	/** Are we currently running on a FUSA device configuration? */
	bool is_fusa_sku;

	u16 pci_class;
#ifdef CONFIG_NVGPU_DGPU
	/* PCI device identifier */
	u16 pci_vendor_id, pci_device_id;
	u16 pci_subsystem_vendor_id, pci_subsystem_device_id;
	u8 pci_revision;

	/*
	 * PCI power management: i2c device index, port and address for
	 * INA3221.
	 */
	u32 ina3221_dcb_index;
	u32 ina3221_i2c_address;
	u32 ina3221_i2c_port;
	bool hardcode_sw_threshold;

	/* PCIe power states. */
	bool xve_l0s;
	bool xve_l1;

#if defined(CONFIG_PCI_MSI)
	/* Check if msi is enabled */
	bool msi_enabled;
#endif
#endif
	/**
	 * The per-device identifier. The iGPUs without a PDI will use
	 * the SoC PDI if one exists. Zero if neither exists.
	 */
	u64 per_device_identifier;

#ifdef CONFIG_NVGPU_TRACK_MEM_USAGE
	struct nvgpu_mem_alloc_tracker *vmallocs;
	struct nvgpu_mem_alloc_tracker *kmallocs;
#endif

#ifdef CONFIG_NVGPU_NON_FUSA
	u64 dma_memory_used;
#endif

#if defined(CONFIG_TEGRA_GK20A_NVHOST)
	/** Full syncpoint aperture base memory address. */
	u64		syncpt_unit_base;
	/** Full syncpoint aperture size. */
	size_t		syncpt_unit_size;
	/** Each syncpoint aperture size */
	u32		syncpt_size;
#endif
	/** Full syncpoint aperture. */
	struct nvgpu_mem syncpt_mem;

#ifdef CONFIG_NVGPU_LS_PMU
	struct nvgpu_list_node boardobj_head;
	struct nvgpu_list_node boardobjgrp_head;
#endif

#ifdef CONFIG_NVGPU_DGPU
	struct nvgpu_mem pdb_cache_errata_mem;

	u16 dgpu_max_clk;
#endif

	/** Max SM diversity configuration count. */
	u32 max_sm_diversity_config_count;

	/**  Multi Instance GPU information. */
	struct nvgpu_mig mig;

	/** Pointer to struct storing CIC-MON's data */
	struct nvgpu_cic_mon *cic_mon;

	/** Pointer to struct storing CIC-RM's data */
	struct nvgpu_cic_rm *cic_rm;

	/** Cache to store IPA to PA translations. */
	struct nvgpu_ipa_pa_cache ipa_pa_cache;

	/** To enable emulate mode */
	u32 emulate_mode;

	/** Flag to check if debugger and profiler support is enabled. */
	u32 support_gpu_tools;

#ifdef CONFIG_NVS_PRESENT
	struct nvgpu_nvs_scheduler *scheduler;
	struct nvgpu_mutex sched_mutex;
#endif

#ifdef CONFIG_NVGPU_ENABLE_MISC_EC
	bool enable_polling;
#endif
};

/**
 * @brief Check if watchdog and context switch timeouts are enabled.
 *
 * @param g [in]	The GPU superstructure.
 * - The function does not perform validation of g parameter.
 *
 * @return timeouts enablement status
 * @retval True  always for safety or if these timeouts are actually enabled on
 *               other builds.
 * @retval False never for safety or if these timeouts are actually disabled on
 *               other builds.
 */
static inline bool nvgpu_is_timeouts_enabled(struct gk20a *g)
{
#ifdef CONFIG_NVGPU_DEBUGGER
	return nvgpu_atomic_read(&g->timeouts_disabled_refcount) == 0;
#else
	(void)g;
	return true;
#endif
}

/** Minimum poll delay value for h/w interactions(in microseconds). */
#define POLL_DELAY_MIN_US	10U
/** Maximum poll delay value for h/w interactions(in microseconds). */
#define POLL_DELAY_MAX_US	200U

/**
 * @brief Get the global poll timeout value
 *
 * @param g [in]	The GPU superstructure.
 * - The function does not perform validation of g parameter.
 *
 * @return The value of the global poll timeout value in microseconds.
 * @retval \ref NVGPU_DEFAULT_POLL_TIMEOUT_MS for safety as timeout is always
 *         enabled.
 */
static inline u32 nvgpu_get_poll_timeout(struct gk20a *g)
{
	return nvgpu_is_timeouts_enabled(g) ?
		g->poll_timeout_default : U32_MAX;
}

/** IO Resource in the device tree for BAR0 */
#define GK20A_BAR0_IORESOURCE_MEM	0U
/** IO Resource in the device tree for BAR1 */
#define GK20A_BAR1_IORESOURCE_MEM	1U
/** IO Resource in the device tree for SIM mem */
#define GK20A_SIM_IORESOURCE_MEM	2U

#ifdef CONFIG_NVGPU_VPR
int gk20a_do_idle(void *_g);
int gk20a_do_unidle(void *_g);
#endif

#ifdef CONFIG_PM
int gk20a_do_idle(void *_g);
int gk20a_do_unidle(void *_g);
#endif

/**
 * Constructs unique and compact GPUID from nvgpu_gpu_characteristics
 * arch/impl fields.
 */
#define GK20A_GPUID(arch, impl) ((u32) ((arch) | (impl)))

/** gk20a HW version */
#define GK20A_GPUID_GK20A   0x000000EAU
/** gm20b HW version */
#define GK20A_GPUID_GM20B   0x0000012BU
/** gm20b.b HW version */
#define GK20A_GPUID_GM20B_B 0x0000012EU
/** gp10b HW version */
#define NVGPU_GPUID_GP10B   0x0000013BU
/** gv11b HW version */
#define NVGPU_GPUID_GV11B   0x0000015BU
/** gv100 HW version */
#define NVGPU_GPUID_GV100   0x00000140U
/** tu104 HW version */
#define NVGPU_GPUID_TU104   0x00000164U
/** ga100 HW Version */
#define NVGPU_GPUID_GA100   0x00000170U
/** ga10b HW version */
#define NVGPU_GPUID_GA10B   0x0000017BU

/**
 * @}
 */

#endif /* GK20A_H */
