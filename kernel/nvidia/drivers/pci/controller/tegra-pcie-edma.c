// SPDX-License-Identifier: GPL-2.0+
/*
 * PCIe EDMA Library Framework
 *
 * Copyright (C) 2021 NVIDIA Corporation. All rights reserved.
 */

#include <linux/dma-iommu.h>
#include <linux/io.h>
#include <linux/of_platform.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/tegra-pcie-edma.h>
#include "tegra-pcie-dma-osi.h"

/** Default number of descriptors used */
#define NUM_EDMA_DESC	4096

/* DMA base offset starts at 0x20000 from ATU_DMA base */
#define DMA_OFFSET	0x20000

/** Calculates timeout based on size.
 *  Time in nano sec = size in bytes / (1000000 * 2).
 *    2Gbps is max speed for Gen 1 with 2.5GT/s at 8/10 encoding.
 *  Convert to milli seconds and add 1sec timeout
 */
/** TODO: How to handle overflow */
#define GET_SYNC_TIMEOUT(s)	((((s) * 8)/2000000) + 1000)

#define INCR_DESC(idx, i) ((idx) = ((idx) + (i)) % (ch->desc_sz))

struct edma_chan {
	void *desc;
	struct tegra_pcie_edma_xfer_info *ring;
	dma_addr_t dma_iova;
	uint32_t desc_sz;
	/** Index from where cleanup needs to be done */
	volatile uint32_t r_idx;
	/** Index from where descriptor update is needed */
	volatile uint32_t w_idx;
	struct mutex lock;
	wait_queue_head_t wq;
	edma_chan_type_t type;
	u64 wcount;
	u64 rcount;
	bool pcs;
	bool db_pos;
};

struct edma_prv {
	void __iomem *edma_desc;
	u32 edma_desc_size;
	int irq;
	char *irq_name;
	bool is_remote_dma;
	/** EDMA base address */
	void *edma_base;
	/** EDMA base address size */
	uint32_t edma_base_size;
	struct platform_device *pdev;
	struct device *dev;
	unsigned long wr_busy;
	unsigned long rd_busy;
	struct edma_chan tx[DMA_WR_CHNL_NUM];
	struct edma_chan rx[DMA_WR_CHNL_NUM];
};

/** TODO: Define osi_ll_init strcuture and make this as OSI */
static inline void edma_ll_ch_init(void *edma_base, uint32_t ch,
					 dma_addr_t ll_phy_addr, bool rw_type,
					 bool is_remote_dma)
{
	uint32_t int_mask_val = OSI_BIT(ch);
	uint32_t val;
	/** configure write by default and overwrite if read */
	uint32_t int_mask = DMA_WRITE_INT_MASK_OFF;
	uint32_t ctrl1_offset = DMA_CH_CONTROL1_OFF_WRCH;
	uint32_t low_offset = DMA_LLP_LOW_OFF_WRCH;
	uint32_t high_offset = DMA_LLP_HIGH_OFF_WRCH;
	uint32_t lle_ccs = DMA_CH_CONTROL1_OFF_WRCH_LIE | DMA_CH_CONTROL1_OFF_WRCH_LLE |
			   DMA_CH_CONTROL1_OFF_WRCH_CCS;
	uint32_t rie = DMA_CH_CONTROL1_OFF_WRCH_RIE;

	if (!rw_type) {
		int_mask = DMA_READ_INT_MASK_OFF;
		low_offset = DMA_LLP_LOW_OFF_RDCH;
		high_offset = DMA_LLP_HIGH_OFF_RDCH;
		ctrl1_offset = DMA_CH_CONTROL1_OFF_RDCH;
		lle_ccs = DMA_CH_CONTROL1_OFF_RDCH_LIE | DMA_CH_CONTROL1_OFF_RDCH_LLE |
			   DMA_CH_CONTROL1_OFF_RDCH_CCS;
		rie = DMA_CH_CONTROL1_OFF_RDCH_RIE;
	}
	/* Enable LIE or RIE for all write channels */
	val = dma_common_rd(edma_base, int_mask);
	if (!is_remote_dma) {
		val &= ~int_mask_val;
		val &= ~(int_mask_val << 16);
	} else {
		val |= int_mask_val;
		val |= (int_mask_val << 16);
	}
	dma_common_wr(edma_base, val, int_mask);

	val = lle_ccs;
	/* Enable RIE for remote DMA */
	val |= (is_remote_dma ? rie : 0);
	dma_channel_wr(edma_base, ch, val, ctrl1_offset);
	dma_channel_wr(edma_base, ch, lower_32_bits(ll_phy_addr), low_offset);
	dma_channel_wr(edma_base, ch, upper_32_bits(ll_phy_addr), high_offset);
}

