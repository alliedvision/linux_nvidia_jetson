// SPDX-License-Identifier: GPL-2.0-only
//
// tegra_audio_graph_card.c - Audio Graph based Tegra Machine Driver
//
// Copyright (c) 2020-2021 NVIDIA CORPORATION.  All rights reserved.

#include <linux/math64.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <sound/graph_card.h>
#include <sound/pcm_params.h>

#include "tegra_asoc_machine.h"
#include "tegra_codecs.h"

#define MAX_PLLA_OUT0_DIV 128

#define simple_to_tegra_priv(simple) \
		container_of(simple, struct tegra_audio_priv, simple)

enum srate_type {
	/*
	 * Sample rates multiple of 8000 Hz and below are supported:
	 * ( 8000, 16000, 32000, 48000, 96000, 192000 Hz )
	 */
	x8_RATE,

	/*
	 * Sample rates multiple of 11025 Hz and below are supported:
	 * ( 11025, 22050, 44100, 88200, 176400 Hz )
	 */
	x11_RATE,

	NUM_RATE_TYPE,
};

struct tegra_audio_priv {
	struct asoc_simple_priv simple;
	struct clk *clk_plla_out0;
	struct clk *clk_plla;
};

/* Tegra audio chip data */
struct tegra_audio_cdata {
	unsigned int plla_rates[NUM_RATE_TYPE];
	unsigned int plla_out0_rates[NUM_RATE_TYPE];
};

/* Setup PLL clock as per the given sample rate */
static int tegra_audio_graph_update_pll(struct snd_pcm_substream *substream,
					struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = asoc_substream_to_rtd(substream);
	struct asoc_simple_priv *simple = snd_soc_card_get_drvdata(rtd->card);
	struct tegra_audio_priv *priv = simple_to_tegra_priv(simple);
	struct device *dev = rtd->card->dev;
	const struct tegra_audio_cdata *data = of_device_get_match_data(dev);
	unsigned int plla_rate, plla_out0_rate, bclk;
	unsigned int srate = params_rate(params);
	int err;

	switch (srate) {
	case 11025:
	case 22050:
	case 44100:
	case 88200:
	case 176400:
		plla_out0_rate = data->plla_out0_rates[x11_RATE];
		plla_rate = data->plla_rates[x11_RATE];
		break;
	case 8000:
	case 16000:
	case 24000:
	case 32000:
	case 48000:
	case 64000:
	case 96000:
	case 192000:
		plla_out0_rate = data->plla_out0_rates[x8_RATE];
		plla_rate = data->plla_rates[x8_RATE];
		break;
	default:
		dev_err(rtd->card->dev, "Unsupported sample rate %u\n",
			srate);
		return -EINVAL;
	}

	/*
	 * Below is the clock relation:
	 *
	 *	PLLA
	 *	  |
	 *	  |--> PLLA_OUT0
	 *		  |
	 *		  |---> I2S modules
	 *		  |
	 *		  |---> DMIC modules
	 *		  |
	 *		  |---> DSPK modules
	 *
	 *
	 * Default PLLA_OUT0 rate might be too high when I/O is running
	 * at minimum PCM configurations. This may result in incorrect
	 * clock rates and glitchy audio. The maximum divider is 128
	 * and any thing higher than that won't work. Thus reduce PLLA_OUT0
	 * to work for lower configurations.
	 *
	 * This problem is seen for I2S only, as DMIC and DSPK minimum
	 * clock requirements are under allowed divider limits.
	 */
	bclk = srate * params_channels(params) * params_width(params);
	if (div_u64(plla_out0_rate, bclk) > MAX_PLLA_OUT0_DIV)
		plla_out0_rate >>= 1;

	dev_dbg(rtd->card->dev,
		"Update clock rates: PLLA(= %u Hz) and PLLA_OUT0(= %u Hz)\n",
		plla_rate, plla_out0_rate);

	/* Set PLLA rate */
	err = clk_set_rate(priv->clk_plla, plla_rate);
	if (err) {
		dev_err(rtd->card->dev,
			"Can't set plla rate for %u, err: %d\n",
			plla_rate, err);
		return err;
	}

	/* Set PLLA_OUT0 rate */
	err = clk_set_rate(priv->clk_plla_out0, plla_out0_rate);
	if (err) {
		dev_err(rtd->card->dev,
			"Can't set plla_out0 rate %u, err: %d\n",
			plla_out0_rate, err);
		return err;
	}

	return err;
}

