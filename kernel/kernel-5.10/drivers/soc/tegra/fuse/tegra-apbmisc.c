// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2014-2021, NVIDIA CORPORATION.  All rights reserved.
 */

#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/io.h>

#include <soc/tegra/fuse.h>
#include <soc/tegra/common.h>
#include <soc/tegra/padctrl.h>

#include "fuse.h"

#define FUSE_SKU_INFO	0x10
#define ERD_MASK_INBAND_ERR 0x1

#define TEGRA_APBMISC_EMU_REVID 0x60
#define TEGRA_MISCREG_EMU_REVID 0x3160
#define ERD_MASK_INBAND_ERR 0x1

#define T210B01_MAJOR_REV 2

#define PMC_STRAPPING_OPT_A_RAM_CODE_SHIFT	4
#define PMC_STRAPPING_OPT_A_RAM_CODE_MASK_LONG	\
	(0xf << PMC_STRAPPING_OPT_A_RAM_CODE_SHIFT)
#define PMC_STRAPPING_OPT_A_RAM_CODE_MASK_SHORT	\
	(0x3 << PMC_STRAPPING_OPT_A_RAM_CODE_SHIFT)

/* platform macros used for major rev 0 */
#define MINOR_QT		0
#define MINOR_FPGA		1
#define MINOR_ASIM_QT		2
#define MINOR_ASIM_LINSIM	3
#define MINOR_DSIM_ASIM_LINSIM  4
#define MINOR_UNIT_FPGA         5
#define MINOR_VDK		6

/* platform macros used in pre-silicon */
#define PRE_SI_QT		1
#define PRE_SI_FPGA		2
#define PRE_SI_UNIT_FPGA        3
#define PRE_SI_ASIM_QT		4
#define PRE_SI_ASIM_LINSIM	5
#define PRE_SI_DSIM_ASIM_LINSIM 6
#define PRE_SI_VDK		8
#define PRE_SI_VSP		9

struct apbmisc_data {
	u32 emu_revid_offset;
};

static void __iomem *apbmisc_base;
static const struct apbmisc_data *apbmisc_data;

static const struct apbmisc_data tegra20_apbmisc_data = {
	.emu_revid_offset = TEGRA_APBMISC_EMU_REVID
};

static const struct apbmisc_data tegra186_apbmisc_data = {
	.emu_revid_offset = TEGRA_MISCREG_EMU_REVID
};

static void __iomem *apbmisc_base;
static bool long_ram_code;
static u32 strapping;
static u32 chipid;

u32 tegra_read_chipid(void)
{
	WARN(!chipid, "Tegra APB MISC not yet available\n");

	return chipid;
}
EXPORT_SYMBOL(tegra_read_chipid);

u8 tegra_get_chip_id(void)
{
	return (tegra_read_chipid() >> 8) & 0xff;
}
EXPORT_SYMBOL(tegra_get_chip_id);