static inline void edma_hw_init(void *cookie)
{
	struct edma_prv *prv = (struct edma_prv *)cookie;
	int i;

	for (i = 0; i < DMA_WR_CHNL_NUM; i++)
		edma_ll_ch_init(prv->edma_base, i, prv->tx[i].dma_iova, true,
				prv->is_remote_dma);

	dma_common_wr(prv->edma_base, WRITE_ENABLE, DMA_WRITE_ENGINE_EN_OFF);

#ifdef EDMA_RX_SUPPORT
	for (i = 0; i < DMA_RD_CHNL_NUM; i++)
		edma_ll_ch_init(prv->edma_base, i, prv->rx[i].dma_iova, false,
				prv->is_remote_dma);

	dma_common_wr(prv->edma_base, READ_ENABLE, DMA_READ_ENGINE_EN_OFF);
#endif
}

static inline void edma_hw_deinit(void *cookie)
{
	struct edma_prv *prv = (struct edma_prv *)cookie;
	int i;

	for (i = 0; i < DMA_WR_CHNL_NUM; i++)
		dma_channel_wr(prv->edma_base, i, 0, DMA_CH_CONTROL1_OFF_WRCH);

	dma_common_wr(prv->edma_base, WRITE_DISABLE, DMA_WRITE_ENGINE_EN_OFF);

#ifdef EDMA_RX_SUPPORT
	for (i = 0; i < DMA_RD_CHNL_NUM; i++)
		dma_channel_wr(prv->edma_base, i, 0, DMA_CH_CONTROL1_OFF_RDCH);

	dma_common_wr(prv->edma_base, READ_DISABLE, DMA_READ_ENGINE_EN_OFF);
#endif
}

/** From OSI */
static inline u32 get_dma_idx_from_llp(struct edma_prv *prv, u32 chan)
{
	u64 cur_iova;
	u32 cur_idx;
	struct edma_chan *ch = &prv->tx[chan];
	u64 block_idx, idx_in_block;

	/*
	 * Read current element address in DMA_LLP register which pending
	 * DMA request.
	 */
	cur_iova = dma_channel_rd(prv->edma_base, chan, DMA_LLP_HIGH_OFF_WRCH);
	cur_iova = (cur_iova << 32);
	cur_iova |= dma_channel_rd(prv->edma_base, chan, DMA_LLP_LOW_OFF_WRCH);
	/* Compute DMA desc index */
	/* TODO: Add checks for index spill over */
	block_idx = ((cur_iova - ch->dma_iova) / sizeof(struct edma_dblock));
	idx_in_block = (cur_iova & (sizeof(struct edma_dblock) - 1)) /
		       sizeof(struct edma_hw_desc);

	cur_idx = (block_idx * 2) + idx_in_block;

	return cur_idx % (ch->desc_sz);
}

