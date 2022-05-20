// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2014-2020, NVIDIA CORPORATION.  All rights reserved.
 */

#include <linux/version.h>
#include <soc/tegra/fuse.h>
#include <soc/tegra/chip-id.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 15, 0)
#include "fuse.h"
#endif

#define MINOR_QT		0
#define MINOR_FPGA		1
#define MINOR_ASIM_QT		2
#define MINOR_ASIM_LINSIM	3
#define MINOR_DSIM_ASIM_LINSIM	4
#define MINOR_UNIT_FPGA		5
#define MINOR_VDK		6

#define PRE_SI_QT		1
#define PRE_SI_FPGA		2
#define PRE_SI_UNIT_FPGA	3
#define PRE_SI_ASIM_QT		4
#define PRE_SI_ASIM_LINSIM	5
#define PRE_SI_DSIM_ASIM_LINSIM	6
#define PRE_SI_VDK		8
#define PRE_SI_VSP		9

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 15, 0)
static u8 tegra_get_pre_si_plat(void)
{
	u8 chip_id = tegra_get_chip_id();
	u8 val;

	switch (chip_id) {
	case TEGRA194:
	case TEGRA234:
		val = (tegra_read_chipid() >> 0x14) & 0xf;
		break;
	default:
		val = 0;
		break;
	}
	return val;
}
#endif

static enum tegra_platform __tegra_get_platform(void)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 15, 0)
	u32 chipid = tegra_read_chipid();
	u32 major = tegra_hidrev_get_majorrev(chipid);
	u32 pre_si_plat = tegra_hidrev_get_pre_si_plat(chipid);
#else
	u32 major = tegra_get_major_rev();
	u32 pre_si_plat = tegra_get_pre_si_plat();
#endif

	/* Having VSP defined only since K4.14 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
	if (pre_si_plat == PRE_SI_VSP)
		return TEGRA_PLATFORM_VSP;
#endif

	if (!major) {
		u32 minor;

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 15, 0)
		minor = tegra_hidrev_get_minorrev(chipid);
#else
		minor = tegra_get_minor_rev();
#endif
		switch (minor) {
		case MINOR_QT:
			return TEGRA_PLATFORM_QT;
		case MINOR_FPGA:
			return TEGRA_PLATFORM_FPGA;
		case MINOR_ASIM_QT:
			return TEGRA_PLATFORM_QT;
		case MINOR_ASIM_LINSIM:
			return TEGRA_PLATFORM_LINSIM;
		case MINOR_DSIM_ASIM_LINSIM:
			return TEGRA_PLATFORM_LINSIM;
		case MINOR_UNIT_FPGA:
			return TEGRA_PLATFORM_UNIT_FPGA;
		case MINOR_VDK:
			return TEGRA_PLATFORM_VDK;
		}
	} else if (pre_si_plat) {
		switch (pre_si_plat) {
		case PRE_SI_QT:
			return TEGRA_PLATFORM_QT;
		case PRE_SI_FPGA:
			return TEGRA_PLATFORM_FPGA;
		case PRE_SI_UNIT_FPGA:
			return TEGRA_PLATFORM_UNIT_FPGA;
		case PRE_SI_ASIM_QT:
			return TEGRA_PLATFORM_QT;
		case PRE_SI_ASIM_LINSIM:
			return TEGRA_PLATFORM_LINSIM;
		case PRE_SI_DSIM_ASIM_LINSIM:
			return TEGRA_PLATFORM_LINSIM;
		case PRE_SI_VDK:
			return TEGRA_PLATFORM_VDK;
	/* Having VSP defined only since K4.14 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
		case PRE_SI_VSP:
			return TEGRA_PLATFORM_VSP;
#endif
		}
	}

	return TEGRA_PLATFORM_SILICON;
}

static enum tegra_platform tegra_platform_id = TEGRA_PLATFORM_MAX;

enum tegra_platform tegra_get_platform(void)
{
	if (unlikely(tegra_platform_id == TEGRA_PLATFORM_MAX))
		tegra_platform_id = __tegra_get_platform();

	return tegra_platform_id;
}
EXPORT_SYMBOL(tegra_get_platform);

bool tegra_cpu_is_asim(void)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 15, 0)
	u32 chipid = tegra_read_chipid();
	u32 major = tegra_hidrev_get_majorrev(chipid);
	u32 pre_si_plat = tegra_hidrev_get_pre_si_plat(chipid);
#else
	u32 major = tegra_get_major_rev();
	u32 pre_si_plat = tegra_get_pre_si_plat();
#endif

	if (!major) {
		u32 minor;

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 15, 0)
		minor = tegra_hidrev_get_minorrev(chipid);
#else
		minor = tegra_get_minor_rev();
#endif
		switch (minor) {
		case MINOR_QT:
		case MINOR_FPGA:
			return false;
		case MINOR_ASIM_QT:
		case MINOR_ASIM_LINSIM:
		case MINOR_DSIM_ASIM_LINSIM:
		case MINOR_UNIT_FPGA:
		case MINOR_VDK:
			return true;
		}
	} else if (pre_si_plat) {
		switch (pre_si_plat) {
		case PRE_SI_QT:
		case PRE_SI_FPGA:
			return false;
		case PRE_SI_UNIT_FPGA:
		case PRE_SI_ASIM_QT:
		case PRE_SI_ASIM_LINSIM:
		case PRE_SI_DSIM_ASIM_LINSIM:
		case PRE_SI_VDK:
			return true;
		}
	}

	return false;
}
EXPORT_SYMBOL_GPL(tegra_cpu_is_asim);

bool tegra_cpu_is_dsim(void)
{
	u32 chipid, major, pre_si_plat;

	chipid = tegra_read_chipid();
	major = tegra_hidrev_get_majorrev(chipid);
	pre_si_plat = tegra_hidrev_get_pre_si_plat(chipid);

	if (!major) {
		u32 minor;

		minor = tegra_hidrev_get_minorrev(chipid);
		switch (minor) {
		case MINOR_QT:
		case MINOR_FPGA:
		case MINOR_ASIM_QT:
		case MINOR_ASIM_LINSIM:
		case MINOR_UNIT_FPGA:
		case MINOR_VDK:
			return false;
		case MINOR_DSIM_ASIM_LINSIM:
			return true;
		}
	} else if (pre_si_plat) {
		switch (pre_si_plat) {
		case PRE_SI_QT:
		case PRE_SI_FPGA:
		case PRE_SI_UNIT_FPGA:
		case PRE_SI_ASIM_QT:
		case PRE_SI_ASIM_LINSIM:
		case PRE_SI_VDK:
			return false;
		case PRE_SI_DSIM_ASIM_LINSIM:
			return true;
		}
	}

	return false;
}
