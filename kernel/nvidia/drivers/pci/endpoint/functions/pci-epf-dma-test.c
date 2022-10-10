// SPDX-License-Identifier: GPL-2.0+
/*
 * PCIe DMA EPF test framework for Tegra PCIe
 *
 * Copyright (C) 2021-2022 NVIDIA Corporation. All rights reserved.
 */

#include <linux/crc32.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/msi.h>
#include <linux/of_platform.h>
#include <linux/pci-epc.h>
#include <linux/pci-epf.h>
#include <linux/pcie_dma.h>
#include <linux/platform_device.h>
#include <linux/kthread.h>
#include <linux/tegra-pcie-edma-test-common.h>

static struct pcie_epf_dma *gepfnv;

struct pcie_epf_dma {
	struct pci_epf_header header;
	struct device *fdev;
	struct device *cdev;
	void *bar0_virt;
	struct dentry *debugfs;
	void __iomem *dma_base;
	int irq;

	u32 dma_size;
	u32 stress_count;
	u32 async_count;

	struct task_struct *wr0_task;
	struct task_struct *wr1_task;
	struct task_struct *wr2_task;
	struct task_struct *wr3_task;
	struct task_struct *rd0_task;
	struct task_struct *rd1_task;
	u8 task_done;
	wait_queue_head_t task_wq;
	void *cookie;

	wait_queue_head_t wr_wq[DMA_WR_CHNL_NUM];
	wait_queue_head_t rd_wq[DMA_RD_CHNL_NUM];
	unsigned long wr_busy;
	unsigned long rd_busy;
	ktime_t wr_start_time[DMA_WR_CHNL_NUM];
	ktime_t wr_end_time[DMA_WR_CHNL_NUM];
	ktime_t rd_start_time[DMA_RD_CHNL_NUM];
	ktime_t rd_end_time[DMA_RD_CHNL_NUM];
	u32 wr_cnt[DMA_WR_CHNL_NUM + DMA_RD_CHNL_NUM];
	u32 rd_cnt[DMA_WR_CHNL_NUM + DMA_RD_CHNL_NUM];
	bool pcs[DMA_WR_CHNL_NUM + DMA_RD_CHNL_NUM];
	bool async_dma;
	ktime_t edma_start_time[DMA_WR_CHNL_NUM];
	u64 tsz;
	u32 edma_ch;
	u32 prev_edma_ch;
	u32 nents;
	struct tegra_pcie_edma_desc *ll_desc;
	struct edmalib_common edma;
};

struct edma_desc {
	dma_addr_t src;
	dma_addr_t dst;
	size_t sz;
};

static irqreturn_t pcie_dma_epf_wr0_msi(int irq, void *arg)
{
	struct pcie_epf_dma *epfnv = (struct pcie_epf_dma *)arg;
	int bit = 0;

	epfnv->wr_busy &= ~BIT(bit);
	wake_up(&epfnv->wr_wq[bit]);

	return IRQ_HANDLED;
}

static irqreturn_t pcie_dma_epf_wr1_msi(int irq, void *arg)
{
	struct pcie_epf_dma *epfnv = (struct pcie_epf_dma *)arg;
	int bit = 1;

	epfnv->wr_busy &= ~BIT(bit);
	wake_up(&epfnv->wr_wq[bit]);

	return IRQ_HANDLED;
}

static irqreturn_t pcie_dma_epf_wr2_msi(int irq, void *arg)
{
	struct pcie_epf_dma *epfnv = (struct pcie_epf_dma *)arg;
	int bit = 2;

	epfnv->wr_busy &= ~BIT(bit);
	wake_up(&epfnv->wr_wq[bit]);

	return IRQ_HANDLED;
}

static irqreturn_t pcie_dma_epf_wr3_msi(int irq, void *arg)
{
	struct pcie_epf_dma *epfnv = (struct pcie_epf_dma *)arg;
	int bit = 3;

	epfnv->wr_busy &= ~BIT(bit);
	wake_up(&epfnv->wr_wq[bit]);

	return IRQ_HANDLED;
}

static irqreturn_t pcie_dma_epf_rd0_msi(int irq, void *arg)
{
	struct pcie_epf_dma *epfnv = (struct pcie_epf_dma *)arg;
	int bit = 0;

	epfnv->rd_busy &= ~BIT(bit);
	wake_up(&epfnv->rd_wq[bit]);

	return IRQ_HANDLED;
}

static irqreturn_t pcie_dma_epf_rd1_msi(int irq, void *arg)
{
	struct pcie_epf_dma *epfnv = (struct pcie_epf_dma *)arg;
	int bit = 1;

	epfnv->rd_busy &= ~BIT(bit);
	wake_up(&epfnv->rd_wq[bit]);

	return IRQ_HANDLED;
}

void pcie_async_dma_handler(struct pcie_epf_dma *epfnv)
{
	struct pcie_epf_bar0 *epf_bar0 = (struct pcie_epf_bar0 *)
					 epfnv->bar0_virt;
	u64 llp_base, llp_iova;
	u32 llp_idx, ridx;
	int i;

	for (i = 0; i < DMA_WR_CHNL_NUM; i++) {
		llp_iova = dma_channel_rd(epfnv->dma_base, i,
					  DMA_LLP_HIGH_OFF_WRCH);
		llp_iova = (llp_iova << 32);
		llp_iova |= dma_channel_rd(epfnv->dma_base, i,
					   DMA_LLP_LOW_OFF_WRCH);
		llp_base = epf_bar0->ep_phy_addr + DMA_LL_WR_OFFSET(i);
		llp_idx = ((llp_iova - llp_base) / sizeof(struct dma_ll));
		llp_idx = llp_idx % DMA_ASYNC_LL_SIZE;

		if (!llp_idx)
			continue;

		ridx = epfnv->rd_cnt[i] % DMA_ASYNC_LL_SIZE;

		while (llp_idx != ridx) {
			epfnv->rd_cnt[i]++;
			ridx = epfnv->rd_cnt[i] % DMA_ASYNC_LL_SIZE;
		}
	}

	for (i = 0; i < DMA_RD_CHNL_NUM; i++) {
		llp_iova = dma_channel_rd(epfnv->dma_base, i,
					  DMA_LLP_HIGH_OFF_RDCH);
		llp_iova = (llp_iova << 32);
		llp_iova |= dma_channel_rd(epfnv->dma_base, i,
					   DMA_LLP_LOW_OFF_RDCH);
		llp_base = epf_bar0->ep_phy_addr + DMA_LL_RD_OFFSET(i);
		llp_idx = ((llp_iova - llp_base) / sizeof(struct dma_ll));
		llp_idx = llp_idx % DMA_ASYNC_LL_SIZE;

		if (!llp_idx)
			continue;

		ridx = epfnv->rd_cnt[DMA_WR_CHNL_NUM + i] % DMA_ASYNC_LL_SIZE;

		while (llp_idx != ridx) {
			epfnv->rd_cnt[DMA_WR_CHNL_NUM + i]++;
			ridx = epfnv->rd_cnt[DMA_WR_CHNL_NUM + i] %
				DMA_ASYNC_LL_SIZE;
		}
	}
}

static irqreturn_t pcie_dma_epf_irq(int irq, void *arg)
{
	return IRQ_WAKE_THREAD;
}

static irqreturn_t pcie_dma_epf_irq_handler(int irq, void *arg)
{
	struct pcie_epf_dma *epfnv = (struct pcie_epf_dma *)arg;
	int bit = 0;
	u32 val;

	val = dma_common_rd(epfnv->dma_base, DMA_WRITE_INT_STATUS_OFF);
	for_each_set_bit(bit, &epfnv->wr_busy, DMA_WR_CHNL_NUM) {
		if (BIT(bit) & val) {
			dma_common_wr(epfnv->dma_base, BIT(bit),
				      DMA_WRITE_INT_CLEAR_OFF);
			epfnv->wr_end_time[bit] = ktime_get();
			epfnv->wr_busy &= ~(BIT(bit));
			wake_up(&epfnv->wr_wq[bit]);
		}
	}

	val = dma_common_rd(epfnv->dma_base, DMA_READ_INT_STATUS_OFF);
	for_each_set_bit(bit, &epfnv->rd_busy, DMA_RD_CHNL_NUM) {
		if (BIT(bit) & val) {
			dma_common_wr(epfnv->dma_base, BIT(bit),
				      DMA_READ_INT_CLEAR_OFF);
			epfnv->rd_end_time[bit] = ktime_get();
			epfnv->rd_busy &= ~(BIT(bit));
			wake_up(&epfnv->rd_wq[bit]);
		}
	}

	if (epfnv->async_dma) {
		val = dma_common_rd(epfnv->dma_base, DMA_WRITE_INT_STATUS_OFF);
		dma_common_wr(epfnv->dma_base, val, DMA_WRITE_INT_CLEAR_OFF);
		val = dma_common_rd(epfnv->dma_base, DMA_READ_INT_STATUS_OFF);
		dma_common_wr(epfnv->dma_base, val, DMA_READ_INT_CLEAR_OFF);
		pcie_async_dma_handler(epfnv);
	}

	return IRQ_HANDLED;
}

