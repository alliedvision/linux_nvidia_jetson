/*
 * tegra_machine_driver.c - Tegra ASoC Machine driver
 *
 * Copyright (c) 2017-2021 NVIDIA CORPORATION.  All rights reserved.
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

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of_platform.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include "tegra210_ahub.h"
#include "tegra_asoc_machine.h"
#include "tegra_codecs.h"

#define DRV_NAME "tegra-asoc:"

static const char * const tegra_machine_srate_text[] = {
	"None",
	"8kHz",
	"16kHz",
	"44kHz",
	"48kHz",
	"11kHz",
	"22kHz",
	"24kHz",
	"32kHz",
	"88kHz",
	"96kHz",
	"176kHz",
	"192kHz",
};

static const char * const tegra_machine_format_text[] = {
	"None",
	"16",
	"32",
};

static const struct soc_enum tegra_machine_codec_rate =
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(tegra_machine_srate_text),
			    tegra_machine_srate_text);

static const struct soc_enum tegra_machine_codec_format =
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(tegra_machine_format_text),
			    tegra_machine_format_text);

static const int tegra_machine_srate_values[] = {
	0,
	8000,
	16000,
	44100,
	48000,
	11025,
	22050,
	24000,
	32000,
	88200,
	96000,
	176400,
	192000,
};

static int tegra_machine_codec_get_rate(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *card = snd_kcontrol_chip(kcontrol);
	struct tegra_machine *machine = snd_soc_card_get_drvdata(card);

	ucontrol->value.integer.value[0] = machine->rate_via_kcontrol;

	return 0;
}

static int tegra_machine_codec_put_rate(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *card = snd_kcontrol_chip(kcontrol);
	struct tegra_machine *machine = snd_soc_card_get_drvdata(card);

	/* set the rate control flag */
	machine->rate_via_kcontrol = ucontrol->value.integer.value[0];

	return 0;
}

static int tegra_machine_codec_get_format(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *card = snd_kcontrol_chip(kcontrol);
	struct tegra_machine *machine = snd_soc_card_get_drvdata(card);

	ucontrol->value.integer.value[0] = machine->fmt_via_kcontrol;

	return 0;
}

static int tegra_machine_codec_put_format(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *card = snd_kcontrol_chip(kcontrol);
	struct tegra_machine *machine = snd_soc_card_get_drvdata(card);

	/* set the format control flag */
	machine->fmt_via_kcontrol = ucontrol->value.integer.value[0];

	return 0;
}

