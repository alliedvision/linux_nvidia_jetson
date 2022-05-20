/*
 * Copyright (c) 2015-2020, NVIDIA CORPORATION.  All rights reserved.
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
 * NVIDIA Tegra Video Input Device Driver
 * Author: Bryan Wu <pengw@nvidia.com>
 */

#ifndef __DT_BINDINGS_MEDIA_TEGRA_VI_H__
#define __DT_BINDINGS_MEDIA_TEGRA_VI_H__

/*
 * Supported CSI to VI Data Formats
 */
#define TEGRA_VF_RAW6          0
#define TEGRA_VF_RAW7          1
#define TEGRA_VF_RAW8          2
#define TEGRA_VF_RAW10         3
#define TEGRA_VF_RAW12         4
#define TEGRA_VF_RAW14         5
#define TEGRA_VF_EMBEDDED8     6
#define TEGRA_VF_RGB565                7
#define TEGRA_VF_RGB555                8
#define TEGRA_VF_RGB888                9
#define TEGRA_VF_RGB444                10
#define TEGRA_VF_RGB666                11
#define TEGRA_VF_YUV422                12
#define TEGRA_VF_YUV420                13
#define TEGRA_VF_YUV420_CSPS   14

#endif /* __DT_BINDINGS_MEDIA_TEGRA_VI_H__ */