static int edma_init(struct pcie_epf_dma *epfnv, bool lie)
{
	u32 val;
	int i;

	/* Enable LIE or RIE for all write channels */
	if (lie) {
		val = dma_common_rd(epfnv->dma_base, DMA_WRITE_INT_MASK_OFF);
		val &= ~0xf;
		val &= ~(0xf << 16);
		dma_common_wr(epfnv->dma_base, val, DMA_WRITE_INT_MASK_OFF);
	} else {
		val = dma_common_rd(epfnv->dma_base, DMA_WRITE_INT_MASK_OFF);
		val |= 0xf;
		val |= (0xf << 16);
		dma_common_wr(epfnv->dma_base, val, DMA_WRITE_INT_MASK_OFF);
	}

	val = DMA_CH_CONTROL1_OFF_WRCH_LIE;
	if (!lie)
		val |= DMA_CH_CONTROL1_OFF_WRCH_RIE;
	for (i = 0; i < DMA_WR_CHNL_NUM; i++)
		dma_channel_wr(epfnv->dma_base, i, val,
			       DMA_CH_CONTROL1_OFF_WRCH);

	/* Enable LIE or RIE for all read channels */
	if (lie) {
		val = dma_common_rd(epfnv->dma_base, DMA_READ_INT_MASK_OFF);
		val &= ~0x3;
		val &= ~(0x3 << 16);
		dma_common_wr(epfnv->dma_base, val, DMA_READ_INT_MASK_OFF);
	} else {
		val = dma_common_rd(epfnv->dma_base, DMA_READ_INT_MASK_OFF);
		val |= 0x3;
		val |= (0x3 << 16);
		dma_common_wr(epfnv->dma_base, val, DMA_READ_INT_MASK_OFF);
	}

	val = DMA_CH_CONTROL1_OFF_RDCH_LIE;
	if (!lie)
		val |= DMA_CH_CONTROL1_OFF_RDCH_RIE;
	for (i = 0; i < DMA_RD_CHNL_NUM; i++)
		dma_channel_wr(epfnv->dma_base, i, val,
			       DMA_CH_CONTROL1_OFF_RDCH);

	dma_common_wr(epfnv->dma_base, WRITE_ENABLE, DMA_WRITE_ENGINE_EN_OFF);
	dma_common_wr(epfnv->dma_base, READ_ENABLE, DMA_READ_ENGINE_EN_OFF);

	return 0;
}

static void edma_deinit(struct pcie_epf_dma *epfnv)
{
	u32 val;

	/* Mask channel interrupts */
	val = dma_common_rd(epfnv->dma_base, DMA_WRITE_INT_MASK_OFF);
	val |= 0xf;
	val |= (0xf << 16);
	dma_common_wr(epfnv->dma_base, val, DMA_WRITE_INT_MASK_OFF);

	val = dma_common_rd(epfnv->dma_base, DMA_READ_INT_MASK_OFF);
	val |= 0x3;
	val |= (0x3 << 16);
	dma_common_wr(epfnv->dma_base, val, DMA_READ_INT_MASK_OFF);

	dma_common_wr(epfnv->dma_base, WRITE_DISABLE, DMA_WRITE_ENGINE_EN_OFF);
	dma_common_wr(epfnv->dma_base, READ_DISABLE, DMA_READ_ENGINE_EN_OFF);
}

static int edma_ll_init(struct pcie_epf_dma *epfnv)
{
	u32 val;
	int i;

	/* Enable linked list mode and set CCS */
	val = DMA_CH_CONTROL1_OFF_WRCH_LLE | DMA_CH_CONTROL1_OFF_WRCH_CCS;
	for (i = 0; i < DMA_WR_CHNL_NUM; i++)
		dma_channel_wr(epfnv->dma_base, i, val,
			       DMA_CH_CONTROL1_OFF_WRCH);

	val = DMA_CH_CONTROL1_OFF_RDCH_LLE | DMA_CH_CONTROL1_OFF_WRCH_CCS;
	for (i = 0; i < DMA_RD_CHNL_NUM; i++)
		dma_channel_wr(epfnv->dma_base, i, val,
			       DMA_CH_CONTROL1_OFF_RDCH);

	return 0;
}

static void edma_ll_deinit(struct pcie_epf_dma *epfnv)
{
	u32 val;
	int i;

	/* Disable linked list mode and clear CCS */
	for (i = 0; i < DMA_WR_CHNL_NUM; i++) {
		val = dma_channel_rd(epfnv->dma_base, i,
				     DMA_CH_CONTROL1_OFF_WRCH);
		val &= ~(DMA_CH_CONTROL1_OFF_WRCH_LLE |
			DMA_CH_CONTROL1_OFF_WRCH_CCS);
		dma_channel_wr(epfnv->dma_base, i, val,
			       DMA_CH_CONTROL1_OFF_WRCH);
	}

	for (i = 0; i < DMA_RD_CHNL_NUM; i++) {
		val = dma_channel_rd(epfnv->dma_base, i,
				     DMA_CH_CONTROL1_OFF_RDCH);
		val &= ~(DMA_CH_CONTROL1_OFF_RDCH_LLE |
			DMA_CH_CONTROL1_OFF_RDCH_CCS);
		dma_channel_wr(epfnv->dma_base, i, val,
			       DMA_CH_CONTROL1_OFF_RDCH);
	}
}

static int edma_submit_direct_tx(struct pcie_epf_dma *epfnv,
				 struct edma_desc *desc, int ch)
{
	int ret = 0;

	epfnv->wr_busy |= 1 << ch;

	/* Populate desc in DMA registers */
	dma_channel_wr(epfnv->dma_base, ch, desc->sz,
		       DMA_TRANSFER_SIZE_OFF_WRCH);
	dma_channel_wr(epfnv->dma_base, ch, lower_32_bits(desc->src),
		       DMA_SAR_LOW_OFF_WRCH);
	dma_channel_wr(epfnv->dma_base, ch, upper_32_bits(desc->src),
		       DMA_SAR_HIGH_OFF_WRCH);
	dma_channel_wr(epfnv->dma_base, ch, lower_32_bits(desc->dst),
		       DMA_DAR_LOW_OFF_WRCH);
	dma_channel_wr(epfnv->dma_base, ch, upper_32_bits(desc->dst),
		       DMA_DAR_HIGH_OFF_WRCH);

	epfnv->wr_start_time[ch] = ktime_get();
	dma_common_wr(epfnv->dma_base, ch, DMA_WRITE_DOORBELL_OFF);

	/* Wait 5 sec to get DMA done interrupt */
	ret = wait_event_timeout(epfnv->wr_wq[ch],
				 !(epfnv->wr_busy & (1 << ch)),
				 msecs_to_jiffies(5000));
	if (ret == 0) {
		dev_err(epfnv->fdev, "%s: DD WR CH: %d TO\n", __func__, ch);
		ret = -ETIMEDOUT;
	}

	return ret;
}

static int edma_submit_direct_rx(struct pcie_epf_dma *epfnv,
				 struct edma_desc *desc, int ch)
{
	int ret = 0;

	epfnv->rd_busy |= 1 << ch;

	/* Populate desc in DMA registers */
	dma_channel_wr(epfnv->dma_base, ch, desc->sz,
		       DMA_TRANSFER_SIZE_OFF_RDCH);
	dma_channel_wr(epfnv->dma_base, ch, lower_32_bits(desc->src),
		       DMA_SAR_LOW_OFF_RDCH);
	dma_channel_wr(epfnv->dma_base, ch, upper_32_bits(desc->src),
		       DMA_SAR_HIGH_OFF_RDCH);
	dma_channel_wr(epfnv->dma_base, ch, lower_32_bits(desc->dst),
		       DMA_DAR_LOW_OFF_RDCH);
	dma_channel_wr(epfnv->dma_base, ch, upper_32_bits(desc->dst),
		       DMA_DAR_HIGH_OFF_RDCH);

	epfnv->rd_start_time[ch] = ktime_get();
	dma_common_wr(epfnv->dma_base, ch, DMA_READ_DOORBELL_OFF);

	ret = wait_event_timeout(epfnv->rd_wq[ch],
				 !(epfnv->rd_busy & (1 << ch)),
				 msecs_to_jiffies(5000));
	if (ret == 0) {
		dev_err(epfnv->fdev, "%s: DD RD CH: %d TO\n",
			__func__, ch);
		ret = -ETIMEDOUT;
	}

	return ret;
}

static int edma_submit_direct_txrx(struct pcie_epf_dma *epfnv,
				   struct edma_desc *desc_wr,
				   struct edma_desc *desc_rd)
{
	int ret = 0, i;

	/* Configure all DMA write and read channels */
	epfnv->wr_busy = DMA_WR_CHNL_MASK;
	epfnv->rd_busy = DMA_RD_CHNL_MASK;

	for (i = 0; i < DMA_WR_CHNL_NUM; i++) {
		dma_channel_wr(epfnv->dma_base, i, desc_wr[i].sz,
			       DMA_TRANSFER_SIZE_OFF_WRCH);
		dma_channel_wr(epfnv->dma_base, i,
			       lower_32_bits(desc_wr[i].src),
			       DMA_SAR_LOW_OFF_WRCH);
		dma_channel_wr(epfnv->dma_base, i,
			       upper_32_bits(desc_wr[i].src),
			       DMA_SAR_HIGH_OFF_WRCH);
		dma_channel_wr(epfnv->dma_base, i,
			       lower_32_bits(desc_wr[i].dst),
			       DMA_DAR_LOW_OFF_WRCH);
		dma_channel_wr(epfnv->dma_base, i,
			       upper_32_bits(desc_wr[i].dst),
			       DMA_DAR_HIGH_OFF_WRCH);
	}