static irqreturn_t edma_irq_handler(int irq, void *cookie)
{
	struct edma_prv *prv = (struct edma_prv *)cookie;
	int bit = 0;
	u32 val, idx;
	struct tegra_pcie_edma_xfer_info *ring;
	struct edma_hw_desc *dma_ll_virt;
	struct edma_dblock *db;

	/* TODO: Add Abort handling also here */
	val = dma_common_rd(prv->edma_base, DMA_WRITE_INT_STATUS_OFF);
	for (bit = 0; bit < DMA_WR_CHNL_NUM; bit++) {
		if (BIT(bit) & val) {
			/* TODO: schedule a work queue for processing */
			struct edma_chan *ch = &prv->tx[bit];

			dma_common_wr(prv->edma_base, BIT(bit),
				      DMA_WRITE_INT_CLEAR_OFF);

			if (ch->type == EDMA_CHAN_XFER_SYNC) {
				if (prv->wr_busy & BIT(bit)) {
					prv->wr_busy &= ~(BIT(bit));
					wake_up(&ch->wq);
				} else {
					dev_err(&prv->pdev->dev, "SYNC mode with wr_busy not set.\n");
				}
			}

			idx = get_dma_idx_from_llp(prv, bit);
			/* TODO add an another pre-defined exit condition */
			while (ch->r_idx != idx) {
				ring = &ch->ring[ch->r_idx];
				db = (struct edma_dblock *)ch->desc + ch->r_idx/2;
				dma_ll_virt = &db->desc[ch->r_idx % 2];
				INCR_DESC(ch->r_idx, 1);
				ch->rcount++;
				if (ch->type == EDMA_CHAN_XFER_ASYNC && ring->complete) {
					ring->complete(ring->priv, EDMA_XFER_SUCCESS, NULL);
					/* Clear ring callback and lie,rie */
					ring->complete = NULL;
					dma_ll_virt->ctrl_reg.ctrl_e.lie = 0;
					dma_ll_virt->ctrl_reg.ctrl_e.rie = 0;
				}
			}
		}
	}

	return IRQ_HANDLED;
}

void *tegra_pcie_edma_initialize(struct tegra_pcie_edma_init_info *info)
{
	struct edma_prv *prv;
	struct resource *dma_res;
	int32_t ret, i, j;
	struct edma_chan *ch = NULL;
	struct edma_dblock *db;
	dma_addr_t addr;

	prv = kzalloc(sizeof(*prv), GFP_KERNEL);
	if (!prv) {
		pr_err("Failed to allocate memory for edma_prv\n");
		return NULL;
	}

	if (info->edma_remote != NULL) {
		pr_err("remote_edma mode not supported yet\n");
		goto free_priv;
	} else if (info->np != NULL) {
		prv->is_remote_dma = false;

		prv->pdev = of_find_device_by_node(info->np);
		if (prv->pdev == NULL) {
			pr_err("Unable to retrieve pdev node\n");
			goto free_priv;
		}

		dma_res = platform_get_resource_byname(prv->pdev, IORESOURCE_MEM,
						       "atu_dma");
		if (!dma_res) {
			dev_err(&prv->pdev->dev, "missing atu_dma resource in DT\n");
			goto free_priv;
		}

		prv->edma_base = devm_ioremap(&prv->pdev->dev, dma_res->start + DMA_OFFSET,
				       resource_size(dma_res) - DMA_OFFSET);
		if (IS_ERR(prv->edma_base)) {
			dev_err(&prv->pdev->dev, "dma region map failed.\n");
			goto free_priv;
		}

		for (i = 0; i < DMA_WR_CHNL_NUM; i++) {
			ch = &prv->tx[i];
			ch->type = info->tx[i].ch_type;
			ch->desc_sz = (info->tx[i].num_descriptors == 0) ?
				NUM_EDMA_DESC : info->tx[i].num_descriptors;
			ch->desc = dma_alloc_coherent(&prv->pdev->dev,
			(sizeof(struct edma_dblock)) * ((ch->desc_sz/2) + 1),
			&ch->dma_iova, GFP_KERNEL);
			if (!ch->desc) {
				dev_err(&prv->pdev->dev, "Cannot allocate required tx descriptos(%d) of size (%lu) for channel:%d\n",
					prv->tx[i].desc_sz,
					(sizeof(struct edma_hw_desc) *
					prv->tx[i].desc_sz), i);
				goto dma_iounmap;
			}

			memset(ch->desc, 0, (sizeof(struct edma_dblock)) *
					    ((ch->desc_sz/2) + 1));

			ch->ring = kcalloc(ch->desc_sz, sizeof(*(ch->ring)), GFP_KERNEL);
			if (!ch->ring)
				dev_err(&prv->pdev->dev, "Failed to allocate %d channel ring\n",
					i);

			db = (struct edma_dblock *)ch->desc + ((ch->desc_sz/2) - 1);
			db->llp.sar_low = lower_32_bits(ch->dma_iova);
			db->llp.sar_high = upper_32_bits(ch->dma_iova);
			db->llp.ctrl_reg.ctrl_e.llp = 1;
			db->llp.ctrl_reg.ctrl_e.tcb = 1;
			for (j = 0; j < (ch->desc_sz/2 - 1); j++) {
				db = (struct edma_dblock *)ch->desc + j;
				addr = ch->dma_iova + (sizeof(struct edma_dblock) * (j+1));
				db->llp.sar_low = lower_32_bits(addr);
				db->llp.sar_high = upper_32_bits(addr);
				db->llp.ctrl_reg.ctrl_e.llp = 1;
			}
			ch->pcs = 1;
		}

		/* TODO RX descriptor creation */

		prv->irq = platform_get_irq_byname(prv->pdev, "intr");
		if (!prv->irq) {
			dev_err(&prv->pdev->dev, "failed to get intr interrupt\n");
			goto free_dma_desc;
		};

		prv->irq_name = kasprintf(GFP_KERNEL, "%s_edma_lib", prv->pdev->name);
		if (!prv->irq_name)
			goto free_dma_desc;


		ret = request_threaded_irq(prv->irq, NULL, edma_irq_handler,
					   IRQF_SHARED | IRQF_ONESHOT,
					   prv->irq_name, prv);
		if (ret < 0) {
			dev_err(&prv->pdev->dev, "failed to request \"intr\" irq\n");
			goto free_irq_name;
		}
	} else {
		pr_err("Neither device node nor edma remote available");
		goto free_priv;
	}

	for (i = 0; i < DMA_WR_CHNL_NUM; i++) {
		mutex_init(&prv->tx[i].lock);
		init_waitqueue_head(&prv->tx[i].wq);
	}

#ifdef EDMA_RX_SUPPORT
	for (i = 0; i < DMA_RD_CHNL_NUM; i++) {
		mutex_init(&prv->rx[i].lock);
		init_waitqueue_head(&prv->rx[i].wq);
	}
#endif

	edma_hw_init(prv);
	dev_info(&prv->pdev->dev, "%s: success", __func__);
	return prv;
free_irq_name:
	kfree(prv->irq_name);
free_dma_desc:
	for (i = 0; i < DMA_WR_CHNL_NUM; i++) {
		if (prv->tx[i].desc)
			dma_free_coherent(&prv->pdev->dev,
			(sizeof(struct edma_hw_desc) * prv->tx[i].desc_sz),
			prv->tx[i].desc, prv->tx[i].dma_iova);
	}
dma_iounmap:
	iounmap(prv->edma_base);
free_priv:
	kfree(prv);
	return NULL;
}
EXPORT_SYMBOL(tegra_pcie_edma_initialize);

