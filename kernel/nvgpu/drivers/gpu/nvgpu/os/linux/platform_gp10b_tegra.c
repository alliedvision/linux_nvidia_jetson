/*
 * GP10B Tegra Platform Interface
 *
 * Copyright (c) 2014-2022, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#include <linux/string.h>
#include <linux/clk.h>
#include <linux/of_platform.h>
#include <linux/debugfs.h>
#include <linux/dma-buf.h>
#include <linux/reset.h>
#include <linux/iommu.h>
#include <linux/pm_runtime.h>
#ifdef CONFIG_TEGRA_BWMGR
#include <linux/platform/tegra/emc_bwmgr.h>
#endif
#include <linux/hashtable.h>

#include <uapi/linux/nvgpu.h>

#ifdef CONFIG_NV_TEGRA_BPMP
#include <soc/tegra/tegra-bpmp-dvfs.h>
#endif /* CONFIG_NV_TEGRA_BPMP */

#ifdef CONFIG_NV_TEGRA_MC
#include <dt-bindings/memory/tegra-swgroup.h>
#endif

#include <nvgpu/kmem.h>
#include <nvgpu/bug.h>
#include <nvgpu/enabled.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/nvhost.h>
#include <nvgpu/soc.h>
#include <nvgpu/pmu/pmu_perfmon.h>

#include "os_linux.h"

#include "clk.h"

#include "platform_gk20a.h"
#include "platform_gk20a_tegra.h"
#include "platform_gp10b.h"
#include "scale.h"
#include "module.h"

unsigned long gp10b_freq_table[GP10B_NUM_SUPPORTED_FREQS];
static bool freq_table_init_complete;
static int num_supported_freq;

#define GPCCLK_INIT_RATE 1000000000

struct gk20a_platform_clk tegra_gp10b_clocks[] = {
	{"gpu", GPCCLK_INIT_RATE},
	{"pwr", 204000000},
	{"fuse", UINT_MAX}
};

/*
 * This function finds clocks in tegra platform and populates
 * the clock information to platform data.
 */

int gp10b_tegra_acquire_platform_clocks(struct device *dev,
		struct gk20a_platform_clk *clk_entries,
		unsigned int num_clk_entries)
{
	struct gk20a_platform *platform = dev_get_drvdata(dev);
	struct gk20a *g = platform->g;
	struct device_node *np = nvgpu_get_node(g);
	unsigned int i, num_clks_dt;
	int err = 0;

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
		struct clk *c = of_clk_get_by_name(np, clk_entries[i].name);
		if (IS_ERR(c)) {
			nvgpu_err(g, "cannot get clock %s",
					clk_entries[i].name);
			err = PTR_ERR(c);
			goto err_get_clock;
		}

		rate = clk_entries[i].default_rate;
		clk_set_rate(c, rate);
		platform->clk[i] = c;
	}

	platform->num_clks = i;

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
	while (i--) {
		clk_put(platform->clk[i]);
		platform->clk[i] = NULL;
	}

	nvgpu_mutex_release(&platform->clks_lock);

	return err;
}

int gp10b_tegra_get_clocks(struct device *dev)
{
	return gp10b_tegra_acquire_platform_clocks(dev, tegra_gp10b_clocks,
			ARRAY_SIZE(tegra_gp10b_clocks));
}

void gp10b_tegra_scale_init(struct device *dev)
{
#ifdef CONFIG_TEGRA_BWMGR
	struct gk20a_platform *platform = gk20a_get_platform(dev);
	struct gk20a_scale_profile *profile = platform->g->scale_profile;
	struct tegra_bwmgr_client *bwmgr_handle;

	if (!profile)
		return;

	if ((struct tegra_bwmgr_client *)profile->private_data)
		return;

	bwmgr_handle = tegra_bwmgr_register(TEGRA_BWMGR_CLIENT_GPU);
	if (!bwmgr_handle)
		return;

	profile->private_data = (void *)bwmgr_handle;
#endif
}

void gp10b_tegra_clks_control(struct device *dev, bool enable)
{
	struct gk20a_platform *platform = gk20a_get_platform(dev);
	struct gk20a *g = get_gk20a(dev);
	int err;
	int i;

	nvgpu_mutex_acquire(&platform->clks_lock);

	for (i = 0; i < platform->num_clks; i++) {
		if (!platform->clk[i]) {
			continue;
		}

		if (enable) {
			nvgpu_log(g, gpu_dbg_info,
				  "clk_prepare_enable");
			err = clk_prepare_enable(platform->clk[i]);
			if (err != 0) {
				nvgpu_err(g, "could not turn on clock %d", i);
			}
		} else {
			nvgpu_log(g, gpu_dbg_info,
				  "clk_disable_unprepare");
			clk_disable_unprepare(platform->clk[i]);
		}
	}

	nvgpu_mutex_release(&platform->clks_lock);
}

int gp10b_tegra_reset_assert(struct device *dev)
{
	struct gk20a_platform *platform = gk20a_get_platform(dev);
	int ret = 0;

	if (!platform->reset_control)
		return -EINVAL;

	ret = reset_control_assert(platform->reset_control);

	return ret;
}

