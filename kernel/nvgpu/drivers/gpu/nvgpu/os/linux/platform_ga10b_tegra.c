/*
 * GA10B Tegra Platform Interface
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
#include <linux/iommu.h>
#include <linux/hashtable.h>
#include <linux/clk.h>
#if defined(CONFIG_INTERCONNECT) && defined(CONFIG_TEGRA_T23X_GRHOST)
#include <linux/platform/tegra/mc_utils.h>
#include <linux/interconnect.h>
#include <dt-bindings/interconnect/tegra_icc_id.h>
#endif
#include <linux/pm_runtime.h>
#include <linux/fuse.h>

#include <nvgpu/gk20a.h>
#include <nvgpu/nvhost.h>
#include <nvgpu/soc.h>
#include <nvgpu/fuse.h>

#ifdef CONFIG_NV_TEGRA_BPMP
#include <soc/tegra/tegra-bpmp-dvfs.h>
#endif /* CONFIG_NV_TEGRA_BPMP */

#ifdef CONFIG_TEGRA_BPMP
#include <soc/tegra/bpmp.h>
#endif /* CONFIG_TEGRA_BPMP */

#include <uapi/linux/nvgpu.h>

#include "os/linux/platform_gk20a.h"
#include "os/linux/clk.h"
#include "os/linux/scale.h"
#include "os/linux/module.h"

#include "os/linux/platform_gp10b.h"

#include "os/linux/os_linux.h"
#include "os/linux/platform_gk20a_tegra.h"

#define EMC3D_GA10B_RATIO 500

/*
 * To-do: clean these defines and include
 * BPMP header.
 * JIRA NVGPU-7124
 */
/** @brief Controls NV_FUSE_CTRL_OPT_GPC */
#define TEGRA234_STRAP_NV_FUSE_CTRL_OPT_GPC		1U
/** @brief Controls NV_FUSE_CTRL_OPT_FBP */
#define TEGRA234_STRAP_NV_FUSE_CTRL_OPT_FBP		2U
/** @brief Controls NV_FUSE_CTRL_OPT_TPC_GPC(0) */
#define TEGRA234_STRAP_NV_FUSE_CTRL_OPT_TPC_GPC0	3U
/** @brief Controls NV_FUSE_CTRL_OPT_TPC_GPC(1) */
#define TEGRA234_STRAP_NV_FUSE_CTRL_OPT_TPC_GPC1	4U

/* Run gpc0, gpc1 and sysclk at same rate */
struct gk20a_platform_clk tegra_ga10b_clocks[] = {
	{"sysclk", UINT_MAX},
	{"gpc0clk", UINT_MAX},
	{"gpc1clk", UINT_MAX},
	{"fuse", UINT_MAX}
};

#define NVGPU_GPC0_DISABLE  BIT(0)
#define NVGPU_GPC1_DISABLE  BIT(1)

#if defined(CONFIG_INTERCONNECT) && defined(CONFIG_TEGRA_T23X_GRHOST)
static int ga10b_tegra_set_emc_rate(struct gk20a_scale_profile *profile,
		unsigned long gpu_rate, unsigned long emc3d_ratio)
{
	unsigned long emc_rate, rate;
	unsigned long peak_bw;
	u32 emc_freq_kbps;

	if (profile && profile->private_data) {

		emc_rate = gpu_rate * EMC_BW_RATIO;
		emc_rate = (emc_rate < gpu_rate) ? ULONG_MAX : emc_rate;
		rate = emc_rate * emc3d_ratio;
		emc_rate = (rate < emc_rate && emc3d_ratio > 0) ? ULONG_MAX : rate;
		emc_rate /= 1000;

		/* peak bandwidth in kilobytes per second  */
		peak_bw = emc_freq_to_bw(emc_rate/1000);
		emc_freq_kbps = (peak_bw > UINT_MAX) ? UINT_MAX : peak_bw;

		return icc_set_bw((struct icc_path *)profile->private_data,
								0, emc_freq_kbps);
	} else {
		/* EMC scaling profile is not available */
		return 0;
	}
}
#endif