static u8 tegra_get_pre_si_plat(void)
{
	u8 val;
	u8 chip_id;

	chip_id = tegra_get_chip_id();
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

u8 tegra_get_major_rev(void)
{
	return (tegra_read_chipid() >> 4) & 0xf;
}
EXPORT_SYMBOL(tegra_get_major_rev);

u8 tegra_get_minor_rev(void)
{
	return (tegra_read_chipid() >> 16) & 0xf;
}
EXPORT_SYMBOL(tegra_get_minor_rev);

u32 tegra_read_straps(void)
{
	WARN(!chipid, "Tegra ABP MISC not yet available\n");

	return strapping;
}

u32 tegra_read_ram_code(void)
{
	u32 straps = tegra_read_straps();

	if (long_ram_code)
		straps &= PMC_STRAPPING_OPT_A_RAM_CODE_MASK_LONG;
	else
		straps &= PMC_STRAPPING_OPT_A_RAM_CODE_MASK_SHORT;

	return straps >> PMC_STRAPPING_OPT_A_RAM_CODE_SHIFT;
}

/*
 * The function sets ERD(Error Response Disable) bit.
 * This allows to mask inband errors and always send an
 * OKAY response from CBB to the master which caused error.
 */
int tegra_miscreg_set_erd(u64 err_config)
{
	int err = 0;

	if (of_machine_is_compatible("nvidia,tegra194") && apbmisc_base) {
		writel_relaxed(ERD_MASK_INBAND_ERR, apbmisc_base + err_config);
	} else {
		WARN(1, "Tegra ABP MISC not yet available\n");
		return -ENODEV;
	}
	return err;
}
EXPORT_SYMBOL(tegra_miscreg_set_erd);

static const struct of_device_id apbmisc_match[] __initconst = {
	{ .compatible = "nvidia,tegra20-apbmisc",
		.data = &tegra20_apbmisc_data, },
	{ .compatible = "nvidia,tegra186-misc",
		.data = &tegra186_apbmisc_data, },
	{ .compatible = "nvidia,tegra194-misc",
		.data = &tegra186_apbmisc_data, },
	{ .compatible = "nvidia,tegra234-misc",
		.data = &tegra186_apbmisc_data, },
	{},
};

void tegra_init_revision(void)
{
	u8 chip_id, minor_rev;

	chip_id = tegra_get_chip_id();
	minor_rev = tegra_get_minor_rev();

	switch (minor_rev) {
	case 1:
		tegra_sku_info.revision = TEGRA_REVISION_A01;
		break;
	case 2:
		tegra_sku_info.revision = TEGRA_REVISION_A02;
		break;
	case 3:
		if (chip_id == TEGRA20 && (tegra_fuse_read_spare(18) ||
					   tegra_fuse_read_spare(19)))
			tegra_sku_info.revision = TEGRA_REVISION_A03p;
		else
			tegra_sku_info.revision = TEGRA_REVISION_A03;
		break;
	case 4:
		tegra_sku_info.revision = TEGRA_REVISION_A04;
		break;
	default:
		tegra_sku_info.revision = TEGRA_REVISION_UNKNOWN;
	}

	tegra_sku_info.sku_id = tegra_fuse_read_early(FUSE_SKU_INFO);
}

void __init tegra_init_apbmisc(void)
{
	void __iomem *strapping_base;
	struct resource apbmisc, straps;
	struct device_node *np;
	const struct of_device_id *match;

	np = of_find_matching_node_and_match(NULL, apbmisc_match, &match);
	if (!np) {
		/*
		 * Fall back to legacy initialization for 32-bit ARM only. All
		 * 64-bit ARM device tree files for Tegra are required to have
		 * an APBMISC node.
		 *
		 * This is for backwards-compatibility with old device trees
		 * that didn't contain an APBMISC node.
		 */
		if (IS_ENABLED(CONFIG_ARM) && soc_is_tegra()) {
			/* APBMISC registers (chip revision, ...) */
			apbmisc.start = 0x70000800;
			apbmisc.end = 0x70000863;
			apbmisc.flags = IORESOURCE_MEM;

			/* strapping options */
			if (of_machine_is_compatible("nvidia,tegra124")) {
				straps.start = 0x7000e864;
				straps.end = 0x7000e867;
			} else {
				straps.start = 0x70000008;
				straps.end = 0x7000000b;
			}

			straps.flags = IORESOURCE_MEM;

			pr_warn("Using APBMISC region %pR\n", &apbmisc);
			pr_warn("Using strapping options registers %pR\n",
				&straps);
		} else {
			/*
			 * At this point we're not running on Tegra, so play
			 * nice with multi-platform kernels.
			 */
			return;
		}
	} else {
		/*
		 * Extract information from the device tree if we've found a
		 * matching node.
		 */
		if (of_address_to_resource(np, 0, &apbmisc) < 0) {
			pr_err("failed to get APBMISC registers\n");
			return;
		}

		if (of_address_to_resource(np, 1, &straps) < 0) {
			pr_err("failed to get strapping options registers\n");
			return;
		}
		apbmisc_data  = match->data;
	}

	apbmisc_base = ioremap(apbmisc.start, resource_size(&apbmisc));
	if (!apbmisc_base) {
		pr_err("failed to map APBMISC registers\n");
	} else {
		chipid = readl_relaxed(apbmisc_base + 4);
		if (!of_machine_is_compatible("nvidia,tegra194") &&
			!of_machine_is_compatible("nvidia,tegra234")) {
			iounmap(apbmisc_base);
		}
	}

	strapping_base = ioremap(straps.start, resource_size(&straps));
	if (!strapping_base) {
		pr_err("failed to map strapping options registers\n");
	} else {
		strapping = readl_relaxed(strapping_base);
		iounmap(strapping_base);
	}

	long_ram_code = of_property_read_bool(np, "nvidia,long-ram-code");
}

u32 tegra_read_emu_revid(void)
{
	return readl_relaxed(apbmisc_base + apbmisc_data->emu_revid_offset);
}
EXPORT_SYMBOL(tegra_read_emu_revid);

u32 tegra_get_sku_id(void)
{
	if (!tegra_sku_info.sku_id)
		tegra_fuse_readl(FUSE_SKU_INFO, &tegra_sku_info.sku_id);

	return tegra_sku_info.sku_id;
}
EXPORT_SYMBOL(tegra_get_sku_id);

enum tegra_revision tegra_chip_get_revision(void)
{
	return tegra_sku_info.revision;
}
EXPORT_SYMBOL(tegra_chip_get_revision);

bool is_t210b01_sku(void)
{
	if ((tegra_get_chip_id() == TEGRA210) &&
		(tegra_get_major_rev() == T210B01_MAJOR_REV))
		return true;
	return false;
}
EXPORT_SYMBOL(is_t210b01_sku);

/*
 * platform query functions begin
 */
static enum tegra_platform __tegra_get_platform(void)
{
	u32 major, pre_si_plat;

	major = tegra_get_major_rev();
	pre_si_plat = tegra_get_pre_si_plat();

	if (pre_si_plat == PRE_SI_VSP)
		return TEGRA_PLATFORM_VSP;

	if (!major) {
		u32 minor;

		minor = tegra_get_minor_rev();
		switch (minor) {
		case MINOR_QT:
			return TEGRA_PLATFORM_QT;
		case MINOR_FPGA:
			return TEGRA_PLATFORM_FPGA;
		case MINOR_ASIM_QT:
			return TEGRA_PLATFORM_QT;
		case MINOR_ASIM_LINSIM:
			return TEGRA_PLATFORM_LINSIM;
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
		case PRE_SI_VDK:
			return TEGRA_PLATFORM_VDK;
		case PRE_SI_VSP:
			return TEGRA_PLATFORM_VSP;
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
	u32 major, pre_si_plat;

	major = tegra_get_major_rev();
	pre_si_plat = tegra_get_pre_si_plat();

	if (!major) {
		u32 minor;

		minor = tegra_get_minor_rev();
		switch (minor) {
		case MINOR_QT:
		case MINOR_FPGA:
			return false;
		case MINOR_ASIM_QT:
		case MINOR_ASIM_LINSIM:
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
		case PRE_SI_VDK:
			return true;
		}
	}

	return false;
}
EXPORT_SYMBOL_GPL(tegra_cpu_is_asim);

void tegra_misc_sd_exp_mux_select(bool sd_exp_en)
{
	u32 value, reg;

	reg = readl_relaxed(apbmisc_base + TEGRA_APBMISC_SDMMC1_EXPRESS_MODE);
	value = sd_exp_en ? TEGRA_APBMISC_SDMMC1_EXPRESS_MODE_SDEXP
			: TEGRA_APBMISC_SDMMC1_EXPRESS_MODE_SDLEGACY;

	if (reg != value)
		writel_relaxed(value, apbmisc_base + TEGRA_APBMISC_SDMMC1_EXPRESS_MODE);
}
EXPORT_SYMBOL_GPL(tegra_misc_sd_exp_mux_select);
/*
 * platform query functions end
 */