	for (i = 0; i < DMA_RD_CHNL_NUM; i++) {
		dma_channel_wr(epfnv->dma_base, i, desc_rd[i].sz,
			       DMA_TRANSFER_SIZE_OFF_RDCH);
		dma_channel_wr(epfnv->dma_base, i,
			       lower_32_bits(desc_rd[i].src),
			       DMA_SAR_LOW_OFF_RDCH);
		dma_channel_wr(epfnv->dma_base, i,
			       upper_32_bits(desc_rd[i].src),
			       DMA_SAR_HIGH_OFF_RDCH);
		dma_channel_wr(epfnv->dma_base, i,
			       lower_32_bits(desc_rd[i].dst),
			       DMA_DAR_LOW_OFF_RDCH);
		dma_channel_wr(epfnv->dma_base, i,
			       upper_32_bits(desc_rd[i].dst),
			       DMA_DAR_HIGH_OFF_RDCH);
	}

	for (i = 0; i < DMA_WR_CHNL_NUM; i++) {
		dma_common_wr(epfnv->dma_base, i, DMA_WRITE_DOORBELL_OFF);
		if (i < DMA_RD_CHNL_NUM)
			dma_common_wr(epfnv->dma_base, i,
				      DMA_READ_DOORBELL_OFF);
	}

	for (i = 0; i < DMA_WR_CHNL_NUM; i++) {
		ret = wait_event_timeout(epfnv->wr_wq[i],
					 !(epfnv->wr_busy & (1 << i)),
					 msecs_to_jiffies(5000));
		if (ret == 0) {
			dev_err(epfnv->fdev, "%s: DD WR CH: %d TO\n",
				__func__, i);
			ret = -ETIMEDOUT;
			goto fail;
		}
	}

	for (i = 0; i < DMA_RD_CHNL_NUM; i++) {
		ret = wait_event_timeout(epfnv->rd_wq[i],
					 !(epfnv->rd_busy & (1 << i)),
					 msecs_to_jiffies(5000));
		if (ret == 0) {
			dev_err(epfnv->fdev, "%s: DD RD CH: %d TO\n",
				__func__, i);
			ret = -ETIMEDOUT;
			goto fail;
		}
	}

fail:
	return ret;
}

static int edma_submit_sync_tx(struct pcie_epf_dma *epfnv,
			       struct edma_desc *desc,
			       int nents, int ch, bool lie)
{
	struct pcie_epf_bar0 *epf_bar0 = (struct pcie_epf_bar0 *)
							epfnv->bar0_virt;
	dma_addr_t ll_phy_addr = epf_bar0->ep_phy_addr + DMA_LL_WR_OFFSET(ch);
	struct dma_ll *dma_ll_virt;
	int i, ret;

	epfnv->wr_busy |= 1 << ch;

	/* Program DMA LL base address in DMA LL pointer register */
	dma_channel_wr(epfnv->dma_base, ch, lower_32_bits(ll_phy_addr),
		       DMA_LLP_LOW_OFF_WRCH);
	dma_channel_wr(epfnv->dma_base, ch, upper_32_bits(ll_phy_addr),
		       DMA_LLP_HIGH_OFF_WRCH);

	/* Populate DMA descriptors in LL */
	dma_ll_virt = (struct dma_ll *)
				(epfnv->bar0_virt + DMA_LL_WR_OFFSET(ch));
	for (i = 0; i < nents; i++) {
		dma_ll_virt->size = desc[i].sz;
		dma_ll_virt->src_low = lower_32_bits(desc[i].src);
		dma_ll_virt->src_high = upper_32_bits(desc[i].src);
		dma_ll_virt->dst_low = lower_32_bits(desc[i].dst);
		dma_ll_virt->dst_high = upper_32_bits(desc[i].dst);
		dma_ll_virt->ele.cb = 1;
		dma_ll_virt++;
	}
	/* Set LIE or RIE in last element */
	dma_ll_virt--;
	dma_ll_virt->ele.lie = 1;
	if (!lie)
		dma_ll_virt->ele.rie = 1;

	epfnv->wr_start_time[ch] = ktime_get();
	dma_common_wr(epfnv->dma_base, ch, DMA_WRITE_DOORBELL_OFF);

	ret = wait_event_timeout(epfnv->wr_wq[ch],
				 !(epfnv->wr_busy & (1 << ch)),
				 msecs_to_jiffies(5000));
	if (ret == 0) {
		dev_err(epfnv->fdev, "%s: LL WR CH: %d TO\n", __func__, ch);
		ret = -ETIMEDOUT;
	}

	return ret;
}

static int edma_submit_sync_rx(struct pcie_epf_dma *epfnv,
			       struct edma_desc *desc,
			       int nents, int ch, bool lie)
{
	struct pcie_epf_bar0 *epf_bar0 = (struct pcie_epf_bar0 *)
							epfnv->bar0_virt;
	dma_addr_t ll_phy_addr = epf_bar0->ep_phy_addr + DMA_LL_RD_OFFSET(ch);
	struct dma_ll *dma_ll_virt;
	int i, ret;

	epfnv->rd_busy |= 1 << ch;

	/* Program DMA LL base address in DMA LL pointer register */
	dma_channel_wr(epfnv->dma_base, ch, lower_32_bits(ll_phy_addr),
		       DMA_LLP_LOW_OFF_RDCH);
	dma_channel_wr(epfnv->dma_base, ch, upper_32_bits(ll_phy_addr),
		       DMA_LLP_HIGH_OFF_RDCH);

	/* Populate DMA descriptors in LL */
	dma_ll_virt = (struct dma_ll *)
				(epfnv->bar0_virt + DMA_LL_RD_OFFSET(ch));
	for (i = 0; i < nents; i++) {
		dma_ll_virt->size = desc[i].sz;
		dma_ll_virt->src_low = lower_32_bits(desc[i].src);
		dma_ll_virt->src_high = upper_32_bits(desc[i].src);
		dma_ll_virt->dst_low = lower_32_bits(desc[i].dst);
		dma_ll_virt->dst_high = upper_32_bits(desc[i].dst);
		dma_ll_virt->ele.cb = 1;
		dma_ll_virt++;
	}
	/* Set LIE or RIE in last element */
	dma_ll_virt--;
	dma_ll_virt->ele.lie = 1;
	if (!lie)
		dma_ll_virt->ele.rie = 1;

	epfnv->rd_start_time[ch] = ktime_get();
	dma_common_wr(epfnv->dma_base, ch, DMA_READ_DOORBELL_OFF);

	ret = wait_event_timeout(epfnv->rd_wq[ch],
				 !(epfnv->rd_busy & (1 << ch)),
				 msecs_to_jiffies(5000));
	if (ret == 0) {
		dev_err(epfnv->fdev, "%s: LL RD CH: %d TO\n",  __func__, ch);
		ret = -ETIMEDOUT;
	}

	return ret;
}

static int edma_submit_sync_txrx(struct pcie_epf_dma *epfnv,
				 struct edma_desc *desc_wr,
				 struct edma_desc *desc_rd, int nents)
{
	struct pcie_epf_bar0 *epf_bar0 = (struct pcie_epf_bar0 *)
							epfnv->bar0_virt;
	dma_addr_t phy_addr = epf_bar0->ep_phy_addr;
	struct dma_ll *dma_ll_virt;
	int i, j, ret;

	epfnv->wr_busy = DMA_WR_CHNL_MASK;
	epfnv->rd_busy = DMA_RD_CHNL_MASK;

	for (i = 0; i < DMA_WR_CHNL_NUM; i++) {
		dma_channel_wr(epfnv->dma_base, i,
			       lower_32_bits(phy_addr + DMA_LL_WR_OFFSET(i)),
			       DMA_LLP_LOW_OFF_WRCH);
		dma_channel_wr(epfnv->dma_base, i,
			       upper_32_bits(phy_addr + DMA_LL_WR_OFFSET(i)),
			       DMA_LLP_HIGH_OFF_WRCH);

		dma_ll_virt = (struct dma_ll *)
				(epfnv->bar0_virt + DMA_LL_WR_OFFSET(i));
		for (j = i * nents; j < ((i + 1) * nents); j++) {
			dma_ll_virt->size = desc_wr[j].sz;
			dma_ll_virt->src_low = lower_32_bits(desc_wr[j].src);
			dma_ll_virt->src_high = upper_32_bits(desc_wr[j].src);
			dma_ll_virt->dst_low = lower_32_bits(desc_wr[j].dst);
			dma_ll_virt->dst_high = upper_32_bits(desc_wr[j].dst);
			dma_ll_virt->ele.cb = 1;
			dma_ll_virt++;
		}
		dma_ll_virt--;
		dma_ll_virt->ele.lie = 1;
	}

	for (i = 0; i < DMA_RD_CHNL_NUM; i++) {
		dma_channel_wr(epfnv->dma_base, i,
			       lower_32_bits(phy_addr + DMA_LL_RD_OFFSET(i)),
			       DMA_LLP_LOW_OFF_RDCH);
		dma_channel_wr(epfnv->dma_base, i,
			       upper_32_bits(phy_addr + DMA_LL_RD_OFFSET(i)),
			       DMA_LLP_HIGH_OFF_RDCH);

		dma_ll_virt = (struct dma_ll *)
				(epfnv->bar0_virt + DMA_LL_RD_OFFSET(i));
		for (j = i * nents; j < ((i + 1) * nents); j++) {
			dma_ll_virt->size = desc_rd[j].sz;
			dma_ll_virt->src_low = lower_32_bits(desc_rd[j].src);
			dma_ll_virt->src_high = upper_32_bits(desc_rd[j].src);
			dma_ll_virt->dst_low = lower_32_bits(desc_rd[j].dst);
			dma_ll_virt->dst_high = upper_32_bits(desc_rd[j].dst);
			dma_ll_virt->ele.cb = 1;
			dma_ll_virt++;
		}
		dma_ll_virt--;
		dma_ll_virt->ele.lie = 1;
	}

