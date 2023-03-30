/*
 * Copyright (C) 2016-2022 NVIDIA CORPORATION. All rights reserved.
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

#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/io.h>
#include "dev.h"
#include "dev-t18x.h"

#define AST_CONTROL					0x000
#define AST_STREAMID_CTL_0				0x020
#define AST_STREAMID_CTL_1				0x024
#define AST_RGN_SLAVE_BASE_LO				0x100
#define AST_RGN_SLAVE_BASE_HI				0x104
#define AST_RGN_MASK_BASE_LO				0x108
#define AST_RGN_MASK_BASE_HI				0x10c
#define AST_RGN_MASTER_BASE_LO				0x110
#define AST_RGN_MASTER_BASE_HI				0x114
#define AST_RGN_CONTROL					0x118

#define AST_PAGE_MASK					(~0xFFF)
#define AST_LO_SHIFT					32
#define AST_LO_MASK					0xFFFFFFFF
#define AST_PHY_SID_IDX					0
#define AST_APE_SID_IDX					1
#define AST_NS						(1 << 3)
#define AST_CARVEOUTID(ID)				(ID << 5)
#define AST_VMINDEX(IDX)				(IDX << 15)
#define AST_PHSICAL(PHY)				(PHY << 19)
#define AST_STREAMID(ID)				(ID << 8)
#define AST_VMINDEX_ENABLE				(1 << 0)
#define AST_RGN_ENABLE					(1 << 0)
#define AST_RGN_OFFSET					0x20

struct acast_region {
	u32	rgn;
	u32	rgn_ctrl;
	u32	strmid_reg;
	u32	strmid_ctrl;
	u64	slave;
	u64	size;
	u64	master;
};

#define NUM_MAX_ACAST			2

#define ACAST_RGN_PHY			0x0
#define ACAST_RGN_CTL_PHY		(AST_PHSICAL(1) | AST_CARVEOUTID(0x7))

#define ACAST_RGN_VM			0x2
#define ACAST_VMINDEX			1
#define ACAST_RGN_CTL_VM(IDX)		AST_VMINDEX(IDX)
#define ACAST_SID_REG_EVAL(IDX)		AST_STREAMID_CTL_##IDX
#define ACAST_STRMID_REG(IDX)		ACAST_SID_REG_EVAL(IDX)

#if KERNEL_VERSION(4, 14, 0) > LINUX_VERSION_CODE
/* Older kernels do not have this function, so stubbing it */
static inline int of_property_read_u64_index(const struct device_node *np,
			const char *propname, u32 index, u64 *out_value)
{
	return -ENOSYS;
}
#endif

static inline void acast_write(void __iomem *acast, u32 reg, u32 val)
{
	writel(val, acast + reg);
}

static inline u32 acast_read(void __iomem *acast, u32 reg)
{
	return readl(acast + reg);
}

static inline u32 acast_rgn_reg(u32 rgn, u32 reg)
{
	return rgn * AST_RGN_OFFSET + reg;
}

static void tegra18x_acast_map(struct device *dev,
			       void __iomem *acast, u32 rgn, u32 rgn_ctrl,
			       u32 strmid_reg, u32 strmid_ctrl,
			       u64 slave, u64 size, u64 master)
{
	u32 val;

	val = acast_read(acast, acast_rgn_reg(rgn, AST_RGN_SLAVE_BASE_LO));
	if (val & AST_RGN_ENABLE) {
		dev_warn(dev, "ACAST rgn %u already mapped...skipping\n", rgn);
		return;
	}

	val = master & AST_LO_MASK;
	acast_write(acast,
		    acast_rgn_reg(rgn, AST_RGN_MASTER_BASE_LO), val);
	val = master >> AST_LO_SHIFT;
	acast_write(acast,
		    acast_rgn_reg(rgn, AST_RGN_MASTER_BASE_HI), val);

	val = ((size - 1) & AST_PAGE_MASK) & AST_LO_MASK;
	acast_write(acast,
		    acast_rgn_reg(rgn, AST_RGN_MASK_BASE_LO), val);
	val = (size - 1) >> AST_LO_SHIFT;
	acast_write(acast,
		    acast_rgn_reg(rgn, AST_RGN_MASK_BASE_HI), val);

	val = acast_read(acast, acast_rgn_reg(rgn, AST_RGN_CONTROL));
	val |= rgn_ctrl;
	acast_write(acast,
		    acast_rgn_reg(rgn, AST_RGN_CONTROL), val);

	if (strmid_reg)
		acast_write(acast, strmid_reg, strmid_ctrl);

	val = slave >> AST_LO_SHIFT;
	acast_write(acast,
		    acast_rgn_reg(rgn, AST_RGN_SLAVE_BASE_HI), val);
	val = (slave & AST_LO_MASK) | AST_RGN_ENABLE;
	acast_write(acast,
		    acast_rgn_reg(rgn, AST_RGN_SLAVE_BASE_LO), val);
}

