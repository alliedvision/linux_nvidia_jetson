/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * tegra210_sfc.h - Definitions for Tegra210 SFC driver
 *
 * Copyright (c) 2014-2021 NVIDIA CORPORATION.  All rights reserved.
 *
 */

#ifndef __TEGRA210_SFC_H__
#define __TEGRA210_SFC_H__

/*
 * SFC_RX registers are with respect to AXBAR.
 * The data is coming from AXBAR to SFC for playback.
 */
#define TEGRA210_SFC_RX_STATUS			0x0c
#define TEGRA210_SFC_RX_INT_STATUS		0x10
#define TEGRA210_SFC_RX_INT_MASK		0x14
#define TEGRA210_SFC_RX_INT_SET			0x18
#define TEGRA210_SFC_RX_INT_CLEAR		0x1c
#define TEGRA210_SFC_RX_CIF_CTRL		0x20
#define TEGRA210_SFC_RX_FREQ			0x24

/*
 * SFC_TX registers are with respect to AXBAR.
 * The data is going out of SFC for playback.
 */
#define TEGRA210_SFC_TX_STATUS			0x4c
#define TEGRA210_SFC_TX_INT_STATUS		0x50
#define TEGRA210_SFC_TX_INT_MASK		0x54
#define TEGRA210_SFC_TX_INT_SET			0x58
#define TEGRA210_SFC_TX_INT_CLEAR		0x5c
#define TEGRA210_SFC_TX_CIF_CTRL		0x60
#define TEGRA210_SFC_TX_FREQ			0x64

/* Register offsets from TEGRA210_SFC*_BASE */
#define TEGRA210_SFC_ENABLE			0x80
#define TEGRA210_SFC_SOFT_RESET			0x84
#define TEGRA210_SFC_CG				0x88
#define TEGRA210_SFC_STATUS			0x8c
#define TEGRA210_SFC_INT_STATUS			0x90
#define TEGRA210_SFC_COEF_RAM			0xbc
#define TEGRA210_SFC_CFG_RAM_CTRL		0xc0
#define TEGRA210_SFC_CFG_RAM_DATA		0xc4

/* Fields in TEGRA210_SFC_ENABLE */
#define TEGRA210_SFC_EN_SHIFT			0
#define TEGRA210_SFC_EN				(1 << TEGRA210_SFC_EN_SHIFT)

#define TEGRA210_SFC_NUM_RATES			13

/* Fields in TEGRA210_SFC_COEF_RAM */
#define TEGRA210_SFC_COEF_RAM_EN		BIT(0)

#define TEGRA210_SFC_SOFT_RESET_EN              BIT(0)

/* SRC coefficients */
#define TEGRA210_SFC_COEF_RAM_DEPTH		64

enum tegra210_sfc_path {
	SFC_RX_PATH,
	SFC_TX_PATH,
	SFC_PATHS,
};

struct tegra210_sfc {
	int srate_in;
	int srate_out;
	int format_in;
	int format_out;
	struct regmap *regmap;
	struct snd_pcm_hw_params in_hw_params;
	struct snd_pcm_hw_params out_hw_params;
	int audio_ch_override[SFC_PATHS];
	int client_ch_override; /* common for both TX and RX */
	int stereo_to_mono[SFC_PATHS];
	int mono_to_stereo[SFC_PATHS];
};

#endif
