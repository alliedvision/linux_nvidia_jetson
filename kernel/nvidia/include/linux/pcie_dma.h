/* SPDX-License-Identifier: GPL-2.0 */
/*
 * PCIe DMA test framework for Tegra PCIe
 *
 * Copyright (C) 2021 NVIDIA Corporation. All rights reserved.
 */

#ifndef PCIE_DMA_H
#define PCIE_DMA_H

/* Update DMA_DD_BUF_SIZE and DMA_LL_BUF_SIZE when changing BAR0_SIZE */
#define BAR0_SIZE		SZ_256M

/* Header includes RP/EP DMA addresses, EP MSI, LL, etc. */
#define BAR0_HEADER_OFFSET	0x0
#define BAR0_HEADER_SIZE	SZ_1M
#define  DMA_LL_OFFSET		SZ_4K
#define  DMA_LL_SIZE		SZ_4K /* 4K size of LL serve 170 desc */
#define  DMA_LL_WR_OFFSET(i)	(DMA_LL_OFFSET + ((i) * DMA_LL_SIZE))
#define  DMA_LL_RD_OFFSET(i)	(DMA_LL_WR_OFFSET(4) + ((i) * DMA_LL_SIZE))
#define  DMA_LL_MIN_SIZE	1
#define  DMA_LL_DEFAULT_SIZE	8
#define  DMA_ASYNC_LL_SIZE	160

#define  BAR0_MSI_OFFSET	SZ_64K

/* DMA'able memory range */
#define BAR0_DMA_BUF_OFFSET	SZ_1M
#define BAR0_DMA_BUF_SIZE	(BAR0_SIZE - SZ_1M)
#define  DMA_DD_BUF_SIZE	SZ_32M
#define  DMA_LL_BUF_SIZE	SZ_4M

/* Each DMA LL channel gets DMA_DD_BUF_SIZE and each desc DMA_LL_BUF_SIZE */
#define DMA_LL_WR_BUF(i)	(BAR0_DMA_BUF_OFFSET + (i) * DMA_DD_BUF_SIZE)
#define DMA_LL_RD_BUF(i)	(DMA_LL_WR_BUF(4) + (i) * DMA_DD_BUF_SIZE)

#define DEFAULT_STRESS_COUNT	10

#define MAX_DMA_ELE_SIZE	SZ_16M

/* DMA base offset starts at 0x20000 from ATU_DMA base */
#define DMA_OFFSET		0x20000

#define DMA_RD_CHNL_NUM		2
#define DMA_RD_CHNL_MASK	0x3
#define DMA_WR_CHNL_NUM		4
#define DMA_WR_CHNL_MASK	0xf

/* DMA common registers */
#define DMA_WRITE_ENGINE_EN_OFF		0xC
#define WRITE_ENABLE			BIT(0)
#define WRITE_DISABLE			0x0

#define DMA_WRITE_DOORBELL_OFF		0x10
#define DMA_WRITE_DOORBELL_OFF_WR_STOP	BIT(31)

#define DMA_READ_ENGINE_EN_OFF		0x2C
#define READ_ENABLE			BIT(0)
#define READ_DISABLE			0x0

#define DMA_READ_DOORBELL_OFF		0x30
#define DMA_READ_DOORBELL_OFF_RD_STOP	BIT(31)

#define DMA_WRITE_INT_STATUS_OFF	0x4C
#define DMA_WRITE_INT_MASK_OFF		0x54
#define DMA_WRITE_INT_CLEAR_OFF		0x58
#define DMA_WRITE_INT_DONE_MASK		0xF
#define DMA_WRITE_INT_ABORT_MASK	0xF0000

#define DMA_WRITE_ERR_STATUS_OFF	0x5C

#define DMA_WRITE_DONE_IMWR_LOW_OFF	0x60
#define DMA_WRITE_DONE_IMWR_HIGH_OFF	0x64
#define DMA_WRITE_ABORT_IMWR_LOW_OFF	0x68
#define DMA_WRITE_ABORT_IMWR_HIGH_OFF	0x6C

#define DMA_WRITE_IMWR_DATA_OFF_BASE	0x70

#define DMA_READ_INT_STATUS_OFF		0xA0
#define DMA_READ_INT_MASK_OFF		0xA8
#define DMA_READ_INT_CLEAR_OFF		0xAC
#define DMA_READ_INT_DONE_MASK		0xF
#define DMA_READ_INT_ABORT_MASK		0xF0000

