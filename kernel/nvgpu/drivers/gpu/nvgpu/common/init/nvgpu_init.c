/*
 * GK20A Graphics
 *
 * Copyright (c) 2011-2022, NVIDIA CORPORATION.  All rights reserved.
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

#include <nvgpu/debug.h>
#include <nvgpu/nvgpu_common.h>
#include <nvgpu/kmem.h>
#include <nvgpu/allocator.h>
#include <nvgpu/timers.h>
#include <nvgpu/soc.h>
#include <nvgpu/enabled.h>
#include <nvgpu/gmmu.h>
#include <nvgpu/vidmem.h>
#include <nvgpu/soc.h>
#include <nvgpu/mc.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/channel_sync.h>
#include <nvgpu/nvgpu_init.h>
#include <nvgpu/trace.h>
#include <nvgpu/nvhost.h>
#include <nvgpu/fb.h>
#include <nvgpu/device.h>
#include <nvgpu/gr/gr.h>
#include <nvgpu/power_features/cg.h>
#ifdef CONFIG_NVGPU_GSP_SCHEDULER
#include <nvgpu/gsp.h>
#include <nvgpu/gsp_sched.h>
#endif
#ifdef CONFIG_NVGPU_GSP_STRESS_TEST
#include <nvgpu/gsp/gsp_test.h>
#endif
#include <nvgpu/pm_reservation.h>
#include <nvgpu/netlist.h>
#include <nvgpu/hal_init.h>
#ifdef CONFIG_NVGPU_NON_FUSA
#include <nvgpu/ptimer.h>
#endif
#include <nvgpu/cic_mon.h>
#include <nvgpu/cic_rm.h>
#include <nvgpu/fbp.h>
#include <nvgpu/nvs.h>

#ifdef CONFIG_NVGPU_LS_PMU
#include <nvgpu/pmu/pmu_pstate.h>
#endif

#ifdef CONFIG_NVGPU_POWER_PG
#include <nvgpu/pmu/pmu_pg.h>
#endif

bool is_nvgpu_gpu_state_valid(struct gk20a *g)
{
	u32 boot_0 = g->ops.mc.get_chip_details(g, NULL, NULL, NULL);

	if (boot_0 == 0xffffffffU) {
		nvgpu_err(g, "GPU has disappeared from bus!!");
		return false;
	}
	return true;
}

void nvgpu_check_gpu_state(struct gk20a *g)
{
	if (!is_nvgpu_gpu_state_valid(g)) {
		nvgpu_err(g, "Entering SW Quiesce!!");
		nvgpu_sw_quiesce(g);
	}
}

static void gk20a_mask_interrupts(struct gk20a *g)
{
	nvgpu_cic_mon_intr_mask(g);
#ifdef CONFIG_NVGPU_NON_FUSA
	nvgpu_cic_rm_log_pending_intrs(g);
#endif
}

#define NVGPU_SW_QUIESCE_TIMEOUT_MS	50

static int nvgpu_sw_quiesce_thread(void *data)
{
	struct gk20a *g = data;

	/* wait until SW quiesce is requested */
	NVGPU_COND_WAIT_INTERRUPTIBLE(&g->sw_quiesce_cond,
		g->sw_quiesce_pending ||
		nvgpu_thread_should_stop(&g->sw_quiesce_thread), 0U);

	if (nvgpu_thread_should_stop(&g->sw_quiesce_thread)) {
		goto done;
	}

	nvgpu_err(g, "SW quiesce thread running");

	nvgpu_msleep(NVGPU_SW_QUIESCE_TIMEOUT_MS);

	nvgpu_disable_irqs(g);
	nvgpu_channel_sw_quiesce(g);
	nvgpu_bug_exit();

done:
	nvgpu_log_info(g, "done");
	return 0;
}

static void nvgpu_sw_quiesce_bug_cb(void *arg)
{
	struct gk20a *g = arg;

	nvgpu_sw_quiesce(g);
}

static void nvgpu_sw_quiesce_thread_stop_fn(void *data)
{
	struct gk20a *g = data;

	/*
	 * If the thread is still waiting on the cond,
	 * nvgpu_thread_should_stop() will return true, and the thread will
	 * exit.
	 */
	nvgpu_cond_signal_interruptible(&g->sw_quiesce_cond);
}

void nvgpu_sw_quiesce_remove_support(struct gk20a *g)
{
	if (g->sw_quiesce_init_done) {
		nvgpu_bug_unregister_cb(&g->sw_quiesce_bug_cb);
		nvgpu_thread_stop_graceful(&g->sw_quiesce_thread,
					   nvgpu_sw_quiesce_thread_stop_fn,
					   g);
		nvgpu_cond_destroy(&g->sw_quiesce_cond);
		g->sw_quiesce_init_done = false;
	}
}

static int nvgpu_sw_quiesce_init_support(struct gk20a *g)
{
	int err;

	if (g->sw_quiesce_init_done) {
		return 0;
	}


	err = nvgpu_cond_init(&g->sw_quiesce_cond);
	if (err != 0) {
		nvgpu_err(g, "nvgpu_cond_init() failed err=%d", err);
		return err;
	}

	g->sw_quiesce_pending = false;

	err = nvgpu_thread_create(&g->sw_quiesce_thread, g,
			nvgpu_sw_quiesce_thread, "sw-quiesce");
	if (err != 0) {
		nvgpu_cond_destroy(&g->sw_quiesce_cond);
		return err;
	}

	g->sw_quiesce_init_done = true;

	/* register callback to SW quiesce GPU in case of BUG() */
	g->sw_quiesce_bug_cb.cb = nvgpu_sw_quiesce_bug_cb;
	g->sw_quiesce_bug_cb.arg = g;
	g->sw_quiesce_bug_cb.sw_quiesce_data = true;
	nvgpu_bug_register_cb(&g->sw_quiesce_bug_cb);

#ifdef CONFIG_NVGPU_RECOVERY
	nvgpu_set_enabled(g, NVGPU_SUPPORT_FAULT_RECOVERY, true);
#else
	nvgpu_set_enabled(g, NVGPU_SUPPORT_FAULT_RECOVERY, false);
#endif

	return 0;
}