static bool ga10b_tegra_is_clock_available(struct gk20a *g, char *clk_name)
{
	u32 gpc_disable = 0U;

	/* Check for gpc floor-sweeping and report availability of gpcclks */
	if (nvgpu_tegra_fuse_read_opt_gpc_disable(g, &gpc_disable)) {
		nvgpu_err(g, "unable to read opt_gpc_disable fuse");
		return false;
	}

	if ((strcmp(clk_name, "gpc0clk") == 0) &&
				((gpc_disable & NVGPU_GPC0_DISABLE) ||
				 (g->gpc_pg_mask & NVGPU_GPC0_DISABLE))) {
		nvgpu_log_info(g, "GPC0 is floor-swept");
		return false;
	}

	if ((strcmp(clk_name, "gpc1clk") == 0) &&
				((gpc_disable & NVGPU_GPC1_DISABLE) ||
				 (g->gpc_pg_mask & NVGPU_GPC1_DISABLE))) {
		nvgpu_log_info(g, "GPC1 is floor-swept");
		return false;
	}

	return true;
}

/*
 * This function finds clocks in tegra platform and populates
 * the clock information to platform data.
 */

static int ga10b_tegra_acquire_platform_clocks(struct device *dev,
		struct gk20a_platform_clk *clk_entries,
		unsigned int num_clk_entries)
{
	struct gk20a_platform *platform = dev_get_drvdata(dev);
	struct gk20a *g = platform->g;
	struct device_node *np = nvgpu_get_node(g);
	unsigned int i, j = 0, num_clks_dt;
	int err;

	/* Get clocks only on supported platforms(silicon and fpga) */
	if (!nvgpu_platform_is_silicon(g) && !nvgpu_platform_is_fpga(g)) {
		return 0;
	}

	num_clks_dt = of_clk_get_parent_count(np);
	if (num_clks_dt > num_clk_entries) {
		nvgpu_err(g, "maximum number of clocks supported is %d",
			num_clk_entries);
		return -EINVAL;
	} else if (num_clks_dt == 0) {
		nvgpu_err(g, "unable to read clocks from DT");
		return -ENODEV;
	}

	nvgpu_mutex_acquire(&platform->clks_lock);

	platform->num_clks = 0;

	for (i = 0; i < num_clks_dt; i++) {
		long rate;
		struct clk *c;

		if (ga10b_tegra_is_clock_available(g, clk_entries[i].name)) {
			c = of_clk_get_by_name(np, clk_entries[i].name);
		} else {
			continue;
		}

		if (IS_ERR(c)) {
			nvgpu_info(g, "cannot get clock %s",
					clk_entries[i].name);
			err = PTR_ERR(c);
			goto err_get_clock;
		} else {
			rate = clk_entries[i].default_rate;
			clk_set_rate(c, rate);

			/*
			 * Note that DT clocks are iterated by index 'i' and
			 * available clocks are iterated by index 'j' in the
			 * platform->clk array.
			 */
			platform->clk[j++] = c;
		}
	}

	platform->num_clks = j;

#ifdef CONFIG_NV_TEGRA_BPMP
	if (platform->clk[0]) {
		i = tegra_bpmp_dvfs_get_clk_id(dev->of_node,
					       clk_entries[0].name);
		if (i > 0)
			platform->maxmin_clk_id = i;
	}
#endif

	nvgpu_mutex_release(&platform->clks_lock);

	return 0;

err_get_clock:
	while (j > 0 && j--) {
		clk_put(platform->clk[j]);
		platform->clk[j] = NULL;
	}

	nvgpu_mutex_release(&platform->clks_lock);

	return err;
}

static int ga10b_tegra_get_clocks(struct device *dev)
{
	return ga10b_tegra_acquire_platform_clocks(dev, tegra_ga10b_clocks,
			ARRAY_SIZE(tegra_ga10b_clocks));
}