	for (i = 0; i < DMA_WR_CHNL_NUM; i++) {
		dma_common_wr(epfnv->dma_base, i, DMA_WRITE_DOORBELL_OFF);
		if (i < DMA_RD_CHNL_NUM)
			dma_common_wr(epfnv->dma_base, i,
				      DMA_READ_DOORBELL_OFF);
	}

	for (i = 0; i < DMA_WR_CHNL_NUM; i++) {
		ret = wait_event_timeout(epfnv->wr_wq[i],
					 !(epfnv->wr_busy & (1 << i)),
					 msecs_to_jiffies(5000));
		if (ret == 0) {
			dev_err(epfnv->fdev, "%s: LL WR CH: %d TO\n",
				__func__, i);
			ret = -ETIMEDOUT;
			goto fail;
		}
	}

	for (i = 0; i < DMA_RD_CHNL_NUM; i++) {
		ret = wait_event_timeout(epfnv->rd_wq[i],
					 !(epfnv->rd_busy & (1 << i)),
					 msecs_to_jiffies(5000));
		if (ret == 0) {
			dev_err(epfnv->fdev, "%s: LL RD CH: %d TO\n",
				__func__, i);
			ret = -ETIMEDOUT;
			goto fail;
		}
	}

fail:
	return ret;
}

/* debugfs to measure direct and LL DMA read/write perf on channel 0 */
static int perf_test(struct seq_file *s, void *data)
{
	struct pcie_epf_dma *epfnv = (struct pcie_epf_dma *)
						dev_get_drvdata(s->private);
	struct pcie_epf_bar0 *epf_bar0 = (struct pcie_epf_bar0 *)
						epfnv->bar0_virt;
	struct edma_desc desc;
	struct edma_desc ll_desc[DMA_LL_DEFAULT_SIZE];
	dma_addr_t ep_dma_addr = epf_bar0->ep_phy_addr + BAR0_DMA_BUF_OFFSET;
	dma_addr_t rp_dma_addr = epf_bar0->rp_phy_addr + BAR0_DMA_BUF_OFFSET;
	long long time;
	int ch = 0, nents = DMA_LL_MIN_SIZE, i, ret;

	if (!rp_dma_addr) {
		dev_err(epfnv->fdev, "RP DMA address is null\n");
		return 0;
	}

	edma_init(epfnv, 1);

	/* Direct DMA perf test with size BAR0_DMA_BUF_SIZE */
	desc.src = ep_dma_addr;
	desc.dst = rp_dma_addr;
	desc.sz = BAR0_DMA_BUF_SIZE;
	ret = edma_submit_direct_tx(epfnv, &desc, ch);
	if (ret < 0) {
		dev_err(epfnv->fdev, "%s: DD WR, SZ: %lu B CH: %d failed\n",
			__func__, desc.sz, ch);
		goto fail;
	}

	time = ktime_to_ns(epfnv->wr_end_time[ch]) -
			ktime_to_ns(epfnv->wr_start_time[ch]);
	dev_info(epfnv->fdev, "%s: DD WR, CH: %d SZ: %lu B, time: %lld ns\n",
		 __func__, ch, desc.sz, time);

	desc.src = rp_dma_addr;
	desc.dst = ep_dma_addr;
	desc.sz = BAR0_DMA_BUF_SIZE;
	ret = edma_submit_direct_rx(epfnv, &desc, ch);
	if (ret < 0) {
		dev_err(epfnv->fdev, "%s: DD RD, SZ: %lu B CH: %d failed\n",
			__func__, desc.sz, ch);
		goto fail;
	}
	time = ktime_to_ns(epfnv->rd_end_time[ch]) -
				ktime_to_ns(epfnv->rd_start_time[ch]);
	dev_info(epfnv->fdev, "%s: DD RD, CH: %d SZ: %lu B, time: %lld ns\n",
		 __func__, ch, desc.sz, time);

	/* Clean DMA LL */
	memset(epfnv->bar0_virt + DMA_LL_WR_OFFSET(0), 0, 6 * DMA_LL_SIZE);
	edma_ll_init(epfnv);

	/* LL DMA perf test with size BAR0_DMA_BUF_SIZE and one desc */
	for (i = 0; i < nents; i++) {
		ll_desc[i].src = ep_dma_addr + (i * BAR0_DMA_BUF_SIZE);
		ll_desc[i].dst = rp_dma_addr + (i * BAR0_DMA_BUF_SIZE);
		ll_desc[i].sz = BAR0_DMA_BUF_SIZE;
	}

	ret = edma_submit_sync_tx(epfnv, ll_desc, nents, ch, 1);
	if (ret < 0) {
		dev_err(epfnv->fdev, "%s: LL WR, SZ: %u B CH: %d failed\n",
			__func__, BAR0_DMA_BUF_SIZE * nents, ch);
		goto fail;
	}
	time = ktime_to_ns(epfnv->wr_end_time[ch]) -
				ktime_to_ns(epfnv->wr_start_time[ch]);
	dev_info(epfnv->fdev, "%s: LL WR, CH: %d N: %d SZ: %d B, time: %lld ns\n",
		 __func__, ch, nents, BAR0_DMA_BUF_SIZE, time);

	for (i = 0; i < nents; i++) {
		ll_desc[i].src = rp_dma_addr + (i * BAR0_DMA_BUF_SIZE);
		ll_desc[i].dst = ep_dma_addr + (i * BAR0_DMA_BUF_SIZE);
		ll_desc[i].sz = BAR0_DMA_BUF_SIZE;
	}

	ret = edma_submit_sync_rx(epfnv, ll_desc, nents, ch, 1);
	if (ret < 0) {
		dev_err(epfnv->fdev, "%s: LL RD, SZ: %u B CH: %d failed\n",
			__func__, BAR0_DMA_BUF_SIZE * nents, ch);
		goto fail;
	}
	time = ktime_to_ns(epfnv->rd_end_time[ch]) -
				ktime_to_ns(epfnv->rd_start_time[ch]);
	dev_info(epfnv->fdev, "%s: LL RD, CH: %d N: %d SZ: %d B, time: %lld ns\n",
		 __func__, ch, nents, BAR0_DMA_BUF_SIZE, time);

	edma_ll_deinit(epfnv);
	edma_deinit(epfnv);

fail:
	return 0;
}

/* debugfs to stress direct and LL DMA on all wr & rd channels */
static int stress_test(struct seq_file *s, void *data)
{
	struct pcie_epf_dma *epfnv = (struct pcie_epf_dma *)
					dev_get_drvdata(s->private);
	struct pcie_epf_bar0 *epf_bar0 = (struct pcie_epf_bar0 *)
					epfnv->bar0_virt;
	struct edma_desc desc_wr[DMA_WR_CHNL_NUM], desc_rd[DMA_RD_CHNL_NUM];
	struct edma_desc ll_desc_wr[DMA_WR_CHNL_NUM * DMA_LL_DEFAULT_SIZE];
	struct edma_desc ll_desc_rd[DMA_RD_CHNL_NUM * DMA_LL_DEFAULT_SIZE];
	dma_addr_t ep_dma_addr = epf_bar0->ep_phy_addr + BAR0_DMA_BUF_OFFSET;
	dma_addr_t rp_dma_addr = epf_bar0->rp_phy_addr + BAR0_DMA_BUF_OFFSET;
	int i, j, ret, nents = DMA_LL_DEFAULT_SIZE;

	if (!rp_dma_addr) {
		dev_err(epfnv->fdev, "RP DMA address is null\n");
		return 0;
	}

	edma_init(epfnv, 1);

	/* Direct DMA stress test with rand size < DMA_DD_BUF_SIZE */
	for (j = 0; j < DMA_WR_CHNL_NUM; j++) {
		desc_wr[j].src = ep_dma_addr + (j * DMA_DD_BUF_SIZE);
		desc_wr[j].dst = rp_dma_addr + (j * DMA_DD_BUF_SIZE);
	}

	for (j = 0; j < DMA_RD_CHNL_NUM; j++) {
		desc_rd[j].src = rp_dma_addr +
				((j + DMA_WR_CHNL_NUM) * DMA_DD_BUF_SIZE);
		desc_rd[j].dst = ep_dma_addr +
				((j + DMA_WR_CHNL_NUM) * DMA_DD_BUF_SIZE);
	}

	for (i = 0; i < epfnv->stress_count; i++) {
		for (j = 0; j < DMA_WR_CHNL_NUM; j++)
			desc_wr[j].sz =
				(get_random_u32() % DMA_DD_BUF_SIZE) + 1;
		for (j = 0; j < DMA_RD_CHNL_NUM; j++)
			desc_rd[j].sz =
				(get_random_u32() % DMA_DD_BUF_SIZE) + 1;
		ret = edma_submit_direct_txrx(epfnv, desc_wr, desc_rd);
		if (ret < 0) {
			dev_err(epfnv->fdev, "%s: DD stress failed\n",
				__func__);
			goto fail;
		}
		dev_info(epfnv->fdev, "%s: DD stress test iteration %d done\n",
			 __func__, i);
	}
	dev_info(epfnv->fdev, "%s: DD stress: all CH, rand SZ, count: %d success\n",
		 __func__, epfnv->stress_count);

	/* Clean DMA LL */
	memset(epfnv->bar0_virt + DMA_LL_WR_OFFSET(0), 0, 6 * DMA_LL_SIZE);
	edma_ll_init(epfnv);

	/* LL DMA stress test with rand size < DMA_LL_BUF_SIZE per desc */
	for (i = 0; i < DMA_WR_CHNL_NUM * nents; i++) {
		ll_desc_wr[i].src = ep_dma_addr + (i * DMA_LL_BUF_SIZE);
		ll_desc_wr[i].dst = rp_dma_addr + (i * DMA_LL_BUF_SIZE);
	}
	for (i = 0; i < DMA_RD_CHNL_NUM * nents; i++) {
		ll_desc_rd[i].src = rp_dma_addr +
				((i + DMA_WR_CHNL_NUM) * DMA_LL_BUF_SIZE);
		ll_desc_rd[i].dst = ep_dma_addr +
				((i + DMA_WR_CHNL_NUM) * DMA_LL_BUF_SIZE);
	}

	for (i = 0; i < epfnv->stress_count; i++) {
		for (j = 0; j < DMA_WR_CHNL_NUM * nents; j++)
			ll_desc_wr[j].sz =
				(get_random_u32() % DMA_LL_BUF_SIZE) + 1;
		for (j = 0; j < DMA_RD_CHNL_NUM * nents; j++)
			ll_desc_rd[j].sz =
				(get_random_u32() % DMA_LL_BUF_SIZE) + 1;
		ret = edma_submit_sync_txrx(epfnv, ll_desc_wr, ll_desc_rd,
					    nents);
		if (ret < 0) {
			dev_err(epfnv->fdev, "%s: DMA LL stress failed\n",
				__func__);
			goto fail;
		}
		dev_info(epfnv->fdev, "%s: LL stress test iteration %d done\n",
			 __func__, i);
	}
	dev_info(epfnv->fdev, "%s: LL stress: all CH, rand SZ, count: %d success\n",
		 __func__, epfnv->stress_count);

	edma_ll_deinit(epfnv);
	edma_deinit(epfnv);

fail:
	return 0;
}

