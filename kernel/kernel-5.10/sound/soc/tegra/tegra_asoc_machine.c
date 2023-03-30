// SPDX-License-Identifier: GPL-2.0-only
/*
 * tegra_asoc_machine.c - Tegra DAI links parser
 *
 * Copyright (c) 2014-2022 NVIDIA CORPORATION.  All rights reserved.
 *
 */

#include <linux/module.h>
#include <linux/of.h>
#include <sound/simple_card_utils.h>
#include <sound/jack.h>
#include <sound/soc.h>
#include "tegra_asoc_machine.h"

#define PREFIX "nvidia-audio-card,"
#define CELL "#sound-dai-cells"
#define DAI "sound-dai"

/* DT also uses similar values to specify link type */
enum dai_link_type {
	PCM_LINK,
	COMPR_LINK,
	C2C_LINK,
};

struct snd_soc_pcm_stream link_params = {
	.formats = SNDRV_PCM_FMTBIT_S8	   |
		   SNDRV_PCM_FMTBIT_S16_LE |
		   SNDRV_PCM_FMTBIT_S24_LE |
		   SNDRV_PCM_FMTBIT_S32_LE,
	.rate_min = 8000,
	.rate_max = 192000,
	.channels_min = 1,
	.channels_max = 16,
};

/* find if DAI link or its cpu/codec DAI nodes are disabled */
static bool of_dai_link_is_available(struct device_node *link_node)
{
	struct device_node *child, *dai_node;

	if (!of_device_is_available(link_node))
		return false;

	for_each_child_of_node(link_node, child) {
		/* check for "cpu" and "codec" nodes only */
		if (of_node_cmp(child->name, "cpu") &&
		    of_node_cmp(child->name, "codec"))
			continue;

		/*
		 * Skip a codec subnode if DAI property is missing. For a
		 * link with multiple codecs, at least one codec needs to
		 * have DAI property (which is ensured while counting the
		 * number of links that DT exposes). Other codec subnodes
		 * can be empty and populated in override file.
		 */
		if (!of_property_read_bool(child, DAI) &&
		    !of_node_cmp(child->name, "codec"))
			continue;

		dai_node = of_parse_phandle(child, DAI, 0);
		if (!dai_node) {
			of_node_put(child);
			return false;
		}

		if (!of_device_is_available(dai_node)) {
			of_node_put(dai_node);
			of_node_put(child);
			return false;
		}

		of_node_put(dai_node);
	}

	return true;
}

/* find number of child nodes with given name and containing DAI property */
static int of_get_child_count_with_name(struct device_node *node,
					const char *name)
{
	struct device_node *child;
	int num = 0;

	for_each_child_of_node(node, child)
		if (!of_node_cmp(child->name, name) &&
		    of_property_read_bool(child, DAI))
			num++;

	return num;
}

static int get_num_dai_links(struct platform_device *pdev,
			     unsigned int *num_links)
{
	struct device_node *top = pdev->dev.of_node;
	struct device_node *link_node;
	unsigned int link_count = 0;

	link_node = of_get_child_by_name(top, PREFIX "dai-link");
	if (!link_node) {
		dev_err(&pdev->dev, "no dai links found\n");
		return -ENOENT;
	}

	do {
		if (!of_dai_link_is_available(link_node)) {
			link_node = of_get_next_child(top, link_node);
			continue;
		}

		link_count++;
		link_node = of_get_next_child(top, link_node);
	} while (link_node);

	*num_links = link_count;

	return 0;
}

static int allocate_link_dais(struct platform_device *pdev,
			struct snd_soc_dai_link *dai_links)
{
	struct device_node *top = pdev->dev.of_node;
	struct device_node *link_node;
	unsigned int link_count = 0, num_codecs;

	link_node = of_get_child_by_name(top, PREFIX "dai-link");
	if (!link_node) {
		dev_err(&pdev->dev, "no dai links found\n");
		return -ENOENT;
	}

