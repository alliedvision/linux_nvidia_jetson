// SPDX-License-Identifier: GPL-2.0-only
//
// tegra210_iqc.c - Tegra210 IQC driver
//
// Copyright (c) 2014-2021 NVIDIA CORPORATION.  All rights reserved.

#include <linux/clk.h>
#include <linux/device.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/regmap.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>

#include "tegra210_ahub.h"
#include "tegra210_iqc.h"
#include "tegra_cif.h"

static const struct reg_default tegra210_iqc_reg_defaults[] = {
	{ TEGRA210_IQC_AXBAR_TX_INT_MASK, 0x0000000f},
	{ TEGRA210_IQC_AXBAR_TX_CIF_CTRL, 0x00007700},
	{ TEGRA210_IQC_CG, 0x1},
	{ TEGRA210_IQC_CTRL, 0x80000020},
};

static int tegra210_iqc_runtime_suspend(struct device *dev)
{
	struct tegra210_iqc *iqc = dev_get_drvdata(dev);

	regcache_cache_only(iqc->regmap, true);
	regcache_mark_dirty(iqc->regmap);

#ifndef CONFIG_MACH_GRENADA
	clk_disable_unprepare(iqc->clk_iqc);
#endif

	return 0;
}

static int tegra210_iqc_runtime_resume(struct device *dev)
{
	struct tegra210_iqc *iqc = dev_get_drvdata(dev);
	int err;

#ifndef CONFIG_MACH_GRENADA
	err = clk_prepare_enable(iqc->clk_iqc);
	if (err) {
		dev_err(dev, "clk_enable failed: %d\n", err);
		return err;
	}
#endif

	regcache_cache_only(iqc->regmap, false);
	regcache_sync(iqc->regmap);

	return 0;
}

static int tegra210_iqc_set_audio_cif(struct tegra210_iqc *iqc,
				struct snd_pcm_hw_params *params,
				unsigned int reg)
{
	int channels, audio_bits;
	struct tegra_cif_conf cif_conf;

	channels = params_channels(params);
	if (channels < 2)
		return -EINVAL;

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		audio_bits = TEGRA_ACIF_BITS_16;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
	case SNDRV_PCM_FORMAT_S32_LE:
		audio_bits = TEGRA_ACIF_BITS_32;
		break;
	default:
		return -EINVAL;
	}

	memset(&cif_conf, 0, sizeof(struct tegra_cif_conf));
	cif_conf.audio_ch = channels;
	cif_conf.client_ch = channels;
	cif_conf.audio_bits = audio_bits;
	cif_conf.client_bits = audio_bits;

	tegra_set_cif(iqc->regmap, reg, &cif_conf);

	return 0;
}

static int tegra210_iqc_hw_params(struct snd_pcm_substream *substream,
				 struct snd_pcm_hw_params *params,
				 struct snd_soc_dai *dai)
{
	struct device *dev = dai->dev;
	struct tegra210_iqc *iqc = snd_soc_dai_get_drvdata(dai);
	int err;

	/* set IQC tx cif */
	err = tegra210_iqc_set_audio_cif(iqc, params,
				TEGRA210_IQC_AXBAR_TX_CIF_CTRL +
				(dai->id * TEGRA210_IQC_AXBAR_TX_STRIDE));
	if (err) {
		dev_err(dev, "Can't set IQC TX CIF: %d\n", err);
		return err;
	}

	/* disable timestamp */
	if (!iqc->timestamp_enable)
		regmap_update_bits(iqc->regmap, TEGRA210_IQC_CTRL,
			TEGRA210_IQC_TIMESTAMP_MASK,
			~(TEGRA210_IQC_TIMESTAMP_EN));

	/* set the IQC data offset */
	if (iqc->data_offset)
		regmap_update_bits(iqc->regmap, TEGRA210_IQC_CTRL,
			TEGRA210_IQC_DATA_OFFSET_MASK,
			iqc->data_offset);

	return err;
}

static struct snd_soc_dai_ops tegra210_iqc_dai_ops = {
	.hw_params	= tegra210_iqc_hw_params,
};

