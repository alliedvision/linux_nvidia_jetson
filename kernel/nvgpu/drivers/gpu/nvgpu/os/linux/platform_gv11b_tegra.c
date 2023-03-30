/*
 * GV11B Tegra Platform Interface
 *
 * Copyright (c) 2016-2022, NVIDIA CORPORATION.  All rights reserved.
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

#include <linux/of_platform.h>
#include <linux/debugfs.h>
#include <linux/dma-buf.h>
#include <linux/reset.h>
#include <linux/hashtable.h>
#include <linux/clk.h>
#include <linux/iommu.h>
#include <linux/pm_runtime.h>
#ifdef CONFIG_TEGRA_BWMGR
#include <linux/platform/tegra/emc_bwmgr.h>
#endif

#include <nvgpu/gk20a.h>
#include <nvgpu/nvhost.h>

#include <uapi/linux/nvgpu.h>

#include "platform_gk20a.h"
#include "clk.h"
#include "scale.h"

#include "platform_gp10b.h"

#include "os_linux.h"
#include "platform_gk20a_tegra.h"

#define EMC3D_GV11B_RATIO 500

void gv11b_tegra_scale_init(struct device *dev)
{
#ifdef CONFIG_TEGRA_BWMGR
	struct gk20a_platform *platform = gk20a_get_platform(dev);
	struct gk20a_scale_profile *profile = platform->g->scale_profile;

	if (!profile)
		return;

	platform->g->emc3d_ratio = EMC3D_GV11B_RATIO;

	gp10b_tegra_scale_init(dev);
#endif
}

static void gv11b_tegra_scale_exit(struct device *dev)
{
#ifdef CONFIG_TEGRA_BWMGR
	struct gk20a_platform *platform = gk20a_get_platform(dev);
	struct gk20a_scale_profile *profile = platform->g->scale_profile;

	if (profile)
		tegra_bwmgr_unregister(
			(struct tegra_bwmgr_client *)profile->private_data);
#endif
}

static int gv11b_tegra_probe(struct device *dev)
{
	struct gk20a_platform *platform = dev_get_drvdata(dev);
	struct device_node *of_chosen;
	int err;
	bool joint_xpu_rail = false;
	struct gk20a *g = platform->g;

#ifdef CONFIG_TEGRA_GK20A_NVHOST
	err = nvgpu_nvhost_syncpt_init(platform->g);
	if (err) {
		if (err != -ENOSYS)
			return err;
	}
#endif

	platform->disable_bigpage = !(iommu_get_domain_for_dev(dev)) &&
					(NVGPU_CPU_PAGE_SIZE < SZ_64K);

#ifdef CONFIG_OF
	of_chosen = of_find_node_by_path("/chosen");
	if (!of_chosen)
		return -ENODEV;

	joint_xpu_rail = of_property_read_bool(of_chosen,
				"nvidia,tegra-joint_xpu_rail");
#endif

	if (joint_xpu_rail) {
		nvgpu_log_info(g, "XPU rails are joint\n");
		platform->can_railgate_init = false;
		nvgpu_set_enabled(g, NVGPU_CAN_RAILGATE, false);
	}

	nvgpu_mutex_init(&platform->clks_lock);

	err = gp10b_tegra_get_clocks(dev);
	if (err != 0) {
		return err;
	}
	nvgpu_linux_init_clk_support(platform->g);

	nvgpu_mutex_init(&platform->clk_get_freq_lock);

	return 0;
}

static int gv11b_tegra_late_probe(struct device *dev)
{
	struct gk20a_platform *platform = dev_get_drvdata(dev);
	int err;

	err = gk20a_tegra_init_secure_alloc(platform);
	if (err)
		return err;

	return 0;
}


static int gv11b_tegra_remove(struct device *dev)
{
	struct gk20a_platform *platform = gk20a_get_platform(dev);

	gv11b_tegra_scale_exit(dev);

#ifdef CONFIG_TEGRA_GK20A_NVHOST
	nvgpu_free_nvhost_dev(get_gk20a(dev));
#endif

	nvgpu_mutex_destroy(&platform->clk_get_freq_lock);
	nvgpu_mutex_destroy(&platform->clks_lock);

	return 0;
}

static bool gv11b_tegra_is_railgated(struct device *dev)
{
	struct gk20a *g = get_gk20a(dev);
	bool ret = false;

	if (pm_runtime_status_suspended(dev)) {
		ret = true;
	}

	nvgpu_log(g, gpu_dbg_info, "railgated? %s", ret ? "yes" : "no");

	return ret;
}

static int gv11b_tegra_railgate(struct device *dev)
{
#ifdef TEGRA194_POWER_DOMAIN_GPU
	struct gk20a *g = get_gk20a(dev);
#endif

#ifdef CONFIG_TEGRA_BWMGR
	struct gk20a_platform *platform = gk20a_get_platform(dev);
	struct gk20a_scale_profile *profile = platform->g->scale_profile;

	/* remove emc frequency floor */
	if (profile)
		tegra_bwmgr_set_emc(
			(struct tegra_bwmgr_client *)profile->private_data,
			0, TEGRA_BWMGR_SET_EMC_FLOOR);