void nvgpu_sw_quiesce(struct gk20a *g)
{
	if (g->is_virtual || (g->enabled_flags == NULL) ||
		nvgpu_is_enabled(g, NVGPU_DISABLE_SW_QUIESCE)) {
		nvgpu_err(g, "SW quiesce not supported");
		return;
	}

	if (!g->sw_quiesce_init_done) {
		nvgpu_err(g, "SW quiesce not initialized");
		return;
	}

	if (g->sw_quiesce_pending) {
		nvgpu_err(g, "SW quiesce already pending");
		return;
	}

	nvgpu_err(g, "SW quiesce requested");

	/*
	 * When this flag is set, interrupt handlers should
	 * exit after masking interrupts. This should mitigate
	 * interrupt storm cases.
	 */
	g->sw_quiesce_pending = true;

	nvgpu_cond_signal_interruptible(&g->sw_quiesce_cond);
	nvgpu_start_gpu_idle(g);
	/*
	 * Avoid register accesses when GPU had disappeared
	 * from the bus.
	 */
	if (is_nvgpu_gpu_state_valid(g)) {
		gk20a_mask_interrupts(g);
		nvgpu_fifo_sw_quiesce(g);
	}
}

/* init interface layer support for all falcons */
static int nvgpu_falcons_sw_init(struct gk20a *g)
{
	int err;

	err = g->ops.falcon.falcon_sw_init(g, FALCON_ID_PMU);
	if (err != 0) {
		nvgpu_err(g, "failed to sw init FALCON_ID_PMU");
		return err;
	}

	err = g->ops.falcon.falcon_sw_init(g, FALCON_ID_FECS);
	if (err != 0) {
		nvgpu_err(g, "failed to sw init FALCON_ID_FECS");
		goto done_pmu;
	}

#ifdef CONFIG_NVGPU_DGPU
	err = g->ops.falcon.falcon_sw_init(g, FALCON_ID_SEC2);
	if (err != 0) {
		nvgpu_err(g, "failed to sw init FALCON_ID_SEC2");
		goto done_fecs;
	}

	err = g->ops.falcon.falcon_sw_init(g, FALCON_ID_NVDEC);
	if (err != 0) {
		nvgpu_err(g, "failed to sw init FALCON_ID_NVDEC");
		goto done_sec2;
	}

#endif
	err = g->ops.falcon.falcon_sw_init(g, FALCON_ID_GSPLITE);
	if (err != 0) {
		nvgpu_err(g, "failed to sw init FALCON_ID_GSPLITE");
		goto done_nvdec;
	}

	return 0;

done_nvdec:
#ifdef CONFIG_NVGPU_DGPU
	g->ops.falcon.falcon_sw_free(g, FALCON_ID_NVDEC);
done_sec2:
	g->ops.falcon.falcon_sw_free(g, FALCON_ID_SEC2);
done_fecs:
#endif
	g->ops.falcon.falcon_sw_free(g, FALCON_ID_FECS);
done_pmu:
	g->ops.falcon.falcon_sw_free(g, FALCON_ID_PMU);

	return err;
}

/* handle poweroff and error case for all falcons interface layer support */
static void nvgpu_falcons_sw_free(struct gk20a *g)
{
	g->ops.falcon.falcon_sw_free(g, FALCON_ID_PMU);
	g->ops.falcon.falcon_sw_free(g, FALCON_ID_FECS);

#ifdef CONFIG_NVGPU_DGPU
	g->ops.falcon.falcon_sw_free(g, FALCON_ID_GSPLITE);
	g->ops.falcon.falcon_sw_free(g, FALCON_ID_NVDEC);
	g->ops.falcon.falcon_sw_free(g, FALCON_ID_SEC2);
#endif
}

int nvgpu_prepare_poweroff(struct gk20a *g)
{
	int tmp_ret, ret = 0;

	nvgpu_log_fn(g, " ");

	if (g->ops.channel.suspend_all_serviceable_ch != NULL) {
		ret = g->ops.channel.suspend_all_serviceable_ch(g);
		if (ret != 0) {
			return ret;
		}
	}

#ifdef CONFIG_NVGPU_LS_PMU
	/* disable elpg before gr or fifo suspend */
	if (g->support_ls_pmu) {
		ret = g->ops.pmu.pmu_destroy(g, g->pmu);
	}
#endif

	nvgpu_pmu_enable_irq(g, false);

#ifdef CONFIG_NVGPU_DGPU
	if (nvgpu_is_enabled(g, NVGPU_SUPPORT_SEC2_RTOS)) {
		tmp_ret = g->ops.sec2.sec2_destroy(g);
		if ((tmp_ret != 0) && (ret == 0)) {
			ret = tmp_ret;
		}
	}
#endif
	tmp_ret = g->ops.gr.gr_suspend(g);
	if (tmp_ret != 0) {
		ret = tmp_ret;
	}
	if (g->ops.grmgr.remove_gr_manager != NULL) {
		tmp_ret = g->ops.grmgr.remove_gr_manager(g);
		if (tmp_ret != 0) {
			nvgpu_err(g, "g->ops.grmgr.remove_gr_manager-failed");
			ret = tmp_ret;
		}
	}
	tmp_ret = g->ops.mm.mm_suspend(g);
	if (tmp_ret != 0) {
		ret = tmp_ret;
	}
	tmp_ret = g->ops.fifo.fifo_suspend(g);
	if (tmp_ret != 0) {
		ret = tmp_ret;
	}

#ifndef CONFIG_NVGPU_DGPU
#ifdef CONFIG_NVGPU_GSP_STRESS_TEST
	tmp_ret = nvgpu_gsp_stress_test_halt(g, true);
	if (tmp_ret != 0) {
		ret = tmp_ret;
		nvgpu_err(g, "Failed to halt GSP stress test");
	}
#endif
#ifdef CONFIG_NVGPU_GSP_SCHEDULER
	nvgpu_gsp_sched_suspend(g, g->gsp_sched);
#endif
#endif

	nvgpu_falcons_sw_free(g);

#ifdef CONFIG_NVGPU_DGPU
	g->ops.ce.ce_app_suspend(g);
#endif

#ifdef CONFIG_NVGPU_DGPU
	if (g->ops.bios.bios_sw_deinit != NULL) {
		/* deinit the bios */
		g->ops.bios.bios_sw_deinit(g, g->bios);
	}
#endif

#ifdef CONFIG_NVGPU_HAL_NON_FUSA
	/* Disable GPCPLL */
	if (g->ops.clk.suspend_clk_support != NULL) {
		g->ops.clk.suspend_clk_support(g);
	}
#endif
#ifdef CONFIG_NVGPU_CLK_ARB
	if (g->ops.clk_arb.stop_clk_arb_threads != NULL) {
		g->ops.clk_arb.stop_clk_arb_threads(g);
	}
#endif

	gk20a_mask_interrupts(g);

	/* Disable CIC after the interrupts are masked;
	 * This will ensure that CIC will not get probed
	 * after it's deinit.
	 */
	tmp_ret = nvgpu_cic_mon_deinit(g);
	if (tmp_ret != 0) {
		ret = tmp_ret;
		nvgpu_err(g, "Failed to deinit CIC-mon.");
	}

	return ret;
}

