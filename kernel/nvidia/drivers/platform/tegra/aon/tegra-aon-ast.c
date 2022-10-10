/*
 * Copyright (c) 2020, NVIDIA CORPORATION. All rights reserved.
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

#include <linux/device.h>
#include <linux/firmware.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/sizes.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/cache.h>
#include <linux/io.h>

#include <asm/cacheflush.h>

#include <aon.h>

#define TEGRA_APS_AST_CONTROL			0
#define TEGRA_APS_AST_CONTROL_DEF_PHY		BIT(19)
#define TEGRA_APS_AST_STREAMID_CTL_0		0x20
#define TEGRA_APS_AST_STREAMID_CTL_1		0x24
#define TEGRA_APS_AST_REGION_0_SLAVE_BASE_LO	0x100
#define TEGRA_APS_AST_REGION_0_SLAVE_BASE_HI	0x104
#define TEGRA_APS_AST_REGION_0_MASK_LO		0x108
#define TEGRA_APS_AST_REGION_0_MASK_HI		0x10c
#define TEGRA_APS_AST_REGION_0_MASTER_BASE_LO	0x110
#define TEGRA_APS_AST_REGION_0_MASTER_BASE_HI	0x114
#define TEGRA_APS_AST_REGION_0_CONTROL		0x118
#define TEGRA_APS_AST_REGION_1_MASK_LO		0x128

#define AST_MAX_REGION				7
#define AST_ADDR_MASK				0xfffff000
#define AST_VM_IDX_MASK				0x00078000
#define AST_VM_IDX_BIT_SHFT			15
#define AST_CARVEOUT_ID_SHIFT			5
#define AST_STREAMID_SHIFT			8

/* TEGRA_APS_AST_CONTROL register fields */
#define AST_MATCH_ERR_CTRL		0x2

/* TEGRA_APS_AST_REGION_<x>_CONTROL register fieds */
#define AST_RGN_CTRL_PHYSICAL		BIT(19)
#define AST_RGN_CTRL_SNOOP		0x4

/* TEGRA_APS_AST_REGION_<x>_SLAVE_BASE_LO register fields */
#define AST_SLV_BASE_LO_ENABLE		1

/* TEGRA_APS_AST_STREAMID_CONTROL_<x> register fields */
#define AST_STREAMID_CTL_ENABLE		1
#define AST_STREAMID_CTL_STREAMID	(SPE_STREAMID << AST_STREAMID_SHIFT)
#define AST_STREAMID_CTL_PHYS_STREAMID	(PHYS_STREAMID << AST_STREAMID_SHIFT)

/* TEGRA_APS_AST_CONTROL_0 register fields */
#define AST_CONTROL_STREAMID_SHIFT	22
#define AST_CONTROL_PHYS_STREAMID	\
				(PHYS_STREAMID << AST_CONTROL_STREAMID_SHIFT)

#define AST_CONTROL_CarveOutLock	BIT(20)
#define AST_CONTROL_Lock		1

struct tegra_ast {
	struct tegra_aon *aon;
	void __iomem *ast_base;
};

static inline u32 tegra_ast_region_offset(u32 region)
{
	const u32 region_stride = TEGRA_APS_AST_REGION_1_MASK_LO -
					TEGRA_APS_AST_REGION_0_MASK_LO;

	return region * region_stride;
}

