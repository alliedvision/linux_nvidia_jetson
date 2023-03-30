/*
 * Tegra234 MC StreamID configuration
 *
 * Copyright (c) 2019-2021, NVIDIA CORPORATION.  All rights reserved.
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

#include <linux/platform/tegra/tegra-mc-sid.h>

enum override_id {
	HDAR,
	HOST1XDMAR,
	NVENCSRD,
	PCIE6AR,
	PCIE6AW,
	PCIE7AR,
	NVENCSWR,
	DLA0RDB,
	DLA0RDB1,
	DLA0WRB,
	DLA1RDB,
	PCIE7AW,
	PCIE8AR,
	PSCR,
	PSCW,
	HDAW,
	OFAR1,
	PCIE8AW,
	PCIE9AR,
	PCIE6AR1,
	PCIE9AW,
	PCIE10AR,
	PCIE10AW,
	ISPRA,
	ISPFALR,
	ISPWA,
	ISPWB,
	PCIE10AR1,
	PCIE7AR1,
	XUSB_HOSTR,
	XUSB_HOSTW,
	XUSB_DEVR,
	XUSB_DEVW,
	TSECSRD,
	TSECSWR,
	XSPI1W,
	MGBEARD,
	MGBEBRD,
	MGBECRD,
	MGBEDRD,
	MGBEAWR,
	OFAR,
	OFAW,
	MGBEBWR,
	SDMMCRA,
	MGBECWR,
	SDMMCRAB,
	SDMMCWA,
	MGBEDWR,
	SDMMCWAB,
	SEU1RD,
	SEU1WR,
	DCER,
	DCEW,
	VICSRD,
	VICSWR,
	DLA1RDB1,
	DLA1WRB,
	VI2W,
	VI2FALR,
	VIW,
	XSPI0R,
	XSPI0W,
	XSPI1R,
	NVDECSRD,
	NVDECSWR,
	APER,
	APEW,
	VI2FALW,
	NVJPGSRD,
	NVJPGSWR,
	SESRD,
	SESWR,
	AXIAPR,
	AXIAPW,
	ETRR,
	ETRW,
	DCEDMAR,
	DCEDMAW,
	AXISR,
	AXISW,
	EQOSR,
	EQOSW,
	UFSHCR,
	UFSHCW,
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
	VICSRD1,
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
	RCER,
	RCEW,
	RCEDMAR,
	RCEDMAW,
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
	PCIE5R1,
	NVENCSRD1,
	ISPRA1,
	PMA0AWR,
	NVJPG1SRD,
	NVJPG1SWR,
	MAX_OID,
};

static struct sid_override_reg sid_override_reg[] = {
	DEFREG(HDAR,        0xa8),
	DEFREG(HOST1XDMAR,  0xb0),
	DEFREG(NVENCSRD,    0xe0),
	DEFREG(PCIE6AR,     0x140),
	DEFREG(PCIE6AW,     0x148),
	DEFREG(PCIE7AR,     0x150),
	DEFREG(NVENCSWR,    0x158),
	DEFREG(DLA0RDB,     0x160),
	DEFREG(DLA0RDB1,    0x168),
	DEFREG(DLA0WRB,     0x170),
	DEFREG(DLA1RDB,     0x178),
	DEFREG(PCIE7AW,     0x180),
	DEFREG(PCIE8AR,     0x190),
	DEFREG(PSCR,        0x198),
	DEFREG(PSCW,        0x1a0),
	DEFREG(HDAW,        0x1a8),
	DEFREG(OFAR1,       0x1d0),
	DEFREG(PCIE8AW,     0x1d8),
	DEFREG(PCIE9AR,     0x1e0),
	DEFREG(PCIE6AR1,    0x1e8),
	DEFREG(PCIE9AW,     0x1f0),
	DEFREG(PCIE10AR,    0x1f8),
	DEFREG(PCIE10AW,    0x200),
	DEFREG(ISPRA,       0x220),
	DEFREG(ISPFALR,     0x228),
	DEFREG(ISPWA,       0x230),
	DEFREG(ISPWB,       0x238),
	DEFREG(PCIE10AR1,   0x240),
	DEFREG(PCIE7AR1,    0x248),
	DEFREG(XUSB_HOSTR,  0x250),
	DEFREG(XUSB_HOSTW,  0x258),
	DEFREG(XUSB_DEVR,   0x260),
	DEFREG(XUSB_DEVW,   0x268),
	DEFREG(TSECSRD,     0x2a0),
	DEFREG(TSECSWR,     0x2a8),
	DEFREG(XSPI1W,      0x2b0),
	DEFREG(MGBEARD,     0x2c0),
	DEFREG(MGBEBRD,     0x2c8),
	DEFREG(MGBECRD,     0x2d0),
	DEFREG(MGBEDRD,     0x2d8),
	DEFREG(MGBEAWR,     0x2e0),
	DEFREG(OFAR,        0x2e8),
	DEFREG(OFAW,        0x2f0),
	DEFREG(MGBEBWR,     0x2f8),
	DEFREG(SDMMCRA,     0x300),
	DEFREG(MGBECWR,     0x308),
	DEFREG(SDMMCRAB,    0x318),
	DEFREG(SDMMCWA,     0x320),
	DEFREG(MGBEDWR,     0x328),
	DEFREG(SDMMCWAB,    0x338),
	DEFREG(SEU1RD,      0x340),
	DEFREG(SEU1WR,      0x348),
	DEFREG(DCER,        0x350),
	DEFREG(DCEW,        0x358),
	DEFREG(VICSRD,      0x360),
	DEFREG(VICSWR,      0x368),
	DEFREG(DLA1RDB1,    0x370),
	DEFREG(DLA1WRB,     0x378),
	DEFREG(VI2W,        0x380),
	DEFREG(VI2FALR,     0x388),
	DEFREG(VIW,         0x390),
	DEFREG(XSPI0R,      0x3a8),
	DEFREG(XSPI0W,      0x3b0),
	DEFREG(XSPI1R,      0x3b8),
	DEFREG(NVDECSRD,    0x3c0),
	DEFREG(NVDECSWR,    0x3c8),
	DEFREG(APER,        0x3d0),
	DEFREG(APEW,        0x3d8),
	DEFREG(VI2FALW,     0x3e0),
	DEFREG(NVJPGSRD,    0x3f0),
	DEFREG(NVJPGSWR,    0x3f8),
	DEFREG(SESRD,       0x400),
	DEFREG(SESWR,       0x408),
	DEFREG(AXIAPR,      0x410),
	DEFREG(AXIAPW,      0x418),
	DEFREG(ETRR,        0x420),
	DEFREG(ETRW,        0x428),
	DEFREG(DCEDMAR,     0x440),
	DEFREG(DCEDMAW,     0x448),
	DEFREG(AXISR,       0x460),
	DEFREG(AXISW,       0x468),
	DEFREG(EQOSR,       0x470),
	DEFREG(EQOSW,       0x478),
	DEFREG(UFSHCR,      0x480),
	DEFREG(UFSHCW,      0x488),
	DEFREG(BPMPR,       0x498),
	DEFREG(BPMPW,       0x4a0),
	DEFREG(BPMPDMAR,    0x4a8),
	DEFREG(BPMPDMAW,    0x4b0),
	DEFREG(AONR,        0x4b8),
	DEFREG(AONW,        0x4c0),
	DEFREG(AONDMAR,     0x4c8),
	DEFREG(AONDMAW,     0x4d0),
	DEFREG(SCER,        0x4d8),
	DEFREG(SCEW,        0x4e0),
	DEFREG(SCEDMAR,     0x4e8),
	DEFREG(SCEDMAW,     0x4f0),
	DEFREG(APEDMAR,     0x4f8),
	DEFREG(APEDMAW,     0x500),
	DEFREG(VICSRD1,     0x510),
	DEFREG(VIFALR,      0x5e0),
	DEFREG(VIFALW,      0x5e8),
	DEFREG(DLA0RDA,     0x5f0),
	DEFREG(DLA0FALRDB,  0x5f8),
	DEFREG(DLA0WRA,     0x600),
	DEFREG(DLA0FALWRB,  0x608),
	DEFREG(DLA1RDA,     0x610),
	DEFREG(DLA1FALRDB,  0x618),
	DEFREG(DLA1WRA,     0x620),
	DEFREG(DLA1FALWRB,  0x628),
	DEFREG(PVA0RDA,     0x630),
	DEFREG(PVA0RDB,     0x638),
	DEFREG(PVA0RDC,     0x640),
	DEFREG(PVA0WRA,     0x648),
	DEFREG(PVA0WRB,     0x650),
	DEFREG(PVA0WRC,     0x658),
	DEFREG(RCER,        0x690),
	DEFREG(RCEW,        0x698),
	DEFREG(RCEDMAR,     0x6a0),
	DEFREG(RCEDMAW,     0x6a8),
	DEFREG(PCIE0R,      0x6c0),
	DEFREG(PCIE0W,      0x6c8),
	DEFREG(PCIE1R,      0x6d0),
	DEFREG(PCIE1W,      0x6d8),
	DEFREG(PCIE2AR,     0x6e0),
	DEFREG(PCIE2AW,     0x6e8),
	DEFREG(PCIE3R,      0x6f0),
	DEFREG(PCIE3W,      0x6f8),
	DEFREG(PCIE4R,      0x700),
	DEFREG(PCIE4W,      0x708),
	DEFREG(PCIE5R,      0x710),
	DEFREG(PCIE5W,      0x718),
	DEFREG(ISPFALW,     0x720),
	DEFREG(DLA0RDA1,    0x748),
	DEFREG(DLA1RDA1,    0x750),
	DEFREG(PVA0RDA1,    0x758),
	DEFREG(PVA0RDB1,    0x760),
	DEFREG(PCIE5R1,     0x778),
	DEFREG(NVENCSRD1,   0x780),
	DEFREG(ISPRA1,      0x790),
	DEFREG(PMA0AWR,        0x910),
	DEFREG(NVJPG1SRD,      0x918),
	DEFREG(NVJPG1SWR,      0x920),
};

static struct sid_to_oids sid_to_oids[] = {
};

static const struct tegra_mc_sid_soc_data tegra234_mc_soc_data = {
	.sid_override_reg = sid_override_reg,
	.nsid_override_reg = ARRAY_SIZE(sid_override_reg),
	.sid_to_oids = sid_to_oids,
	.nsid_to_oids = ARRAY_SIZE(sid_to_oids),
	.max_oids = MAX_OID,
};

static int tegra234_mc_sid_probe(struct platform_device *pdev)
{
	int err = 0;

	err = tegra_mc_sid_probe(pdev, &tegra234_mc_soc_data);
	if (err != 0)
		pr_err("tegra234 mc-sid probe failed\n");
	else
		pr_info("tegra234 mc-sid probe successful\n");
	return err;
}

static const struct of_device_id tegra234_mc_sid_of_match[] = {
	{ .compatible = "nvidia,tegra234-mc-sid", },
	{},
};
MODULE_DEVICE_TABLE(of, tegra234_mc_sid_of_match);

static struct platform_driver tegra234_mc_sid_driver = {
	.probe	= tegra234_mc_sid_probe,
	.remove = tegra_mc_sid_remove,
	.driver	= {
		.owner	= THIS_MODULE,
		.name	= "tegra234-mc-sid",
		.of_match_table	= of_match_ptr(tegra234_mc_sid_of_match),
	},
};

static int __init tegra234_mc_sid_init(void)
{
	struct device_node *np;
	struct platform_device *pdev = NULL;

	np = of_find_compatible_node(NULL, NULL, "nvidia,tegra234-mc-sid");
	if (np) {
		pdev = of_platform_device_create(np, NULL,
						 platform_bus_type.dev_root);
		of_node_put(np);
	}

	if (IS_ERR_OR_NULL(pdev))
		return -ENODEV;

	return platform_driver_register(&tegra234_mc_sid_driver);
}
arch_initcall(tegra234_mc_sid_init);

MODULE_DESCRIPTION("MC StreamID configuration");
MODULE_AUTHOR("Pritesh Raithatha <praithatha@nvidia.com>");
MODULE_LICENSE("GPL v2");