edma_xfer_status_t tegra_pcie_edma_submit_xfer(void *cookie,
						struct tegra_pcie_edma_xfer_info *tx_info)
{
	struct edma_prv *prv = (struct edma_prv *)cookie;
	struct edma_chan *ch;
	struct edma_hw_desc *dma_ll_virt;
	struct edma_dblock *db;
	int i, ret;
	u64 total_sz = 0;
	edma_xfer_status_t st = EDMA_XFER_SUCCESS;
	u32 avail;
	struct tegra_pcie_edma_xfer_info *ring;

	if (prv == NULL || tx_info == NULL ||
	    ((tx_info->channel_num >= DMA_WR_CHNL_NUM) &&
	     (tx_info->type == EDMA_XFER_WRITE)) ||
	    (tx_info->nents == 0) || (tx_info->desc == NULL) ||
	    (tx_info->type == EDMA_XFER_READ))
		return EDMA_XFER_FAIL_INVAL_INPUTS;

	ch = (tx_info->type == EDMA_XFER_WRITE) ? &prv->tx[tx_info->channel_num] :
						  &prv->rx[tx_info->channel_num];

	if ((tx_info->complete == NULL) && (ch->type == EDMA_CHAN_XFER_ASYNC))
		return EDMA_XFER_FAIL_INVAL_INPUTS;

	/* Get hold of the hardware - locking */
	mutex_lock(&ch->lock);

	avail = (ch->r_idx - ch->w_idx - 1U) & (ch->desc_sz - 1U);
	if (tx_info->nents > avail) {
		dev_err(&prv->pdev->dev, "Descriptors full. w_idx %d. r_idx %d, avail %d, req %d\n",
			ch->w_idx, ch->r_idx, avail, tx_info->nents);
		st = EDMA_XFER_FAIL_NOMEM;
		goto unlock;
	}

	prv->wr_busy |= 1 << tx_info->channel_num;