static int tegra_audio_graph_hw_params(struct snd_pcm_substream *substream,
				       struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = asoc_substream_to_rtd(substream);
	struct asoc_simple_priv *priv = snd_soc_card_get_drvdata(rtd->card);
	struct simple_dai_props *dai_props =
		simple_priv_to_props(priv, rtd->num);
	struct snd_soc_card *card = rtd->card;
	struct snd_soc_pcm_runtime *runtime;
	struct snd_soc_pcm_stream *dai_params;
	int err;

	/*
	 * This gets called for each DAI link (FE or BE) when DPCM is used.
	 * We may not want to update PLLA rate for each call. So PLLA update
	 * must be restricted to external I/O links (I2S, DMIC or DSPK) since
	 * they actually depend on it. I/O modules update their clocks in
	 * hw_param() of their respective component driver and PLLA rate
	 * update here helps them to derive appropriate rates.
	 *
	 * TODO: When more HW accelerators get added (like sample rate
	 * converter, volume gain controller etc., which don't really
	 * depend on PLLA) we need a better way to filter here.
	 */
#if IS_ENABLED(TEGRA_DPCM)
	struct snd_soc_dai *cpu_dai = asoc_rtd_to_cpu(rtd, 0);

	if (cpu_dai->driver->ops && rtd->dai_link->no_pcm) {
		err = tegra_audio_graph_update_pll(substream, params);
		if (err)
			return err;
	}
#else
	err = tegra_audio_graph_update_pll(substream, params);
	if (err)
		return err;

	/*
	 * When HW accelerators and I/O components are used with codec2codec
	 * DAPM links, machine hw_param() gets called only once and DAI
	 * params of all active links are overridden here.
	 */
	list_for_each_entry(runtime, &card->rtd_list, list) {
		if (!runtime->dai_link->params)
			continue;

		dai_params = (struct snd_soc_pcm_stream *)runtime->dai_link->params;
		dai_params->rate_min = params_rate(params);
		dai_params->channels_min = params_channels(params);
		dai_params->formats = 1ULL << params_format(params);
	}

	err = tegra_codecs_runtime_setup(card, params_rate(params),
			params_channels(params),
			dai_props->mclk_fs * params_rate(params));
	if (err < 0)
		return err;

#endif

	return asoc_simple_hw_params(substream, params);
}

static const struct snd_soc_ops tegra_audio_graph_ops = {
	.startup	= asoc_simple_startup,
	.shutdown	= asoc_simple_shutdown,
	.hw_params	= tegra_audio_graph_hw_params,
};


static int tegra_audio_graph_compr_startup(struct snd_compr_stream *cstream)
{
	struct snd_soc_pcm_runtime *rtd = cstream->private_data;

	return asoc_simple_dais_clk_enable(rtd);
}

static void tegra_audio_graph_compr_shutdown(struct snd_compr_stream *cstream)
{
	struct snd_soc_pcm_runtime *rtd = cstream->private_data;

	asoc_simple_dais_clk_disable(rtd);
}

static int tegra_audio_graph_compr_set_params(struct snd_compr_stream *cstream)
{
	struct snd_soc_pcm_runtime *rtd = cstream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct snd_codec codec_params;
	int err;

	if (cstream->ops && cstream->ops->get_params) {
		err = cstream->ops->get_params(cstream, &codec_params);
		if (err < 0) {
			dev_err(card->dev, "Failed to get compr params\n");
			return err;
		}
	} else {
		dev_err(card->dev, "compr ops not set\n");
		return -EINVAL;
	}

	return 0;
}

static const struct snd_soc_compr_ops tegra_audio_graph_compr_ops = {
	.startup	= tegra_audio_graph_compr_startup,
	.shutdown	= tegra_audio_graph_compr_shutdown,
	.set_params	= tegra_audio_graph_compr_set_params,
};

