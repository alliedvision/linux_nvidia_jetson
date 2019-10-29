/*
 * tegra186_m3420_alt.c - Tegra t186ref m3402 Machine driver
 *
 * Copyright (c) 2017 NVIDIA CORPORATION.  All rights reserved.
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

#include <linux/clk.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/pm_runtime.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>

#include "tegra_asoc_utils_alt.h"
#include "tegra_asoc_machine_alt.h"
#include "tegra210_xbar_alt.h"

#define DRV_NAME "tegra-snd-t186-m3420"

struct tegra186_m3420 {
	struct tegra_asoc_audio_clock_info audio_clock;
	struct mutex lock;
	struct snd_soc_pcm_runtime *i2s_master;
	unsigned int num_codec_links;
	unsigned int clk_ena_count;
	unsigned int srate;
	unsigned int i2s_master_id;
};

static int tegra186_m3420_clocks_init(struct snd_soc_card *card,
					   unsigned int srate)
{
	struct tegra186_m3420 *machine = snd_soc_card_get_drvdata(card);
	unsigned int mclk, clk_out_rate;
	int err = 0;

	/*
	 * Clocks only need to be configured once for any playback sessions
	 * and so if they are enabled, then they have been configured and
	 * we are done.
	 */
	if (machine->clk_ena_count) {
		if (machine->srate == srate)
			goto out;

		/*
		 * For multi-channel playback, the sample rates MUST be the
		 * same because the I2S master channel drives the bit-clock
		 * and frame-sync for all channels.
		 */
		dev_err(card->dev, "Multi-channel sample-rate conflict!\n");
		return -EINVAL;
	}

	switch (srate) {
	case 32000:
	case 44100:
	case 48000:
		machine->audio_clock.mclk_scale = 256;
		break;
	case 96000:
		machine->audio_clock.mclk_scale = 128;
		break;
	default:
		return -EINVAL;
	}

	if (machine->srate != srate) {
		machine->srate = srate;

		clk_out_rate = srate * machine->audio_clock.mclk_scale;
		mclk = clk_out_rate * 2;

		err = tegra_alt_asoc_utils_set_rate(&machine->audio_clock,
						    srate, mclk, clk_out_rate);
		if (err < 0) {
			dev_err(card->dev, "Can't configure clocks\n");
			return err;
		}
	}

	/*
	 * Clocks are enabled here and NOT in machine start-up because for
	 * Tegra186 we cannot the pll_a's p-divider if the pll is enabled
	 * and we cannot get the exact frequency we need. So enable it now
	 * once we have set the clock rates for the playback scenario.
	 */
	err = tegra_alt_asoc_utils_clk_enable(&machine->audio_clock);
	if (err < 0) {
		dev_err(card->dev, "Can't enable clocks\n");
		return err;
	}

	err = pm_runtime_get_sync(machine->i2s_master->cpu_dai->dev);
	if (err < 0) {
		dev_err(card->dev, "Failed to enable I2S master\n");
		tegra_alt_asoc_utils_clk_disable(&machine->audio_clock);
		return err;
	}

out:
	machine->clk_ena_count++;

	return 0;
}