/* debugfs to perform eDMA lib transfers and do CRC check */
static int edmalib_test(struct seq_file *s, void *data)
{
	struct pcie_epf_dma *epfnv = (struct pcie_epf_dma *)
						dev_get_drvdata(s->private);
	struct pcie_epf_bar0 *epf_bar0 = (struct pcie_epf_bar0 *)
						epfnv->bar0_virt;

	if (!epf_bar0->rp_phy_addr) {
		dev_err(epfnv->fdev, "RP DMA address is null\n");
		return -1;
	}

	epfnv->edma.src_dma_addr = epf_bar0->ep_phy_addr + BAR0_DMA_BUF_OFFSET;
	epfnv->edma.dst_dma_addr = epf_bar0->rp_phy_addr + BAR0_DMA_BUF_OFFSET;
	epfnv->edma.fdev = epfnv->fdev;
	epfnv->edma.bar0_virt = epfnv->bar0_virt;
	epfnv->edma.src_virt = epfnv->bar0_virt + BAR0_DMA_BUF_OFFSET;
	epfnv->edma.dma_base = epfnv->dma_base;
	epfnv->edma.dma_size = epfnv->dma_size;
	epfnv->edma.stress_count = epfnv->stress_count;
	epfnv->edma.edma_ch = epfnv->edma_ch;
	epfnv->edma.nents = epfnv->nents;
	epfnv->edma.of_node = epfnv->cdev->of_node;

	return edmalib_common_test(&epfnv->edma);
}

/* debugfs to perform direct & LL DMA and do CRC check */
static int sanity_test(struct seq_file *s, void *data)
{
	struct pcie_epf_dma *epfnv = (struct pcie_epf_dma *)
						dev_get_drvdata(s->private);
	struct pcie_epf_bar0 *epf_bar0 = (struct pcie_epf_bar0 *)
						epfnv->bar0_virt;
	struct edma_desc desc;
	struct edma_desc ll_desc[DMA_LL_DEFAULT_SIZE];
	dma_addr_t ep_dma_addr = epf_bar0->ep_phy_addr + BAR0_DMA_BUF_OFFSET;
	dma_addr_t rp_dma_addr = epf_bar0->rp_phy_addr + BAR0_DMA_BUF_OFFSET;
	int nents = DMA_LL_DEFAULT_SIZE;
	int i, j, ret;
	u32 crc;

	if (epfnv->dma_size > MAX_DMA_ELE_SIZE) {
		dev_err(epfnv->fdev, "%s: dma_size should be <= 0x%x\n",
			__func__, MAX_DMA_ELE_SIZE);
		goto fail;
	}

	if (!rp_dma_addr) {
		dev_err(epfnv->fdev, "RP DMA address is null\n");
		return 0;
	}

	edma_init(epfnv, 0);

	/* Direct DMA of size epfnv->dma_size */
	for (i = 0; i < DMA_WR_CHNL_NUM; i++) {
		desc.src = ep_dma_addr;
		desc.dst = rp_dma_addr;
		desc.sz = epfnv->dma_size;
		epf_bar0->wr_data[i].size = desc.sz;
		/* generate random bytes to transfer */
		get_random_bytes(epfnv->bar0_virt + BAR0_DMA_BUF_OFFSET,
				 desc.sz);
		ret = edma_submit_direct_tx(epfnv, &desc, i);
		if (ret < 0) {
			dev_err(epfnv->fdev, "%s: DD WR CH: %d failed\n",
				__func__, i);
			goto fail;
		}
		crc = crc32_le(~0, epfnv->bar0_virt + BAR0_DMA_BUF_OFFSET,
			       desc.sz);
		if (crc != epf_bar0->wr_data[i].crc) {
			dev_err(epfnv->fdev, "%s: DD WR, SZ: %lu B CH: %d CRC failed\n",
				__func__, desc.sz, i);
			goto fail;
		}
		dev_info(epfnv->fdev, "%s: DD WR, SZ: %lu B CH: %d success\n",
			 __func__, desc.sz, i);
	}

	for (i = 0; i < DMA_RD_CHNL_NUM; i++) {
		desc.src = rp_dma_addr;
		desc.dst = ep_dma_addr;
		desc.sz = epfnv->dma_size;
		epf_bar0->rd_data[i].size = desc.sz;
		/* Clear memory to receive data */
		memset(epfnv->bar0_virt + BAR0_DMA_BUF_OFFSET, 0, desc.sz);
		ret = edma_submit_direct_rx(epfnv, &desc, i);
		if (ret < 0) {
			dev_err(epfnv->fdev, "%s: DD RD CH: %d failed\n",
				__func__, i);
			goto fail;
		}
		crc = crc32_le(~0, epfnv->bar0_virt + BAR0_DMA_BUF_OFFSET,
			       desc.sz);
		if (crc != epf_bar0->rd_data[i].crc) {
			dev_err(epfnv->fdev, "%s: DD RD, SZ: %lu B CH: %d CRC failed\n",
				__func__, desc.sz, i);
			goto fail;
		}
		dev_info(epfnv->fdev, "%s: DD RD, SZ: %lu B CH: %d success\n",
			 __func__, desc.sz, i);
	}

	/* Clean DMA LL all 6 channels */
	memset(epfnv->bar0_virt + DMA_LL_WR_OFFSET(0), 0, 6 * DMA_LL_SIZE);
	edma_ll_init(epfnv);

	/* LL DMA with size epfnv->dma_size per desc */
	for (i = 0; i < DMA_WR_CHNL_NUM; i++) {
		for (j = 0; j < nents; j++) {
			ll_desc[j].src = ep_dma_addr + (j * epfnv->dma_size);
			ll_desc[j].dst = rp_dma_addr + (j * epfnv->dma_size);
			ll_desc[j].sz = epfnv->dma_size;
		}
		epf_bar0->wr_data[i].size = epfnv->dma_size * nents;
		/* generate random bytes to transfer */
		get_random_bytes(epfnv->bar0_virt + BAR0_DMA_BUF_OFFSET,
				 epf_bar0->wr_data[i].size);

		ret = edma_submit_sync_tx(epfnv, ll_desc, nents, i, 0);
		if (ret < 0) {
			dev_err(epfnv->fdev, "%s: LL WR CH: %d failed\n",
				__func__, i);
			goto fail;
		}
		crc = crc32_le(~0, epfnv->bar0_virt + BAR0_DMA_BUF_OFFSET,
			       epfnv->dma_size * nents);
		if (crc != epf_bar0->wr_data[i].crc) {
			dev_err(epfnv->fdev, "%s: LL WR, SZ: %u B CH: %d CRC failed\n",
				__func__, epfnv->dma_size, i);
			goto fail;
		}
		dev_info(epfnv->fdev, "%s: LL WR, SZ: %u B CH: %d success\n",
			 __func__, epfnv->dma_size, i);
	}

	for (i = 0; i < DMA_RD_CHNL_NUM; i++) {
		for (j = 0; j < nents; j++) {
			ll_desc[j].src = rp_dma_addr + (j * epfnv->dma_size);
			ll_desc[j].dst = ep_dma_addr + (j * epfnv->dma_size);
			ll_desc[j].sz = epfnv->dma_size;
		}
		epf_bar0->rd_data[i].size = epfnv->dma_size * nents;
		/* Clear memory to receive data */
		memset(epfnv->bar0_virt + BAR0_DMA_BUF_OFFSET, 0,
		       epf_bar0->rd_data[i].size);

		ret = edma_submit_sync_rx(epfnv, ll_desc, nents, i, 0);
		if (ret < 0) {
			dev_err(epfnv->fdev, "%s: LL RD failed\n", __func__);
			goto fail;
		}
		crc = crc32_le(~0, epfnv->bar0_virt + BAR0_DMA_BUF_OFFSET,
			       epfnv->dma_size * nents);
		if (crc != epf_bar0->rd_data[i].crc) {
			dev_err(epfnv->fdev, "%s: LL RD, SZ: %u B CH: %d CRC failed\n",
				__func__, epfnv->dma_size, i);
			goto fail;
		}
		dev_info(epfnv->fdev, "%s: LL RD, SZ: %u B CH: %d success\n",
			 __func__, epfnv->dma_size, i);
	}

	edma_ll_deinit(epfnv);
	edma_deinit(epfnv);

fail:
	return 0;
}