	do {
		if (!of_dai_link_is_available(link_node)) {
			link_node = of_get_next_child(top, link_node);
			continue;
		}

		dai_links[link_count].cpus = devm_kzalloc(&pdev->dev,
						 sizeof(*dai_links[link_count].cpus),
						 GFP_KERNEL);
		if (!dai_links[link_count].cpus)
			return -ENOMEM;

		num_codecs = of_get_child_count_with_name(link_node,
							 "codec");
		if (!num_codecs) {
			of_node_put(link_node);
			dev_err(&pdev->dev,
				"no codec subnode or sound-dai property\n");
			return -EINVAL;
		}

		dai_links[link_count].codecs =
			devm_kzalloc(&pdev->dev,
			sizeof(*dai_links[link_count].codecs) * num_codecs,
			GFP_KERNEL);
		if (!dai_links[link_count].codecs)
			return -ENOMEM;

		dai_links[link_count].platforms = devm_kzalloc(&pdev->dev,
						   sizeof(*dai_links[link_count].platforms),
						   GFP_KERNEL);
		if (!dai_links[link_count].platforms)
			return -ENOMEM;

		dai_links[link_count].num_cpus = 1;
		dai_links[link_count].num_codecs = num_codecs;
		dai_links[link_count].num_platforms = 1;

		link_count++;

		link_node = of_get_next_child(top, link_node);
	} while (link_node);

	return 0;
}

static int get_num_codec_confs(struct platform_device *pdev, int *num_confs)
{
	struct device_node *top = pdev->dev.of_node;
	struct device_node *link_node, *codec;
	unsigned int conf_count = 0, num_codecs;

	link_node = of_get_child_by_name(top, PREFIX "dai-link");
	if (!link_node) {
		dev_err(&pdev->dev, "no dai links found\n");
		return -EINVAL;
	}

	do {
		if (!of_dai_link_is_available(link_node)) {
			link_node = of_get_next_child(top, link_node);
			continue;
		}

		num_codecs = of_get_child_count_with_name(link_node,
							  "codec");
		if (!num_codecs) {
			of_node_put(link_node);
			dev_err(&pdev->dev, "missing codec subnode\n");
			return -EINVAL;
		}

		for_each_child_of_node(link_node, codec) {
			if (of_node_cmp(codec->name, "codec"))
				continue;

			if (of_property_read_bool(codec, "prefix"))
				conf_count++;
		}

		link_node = of_get_next_child(top, link_node);
	} while (link_node);

	*num_confs = conf_count;

	return 0;
}

static void parse_mclk_fs(struct snd_soc_card *card)
{
	struct tegra_machine *machine = snd_soc_card_get_drvdata(card);
	struct platform_device *pdev = to_platform_device(card->dev);

	if (of_property_read_u32(pdev->dev.of_node, PREFIX "mclk-fs",
				 &machine->audio_clock.mclk_fs))
		dev_dbg(&pdev->dev, "'%smclk-fs' property is missing\n",
			PREFIX);
}

static int parse_dai(struct device_node *node,
		     struct snd_soc_dai_link_component *dlc)
{
	struct of_phandle_args args;
	int ret;

	ret = of_parse_phandle_with_args(node, DAI, CELL, 0, &args);
	if (ret)
		return ret;

	ret = snd_soc_of_get_dai_name(node, &dlc->dai_name);
	if (ret < 0)
		return ret;

	dlc->of_node = args.np;

	return 0;
}

static int parse_dt_codec_confs(struct snd_soc_card *card)
{
	struct platform_device *pdev = to_platform_device(card->dev);
	struct tegra_machine *machine = snd_soc_card_get_drvdata(card);
	struct device_node *top = pdev->dev.of_node;
	struct device_node *link_node;
	struct snd_soc_codec_conf *codec_confs;
	unsigned int num_confs, i = 0;
	int err;

	err = get_num_codec_confs(pdev, &machine->asoc->num_confs);
	if (err < 0)
		return err;

	num_confs = machine->asoc->num_confs;
	if (!num_confs)
		return 0;

	machine->asoc->codec_confs = devm_kcalloc(&pdev->dev,
						  num_confs,
						  sizeof(*codec_confs),
						  GFP_KERNEL);
	if (!machine->asoc->codec_confs)
		return -ENOMEM;
	codec_confs = machine->asoc->codec_confs;

	link_node = of_get_child_by_name(top, PREFIX "dai-link");
	if (!link_node) {
		dev_err(&pdev->dev, "DAI links not found in DT\n");
		return -ENOENT;
	}

	do {
		struct of_phandle_args args;
		struct device_node *codec;

		if (!of_dai_link_is_available(link_node)) {
			link_node = of_get_next_child(top, link_node);
			continue;
		}

		for_each_child_of_node(link_node, codec) {
			if (of_node_cmp(codec->name, "codec"))
				continue;

			if (!of_property_read_bool(codec, "prefix"))
				continue;

			err = of_parse_phandle_with_args(codec, DAI, CELL, 0,
							 &args);
			if (err < 0) {
				of_node_put(codec);
				of_node_put(link_node);
				return err;
			}

			codec_confs[i].dlc.of_node = args.np;
			codec_confs[i].dlc.name = NULL;

			of_property_read_string(codec, "prefix",
						&codec_confs[i].name_prefix);

			i++;
		}

		link_node = of_get_next_child(top, link_node);
	} while (link_node);

	card->num_configs = num_confs;
	card->codec_conf = codec_confs;

	return 0;
}