static int tegra186_m3420_hw_params(struct snd_pcm_substream *substream,
				    struct snd_pcm_hw_params *params,
				    const char *name)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct snd_soc_pcm_stream *stream;
	struct tegra186_m3420 *machine = snd_soc_card_get_drvdata(card);
	unsigned int dai_fmt, bclk_ratio;
	int err;

	if (params_format(params) != SNDRV_PCM_FORMAT_S16_LE) {
		dev_err(card->dev, "Invalid data format!\n");
		return -EINVAL;
	}

	mutex_lock(&machine->lock);
	err = tegra186_m3420_clocks_init(card, params_rate(params));
	mutex_unlock(&machine->lock);
	if (err)
		return err;

	rtd = snd_soc_get_pcm_runtime(card, name);
	if (!rtd)
		return -EINVAL;

	stream = (struct snd_soc_pcm_stream *)rtd->dai_link->params;
	stream->rate_min = params_rate(params);

	err = tegra_machine_get_bclk_ratio_t18x(rtd, &bclk_ratio);
	if (err < 0) {
		dev_err(card->dev, "Failed to get bclk ratio for %s\n",
			rtd->dai_link->name);
		return err;
	}

	err = snd_soc_dai_set_bclk_ratio(rtd->cpu_dai, bclk_ratio);
	if (err < 0) {
		dev_err(card->dev, "Failed to set bclk ratio for %s\n",
			rtd->dai_link->name);
		return err;
	}

	/*
	 * For the M3420 platform one of the Tegra I2S channels, I2S1,
	 * is the bit-clock and frame-sync master and it drives all the
	 * bit-clocks and frame-syncs for all other I2S channels. Thus,
	 * for all I2S channels, apart from I2S1, both the Tegra I2S
	 * interface and the codecs are I2S slaves. By default, either
	 * the Tegra I2S interface or the codec should be a master but
	 * not both. To ensure there is no signal contention on the
	 * bit-clock and frame-sync signals force the codecs that don't
	 * interface with I2S1 to be bit-clock and frame-sync slaves.
	 */
	if (machine->i2s_master != rtd) {
		dai_fmt = rtd->dai_link->dai_fmt;
		dai_fmt &= ~SND_SOC_DAIFMT_MASTER_MASK;
		dai_fmt |= SND_SOC_DAIFMT_CBS_CFS;
		err = snd_soc_dai_set_fmt(rtd->codec_dai, dai_fmt);
		if (err)
			return err;
	}

	return snd_soc_dai_set_sysclk(rtd->codec_dai, 0,
				      machine->audio_clock.clk_out_rate,
				      SND_SOC_CLOCK_IN);
}

static int tegra186_m3420_i2s1_hw_params(struct snd_pcm_substream *substream,
					 struct snd_pcm_hw_params *params)
{
	return tegra186_m3420_hw_params(substream, params, "i2s-playback-1");
}

static int tegra186_m3420_i2s2_hw_params(struct snd_pcm_substream *substream,
					 struct snd_pcm_hw_params *params)
{
	return tegra186_m3420_hw_params(substream, params, "i2s-playback-2");
}

static int tegra186_m3420_i2s3_hw_params(struct snd_pcm_substream *substream,
					 struct snd_pcm_hw_params *params)
{
	return tegra186_m3420_hw_params(substream, params, "i2s-playback-3");
}

static int tegra186_m3420_i2s4_hw_params(struct snd_pcm_substream *substream,
					 struct snd_pcm_hw_params *params)
{
	return tegra186_m3420_hw_params(substream, params, "i2s-playback-4");
}

static void tegra186_m3420_shutdown(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct tegra186_m3420 *machine;

	machine = snd_soc_card_get_drvdata(rtd->card);

	mutex_lock(&machine->lock);

	if (machine->clk_ena_count > 0) {
		machine->clk_ena_count--;

		if (!machine->clk_ena_count) {
			pm_runtime_put_sync(machine->i2s_master->cpu_dai->dev);
			tegra_alt_asoc_utils_clk_disable(&machine->audio_clock);
		}
	}

	mutex_unlock(&machine->lock);
}

struct snd_soc_pcm_runtime *tegra186_m3420_i2s_rtd(struct snd_soc_card *card,
						   unsigned int id)
{
	struct snd_soc_pcm_runtime *rtd;
	const char *name;

	switch (id) {
	case 0:
		name = "i2s-playback-1";
		break;
	case 1:
		name = "i2s-playback-2";
		break;
	case 2:
		name = "i2s-playback-3";
		break;
	case 3:
		name = "i2s-playback-4";
		break;
	default:
		dev_err(card->dev, "Invalid I2S master!");
		return ERR_PTR(-EINVAL);
	}

	rtd = snd_soc_get_pcm_runtime(card, name);
	if (!rtd)
		return ERR_PTR(-EINVAL);

	return rtd;
}

static int tegra186_m3420_i2s_fmt(struct snd_soc_card *card,
				  struct snd_soc_pcm_runtime *rtd,
				  bool is_master)
{
	unsigned int dai_fmt;

	dai_fmt = rtd->dai_link->dai_fmt;
	dai_fmt &= ~SND_SOC_DAIFMT_MASTER_MASK;
	dai_fmt |= is_master ? SND_SOC_DAIFMT_CBM_CFM : SND_SOC_DAIFMT_CBS_CFS;

	return snd_soc_dai_set_fmt(rtd->cpu_dai, dai_fmt);
}