void ga10b_tegra_scale_init(struct device *dev)
{
#if defined(CONFIG_INTERCONNECT) && defined(CONFIG_TEGRA_T23X_GRHOST)
	struct gk20a_platform *platform = gk20a_get_platform(dev);
	struct gk20a_scale_profile *profile = platform->g->scale_profile;
	struct icc_path *icc_path_handle;

	if (!profile)
		return;

	platform->g->emc3d_ratio = EMC3D_GA10B_RATIO;

	if ((struct icc_path *)profile->private_data)
		return;

	icc_path_handle = icc_get(dev, TEGRA_ICC_GPU, TEGRA_ICC_PRIMARY);
	if (IS_ERR_OR_NULL(icc_path_handle)) {
		dev_err(dev, "%s unable to get icc path (err=%ld)\n",
				__func__, PTR_ERR(icc_path_handle));
		return;
	}

	profile->private_data = (void *)icc_path_handle;
#endif
}

static void ga10b_tegra_scale_exit(struct device *dev)
{
#if defined(CONFIG_INTERCONNECT) && defined(CONFIG_TEGRA_T23X_GRHOST)
	struct gk20a_platform *platform = gk20a_get_platform(dev);
	struct gk20a_scale_profile *profile = platform->g->scale_profile;

	if (profile && profile->private_data) {
		icc_put((struct icc_path *)profile->private_data);
		profile->private_data = NULL;
	}
#endif
}

static int ga10b_tegra_probe(struct device *dev)
{
	struct gk20a_platform *platform = dev_get_drvdata(dev);
	int err;
	bool joint_xpu_rail = false;
	struct gk20a *g = platform->g;
#ifdef CONFIG_OF
	struct device_node *of_chosen;
#endif

	err = nvgpu_nvhost_syncpt_init(platform->g);
	if (err) {
		if (err != -ENOSYS)
			return err;
	}

	platform->disable_bigpage = !iommu_get_domain_for_dev(dev) &&
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

	err = ga10b_tegra_get_clocks(dev);
	if (err != 0) {
		return err;
	}
	nvgpu_linux_init_clk_support(platform->g);

	nvgpu_mutex_init(&platform->clk_get_freq_lock);

	return 0;
}

static int ga10b_tegra_late_probe(struct device *dev)
{
	struct gk20a_platform *platform = gk20a_get_platform(dev);
	int err;

	err = gk20a_tegra_init_secure_alloc(platform);
	if (err)
		return err;

	return 0;
}

static int ga10b_tegra_remove(struct device *dev)
{
	struct gk20a_platform *platform = gk20a_get_platform(dev);

	ga10b_tegra_scale_exit(dev);

#ifdef CONFIG_TEGRA_GK20A_NVHOST
	nvgpu_free_nvhost_dev(get_gk20a(dev));
#endif

	nvgpu_mutex_destroy(&platform->clk_get_freq_lock);
	nvgpu_mutex_destroy(&platform->clks_lock);

	return 0;
}

#if defined(CONFIG_TEGRA_BPMP) && defined(CONFIG_NVGPU_STATIC_POWERGATE)
int ga10b_tegra_static_pg_control(struct device *dev, struct tegra_bpmp *bpmp,
		struct mrq_strap_request *req)
{
	struct tegra_bpmp_message msg;
	int err;

	memset(&msg, 0, sizeof(msg));
	msg.mrq = MRQ_STRAP;
	msg.tx.data = req;
	msg.tx.size = sizeof(*req);

	err = tegra_bpmp_transfer(bpmp, &msg);
	return err;
}
#endif

static bool ga10b_tegra_is_railgated(struct device *dev)
{
	struct gk20a *g = get_gk20a(dev);
	bool ret = false;

	if (pm_runtime_status_suspended(dev)) {
		ret = true;
	}

	nvgpu_log(g, gpu_dbg_info, "railgated? %s", ret ? "yes" : "no");

	return ret;
}

