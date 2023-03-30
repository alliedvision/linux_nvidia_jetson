/*
 * Copyright (c) 2018-2022 NVIDIA Corporation.  All rights reserved.
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

#include <linux/module.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_iommu.h>
#include <linux/of_platform.h>
#include <linux/of_reserved_mem.h>
#include <linux/syscore_ops.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/slab.h>
#include "arm/arm-smmu/arm-smmu.h"
#include "arm-smmu-suspend-regs.h"

#define SMMU_GNSR1_CBAR_CFG(n, smmu_pgshift) \
		((1U << smmu_pgshift) + ARM_SMMU_GR1_CBAR(n))

#define SMMU_GNSR1_CBA2R_CFG(n, smmu_pgshift) \
		((1U << smmu_pgshift) + ARM_SMMU_GR1_CBA2R(n))

#define SMMU_CB_CFG(name, n, smmu_size, smmu_pgshift) \
		((smmu_size >> 1) + (n * (1 << smmu_pgshift)) \
			+ ARM_SMMU_CB_ ## name)

#define SMMU_REG_TABLE_START_REG	0xCAFE05C7U

#define SMMU_REG_TABLE_END_REG		0xFFFFFFFFU
#define SMMU_REG_TABLE_END_VALUE	0xFFFFFFFFU

#define MAX_SMMUS 5

static uint32_t cb_group_max;
static uint32_t smrg_group_max;

struct arm_smmu_reg {
	u32 reg;
	u32 val;
};

struct arm_smmu_context {
	struct arm_smmu_reg *reg_list;
	phys_addr_t reg_list_pa;
	size_t reg_list_table_size;
	size_t reg_list_mem_size;

	void __iomem **smmu_base;
	u32 *smmu_base_pa;
	unsigned long smmu_size;
	unsigned long smmu_pgshift;
	size_t num_smmus;

	void __iomem *scratch_va;
};

static struct arm_smmu_context arm_smmu_ctx;

static phys_addr_t arm_smmu_alloc_reg_list(void)
{
	unsigned int order = get_order(arm_smmu_ctx.reg_list_mem_size);
	struct page *pages = alloc_pages(GFP_KERNEL, order);

	if (!pages)
		return 0;

	return page_to_phys(pages);
}

static void arm_smmu_free_reg_list(void)
{
	unsigned int order = get_order(arm_smmu_ctx.reg_list_mem_size);
	struct page *pages = phys_to_page(arm_smmu_ctx.reg_list_pa);

	__free_pages(pages, order);
}

static int reg_table_index;

static void reg_table_set(u32 reg, u32 val)
{
	arm_smmu_ctx.reg_list[reg_table_index].reg = reg;
	arm_smmu_ctx.reg_list[reg_table_index++].val = val;
}

static void context_save_reg(u32 reg)
{
	u32 reg_paddr, i;
	void __iomem *reg_addr;

	for (i = 0; i < arm_smmu_ctx.num_smmus; i++) {
		reg_addr = arm_smmu_ctx.smmu_base[i] + reg;
		reg_paddr = arm_smmu_ctx.smmu_base_pa[i] + reg;
		reg_table_set(reg_paddr, readl_relaxed(reg_addr));
	}
}

#define SMMU_REG_TABLE_START_SIZE	1
static void context_save_start(void)
{
	reg_table_index = 0;
	reg_table_set(SMMU_REG_TABLE_START_REG,
				arm_smmu_ctx.reg_list_table_size - 1);
}

#define SMMU_REG_TABLE_END_SIZE		1
static void context_save_end(void)
{
	reg_table_set(SMMU_REG_TABLE_END_REG, SMMU_REG_TABLE_END_REG);
}

#define GNSR_GROUP_REG_SIZE		3
static void context_save_gnsr0_group(void)
{
	context_save_reg(ARM_SMMU_GR0_sCR0);
	context_save_reg(ARM_SMMU_GR0_sCR2);
	context_save_reg(ARM_SMMU_GR0_sACR);
}

#define SMRG_GROUP_REG_SIZE		2
static void context_save_smrg_group(int group_num)
{
	context_save_reg(ARM_SMMU_GR0_SMR(group_num));
	context_save_reg(ARM_SMMU_GR0_S2CR(group_num));
}

#define CBAR_GROUP_REG_SIZE		2
static void context_save_cbar_group(int group_num)
{
	context_save_reg(SMMU_GNSR1_CBAR_CFG(group_num,
			arm_smmu_ctx.smmu_pgshift));
	context_save_reg(SMMU_GNSR1_CBA2R_CFG(group_num,
			arm_smmu_ctx.smmu_pgshift));
}

#define CB_GROUP_REG_SIZE		9
static void context_save_cb_group(int group_num)
{
	context_save_reg(SMMU_CB_CFG(SCTLR, group_num,
			arm_smmu_ctx.smmu_size, arm_smmu_ctx.smmu_pgshift));
	context_save_reg(SMMU_CB_CFG(TTBCR2, group_num,
			arm_smmu_ctx.smmu_size, arm_smmu_ctx.smmu_pgshift));
	context_save_reg(SMMU_CB_CFG(TTBR0_LO, group_num,
			arm_smmu_ctx.smmu_size, arm_smmu_ctx.smmu_pgshift));
	context_save_reg(SMMU_CB_CFG(TTBR0_HI, group_num,
			arm_smmu_ctx.smmu_size, arm_smmu_ctx.smmu_pgshift));
	context_save_reg(SMMU_CB_CFG(TTBR1_LO, group_num,
			arm_smmu_ctx.smmu_size, arm_smmu_ctx.smmu_pgshift));
	context_save_reg(SMMU_CB_CFG(TTBR1_HI, group_num,
			arm_smmu_ctx.smmu_size, arm_smmu_ctx.smmu_pgshift));
	context_save_reg(SMMU_CB_CFG(TTBCR, group_num,
			arm_smmu_ctx.smmu_size, arm_smmu_ctx.smmu_pgshift));
	context_save_reg(SMMU_CB_CFG(CONTEXTIDR, group_num,
			arm_smmu_ctx.smmu_size, arm_smmu_ctx.smmu_pgshift));
	context_save_reg(SMMU_CB_CFG(S1_MAIR0, group_num,
			arm_smmu_ctx.smmu_size, arm_smmu_ctx.smmu_pgshift));
}

static int arm_smmu_syscore_suspend(void)
{
	int i;

	context_save_start();

	context_save_gnsr0_group();

	for (i = 0; i < smrg_group_max; i++)
		context_save_smrg_group(i);

	for (i = 0; i < cb_group_max; i++)
		context_save_cbar_group(i);

	for (i = 0; i < cb_group_max; i++)
		context_save_cb_group(i);

	context_save_end();

	return 0;
}

static struct syscore_ops arm_smmu_syscore_ops = {
	.suspend = arm_smmu_syscore_suspend,
};

static int arm_smmu_suspend_init(void __iomem **smmu_base, u32 *smmu_base_pa,
				int num_smmus, unsigned long smmu_size,
				unsigned long smmu_pgshift, u32 scratch_reg_pa)
{
	int ret = 0, i;

	arm_smmu_ctx.reg_list_table_size =
		(SMMU_REG_TABLE_START_SIZE + SMMU_REG_TABLE_END_SIZE
			+ (GNSR_GROUP_REG_SIZE
				+ (CB_GROUP_REG_SIZE * cb_group_max)
				+ (SMRG_GROUP_REG_SIZE * smrg_group_max)
				+ (CBAR_GROUP_REG_SIZE * cb_group_max)
			  ) * num_smmus);

	arm_smmu_ctx.reg_list_mem_size =
			PAGE_ALIGN(arm_smmu_ctx.reg_list_table_size *
			sizeof(struct arm_smmu_reg));

	arm_smmu_ctx.reg_list_pa = arm_smmu_alloc_reg_list();
	if (!arm_smmu_ctx.reg_list_pa) {
		pr_err("Failed to alloc smmu_context memory\n");
		return -ENOMEM;
	}

	arm_smmu_ctx.reg_list = memremap(arm_smmu_ctx.reg_list_pa,
			arm_smmu_ctx.reg_list_mem_size, MEMREMAP_WB);
	if (!arm_smmu_ctx.reg_list) {
		pr_err("Failed to memremap smmu_context\n");
		goto free_reg_list;
	}

	arm_smmu_ctx.scratch_va = ioremap(scratch_reg_pa, 4);
	if (IS_ERR(arm_smmu_ctx.scratch_va)) {
		pr_err("Failed to ioremap scratch register\n");
		ret = PTR_ERR(arm_smmu_ctx.scratch_va);
		goto unmap_reg_list;
	}

	writel(arm_smmu_ctx.reg_list_pa >> 12, arm_smmu_ctx.scratch_va);

	arm_smmu_ctx.smmu_base_pa =
			kcalloc(num_smmus, sizeof(u32), GFP_KERNEL);
	if (arm_smmu_ctx.smmu_base_pa == NULL) {
		pr_err("Failed to allocate memory for base_pa\n");
		goto unmap_reg_list;
	}

	arm_smmu_ctx.smmu_base = (void __iomem **)
				kcalloc(num_smmus, sizeof(void *), GFP_KERNEL);
	if (arm_smmu_ctx.smmu_base == NULL) {
		pr_err("Failed to allocate memory for base\n");
		goto unmap_reg_list;
	}
	for (i = 0; i < num_smmus; i++) {
		arm_smmu_ctx.smmu_base[i] = smmu_base[i];
		arm_smmu_ctx.smmu_base_pa[i] = smmu_base_pa[i];
	}
	arm_smmu_ctx.smmu_size = smmu_size;
	arm_smmu_ctx.smmu_pgshift = smmu_pgshift;
	arm_smmu_ctx.num_smmus = num_smmus;

	register_syscore_ops(&arm_smmu_syscore_ops);

	return 0;

unmap_reg_list:
	memunmap(arm_smmu_ctx.reg_list);

free_reg_list:
	arm_smmu_free_reg_list();

	return ret;
}

static void arm_smmu_suspend_exit(void)
{
	if (arm_smmu_ctx.reg_list)
		memunmap(arm_smmu_ctx.reg_list);

	if (arm_smmu_ctx.reg_list_pa)
		arm_smmu_free_reg_list();

	kfree(arm_smmu_ctx.smmu_base);
	kfree(arm_smmu_ctx.smmu_base_pa);
}

static const struct of_device_id arm_smmu_of_match[] = {
	{ .compatible = "nvidia,smmu_suspend"},
	{ },
};

static int arm_smmu_suspend_probe(struct platform_device *pdev)
{
	struct resource *res;
	void __iomem *bases[MAX_SMMUS];
	struct device *dev = &pdev->dev;
	u32 base_pa[MAX_SMMUS];
	u32 suspend_save_reg, i, num_smmus, id;
	unsigned long size, pgshift;
	int err;

	for (i = 0; i < MAX_SMMUS; i++) {
		res = platform_get_resource(pdev, IORESOURCE_MEM, i);
		if (!res)
			break;

		bases[i] = devm_ioremap(dev, res->start, resource_size(res));
		if (IS_ERR(bases[i]))
			break;

		base_pa[i] = res->start;
		if (i == 0) {
			size = resource_size(res);
		}
	}
	num_smmus = i;

	if (num_smmus == 0) {
		dev_err(dev, "No SMMU device found\n");
		return -ENODEV;
	}

	id = readl_relaxed(bases[0] + ARM_SMMU_GR0_ID1);
	pgshift = (id & ARM_SMMU_ID1_PAGESIZE) ? 16 : 12;
	cb_group_max = FIELD_GET(ARM_SMMU_ID1_NUMCB, id);

	id = readl_relaxed(bases[0] + ARM_SMMU_GR0_ID0);
	smrg_group_max = FIELD_GET(ARM_SMMU_ID0_NUMSMRG, id);

	if (!of_property_read_u32(dev->of_node, "suspend-save-reg",
			&suspend_save_reg)) {

		err = arm_smmu_suspend_init(bases, base_pa, num_smmus, size,
					    pgshift, suspend_save_reg);
		if (err) {
			dev_err(dev, "failed to init arm_smu_suspend\n");
			return err;
		}
	}

	dev_info(dev, "arm_smmu_suspend probe successful\n");

	return 0;
}

static int arm_smmu_suspend_remove(struct platform_device *pdev)
{
	arm_smmu_suspend_exit();

	return 0;
}

static struct platform_driver arm_smmu_suspend_driver = {
	.driver	= {
		.name		= "arm-smmu-suspend",
		.of_match_table	= arm_smmu_of_match,
	},
	.probe	= arm_smmu_suspend_probe,
	.remove	= arm_smmu_suspend_remove,
};

static int __init arm_smmu_suspend_driver_init(void)
{
	platform_driver_register(&arm_smmu_suspend_driver);
	return 0;
}

static void __exit arm_smmu_suspend_driver_exit(void)
{
	platform_driver_unregister(&arm_smmu_suspend_driver);
}

module_init(arm_smmu_suspend_driver_init);
module_exit(arm_smmu_suspend_driver_exit);

MODULE_DESCRIPTION("arm-smmu-suspend: Driver for saving arm-smmu registers during suspend");
MODULE_AUTHOR("Pritesh Raithatha <praithatha@nvidia.com>");
MODULE_AUTHOR("Ashish Mhetre <amhetre@nvidia.com>");
MODULE_LICENSE("GPL v2");