#ifdef CONFIG_NVGPU_STATIC_POWERGATE
static bool have_static_pg_lock = false;

static int nvgpu_init_acquire_static_pg_lock(struct gk20a *g)
{
	nvgpu_mutex_acquire(&g->static_pg_lock);
	have_static_pg_lock = true;
	return 0;
}

static int nvgpu_init_release_static_pg_lock(struct gk20a *g)
{
	nvgpu_mutex_release(&g->static_pg_lock);
	have_static_pg_lock = false;
	return 0;
}
#endif

#ifdef CONFIG_NVGPU_DGPU
static int nvgpu_init_fb_mem_unlock(struct gk20a *g)
{
	int err;

	if ((g->ops.fb.mem_unlock != NULL) && (!g->is_fusa_sku)) {
		err = g->ops.fb.mem_unlock(g);
		if (err != 0) {
			return err;
		}
	} else {
		nvgpu_log_info(g, "skipping fb mem_unlock");
	}

	return 0;
}

static int nvgpu_init_fbpa_ecc(struct gk20a *g)
{
	int err;

	if (g->ops.fb.fbpa_ecc_init != NULL && !g->ecc.initialized) {
		err = g->ops.fb.fbpa_ecc_init(g);
		if (err != 0) {
			return err;
		}
	}

	return 0;
}
#endif

#ifdef CONFIG_NVGPU_STATIC_POWERGATE
static int nvgpu_init_power_gate(struct gk20a *g)
{
	int err;

	/*
	 * Pre-Silicon - Static pg feature related settings
	 * are done in nvgpu driver.
	 * Silicon - Static pg feature related settings
	 * are done in BPMP.
	 */
	if (!(nvgpu_platform_is_silicon(g))) {
		/*
		 * Set the fbp_pg mask. If fbp_pg mask is invalid
		 * halt the GPU poweron.
		 */
		g->can_fbp_pg = false;

		if (g->ops.fbp_pg.init_fbp_pg != NULL) {
			err = g->ops.fbp_pg.init_fbp_pg(g, &g->can_fbp_pg);
			if (err != 0) {
				return err;
			}
		}

		/*
		 * Set the gpc_pg mask. If gpc_pg mask is invalid
		 * halt the GPU poweron.
		 */
		g->can_gpc_pg = false;

		if (g->ops.gpc_pg.init_gpc_pg != NULL) {
			err = g->ops.gpc_pg.init_gpc_pg(g, &g->can_gpc_pg);
			if (err != 0) {
				return err;
			}
		}
	}

	/*
	 * static TPC PG for GV11b is done in NvGPU driver
	 * set the tpc_pg mask. If tpc_pg mask is invalid
	 * halt the GPU poweron.
	 */
	g->can_tpc_pg = false;
	if (g->ops.tpc_pg.init_tpc_pg != NULL) {
		err = g->ops.tpc_pg.init_tpc_pg(g, &g->can_tpc_pg);
		if (err != 0) {
			return err;
		}
	}

	return 0;
}

static int nvgpu_init_power_gate_gr(struct gk20a *g)
{
	/*
	 * Pre-Silicon - Static pg feature related settings
	 * are done in nvgpu driver.
	 * Silicon - Static pg feature related settings
	 * are done in BPMP.
	 */
	if (!(nvgpu_platform_is_silicon(g))) {
		/* powergate FBP as per fbp_pg mask */
		if (g->can_fbp_pg && (g->ops.fbp_pg.fbp_pg != NULL)) {
			g->ops.fbp_pg.fbp_pg(g);
		}

		/* powergate GPC as per gpc_pg mask*/
		if (g->can_gpc_pg && (g->ops.gpc_pg.gpc_pg != NULL)) {
			g->ops.gpc_pg.gpc_pg(g);
		}
	}

	/* powergate TPC as per tpc_pg mask*/
	if (g->can_tpc_pg && (g->ops.tpc_pg.tpc_pg != NULL)) {
		g->ops.tpc_pg.tpc_pg(g);
	}
	return 0;
}
#endif /* CONFIG_NVGPU_STATIC_POWERGATE */

static int nvgpu_init_boot_clk_or_clk_arb(struct gk20a *g)
{
	int err = 0;

	(void)g;
#ifdef CONFIG_NVGPU_LS_PMU
	if (nvgpu_is_enabled(g, NVGPU_PMU_PSTATE) &&
		(g->pmu->fw->ops.clk.clk_set_boot_clk != NULL)) {
		err = g->pmu->fw->ops.clk.clk_set_boot_clk(g);
		if (err != 0) {
			nvgpu_err(g, "failed to set boot clk");
			return err;
		}
	} else
#endif
	{
#ifdef CONFIG_NVGPU_CLK_ARB
		err = g->ops.clk_arb.clk_arb_init_arbiter(g);
		if (err != 0) {
			nvgpu_err(g, "failed to init clk arb");
			return err;
		}
#endif
	}

	return err;
}

static int nvgpu_init_per_device_identifier(struct gk20a *g)
{
	int err = 0;

	if (g->ops.fuse.read_per_device_identifier != NULL) {
		err = g->ops.fuse.read_per_device_identifier(
			g, &g->per_device_identifier);
	}

	return err;
}