static int tegra_ast_region_enable(struct tegra_ast *ast, u32 region,
				   u32 slave_base, u64 size, u64 master_base)
{
	struct tegra_aon *aon = ast->aon;
	struct aon_platform_data *pdata = NULL;
	u32 roffset;
	u32 ast_reg_val;
	u64 mask;
	void __iomem *ast_base;
	void __iomem *ast_reg;
	int ret = 0;

	mask = (size - 1ULL);
	pdata = dev_get_drvdata(aon->dev);
	if ((size & mask) != 0U) {
		dev_err(aon->dev, "Size is not a power of 2\n");
		ret = -EINVAL;
		goto exit;
	}

	if ((master_base & mask) != 0U) {
		dev_err(aon->dev, "Output addr is not aligned to size\n");
		ret = -EINVAL;
		goto exit;
	}

	if ((slave_base & mask) != 0U) {
		dev_err(aon->dev, "Input addr is not aligned to size\n");
		ret = -EINVAL;
		goto exit;
	}

	/* Fetch the region offset */
	roffset = tegra_ast_region_offset(region);
	ast_base = ast->ast_base;

	/* Disable region(enable bit is 0) before programming it */
	ast_reg = ast_base + TEGRA_APS_AST_REGION_0_SLAVE_BASE_LO + roffset;
	writel(slave_base, ast_reg);

	/* Program mask */
	ast_reg = ast_base + TEGRA_APS_AST_REGION_0_MASK_LO + roffset;
	writel(mask & AST_ADDR_MASK, ast_reg);

	/* Program lower 32-bits of master-address */
	ast_reg = ast_base + TEGRA_APS_AST_REGION_0_MASTER_BASE_LO + roffset;
	writel(master_base & AST_ADDR_MASK, ast_reg);

	/* Program upper 32-bits of master-address */
	ast_reg = ast_base + TEGRA_APS_AST_REGION_0_MASTER_BASE_HI + roffset;
	writel((u32)(master_base >> 32), ast_reg);

	/* Program region control register (carveout id, vm index = 0)*/
	ast_reg = ast_base + TEGRA_APS_AST_REGION_0_CONTROL + roffset;
	ast_reg_val = (pdata->fw_carveout_id << AST_CARVEOUT_ID_SHIFT) | BIT(2);
	writel(ast_reg_val, ast_reg);

	/* Program slave-address */
	ast_reg = ast_base + TEGRA_APS_AST_REGION_0_SLAVE_BASE_LO + roffset;
	writel((slave_base & AST_ADDR_MASK), ast_reg);

	/* Program streamid control 0 register as we are using VM index 0 */
	ast_reg_val = 0x1U | (pdata->aon_stream_id  << AST_STREAMID_SHIFT);
	ast_reg = ast_base + TEGRA_APS_AST_STREAMID_CTL_0;
	writel(ast_reg_val, ast_reg);

	/* enable the address translation */
	ast_reg = ast_base + TEGRA_APS_AST_REGION_0_SLAVE_BASE_LO + roffset;
	ast_reg_val = readl(ast_reg);
	ast_reg_val |= AST_SLV_BASE_LO_ENABLE;
	writel(ast_reg_val, ast_reg);

	/* Program global control register DefPhysical = 0 */
	ast_reg = ast_base + TEGRA_APS_AST_CONTROL;
	ast_reg_val = AST_CONTROL_Lock |
			(pdata->phys_stream_id << AST_CONTROL_STREAMID_SHIFT);
	ast_reg_val &= ~TEGRA_APS_AST_CONTROL_DEF_PHY;
	writel(ast_reg_val, ast_reg);

exit:
	return ret;
}

int tegra_aon_ast_config(struct tegra_aon *aon)
{
	struct aon_platform_data *pdata = NULL;
	struct tegra_ast asts[2];
	int i;
	int ret = 0;

	if (!aon) {
		ret = -EINVAL;
		goto exit;
	}

	if (!aon->fw) {
		ret = -EINVAL;
		goto exit;
	}

	asts[0].aon = aon;
	asts[1].aon = aon;
	asts[0].ast_base = aon_reg(aon, ast_ast0_base_r());
	asts[1].ast_base = aon_reg(aon, ast_ast1_base_r());

	pdata = pdata_from_aon(aon);
	for (i = 0; i < ARRAY_SIZE(asts); i++) {
		ret = tegra_ast_region_enable(&asts[i], 0,
					      pdata->fw_carveout_va,
					      pdata->fw_carveout_size,
					      aon->fw->dma_handle);
		if (ret) {
			dev_err(aon->dev,
				"AST %d configuration failed :%d\r\n",
				i, ret);
			break;
		}
	}

exit:
	return ret;
}
