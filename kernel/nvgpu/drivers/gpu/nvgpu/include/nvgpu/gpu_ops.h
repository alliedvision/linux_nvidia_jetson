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
#ifndef NVGPU_GOPS_OPS_H
#define NVGPU_GOPS_OPS_H

#include <nvgpu/types.h>
#include <nvgpu/gops/acr.h>
#include <nvgpu/gops/bios.h>
#include <nvgpu/gops/cbc.h>
#include <nvgpu/gops/clk_arb.h>
#include <nvgpu/gops/debugger.h>
#include <nvgpu/gops/profiler.h>
#include <nvgpu/gops/cyclestats.h>
#include <nvgpu/gops/floorsweep.h>
#include <nvgpu/gops/sbr.h>
#include <nvgpu/gops/func.h>
#include <nvgpu/gops/nvdec.h>
#include <nvgpu/gops/pramin.h>
#include <nvgpu/gops/clk.h>
#include <nvgpu/gops/xve.h>
#include <nvgpu/gops/nvlink.h>
#include <nvgpu/gops/sec2.h>
#include <nvgpu/gops/gsp.h>
#include <nvgpu/gops/class.h>
#include <nvgpu/gops/ce.h>
#include <nvgpu/gops/ptimer.h>
#include <nvgpu/gops/top.h>
#include <nvgpu/gops/bus.h>
#include <nvgpu/gops/gr.h>
#include <nvgpu/gops/falcon.h>
#include <nvgpu/gops/fifo.h>
#include <nvgpu/gops/fuse.h>
#include <nvgpu/gops/ltc.h>
#include <nvgpu/gops/ramfc.h>
#include <nvgpu/gops/ramin.h>
#include <nvgpu/gops/runlist.h>
#include <nvgpu/gops/userd.h>
#include <nvgpu/gops/engine.h>
#include <nvgpu/gops/pbdma.h>
#include <nvgpu/gops/sync.h>
#include <nvgpu/gops/channel.h>
#include <nvgpu/gops/tsg.h>
#include <nvgpu/gops/usermode.h>
#include <nvgpu/gops/mm.h>
#include <nvgpu/gops/netlist.h>
#include <nvgpu/gops/priv_ring.h>
#include <nvgpu/gops/therm.h>
#include <nvgpu/gops/fb.h>
#include <nvgpu/gops/mc.h>
#include <nvgpu/gops/cg.h>
#include <nvgpu/gops/pmu.h>
#include <nvgpu/gops/ecc.h>
#include <nvgpu/gops/grmgr.h>
#include <nvgpu/gops/cic_mon.h>
#ifdef CONFIG_NVGPU_NON_FUSA
#include <nvgpu/gops/mssnvlink.h>
#endif

struct gk20a;
struct nvgpu_debug_context;
struct nvgpu_mem;

/** @cond DOXYGEN_SHOULD_SKIP_THIS */
struct gops_debug {
	void (*show_dump)(struct gk20a *g,
			struct nvgpu_debug_context *o);
};
/** @endcond */

/**
 * @addtogroup unit-common-nvgpu
 * @{
 */

/**
 * @brief HAL methods
 *
 * gpu_ops contains function pointers for the unit HAL interfaces. gpu_ops
 * should only contain function pointers! Non-function pointer members should go
 * in struct gk20a or be implemented with the boolean flag API defined in
 * nvgpu/enabled.h. Each unit should have its own sub-struct in the gpu_ops
 * struct.
 */
struct gpu_ops {
	/** Acr hal ops. */
	struct gops_acr acr;
	/** Ecc hal ops. */
	struct gops_ecc ecc;
	/** Ltc hal ops. */
	struct gops_ltc ltc;
#ifdef CONFIG_NVGPU_COMPRESSION
	struct gops_cbc cbc;
#endif
	/** Ce hal ops. */
	struct gops_ce ce;
	/** Gr hal ops. */
	struct gops_gr gr;
	/** Gpu class hal ops. */
	struct gops_class gpu_class;
	/** Fb hal ops. */
	struct gops_fb fb;
	/** Clock gating hal ops. */
	struct gops_cg cg;
	/** Fifo hal ops. */
	struct gops_fifo fifo;
	/** Fuse hal ops. */
	struct gops_fuse fuse;
	/** Runlist hal ops. */
	struct gops_runlist runlist;
	/** Syncpoint hal ops. */
	struct gops_sync sync;
	/** Channel hal ops. */
	struct gops_channel channel;
	/** Tsg hal ops. */
	struct gops_tsg tsg;
	/** Usermode hal ops. */
	struct gops_usermode usermode;
	/** Engine status hal ops. */
	struct gops_engine_status engine_status;
	/** Netlist hal ops. */
	struct gops_netlist netlist;
	/** Mm hal ops. */
	struct gops_mm mm;

#ifdef CONFIG_NVGPU_DGPU
	struct gops_pramin pramin;
#endif
	/** Therm hal ops. */
	struct gops_therm therm;
	/** Pmu hal ops. */
	struct gops_pmu pmu;
	/** Clock hal ops. */
	struct gops_clk clk;
#ifdef CONFIG_NVGPU_DGPU
	struct gops_clk_mon clk_mon;
#endif
#ifdef CONFIG_NVGPU_CLK_ARB
	struct gops_clk_arb clk_arb;
#endif
#ifdef CONFIG_NVGPU_DEBUGGER
	struct gops_regops regops;
#endif
	/** Mc hal ops. */
	struct gops_mc mc;
#ifdef CONFIG_NVGPU_DEBUGGER
	struct gops_debugger debugger;
	struct gops_perf perf;
	struct gops_perfbuf perfbuf;
#endif
#ifdef CONFIG_NVGPU_PROFILER
	struct gops_pm_reservation pm_reservation;
	struct gops_profiler profiler;
#endif