static int ga10b_tegra_railgate(struct device *dev)
{
#if defined(CONFIG_INTERCONNECT) && defined(CONFIG_TEGRA_T23X_GRHOST)
	struct gk20a_platform *platform = gk20a_get_platform(dev);
	struct gk20a_scale_profile *profile = platform->g->scale_profile;
	int ret = 0;

	/* remove emc frequency floor */
	if (profile && profile->private_data) {
		ret = icc_set_bw((struct icc_path *)profile->private_data,
						0, 0);
		if (ret)
			dev_err(dev, "failed to set emc freq rate:%d\n", ret);
	}
#endif
	gp10b_tegra_clks_control(dev, false);

	return 0;
}

#if defined(CONFIG_TEGRA_BPMP) && defined(CONFIG_NVGPU_STATIC_POWERGATE)
static int ga10b_tegra_bpmp_mrq_set(struct device *dev)
{
	struct tegra_bpmp *bpmp;
	struct mrq_strap_request req;
	struct gk20a *g = get_gk20a(dev);
	u32 i = 0;
	int ret = 0;

	/*
	 * Silicon - Static pg feature fuse settings will done in BPMP
	 * Pre- Silicon - Static pg feature fuse settings will be done
	 * in NvGPU driver during gpu poweron
	 * Send computed GPC, FBP and TPC PG masks to BPMP
	 */
	if (nvgpu_platform_is_silicon(g)) {
		bpmp = tegra_bpmp_get(dev);
		if (!IS_ERR(bpmp)) {
			/* send GPC-PG mask */
			memset(&req, 0, sizeof(req));
			req.cmd = STRAP_SET;
			req.id = TEGRA234_STRAP_NV_FUSE_CTRL_OPT_GPC;
			req.value = g->gpc_pg_mask;
			ret = ga10b_tegra_static_pg_control(dev, bpmp, &req);
			if (ret != 0) {
				nvgpu_err(g, "GPC-PG mask send failed");
				return ret;
			}

			/* send FBP-PG mask */
			memset(&req, 0, sizeof(req));
			req.cmd = STRAP_SET;
			req.id = TEGRA234_STRAP_NV_FUSE_CTRL_OPT_FBP;
			req.value = g->fbp_pg_mask;
			ret = ga10b_tegra_static_pg_control(dev, bpmp, &req);
			if (ret != 0) {
				nvgpu_err(g, "FBP-PG mask send failed");
				return ret;
			}

			/* send TPC-PG mask */
			for (i = 0U; i < MAX_PG_GPC; i++) {
				memset(&req, 0, sizeof(req));
				req.cmd = STRAP_SET;
				req.id = TEGRA234_STRAP_NV_FUSE_CTRL_OPT_TPC_GPC0 + i;
				req.value = g->tpc_pg_mask[i];
				ret = ga10b_tegra_static_pg_control(dev, bpmp, &req);
				if (ret != 0) {
					nvgpu_err(g, "TPC-PG mask send failed "
							"for GPC: %d", i);
					return ret;
				}
			}

			/* re-initialize the clocks */
			ret = ga10b_tegra_get_clocks(dev);
			if (ret != 0) {
				nvgpu_err(g, "get clocks failed ");
				return ret;
			}
		}
	}
	return 0;
}
#endif

static int ga10b_tegra_unrailgate(struct device *dev)
{
	int ret = 0;
#if defined(CONFIG_INTERCONNECT) && defined(CONFIG_TEGRA_T23X_GRHOST)
	struct gk20a_platform *platform = gk20a_get_platform(dev);
	struct gk20a_scale_profile *profile = platform->g->scale_profile;
	unsigned long max_rate;
	long rate;
#endif

#if defined(CONFIG_TEGRA_BPMP)
	ret = ga10b_tegra_bpmp_mrq_set(dev);
	if (ret != 0)
		return ret;
#endif

	/* Setting clk controls */
	gp10b_tegra_clks_control(dev, true);

#if defined(CONFIG_INTERCONNECT) && defined(CONFIG_TEGRA_T23X_GRHOST)
	/* to start with set emc frequency floor for max gpu sys rate*/
	rate = clk_round_rate(platform->clk[0], (UINT_MAX - 1));
	max_rate = (rate < 0) ? ULONG_MAX : (unsigned long)rate;
	ret = ga10b_tegra_set_emc_rate(profile,
					max_rate, platform->g->emc3d_ratio);
	if (ret)
		dev_err(dev, "failed to set emc freq rate:%d\n", ret);
#endif
	return ret;
}