static int tegra_audio_graph_card_probe(struct snd_soc_card *card)
{
	struct asoc_simple_priv *simple = snd_soc_card_get_drvdata(card);
	struct tegra_audio_priv *priv = simple_to_tegra_priv(simple);
	struct snd_soc_pcm_runtime *rtd;
	int err;

	priv->clk_plla = devm_clk_get(card->dev, "pll_a");
	if (IS_ERR(priv->clk_plla)) {
		dev_err(card->dev, "Can't retrieve clk pll_a\n");
		return PTR_ERR(priv->clk_plla);
	}

	priv->clk_plla_out0 = devm_clk_get(card->dev, "plla_out0");
	if (IS_ERR(priv->clk_plla_out0)) {
		dev_err(card->dev, "Can't retrieve clk plla_out0\n");
		return PTR_ERR(priv->clk_plla_out0);
	}

	/*
	 * ADSP component driver expose DAIs which are not only used in
	 * FE links (for PCM or compress interface), but also used in
	 * codec2codec links (with ADMAIF FIFO DAIs). The same is true
	 * for ADMAIF component as well. Currently audio-graph-card
	 * relies on "non_legacy_dai_naming" flag of components to mark
	 * the DAI link as codec2codec. Generally codec component mark
	 * this flag as 1. But in case of ADSP/ADMAIF it cannot be done
	 * so. Hence there is no way to mark some of the links involving
	 * ADSP/ADMAIF as codec2codec links automatically.
	 *
	 * Below is a WAR needed for ADSP use cases.
	 */
	for_each_card_rtds(card, rtd) {
		/*
		 * Following codec2codec links are used in ADSP use cases:
		 *   1. ADSPx <--> ADMAIFx FIFO
		 *   2. ADMAIFx CIF <--> XBAR
		 *
		 * Below checks if ADMAIF "CIF" or "FIFO" DAIs are involed.
		 */
		if (strstr(asoc_rtd_to_cpu(rtd, 0)->name, " CIF") ||
		    strstr(asoc_rtd_to_codec(rtd, 0)->name, " FIFO")) {
			struct snd_soc_pcm_stream *params;

			params = devm_kzalloc(rtd->dev, sizeof(*params),
					      GFP_KERNEL);
			if (!params)
				return -ENOMEM;

			rtd->dai_link->params = params;
			rtd->dai_link->num_params = 1;
		}
	}

	/*
	 * The audio-graph-card does not have a way to identify compress
	 * links automatically. It assumes all as PCM links. Thus below
	 * populates compress callbacks for specific ADSP links.
	 *
	 * TODO: Find a better way to identify compress links.
	 */
	for_each_card_rtds(card, rtd) {
		struct snd_soc_dai *cpu_dai = asoc_rtd_to_cpu(rtd, 0);

		if (strstr(cpu_dai->name, "ADSP COMPR")) {
			priv->simple.compr_ops = &tegra_audio_graph_compr_ops;
			priv->simple.ops = NULL;
		}
	}

	/* Codec specific initialization */
	err = tegra_codecs_init(card);
	if (err < 0)
		return err;

	return graph_card_probe(card);
}

static int tegra_audio_graph_probe(struct platform_device *pdev)
{
	struct tegra_audio_priv *priv;
	struct device *dev = &pdev->dev;
	struct snd_soc_card *card;
	int ret;

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	card = simple_priv_to_card(&priv->simple);
	card->driver_name = "tegra-ape";

	card->probe = tegra_audio_graph_card_probe;

	/* graph_parse_of() depends on below */
	card->component_chaining = 1;
	priv->simple.ops = &tegra_audio_graph_ops;
#if IS_ENABLED(TEGRA_DPCM)
	priv->simple.force_dpcm = 1;
#endif

	ret = graph_parse_of(&priv->simple, dev);
	if (ret < 0)
		return ret;

	tegra_machine_add_i2s_codec_controls(card);

	dev_info(dev, "Registered audio-graph based sound card\n");

	return 0;
}

static const struct tegra_audio_cdata tegra210_data = {
	/* PLLA */
	.plla_rates[x8_RATE] = 368640000,
	.plla_rates[x11_RATE] = 338688000,
	/* PLLA_OUT0 */
	.plla_out0_rates[x8_RATE] = 49152000,
	.plla_out0_rates[x11_RATE] = 45158400,
};

static const struct tegra_audio_cdata tegra186_data = {
	/* PLLA */
	.plla_rates[x8_RATE] = 245760000,
	.plla_rates[x11_RATE] = 270950400,
	/* PLLA_OUT0 */
	.plla_out0_rates[x8_RATE] = 49152000,
	.plla_out0_rates[x11_RATE] = 45158400,
};

static const struct of_device_id graph_of_tegra_match[] = {
	{ .compatible = "nvidia,tegra210-audio-graph-card",
	  .data = &tegra210_data },
	{ .compatible = "nvidia,tegra186-audio-graph-card",
	  .data = &tegra186_data },
	{},
};
MODULE_DEVICE_TABLE(of, graph_of_tegra_match);

static struct platform_driver tegra_audio_graph_card = {
	.driver = {
		.name = "tegra-audio-graph-card",
		.pm = &snd_soc_pm_ops,
		.of_match_table = graph_of_tegra_match,
	},
	.probe = tegra_audio_graph_probe,
};
module_platform_driver(tegra_audio_graph_card);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("ASoC Tegra Audio Graph Sound Card");
MODULE_AUTHOR("Sameer Pujar <spujar@nvidia.com>");