#endif /* CONFIG_TEGRA_BWMGR */

	gp10b_tegra_clks_control(dev, false);

	return 0;
}

static int gv11b_tegra_unrailgate(struct device *dev)
{
	int ret = 0;
#ifdef CONFIG_TEGRA_BWMGR
	struct gk20a_platform *platform = gk20a_get_platform(dev);
	struct gk20a_scale_profile *profile = platform->g->scale_profile;
#endif

	gp10b_tegra_clks_control(dev, true);

#ifdef CONFIG_TEGRA_BWMGR
	/* to start with set emc frequency floor to max rate*/
	if (profile)
		tegra_bwmgr_set_emc(
			(struct tegra_bwmgr_client *)profile->private_data,
			tegra_bwmgr_get_max_emc_rate(),
			TEGRA_BWMGR_SET_EMC_FLOOR);
#endif

	return ret;
}

static int gv11b_tegra_suspend(struct device *dev)
{
	return 0;
}

static bool gv11b_tegra_is_tpc_mask_valid(struct gk20a_platform *platform,
						u32 dt_tpc_pg_mask)
{
	u32 i;
	bool valid = false;

	for (i = 0U; i < MAX_PG_TPC_CONFIGS; i++) {
		if (dt_tpc_pg_mask == platform->valid_tpc_pg_mask[i]) {
			valid = true;
			break;
		}
	}
	return valid;
}

static int gv11b_tegra_set_tpc_pg_mask(struct device *dev, u32 dt_tpc_pg_mask)
{
	struct gk20a_platform *platform = gk20a_get_platform(dev);
	struct gk20a *g = get_gk20a(dev);

	if (gv11b_tegra_is_tpc_mask_valid(platform, dt_tpc_pg_mask)) {
		g->tpc_pg_mask[PG_GPC0] = dt_tpc_pg_mask;
		return 0;
	} else {
		nvgpu_err(g, "Invalid TPC-PG mask");
		return -EINVAL;
	}
}

struct gk20a_platform gv11b_tegra_platform = {
#ifdef CONFIG_TEGRA_GK20A_NVHOST
	.has_syncpoints = true,
#endif

	/* ptimer src frequency in hz*/
	.ptimer_src_freq	= 31250000,

	.ch_wdt_init_limit_ms = 5000,

	.probe = gv11b_tegra_probe,
	.late_probe = gv11b_tegra_late_probe,
	.remove = gv11b_tegra_remove,
	.railgate_delay_init    = 500,

	.can_railgate_init      = true,
#ifdef CONFIG_NVGPU_STATIC_POWERGATE
	.can_tpc_pg                = true,
	.valid_tpc_pg_mask[0]      = 0x0,
	.valid_tpc_pg_mask[1]      = 0x1,
	.valid_tpc_pg_mask[2]      = 0x2,
	.valid_tpc_pg_mask[3]      = 0x4,
	.valid_tpc_pg_mask[4]      = 0x8,
	.valid_tpc_pg_mask[5]      = 0x5,
	.valid_tpc_pg_mask[6]      = 0x6,
	.valid_tpc_pg_mask[7]      = 0x9,
	.valid_tpc_pg_mask[8]      = 0xa,

	.set_tpc_pg_mask	= gv11b_tegra_set_tpc_pg_mask,
#endif
	.can_slcg               = true,
	.can_blcg               = true,
	.can_elcg               = true,
	.enable_slcg            = true,
	.enable_blcg            = true,
	.enable_elcg            = true,
	.enable_perfmon         = true,

	/* power management configuration */
	.enable_elpg		= true,
	.enable_elpg_ms		= false,
	.can_elpg_init		= true,
	.enable_aelpg           = true,

	/* power management callbacks */
	.suspend = gv11b_tegra_suspend,
	.railgate = gv11b_tegra_railgate,
	.unrailgate = gv11b_tegra_unrailgate,
	.is_railgated = gv11b_tegra_is_railgated,

	.busy = gk20a_tegra_busy,
	.idle = gk20a_tegra_idle,

	.clk_round_rate = gp10b_round_clk_rate,
	.get_clk_freqs = gp10b_clk_get_freqs,

	/* frequency scaling configuration */
	.initscale = gv11b_tegra_scale_init,
	.prescale = gp10b_tegra_prescale,
	.postscale = gp10b_tegra_postscale,
	.devfreq_governor = "nvhost_podgov",

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0)
	.qos_min_notify = gk20a_scale_qos_min_notify,
	.qos_max_notify = gk20a_scale_qos_max_notify,
#else
	.qos_notify = gk20a_scale_qos_notify,
#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0) */

	.dump_platform_dependencies = gk20a_tegra_debug_dump,

	.platform_chip_id = TEGRA_194,
	.soc_name = "tegra19x",

	.honors_aperture = true,
	.unified_memory = true,
	.dma_mask = DMA_BIT_MASK(38),

	.reset_assert = gp10b_tegra_reset_assert,
	.reset_deassert = gp10b_tegra_reset_deassert,

	.secure_buffer_size = 667648,
};
