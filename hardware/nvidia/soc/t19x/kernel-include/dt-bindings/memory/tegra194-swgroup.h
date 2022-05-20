/*
 * Copyright (c) 2017-2020, NVIDIA CORPORATION.  All rights reserved.
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
#ifndef _DT_BINDINGS_MEMORY_TEGRA194_SWGROUP_H
#define _DT_BINDINGS_MEMORY_TEGRA194_SWGROUP_H

/*
 * This is the t19x specific component of the new SID dt-binding.
 */
#define TEGRA_SID_RCE		0x2aU	/* 42 */
#define TEGRA_SID_RCE_VM2	0x2bU	/* 43 */

#define TEGRA_SID_RCE_RM	0x2FU	/* 47 */
#define TEGRA_SID_VIFALC	0x30U	/* 48 */
#define TEGRA_SID_ISPFALC	0x31U	/* 49 */

#define TEGRA_SID_MIU		0x50U	/* 80 */

#define TEGRA_SID_NVDLA0	0x51U	/* 81 */
#define TEGRA_SID_NVDLA1	0x52U	/* 82 */

#define TEGRA_SID_PVA0		0x53U	/* 83 */
#define TEGRA_SID_PVA1		0x54U	/* 84 */

#define TEGRA_SID_NVENC1	0x55U	/* 85 */

#define TEGRA_SID_PCIE0		0x56U	/* 86 */
#define TEGRA_SID_PCIE1		0x57U	/* 87 */
#define TEGRA_SID_PCIE2		0x58U	/* 88 */
#define TEGRA_SID_PCIE3		0x59U	/* 89 */
#define TEGRA_SID_PCIE4		0x5AU	/* 90 */
#define TEGRA_SID_PCIE5		0x5BU	/* 91 */

#define TEGRA_SID_NVDEC1	0x5CU	/* 92 */

#define TEGRA_SID_RCE_VM3	0x61U	/* 97 */

#define TEGRA_SID_VI_VM2	0x62U	/* 98 */
#define TEGRA_SID_VI_VM3	0x63U	/* 99 */
#define TEGRA_SID_RCE_SERVER	0x64U	/* 100 */

/* EQOS virtual functions */
#define TEGRA_SID_EQOS_VF1     0x65U    /* 101 */
#define TEGRA_SID_EQOS_VF2     0x66U    /* 102 */
#define TEGRA_SID_EQOS_VF3     0x67U    /* 103 */
#define TEGRA_SID_EQOS_VF4     0x68U    /* 104 */

#define TEGRA_SID_GPCDMA_8	0x69U	/* 105 */
#define TEGRA_SID_GPCDMA_9	0x6aU	/* 106 */
#define TEGRA_SID_GPCDMA_10	0x6bU	/* 107 */
#define TEGRA_SID_GPCDMA_11	0x6cU	/* 108 */
#define TEGRA_SID_GPCDMA_12	0x6dU	/* 109 */
#define TEGRA_SID_GPCDMA_13	0x6eU	/* 110 */
#define TEGRA_SID_GPCDMA_14	0x6fU	/* 111 */
#define TEGRA_SID_GPCDMA_15	0x70U	/* 112 */

#endif