static int ga10b_tegra_suspend(struct device *dev)
{
	return 0;
}

#ifdef CONFIG_NVGPU_STATIC_POWERGATE
static bool ga10b_tegra_is_gpc_fbp_pg_mask_valid(struct gk20a_platform *platform,
					u32 dt_gpc_fbp_pg_mask)
{
	u32 i;
	bool valid = false;

	for (i = 0U; i < MAX_PG_GPC_FBP_CONFIGS; i++) {
		/*
		 * check if gpc/fbp pg mask passed by DT node
		 * is valid gpc/fbp pg mask
		 */
		if (dt_gpc_fbp_pg_mask == platform->valid_gpc_fbp_pg_mask[i]) {
			valid = true;
			break;
		}
	}
	return valid;
}

static int ga10b_tegra_set_gpc_pg_mask(struct device *dev, u32 dt_gpc_pg_mask)
{
	struct gk20a_platform *platform = gk20a_get_platform(dev);
	struct gk20a *g = get_gk20a(dev);

	if (ga10b_tegra_is_gpc_fbp_pg_mask_valid(platform, dt_gpc_pg_mask)) {
		g->gpc_pg_mask = dt_gpc_pg_mask;
#if defined(CONFIG_TEGRA_BPMP)
		return ga10b_tegra_bpmp_mrq_set(dev);
#else
		return 0;
#endif
	}

	nvgpu_err(g, "Invalid GPC-PG mask");
	return -EINVAL;
}

static int ga10b_tegra_set_fbp_pg_mask(struct device *dev, u32 dt_fbp_pg_mask)
{
	struct gk20a_platform *platform = gk20a_get_platform(dev);
	struct gk20a *g = get_gk20a(dev);

	if (ga10b_tegra_is_gpc_fbp_pg_mask_valid(platform, dt_fbp_pg_mask)) {
		g->fbp_pg_mask = dt_fbp_pg_mask;
#if defined(CONFIG_TEGRA_BPMP)
		return ga10b_tegra_bpmp_mrq_set(dev);
#else
		return 0;
#endif
	}

	nvgpu_err(g, "Invalid FBP-PG mask");
	return -EINVAL;
}

void ga10b_tegra_postscale(struct device *pdev,
					unsigned long freq)
{
#if defined(CONFIG_INTERCONNECT) && defined(CONFIG_TEGRA_T23X_GRHOST)
	struct gk20a_platform *platform = gk20a_get_platform(pdev);
	struct gk20a_scale_profile *profile = platform->g->scale_profile;
	struct gk20a *g = get_gk20a(pdev);
	int ret = 0;

	nvgpu_log_fn(g, " ");
	if (profile && profile->private_data &&
			!platform->is_railgated(pdev)) {
		unsigned long emc_scale;

		if (freq <= gp10b_freq_table[0])
			emc_scale = 0;
		else
			emc_scale = g->emc3d_ratio;

		ret = ga10b_tegra_set_emc_rate(profile,
					freq, emc_scale);
		if (ret)
			dev_err(pdev, "failed to set emc freq rate:%d\n", ret);
	}
	nvgpu_log_fn(g, "done");
#endif
}

static void ga10b_tegra_set_valid_tpc_pg_mask(struct gk20a_platform *platform)
{
	u32 i;

	for (i = 0U; i < MAX_PG_TPC_CONFIGS; i++) {
		/*
		 * There are 4 TPCs in each GPC in ga10b
		 * thus, valid tpc pg mask ranges from
		 * 0x0 to 0xF
		 * 0XF will powergate all TPCs, but this
		 * value will be re-checked again as per the
		 * the GPC-PG mask
		 */
		platform->valid_tpc_pg_mask[i] = i;
	}
}

