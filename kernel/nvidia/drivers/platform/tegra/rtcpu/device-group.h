/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2022, NVIDIA CORPORATION.  All rights reserved.
 */

#ifndef INCLUDE_DEVICE_GROUP_H
#define INCLUDE_DEVICE_GROUP_H

struct device;
struct platform_device;

struct camrtc_device_group *camrtc_device_group_get(
	struct device *dev,
	const char *property_name,
	const char *names_property_name);

struct platform_device *camrtc_device_get_byname(
	struct camrtc_device_group *grp,
	const char *device_name);

int camrtc_device_group_busy(const struct camrtc_device_group *grp);
void camrtc_device_group_idle(const struct camrtc_device_group *grp);
void camrtc_device_group_reset(const struct camrtc_device_group *grp);

#endif /* INCLUDE_DEVICE_GROUP_H */
