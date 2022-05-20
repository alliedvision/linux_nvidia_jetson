// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2016-2021, NVIDIA CORPORATION.  All rights reserved.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <soc/tegra/chip-id.h>
#include <soc/tegra/fuse.h>

#define TEGRAID_CHIPID_MASK 0xFF00
#define TEGRAID_CHIPID_SHIFT 8
#define TEGRAID_MAJOR_MASK 0xF0
#define TEGRAID_MAJOR_SHIFT 4
#define TEGRAID_MINOR_MASK 0xF0000
#define TEGRAID_MINOR_SHIFT 16
#define TEGRAID_NETLIST_MASK 0xFF
#define TEGRAID_PATCH_MASK 0xFF00
#define TEGRAID_PATCH_SHIFT 8

struct tegra_id {
	enum tegra_chipid chipid;
	enum tegra_revision revision;
	unsigned int major;
	unsigned int minor;
	unsigned int netlist;
	unsigned int patch;
	char *priv;
};

static const char *tegra_platform_name[TEGRA_PLATFORM_MAX] = {
	[TEGRA_PLATFORM_SILICON] = "silicon",
	[TEGRA_PLATFORM_QT]      = "quickturn",
	[TEGRA_PLATFORM_LINSIM]  = "linsim",
	[TEGRA_PLATFORM_FPGA]    = "fpga",
	[TEGRA_PLATFORM_UNIT_FPGA] = "unit fpga",
	[TEGRA_PLATFORM_VDK] = "vdk",
};

static struct tegra_id tegra_id;
static const char *tegra_platform_ptr;
static const char *tegra_cpu_ptr;
static u32 prod_mode;

static int get_platform(char *val, const struct kernel_param *kp)
{
	enum tegra_platform platform;

	platform = tegra_get_platform();
	tegra_platform_ptr = tegra_platform_name[platform];
	return param_get_charp(val, kp);
}
static struct kernel_param_ops tegra_platform_ops = {
	.get = get_platform,
};
module_param_cb(tegra_platform, &tegra_platform_ops, &tegra_platform_ptr, 0444);

static int get_cpu_type(char *val, const struct kernel_param *kp)
{
	enum tegra_platform platform;

	if (tegra_cpu_is_asim()) {
		tegra_cpu_ptr = "asim";
	} else {
		platform = tegra_get_platform();
		tegra_cpu_ptr = tegra_platform_name[platform];
	}
	return param_get_charp(val, kp);
}
static struct kernel_param_ops tegra_cpu_ops = {
	.get = get_cpu_type,
};
module_param_cb(tegra_cpu, &tegra_cpu_ops, &tegra_cpu_ptr, 0444);

static int get_chip_id(char *val, const struct kernel_param *kp)
{
	if (tegra_id.chipid == TEGRA_CHIPID_UNKNOWN)
		tegra_set_tegraid_from_hw();

	return param_get_uint(val, kp);
}

static int get_revision(char *val, const struct kernel_param *kp)
{
	if (tegra_id.revision == TEGRA_REVISION_UNKNOWN)
		tegra_set_tegraid_from_hw();

	return param_get_uint(val, kp);
}

static int get_major_rev(char *val, const struct kernel_param *kp)
{
	if (tegra_id.revision == TEGRA_REVISION_UNKNOWN)
		tegra_set_tegraid_from_hw();

	return param_get_uint(val, kp);
}

static struct kernel_param_ops tegra_chip_id_ops = {
	.get = get_chip_id,
};

static struct kernel_param_ops tegra_revision_ops = {
	.get = get_revision,
};

static struct kernel_param_ops tegra_major_rev_ops = {
	.get = get_major_rev,
};

module_param_cb(tegra_chip_id, &tegra_chip_id_ops, &tegra_id.chipid, 0444);
module_param_cb(tegra_chip_rev, &tegra_revision_ops, &tegra_id.revision, 0444);
module_param_cb(tegra_chip_major_rev,
		&tegra_major_rev_ops, &tegra_id.major, 0444);

static int get_prod_mode(char *val, const struct kernel_param *kp)
{
	u32 reg = 0;
	int ret;

	if (tegra_get_platform() == TEGRA_PLATFORM_SILICON) {
		ret = tegra_fuse_readl(TEGRA_FUSE_PRODUCTION_MODE, &reg);
		if (!ret)
			prod_mode = reg;
	}
	return param_get_uint(val, kp);
}

static struct kernel_param_ops tegra_prod_mode_ops = {
	.get = get_prod_mode,
};

module_param_cb(tegra_prod_mode, &tegra_prod_mode_ops, &prod_mode, 0444);

void tegra_set_tegraid_from_hw(void)
{
	u32 cid;
	u32 emu_id;

	cid = tegra_read_chipid();
	emu_id = tegra_read_emu_revid();

	tegra_id.chipid  = (cid & TEGRAID_CHIPID_MASK) >> TEGRAID_CHIPID_SHIFT;
	tegra_id.major = (cid & TEGRAID_MAJOR_MASK) >> TEGRAID_MAJOR_SHIFT;
	tegra_id.minor   = (cid & TEGRAID_MINOR_MASK) >> TEGRAID_MINOR_SHIFT;
	tegra_id.netlist = emu_id & TEGRAID_NETLIST_MASK;
	tegra_id.patch   = (emu_id & TEGRAID_PATCH_MASK) >> TEGRAID_PATCH_SHIFT;
	tegra_id.revision = tegra_sku_info.revision;
}

enum tegra_chipid tegra_get_chipid(void)
{
	if (tegra_id.chipid == TEGRA_CHIPID_UNKNOWN)
		tegra_set_tegraid_from_hw();

	return tegra_id.chipid;
}
EXPORT_SYMBOL(tegra_get_chipid);
