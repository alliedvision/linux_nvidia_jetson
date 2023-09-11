/*
 * Copyright (c) 2016-2023, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/reboot.h>
#include <nvgpu/errata.h>
#include <linux/dma-mapping.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/pm_runtime.h>
#include <linux/of_platform.h>
#include <uapi/linux/nvgpu.h>
#include <linux/devfreq.h>

#include <nvgpu/defaults.h>
#include <nvgpu/kmem.h>
#include <nvgpu/nvgpu_common.h>
#include <nvgpu/soc.h>
#include <nvgpu/bug.h>
#include <nvgpu/enabled.h>
#include <nvgpu/debug.h>
#include <nvgpu/sizes.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/regops.h>
#include <nvgpu/tsg.h>
#include <nvgpu/gr/gr.h>
#include <nvgpu/cic_rm.h>

#include "platform_gk20a.h"
#include "module.h"
#include "os_linux.h"
#include "sysfs.h"
#include "ioctl.h"
#include "scale.h"

#define EMC3D_DEFAULT_RATIO 750

void nvgpu_kernel_restart(void *cmd)
{
	kernel_restart(cmd);
}

void nvgpu_read_support_gpu_tools(struct gk20a *g)
{
	struct device_node *np;
	int ret = 0;
	u32 val = 0;

	np = nvgpu_get_node(g);
	ret = of_property_read_u32(np, "support-gpu-tools", &val);
	if (ret != 0) {
		/* The debugger/profiler support should be enabled by default.
		 * So, set support_gpu_tools to 1 even if the property is missing. */
		g->support_gpu_tools = 1;
		nvgpu_log_info(g, "GPU tools support enabled by default");
	} else {
		if (val != 0U) {
			g->support_gpu_tools = 1;
		} else {
			g->support_gpu_tools = 0;
		}
	}
}

void nvgpu_read_devfreq_timer(struct gk20a *g)
{
	struct device_node *np;
	int ret = 0;
	const char *timer;

	np = nvgpu_get_node(g);
	ret = of_property_read_string(np, "devfreq-timer", &timer);
	if (ret != 0) {
		nvgpu_log_info(g, "GPU devfreq monitor uses default timer");
	} else {
		if (strncmp(timer, "deferrable",
					sizeof("deferrable") - 1) == 0) {
			g->scale_profile->devfreq_profile.timer =
						DEVFREQ_TIMER_DEFERRABLE;
		} else if (strncmp(timer, "delayed",
					sizeof("delayed") - 1) == 0) {
			g->scale_profile->devfreq_profile.timer =
						DEVFREQ_TIMER_DELAYED;
		} else {
			nvgpu_err(g, "dt specified "
				"invalid devfreq timer for GPU: %s", timer);
		}
	}
}

static void nvgpu_init_vars(struct gk20a *g)
{
	struct nvgpu_os_linux *l = nvgpu_os_linux_from_gk20a(g);
	struct device *dev = dev_from_gk20a(g);
	struct gk20a_platform *platform = dev_get_drvdata(dev);

	init_rwsem(&l->busy_lock);
	nvgpu_rwsem_init(&g->deterministic_busy);

	nvgpu_spinlock_init(&g->mc.enable_lock);

	nvgpu_spinlock_init(&g->power_spinlock);

	nvgpu_spinlock_init(&g->mc.intr_lock);

	nvgpu_mutex_init(&platform->railgate_lock);
	nvgpu_mutex_init(&g->dbg_sessions_lock);
	nvgpu_mutex_init(&g->power_lock);
	nvgpu_mutex_init(&g->static_pg_lock);
	nvgpu_mutex_init(&g->clk_arb_enable_lock);
	nvgpu_mutex_init(&g->cg_pg_lock);
#if defined(CONFIG_NVGPU_CYCLESTATS)
	nvgpu_mutex_init(&g->cs_lock);
#endif

	/* Init the clock req count to 0 */
	nvgpu_atomic_set(&g->clk_arb_global_nr, 0);

	/* Atomic set doesn't guarantee a barrier */
	nvgpu_smp_wmb();

	nvgpu_mutex_init(&l->ctrl_privs_lock);
	nvgpu_init_list_node(&l->ctrl_privs);

	g->regs_saved = g->regs;
	g->bar1_saved = g->bar1;

	g->emc3d_ratio = EMC3D_DEFAULT_RATIO;

	/* Set DMA parameters to allow larger sgt lists */
	dev->dma_parms = &l->dma_parms;
	dma_set_max_seg_size(dev, UINT_MAX);

	/*
	 * A default of 16GB is the largest supported DMA size that is
	 * acceptable to all currently supported Tegra SoCs.
	 */
	if (!platform->dma_mask)
		platform->dma_mask = DMA_BIT_MASK(34);

	dma_set_mask(dev, platform->dma_mask);
	dma_set_coherent_mask(dev, platform->dma_mask);
	dma_set_seg_boundary(dev, platform->dma_mask);

	nvgpu_init_list_node(&g->profiler_objects);

	nvgpu_init_list_node(&g->boardobj_head);
	nvgpu_init_list_node(&g->boardobjgrp_head);

	nvgpu_set_enabled(g, NVGPU_HAS_SYNCPOINTS, platform->has_syncpoints);

	nvgpu_set_enabled(g, NVGPU_SUPPORT_NVS, true);
}