static int ga10b_tegra_set_tpc_pg_mask(struct device *dev, u32 dt_tpc_pg_mask)
{
	struct gk20a *g = get_gk20a(dev);
	struct gk20a_platform *platform = gk20a_get_platform(dev);
	u32 i, j;
	bool pg_status, valid = false;
	/* Hold tpc pg mask value for validation */
	u32 tmp_tpc_pg_mask[MAX_PG_GPC];
	/* Hold user requested combined tpc pg mask value */
	u32 combined_tpc_pg_mask;

	/* first set the valid masks ranges for tpc pg */
	ga10b_tegra_set_valid_tpc_pg_mask(platform);

	/*
	 * if tpc pg mask sent from DT node tries
	 * to powergate all the TPCs in both GPC0 and GPC1
	 * then it is inavlid pg config
	 */
	if (dt_tpc_pg_mask == 0xFF) {
		nvgpu_err(g, "Invalid TPC_PG_MASK:0x%x", dt_tpc_pg_mask);
		return -EINVAL;
	} else {
		/* store dt_tpc_pg_mask in a temp variable */
		combined_tpc_pg_mask = dt_tpc_pg_mask;

		/* separately store tpc pg mask in a temp array */
		for (i = 0U; i < MAX_PG_GPC; i++) {
			tmp_tpc_pg_mask[i] = (combined_tpc_pg_mask >> (4*i)) & 0xFU;
		}

		/* check if the tpc pg mask sent from DT is valid or not */
		for (i = 0U ; i < MAX_PG_GPC; i++) {
			for (j = 0U; j < MAX_PG_TPC_CONFIGS; j++) {
				if (tmp_tpc_pg_mask[i] == platform->valid_tpc_pg_mask[j]) {
					/* store the valid config */
					g->tpc_pg_mask[i] = tmp_tpc_pg_mask[i];
					valid = true;
					break;
				}
			}
			if (valid == false) {
				nvgpu_err(g, "Invalid TPC PG mask: 0x%x",
								tmp_tpc_pg_mask[i]);
				return -EINVAL;
			}
		}
		/*
		 * check if all TPCs of a GPC are powergated
		 * then powergate the corresponding GPC
		 */
		for (i = 0U; i < MAX_PG_GPC; i++) {
			if (g->tpc_pg_mask[i] == 0xFU) {
				g->gpc_pg_mask = (0x1U << i);
			}
		}

		/*
		 * If any one GPC is already floorswept
		 * then all the TPCs in that GPC are floorswept
		 * based on gpc_mask update the tpc_pg_mask
		 */
		switch (g->gpc_pg_mask) {
		case 0x0: /* do nothing as all GPCs are active */
			break;
		case 0x1:
			g->tpc_pg_mask[PG_GPC0] = 0xFU;
			break;
		case 0x2:
			g->tpc_pg_mask[PG_GPC1] = 0xFU;
			break;
		default:
			nvgpu_err(g, "Invalid GPC PG mask: 0x%x",
					 g->gpc_pg_mask);
			return -EINVAL;
		}
	}

	/*
	 * If both GPC0_TPC and GPC1_TPC mask are 0xF
	 * then it is invalid as we cannot powergate
	 * all the TPCs on the chip. This is invalid
	 * configuration
	 */
	for (i = 0U; i < MAX_PG_GPC; i++) {
		if (g->tpc_pg_mask[i] == 0xF) {
			pg_status = true;
		} else {
			pg_status = false;
			break;
		}
	}
	if (pg_status == true) {
		nvgpu_err(g, "Disabling all TPCs isn't allowed!");
		return -EINVAL;
	}
#if defined(CONFIG_TEGRA_BPMP)
	return ga10b_tegra_bpmp_mrq_set(dev);
#else
	return 0;
#endif
}
#endif

struct gk20a_platform ga10b_tegra_platform = {
#ifdef CONFIG_TEGRA_GK20A_NVHOST
	.has_syncpoints = true,
#endif

	/* ptimer src frequency in hz*/
	.ptimer_src_freq = 31250000,

