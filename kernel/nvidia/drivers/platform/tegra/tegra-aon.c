/*
 * Copyright (c) 2015-2020, NVIDIA CORPORATION.  All rights reserved.
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

#include <linux/version.h>

#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/tegra-hsp.h>
#include <linux/mailbox_controller.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_reserved_mem.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/tegra-aon.h>
#include <linux/tegra-ivc.h>
#include <linux/tegra-ivc-instance.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0)
#include <linux/mailbox_client.h>
#include <linux/completion.h>
#else
#include <linux/tegra-hsp.h>
#endif

#include <asm/cache.h>

/* This has to be a multiple of the cache line size */
static inline int ivc_min_frame_size(void)
{
	return cache_line_size();
}

#define TEGRA_AON_HSP_DATA_ARRAY_SIZE	3

#define IPCBUF_SIZE 2097152

#define SMBOX_IVC_NOTIFY_MASK	0xFFFF

#define SHRD_SEM_OFFSET	0x10000
#define SHRD_SEM_SET	0x4u
#define SHRD_SEM_CLR	0x8u
#define AON_SS_MAX	8

#define IVC_INIT_TIMEOUT_US (200000)

enum smbox_msgs {
	SMBOX_IVC_READY_MSG = 0xAAAA5555,
	SMBOX_IVC_DBG_ENABLE = 0xAAAA6666,
	SMBOX_IVC_NOTIFY = 0x0000AABB,
};

enum ivc_tasks_dbg_enable {
	IVC_TASKS_GLOBAL_DBG_ENABLE = 0,
	IVC_ECHO_TASK_DBG_ENABLE = 1,
	IVC_DBG_TASK_DBG_ENABLE = 2,
	IVC_SPI_TASK_DBG_ENABLE = 3,
	IVC_TASK_ENABLE_MAX = 4,
	IVC_TASKS_MAX = 31,
	IVC_TASKS_DBG_ENABLE_BIT = 31,
};

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0)
struct aon_hsp_sm {
	struct mbox_client client;
	struct mbox_chan *chan;
};

struct aon_hsp {
	struct aon_hsp_sm rx;
	struct aon_hsp_sm tx;
	struct device dev;
	struct completion emptied;
};
#else
struct aon_hsp {
	struct tegra_hsp_sm_rx *rx;
	struct tegra_hsp_sm_tx *tx;
	struct device dev;
};
#endif

struct tegra_aon {
	struct mbox_controller mbox;
	struct aon_hsp *hsp;
	void __iomem *ss_base;
	void *ipcbuf;
	dma_addr_t ipcbuf_dma;
	size_t ipcbuf_size;
	u32 ivc_carveout_base_ss;
	u32 ivc_carveout_size_ss;
	u32 ivc_dbg_enable_ss;
	u32 ivc_tx_ss;
	u32 ivc_rx_ss;
};

struct tegra_aon_ivc_chan {
	struct ivc ivc;
	char *name;
	int chan_id;
	struct tegra_aon *aon;
	bool last_tx_done;
};

static void __iomem *tegra_aon_hsp_ss_reg(const struct tegra_aon *aon, u32 ss)
{
	return aon->ss_base + (SHRD_SEM_OFFSET * ss);
}

static u32 tegra_aon_hsp_ss_status(const struct tegra_aon *aon, u32 ss)
{
	void __iomem *reg;

	WARN_ON(ss >= AON_SS_MAX);
	reg = tegra_aon_hsp_ss_reg(aon, ss);

	return readl(reg);
}

static void tegra_aon_hsp_ss_set(const struct tegra_aon *aon, u32 ss, u32 bits)
{
	void __iomem *reg;

	WARN_ON(ss >= AON_SS_MAX);
	reg = tegra_aon_hsp_ss_reg(aon, ss);

	writel(bits, reg + SHRD_SEM_SET);
}

static void tegra_aon_hsp_ss_clr(const struct tegra_aon *aon, u32 ss, u32 bits)
{
	void __iomem *reg;

	WARN_ON(ss >= AON_SS_MAX);
	reg = tegra_aon_hsp_ss_reg(aon, ss);

	writel(bits, reg + SHRD_SEM_CLR);
}

