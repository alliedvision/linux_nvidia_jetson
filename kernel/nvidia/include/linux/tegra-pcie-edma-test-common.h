/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * PCIe DMA EPF Library for Tegra PCIe
 *
 * Copyright (C) 2022 NVIDIA Corporation. All rights reserved.
 */

#ifndef TEGRA_PCIE_EDMA_TEST_COMMON_H
#define TEGRA_PCIE_EDMA_TEST_COMMON_H

#include <linux/pci-epf.h>
#include <linux/pcie_dma.h>
#include <linux/tegra-pcie-edma.h>

#define EDMA_ABORT_TEST_EN	(edma->edma_ch & 0x40000000)
#define IS_EDMA_CH_ENABLED(i)	(edma->edma_ch & ((BIT(i) << 4)))
#define IS_EDMA_CH_ASYNC(i)	(edma->edma_ch & BIT(i))
#define REMOTE_EDMA_TEST_EN	(edma->edma_ch & 0x80000000)
#define EDMA_PERF (edma->tsz / (diff / 1000))
#define EDMA_CPERF ((edma->tsz * (edma->nents / edma->nents_per_ch)) / (diff / 1000))

#define EDMA_PRIV_CH_OFF	16
#define EDMA_PRIV_LR_OFF	20
#define EDMA_PRIV_XF_OFF	21

struct edmalib_common {
	struct device *fdev;
	void *bar0_virt;
	void *src_virt;
	void __iomem *dma_base;
	u32 dma_size;
	dma_addr_t src_dma_addr;
	dma_addr_t dst_dma_addr;
	dma_addr_t bar0_phy;
	u32 stress_count;
	void *cookie;
	struct device_node *of_node;
	wait_queue_head_t wr_wq[DMA_WR_CHNL_NUM];
	wait_queue_head_t rd_wq[DMA_RD_CHNL_NUM];
	unsigned long wr_busy;
	unsigned long rd_busy;
	ktime_t edma_start_time[DMA_WR_CHNL_NUM];
	u64 tsz;
	u32 edma_ch;
	u32 prev_edma_ch;
	u32 nents;
	struct tegra_pcie_edma_desc *ll_desc;
	int priv_iter[DMA_WR_CHNL_NUM];
	struct pcie_tegra_edma_remote_info edma_remote;
	u32 nents_per_ch;
	u32 st_as_ch;
	u32 ls_as_ch;
};

static struct edmalib_common *l_edma;

static void edma_final_complete(void *priv, edma_xfer_status_t status,
				struct tegra_pcie_edma_desc *desc)
{
	struct edmalib_common *edma = l_edma;
	int cb = *(int *)priv;
	u32 ch = (cb >> EDMA_PRIV_CH_OFF) & 0xF;
	edma_xfer_type_t xfer_type = (cb >> EDMA_PRIV_XF_OFF) & 0x1;
	char *xfer_str[2] = {"WR", "RD"};
	u32 l_r = (cb >> EDMA_PRIV_LR_OFF) & 0x1;
	char *l_r_str[2] = {"local", "remote"};
	u64 diff = ktime_to_ns(ktime_get()) - ktime_to_ns(edma->edma_start_time[ch]);
	u64 cdiff = ktime_to_ns(ktime_get()) - ktime_to_ns(edma->edma_start_time[edma->st_as_ch]);

	cb = cb & 0xFFFF;
	if (EDMA_ABORT_TEST_EN && status == EDMA_XFER_SUCCESS)
		dma_common_wr(edma->dma_base, DMA_WRITE_DOORBELL_OFF_WR_STOP | (ch + 1),
			      DMA_WRITE_DOORBELL_OFF);

	dev_info(edma->fdev, "%s: %s-%s-Async complete for chan %d with status %d. Total desc %d of Sz %d Bytes done in time %llu nsec. Perf is %llu Mbps\n",
		 __func__, xfer_str[xfer_type], l_r_str[l_r], ch, status, edma->nents_per_ch*(cb+1),
		 edma->dma_size, diff, EDMA_PERF);

	if (ch == edma->ls_as_ch)
		dev_info(edma->fdev, "%s: All Async channels. Cumulative Perf %llu Mbps, time %llu nsec\n",
			 __func__, EDMA_CPERF, cdiff);
}

static void edma_complete(void *priv, edma_xfer_status_t status, struct tegra_pcie_edma_desc *desc)
{
	struct edmalib_common *edma = l_edma;
	int cb = *(int *)priv;
	u32 ch = (cb >> 16);

	if (BIT(ch) & edma->wr_busy) {
		edma->wr_busy &= ~(BIT(ch));
		wake_up(&edma->wr_wq[ch]);
	}

	if (status == 0)
		dev_dbg(edma->fdev, "%s: status %d, cb %d\n", __func__, status, cb);
}