static int tegra18x_acast_init(struct device *dev,
		uint32_t acast_addr, uint32_t acast_size,
		struct acast_region *acast_regions, uint32_t num_regions)
{
	void __iomem *acast_base;
	int i;

	acast_base = devm_ioremap(dev, acast_addr, acast_size);
	if (IS_ERR_OR_NULL(acast_base)) {
		dev_err(dev, "failed to map ACAST 0x%x\n", acast_addr);
		return PTR_ERR(acast_base);
	}

	for (i = 0; i < num_regions; i++) {
		tegra18x_acast_map(dev, acast_base,
				   acast_regions[i].rgn,
				   acast_regions[i].rgn_ctrl,
				   acast_regions[i].strmid_reg,
				   acast_regions[i].strmid_ctrl,
				   acast_regions[i].slave,
				   acast_regions[i].size,
				   acast_regions[i].master);

		dev_dbg(dev, "i:%d rgn:0x%x rgn_ctrl:0x%x ",
			i, acast_regions[i].rgn, acast_regions[i].rgn_ctrl);
		dev_dbg(dev, "strmid_reg:0x%x strmid_ctrl:0x%x ",
			acast_regions[i].strmid_reg,
			acast_regions[i].strmid_ctrl);
		dev_dbg(dev, "slave:0x%llx size:0x%llx master:0x%llx\n",
			acast_regions[i].slave, acast_regions[i].size,
			acast_regions[i].master);
	}

	return 0;
}

int nvadsp_acast_t18x_init(struct platform_device *pdev)
{
	struct nvadsp_drv_data *drv_data = platform_get_drvdata(pdev);
	struct device *dev = &pdev->dev;
	struct resource *co_mem = &drv_data->co_mem;
	uint32_t acast_addr, acast_size;
	int iter, num_acast = 0, ret = 0;
	struct acast_region acast_config;

	if (co_mem->start) {
		acast_config.rgn         = ACAST_RGN_PHY;
		acast_config.rgn_ctrl    = ACAST_RGN_CTL_PHY;
		acast_config.strmid_reg  = 0;
		acast_config.strmid_ctrl = 0;
		acast_config.slave   = drv_data->adsp_mem[ADSP_OS_ADDR];
		acast_config.size    = drv_data->adsp_mem[ADSP_OS_SIZE];
		acast_config.master  = co_mem->start;
	} else {
		uint32_t stream_id;
		uint64_t iommu_addr_start, iommu_addr_end;

		if (of_property_read_u32_index(dev->of_node,
				"iommus", 1, &stream_id)) {
			dev_warn(dev, "no SMMU stream ID found\n");
			goto exit;
		}
		if (of_property_read_u64_index(dev->of_node,
				"iommu-resv-regions", 1, &iommu_addr_start)) {
			dev_warn(dev, "no IOMMU reserved region\n");
			goto exit;
		}
		if (of_property_read_u64_index(dev->of_node,
				"iommu-resv-regions", 2, &iommu_addr_end)) {
			dev_warn(dev, "no IOMMU reserved region\n");
			goto exit;
		}

		acast_config.rgn         = ACAST_RGN_VM;
		acast_config.rgn_ctrl    = ACAST_RGN_CTL_VM(ACAST_VMINDEX);
		acast_config.strmid_reg  = ACAST_STRMID_REG(ACAST_VMINDEX);
		acast_config.strmid_ctrl = AST_STREAMID(stream_id) |
						AST_VMINDEX_ENABLE;
		acast_config.slave   = iommu_addr_start;
		acast_config.size    = (iommu_addr_end - acast_config.slave);
		acast_config.master  = iommu_addr_start;
	}

	for (iter = 0; iter < (NUM_MAX_ACAST * 2); iter += 2) {
		if (of_property_read_u32_index(dev->of_node,
			"nvidia,acast_config", iter, &acast_addr))
			continue;
		if (of_property_read_u32_index(dev->of_node,
			"nvidia,acast_config", (iter + 1), &acast_size))
			continue;

		ret = tegra18x_acast_init(dev, acast_addr, acast_size,
					&acast_config, 1);
		if (ret)
			goto exit;

		num_acast++;
	}

	if (num_acast == 0)
		dev_warn(dev, "no ACAST configurations found\n");

exit:
	return ret;
}