static int async_dma_test_fn(struct pcie_epf_dma *epfnv, int ch)
{
	struct pcie_epf_bar0 *epf_bar0 = (struct pcie_epf_bar0 *)
					 epfnv->bar0_virt;
	dma_addr_t ep_dma_addr, rp_dma_addr, phy_addr;
	struct dma_ll *dma_ll_virt;
	u32 nents = epfnv->async_count, count = 0, idx, i;

	ep_dma_addr = epf_bar0->ep_phy_addr + BAR0_DMA_BUF_OFFSET +
		      (ch * DMA_ASYNC_LL_SIZE * SZ_64K);
	rp_dma_addr = epf_bar0->rp_phy_addr + BAR0_DMA_BUF_OFFSET +
		      (ch * DMA_ASYNC_LL_SIZE * SZ_64K);

	epfnv->wr_cnt[ch] = 0;
	epfnv->rd_cnt[ch] = 0;

	dma_ll_virt = (struct dma_ll *)
		      (epfnv->bar0_virt + DMA_LL_WR_OFFSET(ch));
	phy_addr = epf_bar0->ep_phy_addr + DMA_LL_WR_OFFSET(ch);
	dma_ll_virt[DMA_ASYNC_LL_SIZE].src_low = lower_32_bits(phy_addr);
	dma_ll_virt[DMA_ASYNC_LL_SIZE].src_high = upper_32_bits(phy_addr);
	dma_ll_virt[DMA_ASYNC_LL_SIZE].ele.llp = 1;
	dma_ll_virt[DMA_ASYNC_LL_SIZE].ele.tcb = 1;
	epfnv->pcs[ch] = 1;
	dma_ll_virt[DMA_ASYNC_LL_SIZE].ele.cb = !epfnv->pcs[ch];

	for (i = 0; i < nents; i++) {
		while ((epfnv->wr_cnt[ch] - epfnv->rd_cnt[ch] + 2) >=
			DMA_ASYNC_LL_SIZE) {
			msleep(100);
			if (++count == 100) {
				dev_info(epfnv->fdev, "%s: CH: %d LL is full wr_cnt: %u rd_cnt: %u\n",
					 __func__, ch, epfnv->wr_cnt[ch],
					 epfnv->rd_cnt[ch]);
				goto fail;
			}
		}
		count = 0;

		idx = i % DMA_ASYNC_LL_SIZE;

		dma_ll_virt[idx].size = SZ_64K;
		if (ch < DMA_WR_CHNL_NUM) {
			phy_addr = ep_dma_addr +
					(idx % DMA_ASYNC_LL_SIZE) * SZ_64K;
			dma_ll_virt[idx].src_low = lower_32_bits(phy_addr);
			dma_ll_virt[idx].src_high = upper_32_bits(phy_addr);
			phy_addr = rp_dma_addr +
					(idx % DMA_ASYNC_LL_SIZE) * SZ_64K;
			dma_ll_virt[idx].dst_low = lower_32_bits(phy_addr);
			dma_ll_virt[idx].dst_high = upper_32_bits(phy_addr);
		} else {
			phy_addr = rp_dma_addr +
					(idx % DMA_ASYNC_LL_SIZE) * SZ_64K;
			dma_ll_virt[idx].src_low = lower_32_bits(phy_addr);
			dma_ll_virt[idx].src_high = upper_32_bits(phy_addr);
			phy_addr = ep_dma_addr +
					(idx % DMA_ASYNC_LL_SIZE) * SZ_64K;
			dma_ll_virt[idx].dst_low = lower_32_bits(phy_addr);
			dma_ll_virt[idx].dst_high = upper_32_bits(phy_addr);
		}
		dma_ll_virt[idx].ele.lie = 1;
		/*
		 * DMA desc should not be touched after CB bit set, add
		 * write barrier to stop desc writes going out of order
		 * wrt CB bit set.
		 */
		wmb();
		dma_ll_virt[idx].ele.cb = epfnv->pcs[ch];
		if (idx == (DMA_ASYNC_LL_SIZE - 1)) {
			epfnv->pcs[ch] = !epfnv->pcs[ch];
			dma_ll_virt[idx + 1].ele.cb  = epfnv->pcs[ch];
		}
		if (ch < DMA_WR_CHNL_NUM)
			dma_common_wr(epfnv->dma_base, ch,
				      DMA_WRITE_DOORBELL_OFF);
		else
			dma_common_wr(epfnv->dma_base, ch - DMA_WR_CHNL_NUM,
				      DMA_READ_DOORBELL_OFF);
		epfnv->wr_cnt[ch]++;
		/* Print status for very 10000 iterations */
		if ((i % 10000) == 0)
			dev_info(epfnv->fdev, "%s: CH: %u async DMA test itr: %u done, wr_cnt: %u rd_cnt: %u\n",
				 __func__, ch, i, epfnv->wr_cnt[ch],
				 epfnv->rd_cnt[ch]);
	}
	count = 0;
	while (epfnv->wr_cnt[ch] != epfnv->rd_cnt[ch]) {
		msleep(20);
		if (++count == 100) {
			dev_info(epfnv->fdev, "%s: CH: %d async DMA test failed, wr_cnt: %u rd_cnt: %u\n",
				 __func__, ch, epfnv->wr_cnt[ch],
				 epfnv->rd_cnt[ch]);
			goto fail;
		}
	}
	dev_info(epfnv->fdev, "%s: CH: %d async DMA success\n", __func__, ch);

fail:
	epfnv->wr_cnt[ch] = 0;
	epfnv->rd_cnt[ch] = 0;

	return 0;
}

static int async_wr0_work(void *data)
{
	struct pcie_epf_dma *epfnv = data;

	async_dma_test_fn(epfnv, 0);

	epfnv->task_done++;
	wake_up(&epfnv->task_wq);

	return 0;
}

static int async_wr1_work(void *data)
{
	struct pcie_epf_dma *epfnv = data;

	async_dma_test_fn(epfnv, 1);

	epfnv->task_done++;
	wake_up(&epfnv->task_wq);

	return 0;
}

static int async_wr2_work(void *data)
{
	struct pcie_epf_dma *epfnv = data;

	async_dma_test_fn(epfnv, 2);

	epfnv->task_done++;
	wake_up(&epfnv->task_wq);

	return 0;
}

static int async_wr3_work(void *data)
{
	struct pcie_epf_dma *epfnv = data;

	async_dma_test_fn(epfnv, 3);

	epfnv->task_done++;
	wake_up(&epfnv->task_wq);

	return 0;
}

static int async_rd0_work(void *data)
{
	struct pcie_epf_dma *epfnv = data;

	async_dma_test_fn(epfnv, 4);

	epfnv->task_done++;
	wake_up(&epfnv->task_wq);

	return 0;
}

static int async_rd1_work(void *data)
{
	struct pcie_epf_dma *epfnv = data;

	async_dma_test_fn(epfnv, 5);

	epfnv->task_done++;
	wake_up(&epfnv->task_wq);

	return 0;
}

