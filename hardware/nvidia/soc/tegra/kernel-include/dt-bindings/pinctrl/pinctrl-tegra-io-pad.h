/*
 * Copyright (c) 2016-2020, NVIDIA CORPORATION.  All rights reserved.
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

/*
 * pinctrl-tegra-io-pad.h: Provides constants for Tegra IO pads
 * 			   pinctrl bindings.
 */

#ifndef _DT_BINDINGS_PINCTRL_TEGRA_IO_PAD_H
#define _DT_BINDINGS_PINCTRL_TEGRA_IO_PAD_H

/* Power source voltage of IO pads. */
#if TEGRA_SDMMC_VERSION >= DT_VERSION_2
#define TEGRA_IO_PAD_VOLTAGE_1V8	0
#define TEGRA_IO_PAD_VOLTAGE_3V3	1
#endif
#define TEGRA_IO_PAD_VOLTAGE_1800000UV		0
#define TEGRA_IO_PAD_VOLTAGE_3300000UV		1

#endif