	.ch_wdt_init_limit_ms = 5000,

	.probe = ga10b_tegra_probe,
	.late_probe = ga10b_tegra_late_probe,
	.remove = ga10b_tegra_remove,
	.railgate_delay_init    = 500,
	.can_railgate_init      = true,

#ifdef CONFIG_NVGPU_STATIC_POWERGATE
	/* add tpc powergate JIRA NVGPU-4683 */
	.can_tpc_pg             = false,
	.can_gpc_pg             = false,
	.can_fbp_pg             = false,

	/*
	 * there are 2 GPCs and 2 FBPs
	 * so the valid config to powergate
	 * is 0x0 (all active)
	 * 0x1 (GPC1/FBP1 active)
	 * 0x2 (GPC0/FBP0 active)
	 * 0x3 (both powergated) becomes invalid
	 */
	.valid_gpc_fbp_pg_mask[0]  = 0x0,
	.valid_gpc_fbp_pg_mask[1]  = 0x1,
	.valid_gpc_fbp_pg_mask[2]  = 0x2,

	.set_tpc_pg_mask        = ga10b_tegra_set_tpc_pg_mask,
	.set_gpc_pg_mask        = ga10b_tegra_set_gpc_pg_mask,
	.set_fbp_pg_mask        = ga10b_tegra_set_fbp_pg_mask,
#endif
	.can_slcg               = true,
	.can_blcg               = true,
	.can_elcg               = true,
	.enable_slcg            = true,
	.enable_blcg            = true,
	.enable_elcg            = true,
	.enable_perfmon         = true,

	/* power management configuration  JIRA NVGPU-4683 */
	.enable_elpg            = true,
	.enable_elpg_ms         = false,
	.can_elpg_init          = true,
	.enable_aelpg           = false,

	/* power management callbacks */
	.suspend = ga10b_tegra_suspend,
	.railgate = ga10b_tegra_railgate,
	.unrailgate = ga10b_tegra_unrailgate,
	.is_railgated = ga10b_tegra_is_railgated,

	.busy = gk20a_tegra_busy,
	.idle = gk20a_tegra_idle,

	.clk_round_rate = gp10b_round_clk_rate,
	.get_clk_freqs = gp10b_clk_get_freqs,

	/* frequency scaling configuration */
	.initscale = ga10b_tegra_scale_init,
	.prescale = gp10b_tegra_prescale,
	.postscale = ga10b_tegra_postscale,
	.devfreq_governor = "nvhost_podgov",

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0)
	.qos_min_notify = gk20a_scale_qos_min_notify,
	.qos_max_notify = gk20a_scale_qos_max_notify,
#else
	.qos_notify = gk20a_scale_qos_notify,
#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0) */

	.dump_platform_dependencies = gk20a_tegra_debug_dump,

	.platform_chip_id = TEGRA_234,
	.soc_name = "tegra23x",

	.honors_aperture = true,
	.unified_memory = true,

	/*
	 * This specifies the maximum contiguous size of a DMA mapping to Linux
	 * kernel's DMA framework.
	 * The IOMMU is capable of mapping all of physical memory and hence
	 * dma_mask is set to memory size (512GB in this case).
	 * For iGPU, nvgpu executes own dma allocs (e.g. alloc_page()) and
	 * sg_table construction. No IOMMU mapping is required and so dma_mask
	 * value is not important.
	 * However, for dGPU connected over PCIe through an IOMMU, dma_mask is
	 * significant. In this case, IOMMU bit in GPU physical address is not
	 * relevant.
	 */
	.dma_mask = DMA_BIT_MASK(39),

	.reset_assert = gp10b_tegra_reset_assert,
	.reset_deassert = gp10b_tegra_reset_deassert,

	/*
	 * Size includes total size of ctxsw VPR buffers.
	 * The size can vary for different chips as attribute ctx buffer
	 * size depends on max number of tpcs supported on the chip.
	 */
	.secure_buffer_size = 0x400000, /* 4 MB */
};