static void tegra_aon_rx_handler(struct tegra_aon *aon, u32 ivc_chans)
{
	struct mbox_chan *mbox_chan;
	struct ivc *ivc;
	struct tegra_aon_ivc_chan *ivc_chan;
	struct tegra_aon_mbox_msg msg;
	int i;

	ivc_chans &= BIT(aon->mbox.num_chans) - 1;
	while (ivc_chans) {
		i = __builtin_ctz(ivc_chans);
		ivc_chans &= ~BIT(i);
		mbox_chan = &aon->mbox.chans[i];
		ivc_chan = (struct tegra_aon_ivc_chan *)mbox_chan->con_priv;
		/* check if mailbox client exists */
		if (ivc_chan->chan_id == -1)
			continue;
		ivc = &ivc_chan->ivc;
		while (tegra_ivc_can_read(ivc)) {
			msg.data = tegra_ivc_read_get_next_frame(ivc);
			msg.length = ivc->frame_size;
			mbox_chan_received_data(mbox_chan, &msg);
			tegra_ivc_read_advance(ivc);
		}
	}
}

static void tegra_aon_hsp_sm_full_notify(void *data, u32 value)
{
	struct tegra_aon *aon = data;
	u32 ss_val;

	if (value != SMBOX_IVC_NOTIFY) {
		dev_err(aon->mbox.dev, "Invalid IVC notification\n");
		return;
	}

	ss_val = tegra_aon_hsp_ss_status(aon, aon->ivc_rx_ss);
	tegra_aon_hsp_ss_clr(aon, aon->ivc_rx_ss, ss_val);
	tegra_aon_rx_handler(aon, ss_val);
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0)
static int tegra_aon_hsp_sm_tx_write(struct aon_hsp *aonhsp, u32 value)
{
	return mbox_send_message(aonhsp->tx.chan,
				 (void *) (unsigned long) value);
}

static bool tegra_aon_hsp_sm_tx_is_empty(struct aon_hsp *aonhsp)
{
	return try_wait_for_completion(&aonhsp->emptied);
}

static void aon_hsp_rx_full_notify(struct mbox_client *cl, void *data)
{
	struct aon_hsp *aonhsp = dev_get_drvdata(cl->dev);
	struct tegra_aon *aon = dev_get_drvdata(aonhsp->dev.parent);
	u32 msg = (u32) (unsigned long) data;

	tegra_aon_hsp_sm_full_notify(aon, msg);
}

static void aon_hsp_tx_empty_notify(struct mbox_client *cl, void *data,
				    int empty_value)
{
	struct aon_hsp *aonhsp = dev_get_drvdata(cl->dev);

	(void)empty_value;	/* ignored */

	complete(&aonhsp->emptied);
}
#else
static int tegra_aon_hsp_sm_tx_write(struct aon_hsp *aonhsp, u32 value)
{
	tegra_hsp_sm_tx_write(aonhsp->tx, value);

	return 0;
}

static bool tegra_aon_hsp_sm_tx_is_empty(struct aon_hsp *aonhsp)
{
	return tegra_hsp_sm_tx_is_empty(aonhsp->tx);
}
#endif

static void tegra_aon_notify_remote(struct ivc *ivc)
{
	struct tegra_aon_ivc_chan *ivc_chan;

	ivc_chan = container_of(ivc, struct tegra_aon_ivc_chan, ivc);
	tegra_aon_hsp_ss_set(ivc_chan->aon, ivc_chan->aon->ivc_tx_ss,
				BIT(ivc_chan->chan_id));
	tegra_aon_hsp_sm_tx_write(ivc_chan->aon->hsp, SMBOX_IVC_NOTIFY);
}

#define NV(p) "nvidia," p

