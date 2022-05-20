/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * tegra_asoc_machine.h
 *
 * Copyright (c) 2014-2020 NVIDIA CORPORATION.  All rights reserved.
 *
 */

#ifndef __TEGRA_ASOC_MACHINE_H__
#define __TEGRA_ASOC_MACHINE_H__

#include "tegra_asoc_utils.h"

/*
 * struct tegra_asoc - ASoC topology of dai links and codec confs
 * @codec_confs: Configuration of codecs from xbar and devicetree
 * @dai_links: All DAI links from xbar and device tree
 * @num_links: Total number of DAI links for given card
 * @num_confs: Total number of codec confs for given card
 * @tx_slot: TDM slot for Tx path
 * @rx_slot: TDM slot for Rx path
 */
struct tegra_asoc {
	struct snd_soc_codec_conf *codec_confs;
	struct snd_soc_dai_link *dai_links;
	unsigned int num_links;
	unsigned int num_confs;
	unsigned int *tx_slot;
	unsigned int *rx_slot;
};

/* machine structure which holds sound card */
struct tegra_machine {
	struct tegra_asoc_utils_data audio_clock;
	struct tegra_asoc *asoc;
	unsigned int num_codec_links;
	int rate_via_kcontrol;
	int fmt_via_kcontrol;
};

int tegra_machine_add_i2s_codec_controls(struct snd_soc_card *card);
int tegra_machine_add_codec_jack_control(struct snd_soc_card *card,
					 struct snd_soc_pcm_runtime *rtd,
					 struct snd_soc_jack *jack);

void release_asoc_phandles(struct tegra_machine *machine);

/*
 * new helper functions for parsing all DAI links from DT.
 * Representation of XBAR and codec links would be similar.
 */
int parse_card_info(struct snd_soc_card *card, struct snd_soc_ops *pcm_ops,
		    struct snd_soc_compr_ops *compr_ops);

#endif