static int parse_dai_link_params(struct platform_device *pdev,
				 struct device_node *link_node,
				 struct snd_soc_dai_link *dai_link)
{
	struct snd_soc_pcm_stream *params;
	char *str;

	params = devm_kzalloc(&pdev->dev, sizeof(*params), GFP_KERNEL);
	if (!params)
		return -ENOMEM;

	/* Copy default settings */
	memcpy(params, &link_params, sizeof(*params));

	if (!of_property_read_u32(link_node, "srate",
				  &params->rate_min)) {
		if (params->rate_min < link_params.rate_min ||
		    params->rate_min > link_params.rate_max) {
			dev_err(&pdev->dev,
				"Unsupported rate %d for DAI link (%pOF)\n",
				params->rate_min, link_node);

			return -EOPNOTSUPP;
		}

		params->rate_max = params->rate_min;
	}

	if (!of_property_read_u32(link_node, "num-channel",
				  &params->channels_min)) {
		if (params->channels_min < link_params.channels_min ||
		    params->channels_min > link_params.channels_max) {
			dev_err(&pdev->dev,
				"Unsupported channel %d for DAI link (%pOF)\n",
				params->channels_min, link_node);

			return -EOPNOTSUPP;
		}

		params->channels_max = params->channels_min;
	}

	if (!of_property_read_string(link_node, "bit-format",
				     (const char **)&str)) {
		if (!strcmp("s8", str)) {
			params->formats = SNDRV_PCM_FMTBIT_S8;
		} else if (!strcmp("s16_le", str)) {
			params->formats = SNDRV_PCM_FMTBIT_S16_LE;
		} else if (!strcmp("s24_le", str)) {
			params->formats = SNDRV_PCM_FMTBIT_S24_LE;
		} else if (!strcmp("s32_le", str)) {
			params->formats = SNDRV_PCM_FMTBIT_S32_LE;
		} else {
			dev_err(&pdev->dev,
				"Unsupported format %s for DAI link (%pOF)\n",
				str, link_node);

			return -EOPNOTSUPP;
		}

		if (!(params->formats & link_params.formats)) {
			dev_err(&pdev->dev,
				"Unsupported format %s for DAI link (%pOF)\n",
				str, link_node);

			return -EOPNOTSUPP;
		}
	}

	dai_link->params = params;

	return 0;
}

static int parse_dt_dai_links(struct snd_soc_card *card,
			      struct snd_soc_ops *pcm_ops,
			      struct snd_soc_compr_ops *compr_ops)
{
	struct platform_device *pdev = to_platform_device(card->dev);
	struct tegra_machine *machine = snd_soc_card_get_drvdata(card);
	struct device_node *top = pdev->dev.of_node;
	struct device_node *link_node;
	struct snd_soc_dai_link *dai_links;
	unsigned int num_links, link_count = 0;
	int ret;

	ret = get_num_dai_links(pdev, &machine->asoc->num_links);
	if (ret < 0)
		return ret;

	num_links = machine->asoc->num_links;
	if (!num_links)
		return -EINVAL;

	dai_links = devm_kcalloc(&pdev->dev, num_links, sizeof(*dai_links),
				 GFP_KERNEL);
	if (!dai_links)
		return -ENOMEM;

	ret = allocate_link_dais(pdev, dai_links);
	if (ret < 0)
		return ret;

	machine->asoc->dai_links = dai_links;

	link_node = of_get_child_by_name(top, PREFIX "dai-link");
	if (!link_node) {
		dev_err(&pdev->dev, "DAI links not found in DT\n");
		return -ENOENT;
	}