static int tegra_aon_parse_channel(struct device *dev,
				struct mbox_chan *mbox_chan,
				struct device_node *ch_node,
				int chan_id)
{
	struct tegra_aon *aon;
	struct tegra_aon_ivc_chan *ivc_chan;
	struct {
		u32 rx, tx;
	} start, end;
	u32 nframes, frame_size;
	int ret = 0;

	aon = platform_get_drvdata(to_platform_device(dev));

	/* Sanity check */
	if (!mbox_chan || !ch_node || !dev || !aon)
		return -EINVAL;

	ret = of_property_read_u32_array(ch_node, "reg", &start.rx, 2);
	if (ret) {
		dev_err(dev, "missing <%s> property\n", "reg");
		return ret;
	}
	ret = of_property_read_u32(ch_node, NV("frame-count"), &nframes);
	if (ret) {
		dev_err(dev, "missing <%s> property\n", NV("frame-count"));
		return ret;
	}
	ret = of_property_read_u32(ch_node, NV("frame-size"), &frame_size);
	if (ret) {
		dev_err(dev, "missing <%s> property\n", NV("frame-size"));
		return ret;
	}

	if (!nframes) {
		dev_err(dev, "Invalid <nframes> property\n");
		return -EINVAL;
	}

	if (frame_size < ivc_min_frame_size()) {
		dev_err(dev, "Invalid <frame-size> property\n");
		return -EINVAL;
	}

	end.rx = start.rx + tegra_ivc_total_queue_size(nframes * frame_size);
	end.tx = start.tx + tegra_ivc_total_queue_size(nframes * frame_size);

	if (end.rx > aon->ipcbuf_size) {
		dev_err(dev, "%s buffer exceeds ivc size\n", "rx");
		return -EINVAL;
	}
	if (end.tx > aon->ipcbuf_size) {
		dev_err(dev, "%s buffer exceeds ivc size\n", "tx");
		return -EINVAL;
	}

	if (start.tx < start.rx ? end.tx > start.rx : end.rx > start.tx) {
		dev_err(dev, "rx and tx buffers overlap on channel %s\n",
			ch_node->name);
		return -EINVAL;
	}

	ivc_chan = devm_kzalloc(dev, sizeof(*ivc_chan), GFP_KERNEL);
	if (!ivc_chan) {
		dev_err(dev, "Failed to allocate AON IVC channel\n");
		return -ENOMEM;
	}

	ivc_chan->name = devm_kstrdup(dev, ch_node->name, GFP_KERNEL);
	if (!ivc_chan->name)
		return -ENOMEM;

	ivc_chan->chan_id = chan_id;

	/* Allocate the IVC links */
	ret = tegra_ivc_init_with_dma_handle(&ivc_chan->ivc,
			     (unsigned long)aon->ipcbuf + start.rx,
			     (u64)aon->ipcbuf_dma + start.rx,
			     (unsigned long)aon->ipcbuf + start.tx,
			     (u64)aon->ipcbuf_dma + start.tx,
			     nframes, frame_size, dev,
			     tegra_aon_notify_remote);
	if (ret) {
		dev_err(dev, "failed to instantiate IVC.\n");
		return ret;
	}

	ivc_chan->aon = aon;
	mbox_chan->con_priv = ivc_chan;

	dev_dbg(dev, "%s: RX: 0x%x-0x%x TX: 0x%x-0x%x\n",
		ivc_chan->name, start.rx, end.rx, start.tx, end.tx);

	return ret;
}

static int tegra_aon_check_channels_overlap(struct device *dev,
			struct tegra_aon_ivc_chan *ch0,
			struct tegra_aon_ivc_chan *ch1)
{
	unsigned s0, s1;
	unsigned long tx0, rx0, tx1, rx1;

	if (ch0 == NULL || ch1 == NULL)
		return -EINVAL;

	tx0 = (unsigned long)ch0->ivc.tx_channel;
	rx0 = (unsigned long)ch0->ivc.rx_channel;
	s0 = ch0->ivc.nframes * ch0->ivc.frame_size;
	s0 = tegra_ivc_total_queue_size(s0);

	tx1 = (unsigned long)ch1->ivc.tx_channel;
	rx1 = (unsigned long)ch1->ivc.rx_channel;
	s1 = ch1->ivc.nframes * ch1->ivc.frame_size;
	s1 = tegra_ivc_total_queue_size(s1);

	if ((tx0 < tx1 ? tx0 + s0 > tx1 : tx1 + s1 > tx0) ||
		(rx0 < tx1 ? rx0 + s0 > tx1 : tx1 + s1 > rx0) ||
		(rx0 < rx1 ? rx0 + s0 > rx1 : rx1 + s1 > rx0) ||
		(tx0 < rx1 ? tx0 + s0 > rx1 : rx1 + s1 > tx0)) {
		dev_err(dev, "ivc buffers overlap on channels %s and %s\n",
			ch0->name, ch1->name);
		return -EINVAL;
	}

