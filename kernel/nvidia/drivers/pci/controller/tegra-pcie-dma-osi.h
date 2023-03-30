/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * PCIe EDMA Framework
 *
 * Copyright (C) 2021-2022 NVIDIA Corporation. All rights reserved.
 */

#ifndef TEGRA_PCIE_DMA_OSI_H
#define TEGRA_PCIE_DMA_OSI_H

#define OSI_BIT(b)		(1U << (b))
/** generates bit mask for 32 bit value */
#define OSI_GENMASK(h, l)	(((~0U) << (l)) & (~0U >> (31U - (h))))

/* Channel specific registers */
#define DMA_CH_CONTROL1_OFF_WRCH		0x0
#define DMA_CH_CONTROL1_OFF_WRCH_LLE		OSI_BIT(9)
#define DMA_CH_CONTROL1_OFF_WRCH_CCS		OSI_BIT(8)
#define DMA_CH_CONTROL1_OFF_WRCH_CS_MASK	OSI_GENMASK(6U, 5U)
#define DMA_CH_CONTROL1_OFF_WRCH_CS_SHIFT	5
#define DMA_CH_CONTROL1_OFF_WRCH_RIE		OSI_BIT(4)
#define DMA_CH_CONTROL1_OFF_WRCH_LIE		OSI_BIT(3)
#define DMA_CH_CONTROL1_OFF_WRCH_LLP		OSI_BIT(2)
#define DMA_CH_CONTROL1_OFF_WRCH_CB		OSI_BIT(0)

#define DMA_WRITE_ENGINE_EN_OFF		0xC
#define WRITE_ENABLE			OSI_BIT(0)
#define WRITE_DISABLE			0x0

#define DMA_WRITE_DOORBELL_OFF		0x10
#define DMA_WRITE_DOORBELL_OFF_WR_STOP	OSI_BIT(31)

#define DMA_READ_ENGINE_EN_OFF		0x2C
#define READ_ENABLE			OSI_BIT(0)
#define READ_DISABLE			0x0

#define DMA_READ_DOORBELL_OFF		0x30
#define DMA_READ_DOORBELL_OFF_RD_STOP	OSI_BIT(31)

#define DMA_TRANSFER_SIZE_OFF_WRCH		0x8
#define DMA_SAR_LOW_OFF_WRCH			0xC
#define DMA_SAR_HIGH_OFF_WRCH			0x10
#define DMA_DAR_LOW_OFF_WRCH			0x14
#define DMA_DAR_HIGH_OFF_WRCH			0x18
#define DMA_LLP_LOW_OFF_WRCH			0x1C
#define DMA_LLP_HIGH_OFF_WRCH			0x20

#define DMA_WRITE_DONE_IMWR_LOW_OFF		0x60
#define DMA_WRITE_DONE_IMWR_HIGH_OFF		0x64
#define DMA_WRITE_ABORT_IMWR_LOW_OFF		0x68
#define DMA_WRITE_ABORT_IMWR_HIGH_OFF		0x6c
#define DMA_WRITE_CH01_IMWR_DATA_OFF		0x70
#define DMA_WRITE_CH23_IMWR_DATA_OFF		0x74

#define DMA_WRITE_LINKED_LIST_ERR_EN_OFF	0x90
#define DMA_READ_LINKED_LIST_ERR_EN_OFF		0xC4

#define DMA_READ_DONE_IMWR_LOW_OFF		0xcc
#define DMA_READ_DONE_IMWR_HIGH_OFF		0xd0
#define DMA_READ_ABORT_IMWR_LOW_OFF		0xd4
#define DMA_READ_ABORT_IMWR_HIGH_OFF		0xd8
#define DMA_READ_CH01_IMWR_DATA_OFF		0xdc

#define DMA_CH_CONTROL1_OFF_RDCH		0x100
#define DMA_CH_CONTROL1_OFF_RDCH_LLE		OSI_BIT(9)
#define DMA_CH_CONTROL1_OFF_RDCH_CCS		OSI_BIT(8)
#define DMA_CH_CONTROL1_OFF_RDCH_CS_MASK	OSI_GENMASK(6U, 5U)
#define DMA_CH_CONTROL1_OFF_RDCH_CS_SHIFT	5
#define DMA_CH_CONTROL1_OFF_RDCH_RIE		OSI_BIT(4)
#define DMA_CH_CONTROL1_OFF_RDCH_LIE		OSI_BIT(3)
#define DMA_CH_CONTROL1_OFF_RDCH_LLP		OSI_BIT(2)
#define DMA_CH_CONTROL1_OFF_RDCH_CB		OSI_BIT(0)

#define DMA_TRANSFER_SIZE_OFF_RDCH		0x108
#define DMA_SAR_LOW_OFF_RDCH			0x10c
#define DMA_SAR_HIGH_OFF_RDCH			0x110
#define DMA_DAR_LOW_OFF_RDCH			0x114
#define DMA_DAR_HIGH_OFF_RDCH			0x118
#define DMA_LLP_LOW_OFF_RDCH			0x11c
#define DMA_LLP_HIGH_OFF_RDCH			0x120

#define DMA_WRITE_INT_STATUS_OFF	0x4C
#define DMA_WRITE_INT_MASK_OFF		0x54
#define DMA_WRITE_INT_CLEAR_OFF		0x58

#define DMA_READ_INT_STATUS_OFF		0xA0
#define DMA_READ_INT_MASK_OFF		0xA8
#define DMA_READ_INT_CLEAR_OFF		0xAC

struct edma_ctrl {
	uint32_t cb:1;
	uint32_t tcb:1;
	uint32_t llp:1;
	uint32_t lie:1;
	uint32_t rie:1;
};

struct edma_hw_desc {
	volatile union {
		struct edma_ctrl ctrl_e;
		uint32_t ctrl_d;
	} ctrl_reg;
	uint32_t size;
	uint32_t sar_low;
	uint32_t sar_high;
	uint32_t dar_low;
	uint32_t dar_high;
};

struct edma_hw_desc_llp {
	volatile union {
		struct edma_ctrl ctrl_e;
		uint32_t ctrl_d;
	} ctrl_reg;
	uint32_t size;
	uint32_t sar_low;
	uint32_t sar_high;
};

struct edma_dblock {
	struct edma_hw_desc desc[2];
	struct edma_hw_desc_llp llp;
};

static inline unsigned int dma_common_rd(void __iomem *p, unsigned int offset)
{
	return readl(p + offset);
}

static inline void dma_common_wr(void __iomem *p, unsigned int val, unsigned int offset)
{
	writel(val, p + offset);
}

static inline void dma_channel_wr(void __iomem *p, unsigned char c, unsigned int val,
				  u32 offset)
{
	writel(val, (0x200 * (c + 1)) + p + offset);
}

static inline unsigned int dma_channel_rd(void __iomem *p, unsigned char c, u32 offset)
{
	return readl((0x200 * (c + 1)) + p + offset);
}

#endif // TEGRA_PCIE_DMA_OSI_H
