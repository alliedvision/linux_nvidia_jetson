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
 * This header provides macros for nvidia,xhci controller
 * device bindings.
 */

#ifndef __DT_TEGRA_XHCI_H__
#define __DT_TEGRA_XHCI_H__

#define TEGRA_XHCI_SS_P0	0
#define TEGRA_XHCI_SS_P1	1
#define TEGRA_XHCI_SS_P2	2
#define TEGRA_XHCI_SS_P3	3

#define TEGRA_XHCI_USB2_P0	0
#define TEGRA_XHCI_USB2_P1	1
#define TEGRA_XHCI_USB2_P2	2
#define TEGRA_XHCI_USB2_P3	3

#define TEGRA_XHCI_LANE_0	0
#define TEGRA_XHCI_LANE_1	1
#define TEGRA_XHCI_LANE_2	2
#define TEGRA_XHCI_LANE_3	3
#define TEGRA_XHCI_LANE_4	4
#define TEGRA_XHCI_LANE_5	5
#define TEGRA_XHCI_LANE_6	6

#define TEGRA_XHCI_PORT_OTG	1
#define TEGRA_XHCI_PORT_STD	0

#define TEGRA_XHCI_UNUSED_PORT	7
#define TEGRA_XHCI_UNUSED_LANE	0xF

#endif