static int async_dma_test(struct seq_file *s, void *data)
{
	struct pcie_epf_dma *epfnv = (struct pcie_epf_dma *)
				     dev_get_drvdata(s->private);
	struct pcie_epf_bar0 *epf_bar0 = (struct pcie_epf_bar0 *)
					 epfnv->bar0_virt;
	dma_addr_t phy_addr;
	int i;

	epfnv->task_done = 0;
	epfnv->async_dma = true;

	edma_init(epfnv, 1);
	/* Clean DMA LL all 6 channels */
	memset(epfnv->bar0_virt + DMA_LL_WR_OFFSET(0), 0, 6 * DMA_LL_SIZE);
	edma_ll_init(epfnv);

	/* Program DMA LL base address in DMA LL pointer register */
	for (i = 0; i < DMA_WR_CHNL_NUM; i++) {
		phy_addr = epf_bar0->ep_phy_addr + DMA_LL_WR_OFFSET(i);
		dma_channel_wr(epfnv->dma_base, i, lower_32_bits(phy_addr),
			       DMA_LLP_LOW_OFF_WRCH);
		dma_channel_wr(epfnv->dma_base, i, upper_32_bits(phy_addr),
			       DMA_LLP_HIGH_OFF_WRCH);
	}

	for (i = 0; i < DMA_RD_CHNL_NUM; i++) {
		phy_addr = epf_bar0->ep_phy_addr + DMA_LL_RD_OFFSET(i);
		dma_channel_wr(epfnv->dma_base, i, lower_32_bits(phy_addr),
			       DMA_LLP_LOW_OFF_RDCH);
		dma_channel_wr(epfnv->dma_base, i, upper_32_bits(phy_addr),
			       DMA_LLP_HIGH_OFF_RDCH);
	}

	epfnv->wr0_task = kthread_create_on_cpu(async_wr0_work, epfnv, 0,
						"async_wr0_work");
	if (IS_ERR(epfnv->wr0_task)) {
		dev_err(epfnv->fdev, "failed to create async_wr0 thread\n");
		goto wr0_task;
	}
	epfnv->wr1_task = kthread_create_on_cpu(async_wr1_work, epfnv, 1,
						"async_wr1_work");
	if (IS_ERR(epfnv->wr1_task)) {
		dev_err(epfnv->fdev, "failed to create async_wr1 thread\n");
		goto wr1_task;
	}
	epfnv->wr2_task = kthread_create_on_cpu(async_wr2_work, epfnv, 2,
						"async_wr2_work");
	if (IS_ERR(epfnv->wr2_task)) {
		dev_err(epfnv->fdev, "failed to create async_wr2 thread\n");
		goto wr2_task;
	}
	epfnv->wr3_task = kthread_create_on_cpu(async_wr3_work, epfnv, 3,
						"async_wr3_work");
	if (IS_ERR(epfnv->wr3_task)) {
		dev_err(epfnv->fdev, "failed to create async_wr3 thread\n");
		goto wr3_task;
	}
	epfnv->rd0_task = kthread_create_on_cpu(async_rd0_work, epfnv, 4,
						"async_rd0_work");
	if (IS_ERR(epfnv->rd0_task)) {
		dev_err(epfnv->fdev, "failed to create async_rd0 thread\n");
		goto rd0_task;
	}
	epfnv->rd1_task = kthread_create_on_cpu(async_rd1_work, epfnv, 5,
						"async_rd1_work");
	if (IS_ERR(epfnv->rd1_task)) {
		dev_err(epfnv->fdev, "failed to create async_rd1 thread\n");
		goto rd1_task;
	}

	init_waitqueue_head(&epfnv->task_wq);

	wake_up_process(epfnv->wr0_task);
	wake_up_process(epfnv->wr1_task);
	wake_up_process(epfnv->wr2_task);
	wake_up_process(epfnv->wr3_task);
	wake_up_process(epfnv->rd0_task);
	wake_up_process(epfnv->rd1_task);

	wait_event(epfnv->task_wq,
		   (epfnv->task_done == (DMA_WR_CHNL_NUM + DMA_RD_CHNL_NUM)));
	dev_info(epfnv->fdev, "%s: Async DMA test done\n", __func__);

	edma_ll_deinit(epfnv);
	edma_deinit(epfnv);

	epfnv->async_dma = false;
	epfnv->task_done = 0;

	return 0;

rd1_task:
	kthread_stop(epfnv->rd0_task);
rd0_task:
	kthread_stop(epfnv->wr3_task);
wr3_task:
	kthread_stop(epfnv->wr2_task);
wr2_task:
	kthread_stop(epfnv->wr1_task);
wr1_task:
	kthread_stop(epfnv->wr0_task);
wr0_task:
	epfnv->async_dma = false;
	epfnv->task_done = 0;

	return 0;
}

static void init_debugfs(struct pcie_epf_dma *epfnv)
{
	debugfs_create_devm_seqfile(epfnv->fdev, "perf_test", epfnv->debugfs,
				    perf_test);

	debugfs_create_devm_seqfile(epfnv->fdev, "stress_test", epfnv->debugfs,
				    stress_test);

	debugfs_create_devm_seqfile(epfnv->fdev, "sanity_test", epfnv->debugfs,
				    sanity_test);

	debugfs_create_devm_seqfile(epfnv->fdev, "async_dma_test",
				    epfnv->debugfs, async_dma_test);
	debugfs_create_devm_seqfile(epfnv->fdev, "edmalib_test", epfnv->debugfs,
				    edmalib_test);

	debugfs_create_u32("dma_size", 0644, epfnv->debugfs, &epfnv->dma_size);
	epfnv->dma_size = SZ_1M;
	epfnv->edma.st_as_ch = -1;

	debugfs_create_u32("edma_ch", 0644, epfnv->debugfs, &epfnv->edma_ch);
	/* Enable ASYNC for ch 0 as default and other channels. Usage:
	 * BITS 0-3 - Async mode or sync mode for corresponding WR channels
	 * BITS 4-7 - Enable/disable corresponding WR channel
	 * BITS 8-9 - Async mode or sync mode for corresponding RD channels
	 * BITS 10-11 - Enable/disable or corresponding RD channels
	 * Bit 12 - Abort testing.
	 */
	epfnv->edma_ch = 0xF1;

	debugfs_create_u32("nents", 0644, epfnv->debugfs, &epfnv->nents);
	/* Set DMA_LL_DEFAULT_SIZE as default nents, Max NUM_EDMA_DESC */
	epfnv->nents = DMA_LL_DEFAULT_SIZE;

	debugfs_create_u32("stress_count", 0644, epfnv->debugfs,
			   &epfnv->stress_count);
	epfnv->stress_count = DEFAULT_STRESS_COUNT;

	debugfs_create_u32("async_count", 0644, epfnv->debugfs,
			   &epfnv->async_count);
	epfnv->async_count = 4096;
}

static void pcie_dma_epf_write_msi_msg(struct msi_desc *desc,
				       struct msi_msg *msg)
{
	/* TODO get rid of global variable gepfnv */
	struct pcie_epf_bar0 *epf_bar0 = (struct pcie_epf_bar0 *)
							gepfnv->bar0_virt;
	struct device *cdev = msi_desc_to_dev(desc);
	int idx = desc->platform.msi_index;

	epf_bar0->msi_data[idx] = msg->data;
	dev_info(cdev, "%s: MSI idx: %d data: %d\n", __func__, idx, msg->data);
}

static int pcie_dma_epf_core_init(struct pci_epf *epf)
{
	struct pci_epc *epc = epf->epc;
	struct device *fdev = &epf->dev;
	struct pci_epf_bar *epf_bar;
	int ret;

	ret = pci_epc_write_header(epc, epf->func_no, epf->header);
	if (ret < 0) {
		dev_err(fdev, "Failed to write PCIe header: %d\n", ret);
		return ret;
	}

	epf_bar = &epf->bar[BAR_0];
	ret = pci_epc_set_bar(epc, epf->func_no, epf_bar);
	if (ret < 0) {
		dev_err(fdev, "PCIe set BAR0 failed: %d\n", ret);
		return ret;
	}

	dev_info(fdev, "BAR0 phy_addr: %llx size: %lx\n",
		 epf_bar->phys_addr, epf_bar->size);

	ret = pci_epc_set_msi(epc, epf->func_no, epf->msi_interrupts);
	if (ret) {
		dev_err(fdev, "pci_epc_set_msi() failed: %d\n", ret);
		return ret;
	}

	return 0;
}

static int pcie_dma_epf_msi_init(struct pci_epf *epf)
{
	struct pcie_epf_dma *epfnv = epf_get_drvdata(epf);
	struct pci_epc *epc = epf->epc;
	struct device *cdev = epc->dev.parent;
	struct device *fdev = &epf->dev;
	struct msi_desc *desc;
	int ret;

	/* LL DMA in sanity test will not work without MSI for EP */
	if (!cdev->msi_domain) {
		dev_info(fdev, "msi_domain absent, no interrupts\n");
		return 0;
	}
	ret = platform_msi_domain_alloc_irqs(cdev,
					     DMA_WR_CHNL_NUM + DMA_RD_CHNL_NUM,
					     pcie_dma_epf_write_msi_msg);
	if (ret < 0) {
		dev_err(fdev, "failed to allocate MSIs\n");
		return ret;
	}

	for_each_msi_entry(desc, cdev) {
		switch (desc->platform.msi_index) {
		case 0:
			ret = request_irq(desc->irq, pcie_dma_epf_wr0_msi, 0,
					  "pcie_dma_wr0", epfnv);
			if (ret < 0)
				dev_err(fdev, "failed to register wr0 irq\n");
			break;
		case 1:
			ret = request_irq(desc->irq, pcie_dma_epf_wr1_msi, 0,
					  "pcie_dma_wr1", epfnv);
			if (ret < 0)
				dev_err(fdev, "failed to register wr1 irq\n");
			break;
		case 2:
			ret = request_irq(desc->irq, pcie_dma_epf_wr2_msi, 0,
					  "pcie_dma_wr2", epfnv);
			if (ret < 0)
				dev_err(fdev, "failed to register wr2 irq\n");
			break;
		case 3:
			ret = request_irq(desc->irq, pcie_dma_epf_wr3_msi, 0,
					  "pcie_dma_wr3", epfnv);
			if (ret < 0)
				dev_err(fdev, "failed to register wr3 irq\n");
			break;
		case 4:
			ret = request_irq(desc->irq, pcie_dma_epf_rd0_msi, 0,
					  "pcie_dma_rd0", epfnv);
			if (ret < 0)
				dev_err(fdev, "failed to register rd0 irq\n");
			break;
		case 5:
			ret = request_irq(desc->irq, pcie_dma_epf_rd1_msi, 0,
					  "pcie_dma_rd1", epfnv);
			if (ret < 0)
				dev_err(fdev, "failed to register rd1 irq\n");
			break;
		default:
			dev_err(fdev, "Unknown MSI irq: %d\n", desc->irq);
			continue;
		}
	}

	return 0;
}