#define IN_DAI(id)						\
	{							\
		.name = "DAP",					\
		.playback = {					\
			.stream_name = "DAP" #id " Receive",	\
			.channels_min = 1,			\
			.channels_max = 16,			\
			.rates = SNDRV_PCM_RATE_8000_96000,	\
			.formats = SNDRV_PCM_FMTBIT_S16_LE,	\
		},						\
	}

#define OUT_DAI(id)						\
	{							\
		.name = "CIF",					\
		.capture = {					\
			.stream_name = "CIF" #id " Transmit",	\
			.channels_min = 1,			\
			.channels_max = 16,			\
			.rates = SNDRV_PCM_RATE_8000_96000,	\
			.formats = SNDRV_PCM_FMTBIT_S16_LE,	\
		},						\
		.ops = &tegra210_iqc_dai_ops,			\
	}

static struct snd_soc_dai_driver tegra210_iqc_dais[] = {
	OUT_DAI(1),
	OUT_DAI(2),
	IN_DAI(1),
	IN_DAI(2),
};

static const struct snd_kcontrol_new tegra210_iqc_controls[] = {
	SOC_SINGLE("IQC Enable", TEGRA210_IQC_ENABLE, 0, 1, 0),
};

static const struct snd_soc_dapm_widget tegra210_iqc_widgets[] = {
	SND_SOC_DAPM_AIF_IN("IQC RX1", NULL, 0, SND_SOC_NOPM,
				0, 0),
	SND_SOC_DAPM_AIF_IN("IQC RX2", NULL, 0, SND_SOC_NOPM,
				0, 0),
	SND_SOC_DAPM_AIF_OUT("IQC TX1", NULL, 0, SND_SOC_NOPM,
				0, 0),
	SND_SOC_DAPM_AIF_OUT("IQC TX2", NULL, 0, SND_SOC_NOPM,
				0, 0),
};

static const struct snd_soc_dapm_route tegra210_iqc_routes[] = {
	{ "IQC RX1",       NULL, "DAP1 Receive" },
	{ "IQC TX1",       NULL, "IQC RX1" },
	{ "CIF1 Transmit", NULL, "IQC TX1" },

	{ "IQC RX2",       NULL, "DAP2 Receive" },
	{ "IQC TX2",       NULL, "IQC RX2" },
	{ "CIF2 Transmit", NULL, "IQC TX2" },
};

static struct snd_soc_component_driver tegra210_iqc_cmpnt = {
	.dapm_widgets = tegra210_iqc_widgets,
	.num_dapm_widgets = ARRAY_SIZE(tegra210_iqc_widgets),
	.dapm_routes = tegra210_iqc_routes,
	.num_dapm_routes = ARRAY_SIZE(tegra210_iqc_routes),
	.controls = tegra210_iqc_controls,
	.num_controls = ARRAY_SIZE(tegra210_iqc_controls),
};

static bool tegra210_iqc_wr_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case TEGRA210_IQC_AXBAR_TX_INT_MASK:
	case TEGRA210_IQC_AXBAR_TX_INT_SET:
	case TEGRA210_IQC_AXBAR_TX_INT_CLEAR:
	case TEGRA210_IQC_AXBAR_TX_CIF_CTRL:
	case TEGRA210_IQC_ENABLE:
	case TEGRA210_IQC_SOFT_RESET:
	case TEGRA210_IQC_CG:
	case TEGRA210_IQC_CTRL:
	case TEGRA210_IQC_CYA:
		return true;
	default:
		return false;
	}
}

static bool tegra210_iqc_rd_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case TEGRA210_IQC_AXBAR_TX_STATUS:
	case TEGRA210_IQC_AXBAR_TX_INT_STATUS:
	case TEGRA210_IQC_AXBAR_TX_INT_MASK:
	case TEGRA210_IQC_AXBAR_TX_INT_SET:
	case TEGRA210_IQC_AXBAR_TX_INT_CLEAR:
	case TEGRA210_IQC_AXBAR_TX_CIF_CTRL:
	case TEGRA210_IQC_ENABLE:
	case TEGRA210_IQC_SOFT_RESET:
	case TEGRA210_IQC_CG:
	case TEGRA210_IQC_STATUS:
	case TEGRA210_IQC_INT_STATUS:
	case TEGRA210_IQC_CTRL:
	case TEGRA210_IQC_TIME_STAMP_STATUS_0:
	case TEGRA210_IQC_TIME_STAMP_STATUS_1:
	case TEGRA210_IQC_WS_EDGE_STATUS:
	case TEGRA210_IQC_CYA:
		return true;
	default:
		return false;
	}
}