static int nvgpu_init_set_debugger_mode(struct gk20a *g)
{
	(void)g;
#ifdef CONFIG_NVGPU_DEBUGGER
	/* Restore the debug setting */
	g->ops.fb.set_debug_mode(g, g->mmu_debug_ctrl);
#endif
	return 0;
}

static int nvgpu_init_xve_set_speed(struct gk20a *g)
{
#ifdef CONFIG_NVGPU_DGPU
	int err;

	if (g->ops.xve.available_speeds != NULL) {
		u32 speed;

		if (!nvgpu_is_enabled(g, NVGPU_SUPPORT_ASPM) &&
				(g->ops.xve.disable_aspm != NULL)) {
			g->ops.xve.disable_aspm(g);
		}

		g->ops.xve.available_speeds(g, &speed);

		/* Set to max speed */
		speed = (u32)nvgpu_fls(speed);

		if (speed > 0U) {
			speed = BIT32((speed - 1U));
		} else {
			speed = BIT32(speed);
		}

		err = g->ops.xve.set_speed(g, speed);
		if (err != 0) {
			nvgpu_err(g, "Failed to set PCIe bus speed!");
			return err;
		}
	}
#else
	(void)g;
#endif
	return 0;
}

static int nvgpu_init_syncpt_mem(struct gk20a *g)
{
#if defined(CONFIG_TEGRA_GK20A_NVHOST)
	int err;
	u64 nr_pages;

	if (nvgpu_has_syncpoints(g) && (g->syncpt_unit_size != 0UL)) {
		if (!nvgpu_mem_is_valid(&g->syncpt_mem)) {
			nr_pages = U64(DIV_ROUND_UP(g->syncpt_unit_size,
						    NVGPU_CPU_PAGE_SIZE));
			err = nvgpu_mem_create_from_phys(g, &g->syncpt_mem,
					g->syncpt_unit_base, nr_pages);
			if (err != 0) {
				nvgpu_err(g, "Failed to create syncpt mem");
				return err;
			}
		}
	}
#endif
	return 0;
}

static int nvgpu_init_slcg_acb_load_gating_prod(struct gk20a *g)
{
	if (g->ops.cg.slcg_acb_load_gating_prod != NULL) {
		g->ops.cg.slcg_acb_load_gating_prod(g, true);
	}

	return 0;
}

static int nvgpu_init_cg_ltc_load_gating_prod(struct gk20a *g)
{
	nvgpu_cg_slcg_ltc_load_enable(g);
	nvgpu_cg_blcg_ltc_load_enable(g);

	return 0;
}

static int nvgpu_init_cg_ctrl_load_gating_prod(struct gk20a *g)
{
	nvgpu_cg_slcg_ctrl_load_enable(g, true);

	return 0;
}

static int nvgpu_ipa_pa_rwsem_init(struct gk20a *g)
{
	nvgpu_rwsem_init(&(g->ipa_pa_cache.ipa_pa_rw_lock));
	return 0;
}

static int nvgpu_init_interrupt_setup(struct gk20a *g)
{
	/**
	 * Disable all interrupts at the start.
	 */
	nvgpu_cic_mon_intr_mask(g);

#ifdef CONFIG_NVGPU_NON_FUSA
	/**
	 * For certain chips like gm20b, there is global interrupt control in
	 * registers mc_intr_en_*_r. Program them here upfront.
	 */
	nvgpu_cic_mon_intr_enable(g);
#endif

	return 0;
}

