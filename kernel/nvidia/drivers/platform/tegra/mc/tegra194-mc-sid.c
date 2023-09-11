/*
 * Tegra194 MC StreamID configuration
 *
 * Copyright (c) 2016-2023, NVIDIA CORPORATION.  All rights reserved.
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

#define pr_fmt(fmt)	"%s(): " fmt, __func__

#include <linux/err.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/version.h>

#include <linux/platform/tegra/tegra-mc-sid.h>
#include <dt-bindings/memory/tegra-swgroup.h>
#include <dt-bindings/memory/tegra194-swgroup.h>

#if KERNEL_VERSION(5, 10, 0) <= LINUX_VERSION_CODE
#include <dt-bindings/memory/tegra194-mc.h>
#endif

enum override_id {
	PTCR,
	HDAR,
	HOST1XDMAR,
	NVENCSRD,
	SATAR,
	MPCORER,
	NVENCSWR,
	HDAW,
	MPCOREW,
	SATAW,
	ISPRA,
	ISPFALR,
	ISPWA,
	ISPWB,
	XUSB_HOSTR,
	XUSB_HOSTW,
	XUSB_DEVR,
	XUSB_DEVW,
	TSECSRD,
	TSECSWR,
	SDMMCRA,
	SDMMCR,
	SDMMCRAB,
	SDMMCWA,
	SDMMCW,
	SDMMCWAB,
	VICSRD,
	VICSWR,
	VIW,
	NVDECSRD,
	NVDECSWR,
	APER,
	APEW,
	NVJPGSRD,
	NVJPGSWR,
	SESRD,
	SESWR,
	AXIAPR,
	AXIAPW,
	ETRR,
	ETRW,
	TSECSRDB,
	TSECSWRB,
	AXISR,
	AXISW,
	EQOSR,
	EQOSW,
	UFSHCR,
	UFSHCW,
	NVDISPLAYR,
	BPMPR,
	BPMPW,
	BPMPDMAR,
	BPMPDMAW,
	AONR,
	AONW,
	AONDMAR,
	AONDMAW,
	SCER,
	SCEW,
	SCEDMAR,
	SCEDMAW,
	APEDMAR,
	APEDMAW,
	NVDISPLAYR1,
	VICSRD1,
	NVDECSRD1,
	MIU0R,
	MIU0W,
	MIU1R,
	MIU1W,
	MIU2R,
	MIU2W,
	MIU3R,
	MIU3W,
	VIFALR,
	VIFALW,
	DLA0RDA,
	DLA0FALRDB,
	DLA0WRA,
	DLA0FALWRB,
	DLA1RDA,
	DLA1FALRDB,
	DLA1WRA,
	DLA1FALWRB,
	PVA0RDA,
	PVA0RDB,
	PVA0RDC,
	PVA0WRA,
	PVA0WRB,
	PVA0WRC,
	PVA1RDA,
	PVA1RDB,
	PVA1RDC,
	PVA1WRA,
	PVA1WRB,
	PVA1WRC,
	RCER,
	RCEW,
	RCEDMAR,
	RCEDMAW,
	NVENC1SRD,
	NVENC1SWR,
	PCIE0R,
	PCIE0W,
	PCIE1R,
	PCIE1W,
	PCIE2AR,
	PCIE2AW,
	PCIE3R,
	PCIE3W,
	PCIE4R,
	PCIE4W,
	PCIE5R,
	PCIE5W,
	ISPFALW,
	DLA0RDA1,
	DLA1RDA1,
	PVA0RDA1,
	PVA0RDB1,
	PVA1RDA1,
	PVA1RDB1,
	PCIE5R1,
	NVENCSRD1,
	NVENC1SRD1,
	ISPRA1,
	PCIE0R1,
	NVDEC1SRD,
	NVDEC1SRD1,
	NVDEC1SWR,
	MAX_OID,
};

static struct sid_override_reg sid_override_reg[] = {
	DEFREG(PTCR,		0x000),
	DEFREG(HDAR,		0x0A8),
	DEFREG(HOST1XDMAR,	0x0B0),
	DEFREG(NVENCSRD,	0x0E0),
	DEFREG(SATAR,		0x0F8),
	DEFREG(MPCORER,		0x138),
	DEFREG(NVENCSWR,	0x158),
	DEFREG(HDAW,		0x1A8),
	DEFREG(MPCOREW,		0x1C8),
	DEFREG(SATAW,		0x1E8),
	DEFREG(ISPRA,		0x220),
	DEFREG(ISPFALR,		0x228),
	DEFREG(ISPWA,		0x230),
	DEFREG(ISPWB,		0x238),
	DEFREG(XUSB_HOSTR,	0x250),
	DEFREG(XUSB_HOSTW,	0x258),
	DEFREG(XUSB_DEVR,	0x260),
	DEFREG(XUSB_DEVW,	0x268),
	DEFREG(TSECSRD,		0x2A0),
	DEFREG(TSECSWR,		0x2A8),
	DEFREG(SDMMCRA,		0x300),
	DEFREG(SDMMCR,		0x310),
	DEFREG(SDMMCRAB,	0x318),
	DEFREG(SDMMCWA,		0x320),
	DEFREG(SDMMCW,		0x330),
	DEFREG(SDMMCWAB,	0x338),
	DEFREG(VICSRD,		0x360),
	DEFREG(VICSWR,		0x368),
	DEFREG(VIW,		0x390),
	DEFREG(NVDECSRD,	0x3C0),
	DEFREG(NVDECSWR,	0x3C8),
	DEFREG(APER,		0x3D0),
	DEFREG(APEW,		0x3D8),
	DEFREG(NVJPGSRD,	0x3F0),
	DEFREG(NVJPGSWR,	0x3F8),
	DEFREG(SESRD,		0x400),
	DEFREG(SESWR,		0x408),
	DEFREG(AXIAPR,		0x410),
	DEFREG(AXIAPW,		0x418),
	DEFREG(ETRR,		0x420),
	DEFREG(ETRW,		0x428),
	DEFREG(TSECSRDB,	0x430),
	DEFREG(TSECSWRB,	0x438),
	DEFREG(AXISR,		0x460),
	DEFREG(AXISW,		0x468),
	DEFREG(EQOSR,		0x470),
	DEFREG(EQOSW,		0x478),
	DEFREG(UFSHCR,		0x480),
	DEFREG(UFSHCW,		0x488),
	DEFREG(NVDISPLAYR,	0x490),
	DEFREG(BPMPR,		0x498),
	DEFREG(BPMPW,		0x4A0),
	DEFREG(BPMPDMAR,	0x4A8),
	DEFREG(BPMPDMAW,	0x4B0),
	DEFREG(AONR,		0x4B8),
	DEFREG(AONW,		0x4C0),
	DEFREG(AONDMAR,		0x4C8),
	DEFREG(AONDMAW,		0x4D0),
	DEFREG(SCER,		0x4D8),
	DEFREG(SCEW,		0x4E0),
	DEFREG(SCEDMAR,		0x4E8),
	DEFREG(SCEDMAW,		0x4F0),
	DEFREG(APEDMAR,		0x4F8),
	DEFREG(APEDMAW,		0x500),
	DEFREG(NVDISPLAYR1,	0x508),
	DEFREG(VICSRD1,		0x510),
	DEFREG(NVDECSRD1,	0x518),
	DEFREG(MIU0R,		0x530),
	DEFREG(MIU0W,		0x538),
	DEFREG(MIU1R,		0x540),
	DEFREG(MIU1W,		0x548),
	DEFREG(MIU2R,		0x570),
	DEFREG(MIU2W,		0x578),
	DEFREG(MIU3R,		0x580),
	DEFREG(MIU3W,		0x588),
	DEFREG(VIFALR,		0x5E0),
	DEFREG(VIFALW,		0x5E8),
	DEFREG(DLA0RDA,		0x5F0),
	DEFREG(DLA0FALRDB,	0x5F8),
	DEFREG(DLA0WRA,		0x600),
	DEFREG(DLA0FALWRB,	0x608),
	DEFREG(DLA1RDA,		0x610),
	DEFREG(DLA1FALRDB,	0x618),
	DEFREG(DLA1WRA,		0x620),
	DEFREG(DLA1FALWRB,	0x628),
	DEFREG(PVA0RDA,		0x630),
	DEFREG(PVA0RDB,		0x638),
	DEFREG(PVA0RDC,		0x640),
	DEFREG(PVA0WRA,		0x648),
	DEFREG(PVA0WRB,		0x650),
	DEFREG(PVA0WRC,		0x658),
	DEFREG(PVA1RDA,		0x660),
	DEFREG(PVA1RDB,		0x668),
	DEFREG(PVA1RDC,		0x670),
	DEFREG(PVA1WRA,		0x678),
	DEFREG(PVA1WRB,		0x680),
	DEFREG(PVA1WRC,		0x688),
	DEFREG(RCER,		0x690),
	DEFREG(RCEW,		0x698),
	DEFREG(RCEDMAR,		0x6A0),
	DEFREG(RCEDMAW,		0x6A8),
	DEFREG(NVENC1SRD,	0x6B0),
	DEFREG(NVENC1SWR,	0x6B8),
	DEFREG(PCIE0R,		0x6C0),
	DEFREG(PCIE0W,		0x6C8),
	DEFREG(PCIE1R,		0x6D0),
	DEFREG(PCIE1W,		0x6D8),
	DEFREG(PCIE2AR,		0x6E0),
	DEFREG(PCIE2AW,		0x6E8),
	DEFREG(PCIE3R,		0x6F0),
	DEFREG(PCIE3W,		0x6F8),
	DEFREG(PCIE4R,		0x700),
	DEFREG(PCIE4W,		0x708),
	DEFREG(PCIE5R,		0x710),
	DEFREG(PCIE5W,		0x718),
	DEFREG(ISPFALW,		0x720),
	DEFREG(DLA0RDA1,	0x748),
	DEFREG(DLA1RDA1,	0x750),
	DEFREG(PVA0RDA1,	0x758),
	DEFREG(PVA0RDB1,	0x760),
	DEFREG(PVA1RDA1,	0x768),
	DEFREG(PVA1RDB1,	0x770),
	DEFREG(PCIE5R1,		0x778),
	DEFREG(NVENCSRD1,	0x780),
	DEFREG(NVENC1SRD1,	0x788),
	DEFREG(ISPRA1,		0x790),
	DEFREG(PCIE0R1,		0x798),
	DEFREG(NVDEC1SRD,	0x7C8),
	DEFREG(NVDEC1SRD1,	0x7D0),
	DEFREG(NVDEC1SWR,	0x7D8),
};

static struct sid_to_oids sid_to_oids[] = {
	{
#if KERNEL_VERSION(5, 10, 0) <= LINUX_VERSION_CODE
		.client_id = TEGRA194_MEMORY_CLIENT_NVDISPLAYR,
#endif
		.sid	= TEGRA_SID_NVDISPLAY,
		.noids	= 1,
		.oid	= {
			NVDISPLAYR,
		},
		.ord = OVERRIDE,
		.name = "NVDISPLAYR",
	},
	{
#if KERNEL_VERSION(5, 10, 0) <= LINUX_VERSION_CODE
		.client_id = TEGRA194_MEMORY_CLIENT_NVDISPLAYR1,
#endif
		.sid	= TEGRA_SID_NVDISPLAY,
		.noids	= 1,
		.oid	= {
			NVDISPLAYR1,
		},
		.ord = OVERRIDE,
		.name = "NVDISPLAYR1",
	},
};

static const struct tegra_mc_sid_soc_data tegra194_mc_soc_data = {
	.sid_override_reg = sid_override_reg,
	.nsid_override_reg = ARRAY_SIZE(sid_override_reg),
	.sid_to_oids = sid_to_oids,
	.nsid_to_oids = ARRAY_SIZE(sid_to_oids),
	.max_oids = MAX_OID,
};

static int tegra194_mc_sid_probe(struct platform_device *pdev)
{
	return tegra_mc_sid_probe(pdev, &tegra194_mc_soc_data);
}

static const struct of_device_id tegra194_mc_sid_of_match[] = {
	{ .compatible = "nvidia,tegra194-mc-sid", },
	{},
};
MODULE_DEVICE_TABLE(of, tegra194_mc_sid_of_match);

static const struct dev_pm_ops tegra194_mc_sid_pm_ops = {
	.resume_early = tegra_mc_sid_resume_early,
};

static struct platform_driver tegra194_mc_sid_driver = {
	.probe	= tegra194_mc_sid_probe,
	.remove = tegra_mc_sid_remove,
	.driver	= {
		.owner	= THIS_MODULE,
		.name	= "tegra194-mc-sid",
		.pm     = &tegra194_mc_sid_pm_ops,
		.of_match_table	= of_match_ptr(tegra194_mc_sid_of_match),
	},
};

static int __init tegra194_mc_sid_init(void)
{
	struct device_node *np;
	struct platform_device *pdev = NULL;

	np = of_find_compatible_node(NULL, NULL, "nvidia,tegra194-mc-sid");
	if (np) {
		pdev = of_platform_device_create(np, NULL,
						 platform_bus_type.dev_root);
		of_node_put(np);
	}

	if (IS_ERR_OR_NULL(pdev))
		return -ENODEV;

	return platform_driver_register(&tegra194_mc_sid_driver);
}
arch_initcall(tegra194_mc_sid_init);

MODULE_DESCRIPTION("MC StreamID configuration");
MODULE_AUTHOR("Hiroshi DOYU <hdoyu@nvidia.com>, Pritesh Raithatha <praithatha@nvidia.com>");
MODULE_LICENSE("GPL v2");