int gp10b_tegra_reset_deassert(struct device *dev)
{
	struct gk20a_platform *platform = gk20a_get_platform(dev);
	int ret = 0;

	if (!platform->reset_control)
		return -EINVAL;

	ret = reset_control_deassert(platform->reset_control);

	return ret;
}

void gp10b_tegra_prescale(struct device *dev)
{
	struct gk20a *g = get_gk20a(dev);
	u32 avg = 0;

	nvgpu_log_fn(g, " ");

	nvgpu_pmu_load_norm(g, &avg);

	nvgpu_log_fn(g, "done");
}

void gp10b_tegra_postscale(struct device *pdev,
					unsigned long freq)
{
#ifdef CONFIG_TEGRA_BWMGR
	struct gk20a_platform *platform = gk20a_get_platform(pdev);
	struct gk20a_scale_profile *profile = platform->g->scale_profile;
	struct gk20a *g = get_gk20a(pdev);
	unsigned long emc_rate;

	nvgpu_log_fn(g, " ");
	if (profile && profile->private_data &&
			!platform->is_railgated(pdev)) {
		unsigned long emc_scale;

		if (freq <= gp10b_freq_table[0])
			emc_scale = 0;
		else
			emc_scale = g->emc3d_ratio;

		emc_rate = (freq * EMC_BW_RATIO * emc_scale) / 1000;

		if (emc_rate > tegra_bwmgr_get_max_emc_rate())
			emc_rate = tegra_bwmgr_get_max_emc_rate();

		tegra_bwmgr_set_emc(
			(struct tegra_bwmgr_client *)profile->private_data,
			emc_rate, TEGRA_BWMGR_SET_EMC_FLOOR);
	}
	nvgpu_log_fn(g, "done");
#endif
}

long gp10b_round_clk_rate(struct device *dev, unsigned long rate)
{
	struct gk20a *g = get_gk20a(dev);
	struct gk20a_scale_profile *profile = g->scale_profile;
	unsigned long *freq_table = profile->devfreq_profile.freq_table;
	int max_states = profile->devfreq_profile.max_state;
	int i;

	for (i = 0; i < max_states; ++i)
		if (freq_table[i] >= rate)
			return freq_table[i];

	return freq_table[max_states - 1];
}

int gp10b_clk_get_freqs(struct device *dev,
				unsigned long **freqs, int *num_freqs)
{
	struct gk20a_platform *platform = gk20a_get_platform(dev);
	struct gk20a *g = platform->g;
	unsigned long max_rate;
	unsigned long new_rate = 0, prev_rate = 0;
	int i, freq_counter = 0;
	int sel_freq_cnt;
	unsigned long loc_freq_table[GP10B_MAX_SUPPORTED_FREQS];

	nvgpu_mutex_acquire(&platform->clk_get_freq_lock);

	if (freq_table_init_complete) {

		*freqs = gp10b_freq_table;
		*num_freqs = num_supported_freq;

		nvgpu_mutex_release(&platform->clk_get_freq_lock);

		return 0;
	}

	max_rate = clk_round_rate(platform->clk[0], (UINT_MAX - 1));

	/*
	 * Walk the h/w frequency table and update the local table
	 */
	for (i = 0; i < GP10B_MAX_SUPPORTED_FREQS; ++i) {
		prev_rate = new_rate;
		new_rate = clk_round_rate(platform->clk[0],
						prev_rate + 1);
		loc_freq_table[i] = new_rate;
		if (new_rate == max_rate) {
			++i;
			break;
		}
	}
	/* freq_counter indicates the count of frequencies capped
	 * to GP10B_MAX_SUPPORTED_FREQS or till max_rate is reached.
	*/
	freq_counter = i;

	/*
	 * If the number of achievable frequencies is less than or
	 * equal to GP10B_NUM_SUPPORTED_FREQS, select all frequencies
	 * else, select one out of every 8 frequencies
	 */
	if (freq_counter <= GP10B_NUM_SUPPORTED_FREQS) {
		for (sel_freq_cnt = 0; sel_freq_cnt < freq_counter; ++sel_freq_cnt)
			gp10b_freq_table[sel_freq_cnt] =
					loc_freq_table[sel_freq_cnt];
	} else {
		/*
		 * Walk the h/w frequency table and only select
		 * GP10B_FREQ_SELECT_STEP'th frequencies and
		 * add MAX freq to last
		 */
		sel_freq_cnt = 0;
		for (i = 0; i < GP10B_MAX_SUPPORTED_FREQS &&
				sel_freq_cnt < GP10B_NUM_SUPPORTED_FREQS; ++i) {
			new_rate = loc_freq_table[i];

			if ((i % GP10B_FREQ_SELECT_STEP == 0) ||
					new_rate == max_rate) {
				gp10b_freq_table[sel_freq_cnt++] = new_rate;

				if (new_rate == max_rate)
					break;
			}
		}
	}

	/* Fill freq table */
	*freqs = gp10b_freq_table;
	*num_freqs = sel_freq_cnt;
	num_supported_freq = sel_freq_cnt;

	freq_table_init_complete = true;

	nvgpu_log_info(g, "min rate: %ld max rate: %ld num_of_freq %d\n",
				gp10b_freq_table[0], max_rate, *num_freqs);

	nvgpu_mutex_release(&platform->clk_get_freq_lock);

	return 0;
}