typedef int (*nvgpu_init_func_t)(struct gk20a *g);
struct nvgpu_init_table_t {
	nvgpu_init_func_t func;
	const char *name;
	u32 enable_flag;
};
#define NVGPU_INIT_TABLE_ENTRY(ops_ptr, enable_flag) \
	{ (ops_ptr), #ops_ptr,  (enable_flag) }
#define NO_FLAG 0U

static bool needs_init(struct gk20a *g, nvgpu_init_func_t func, u32 enable_flag)
{
	return ((enable_flag == NO_FLAG) ||
		nvgpu_is_enabled(g, enable_flag)) && (func != NULL);
}

static int nvgpu_early_init(struct gk20a *g)
{
	int err = 0;
	size_t i;

	/*
	 * This cannot be static because we use the func ptrs as initializers
	 * and static variables require constant literals for initializers.
	 */
	const struct nvgpu_init_table_t nvgpu_early_init_table[] = {
		NVGPU_INIT_TABLE_ENTRY(&nvgpu_init_slcg_acb_load_gating_prod,
					NO_FLAG),
		/*
		 * ECC support initialization is split into generic init
		 * followed by per unit initialization and ends with sysfs
		 * support init. This is done to setup ECC data structures
		 * prior to enabling interrupts for corresponding units.
		 */
		NVGPU_INIT_TABLE_ENTRY(g->ops.ecc.ecc_init_support, NO_FLAG),
		NVGPU_INIT_TABLE_ENTRY(&nvgpu_ipa_pa_rwsem_init, NO_FLAG),
		NVGPU_INIT_TABLE_ENTRY(&nvgpu_device_init, NO_FLAG),
#ifdef CONFIG_NVGPU_DGPU
		NVGPU_INIT_TABLE_ENTRY(g->ops.bios.bios_sw_init, NO_FLAG),
#endif
		NVGPU_INIT_TABLE_ENTRY(&nvgpu_init_interrupt_setup, NO_FLAG),
		NVGPU_INIT_TABLE_ENTRY(g->ops.bus.init_hw, NO_FLAG),
#ifdef CONFIG_NVGPU_STATIC_POWERGATE
		NVGPU_INIT_TABLE_ENTRY(&nvgpu_init_power_gate, NO_FLAG),
		NVGPU_INIT_TABLE_ENTRY(&nvgpu_init_acquire_static_pg_lock, NO_FLAG),
		NVGPU_INIT_TABLE_ENTRY(&nvgpu_init_power_gate_gr, NO_FLAG),
#endif
		NVGPU_INIT_TABLE_ENTRY(g->ops.priv_ring.enable_priv_ring,
				   NO_FLAG),
#ifdef CONFIG_NVGPU_NON_FUSA
		NVGPU_INIT_TABLE_ENTRY(&nvgpu_ptimer_init, NO_FLAG),
#endif
		/* TBD: move this after graphics init in which blcg/slcg is
		 * enabled. This function removes SlowdownOnBoot which applies
		 * 32x divider on gpcpll bypass path. The purpose of slowdown is
		 * to save power during boot but it also significantly slows
		 * down gk20a init on simulation and emulation. We should remove
		 * SOB after graphics power saving features (blcg/slcg) are
		 * enabled. For now, do it here.
		 */
#ifdef CONFIG_NVGPU_HAL_NON_FUSA
		NVGPU_INIT_TABLE_ENTRY(g->ops.clk.init_clk_support, NO_FLAG),
#endif
#ifdef CONFIG_NVGPU_DGPU
		NVGPU_INIT_TABLE_ENTRY(&nvgpu_init_fbpa_ecc, NO_FLAG),
		NVGPU_INIT_TABLE_ENTRY(g->ops.fb.init_fbpa, NO_FLAG),
#endif
		NVGPU_INIT_TABLE_ENTRY(g->ops.fifo.reset_enable_hw, NO_FLAG),
		NVGPU_INIT_TABLE_ENTRY(&nvgpu_init_fb_support, NO_FLAG),
		NVGPU_INIT_TABLE_ENTRY(g->ops.ltc.init_ltc_support, NO_FLAG),
		NVGPU_INIT_TABLE_ENTRY(nvgpu_fbp_init_support, NO_FLAG),
		NVGPU_INIT_TABLE_ENTRY(g->ops.grmgr.init_gr_manager, NO_FLAG),
	};

	for (i = 0; i < ARRAY_SIZE(nvgpu_early_init_table); i++) {
		if (!needs_init(g, nvgpu_early_init_table[i].func,
				nvgpu_early_init_table[i].enable_flag)) {
			nvgpu_log_info(g,
				"Skipping initializing %s (enable_flag=%u func=%p)",
				   nvgpu_early_init_table[i].name,
				   nvgpu_early_init_table[i].enable_flag,
				   nvgpu_early_init_table[i].func);
		} else {
			nvgpu_log_info(g, "Initializing %s",
					   nvgpu_early_init_table[i].name);
			err = nvgpu_early_init_table[i].func(g);
			if (err != 0) {
				nvgpu_err(g, "Failed initialization for: %s",
					  nvgpu_early_init_table[i].name);
				goto done;
			}
		}
	}

done:
	return err;
}

int nvgpu_early_poweron(struct gk20a *g)
{
	int err = 0;

	err = nvgpu_detect_chip(g);
	if (err != 0) {
		nvgpu_err(g, "nvgpu_detect_chip failed[%d]", err);
		goto done;
	}

#ifdef CONFIG_NVGPU_DGPU
	/*
	 * Before probing the GPU make sure the GPU's state is cleared. This is
	 * relevant for rebind operations.
	 */
	if ((g->ops.xve.reset_gpu != NULL) && !g->gpu_reset_done) {
		g->ops.xve.reset_gpu(g);
		g->gpu_reset_done = true;
	}
#endif

	/*
	 * nvgpu poweron sequence split into two stages:
	 * - nvgpu_early_init() - Initializes the sub units
	 *   which are required to be initialized before the grgmr init.
	 *   For creating dev node, grmgr init and its dependency unit
	 *   needs to move to early stage of GPU power on.
	 *   After successful nvgpu_early_init() sequence,
	 *   NvGpu can indetify the number of MIG instance required
	 *   for each physical GPU.
	 * - nvgpu_finalize_poweron() - Initializes the sub units which
	 *   can be initialized at the later stage of GPU power on sequence.
	 *
	 * grmgr init depends on the following HAL sub units,
	 * device - To get the device caps.
	 * priv_ring - To get the gpc count and other MIG config programming.
	 * fifo_reset_hw - In simulation/emulation/GPU standalone platform,
	 *                 XBAR, L2 and HUB are enabled during
	 *                 g->ops.fifo.reset_enable_hw(). This introduces a
	 *                 dependency to get the MIG map conf information.
	 * fb - MIG config programming.
	 * ltc - MIG config programming.
	 * bios, bus, ecc and clk - dependent module of priv_ring/fb/ltc.
	 *
	 */
	err = nvgpu_early_init(g);
	if (err != 0) {
		nvgpu_err(g, "nvgpu_early_init failed[%d]", err);
		goto done;
	}

	/* Initialize CIC early on before the interrupts are
	 * enabled.
	 */
	err = nvgpu_cic_mon_init_lut(g);
	if (err != 0) {
		nvgpu_err(g, "CIC LUT Initialization failed[%d]", err);
		goto done;
	}
done:
	return err;
}

int nvgpu_finalize_poweron(struct gk20a *g)
{
	int err = 0;
	/*
	 * This cannot be static because we use the func ptrs as initializers
	 * and static variables require constant literals for initializers.
	 */
	const struct nvgpu_init_table_t nvgpu_init_table[] = {
		/*
		 * Do this early so any early VMs that get made are capable of
		 * mapping buffers.
		 */
		NVGPU_INIT_TABLE_ENTRY(g->ops.mm.pd_cache_init, NO_FLAG),
		NVGPU_INIT_TABLE_ENTRY(&nvgpu_falcons_sw_init, NO_FLAG),
		NVGPU_INIT_TABLE_ENTRY(g->ops.pmu.pmu_early_init, NO_FLAG),

#ifdef CONFIG_NVGPU_DGPU
		NVGPU_INIT_TABLE_ENTRY(g->ops.sec2.init_sec2_setup_sw,
				       NVGPU_SUPPORT_SEC2_RTOS),
#endif
		NVGPU_INIT_TABLE_ENTRY(g->ops.acr.acr_init,
				       NVGPU_SEC_PRIVSECURITY),
		NVGPU_INIT_TABLE_ENTRY(&nvgpu_sw_quiesce_init_support, NO_FLAG),
#ifdef CONFIG_NVGPU_NVLINK
		NVGPU_INIT_TABLE_ENTRY(g->ops.nvlink.init,
				       NVGPU_SUPPORT_NVLINK),
#endif

#ifdef CONFIG_NVGPU_DEBUGGER
		NVGPU_INIT_TABLE_ENTRY(g->ops.ptimer.config_gr_tick_freq,
				       NO_FLAG),
#endif

#ifdef CONFIG_NVGPU_DGPU
		NVGPU_INIT_TABLE_ENTRY(&nvgpu_init_fb_mem_unlock, NO_FLAG),
#endif

		NVGPU_INIT_TABLE_ENTRY(g->ops.mm.init_mm_support, NO_FLAG),
		NVGPU_INIT_TABLE_ENTRY(g->ops.fifo.fifo_init_support, NO_FLAG),
		NVGPU_INIT_TABLE_ENTRY(nvgpu_nvs_init, NO_FLAG),
		NVGPU_INIT_TABLE_ENTRY(g->ops.therm.elcg_init_idle_filters,
				       NO_FLAG),
		NVGPU_INIT_TABLE_ENTRY(&nvgpu_netlist_init_ctx_vars, NO_FLAG),
		/* prepare portion of sw required for enable hw */
		NVGPU_INIT_TABLE_ENTRY(&nvgpu_gr_alloc, NO_FLAG),
		NVGPU_INIT_TABLE_ENTRY(&nvgpu_gr_enable_hw, NO_FLAG),
		NVGPU_INIT_TABLE_ENTRY(g->ops.acr.acr_construct_execute,
				       NVGPU_SEC_PRIVSECURITY),
		/**
		 * Set ltc_lts_set_mgmt registers only after ACR boot(See
		 * bug200601972 for details). In order to accomplish this
		 * ltc_lts_set_mgmt_setup is decoupled from
		 * nvgpu_init_ltc_support which needs to be executed before ACR
		 * boot.
		 */
		NVGPU_INIT_TABLE_ENTRY(g->ops.ltc.ltc_lts_set_mgmt_setup,
				       NO_FLAG),
		/**
		 * Set atomic mode after acr boot(See Bug 3268664 for
		 * details). For acr to boot, nvgpu_init_fb_support
		 * and init_mm_support is required.
		 * So, set_atomic_mode is decoupled from nvgpu_init_fb_support
		 * in the init sequence and called after acr boot.
		 */
		NVGPU_INIT_TABLE_ENTRY(g->ops.fb.set_atomic_mode, NO_FLAG),

		/**
		 * During acr boot, PLM for ltc clock gating registers
		 * will be lowered for nvgpu(PL0) write access. So,
		 * ltc clock gating programming is done after acr boot.
		 * Bug 3469873
		 */
		NVGPU_INIT_TABLE_ENTRY(&nvgpu_init_cg_ltc_load_gating_prod,
								NO_FLAG),
		/* Load SLCG for CTRL unit */
		NVGPU_INIT_TABLE_ENTRY(&nvgpu_init_cg_ctrl_load_gating_prod,
								NO_FLAG),
#ifdef CONFIG_NVGPU_DGPU
		NVGPU_INIT_TABLE_ENTRY(g->ops.sec2.init_sec2_support,
				       NVGPU_SUPPORT_SEC2_RTOS),
#endif
#ifdef CONFIG_NVGPU_LS_PMU
		NVGPU_INIT_TABLE_ENTRY(g->ops.pmu.pmu_rtos_init, NO_FLAG),
#endif
		NVGPU_INIT_TABLE_ENTRY(g->ops.gr.gr_init_support, NO_FLAG),
		/**
		 * All units requiring ECC stats must initialize ECC counters
		 * before this call to finalize ECC support.
		 */
		NVGPU_INIT_TABLE_ENTRY(g->ops.ecc.ecc_finalize_support,
				       NO_FLAG),
#ifdef CONFIG_NVGPU_STATIC_POWERGATE
		NVGPU_INIT_TABLE_ENTRY(&nvgpu_init_release_static_pg_lock,
				       NO_FLAG),
#endif

#ifdef CONFIG_NVGPU_LS_PMU
		NVGPU_INIT_TABLE_ENTRY(g->ops.pmu.pmu_pstate_sw_setup,
				       NVGPU_PMU_PSTATE),
		NVGPU_INIT_TABLE_ENTRY(g->ops.pmu.pmu_pstate_pmu_setup,
				       NVGPU_PMU_PSTATE),
#endif
		NVGPU_INIT_TABLE_ENTRY(&nvgpu_init_boot_clk_or_clk_arb, NO_FLAG),
		NVGPU_INIT_TABLE_ENTRY(g->ops.therm.init_therm_support, NO_FLAG),
#ifdef CONFIG_NVGPU_COMPRESSION
		NVGPU_INIT_TABLE_ENTRY(g->ops.cbc.cbc_init_support,
				NVGPU_SUPPORT_COMPRESSION),
#endif
		NVGPU_INIT_TABLE_ENTRY(g->ops.chip_init_gpu_characteristics,
				       NO_FLAG),
		NVGPU_INIT_TABLE_ENTRY(&nvgpu_init_per_device_identifier,
				       NO_FLAG),
		NVGPU_INIT_TABLE_ENTRY(&nvgpu_init_set_debugger_mode, NO_FLAG),
		NVGPU_INIT_TABLE_ENTRY(g->ops.ce.ce_init_support, NO_FLAG),
#ifdef CONFIG_NVGPU_DGPU
		NVGPU_INIT_TABLE_ENTRY(g->ops.ce.ce_app_init_support, NO_FLAG),
#endif
		NVGPU_INIT_TABLE_ENTRY(&nvgpu_init_xve_set_speed, NO_FLAG),
		NVGPU_INIT_TABLE_ENTRY(&nvgpu_init_syncpt_mem, NO_FLAG),
#ifdef CONFIG_NVGPU_PROFILER
		NVGPU_INIT_TABLE_ENTRY(&nvgpu_pm_reservation_init, NO_FLAG),
#endif
#ifdef CONFIG_NVGPU_POWER_PG
		NVGPU_INIT_TABLE_ENTRY(g->ops.pmu.pmu_restore_golden_img_state,
				       NO_FLAG),
#endif
		NVGPU_INIT_TABLE_ENTRY(g->ops.channel.resume_all_serviceable_ch,
				       NO_FLAG),
#ifndef CONFIG_NVGPU_DGPU
#ifdef CONFIG_NVGPU_GSP_SCHEDULER
		/* Init gsp ops */
		NVGPU_INIT_TABLE_ENTRY(&nvgpu_gsp_sched_sw_init, NO_FLAG),
#endif
#ifdef CONFIG_NVGPU_GSP_STRESS_TEST
		NVGPU_INIT_TABLE_ENTRY(&nvgpu_gsp_stress_test_sw_init, NO_FLAG),
#endif
#endif
	};
	size_t i;

	nvgpu_log_fn(g, " ");

	for (i = 0; i < ARRAY_SIZE(nvgpu_init_table); i++) {
		if (!needs_init(g, nvgpu_init_table[i].func,
				nvgpu_init_table[i].enable_flag)) {
			nvgpu_log_info(g, "Skipping initializing %s (enable_flag=%u func=%p)",
				       nvgpu_init_table[i].name,
				       nvgpu_init_table[i].enable_flag,
				       nvgpu_init_table[i].func);
		} else {
			nvgpu_log_info(g, "Initializing %s",
				       nvgpu_init_table[i].name);
			err = nvgpu_init_table[i].func(g);
			if (err != 0) {
				nvgpu_err(g, "Failed initialization for: %s",
					  nvgpu_init_table[i].name);
				goto done;
			}
		}
	}

	nvgpu_print_enabled_flags(g);
	return err;

done:
#ifdef CONFIG_NVGPU_STATIC_POWERGATE
	if (have_static_pg_lock) {
		int release_err = nvgpu_init_release_static_pg_lock(g);

		if (release_err != 0) {
			nvgpu_err(g, "failed to release static_pg_lock");
		}
	}
#endif
	nvgpu_falcons_sw_free(g);

	return err;
}

/*
 * Check if the device can go busy. Basically if the driver is currently
 * in the process of dying then do not let new places make the driver busy.
 */
int nvgpu_can_busy(struct gk20a *g)
{
	/* Can't do anything if the system is rebooting/shutting down
	 * or the driver is restarting
	 */

	if (g->sw_quiesce_pending) {
		return 0;
	}

	if (nvgpu_is_enabled(g, NVGPU_KERNEL_IS_DYING) ||
		nvgpu_is_enabled(g, NVGPU_DRIVER_IS_DYING)) {
		return 0;
	} else {
		return 1;
	}
}

int nvgpu_init_gpu_characteristics(struct gk20a *g)
{
#ifdef CONFIG_NVGPU_BUILD_CONFIGURATION_IS_SAFETY
	nvgpu_set_enabled(g, NVGPU_DRIVER_REDUCED_PROFILE, true);
#endif
	nvgpu_set_enabled(g, NVGPU_SUPPORT_MAP_DIRECT_KIND_CTRL, true);
	nvgpu_set_enabled(g, NVGPU_SUPPORT_MAP_BUFFER_BATCH, true);
	nvgpu_set_enabled(g, NVGPU_SUPPORT_SPARSE_ALLOCS, true);
	nvgpu_set_enabled(g, NVGPU_SUPPORT_MAP_ACCESS_TYPE, true);

	/*
	 * Fast submits are supported as long as the user doesn't request
	 * anything that depends on job tracking. (Here, fast means strictly no
	 * metadata, just the gpfifo contents are copied and gp_put updated).
	 */
	nvgpu_set_enabled(g,
			NVGPU_SUPPORT_DETERMINISTIC_SUBMIT_NO_JOBTRACKING,
			true);

	/*
	 * Full deterministic submit means that synchronization (pre and post
	 * fences; implies job tracking) can be used. If such submits can be
	 * guaranteed as long as the channel is set up correctly by userspace
	 * (e.g., watchdog disabled), this bit is set.
	 *
	 * Sync framework is needed when we don't have syncpoint support
	 * because we don't have a means to expose raw gpu semas in a way
	 * similar to raw syncpts. Use of the framework requires unpredictable
	 * actions including deferred job cleanup and wrapping syncs in FDs.
	 *
	 * Aggressive sync destroy causes the channel syncpoint to be abruptly
	 * allocated and deleted during submit path and deferred cleanup.
	 *
	 * Note that userspace expects this to be set for usermode submits
	 * (even if kernel-mode submits aren't enabled where full deterministic
	 * features matter).
	 */
#ifdef CONFIG_NVGPU_KERNEL_MODE_SUBMIT
	if (nvgpu_has_syncpoints(g) &&
			g->aggressive_sync_destroy_thresh == 0U) {
#else
	if (nvgpu_has_syncpoints(g)) {
#endif
		nvgpu_set_enabled(g,
				NVGPU_SUPPORT_DETERMINISTIC_SUBMIT_FULL,
				true);
	}

	nvgpu_set_enabled(g, NVGPU_SUPPORT_TSG, true);

#ifdef CONFIG_NVGPU_CLK_ARB
	if ((g->ops.clk_arb.check_clk_arb_support != NULL) &&
			(nvgpu_is_enabled(g, NVGPU_CLK_ARB_ENABLED))){
		if (g->ops.clk_arb.check_clk_arb_support(g)) {
			nvgpu_set_enabled(g, NVGPU_SUPPORT_CLOCK_CONTROLS,
					true);
		}
	}
#endif

	g->ops.gr.init.detect_sm_arch(g);

#ifdef CONFIG_NVGPU_CYCLESTATS
	if (g->ops.gr.init_cyclestats != NULL) {
		g->ops.gr.init_cyclestats(g);
	}
#endif

	nvgpu_set_enabled(g, NVGPU_SUPPORT_COMPUTE, true);

	if (!nvgpu_is_enabled(g, NVGPU_SUPPORT_MIG)) {
		/*
		 * In MIG mode, 2D, 3D, I2M and ZBC classes are not supported
		 * by GR engine. Default values for legacy mode (non MIG).
		 */
		nvgpu_set_enabled(g, NVGPU_SUPPORT_2D, true);
		nvgpu_set_enabled(g, NVGPU_SUPPORT_3D, true);
		nvgpu_set_enabled(g, NVGPU_SUPPORT_I2M, true);
		nvgpu_set_enabled(g, NVGPU_SUPPORT_ZBC, true);
	} else {
		nvgpu_set_enabled(g, NVGPU_SUPPORT_2D, false);
		nvgpu_set_enabled(g, NVGPU_SUPPORT_3D, false);
		nvgpu_set_enabled(g, NVGPU_SUPPORT_I2M, false);
		nvgpu_set_enabled(g, NVGPU_SUPPORT_ZBC, false);
		nvgpu_set_enabled(g, NVGPU_SUPPORT_ZBC_STENCIL, false);
		nvgpu_set_enabled(g, NVGPU_SUPPORT_PREEMPTION_GFXP, false);
	}

	nvgpu_set_enabled(g, NVGPU_SUPPORT_GET_GR_CONTEXT, true);

	return 0;
}

static struct gk20a *gk20a_from_refcount(struct nvgpu_ref *refcount)
{
	return (struct gk20a *)((uintptr_t)refcount -
				offsetof(struct gk20a, refcount));
}

/*
 * Free the gk20a struct.
 */
static void gk20a_free_cb(struct nvgpu_ref *refcount)
{
	struct gk20a *g = gk20a_from_refcount(refcount);

	nvgpu_log(g, gpu_dbg_shutdown, "Freeing GK20A struct!");

#ifdef CONFIG_NVGPU_DGPU
	if (g->ops.ce.ce_app_destroy != NULL) {
		g->ops.ce.ce_app_destroy(g);
	}
#endif

#ifdef CONFIG_NVGPU_COMPRESSION
	if (g->ops.cbc.cbc_remove_support != NULL) {
		g->ops.cbc.cbc_remove_support(g);
	}
#endif

	if (g->ops.ecc.ecc_remove_support != NULL) {
		g->ops.ecc.ecc_remove_support(g);
	}

#ifdef CONFIG_NVGPU_NON_FUSA
	if (g->remove_support != NULL) {
		g->remove_support(g);
	}
#endif

	if (g->ops.ltc.ltc_remove_support != NULL) {
		g->ops.ltc.ltc_remove_support(g);
	}

	(void)nvgpu_cic_rm_deinit_vars(g);
	(void)nvgpu_cic_mon_remove(g);
	(void)nvgpu_cic_rm_remove(g);

	/*
	 * Free the device list once the gk20a struct is removed. We don't want
	 * to do this during the railgate poweroff sequence since that means
	 * that the device list disappears every time we rail-gate. That will
	 * cause the fifo engine code to explode.
	 */
	nvgpu_device_cleanup(g);

#ifdef CONFIG_NVGPU_PROFILER
	nvgpu_pm_reservation_deinit(g);
#endif

	nvgpu_sw_quiesce_remove_support(g);

	gk20a_debug_deinit(g);

#ifdef CONFIG_NVGPU_NON_FUSA
	if (g->gfree != NULL) {
		g->gfree(g);
	}
#endif
}

struct gk20a *nvgpu_get(struct gk20a *g)
{
	int success;

	/*
	 * Handle the possibility we are still freeing the gk20a struct while
	 * nvgpu_get() is called. Unlikely but plausible race condition. Ideally
	 * the code will never be in such a situation that this race is
	 * possible.
	 */
	success = nvgpu_ref_get_unless_zero(&g->refcount);

	nvgpu_log(g, gpu_dbg_shutdown, "GET: refs currently %d %s",
		nvgpu_atomic_read(&g->refcount.refcount),
			(success != 0) ? "" : "(FAILED)");

	return (success != 0) ? g : NULL;
}

void nvgpu_put(struct gk20a *g)
{
	/*
	 * Note - this is racy, two instances of this could run before the
	 * actual kref_put(0 runs, you could see something like:
	 *
	 *  ... PUT: refs currently 2
	 *  ... PUT: refs currently 2
	 *  ... Freeing GK20A struct!
	 */
	nvgpu_log(g, gpu_dbg_shutdown, "PUT: refs currently %d",
		nvgpu_atomic_read(&g->refcount.refcount));

	nvgpu_ref_put(&g->refcount, gk20a_free_cb);
}

void nvgpu_set_power_state(struct gk20a *g, u32 state)
{
	unsigned long flags = 0U;

	nvgpu_spinlock_irqsave(&g->power_spinlock, flags);
	g->power_on_state = state;
	nvgpu_spinunlock_irqrestore(&g->power_spinlock, flags);
}

const char *nvgpu_get_power_state(struct gk20a *g)
{
	unsigned long flags = 0U;
	const char *str = NULL;
	u32 state;

	nvgpu_spinlock_irqsave(&g->power_spinlock, flags);
	state = g->power_on_state;
	nvgpu_spinunlock_irqrestore(&g->power_spinlock, flags);

	switch (state) {
	case NVGPU_STATE_POWERED_OFF:
		str = "off";
		break;
	case NVGPU_STATE_POWERING_ON:
		str = "powering on";
		break;
	case NVGPU_STATE_POWERED_ON:
		str = "on";
		break;
	default:
		nvgpu_err(g, "Invalid nvgpu power state.");
		break;
	}

	return str;
}

bool nvgpu_poweron_started(struct gk20a *g)
{
	unsigned long flags = 0U;
	u32 power_on;

	nvgpu_spinlock_irqsave(&g->power_spinlock, flags);
	power_on = g->power_on_state;
	nvgpu_spinunlock_irqrestore(&g->power_spinlock, flags);

	return (power_on == NVGPU_STATE_POWERED_ON ||
		power_on == NVGPU_STATE_POWERING_ON);
}

bool nvgpu_is_powered_on(struct gk20a *g)
{
	unsigned long flags = 0U;
	u32 power_on;

	nvgpu_spinlock_irqsave(&g->power_spinlock, flags);
	power_on = g->power_on_state;
	nvgpu_spinunlock_irqrestore(&g->power_spinlock, flags);

	return power_on == NVGPU_STATE_POWERED_ON;
}

bool nvgpu_is_powered_off(struct gk20a *g)
{
	unsigned long flags = 0U;
	u32 power_on;

	nvgpu_spinlock_irqsave(&g->power_spinlock, flags);
	power_on = g->power_on_state;
	nvgpu_spinunlock_irqrestore(&g->power_spinlock, flags);

	return power_on == NVGPU_STATE_POWERED_OFF;
}
