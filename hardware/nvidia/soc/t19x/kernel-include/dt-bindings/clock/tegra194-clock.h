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

#ifndef _DT_BINDINGS_CLOCK_TEGRA194_CLOCK_H
#define _DT_BINDINGS_CLOCK_TEGRA194_CLOCK_H

#include <dt-bindings/clock/tegra194-clk.h>

#if (TEGRA_BPMP_FW_DT_VERSION >= DT_VERSION_2)
#define bpmp_clks bpmp
#endif

#endif