	return 0;
}

static int tegra_aon_validate_channels(struct device *dev)
{
	struct tegra_aon *aon;
	struct tegra_aon_ivc_chan *i_chan, *j_chan;
	int i, j;
	int ret;

	aon = dev_get_drvdata(dev);

	for (i = 0; i < aon->mbox.num_chans; i++) {
		i_chan = aon->mbox.chans[i].con_priv;

		for (j = i + 1; j < aon->mbox.num_chans; j++) {
			j_chan = aon->mbox.chans[j].con_priv;

			ret = tegra_aon_check_channels_overlap(dev, i_chan,
								j_chan);
			if (ret)
				return ret;
		}
	}

	return 0;
}

static int tegra_aon_parse_channels(struct device *dev)
{
	struct tegra_aon *aon;
	struct device_node *reg_node, *ch_node;
	int ret, i;

	aon = dev_get_drvdata(dev);
	i = 0;

	for_each_child_of_node(dev->of_node, reg_node) {
		if (strcmp(reg_node->name, "ivc-channels"))
			continue;

		for_each_child_of_node(reg_node, ch_node) {
			ret = tegra_aon_parse_channel(dev,
						&aon->mbox.chans[i],
						ch_node, i);
			i++;
			if (ret) {
				dev_err(dev, "failed to parse a channel\n");
				return ret;
			}
		}
		break;
	}

	return tegra_aon_validate_channels(dev);
}

static int tegra_aon_mbox_get_max_txsize(struct mbox_chan *mbox_chan)
{
	struct tegra_aon_ivc_chan *ivc_chan;

	ivc_chan = (struct tegra_aon_ivc_chan *)mbox_chan->con_priv;

	return ivc_chan->ivc.frame_size;
}

static int tegra_aon_mbox_send_data(struct mbox_chan *mbox_chan, void *data)
{
	struct tegra_aon_ivc_chan *ivc_chan;
	struct tegra_aon_mbox_msg *msg;
	int bytes;
	int ret;

	msg = (struct tegra_aon_mbox_msg *)data;
	ivc_chan = (struct tegra_aon_ivc_chan *)mbox_chan->con_priv;
	bytes = tegra_ivc_write(&ivc_chan->ivc, msg->data, msg->length);
	ret = (bytes != msg->length) ? -EBUSY : 0;
	if (bytes < 0) {
		pr_err("%s mbox send failed with error %d\n", __func__, bytes);
		ret = bytes;
	}
	ivc_chan->last_tx_done = (ret == 0);

	return ret;
}

static int tegra_aon_mbox_startup(struct mbox_chan *mbox_chan)
{
	return 0;
}

static void tegra_aon_mbox_shutdown(struct mbox_chan *mbox_chan)
{
	struct tegra_aon_ivc_chan *ivc_chan;

	ivc_chan = (struct tegra_aon_ivc_chan *)mbox_chan->con_priv;
	ivc_chan->chan_id = -1;
}

static bool tegra_aon_mbox_last_tx_done(struct mbox_chan *mbox_chan)
{
	struct tegra_aon_ivc_chan *ivc_chan;

	ivc_chan = (struct tegra_aon_ivc_chan *)mbox_chan->con_priv;

	return ivc_chan->last_tx_done;
}

static struct mbox_chan_ops tegra_aon_mbox_chan_ops = {
	.get_max_txsize = tegra_aon_mbox_get_max_txsize,
	.send_data = tegra_aon_mbox_send_data,
	.startup = tegra_aon_mbox_startup,
	.shutdown = tegra_aon_mbox_shutdown,
	.last_tx_done = tegra_aon_mbox_last_tx_done,
};

static int tegra_aon_count_ivc_channels(struct device_node *dev_node)
{
	int num = 0;
	struct device_node *child_node;

	for_each_child_of_node(dev_node, child_node) {
		if (strcmp(child_node->name, "ivc-channels"))
			continue;
		num = of_get_child_count(child_node);
		break;
	}

	return num;
}

