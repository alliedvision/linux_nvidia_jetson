/*
 * Copyright (c) 2019-2022, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#include <nvgpu/vpr.h>

#include <linux/init.h>

#if NVGPU_VPR_RESIZE_SUPPORTED
#include <linux/platform/tegra/common.h>
#endif

bool nvgpu_is_vpr_resize_enabled(void)
{
#if NVGPU_VPR_RESIZE_SUPPORTED
	return tegra_is_vpr_resize_enabled();
#else
	return false;
#endif
}