static void nvgpu_init_max_comptag(struct gk20a *g)
{
#ifdef CONFIG_NVGPU_COMPRESSION
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 0, 0)
	nvgpu_log_info(g, "total ram pages : %lu", totalram_pages());
#else
	nvgpu_log_info(g, "total ram pages : %lu", totalram_pages);
#endif
	g->max_comptag_mem = totalram_size_in_mb;
#endif
}

static void nvgpu_init_timeout(struct gk20a *g)
{
	struct gk20a_platform *platform = dev_get_drvdata(dev_from_gk20a(g));

	g->timeouts_disabled_by_user = false;
	nvgpu_atomic_set(&g->timeouts_disabled_refcount, 0);

	if (nvgpu_platform_is_silicon(g)) {
		g->poll_timeout_default = NVGPU_DEFAULT_POLL_TIMEOUT_MS;
		g->ch_wdt_init_limit_ms = platform->ch_wdt_init_limit_ms;
	} else if (nvgpu_platform_is_fpga(g)) {
		g->poll_timeout_default = NVGPU_DEFAULT_FPGA_TIMEOUT_MS;
		g->ch_wdt_init_limit_ms = 100U * platform->ch_wdt_init_limit_ms;
	} else {
		g->poll_timeout_default = (u32)ULONG_MAX;
		g->ch_wdt_init_limit_ms = 100U * platform->ch_wdt_init_limit_ms;
	}
	g->ctxsw_timeout_period_ms = CTXSW_TIMEOUT_PERIOD_MS;
}

static void nvgpu_init_timeslice(struct gk20a *g)
{
	g->runlist_interleave = true;

	g->tsg_timeslice_low_priority_us =
			NVGPU_TSG_TIMESLICE_LOW_PRIORITY_US;
	g->tsg_timeslice_medium_priority_us =
			NVGPU_TSG_TIMESLICE_MEDIUM_PRIORITY_US;
	g->tsg_timeslice_high_priority_us =
			NVGPU_TSG_TIMESLICE_HIGH_PRIORITY_US;

	g->tsg_timeslice_min_us = NVGPU_TSG_TIMESLICE_MIN_US;
	g->tsg_timeslice_max_us = NVGPU_TSG_TIMESLICE_MAX_US;
	g->tsg_dbg_timeslice_max_us = NVGPU_TSG_DBG_TIMESLICE_MAX_US_DEFAULT;
}

static void nvgpu_init_pm_vars(struct gk20a *g)
{
	struct device *dev = dev_from_gk20a(g);
	struct gk20a_platform *platform = dev_get_drvdata(dev);

	/*
	 * Set up initial power settings. For non-slicon platforms, disable
	 * power features and for silicon platforms, read from platform data
	 */
	g->slcg_enabled =
		nvgpu_platform_is_silicon(g) ? platform->enable_slcg : false;
	g->blcg_enabled =
		nvgpu_platform_is_silicon(g) ? platform->enable_blcg : false;
	g->elcg_enabled =
		nvgpu_platform_is_silicon(g) ? platform->enable_elcg : false;

	/* disable devfreq for pre-silicon */
	if (!nvgpu_platform_is_silicon(g)) {
		platform->devfreq_governor = NULL;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0)
		platform->qos_min_notify = NULL;
		platform->qos_max_notify = NULL;
#else
		platform->qos_notify = NULL;
#endif
	}

	nvgpu_set_enabled(g, NVGPU_GPU_CAN_ELCG,
		nvgpu_platform_is_silicon(g) ? platform->can_elcg : false);
	nvgpu_set_enabled(g, NVGPU_GPU_CAN_SLCG,
		nvgpu_platform_is_silicon(g) ? platform->can_slcg : false);
	nvgpu_set_enabled(g, NVGPU_GPU_CAN_BLCG,
		nvgpu_platform_is_silicon(g) ? platform->can_blcg : false);

	g->aggressive_sync_destroy_thresh = platform->aggressive_sync_destroy_thresh;