static int tegra186_m3420_i2s_config(struct snd_soc_card *card,
				     struct tegra186_m3420 *machine,
				     unsigned new_master_id)
{
	struct snd_soc_pcm_runtime *rtd, *new_master_rtd;
	int ret = 0;

	mutex_lock(&machine->lock);

	if (machine->i2s_master && machine->i2s_master_id == new_master_id)
		goto out;

	if (machine->clk_ena_count > 0) {
		dev_warn(card->dev,
			 "Unable to set I2S master while audio is active\n");
		ret = -EBUSY;
		goto out;
	}

	rtd = tegra186_m3420_i2s_rtd(card, new_master_id);
	if (IS_ERR(rtd)) {
		ret = PTR_ERR(rtd);
		goto out;
	}

	new_master_rtd = rtd;

	/* Configure the current I2S master as an I2S slave */
	if (machine->i2s_master) {
		ret = tegra186_m3420_i2s_fmt(card, machine->i2s_master, false);
		if (ret)
			goto out;
	}

	/* Configure the new I2S master */
	ret = tegra186_m3420_i2s_fmt(card, new_master_rtd, true);
	if (ret) {
		if (machine->i2s_master)
			tegra186_m3420_i2s_fmt(card, machine->i2s_master, true);
		goto out;
	}

	machine->i2s_master_id = new_master_id;
	machine->i2s_master = new_master_rtd;

out:
	mutex_unlock(&machine->lock);

	return ret;
}

static int tegra186_m3420_i2s_master_get(struct snd_kcontrol *kcontrol,
					 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *card = snd_kcontrol_chip(kcontrol);
	struct tegra186_m3420 *machine = snd_soc_card_get_drvdata(card);

	ucontrol->value.integer.value[0] = machine->i2s_master_id;

	return 0;
}

static int tegra186_m3420_i2s_master_put(struct snd_kcontrol *kcontrol,
					 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *card = snd_kcontrol_chip(kcontrol);
	struct tegra186_m3420 *machine = snd_soc_card_get_drvdata(card);
	unsigned int i2s_master_id;

	i2s_master_id = ucontrol->value.integer.value[0];

	return tegra186_m3420_i2s_config(card, machine, i2s_master_id);
}

static struct snd_soc_ops tegra186_m3420_i2s1_ops = {
	.hw_params = tegra186_m3420_i2s1_hw_params,
	.shutdown = tegra186_m3420_shutdown,
};

static struct snd_soc_ops tegra186_m3420_i2s2_ops = {
	.hw_params = tegra186_m3420_i2s2_hw_params,
	.shutdown = tegra186_m3420_shutdown,
};

static struct snd_soc_ops tegra186_m3420_i2s3_ops = {
	.hw_params = tegra186_m3420_i2s3_hw_params,
	.shutdown = tegra186_m3420_shutdown,
};

static struct snd_soc_ops tegra186_m3420_i2s4_ops = {
	.hw_params = tegra186_m3420_i2s4_hw_params,
	.shutdown = tegra186_m3420_shutdown,
};

static const char * const tegra186_i2s_control_text[] = {
	"I2S1", "I2S2", "I2S3", "I2S4",
};

static const struct soc_enum tegra186_i2s_control =
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(tegra186_i2s_control_text),
		tegra186_i2s_control_text);

static const struct snd_kcontrol_new tegra_m3420_controls[] = {
	SOC_ENUM_EXT("I2S MASTER", tegra186_i2s_control,
		       tegra186_m3420_i2s_master_get,
		       tegra186_m3420_i2s_master_put),
};

static const struct snd_soc_dapm_widget tegra_m3420_dapm_widgets[] = {
	SND_SOC_DAPM_HP("Headphone-1", NULL),
	SND_SOC_DAPM_HP("Headphone-2", NULL),
	SND_SOC_DAPM_HP("Headphone-3", NULL),
	SND_SOC_DAPM_HP("Headphone-4", NULL),
	SND_SOC_DAPM_MIC("Mic-1", NULL),
	SND_SOC_DAPM_MIC("Mic-2", NULL),
	SND_SOC_DAPM_MIC("Mic-3", NULL),
	SND_SOC_DAPM_MIC("Mic-4", NULL),
};

static int tegra186_m3420_suspend_pre(struct snd_soc_card *card)
{
	struct snd_soc_pcm_runtime *rtd;

	/* DAPM dai link stream work for non pcm links */
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 5, 0)
	unsigned int idx;
	for (idx = 0; idx < card->num_rtd; idx++) {
		rtd = &card->rtd[idx];
#else
	list_for_each_entry(rtd, &card->rtd_list, list) {
#endif
		if (rtd->dai_link->params)
			INIT_DELAYED_WORK(&rtd->delayed_work, NULL);
	}

	return 0;
}

