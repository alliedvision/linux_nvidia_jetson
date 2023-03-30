/*
 * tegra_asoc_dt_parser.c - Tegra DAI links parser
 *
 * Copyright (c) 2019 NVIDIA CORPORATION.  All rights reserved.
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
#include <linux/of.h>
#include <sound/simple_card_utils.h>
#include <sound/jack.h>
#include <sound/soc.h>
#include "tegra_asoc_machine_alt.h"

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
	struct device_node *link_node, *codec;
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

		/*
		 * depending on the number of codec subnodes, DAI link
		 * count is incremented. DT can have one DAI link entry
		 * with multiple codec nodes(for ex: DSPK), driver can
		 * create multiple links out of it.
		 */
		num_codecs = of_get_child_count_with_name(link_node,
							  "codec");
		if (!num_codecs) {
			of_node_put(link_node);
			dev_err(&pdev->dev,
				"no codec subnode or sound-dai property\n");
			return -EINVAL;
		}

		for_each_child_of_node(link_node, codec) {
			if (of_node_cmp(codec->name, "codec"))
				continue;

			if (of_property_read_bool(codec, DAI))
				link_count++;
		}

		link_node = of_get_next_child(top, link_node);
	} while (link_node);

	*num_links = link_count;

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
				 &machine->audio_clock.mclk_scale))
		dev_dbg(&pdev->dev, "'%smclk-fs' property is missing\n",
			PREFIX);
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

			codec_confs[i].of_node = args.np;
			codec_confs[i].dev_name = NULL;
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

		cpu = of_get_child_by_name(link_node, "cpu");
		if (!cpu) {
			dev_err(&pdev->dev, "cpu subnode is missing");
			ret = -ENOENT;
			goto cleanup;
		}

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

			dai_link = &dai_links[link_count];

			/* parse CPU DAI */
			ret = asoc_simple_card_parse_cpu(cpu, dai_link, DAI,
							 CELL, NULL);
			if (ret < 0)
				goto cleanup;

			/* parse CODEC DAI */
			ret = asoc_simple_card_parse_codec(codec, dai_link,
							   DAI, CELL);
			if (ret < 0)
				goto cleanup;

			/* set DAI link name */
			if (of_property_read_string_index(link_node,
							  "link-name",
							  codec_count,
							  &dai_link->name)) {
				ret = asoc_simple_card_set_dailink_name(
					&pdev->dev, dai_link, "%s-%d",
					"tegra-dlink", link_count);
				if (ret < 0)
					goto cleanup;
			}

			asoc_simple_card_parse_daifmt(&pdev->dev, link_node,
						      codec, NULL,
						      &dai_link->dai_fmt);

			of_property_read_u32(link_node, "link-type",
					     &link_type);
			switch (link_type) {
			case PCM_LINK:
				dai_link->ops = pcm_ops;
				asoc_simple_card_canonicalize_dailink(dai_link);
				dai_link->ignore_pmdown_time = 1;
				break;
			case COMPR_LINK:
				dai_link->compr_ops = compr_ops;
				asoc_simple_card_canonicalize_dailink(dai_link);
				dai_link->ignore_pmdown_time = 1;
				break;
			case C2C_LINK:
				dai_link->params = &link_params;
				break;
			default:
				dev_err(&pdev->dev, "DAI link type invalid\n");
				ret = -EINVAL;
				goto cleanup;
			}

			link_count++;
			codec_count++;
		}
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
	struct device_node *node = card->dev->of_node;
	int ret;

	ret = asoc_simple_card_parse_card_name(card, PREFIX);
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

	ret = parse_dt_dai_links(card, pcm_ops, compr_ops);
	if (ret < 0)
		return ret;

	ret = parse_dt_codec_confs(card);
	if (ret < 0)
		return ret;

	return 0;
}
EXPORT_SYMBOL_GPL(parse_card_info);

MODULE_DESCRIPTION("Tegra ASoC Machine driver DT parser code");
MODULE_AUTHOR("Sameer Pujar <spujar@nvidia.com>");
MODULE_LICENSE("GPL v2");
