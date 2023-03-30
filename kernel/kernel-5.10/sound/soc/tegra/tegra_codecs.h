/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * tegra_codec.h
 *
 * Copyright (c) 2021 NVIDIA CORPORATION.  All rights reserved.
 *
 */

#ifndef __TEGRA_CODECS_H__
#define __TEGRA_CODECS_H__

#include <sound/soc.h>

int tegra_codecs_init(struct snd_soc_card *card);

int tegra_codecs_runtime_setup(struct snd_soc_card *card,
			       unsigned int srate,
			       unsigned int channels,
			       unsigned int aud_mclk);

#endif