static bool tegra210_iqc_volatile_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case TEGRA210_IQC_AXBAR_TX_CIF_CTRL:
	case TEGRA210_IQC_ENABLE:
	case TEGRA210_IQC_CTRL:
		return true;
	default:
		return false;
	}
}

static const struct regmap_config tegra210_iqc_regmap_config = {
	.reg_bits = 32,
	.reg_stride = 4,
	.val_bits = 32,
	.max_register = TEGRA210_IQC_CYA,
	.writeable_reg = tegra210_iqc_wr_reg,
	.readable_reg = tegra210_iqc_rd_reg,
	.volatile_reg = tegra210_iqc_volatile_reg,
	.reg_defaults = tegra210_iqc_reg_defaults,
	.num_reg_defaults = ARRAY_SIZE(tegra210_iqc_reg_defaults),
	.cache_type = REGCACHE_FLAT,
};

static const struct of_device_id tegra210_iqc_of_match[] = {
	{ .compatible = "nvidia,tegra210-iqc" },
	{},
};
MODULE_DEVICE_TABLE(of, tegra210_iqc_of_match);

static int tegra210_iqc_platform_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct tegra210_iqc *iqc;
	void __iomem *regs;
	int err;

	iqc = devm_kzalloc(dev, sizeof(*iqc), GFP_KERNEL);
	if (!iqc)
		return -ENOMEM;

	dev_set_drvdata(dev, iqc);

	iqc->clk_iqc = devm_clk_get(dev, NULL);
	if (IS_ERR(iqc->clk_iqc)) {
		dev_err(dev, "Can't retrieve iqc clock\n");
		return PTR_ERR(iqc->clk_iqc);
	}

	regs = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(regs))
		return PTR_ERR(regs);

	iqc->regmap = devm_regmap_init_mmio(dev, regs,
					    &tegra210_iqc_regmap_config);
	if (IS_ERR(iqc->regmap)) {
		dev_err(dev, "regmap init failed\n");
		return PTR_ERR(iqc->regmap);
	}

	regcache_cache_only(iqc->regmap, true);

	if (of_property_read_u32(dev->of_node,
				 "timestamp-enable",
				 &iqc->timestamp_enable) < 0) {
		dev_dbg(dev, "Missing property timestamp-enable for IQC\n");
		iqc->timestamp_enable = 1;
	}

	if (of_property_read_u32(dev->of_node,
				"data-offset",
				&iqc->data_offset) < 0) {
		dev_dbg(dev, "Missing property data-offset for IQC\n");
		iqc->data_offset = 0;
	}

	err = devm_snd_soc_register_component(dev, &tegra210_iqc_cmpnt,
				     tegra210_iqc_dais,
				     ARRAY_SIZE(tegra210_iqc_dais));
	if (err) {
		dev_err(dev, "can't register IQC component, err: %d\n", err);
		return err;
	}

	pm_runtime_enable(dev);

	return 0;
}

static int tegra210_iqc_platform_remove(struct platform_device *pdev)
{
	pm_runtime_disable(&pdev->dev);

	return 0;
}

static const struct dev_pm_ops tegra210_iqc_pm_ops = {
	SET_RUNTIME_PM_OPS(tegra210_iqc_runtime_suspend,
			   tegra210_iqc_runtime_resume, NULL)
	SET_LATE_SYSTEM_SLEEP_PM_OPS(pm_runtime_force_suspend,
				     pm_runtime_force_resume)
};

static struct platform_driver tegra210_iqc_driver = {
	.driver = {
		.name = "tegra210-iqc",
		.of_match_table = tegra210_iqc_of_match,
		.pm = &tegra210_iqc_pm_ops,
	},
	.probe = tegra210_iqc_platform_probe,
	.remove = tegra210_iqc_platform_remove,
};
module_platform_driver(tegra210_iqc_driver)

MODULE_AUTHOR("Arun S L <aruns@nvidia.com>");
MODULE_DESCRIPTION("Tegra210 IQC ASoC driver");
MODULE_LICENSE("GPL");