static struct snd_soc_card snd_soc_tegra186_m3420 = {
	.name = "tegra186-m3420",
	.owner = THIS_MODULE,
	.suspend_pre = tegra186_m3420_suspend_pre,
	.dapm_widgets = tegra_m3420_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(tegra_m3420_dapm_widgets),
	.controls = tegra_m3420_controls,
	.num_controls = ARRAY_SIZE(tegra_m3420_controls),
	.fully_routed = true,
};

static bool codec_i2s_is_master(unsigned int dai_fmt)
{
	/*
	 * If the codec is a bit-clock and frame-sync slave,
	 * then this codec interfaces to the i2s master channel.
	 */
	if ((dai_fmt & SND_SOC_DAIFMT_MASTER_MASK) == SND_SOC_DAIFMT_CBS_CFS)
		return true;
	else
		return false;
}

static void dai_set_ops(struct snd_soc_ops *ops, unsigned int idx_start,
			unsigned int idx_end)
{
	unsigned int i;

	for (i = idx_start; i <= idx_end; i++)
		tegra_machine_set_dai_ops(i, ops);
}

static int dai_link_setup(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);
	struct tegra186_m3420 *machine = snd_soc_card_get_drvdata(card);
	struct snd_soc_codec_conf *tegra_machine_codec_conf = NULL;
	struct snd_soc_codec_conf *tegra186_codec_conf = NULL;
	struct snd_soc_dai_link *dai_links = NULL;
	struct snd_soc_dai_link *codec_links = NULL;
	unsigned int i2s_master_count = 0;
	int i, err = -ENODEV;

	/* set new codec links and conf */
	codec_links = tegra_machine_new_codec_links(pdev, codec_links,
						    &machine->num_codec_links);
	if (!codec_links)
		return err;

	tegra186_codec_conf = tegra_machine_new_codec_conf(pdev,
		tegra186_codec_conf,
		&machine->num_codec_links);
	if (!tegra186_codec_conf)
		goto err_alloc_dai_link;

	/* get the xbar dai link/codec conf structure */
	dai_links = tegra_machine_get_dai_link_t18x();
	if (!dai_links)
		goto err_alloc_codec_conf;

	tegra_machine_codec_conf = tegra_machine_get_codec_conf_t18x();
	if (!tegra_machine_codec_conf)
		goto err_alloc_codec_conf;

	/* set ADMAIF dai_ops */
	dai_set_ops(&tegra186_m3420_i2s1_ops, TEGRA186_DAI_LINK_ADMAIF1,
		    TEGRA186_DAI_LINK_ADMAIF20);

	for (i = 0; i < machine->num_codec_links; i++) {
		if (!codec_links[i].name)
			continue;

		if (strstr(codec_links[i].name, "i2s-playback-1")) {
			dai_set_ops(&tegra186_m3420_i2s1_ops,
				    TEGRA186_DAI_LINK_ADMAIF1,
				    TEGRA186_DAI_LINK_ADMAIF2);
			if (codec_i2s_is_master(codec_links[i].dai_fmt)) {
				machine->i2s_master_id = 0;
				i2s_master_count++;
			}
		} else if (strstr(codec_links[i].name, "i2s-playback-2")) {
			dai_set_ops(&tegra186_m3420_i2s2_ops,
				    TEGRA186_DAI_LINK_ADMAIF3,
				    TEGRA186_DAI_LINK_ADMAIF4);
			if (codec_i2s_is_master(codec_links[i].dai_fmt)) {
				machine->i2s_master_id = 1;
				i2s_master_count++;
			}
		} else if (strstr(codec_links[i].name, "i2s-playback-3")) {
			dai_set_ops(&tegra186_m3420_i2s3_ops,
				    TEGRA186_DAI_LINK_ADMAIF5,
				    TEGRA186_DAI_LINK_ADMAIF6);
			if (codec_i2s_is_master(codec_links[i].dai_fmt)) {
				machine->i2s_master_id = 2;
				i2s_master_count++;
			}
		} else if (strstr(codec_links[i].name, "i2s-playback-4")) {
			dai_set_ops(&tegra186_m3420_i2s4_ops,
				    TEGRA186_DAI_LINK_ADMAIF7,
				    TEGRA186_DAI_LINK_ADMAIF8);
			if (codec_i2s_is_master(codec_links[i].dai_fmt)) {
				machine->i2s_master_id = 3;
				i2s_master_count++;
			}
		}
	}

	if (i2s_master_count != 1) {
		dev_err(&pdev->dev, "Invalid i2s master configuration!\n");
		goto err_alloc_codec_conf;
	}

	/* append t186ref specific dai_links */
	card->num_links = tegra_machine_append_dai_link_t18x(codec_links,
						2 * machine->num_codec_links);
	dai_links = tegra_machine_get_dai_link_t18x();
	card->dai_link = dai_links;

	/* append t186ref specific codec_conf */
	card->num_configs =
		tegra_machine_append_codec_conf_t18x(tegra186_codec_conf,
			machine->num_codec_links);
	tegra_machine_codec_conf = tegra_machine_get_codec_conf_t18x();
	card->codec_conf = tegra_machine_codec_conf;

	return 0;