static void pcie_dma_epf_msi_deinit(struct pci_epf *epf)
{
	struct pcie_epf_dma *epfnv = epf_get_drvdata(epf);
	struct pci_epc *epc = epf->epc;
	struct device *cdev = epc->dev.parent;
	struct device *fdev = &epf->dev;
	struct msi_desc *desc;

	/* LL DMA in sanity test will not work without MSI for EP */
	if (!cdev->msi_domain) {
		dev_info(fdev, "msi_domain absent, no interrupts\n");
		return;
	}

	for_each_msi_entry(desc, cdev)
		free_irq(desc->irq, epfnv);

	platform_msi_domain_free_irqs(cdev);
}

static int pcie_dma_epf_notifier(struct notifier_block *nb,
				 unsigned long val, void *data)
{
	struct pci_epf *epf = container_of(nb, struct pci_epf, nb);
	int ret;

	switch (val) {
	case CORE_INIT:
		ret = pcie_dma_epf_core_init(epf);
		if (ret < 0)
			return NOTIFY_BAD;
		break;

	case LINK_UP:
		break;

	default:
		dev_err(&epf->dev, "Invalid notifier event\n");
		return NOTIFY_BAD;
	}

	return NOTIFY_OK;
}

static int pcie_dma_epf_block_notifier(struct notifier_block *nb,
				       unsigned long val, void *data)
{
	struct pci_epf *epf = container_of(nb, struct pci_epf, block_nb);
	struct pcie_epf_dma *epfnv = epf_get_drvdata(epf);
	void *cookie = epfnv->edma.cookie;
	struct pcie_epf_bar0 *epf_bar0 = (struct pcie_epf_bar0 *) epfnv->bar0_virt;

	switch (val) {
	case CORE_DEINIT:
		epfnv->edma.cookie = NULL;
		epf_bar0->rp_phy_addr = 0;
		tegra_pcie_edma_deinit(cookie);
		break;

	default:
		dev_err(&epf->dev, "Invalid blocking notifier event\n");
		return NOTIFY_BAD;
	}

	return NOTIFY_OK;
}

static void pcie_dma_epf_unbind(struct pci_epf *epf)
{
	struct pcie_epf_dma *epfnv = epf_get_drvdata(epf);
	struct pci_epc *epc = epf->epc;
	struct pci_epf_bar *epf_bar = &epf->bar[BAR_0];
	void *cookie = epfnv->edma.cookie;
	struct pcie_epf_bar0 *epf_bar0 = (struct pcie_epf_bar0 *) epfnv->bar0_virt;

	epfnv->edma.cookie = NULL;
	epf_bar0->rp_phy_addr = 0;
	tegra_pcie_edma_deinit(cookie);

	pcie_dma_epf_msi_deinit(epf);
	pci_epc_stop(epc);
	pci_epc_clear_bar(epc, epf->func_no, epf_bar);
	pci_epf_free_space(epf, epfnv->bar0_virt, BAR_0);
}

static int pcie_dma_epf_bind(struct pci_epf *epf)
{
	struct pci_epc *epc = epf->epc;
	struct pcie_epf_dma *epfnv = epf_get_drvdata(epf);
	struct device *fdev = &epf->dev;
	struct device *cdev = epc->dev.parent;
	struct platform_device *pdev = of_find_device_by_node(cdev->of_node);
	struct pci_epf_bar *epf_bar = &epf->bar[BAR_0];
	struct pcie_epf_bar0 *epf_bar0;
	struct resource *res;
	char *name;
	int ret, i;

	epfnv->fdev = fdev;
	epfnv->cdev = cdev;

	epfnv->bar0_virt = pci_epf_alloc_space(epf, BAR0_SIZE, BAR_0, SZ_64K);
	if (!epfnv->bar0_virt) {
		dev_err(fdev, "Failed to allocate memory for BAR0\n");
		return -ENOMEM;
	}
	get_random_bytes(epfnv->bar0_virt, BAR0_SIZE);
	memset(epfnv->bar0_virt, 0, BAR0_HEADER_SIZE);

	/* Update BAR header with EP DMA PHY addr */
	epf_bar0 = (struct pcie_epf_bar0 *)epfnv->bar0_virt;
	epf_bar0->ep_phy_addr = epf_bar->phys_addr;
	/* Set BAR0 mem type as 64-bit */
	epf_bar->flags |= PCI_BASE_ADDRESS_MEM_TYPE_64 |
			PCI_BASE_ADDRESS_MEM_PREFETCH;

	epf->nb.notifier_call = pcie_dma_epf_notifier;
	pci_epc_register_notifier(epc, &epf->nb);

	epf->block_nb.notifier_call = pcie_dma_epf_block_notifier;
	pci_epc_register_block_notifier(epc, &epf->block_nb);

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "atu_dma");
	if (!res) {
		dev_err(fdev, "missing atu_dma resource in DT\n");
		ret = PTR_ERR(res);
		goto fail_atu_dma;
	}

	epfnv->dma_base = devm_ioremap(fdev, res->start + DMA_OFFSET,
				       resource_size(res) - DMA_OFFSET);
	if (IS_ERR(epfnv->dma_base)) {
		ret = PTR_ERR(epfnv->dma_base);
		dev_err(fdev, "dma region map failed: %d\n", ret);
		goto fail_atu_dma;
	}

	epfnv->irq = platform_get_irq_byname(pdev, "intr");
	if (!epfnv->irq) {
		dev_err(fdev, "failed to get intr interrupt\n");
		ret = -ENODEV;
		goto fail_atu_dma;
	}

	name = devm_kasprintf(fdev, GFP_KERNEL, "%s_epf_dma_test", pdev->name);
	if (!name) {
		ret = -ENOMEM;
		goto fail_atu_dma;
	}

	ret = devm_request_threaded_irq(fdev, epfnv->irq, pcie_dma_epf_irq,
					pcie_dma_epf_irq_handler,
					IRQF_SHARED,
					name, epfnv);
	if (ret < 0) {
		dev_err(fdev, "failed to request \"intr\" irq\n");
		goto fail_atu_dma;
	}

	ret = pcie_dma_epf_msi_init(epf);
	if (ret < 0) {
		dev_err(fdev, "failed to init platform msi: %d\n", ret);
		goto fail_atu_dma;
	}

	for (i = 0; i < DMA_WR_CHNL_NUM; i++) {
		init_waitqueue_head(&epfnv->wr_wq[i]);
		init_waitqueue_head(&epfnv->edma.wr_wq[i]);
	}

	for (i = 0; i < DMA_RD_CHNL_NUM; i++) {
		init_waitqueue_head(&epfnv->rd_wq[i]);
		init_waitqueue_head(&epfnv->edma.rd_wq[i]);
	}

	epfnv->debugfs = debugfs_create_dir(name, NULL);
	init_debugfs(epfnv);

	return 0;

fail_atu_dma:
	pci_epf_free_space(epf, epfnv->bar0_virt, BAR_0);

	return ret;
}

static const struct pci_epf_device_id pcie_dma_epf_ids[] = {
	{
		.name = "tegra_pcie_dma_epf",
	},
	{},
};

static int pcie_dma_epf_probe(struct pci_epf *epf)
{
	struct device *dev = &epf->dev;
	struct pcie_epf_dma *epfnv;

	epfnv = devm_kzalloc(dev, sizeof(*epfnv), GFP_KERNEL);
	if (!epfnv)
		return -ENOMEM;

	epfnv->edma.ll_desc = devm_kzalloc(dev, sizeof(*epfnv->ll_desc) * NUM_EDMA_DESC,
					   GFP_KERNEL);
	if (!epfnv)
		return -ENOMEM;

	gepfnv = epfnv;
	epf_set_drvdata(epf, epfnv);

	epfnv->header.vendorid = PCI_VENDOR_ID_NVIDIA;
	epfnv->header.deviceid = 0x1AD6;
	epfnv->header.baseclass_code = PCI_BASE_CLASS_MEMORY;
	epfnv->header.interrupt_pin = PCI_INTERRUPT_INTA;
	epf->header = &epfnv->header;

	return 0;
}

static struct pci_epf_ops ops = {
	.unbind	= pcie_dma_epf_unbind,
	.bind	= pcie_dma_epf_bind,
};

static struct pci_epf_driver test_driver = {
	.driver.name	= "pcie_dma_epf",
	.probe		= pcie_dma_epf_probe,
	.id_table	= pcie_dma_epf_ids,
	.ops		= &ops,
	.owner		= THIS_MODULE,
};

static int __init pcie_dma_epf_init(void)
{
	int ret;

	ret = pci_epf_register_driver(&test_driver);
	if (ret < 0) {
		pr_err("Failed to register PCIe DMA EPF driver: %d\n", ret);
		return ret;
	}

	return 0;
}
module_init(pcie_dma_epf_init);

static void __exit pcie_dma_epf_exit(void)
{
	pci_epf_unregister_driver(&test_driver);
}
module_exit(pcie_dma_epf_exit);

MODULE_DESCRIPTION("TEGRA PCIe DMA EPF driver");
MODULE_AUTHOR("Om Prakash Singh <omp@nvidia.com>");
MODULE_LICENSE("GPL v2");
