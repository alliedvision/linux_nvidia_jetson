/*
 * Copyright (c) 2019, NVIDIA CORPORATION. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __UAPI_TEGRA_HV_PTP_IOCTL_H__
#define __UAPI_TEGRA_HV_PTP_IOCTL_H__

struct tegra_hv_ptp_payload {
	__u32 phc_sec;
	__u32 phc_ns;
	__u64 gt;
	__u16 currentUtcOffset;
	__u8  currentUtcOffsetValid;
};

#define TEGRA_HV_PTP_GETTIME	_IOR('p', 0x1, struct tegra_hv_ptp_payload *)

#endif