	do {
		struct device_node *codec = NULL, *cpu = NULL;
		struct snd_soc_dai_link *dai_link;
		int link_type = 0, codec_count = 0;

		if (!of_dai_link_is_available(link_node)) {
			link_node = of_get_next_child(top, link_node);
			continue;
		}

		dev_dbg(&pdev->dev, "parsing (%pOF)\n", link_node);

		dai_link = &dai_links[link_count];
		cpu = of_get_child_by_name(link_node, "cpu");
		if (!cpu) {
			dev_err(&pdev->dev, "cpu subnode is missing");
			ret = -ENOENT;
			goto cleanup;
		}

		/* parse CPU DAI */
		ret = parse_dai(cpu, dai_link->cpus);
		if (ret < 0)
			goto cleanup;

		for_each_child_of_node(link_node, codec) {
			/* loop over codecs only */
			if (of_node_cmp(codec->name, "codec"))
				continue;

			if (!of_property_read_bool(codec, DAI)) {
				dev_dbg(&pdev->dev,
					"sound-dai prop missing for (%pOF)\n",
					codec);
				codec_count++;
				continue;
			}

			/* parse CODEC DAI */
			ret = parse_dai(codec, &dai_link->codecs[codec_count]);
			if (ret < 0)
				goto cleanup;

			codec_count++;
		}

		/* set DAI link name */
		if (of_property_read_string(link_node,
					    "link-name",
					    &dai_link->name)) {
			ret = asoc_simple_set_dailink_name(
				&pdev->dev, dai_link, "%s-%d",
				"tegra-dlink", link_count);
			if (ret < 0)
				goto cleanup;
		}

		dai_link->dai_fmt =
			snd_soc_of_parse_daifmt(link_node, NULL,
						NULL, NULL);

		asoc_simple_canonicalize_platform(dai_link);

		of_property_read_u32(link_node, "link-type",
				     &link_type);
		switch (link_type) {
		case PCM_LINK:
			dai_link->ops = pcm_ops;
			break;
		case COMPR_LINK:
			dai_link->compr_ops = compr_ops;
			break;
		case C2C_LINK:
			/* Parse DT provided link params */
			ret = parse_dai_link_params(pdev, link_node,
							    dai_link);
				if (ret < 0)
					goto cleanup;

				break;
			default:
				dev_err(&pdev->dev, "DAI link type invalid\n");
				ret = -EINVAL;
				goto cleanup;
		}

		link_count++;
cleanup:
		of_node_put(cpu);
		if (ret < 0) {
			of_node_put(codec);
			of_node_put(link_node);
			return ret;
		}

		link_node = of_get_next_child(top, link_node);
	} while (link_node);

	card->num_links = num_links;
	card->dai_link = dai_links;

	return 0;
}

int parse_card_info(struct snd_soc_card *card, struct snd_soc_ops *pcm_ops,
		    struct snd_soc_compr_ops *compr_ops)
{
	struct tegra_machine *machine = snd_soc_card_get_drvdata(card);
	struct device_node *node = card->dev->of_node;
	int ret;

	ret = asoc_simple_parse_card_name(card, PREFIX);
	if (ret < 0)
		return ret;

	/* parse machine DAPM widgets */
	if (of_property_read_bool(node, PREFIX "widgets")) {
		ret = snd_soc_of_parse_audio_simple_widgets(card,
			PREFIX "widgets");
		if (ret < 0)
			return ret;
	}

	/*
	 * Below property of routing map is required only when there
	 * are DAPM input/output widgets available for external codec,
	 * which require them to be connected to machine source/sink
	 * DAPM widgets.
	 */
	if (of_property_read_bool(node, PREFIX "routing")) {
		ret = snd_soc_of_parse_audio_routing(card, PREFIX "routing");
		if (ret < 0)
			return ret;
	}

	parse_mclk_fs(card);

	if (of_property_read_bool(node, "fixed-pll")) {
		machine->audio_clock.fixed_pll = true;
		dev_info(card->dev, "PLL configuration is fixed from DT\n");
	}

	ret = parse_dt_dai_links(card, pcm_ops, compr_ops);
	if (ret < 0)
		return ret;

	ret = parse_dt_codec_confs(card);
	if (ret < 0)
		return ret;

	return 0;
}
EXPORT_SYMBOL_GPL(parse_card_info);

struct tegra_machine_control_data {
	struct snd_soc_pcm_runtime *rtd;
	unsigned int frame_mode;
	unsigned int master_mode;
};

