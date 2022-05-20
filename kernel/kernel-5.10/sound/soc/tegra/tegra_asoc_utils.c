// SPDX-License-Identifier: GPL-2.0-only
/*
 * tegra_asoc_utils.c - Harmony machine ASoC driver
 *
 * Author: Stephen Warren <swarren@nvidia.com>
 * Copyright (c) 2010-2021 NVIDIA CORPORATION. All rights reserved.
 */

#include <linux/clk.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>

#include "tegra_asoc_utils.h"

#define MAX(X, Y) ((X > Y) ? (X) : (Y))
/*
 * this will be used for platforms from Tegra210 onwards.
 * odd rates: sample rates multiple of 11.025kHz
 * even_rates: sample rates multiple of 8kHz
 */
enum rate_type {
	ODD_RATE,
	EVEN_RATE,
	NUM_RATE_TYPE,
};
unsigned int tegra210_pll_base_rate[NUM_RATE_TYPE] = {338688000, 368640000};
unsigned int tegra186_pll_stereo_base_rate[NUM_RATE_TYPE] = {270950400, 294912000};
unsigned int default_pll_out_stereo_rate[NUM_RATE_TYPE] = {45158400, 49152000};

int tegra_asoc_utils_set_rate(struct tegra_asoc_utils_data *data, int srate,
			      int mclk)
{
	int new_baseclock;
	bool clk_change;
	int err;

	switch (srate) {
	case 11025:
	case 22050:
	case 44100:
	case 88200:
		if (data->soc == TEGRA_ASOC_UTILS_SOC_TEGRA20)
			new_baseclock = 56448000;
		else if (data->soc == TEGRA_ASOC_UTILS_SOC_TEGRA30)
			new_baseclock = 564480000;
		else
			new_baseclock = 282240000;
		break;
	case 8000:
	case 16000:
	case 24000:
	case 32000:
	case 48000:
	case 64000:
	case 96000:
		if (data->soc == TEGRA_ASOC_UTILS_SOC_TEGRA20)
			new_baseclock = 73728000;
		else if (data->soc == TEGRA_ASOC_UTILS_SOC_TEGRA30)
			new_baseclock = 552960000;
		else
			new_baseclock = 368640000;
		break;
	default:
		return -EINVAL;
	}

	clk_change = ((new_baseclock != data->set_baseclock) ||
			(mclk != data->set_mclk));
	if (!clk_change)
		return 0;

	data->set_baseclock = 0;
	data->set_mclk = 0;

	clk_disable_unprepare(data->clk_cdev1);
	clk_disable_unprepare(data->clk_pll_a_out0);
	clk_disable_unprepare(data->clk_pll_a);

	err = clk_set_rate(data->clk_pll_a, new_baseclock);
	if (err) {
		dev_err(data->dev, "Can't set base pll rate: %d\n", err);
		return err;
	}

	err = clk_set_rate(data->clk_pll_a_out0, mclk);
	if (err) {
		dev_err(data->dev, "Can't set pll_out rate: %d\n", err);
		return err;
	}

	/* Don't set cdev1/extern1 rate; it's locked to pll_out */

	err = clk_prepare_enable(data->clk_pll_a);
	if (err) {
		dev_err(data->dev, "Can't enable pll: %d\n", err);
		return err;
	}

	err = clk_prepare_enable(data->clk_pll_a_out0);
	if (err) {
		dev_err(data->dev, "Can't enable pll_out: %d\n", err);
		return err;
	}

	err = clk_prepare_enable(data->clk_cdev1);
	if (err) {
		dev_err(data->dev, "Can't enable clk_cdev1: %d\n", err);
		return err;
	}

	data->set_baseclock = new_baseclock;
	data->set_mclk = mclk;

	return 0;
}
EXPORT_SYMBOL_GPL(tegra_asoc_utils_set_rate);

int tegra_asoc_utils_set_ac97_rate(struct tegra_asoc_utils_data *data)
{
	const int pll_rate = 73728000;
	const int ac97_rate = 24576000;
	int err;

	clk_disable_unprepare(data->clk_cdev1);
	clk_disable_unprepare(data->clk_pll_a_out0);
	clk_disable_unprepare(data->clk_pll_a);

	/*
	 * AC97 rate is fixed at 24.576MHz and is used for both the host
	 * controller and the external codec
	 */
	err = clk_set_rate(data->clk_pll_a, pll_rate);
	if (err) {
		dev_err(data->dev, "Can't set pll_a rate: %d\n", err);
		return err;
	}

	err = clk_set_rate(data->clk_pll_a_out0, ac97_rate);
	if (err) {
		dev_err(data->dev, "Can't set pll_a_out0 rate: %d\n", err);
		return err;
	}

	/* Don't set cdev1/extern1 rate; it's locked to pll_a_out0 */

	err = clk_prepare_enable(data->clk_pll_a);
	if (err) {
		dev_err(data->dev, "Can't enable pll_a: %d\n", err);
		return err;
	}

	err = clk_prepare_enable(data->clk_pll_a_out0);
	if (err) {
		dev_err(data->dev, "Can't enable pll_a_out0: %d\n", err);
		return err;
	}

	err = clk_prepare_enable(data->clk_cdev1);
	if (err) {
		dev_err(data->dev, "Can't enable cdev1: %d\n", err);
		return err;
	}

	data->set_baseclock = pll_rate;
	data->set_mclk = ac97_rate;

	return 0;
}
EXPORT_SYMBOL_GPL(tegra_asoc_utils_set_ac97_rate);