	/**
	 * @brief Ops to get litter value corresponding to litter define.
	 *
	 * @param g 	[in] The GPU driver struct.
	 * - The function does not perform validation of g parameter.
	 * @param value	[in] Litter define.
	 * - Must be one of the litter defined in common.nvgpu unit.
	 *
	 * This function returns the value of the litter define.
	 *
	 * Steps:
	 * - Use switch-case statement on param \a value and return chip
	 *   specific litter value.
	 * - Call #BUG() if the default label of switch-case is hit.
	 */
	u32 (*get_litter_value)(struct gk20a *g, int value);

	/**
	 * @brief Ops to initialize gpu characteristics.
	 *
	 * @param g 	[in] The GPU driver struct.
	 * - The function does not perform validation of g parameter.
	 *
	 * This function initializes gpu characteristics for the specific chip.
	 *
	 * Steps:
	 * - Calls \ref nvgpu_init_gpu_characteristics
	 *   "nvgpu_init_gpu_characteristics(g)" to initialize the default
	 *   characteristics and return error if it fails.
	 * - Calls \ref nvgpu_set_enabled
	 *   "nvgpu_set_enabled(g, NVGPU_SUPPORT_TSG_SUBCONTEXTS, true)".
	 * - Calls \ref nvgpu_set_enabled
	 *   "nvgpu_set_enabled(g, NVGPU_SUPPORT_SCG, true)".
	 * - Calls \ref nvgpu_set_enabled
	 *   "nvgpu_set_enabled(g, NVGPU_SUPPORT_SYNCPOINT_ADDRESS, true)".
	 * - Calls \ref nvgpu_set_enabled
	 *   "nvgpu_set_enabled(g, NVGPU_SUPPORT_USER_SYNCPOINT, true)".
	 * - Calls \ref nvgpu_set_enabled
	 *   "nvgpu_set_enabled(g, NVGPU_SUPPORT_USER_SYNCPOINT, true)".
	 *
	 * @return      0 in case of success, < 0 otherwise.
	 * @retval      0 in case of success.
	 */
	int (*chip_init_gpu_characteristics)(struct gk20a *g);

	/** Bus hal ops. */
	struct gops_bus bus;
	/** Ptimer hal ops. */
	struct gops_ptimer ptimer;
#ifdef CONFIG_NVGPU_CYCLESTATS
	struct gops_css css;
#endif
#ifdef CONFIG_NVGPU_DGPU
	struct gops_bios bios;
	struct gops_xve xve;
#endif
	/** Falcon hal ops. */
	struct gops_falcon falcon;
	/** Priv ring hal ops. */
	struct gops_priv_ring priv_ring;
	/** Top hal ops. */
	struct gops_top top;
/** @cond DOXYGEN_SHOULD_SKIP_THIS */
	struct gops_sbr sbr;
	struct gops_func func;
	struct gops_nvdec nvdec;
	struct gops_ramfc ramfc;
	struct gops_ramin ramin;
	struct gops_userd userd;
	struct gops_engine engine;
	struct gops_pbdma pbdma;
	struct gops_pbdma_status pbdma_status;
	/*
	 * This function is called to allocate secure memory (memory
	 * that the CPU cannot see). The function should fill the
	 * context buffer descriptor (especially fields destroy, sgt,
	 * size).
	 */
	int (*secure_alloc)(struct gk20a *g, struct nvgpu_mem *desc_mem,
			size_t size,
			void (**fn)(struct gk20a *g, struct nvgpu_mem *mem));
	struct gops_pmu_perf pmu_perf;
	struct gops_debug debug;
#ifdef CONFIG_NVGPU_DGPU
	struct gops_nvlink nvlink;
#endif
	struct gops_sec2 sec2;
	struct gops_gsp gsp;
/** @endcond */
#ifdef CONFIG_NVGPU_STATIC_POWERGATE
	struct gops_tpc_pg tpc_pg;
	struct gops_fbp_pg fbp_pg;
	struct gops_gpc_pg gpc_pg;
#endif
	/** Wake up all threads waiting on semaphore wait. */
	void (*semaphore_wakeup)(struct gk20a *g, bool post_events);

	struct gops_grmgr grmgr;

	struct gops_cic_mon cic_mon;

/** @cond DOXYGEN_SHOULD_SKIP_THIS */
#ifdef CONFIG_NVGPU_NON_FUSA
	struct gops_mssnvlink mssnvlink;
#endif
/** @endcond */
};

#endif /* NVGPU_GOPS_OPS_H */
