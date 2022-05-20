/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2021 NVIDIA Corporation
 */
#ifndef __SOC_TEGRA_PADCTRL_H__
#define __SOC_TEGRA_PADCTRL_H__

#define TEGRA_APBMISC_SDMMC1_EXPRESS_MODE 0x4004
#define TEGRA_APBMISC_SDMMC1_EXPRESS_MODE_SDLEGACY 0
#define TEGRA_APBMISC_SDMMC1_EXPRESS_MODE_SDEXP 1

void tegra_misc_sd_exp_mux_select(bool sd_exp_en);

#endif /* __SOC_TEGRA_PADCTRL_H__ */