static int modify_parent_clk_base_rates(unsigned int *new_pll_base,
			unsigned int *pll_out,
			unsigned int req_bclk,
			struct tegra_asoc_utils_data *data)
{
	unsigned int bclk_div, pll_div;
	bool pll_out_halved = false;

	if (req_bclk == 0)
		return 0;

	if (req_bclk > *pll_out)
		return -EOPNOTSUPP;

	if ((*pll_out / req_bclk) > 128) {
		/* reduce pll_out rate to support lower sampling rates */
		*pll_out >>= 1;
		pll_out_halved = true;
	}

	/* Modify base rates on chips >= T186 if fractional dividier is seen */
	if (data->soc >= TEGRA_ASOC_UTILS_SOC_TEGRA186 &&
			(*pll_out % req_bclk)) {

		/* Below logic is added to reduce dynamic range
		 * of PLLA (~37MHz). Min and max plla for chips >= t186
		 * are 258.048 MHz and 294.912 MHz respectively. PLLA dynamic
		 * range is kept minimal to avoid clk ramp up/down issues
		 * and avoid halving plla if already halved
		 */
		if (!pll_out_halved && req_bclk <= (*pll_out >> 1))
			*pll_out >>= 1;

		*new_pll_base = MAX(data->pll_base_rate[EVEN_RATE],
					data->pll_base_rate[ODD_RATE]);

		/* Modifying base rates for i2s parent and grand parent
		 * clocks so that i2s rate can be derived with integer division
		 * as fractional divider is not supported in HW
		 */

		bclk_div =  *pll_out / req_bclk;
		*pll_out = req_bclk * bclk_div;
		pll_div = *new_pll_base / *pll_out;
		*new_pll_base = pll_div * (*pll_out);
		/* TODO: Make sure that the dynamic range is not violated
		 * by having chip specific lower and upper limits of PLLA
		 */
	}
	return 0;
}

