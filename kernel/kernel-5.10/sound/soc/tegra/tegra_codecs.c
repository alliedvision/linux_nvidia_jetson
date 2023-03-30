// SPDX-License-Identifier: GPL-2.0-only
//
// tegra_codecs.c - External audio codec setup
//
// Copyright (c) 2021-2022, NVIDIA CORPORATION.  All rights reserved.

#include <dt-bindings/sound/tas2552.h>
#include <linux/input.h>
#include <sound/jack.h>
#include <sound/soc.h>
#include <sound/simple_card_utils.h>

#include "tegra_asoc_machine.h"
#include "tegra_codecs.h"
#include "../codecs/rt5640.h"
#include "../codecs/rt5659.h"
#include "../codecs/sgtl5000.h"

static int tegra_audio_dai_init(struct snd_soc_pcm_runtime *rtd)
{
	struct device_node *node = rtd->card->dev->of_node;

	/* Used for audio graph based sound cards only */
	if (of_device_is_compatible(node, "nvidia,tegra186-audio-graph-card") ||
	    of_device_is_compatible(node, "nvidia,tegra210-audio-graph-card"))
		return asoc_simple_dai_init(rtd);

	return 0;
}

static int tegra_machine_rt56xx_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_component *cmpnt = rtd->dais[rtd->num_cpus]->component;
	struct snd_soc_card *card = rtd->card;
	struct snd_soc_jack *jack;
	int err;

	if (!cmpnt->driver->set_jack)
		goto dai_init;

	jack = devm_kzalloc(card->dev, sizeof(struct snd_soc_jack), GFP_KERNEL);
	if (!jack)
		return -ENOMEM;

	err = snd_soc_card_jack_new(card, "Headset Jack", SND_JACK_HEADSET,
				    jack, NULL, 0);
	if (err) {
		dev_err(card->dev, "Headset Jack creation failed %d\n", err);
		return err;
	}

	err = tegra_machine_add_codec_jack_control(card, rtd, jack);
	if (err) {
		dev_err(card->dev, "Failed to add jack control: %d\n", err);
		return err;
	}

	err = cmpnt->driver->set_jack(cmpnt, jack, NULL);
	if (err) {
		dev_err(cmpnt->dev, "Failed to set jack: %d\n", err);
		return err;
	}

	/* single button supporting play/pause */
	snd_jack_set_key(jack->jack, SND_JACK_BTN_0, KEY_MEDIA);

	/* multiple buttons supporting play/pause and volume up/down */
	snd_jack_set_key(jack->jack, SND_JACK_BTN_1, KEY_MEDIA);
	snd_jack_set_key(jack->jack, SND_JACK_BTN_2, KEY_VOLUMEUP);
	snd_jack_set_key(jack->jack, SND_JACK_BTN_3, KEY_VOLUMEDOWN);

	snd_soc_dapm_sync(&card->dapm);


dai_init:
	return tegra_audio_dai_init(rtd);
}

static int tegra_machine_fepi_init(struct snd_soc_pcm_runtime *rtd)
{
	struct device *dev = rtd->card->dev;
	int err;

	err = snd_soc_dai_set_sysclk(rtd->dais[rtd->num_cpus], SGTL5000_SYSCLK, 12288000,
				     SND_SOC_CLOCK_IN);
	if (err) {
		dev_err(dev, "failed to set sgtl5000 sysclk!\n");
		return err;
	}

	return tegra_audio_dai_init(rtd);
}

static int tegra_machine_respeaker_init(struct snd_soc_pcm_runtime *rtd)
{
	struct device *dev = rtd->card->dev;
	int err;

	/* ac108 codec driver hardcodes the freq as 24000000
	 * and source as PLL irrespective of args passed through
	 * this callback
	 */
	err = snd_soc_dai_set_sysclk(rtd->dais[rtd->num_cpus], 0, 24000000,
				     SND_SOC_CLOCK_IN);
	if (err) {
		dev_err(dev, "failed to set ac108 sysclk!\n");
		return err;
	}

	return tegra_audio_dai_init(rtd);
}

static struct snd_soc_pcm_runtime *get_pcm_runtime(struct snd_soc_card *card,
						   const char *link_name)
{
	struct snd_soc_pcm_runtime *rtd;

	for_each_card_rtds(card, rtd) {
		if (!strcmp(rtd->dai_link->name, link_name))
			return rtd;
	}

	return NULL;
}