static int tegra_machine_dai_init(struct snd_soc_pcm_runtime *runtime,
				  unsigned int rate, unsigned int channels,
				  u64 formats)
{
	unsigned int mask = (1 << channels) - 1;
	struct snd_soc_card *card = runtime->card;
	struct tegra_machine *machine = snd_soc_card_get_drvdata(card);
	struct snd_soc_pcm_stream *dai_params;
	unsigned int aud_mclk, srate;
	u64 format_k, fmt;
	int err, sample_size;
	struct snd_soc_pcm_runtime *rtd;

	srate = (machine->rate_via_kcontrol) ?
			tegra_machine_srate_values[machine->rate_via_kcontrol] :
			rate;
	format_k = (machine->fmt_via_kcontrol == 2) ?
			(1ULL << SNDRV_PCM_FORMAT_S32_LE) : (1 << formats);
	switch (formats) {
	case SNDRV_PCM_FORMAT_S8:
		sample_size = 8;
		break;
	case SNDRV_PCM_FORMAT_S16_LE:
		sample_size = 16;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
	/*
	 * I2S bit clock is derived from PLLA_OUT0 and size of
	 * 24 bits results in fractional value and the clock
	 * is not accurate with this. To have integer clock
	 * division below is used. It means there are additional
	 * bit clocks (8 cycles) which are ignored. Codec picks
	 * up data for other channel when LRCK signal toggles.
	 */
	case SNDRV_PCM_FORMAT_S32_LE:
		sample_size = 32;
		break;
	default:
		pr_err("Wrong format!\n");
		return -EINVAL;
	}

	err = tegra_asoc_utils_set_tegra210_rate(&machine->audio_clock, srate,
						channels, sample_size);
	if (err < 0) {
		dev_err(card->dev, "Can't configure clocks\n");
		return err;
	}

	aud_mclk = machine->audio_clock.set_mclk;

	pr_debug("pll_a_out0 = %u Hz, aud_mclk = %u Hz, sample rate = %u Hz\n",
		 machine->audio_clock.set_pll_out, aud_mclk, srate);

	list_for_each_entry(rtd, &card->rtd_list, list) {
		if (!rtd->dai_link->params)
			continue;
		dai_params = (struct snd_soc_pcm_stream *)rtd->dai_link->params;
		dai_params->rate_min = srate;
		dai_params->channels_min = channels;
		dai_params->formats = format_k;

		fmt = rtd->dai_link->dai_fmt & SND_SOC_DAIFMT_FORMAT_MASK;
		/* set TDM slot mask */
		if (fmt == SND_SOC_DAIFMT_DSP_A ||
		    fmt == SND_SOC_DAIFMT_DSP_B) {
			err = snd_soc_dai_set_tdm_slot(
					rtd->dais[0], mask,
					mask, 0, 0);
			if (err < 0) {
				dev_err(card->dev,
				"%s cpu DAI slot mask not set\n",
				rtd->dais[0]->name);
				return err;
			}
		}
	}

	return tegra_codecs_runtime_setup(card, srate, channels, aud_mclk);
}

static int tegra_machine_pcm_hw_params(struct snd_pcm_substream *substream,
					struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	int err;

	err = tegra_machine_dai_init(rtd, params_rate(params),
				     params_channels(params),
				     params_format(params));
	if (err < 0) {
		dev_err(card->dev, "Failed dai init\n");
		return err;
	}

	return 0;
}

static int tegra_machine_pcm_startup(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct tegra_machine *machine = snd_soc_card_get_drvdata(rtd->card);

	tegra_asoc_utils_clk_enable(&machine->audio_clock);

	return 0;
}

static void tegra_machine_pcm_shutdown(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct tegra_machine *machine = snd_soc_card_get_drvdata(rtd->card);

	tegra_asoc_utils_clk_disable(&machine->audio_clock);
}

static int tegra_machine_compr_startup(struct snd_compr_stream *cstream)
{
	struct snd_soc_pcm_runtime *rtd = cstream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct tegra_machine *machine = snd_soc_card_get_drvdata(card);

	tegra_asoc_utils_clk_enable(&machine->audio_clock);

	return 0;
}

static void tegra_machine_compr_shutdown(struct snd_compr_stream *cstream)
{
	struct snd_soc_pcm_runtime *rtd = cstream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct tegra_machine *machine = snd_soc_card_get_drvdata(card);

	tegra_asoc_utils_clk_disable(&machine->audio_clock);
}

static int tegra_machine_compr_set_params(struct snd_compr_stream *cstream)
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

	err = tegra_machine_dai_init(rtd, codec_params.sample_rate,
				     codec_params.ch_out,
				     SNDRV_PCM_FORMAT_S16_LE);
	if (err < 0) {
		dev_err(card->dev, "Failed dai init\n");
		return err;
	}

	return 0;
}

static struct snd_soc_ops tegra_machine_pcm_ops = {
	.hw_params	= tegra_machine_pcm_hw_params,
	.startup	= tegra_machine_pcm_startup,
	.shutdown	= tegra_machine_pcm_shutdown,
};

static struct snd_soc_compr_ops tegra_machine_compr_ops = {
	.set_params	= tegra_machine_compr_set_params,
	.startup	= tegra_machine_compr_startup,
	.shutdown	= tegra_machine_compr_shutdown,
};