int tegra_asoc_utils_set_tegra210_rate(struct tegra_asoc_utils_data *data,
				       unsigned int sample_rate,
				       unsigned int channels,
				       unsigned int sample_size)
{
	unsigned int new_pll_base, pll_out, aud_mclk = 0, req_bclk;
	int err;

	if (data->fixed_pll)
		goto update_mclk_rate;

	switch (sample_rate) {
	case 11025:
	case 22050:
	case 44100:
	case 88200:
	case 176400:
		new_pll_base = data->pll_base_rate[ODD_RATE];
		pll_out = default_pll_out_stereo_rate[ODD_RATE];
		break;
	case 8000:
	case 16000:
	case 24000:
	case 32000:
	case 48000:
	case 64000:
	case 96000:
	case 192000:
		new_pll_base = data->pll_base_rate[EVEN_RATE];
		pll_out = default_pll_out_stereo_rate[EVEN_RATE];
		break;
	default:
		return -EINVAL;
	}

	req_bclk = sample_rate * channels * sample_size;

	err = modify_parent_clk_base_rates(&new_pll_base,
			&pll_out, req_bclk, data);
	if (err) {
		dev_err(data->dev, "Clk rate %d not supported\n",
			req_bclk);
		return err;
	}

	if (data->set_baseclock != new_pll_base) {
		err = clk_set_rate(data->clk_pll_a, new_pll_base);
		if (err) {
			dev_err(data->dev, "Can't set clk_pll_a rate: %d\n",
				err);
			return err;
		}
		data->set_baseclock = new_pll_base;
	}

	if (data->set_pll_out != pll_out) {
		err = clk_set_rate(data->clk_pll_a_out0, pll_out);
		if (err) {
			dev_err(data->dev, "Can't set clk_pll_a_out0 rate: %d\n",
				err);
			return err;
		}
		data->set_pll_out = pll_out;
	}

update_mclk_rate:
	if (data->mclk_fs)
		aud_mclk = sample_rate * data->mclk_fs;

	if (data->set_mclk != aud_mclk) {
		err = clk_set_rate(data->clk_cdev1, aud_mclk);
		if (err) {
			dev_err(data->dev, "Can't set clk_cdev1 rate: %d\n",
				err);
			return err;
		}
		data->set_mclk = aud_mclk;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(tegra_asoc_utils_set_tegra210_rate);

int tegra_asoc_utils_clk_enable(struct tegra_asoc_utils_data *data)
{
	int err;

	err = clk_prepare_enable(data->clk_cdev1);
	if (err) {
		dev_err(data->dev, "Can't enable clock cdev1\n");
		return err;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(tegra_asoc_utils_clk_enable);

void tegra_asoc_utils_clk_disable(struct tegra_asoc_utils_data *data)
{
	clk_disable_unprepare(data->clk_cdev1);
}
EXPORT_SYMBOL_GPL(tegra_asoc_utils_clk_disable);

int tegra_asoc_utils_init(struct tegra_asoc_utils_data *data,
			  struct device *dev)
{
	struct clk *clk_out_1, *clk_extern1;
	int ret;

	data->dev = dev;

	if (of_machine_is_compatible("nvidia,tegra20"))
		data->soc = TEGRA_ASOC_UTILS_SOC_TEGRA20;
	else if (of_machine_is_compatible("nvidia,tegra30"))
		data->soc = TEGRA_ASOC_UTILS_SOC_TEGRA30;
	else if (of_machine_is_compatible("nvidia,tegra114"))
		data->soc = TEGRA_ASOC_UTILS_SOC_TEGRA114;
	else if (of_machine_is_compatible("nvidia,tegra124"))
		data->soc = TEGRA_ASOC_UTILS_SOC_TEGRA124;
	else if (of_machine_is_compatible("nvidia,tegra210"))
		data->soc = TEGRA_ASOC_UTILS_SOC_TEGRA210;
	else if (of_machine_is_compatible("nvidia,tegra186"))
		data->soc = TEGRA_ASOC_UTILS_SOC_TEGRA186;
	else if (of_machine_is_compatible("nvidia,tegra194"))
		data->soc = TEGRA_ASOC_UTILS_SOC_TEGRA194;
	else if (of_machine_is_compatible("nvidia,tegra234"))
		data->soc = TEGRA_ASOC_UTILS_SOC_TEGRA234;
	else {
		dev_err(data->dev, "SoC unknown to Tegra ASoC utils\n");
		return -EINVAL;
	}

	data->clk_pll_a = devm_clk_get(dev, "pll_a");
	if (IS_ERR(data->clk_pll_a)) {
		dev_err(data->dev, "Can't retrieve clk pll_a\n");
		return PTR_ERR(data->clk_pll_a);
	}

	data->clk_pll_a_out0 = devm_clk_get(dev, "pll_a_out0");
	if (IS_ERR(data->clk_pll_a_out0)) {
		dev_err(data->dev, "Can't retrieve clk pll_a_out0\n");
		return PTR_ERR(data->clk_pll_a_out0);
	}

	/* FIXME: data->clk_cdev1 = devm_clk_get_optional(dev, "mclk"); */
	data->clk_cdev1 = devm_clk_get_optional(dev, "extern1");
	if (IS_ERR(data->clk_cdev1)) {
		dev_err(data->dev, "Can't retrieve clk cdev1\n");
		return PTR_ERR(data->clk_cdev1);
	}

	if (data->soc < TEGRA_ASOC_UTILS_SOC_TEGRA210) {
		ret = tegra_asoc_utils_set_rate(data, 44100, 256 * 44100);
		if (ret)
			return ret;
	}

	if (data->soc < TEGRA_ASOC_UTILS_SOC_TEGRA186)
		data->pll_base_rate = tegra210_pll_base_rate;
	else
		data->pll_base_rate = tegra186_pll_stereo_base_rate;
	/*
	 * If clock parents are not set in DT, configure here to use clk_out_1
	 * as mclk and extern1 as parent for Tegra30 and higher.
	 */
	if (!of_find_property(dev->of_node, "assigned-clock-parents", NULL) &&
	    data->soc > TEGRA_ASOC_UTILS_SOC_TEGRA20) {
		dev_warn(data->dev,
			 "Configuring clocks for a legacy device-tree\n");
		dev_warn(data->dev,
			 "Please update DT to use assigned-clock-parents\n");
		clk_extern1 = devm_clk_get_optional(dev, "extern1");
		if (IS_ERR(clk_extern1)) {
			dev_err(data->dev, "Can't retrieve clk extern1\n");
			return PTR_ERR(clk_extern1);
		}

		ret = clk_set_parent(clk_extern1, data->clk_pll_a_out0);
		if (ret < 0) {
			dev_err(data->dev,
				"Set parent failed for clk extern1\n");
			return ret;
		}

		clk_out_1 = devm_clk_get(dev, "pmc_clk_out_1");
		if (IS_ERR(clk_out_1)) {
			dev_err(data->dev, "Can't retrieve pmc_clk_out_1\n");
			return PTR_ERR(clk_out_1);
		}

		ret = clk_set_parent(clk_out_1, clk_extern1);
		if (ret < 0) {
			dev_err(data->dev,
				"Set parent failed for pmc_clk_out_1\n");
			return ret;
		}

		data->clk_cdev1 = clk_out_1;
	}

	/*
	 * FIXME: There is some unknown dependency between audio mclk disable
	 * and suspend-resume functionality on Tegra30, although audio mclk is
	 * only needed for audio.
	 */
	ret = clk_prepare_enable(data->clk_cdev1);
	if (ret) {
		dev_err(data->dev, "Can't enable cdev1: %d\n", ret);
		return ret;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(tegra_asoc_utils_init);

MODULE_AUTHOR("Stephen Warren <swarren@nvidia.com>");
MODULE_DESCRIPTION("Tegra ASoC utility code");
MODULE_LICENSE("GPL");
