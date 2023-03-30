/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2015-2022, NVIDIA CORPORATION.  All rights reserved.
 */

#ifndef INCLUDE_RTCPU_MONITOR_H
#define INCLUDE_RTCPU_MONITOR_H

struct device;
struct tegra_camrtc_mon;

int tegra_camrtc_mon_restore_rtcpu(struct tegra_camrtc_mon *);
struct tegra_camrtc_mon *tegra_camrtc_mon_create(struct device *);
int tegra_cam_rtcpu_mon_destroy(struct tegra_camrtc_mon *);

#endif /* INCLUDE_RTCPU_MONITOR_H */