static ssize_t store_ivc_dbg(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct tegra_aon *aon = dev_get_drvdata(dev);
	u32 shrdsem_msg;
	u32 channel;
	u32 enable;
	int ret;

	if (count > ivc_min_frame_size())
		return -EINVAL;

	ret = kstrtouint(buf, 0, &channel);
	if (ret)
		return -EINVAL;

	enable = channel & BIT(IVC_TASKS_DBG_ENABLE_BIT);
	channel &= ~BIT(IVC_TASKS_DBG_ENABLE_BIT);
	if (channel >= BIT(IVC_TASK_ENABLE_MAX))
		return -EINVAL;

	shrdsem_msg = channel | enable;
	tegra_aon_hsp_ss_set(aon, aon->ivc_dbg_enable_ss, shrdsem_msg);
	tegra_aon_hsp_sm_tx_write(aon->hsp, SMBOX_IVC_DBG_ENABLE);

	return count;
}


static DEVICE_ATTR(ivc_dbg, S_IWUSR, NULL, store_ivc_dbg);

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0)
static struct device_node *of_get_compatible_child(
				const struct device_node *parent,
				const char *compatible)
{
	struct device_node *child;

	for_each_child_of_node(parent, child) {
		if (of_device_is_compatible(child, compatible))
			break;
	}

	return child;
}
#endif

static int aon_hsp_probe(struct aon_hsp *aonhsp)
{
	struct device_node *np = aonhsp->dev.parent->of_node;
	struct tegra_aon *aon = dev_get_drvdata(aonhsp->dev.parent);
	int err = -ENODEV;

	np = of_get_compatible_child(np, "nvidia,tegra-aon-hsp");
	if (!of_device_is_available(np)) {
		of_node_put(np);
		dev_err(&aonhsp->dev, "no hsp protocol \"%s\"\n",
				"nvidia,tegra-aon-hsp");
		return -ENODEV;
	}

	aonhsp->dev.of_node = np;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0)
	(void) aon;
	aonhsp->rx.chan = mbox_request_channel_byname(&aonhsp->rx.client,
						      "ivc-rx");
	if (IS_ERR(aonhsp->rx.chan)) {
		err = PTR_ERR(aonhsp->rx.chan);
		goto fail;
	}

	aonhsp->tx.chan = mbox_request_channel_byname(&aonhsp->tx.client,
						      "ivc-tx");
	if (IS_ERR(aonhsp->tx.chan)) {
		err = PTR_ERR(aonhsp->tx.chan);
		goto fail;
	}
#else
	/* Fetch the shared mailbox associated with IVC rx */
	aonhsp->rx = of_tegra_hsp_sm_rx_by_name(np, "ivc-rx",
					tegra_aon_hsp_sm_full_notify, aon);
	if (IS_ERR(aonhsp->rx)) {
		err = PTR_ERR(aonhsp->rx);
		if (err != -EPROBE_DEFER)
			dev_err(&aonhsp->dev, "failed to fetch rx sm : %d\n",
				err);
		goto fail;
	}

	/* Fetch the shared mailbox associated with IVC tx */
	aonhsp->tx = of_tegra_hsp_sm_tx_by_name(np, "ivc-tx", NULL, aon);
	if (IS_ERR(aonhsp->tx)) {
		err = PTR_ERR(aonhsp->tx);
		if (err != -EPROBE_DEFER)
			dev_err(&aonhsp->dev, "failed to fetch tx sm : %d\n",
				err);
		goto fail;
	}
#endif
	return 0;

fail:
	if (err != -EPROBE_DEFER) {
		dev_err(&aonhsp->dev, "%s: failed to obtain : %d\n",
			np->name, err);
	}
	of_node_put(np);
	return err;
}

static const struct device_type aon_hsp_combo_dev_type = {
	.name	= "aon-hsp-protocol",
};

static void aon_hsp_combo_dev_release(struct device *dev)
{
	struct aon_hsp *aonhsp = container_of(dev, struct aon_hsp, dev);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0)
	if (!IS_ERR_OR_NULL(aonhsp->rx.chan))
		mbox_free_channel(aonhsp->rx.chan);
	if (!IS_ERR_OR_NULL(aonhsp->tx.chan))
		mbox_free_channel(aonhsp->tx.chan);
