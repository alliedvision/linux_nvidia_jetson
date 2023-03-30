/*
 * Copyright (c) 2014-2020, NVIDIA CORPORATION.  All rights reserved.
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
 * Author: Mikko Perttunen <mperttunen@nvidia.com>
 */



#ifndef _DT_BINDINGS_THERMAL_TEGRA124_SOCTHERM_H
#define _DT_BINDINGS_THERMAL_TEGRA124_SOCTHERM_H

#define TEGRA124_SOCTHERM_SENSOR_CPU 0
#define TEGRA124_SOCTHERM_SENSOR_MEM 1
#define TEGRA124_SOCTHERM_SENSOR_GPU 2
#define TEGRA124_SOCTHERM_SENSOR_PLLX 3
#define TEGRA124_SOCTHERM_SENSOR_NUM 4

#define TEGRA_SOCTHERM_THROT_LEVEL_NONE 0
#define TEGRA_SOCTHERM_THROT_LEVEL_LOW 1
#define TEGRA_SOCTHERM_THROT_LEVEL_MED 2
#define TEGRA_SOCTHERM_THROT_LEVEL_HIGH 3


#endif