static int set_pll_sysclk(struct device *dev, struct snd_soc_pcm_runtime *rtd,
		int pll_src, int clk_id, unsigned int srate,
		unsigned int channels)
{
	struct snd_soc_pcm_stream *dai_params;
	unsigned int bclk_rate;
	int err;

	dai_params = (struct snd_soc_pcm_stream *)rtd->dai_link->params;

	switch (dai_params->formats) {
	case SNDRV_PCM_FMTBIT_S8:
		bclk_rate = srate * channels * 8;
		break;
	case SNDRV_PCM_FMTBIT_S16_LE:
		bclk_rate = srate * channels * 16;
		break;
	case SNDRV_PCM_FMTBIT_S24_LE:
		bclk_rate = srate * channels * 24;
		break;
	case SNDRV_PCM_FMTBIT_S32_LE:
		bclk_rate = srate * channels * 32;
		break;
	default:
		dev_err(dev, "invalid format %llu\n",
				dai_params->formats);
		return -EINVAL;
	}

	err = snd_soc_dai_set_pll(rtd->dais[rtd->num_cpus], 0,
			pll_src, bclk_rate, srate * 256);
	if (err < 0) {
		dev_err(dev, "failed to set codec pll\n");
		return err;
	}

	err = snd_soc_dai_set_sysclk(rtd->dais[rtd->num_cpus], clk_id,
			srate * 256, SND_SOC_CLOCK_IN);
	if (err < 0) {
		dev_err(dev, "dais[%d] clock not set\n", rtd->num_cpus);
		return err;
	}

	return 0;
}

int tegra_codecs_runtime_setup(struct snd_soc_card *card,
			       unsigned int srate,
			       unsigned int channels,
			       unsigned int aud_mclk)
{
	struct snd_soc_pcm_runtime *rtd;
	int i, err;

	rtd = get_pcm_runtime(card, "rt565x-playback");
	if (rtd) {
		err = snd_soc_dai_set_sysclk(rtd->dais[rtd->num_cpus],
					     RT5659_SCLK_S_MCLK,
					     aud_mclk, SND_SOC_CLOCK_IN);
		if (err < 0) {
			dev_err(card->dev, "dais[%d] clock not set\n",
				rtd->num_cpus);
			return err;
		}
	}

	rtd = get_pcm_runtime(card, "rt5640-playback");
	if (rtd) {
		err = snd_soc_dai_set_sysclk(rtd->dais[rtd->num_cpus],
					     RT5640_SCLK_S_MCLK,
					     aud_mclk, SND_SOC_CLOCK_IN);
		if (err < 0) {
			dev_err(card->dev, "dais[%d] clock not set\n",
				rtd->num_cpus);
			return err;
		}
	}

	rtd = get_pcm_runtime(card, "rt565x-codec-sysclk-bclk1");
	if (rtd) {
		err = set_pll_sysclk(card->dev, rtd, RT5659_PLL1_S_BCLK1,
				RT5659_SCLK_S_PLL1, srate, channels);
		if (err < 0) {
			dev_err(card->dev, "failed to set pll clk\n");
			return err;
		}
	}

	rtd = get_pcm_runtime(card, "rt5640-codec-sysclk-bclk1");
	if (rtd) {
		err = set_pll_sysclk(card->dev, rtd, RT5640_PLL1_S_BCLK1,
				RT5640_SCLK_S_PLL1, srate, channels);
		if (err < 0) {
			dev_err(card->dev, "failed to set pll clk\n");
			return err;
		}
	}

	rtd = get_pcm_runtime(card, "dspk-playback-dual-tas2552");
	if (rtd) {
		for (i = 0; i < rtd->num_codecs; i++) {
			if (!strcmp(rtd->dais[rtd->num_cpus + i]->name,
							"tas2552-amplifier")) {
				err = snd_soc_dai_set_sysclk(
					rtd->dais[rtd->num_cpus + i],
					TAS2552_PDM_CLK_IVCLKIN, aud_mclk,
					SND_SOC_CLOCK_IN);
				if (err < 0) {
					dev_err(card->dev,
						"dais[%d] clock not set\n",
						rtd->num_cpus + i);
					return err;
				}
			}
		}
	}

	return 0;
}
EXPORT_SYMBOL_GPL(tegra_codecs_runtime_setup);

int tegra_codecs_init(struct snd_soc_card *card)
{
	struct snd_soc_dai_link *dai_links = card->dai_link;
	int i;

	if (!dai_links || !card->num_links)
		return -EINVAL;

	for (i = 0; i < card->num_links; i++) {
		if (strstr(dai_links[i].name, "rt565x-playback") ||
		    strstr(dai_links[i].name, "rt5640-playback") ||
		    strstr(dai_links[i].name, "rt565x-codec-sysclk-bclk1") ||
		    strstr(dai_links[i].name, "rt5640-codec-sysclk-bclk1"))
			dai_links[i].init = tegra_machine_rt56xx_init;
		else if (strstr(dai_links[i].name, "fe-pi-audio-z-v2"))
			dai_links[i].init = tegra_machine_fepi_init;
		else if (strstr(dai_links[i].name, "respeaker-4-mic-array"))
			dai_links[i].init = tegra_machine_respeaker_init;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(tegra_codecs_init);