static int tegra_machine_codec_set_dai_fmt(struct snd_soc_pcm_runtime *rtd,
					   unsigned int frame_mode,
					   unsigned int master_mode)
{
	unsigned int fmt = rtd->dai_link->dai_fmt;

	if (frame_mode) {
		fmt &= ~SND_SOC_DAIFMT_FORMAT_MASK;
		fmt |= frame_mode;
	}

	if (master_mode) {
		fmt &= ~SND_SOC_DAIFMT_MASTER_MASK;
		master_mode <<= ffs(SND_SOC_DAIFMT_MASTER_MASK) - 1;

		if (master_mode == SND_SOC_DAIFMT_CBM_CFM)
			fmt |= SND_SOC_DAIFMT_CBM_CFM;
		else
			fmt |= SND_SOC_DAIFMT_CBS_CFS;
	}

	return snd_soc_runtime_set_dai_fmt(rtd, fmt);
}

/*
 * The order of the below must not be changed as this
 * aligns with the SND_SOC_DAIFMT_XXX definitions in
 * include/sound/soc-dai.h.
 */
static const char * const tegra_machine_frame_mode_text[] = {
	"None",
	"i2s",
	"right-j",
	"left-j",
	"dsp-a",
	"dsp-b",
};

static int tegra_machine_codec_get_frame_mode(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct tegra_machine_control_data *data = kcontrol->private_data;

	ucontrol->value.integer.value[0] = data->frame_mode;

	return 0;
}

static int tegra_machine_codec_put_frame_mode(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct tegra_machine_control_data *data = kcontrol->private_data;
	int err;

	err = tegra_machine_codec_set_dai_fmt(data->rtd,
					      ucontrol->value.integer.value[0],
					      data->master_mode);
	if (err)
		return err;

	data->frame_mode = ucontrol->value.integer.value[0];

	return 0;
}

static const char * const tegra_machine_master_mode_text[] = {
	"None",
	"cbm-cfm",
	"cbs-cfs",
};

static int tegra_machine_codec_get_master_mode(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct tegra_machine_control_data *data = kcontrol->private_data;

	ucontrol->value.integer.value[0] = data->master_mode;

	return 0;
}

static int tegra_machine_codec_put_master_mode(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct tegra_machine_control_data *data = kcontrol->private_data;
	int err;

	err = tegra_machine_codec_set_dai_fmt(data->rtd,
					      data->frame_mode,
					      ucontrol->value.integer.value[0]);
	if (err)
		return err;

	data->master_mode = ucontrol->value.integer.value[0];

	return 0;
}

static const struct soc_enum tegra_machine_codec_frame_mode =
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(tegra_machine_frame_mode_text),
		tegra_machine_frame_mode_text);

static const struct soc_enum tegra_machine_codec_master_mode =
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(tegra_machine_master_mode_text),
		tegra_machine_master_mode_text);

static int tegra_machine_add_ctl(struct snd_soc_card *card,
				 struct snd_kcontrol_new *knew,
				 void *private_data,
				 const unsigned char *name)
{
	struct snd_kcontrol *kctl;
	int ret;

	kctl = snd_ctl_new1(knew, private_data);
	if (!kctl)
		return -ENOMEM;

	ret = snd_ctl_add(card->snd_card, kctl);
	if (ret < 0)
		return ret;

	return 0;
}

static int tegra_machine_add_frame_mode_ctl(struct snd_soc_card *card,
	struct snd_soc_pcm_runtime *rtd, const unsigned char *name,
	struct tegra_machine_control_data *data)
{
	struct snd_kcontrol_new knew = {
		.iface		= SNDRV_CTL_ELEM_IFACE_MIXER,
		.name		= name,
		.info		= snd_soc_info_enum_double,
		.index		= 0,
		.get		= tegra_machine_codec_get_frame_mode,
		.put		= tegra_machine_codec_put_frame_mode,
		.private_value	=
				(unsigned long)&tegra_machine_codec_frame_mode,
	};

	return tegra_machine_add_ctl(card, &knew, data, name);
}

static int tegra_machine_add_master_mode_ctl(struct snd_soc_card *card,
	struct snd_soc_pcm_runtime *rtd, const unsigned char *name,
	struct tegra_machine_control_data *data)
{
	struct snd_kcontrol_new knew = {
		.iface		= SNDRV_CTL_ELEM_IFACE_MIXER,
		.name		= name,
		.info		= snd_soc_info_enum_double,
		.index		= 0,
		.get		= tegra_machine_codec_get_master_mode,
		.put		= tegra_machine_codec_put_master_mode,
		.private_value	=
				(unsigned long)&tegra_machine_codec_master_mode,
	};