err_alloc_codec_conf:
	tegra_machine_remove_codec_conf();
err_alloc_dai_link:
	tegra_machine_remove_dai_link();

	return err;
}

static int tegra186_m3420_driver_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct snd_soc_card *card = &snd_soc_tegra186_m3420;
	struct tegra186_m3420 *machine;
	struct tegra_asoc_audio_clock_info *clocks;
	int err = 0;

	if (!np) {
		dev_err(&pdev->dev, "No device tree node found!");
		return -ENODEV;
	}

	machine = devm_kzalloc(&pdev->dev, sizeof(struct tegra186_m3420),
			       GFP_KERNEL);
	if (!machine)
		return -ENOMEM;

	card->dev = &pdev->dev;
	platform_set_drvdata(pdev, card);
	snd_soc_card_set_drvdata(card, machine);
	mutex_init(&machine->lock);
	clocks = &machine->audio_clock;

	err = snd_soc_of_parse_card_name(card, "nvidia,model");
	if (err)
		return err;

	err = snd_soc_of_parse_audio_routing(card, "nvidia,audio-routing");
	if (err)
		return err;

	if (of_property_read_u32(np, "nvidia,num-clk", &clocks->num_clk) < 0) {
		dev_err(&pdev->dev, "Missing property nvidia,num-clk\n");
		return -ENODEV;
	}

	if (of_property_read_u32_array(np, "nvidia,clk-rates",
				       clocks->clk_rates,
				       clocks->num_clk) < 0) {
		dev_err(&pdev->dev, "Missing property nvidia,clk-rates\n");
		return -ENODEV;
	}

	tegra_machine_dma_set_mask(pdev);

	err = dai_link_setup(pdev);
	if (err < 0) {
		dev_err(&pdev->dev, "Failed to configured DAIs!\n");
		return err;
	}

	err = tegra_alt_asoc_utils_init(clocks, &pdev->dev, card);
	if (err)
		goto err_remove_dai_link;

	tegra_alt_asoc_utils_set_parent(&machine->audio_clock, true);

	err = snd_soc_register_card(card);
	if (err) {
		dev_err(&pdev->dev, "snd_soc_register_card failed (%d)\n", err);
		goto err_remove_dai_link;
	}

	return tegra186_m3420_i2s_config(card, machine, machine->i2s_master_id);

err_remove_dai_link:
	tegra_machine_remove_dai_link();
	tegra_machine_remove_codec_conf();

	return err;
}

static int tegra186_m3420_driver_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);

	snd_soc_unregister_card(card);
	tegra_machine_remove_dai_link();
	tegra_machine_remove_codec_conf();

	return 0;
}

static const struct of_device_id tegra186_m3420_of_match[] = {
	{ .compatible = "nvidia,tegra-audio-t186ref-m3420", },
	{ },
};

static struct platform_driver tegra186_m3420_driver = {
	.driver = {
		.name = DRV_NAME,
		.owner = THIS_MODULE,
		.pm = &snd_soc_pm_ops,
		.of_match_table = tegra186_m3420_of_match,
	},
	.probe = tegra186_m3420_driver_probe,
	.remove = tegra186_m3420_driver_remove,
};
module_platform_driver(tegra186_m3420_driver);

MODULE_AUTHOR("Jon Hunter <jonathanh@nvidia.com>");
MODULE_DESCRIPTION("Tegra186 M3420 machine ASoC driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:" DRV_NAME);
MODULE_DEVICE_TABLE(of, tegra186_m3420_of_match);