#ifdef CONFIG_NVGPU_SUPPORT_CDE
	g->has_cde = platform->has_cde;
#endif
	g->ptimer_src_freq = platform->ptimer_src_freq;

	if (nvgpu_is_hypervisor_mode(g)) {
		nvgpu_set_enabled(g, NVGPU_CAN_RAILGATE, false);
		platform->can_railgate_init = false;
		/* Disable frequency scaling for hypervisor platforms */
		platform->devfreq_governor = NULL;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0)
		platform->qos_min_notify = NULL;
		platform->qos_max_notify = NULL;
#else
		platform->qos_notify = NULL;
#endif
	} else {
		/* Always enable railgating on simulation platform */
		platform->can_railgate_init = nvgpu_platform_is_simulation(g) ?
			true : platform->can_railgate_init;

		/*
		 * Disable railgating if GPU power domain node is not defined
		 * in the DT as bpmp will not powergate/ungate the GPU on
		 * suspend/resume and can lead to ACR failure on resume
		 * as it expects GPU to be reset on every resume.
		 */
		if (!of_property_read_bool(dev->of_node, "power-domains")) {
			platform->can_railgate_init = false;
		}

		nvgpu_set_enabled(g, NVGPU_CAN_RAILGATE,
				platform->can_railgate_init);
	}
#ifdef CONFIG_NVGPU_STATIC_POWERGATE
	g->can_tpc_pg = platform->can_tpc_pg;
	g->can_gpc_pg = platform->can_gpc_pg;
	g->can_fbp_pg = platform->can_fbp_pg;
#endif
	g->ldiv_slowdown_factor = platform->ldiv_slowdown_factor_init;
	/* if default delay is not set, set default delay to 500msec */
	if (platform->railgate_delay_init)
		g->railgate_delay = platform->railgate_delay_init;
	else
		g->railgate_delay = NVGPU_DEFAULT_RAILGATE_IDLE_TIMEOUT;

	g->support_ls_pmu = support_gk20a_pmu(dev_from_gk20a(g));

	if (g->support_ls_pmu) {
		if (nvgpu_is_hypervisor_mode(g)) {
			g->elpg_enabled = false;
			g->aelpg_enabled = false;
			g->can_elpg = false;
		} else {
			g->elpg_enabled =
				nvgpu_platform_is_silicon(g) ? platform->enable_elpg : false;
			g->aelpg_enabled =
				nvgpu_platform_is_silicon(g) ? platform->enable_aelpg : false;
			g->can_elpg =
				nvgpu_platform_is_silicon(g) ? platform->can_elpg_init : false;
		}
		g->mscg_enabled =
			nvgpu_platform_is_silicon(g) ? platform->enable_mscg : false;
		if (nvgpu_is_enabled(g, NVGPU_SUPPORT_MIG)) {
			g->can_elpg = false;
		}

		nvgpu_set_enabled(g, NVGPU_PMU_PERFMON, platform->enable_perfmon);

		/* ELPG feature enable is SW pre-requisite for ELPG_MS */
		if (g->elpg_enabled) {
			nvgpu_set_enabled(g, NVGPU_ELPG_MS_ENABLED,
						platform->enable_elpg_ms);
			g->elpg_ms_enabled = platform->enable_elpg_ms;
		}
	}

	nvgpu_set_enabled(g, NVGPU_SUPPORT_ASPM, !platform->disable_aspm);
#ifdef CONFIG_NVGPU_SIM
	if (nvgpu_is_enabled(g, NVGPU_IS_FMODEL)) {
		nvgpu_set_enabled(g, NVGPU_PMU_PSTATE, false);
	} else
#endif
	{
		nvgpu_set_enabled(g, NVGPU_PMU_PSTATE, platform->pstate);
	}
}

static void nvgpu_init_vbios_vars(struct gk20a *g)
{
	struct gk20a_platform *platform = dev_get_drvdata(dev_from_gk20a(g));

	nvgpu_set_enabled(g, NVGPU_PMU_RUN_PREOS, platform->run_preos);
}

static void  nvgpu_init_ltc_vars(struct gk20a *g)
{
	struct gk20a_platform *platform = dev_get_drvdata(dev_from_gk20a(g));

	g->ltc_streamid = platform->ltc_streamid;
}