static int add_dai_links(struct snd_soc_card *card)
{
	int ret;

	ret = parse_card_info(card, &tegra_machine_pcm_ops,
			      &tegra_machine_compr_ops);
	if (ret < 0)
		return ret;

	ret = tegra_codecs_init(card);
	if (ret < 0)
		return ret;

	return 0;
}

static const struct snd_kcontrol_new tegra_machine_controls[] = {
	SOC_ENUM_EXT("codec-x rate", tegra_machine_codec_rate,
		tegra_machine_codec_get_rate, tegra_machine_codec_put_rate),
	SOC_ENUM_EXT("codec-x format", tegra_machine_codec_format,
		tegra_machine_codec_get_format, tegra_machine_codec_put_format),
};

static struct snd_soc_card snd_soc_tegra_card = {
	.owner = THIS_MODULE,
	.controls = tegra_machine_controls,
	.num_controls = ARRAY_SIZE(tegra_machine_controls),
	.fully_routed = true,
	.driver_name = "tegra-ape",
};

/* structure to match device tree node */
static const struct of_device_id tegra_machine_of_match[] = {
	{ .compatible = "nvidia,tegra186-ape" },
	{ .compatible = "nvidia,tegra210-ape" },
	{},
};

static int tegra_machine_driver_probe(struct platform_device *pdev)
{
	struct snd_soc_card *card = &snd_soc_tegra_card;
	struct tegra_machine *machine;
	int ret = 0;

	machine = devm_kzalloc(&pdev->dev, sizeof(*machine), GFP_KERNEL);
	if (!machine)
		return -ENOMEM;

	machine->asoc = devm_kzalloc(&pdev->dev, sizeof(*machine->asoc),
				     GFP_KERNEL);
	if (!machine->asoc)
		return -ENOMEM;

	card->dev = &pdev->dev;
	platform_set_drvdata(pdev, card);
	snd_soc_card_set_drvdata(card, machine);

	card->dapm.idle_bias_off = true;

	memset(&machine->audio_clock, 0, sizeof(machine->audio_clock));
	ret = tegra_asoc_utils_init(&machine->audio_clock, &pdev->dev);
	if (ret < 0)
		return ret;

	ret = add_dai_links(card);
	if (ret < 0)
		goto cleanup_asoc;

	ret = snd_soc_register_card(card);
	if (ret) {
		dev_err(&pdev->dev, "snd_soc_register_card failed (%d)\n",
			ret);
		goto cleanup_asoc;
	}

	tegra_machine_add_i2s_codec_controls(card);

	return 0;
cleanup_asoc:
	release_asoc_phandles(machine);
	return ret;
}

static int tegra_machine_driver_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);

	snd_soc_unregister_card(card);

	return 0;
}

#if CONFIG_PM
static void tegra_asoc_machine_resume(struct device *dev)
{
	WARN_ON(snd_soc_resume(dev));
}
#else
#define tegra_asoc_machine_resume NULL
#endif

static const struct dev_pm_ops tegra_asoc_machine_pm_ops = {
	.prepare = snd_soc_suspend,
	.complete = tegra_asoc_machine_resume,
	.poweroff = snd_soc_poweroff,
};

static struct platform_driver tegra_asoc_machine_driver = {
	.driver = {
		.name = DRV_NAME,
		.owner = THIS_MODULE,
		.pm = &tegra_asoc_machine_pm_ops,
		.of_match_table = tegra_machine_of_match,
	},
	.probe = tegra_machine_driver_probe,
	.remove = tegra_machine_driver_remove,
};
module_platform_driver(tegra_asoc_machine_driver);

MODULE_AUTHOR("Mohan Kumar <mkumard@nvidia.com>, Sameer Pujar <spujar@nvidia.com>");
MODULE_DESCRIPTION("Tegra ASoC machine driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:" DRV_NAME);
MODULE_DEVICE_TABLE(of, tegra_machine_of_match);