#define DMA_READ_DONE_IMWR_LOW_OFF	0xCC
#define DMA_READ_DONE_IMWR_HIGH_OFF	0xD0
#define DMA_READ_ABORT_IMWR_LOW_OFF	0xD4
#define DMA_READ_ABORT_IMWR_HIGH_OFF	0xD8

#define DMA_READ_IMWR_DATA_OFF_BASE	0xDC

/* DMA channel specific registers */
#define DMA_CH_CONTROL1_OFF_WRCH	0x0
#define DMA_CH_CONTROL1_OFF_WRCH_LLE	BIT(9)
#define DMA_CH_CONTROL1_OFF_WRCH_CCS	BIT(8)
#define DMA_CH_CONTROL1_OFF_WRCH_RIE	BIT(4)
#define DMA_CH_CONTROL1_OFF_WRCH_LIE	BIT(3)
#define DMA_CH_CONTROL1_OFF_WRCH_LLP	BIT(2)
#define DMA_TRANSFER_SIZE_OFF_WRCH	0x8
#define DMA_SAR_LOW_OFF_WRCH		0xC
#define DMA_SAR_HIGH_OFF_WRCH		0x10
#define DMA_DAR_LOW_OFF_WRCH		0x14
#define DMA_DAR_HIGH_OFF_WRCH		0x18
#define DMA_LLP_LOW_OFF_WRCH		0x1C
#define DMA_LLP_HIGH_OFF_WRCH		0x20

#define DMA_CH_CONTROL1_OFF_RDCH	0x100
#define DMA_CH_CONTROL1_OFF_RDCH_LLE	BIT(9)
#define DMA_CH_CONTROL1_OFF_RDCH_CCS	BIT(8)
#define DMA_CH_CONTROL1_OFF_RDCH_RIE	BIT(4)
#define DMA_CH_CONTROL1_OFF_RDCH_LIE	BIT(3)
#define DMA_CH_CONTROL1_OFF_RDCH_LLP	BIT(2)
#define DMA_TRANSFER_SIZE_OFF_RDCH	0x108
#define DMA_SAR_LOW_OFF_RDCH		0x10C
#define DMA_SAR_HIGH_OFF_RDCH		0x110
#define DMA_DAR_LOW_OFF_RDCH		0x114
#define DMA_DAR_HIGH_OFF_RDCH		0x118
#define DMA_LLP_LOW_OFF_RDCH		0x11C
#define DMA_LLP_HIGH_OFF_RDCH		0x120

struct sanity_data {
	u32 size;
	u32 crc;
};

/* First 1MB of BAR0 is reserved for control data */
struct pcie_epf_bar0 {
	/* RP system memory allocated for EP DMA operations */
	u64 rp_phy_addr;
	/* EP system memory allocated as BAR */
	u64 ep_phy_addr;
	/* MSI data for RP -> EP interrupts */
	u32 msi_data[DMA_WR_CHNL_NUM + DMA_RD_CHNL_NUM];
	struct sanity_data wr_data[DMA_WR_CHNL_NUM];
	struct sanity_data rd_data[DMA_RD_CHNL_NUM];
};

struct dma_ll_ctrl {
	u32 cb:1;
	u32 tcb:1;
	u32 llp:1;
	u32 lie:1;
	u32 rie:1;
};

struct dma_ll {
	volatile struct dma_ll_ctrl ele;
	u32 size;
	u32 src_low;
	u32 src_high;
	u32 dst_low;
	u32 dst_high;
};

static inline void dma_common_wr16(void __iomem *p, u32 val, u32 offset)
{
	writew(val, offset + p);
}

static inline u16 dma_common_rd16(void __iomem *p, u32 offset)
{
	return readw(offset + p);
}

static inline void dma_common_wr(void __iomem *p, u32 val, u32 offset)
{
	writel(val, offset + p);
}

static inline u32 dma_common_rd(void __iomem *p, u32 offset)
{
	return readl(offset + p);
}

static inline void dma_channel_wr(void __iomem *p, u8 channel, u32 val,
				  u32 offset)
{
	writel(val, (0x200 * (channel + 1)) + offset + p);
}

static inline u32 dma_channel_rd(void __iomem *p, u8 channel, u32 offset)
{
	return readl((0x200 * (channel + 1)) + offset + p);
}

#endif