static void nvgpu_init_mm_vars(struct gk20a *g)
{
	struct gk20a_platform *platform = dev_get_drvdata(dev_from_gk20a(g));

	g->mm.disable_bigpage = platform->disable_bigpage;
	nvgpu_set_enabled(g, NVGPU_MM_HONORS_APERTURE,
			    platform->honors_aperture);
	nvgpu_set_enabled(g, NVGPU_MM_UNIFIED_MEMORY,
			    platform->unified_memory);
	nvgpu_set_enabled(g, NVGPU_MM_UNIFY_ADDRESS_SPACES,
			    platform->unify_address_spaces);
	nvgpu_set_errata(g, NVGPU_ERRATA_MM_FORCE_128K_PMU_VM,
			    platform->force_128K_pmu_vm);

	nvgpu_mutex_init(&g->mm.tlb_lock);
}

int nvgpu_probe(struct gk20a *g,
		const char *debugfs_symlink)
{
	struct device *dev = dev_from_gk20a(g);
	struct gk20a_platform *platform = dev_get_drvdata(dev);
	struct nvgpu_os_linux *l = nvgpu_os_linux_from_gk20a(g);
	int err = 0;
	struct device_node *np = dev->of_node;
	bool disable_l3_alloc = false;

	err = nvgpu_cic_rm_setup(g);
	if (err != 0) {
		nvgpu_err(g, "CIC-RM setup failed");
		return err;
	}

	err = nvgpu_cic_rm_init_vars(g);
	if (err != 0) {
		nvgpu_err(g, "CIC-RM init vars failed");
		(void) nvgpu_cic_rm_remove(g);
		return err;
	}

	nvgpu_init_vars(g);
	nvgpu_init_max_comptag(g);
	nvgpu_init_timeout(g);
	nvgpu_init_timeslice(g);
	nvgpu_init_pm_vars(g);
	nvgpu_init_vbios_vars(g);
	nvgpu_init_ltc_vars(g);
	err = nvgpu_init_soc_vars(g);
	if (err) {
		nvgpu_err(g, "init soc vars failed");
		return err;
	}

	/* Initialize the platform interface. */
	err = platform->probe(dev);
	if (err) {
		if (err == -EPROBE_DEFER)
			nvgpu_info(g, "platform probe failed");
		else
			nvgpu_err(g, "platform probe failed");
		return err;
	}

	disable_l3_alloc = of_property_read_bool(np, "disable_l3_alloc");
	if (disable_l3_alloc) {
		nvgpu_log_info(g, "L3 alloc is disabled");
		nvgpu_set_enabled(g, NVGPU_DISABLE_L3_SUPPORT, true);
	}

	nvgpu_init_mm_vars(g);
	err = gk20a_power_node_init(dev);
	if (err) {
		nvgpu_err(g, "power_node creation failed");
		return err;
	}

	/* Read the DT 'support-gpu-tools' property before creating
	 * user nodes (via gk20a_user_nodes_init().
	 */
	nvgpu_read_support_gpu_tools(g);

	/*
	* TODO: While removing the legacy nodes the following condition
	* need to be removed.
	*/
	if (platform->platform_chip_id == TEGRA_210) {
		err = gk20a_user_nodes_init(dev);
		if (err)
			return err;
		l->dev_nodes_created = true;
	}

	/*
	 * Note that for runtime suspend to work the clocks have to be setup
	 * which happens in the probe call above. Hence the driver resume
	 * is done here and not in gk20a_pm_init.
	 */
	pm_runtime_get_sync(dev);

	if (platform->late_probe) {
		err = platform->late_probe(dev);
		if (err) {
			nvgpu_err(g, "late probe failed");
			return err;
		}
	}

	pm_runtime_put_sync_autosuspend(dev);

	nvgpu_create_sysfs(dev);
	gk20a_debug_init(g, debugfs_symlink);

#ifdef CONFIG_NVGPU_DEBUGGER
	g->dbg_regops_tmp_buf = nvgpu_kzalloc(g, SZ_4K);
	if (!g->dbg_regops_tmp_buf) {
		nvgpu_err(g, "couldn't allocate regops tmp buf");
		return -ENOMEM;
	}
	g->dbg_regops_tmp_buf_ops =
		SZ_4K / sizeof(g->dbg_regops_tmp_buf[0]);
#endif

	g->remove_support = gk20a_remove_support;

	nvgpu_ref_init(&g->refcount);

	return 0;
}

static void nvgpu_free_gk20a(struct gk20a *g)
{
	struct nvgpu_os_linux *l = nvgpu_os_linux_from_gk20a(g);

	g->probe_done = false;

	kfree(l);
}

void nvgpu_init_gk20a(struct gk20a *g)
{
	g->gfree = nvgpu_free_gk20a;
}