/* debugfs to perform eDMA lib transfers and do CRC check */
static int edmalib_common_test(struct edmalib_common *edma)
{
	struct tegra_pcie_edma_desc *ll_desc = edma->ll_desc;
	dma_addr_t src_dma_addr = edma->src_dma_addr;
	dma_addr_t dst_dma_addr = edma->dst_dma_addr;
	u32 nents = edma->nents, num_chans = 0, nents_per_ch = 0, nent_id = 0, chan_count;
	u32 i, j, k, max_size, db_off, num_descriptors;
	edma_xfer_status_t ret;
	struct tegra_pcie_edma_init_info info = {};
	struct tegra_pcie_edma_chans_info *chan_info;
	struct tegra_pcie_edma_xfer_info tx_info = {};
	u64 diff;
	edma_xfer_type_t xfer_type;
	char *xfer_str[2] = {"WR", "RD"};
	u32 l_r;
	char *l_r_str[2] = {"local", "remote"};

	if (!edma->stress_count) {
		tegra_pcie_edma_deinit(edma->cookie);
		edma->cookie = NULL;
		return 0;
	}

	l_edma = edma;

	if (EDMA_ABORT_TEST_EN) {
		edma->edma_ch &= ~0xFF;
		/* only channel 0, 2 is ASYNC, where chan 0 async gets aborted */
		edma->edma_ch |= 0xF5;
	}

	if (edma->cookie && edma->prev_edma_ch != edma->edma_ch) {
		edma->st_as_ch = -1;
		dev_info(edma->fdev, "edma_ch changed from 0x%x != 0x%x, deinit\n",
			 edma->prev_edma_ch, edma->edma_ch);
		tegra_pcie_edma_deinit(edma->cookie);
		edma->cookie = NULL;
	}

	info.np = edma->of_node;

	if (REMOTE_EDMA_TEST_EN) {
		num_descriptors = 1024;
		info.rx[0].desc_phy_base = edma->bar0_phy + SZ_512K;
		info.rx[0].desc_iova = 0xf0000000 + SZ_512K;
		info.rx[1].desc_phy_base = edma->bar0_phy + SZ_512K + SZ_256K;
		info.rx[1].desc_iova = 0xf0000000 + SZ_512K + SZ_256K;
		info.edma_remote = &edma->edma_remote;
		chan_count = DMA_RD_CHNL_NUM;
		chan_info = &info.rx[0];
		xfer_type = EDMA_XFER_READ;
		db_off = DMA_WRITE_DOORBELL_OFF;
		l_r = 1;
	} else {
		chan_count = DMA_WR_CHNL_NUM;
		num_descriptors = 4096;
		chan_info = &info.tx[0];
		xfer_type = EDMA_XFER_WRITE;
		db_off = DMA_READ_DOORBELL_OFF;
		l_r = 0;
	}

	for (i = 0; i < chan_count; i++) {
		struct tegra_pcie_edma_chans_info *ch = chan_info + i;

		ch->ch_type = IS_EDMA_CH_ASYNC(i) ? EDMA_CHAN_XFER_ASYNC :
							   EDMA_CHAN_XFER_SYNC;
		if (IS_EDMA_CH_ENABLED(i)) {
			if (edma->st_as_ch == -1)
				edma->st_as_ch = i;
			edma->ls_as_ch = i;
			ch->num_descriptors = num_descriptors;
			num_chans++;
		} else
			ch->num_descriptors = 0;
	}

	max_size = (BAR0_DMA_BUF_SIZE - BAR0_DMA_BUF_OFFSET) / 2;
	if (((edma->dma_size * nents) > max_size) || (nents > NUM_EDMA_DESC)) {
		dev_err(edma->fdev, "%s: max dma size including all nents(%d), max_nents(%d), dma_size(%d) should be <= 0x%x\n",
			__func__, nents, NUM_EDMA_DESC, edma->dma_size, max_size);
		return 0;
	}

	nents_per_ch = nents / num_chans;
	if (nents_per_ch == 0) {
		dev_err(edma->fdev, "%s: nents(%d) < enabled chanes(%d)\n",
			__func__, nents, num_chans);
		return 0;
	}

	for (j = 0; j < nents; j++) {
		ll_desc->src = src_dma_addr + (j * edma->dma_size);
		ll_desc->dst = dst_dma_addr + (j * edma->dma_size);
		dev_dbg(edma->fdev, "src %llx, dst %llx at %d\n", ll_desc->src, ll_desc->dst, j);
		ll_desc->sz = edma->dma_size;
		ll_desc++;
	}
	ll_desc = edma->ll_desc;

	edma->tsz = (u64)edma->stress_count * (nents_per_ch) * (u64)edma->dma_size * 8UL;

	if (!edma->cookie && (edma->prev_edma_ch != edma->edma_ch)) {
		dev_info(edma->fdev, "%s: re-init edma lib prev_ch(%x) != current chans(%x)\n",
			 __func__, edma->prev_edma_ch, edma->edma_ch);
		edma->cookie = tegra_pcie_edma_initialize(&info);
		edma->prev_edma_ch = edma->edma_ch;
	}

	edma->nents_per_ch = nents_per_ch;

	/* generate random bytes to transfer */
	get_random_bytes(edma->src_virt, edma->dma_size * nents_per_ch);
	dev_info(edma->fdev, "%s: EDMA LIB %s started for %d chans, size %d Bytes, iterations: %d of descriptors %d\n",
		 __func__, xfer_str[xfer_type], num_chans, edma->dma_size, edma->stress_count,
		 nents_per_ch);

	/* LL DMA with size epfnv->dma_size per desc */
	for (i = 0; i < chan_count; i++) {
		int ch = i;
		struct tegra_pcie_edma_chans_info *ch_info = chan_info + i;

		if (ch_info->num_descriptors == 0)
			continue;

		edma->edma_start_time[i] = ktime_get();
		tx_info.desc = &ll_desc[nent_id++ * nents_per_ch];
		for (k = 0; k < edma->stress_count; k++) {
			tx_info.channel_num = ch;
			tx_info.type = xfer_type;
			tx_info.nents = nents_per_ch;
			if (ch_info->ch_type == EDMA_CHAN_XFER_ASYNC) {
				if (k == edma->stress_count - 1)
					tx_info.complete = edma_final_complete;
				else
					tx_info.complete = edma_complete;
			}
			edma->priv_iter[ch] = k | (xfer_type << EDMA_PRIV_XF_OFF) |
					      (l_r << EDMA_PRIV_LR_OFF) | (ch << EDMA_PRIV_CH_OFF);
			tx_info.priv = &edma->priv_iter[ch];
			ret = tegra_pcie_edma_submit_xfer(edma->cookie, &tx_info);
			if (ret == EDMA_XFER_FAIL_NOMEM) {
				/** Retry after 20 msec */
				dev_dbg(edma->fdev, "%s: EDMA_XFER_FAIL_NOMEM stress count %d on channel %d iter %d\n",
					__func__, edma->stress_count, i, k);
				ret = wait_event_timeout(edma->wr_wq[i],
							 !(edma->wr_busy & (1 << i)),
							 msecs_to_jiffies(500));
				/* Do a more sleep to avoid repeated wait and wake calls */
				msleep(100);
				if (ret == 0) {
					dev_err(edma->fdev, "%s: %d timedout\n", __func__, i);
					ret = -ETIMEDOUT;
					goto fail;
				}
				k--;
				continue;
			} else if ((ret != EDMA_XFER_SUCCESS) && (ret != EDMA_XFER_FAIL_NOMEM)) {
				dev_err(edma->fdev, "%s: LL %d, SZ: %u B CH: %d failed. %d at iter %d ret: %d\n",
					__func__, xfer_type, edma->dma_size, ch, ret, k, ret);
				if (EDMA_ABORT_TEST_EN) {
					msleep(5000);
					break;
				}
				goto fail;
			}
			dev_dbg(edma->fdev, "%s: LL EDMA LIB %d, SZ: %u B CH: %d iter %d\n",
				__func__, xfer_type, edma->dma_size, ch, i);
		}
		if (EDMA_ABORT_TEST_EN && i == 0) {
			msleep(edma->stress_count);
			dma_common_wr(edma->dma_base, DMA_WRITE_DOORBELL_OFF_WR_STOP,
				      db_off);
		}
		diff = ktime_to_ns(ktime_get()) - ktime_to_ns(edma->edma_start_time[i]);
		if (ch_info->ch_type == EDMA_CHAN_XFER_SYNC)
			dev_info(edma->fdev, "%s: EDMA LIB %s-%s-SYNC done for %d iter on channel %d. Total Size %llu bytes, time %llu nsec. Perf is %llu Mbps\n",
				 __func__, xfer_str[xfer_type], l_r_str[l_r], edma->stress_count, i,
				 edma->tsz, diff, EDMA_PERF);
	}
	dev_info(edma->fdev, "%s: EDMA LIB submit done\n", __func__);

	return 0;
fail:
	if (ret != EDMA_XFER_DEINIT) {
		tegra_pcie_edma_deinit(edma->cookie);
		edma->cookie = NULL;
	}
	return -1;
}

#endif /* TEGRA_PCIE_EDMA_TEST_COMMON_H */