	return tegra_machine_add_ctl(card, &knew, data, name);
}

int tegra_machine_add_i2s_codec_controls(struct snd_soc_card *card)
{
	struct tegra_machine_control_data *data;
	struct snd_soc_pcm_runtime *rtd;
	struct device_node *np;
	char name[SNDRV_CTL_ELEM_ID_NAME_MAXLEN];
	unsigned int id;
	int ret;

	list_for_each_entry(rtd, &card->rtd_list, list) {
		np = rtd->dai_link->cpus->of_node;

		if (!np)
			continue;

		data = devm_kzalloc(card->dev, sizeof(*data), GFP_KERNEL);
		if (!data)
			return -ENOMEM;

		data->rtd = rtd;
		data->frame_mode = 0;
		data->master_mode = 0;

		if (of_property_read_u32(np, "nvidia,ahub-i2s-id", &id) < 0)
			continue;

		snprintf(name, sizeof(name), "I2S%d codec frame mode", id+1);

		ret = tegra_machine_add_frame_mode_ctl(card, rtd, name, data);
		if (ret)
			dev_warn(card->dev, "Failed to add control: %s!\n",
				 name);

		snprintf(name, sizeof(name), "I2S%d codec master mode", id+1);

		ret = tegra_machine_add_master_mode_ctl(card, rtd, name, data);
		if (ret) {
			dev_warn(card->dev, "Failed to add control: %s!\n",
				 name);
			return ret;
		}
	}

	return 0;
}
EXPORT_SYMBOL_GPL(tegra_machine_add_i2s_codec_controls);

/*
 * The order of the following definitions should align with
 * the 'snd_jack_types' enum as defined in include/sound/jack.h.
 */
static const char * const tegra_machine_jack_state_text[] = {
	"None",
	"HP",
	"MIC",
	"HS",
};

static const struct soc_enum tegra_machine_jack_state =
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(tegra_machine_jack_state_text),
		tegra_machine_jack_state_text);

static int tegra_machine_codec_get_jack_state(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_jack *jack = kcontrol->private_data;

	ucontrol->value.integer.value[0] = jack->status;

	return 0;
}

static int tegra_machine_codec_put_jack_state(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_jack *jack = kcontrol->private_data;

	snd_soc_jack_report(jack, ucontrol->value.integer.value[0],
			    SND_JACK_HEADSET);

	return 0;
}

int tegra_machine_add_codec_jack_control(struct snd_soc_card *card,
					 struct snd_soc_pcm_runtime *rtd,
					 struct snd_soc_jack *jack)
{
	char name[SNDRV_CTL_ELEM_ID_NAME_MAXLEN];
	struct snd_kcontrol_new knew = {
		.iface		= SNDRV_CTL_ELEM_IFACE_MIXER,
		.name		= name,
		.info		= snd_soc_info_enum_double,
		.index		= 0,
		.get		= tegra_machine_codec_get_jack_state,
		.put		= tegra_machine_codec_put_jack_state,
		.private_value	= (unsigned long)&tegra_machine_jack_state,
	};

	if (rtd->dais[rtd->num_cpus]->component->name_prefix)
		snprintf(name, sizeof(name), "%s Jack-state",
			 rtd->dais[rtd->num_cpus]->component->name_prefix);
	else
		snprintf(name, sizeof(name), "Jack-state");

	return tegra_machine_add_ctl(card, &knew, jack, name);
}
EXPORT_SYMBOL_GPL(tegra_machine_add_codec_jack_control);

void release_asoc_phandles(struct tegra_machine *machine)
{
	unsigned int i;

	if (machine->asoc->dai_links) {
		for (i = 0; i < machine->asoc->num_links; i++) {
			of_node_put(machine->asoc->dai_links[i].cpus->of_node);
			of_node_put(machine->asoc->dai_links[i].codecs->of_node);
		}
	}
	if (machine->asoc->codec_confs) {
		for (i = 0; i < machine->asoc->num_confs; i++)
			of_node_put(machine->asoc->codec_confs[i].dlc.of_node);
	}
}
EXPORT_SYMBOL_GPL(release_asoc_phandles);
MODULE_LICENSE("GPL");

MODULE_DESCRIPTION("Tegra ASoC Machine Utility Code");
MODULE_LICENSE("GPL v2");