#else
	if (!IS_ERR_OR_NULL(aonhsp->rx))
		tegra_hsp_sm_rx_free(aonhsp->rx);
	if (!IS_ERR_OR_NULL(aonhsp->tx))
		tegra_hsp_sm_tx_free(aonhsp->tx);
#endif

	of_node_put(dev->of_node);
	kfree(aonhsp);
}

static void aon_hsp_free(struct aon_hsp *aonhsp)
{
	if (IS_ERR_OR_NULL(aonhsp))
		return;

	if (dev_get_drvdata(&aonhsp->dev) != NULL)
		device_unregister(&aonhsp->dev);
	else
		put_device(&aonhsp->dev);
}

static struct aon_hsp *aon_hsp_create(struct device *dev)
{
	struct aon_hsp *aonhsp;
	int ret = -EINVAL;

	aonhsp = kzalloc(sizeof(*aonhsp), GFP_KERNEL);
	if (aonhsp == NULL)
		return ERR_PTR(-ENOMEM);

	aonhsp->dev.parent = dev;

	aonhsp->dev.type = &aon_hsp_combo_dev_type;
	aonhsp->dev.release = aon_hsp_combo_dev_release;
	device_initialize(&aonhsp->dev);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0)
	init_completion(&aonhsp->emptied);
	aonhsp->tx.client.tx_block = false;
	aonhsp->rx.client.rx_callback = aon_hsp_rx_full_notify;
	aonhsp->tx.client.tx_done = aon_hsp_tx_empty_notify;
	aonhsp->rx.client.dev = aonhsp->tx.client.dev = &(aonhsp->dev);
#endif

	dev_set_name(&aonhsp->dev, "%s:%s", dev_name(dev), "hsp");

	ret = aon_hsp_probe(aonhsp);
	if (ret < 0)
		goto fail;

	ret = device_add(&aonhsp->dev);
	if (ret < 0)
		goto fail;

	dev_set_drvdata(&aonhsp->dev, aonhsp);

	return aonhsp;

fail:
	aon_hsp_free(aonhsp);
	return ERR_PTR(ret);
}

static void tegra_aon_hsp_sm_pair_free(struct tegra_aon *aon)
{
	if (aon->hsp) {
		aon_hsp_free(aon->hsp);
		aon->hsp = NULL;
	}
}