	dev_dbg(&prv->pdev->dev, "xmit for %d nents at %d widx and %d ridx\n",
		 tx_info->nents, ch->w_idx, ch->r_idx);
	db = (struct edma_dblock *)ch->desc + (ch->w_idx/2);
	for (i = 0; i < tx_info->nents; i++) {
		dma_ll_virt = &db->desc[ch->db_pos];
		dma_ll_virt->size = tx_info->desc[i].sz;
		/* calculate number of packets and add those many headers */
		total_sz += ((tx_info->desc[i].sz/4096) + 1)*30;
		total_sz += tx_info->desc[i].sz;
		dma_ll_virt->sar_low = lower_32_bits(tx_info->desc[i].src);
		dma_ll_virt->sar_high = upper_32_bits(tx_info->desc[i].src);
		dma_ll_virt->dar_low = lower_32_bits(tx_info->desc[i].dst);
		dma_ll_virt->dar_high = upper_32_bits(tx_info->desc[i].dst);
		dma_ll_virt->ctrl_reg.ctrl_e.cb = ch->pcs;
		ch->db_pos = !ch->db_pos;
		avail = ch->w_idx;
		ch->w_idx++;
		if (!ch->db_pos) {
			ch->wcount = 0;
			db->llp.ctrl_reg.ctrl_e.cb = ch->pcs;
			if (ch->w_idx == ch->desc_sz) {
				ch->pcs = !ch->pcs;
				db->llp.ctrl_reg.ctrl_e.cb = ch->pcs;
				dev_dbg(&prv->pdev->dev, "Toggled pcs at w_idx %d\n", ch->w_idx);
				ch->w_idx = 0;
			}
			db = (struct edma_dblock *)ch->desc + (ch->w_idx/2);
		}
	}

	ring = &ch->ring[avail];
	ring->complete = tx_info->complete;
	ring->priv = tx_info->priv;
	ring->nents = tx_info->nents;
	ring->desc = tx_info->desc;

	/* Set LIE or RIE in last element */
	dma_ll_virt->ctrl_reg.ctrl_e.lie = 1;
	if (prv->is_remote_dma)
		dma_ll_virt->ctrl_reg.ctrl_e.rie = 1;

	dma_common_wr(prv->edma_base, tx_info->channel_num, DMA_WRITE_DOORBELL_OFF);

	if (ch->type == EDMA_CHAN_XFER_SYNC) {
		ret = wait_event_timeout(ch->wq,
				!(prv->wr_busy & (1 << tx_info->channel_num)),
				msecs_to_jiffies(GET_SYNC_TIMEOUT(total_sz)));
		if (ret == 0) {
			dev_err(&prv->pdev->dev,
				"%s: timeout at %d ch, w_idx(%d), r_idx(%d)\n",
				__func__, tx_info->channel_num, ch->w_idx,
				ch->r_idx);
			dev_err(&prv->pdev->dev, "int status %x",  dma_common_rd(prv->edma_base,
				DMA_WRITE_INT_STATUS_OFF));
			st = EDMA_XFER_FAIL_TIMEOUT;
			goto unlock;
		}
		dev_dbg(&prv->pdev->dev, "xmit done for %d nents at %d widx and %d ridx\n",
			tx_info->nents, ch->w_idx, ch->r_idx);
	}
unlock:
	/* release hardware - unlocking */
	mutex_unlock(&ch->lock);

	return st;
}
EXPORT_SYMBOL(tegra_pcie_edma_submit_xfer);

void tegra_pcie_edma_deinit(void *cookie)
{
	struct edma_prv *prv = (struct edma_prv *)cookie;
	int i;

	if (cookie == NULL)
		return;

	edma_hw_deinit(cookie);

	free_irq(prv->irq, prv);
	kfree(prv->irq_name);

	for (i = 0; i < DMA_WR_CHNL_NUM; i++) {
		if (prv->tx[i].desc)
			dma_free_coherent(&prv->pdev->dev,
			(sizeof(struct edma_hw_desc) * prv->tx[i].desc_sz),
			prv->tx[i].desc, prv->tx[i].dma_iova);
	}

	iounmap(prv->edma_base);
	kfree(prv);
}
EXPORT_SYMBOL(tegra_pcie_edma_deinit);
