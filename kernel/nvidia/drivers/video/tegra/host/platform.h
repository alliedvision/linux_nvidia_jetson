/*
 * Copyright (c) 2020-2022, NVIDIA Corporation.  All rights reserved.
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

#ifndef __NVHOST_PLATFORM_H
#define __NVHOST_PLATFORM_H

#include <linux/version.h>

#include <soc/tegra/fuse-helper.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(5,5,0)

#include <soc/tegra/chip-id.h>

#else

#ifndef TEGRA_FUSE_HAS_PLATFORM_APIS

static inline bool tegra_platform_is_vdk(void)
{
	return false;
}

static inline bool tegra_platform_is_sim(void)
{
	return false;
}

static inline bool tegra_platform_is_fpga(void)
{
	return false;
}

static inline bool tegra_cpu_is_asim(void)
{
	return false;
}

static inline bool tegra_platform_is_qt(void)
{
	return false;
}

static inline bool tegra_platform_is_silicon(void)
{
	return true;
}

#endif

#ifndef TEGRA186
#define TEGRA186 0x18
#endif

#ifndef TEGRA194
#define TEGRA194 0x19
#endif

#endif // LINUX_VERSION_CODE < KERNEL_VERSION(5,5,0)

#endif // __NVHOST_PLATFORM_H