static int tegra_aon_probe(struct platform_device *pdev)
{
	struct tegra_aon *aon;
	struct device *dev = &pdev->dev;
	struct device_node *dn = dev->of_node;
	struct device_node *hsp_node;
	int num_chans;
	int ret = 0;
	ktime_t tstart;

	dev_dbg(&pdev->dev, "tegra aon driver probe Start\n");

	aon = devm_kzalloc(&pdev->dev, sizeof(*aon), GFP_KERNEL);
	if (!aon)
		return -ENOMEM;
	platform_set_drvdata(pdev, aon);
	aon->ipcbuf_size = IPCBUF_SIZE;

	aon->ipcbuf = dmam_alloc_coherent(dev, aon->ipcbuf_size,
			&aon->ipcbuf_dma, GFP_KERNEL | __GFP_ZERO);
	if (!aon->ipcbuf) {
		dev_err(dev, "failed to allocate IPC memory\n");
		return -ENOMEM;
	}

	aon->ss_base = of_iomap(dn, 0);
	if (!aon->ss_base) {
		dev_err(dev, "failed to map shared semaphore IO space\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(dn, NV("ivc-carveout-base-ss"),
					&aon->ivc_carveout_base_ss);
	if (ret) {
		dev_err(dev, "missing <%s> property\n",
			NV("ivc-carveout-base-ss"));
		return ret;
	}

	ret = of_property_read_u32(dn, NV("ivc-carveout-size-ss"),
					&aon->ivc_carveout_size_ss);
	if (ret) {
		dev_err(dev, "missing <%s> property\n",
			NV("ivc-carveout-size-ss"));
		return ret;
	}

	ret = of_property_read_u32(dn, NV("ivc-dbg-enable-ss"),
					&aon->ivc_dbg_enable_ss);
	if (ret) {
		dev_err(dev, "missing <%s> property\n",
			NV("ivc-dbg-enable-ss"));
		return ret;
	}

	ret = of_property_read_u32(dn, NV("ivc-rx-ss"),
					&aon->ivc_rx_ss);
	if (ret) {
		dev_err(dev, "missing <%s> property\n", NV("ivc-rx-ss"));
		return ret;
	}

	ret = of_property_read_u32(dn, NV("ivc-tx-ss"),
					&aon->ivc_tx_ss);
	if (ret) {
		dev_err(dev, "missing <%s> property\n", NV("ivc-tx-ss"));
		return ret;
	}

	num_chans = tegra_aon_count_ivc_channels(dn);
	if (num_chans <= 0) {
		dev_err(dev, "no ivc channels\n");
		ret = -EINVAL;
		goto exit;
	}

	aon->mbox.dev = &pdev->dev;
	aon->mbox.chans = devm_kzalloc(&pdev->dev,
					num_chans * sizeof(*aon->mbox.chans),
					GFP_KERNEL);
	if (!aon->mbox.chans) {
		ret = -ENOMEM;
		goto exit;
	}

	aon->mbox.num_chans = num_chans;
	aon->mbox.ops = &tegra_aon_mbox_chan_ops;
	aon->mbox.txdone_poll = true;
	aon->mbox.txpoll_period = 1;

	/* Parse out all channels from DT */
	ret = tegra_aon_parse_channels(dev);
	if (ret) {
		dev_err(dev, "ivc-channels set up failed: %d\n", ret);
		goto exit;
	}

	hsp_node = of_get_child_by_name(dn, "hsp");
	if (hsp_node == NULL) {
		dev_err(dev, "No hsp child node for AON\n");
		ret = -ENODEV;
		goto exit;
	}

	aon->hsp = aon_hsp_create(dev);
	if (IS_ERR(aon->hsp)) {
		aon->hsp = NULL;
		ret = PTR_ERR(aon->hsp);
		goto exit;
	}

	ret = device_create_file(dev, &dev_attr_ivc_dbg);
	if (ret) {
		dev_err(dev, "failed to create device file: %d\n", ret);
		tegra_aon_hsp_sm_pair_free(aon);
		mbox_controller_unregister(&aon->mbox);
		goto exit;
	}

	tegra_aon_hsp_ss_set(aon, aon->ivc_carveout_base_ss,
					(u32)aon->ipcbuf_dma);
	tegra_aon_hsp_ss_set(aon, aon->ivc_carveout_size_ss,
					(u32)aon->ipcbuf_size);
	tegra_aon_hsp_sm_tx_write(aon->hsp, SMBOX_IVC_READY_MSG);

	tstart = ktime_get();
	while (!tegra_aon_hsp_sm_tx_is_empty(aon->hsp)) {
		if (ktime_us_delta(ktime_get(), tstart) > IVC_INIT_TIMEOUT_US) {
			dev_err(dev, "IVC init timeout\n");
			tegra_aon_hsp_sm_pair_free(aon);
			ret = -ETIMEDOUT;
			goto exit;
		}
	}

	ret = mbox_controller_register(&aon->mbox);
	if (ret) {
		dev_err(&pdev->dev, "failed to register mailbox: %d\n", ret);
		tegra_aon_hsp_sm_pair_free(aon);
		goto exit;
	}

	dev_info(&pdev->dev, "tegra aon driver probe OK\n");

	return ret;

exit:
	iounmap(aon->ss_base);
	return ret;
}

static int tegra_aon_remove(struct platform_device *pdev)
{
	struct tegra_aon *aon = platform_get_drvdata(pdev);

	iounmap(aon->ss_base);
	mbox_controller_unregister(&aon->mbox);
	tegra_aon_hsp_sm_pair_free(aon);

	return 0;
}

static const struct of_device_id tegra_aon_of_match[] = {
	{ .compatible = NV("tegra186-aon"), },
	{},
};
MODULE_DEVICE_TABLE(of, tegra_aon_of_match);

static struct platform_driver tegra_aon_driver = {
	.probe	= tegra_aon_probe,
	.remove = tegra_aon_remove,
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "tegra_aon",
		.of_match_table = of_match_ptr(tegra_aon_of_match),
	},
};
module_platform_driver(tegra_aon_driver);
